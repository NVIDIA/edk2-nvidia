/** @file
  Acpi Table List parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "AcpiTableListParser.h"
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/DebugLib.h>

#include "Dsdt_T194.hex"
#include "Dsdt_T194.offset.h"

#include "Dsdt_T234.hex"
#include "Dsdt_T234.offset.h"

#include "SdhciInfo/SdhciInfoParser.h"
#include "I2cInfo/I2cInfoParser.h"

/** The platform ACPI info for T194.
*/
STATIC EFI_ACPI_DESCRIPTION_HEADER  *AcpiTableArray_T194[] = {
  (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_t194_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)sdctemplate_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)i2ctemplate_aml_code
};

STATIC AML_OFFSET_TABLE_ENTRY  *OffsetTableArray_T194[] = {
  DSDT_TEGRA194_OffsetTable,
  SSDT_SDCTEMP_OffsetTable,
  SSDT_I2CTEMP_OffsetTable
};

// CmAcpiTableList_T194_T234 is shared between T194 and T234
STATIC
CM_STD_OBJ_ACPI_TABLE_INFO  CmAcpiTableList_T194_T234[] = {
  // FADT Table
  {
    EFI_ACPI_6_4_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_4_FIXED_ACPI_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdFadt),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision),
    0
  },
  // GTDT Table
  {
    EFI_ACPI_6_4_GENERIC_TIMER_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_4_GENERIC_TIMER_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdGtdt),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision),
    0
  },
  // MADT Table
  {
    EFI_ACPI_6_5_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_5_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMadt),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision),
    0
  },
  // DSDT Table
  {
    EFI_ACPI_6_4_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_4_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt),
    NULL, // Will be filled in later
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision),
    0
  },
  // PPTT Table
  {
    EFI_ACPI_6_4_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_STRUCTURE_SIGNATURE,
    EFI_ACPI_6_4_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdPptt),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision),
    0
  },
  // SSDT Table - Cpu Topology
  {
    EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdtCpuTopology),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision),
    0
  }
};

/** The platform ACPI info for T234.
*/
STATIC EFI_ACPI_DESCRIPTION_HEADER  *AcpiTableArray_T234[] = {
  (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_t234_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)sdctemplate_aml_code
};

STATIC AML_OFFSET_TABLE_ENTRY  *OffsetTableArray_T234[] = {
  DSDT_TEGRA234_OffsetTable,
  SSDT_SDCTEMP_OffsetTable
};

/** Acpi table list parser function.

  The following structures are populated:

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
AcpiTableListParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                   Status;
  CM_OBJ_DESCRIPTOR            Desc;
  UINTN                        ChipID;
  UINTN                        Index;
  EFI_ACPI_DESCRIPTION_HEADER  **AcpiTableArray;
  AML_OFFSET_TABLE_ENTRY       **OffsetTableArray;
  UINTN                        ArraySize;
  VOID                         *DsdtTable;
  NVIDIA_AML_PATCH_PROTOCOL    *PatchProtocol;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  Status = NvGetCmPatchProtocol (ParserHandle, &PatchProtocol);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  ChipID = TegraGetChipID ();

  Desc.ObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);

  // add Tables to Desc
  if (ChipID == T194_CHIP_ID) {
    DsdtTable = dsdt_t194_aml_code;

    AcpiTableArray   = AcpiTableArray_T194;
    OffsetTableArray = OffsetTableArray_T194;
    ArraySize        = ARRAY_SIZE (AcpiTableArray_T194);

    Desc.Size  = sizeof (CmAcpiTableList_T194_T234);
    Desc.Count = sizeof (CmAcpiTableList_T194_T234) / sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
    Desc.Data  = &CmAcpiTableList_T194_T234;
  } else if (ChipID == T234_CHIP_ID) {
    DsdtTable = dsdt_t234_aml_code;

    AcpiTableArray   = AcpiTableArray_T234;
    OffsetTableArray = OffsetTableArray_T234;
    ArraySize        = ARRAY_SIZE (AcpiTableArray_T234);

    Desc.Size  = sizeof (CmAcpiTableList_T194_T234);
    Desc.Count = sizeof (CmAcpiTableList_T194_T234) / sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
    Desc.Data  = &CmAcpiTableList_T194_T234;
  } else {
    // Not currently supported
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  Status = PatchProtocol->RegisterAmlTables (
                            PatchProtocol,
                            AcpiTableArray,
                            OffsetTableArray,
                            ArraySize
                            );
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  // Table fixups
  for (Index = 0; Index < Desc.Count; Index++) {
    if (((CM_STD_OBJ_ACPI_TABLE_INFO *)Desc.Data)[Index].AcpiTableSignature != EFI_ACPI_6_4_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE) {
      // Fix the OemTableId on all entries except SPCR
      ((CM_STD_OBJ_ACPI_TABLE_INFO *)Desc.Data)[Index].OemTableId =  PcdGet64 (PcdAcpiDefaultOemTableId);

      // Fill in the DSDT table pointer in DSDT entry
      if (((CM_STD_OBJ_ACPI_TABLE_INFO *)Desc.Data)[Index].AcpiTableSignature == EFI_ACPI_6_4_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {
        ((CM_STD_OBJ_ACPI_TABLE_INFO *)Desc.Data)[Index].AcpiTableData = DsdtTable;
      }
    }
  }

  Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  return Status;
}
