/** @file
  Acpi Table List parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "AcpiTableListParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/BaseMemoryLib.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/DebugLib.h>
#include <Library/MpCoreInfoLib.h>

#include <Protocol/AmlPatchProtocol.h>

// #include "Dsdt_T194.offset.h"
extern AML_OFFSET_TABLE_ENTRY  DSDT_TEGRA194_OffsetTable[];
// #include "Dsdt_T194.hex"
extern unsigned char  dsdt_t194_aml_code[];

// #include "Dsdt_T234.hex"
extern AML_OFFSET_TABLE_ENTRY  DSDT_TEGRA234_OffsetTable[];
// #include "Dsdt_T234.offset.h"
extern unsigned char  dsdt_t234_aml_code[];

// #include "Dsdt_TH500.hex"
extern unsigned char  dsdt_th500_aml_code[];
// #include "Dsdt_TH500.offset.h"
extern AML_OFFSET_TABLE_ENTRY  DSDT_TH500_OffsetTable[];
// #include "SsdtSocket1_TH500.hex"
extern unsigned char  ssdtsocket1_th500_aml_code[];
// #include "SsdtSocket1_TH500.offset.h"
extern AML_OFFSET_TABLE_ENTRY  SSDT_TH500_S1_OffsetTable[];
// #include "SsdtSocket2_TH500.hex"
extern unsigned char  ssdtsocket2_th500_aml_code[];
// #include "SsdtSocket2_TH500.offset.h"
extern AML_OFFSET_TABLE_ENTRY  SSDT_TH500_S2_OffsetTable[];
// #include "SsdtSocket3_TH500.hex"
extern unsigned char  ssdtsocket3_th500_aml_code[];
// #include "SsdtSocket3_TH500.offset.h"
extern AML_OFFSET_TABLE_ENTRY  SSDT_TH500_S3_OffsetTable[];

// #include "BpmpSsdtSocket0_TH500.hex"
extern unsigned char  bpmpssdtsocket0_th500_aml_code[];
// #include "BpmpSsdtSocket0_TH500.offset.h"
extern AML_OFFSET_TABLE_ENTRY  SSDT_BPMP_S0_OffsetTable[];
// #include "BpmpSsdtSocket1_TH500.hex"
extern unsigned char  bpmpssdtsocket1_th500_aml_code[];
// #include "BpmpSsdtSocket1_TH500.offset.h"
extern AML_OFFSET_TABLE_ENTRY  SSDT_BPMP_S1_OffsetTable[];
// #include "BpmpSsdtSocket2_TH500.hex"
extern unsigned char  bpmpssdtsocket2_th500_aml_code[];
// #include "BpmpSsdtSocket2_TH500.offset.h"
extern AML_OFFSET_TABLE_ENTRY  SSDT_BPMP_S2_OffsetTable[];
// #include "BpmpSsdtSocket3_TH500.hex"
extern unsigned char  bpmpssdtsocket3_th500_aml_code[];
// #include "BpmpSsdtSocket3_TH500.offset.h"
extern AML_OFFSET_TABLE_ENTRY  SSDT_BPMP_S3_OffsetTable[];

// #include "SdcTemplate.hex"
extern unsigned char  sdctemplate_aml_code[];
// #include "SdcTemplate.offset.h"
extern AML_OFFSET_TABLE_ENTRY  SSDT_SDCTEMP_OffsetTable[];

// #include "I2cTemplate.hex"
extern unsigned char  i2ctemplate_aml_code[];
// #include "I2cTemplate.offset.h"
extern AML_OFFSET_TABLE_ENTRY  SSDT_I2CTEMP_OffsetTable[];

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

// CmAcpiTableList is shared between T194, T234, and TH500
STATIC
CM_STD_OBJ_ACPI_TABLE_INFO  CmAcpiTableList[] = {
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

/** The platform ACPI info for TH500.
*/
STATIC EFI_ACPI_DESCRIPTION_HEADER  *AcpiTableArray_TH500[] = {
  (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_th500_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)ssdtsocket1_th500_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)ssdtsocket2_th500_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)ssdtsocket3_th500_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket0_th500_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket1_th500_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket2_th500_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket3_th500_aml_code
};

STATIC AML_OFFSET_TABLE_ENTRY  *OffsetTableArray_TH500[] = {
  DSDT_TH500_OffsetTable,
  SSDT_TH500_S1_OffsetTable,
  SSDT_TH500_S2_OffsetTable,
  SSDT_TH500_S3_OffsetTable,
  SSDT_BPMP_S0_OffsetTable,
  SSDT_BPMP_S1_OffsetTable,
  SSDT_BPMP_S2_OffsetTable,
  SSDT_BPMP_S3_OffsetTable
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

  // Locate the tables based on ChipID
  switch (ChipID) {
    case T194_CHIP_ID:
      DsdtTable        = dsdt_t194_aml_code;
      AcpiTableArray   = AcpiTableArray_T194;
      OffsetTableArray = OffsetTableArray_T194;
      ArraySize        = ARRAY_SIZE (AcpiTableArray_T194);
      break;

    case T234_CHIP_ID:
      DsdtTable        = dsdt_t234_aml_code;
      AcpiTableArray   = AcpiTableArray_T234;
      OffsetTableArray = OffsetTableArray_T234;
      ArraySize        = ARRAY_SIZE (AcpiTableArray_T234);
      break;

    case TH500_CHIP_ID:
      DsdtTable        = dsdt_th500_aml_code;
      AcpiTableArray   = AcpiTableArray_TH500;
      OffsetTableArray = OffsetTableArray_TH500;
      ArraySize        = ARRAY_SIZE (AcpiTableArray_TH500);
      break;

    default:
      // Not currently supported
      DEBUG ((DEBUG_ERROR, "%a: Unknown ChipID 0x%x\n", __FUNCTION__, ChipID));
      Status = EFI_NOT_FOUND;
      goto CleanupAndReturn;
  }

  // Update the OemId in the tables to match the PCD
  for (Index = 0; Index < ArraySize; Index++) {
    CopyMem (AcpiTableArray[Index]->OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (AcpiTableArray[Index]->OemId));
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

  // Add each table to CM, and fix up the DSDT pointer
  for (Index = 0; Index < ARRAY_SIZE (CmAcpiTableList); Index++) {
    // Fix the OemTableId on all entries
    CmAcpiTableList[Index].OemTableId =  PcdGet64 (PcdAcpiDefaultOemTableId);

    // Fill in the DSDT table pointer in DSDT entry
    if (CmAcpiTableList[Index].AcpiTableSignature == EFI_ACPI_6_4_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {
      CmAcpiTableList[Index].AcpiTableData = DsdtTable;
    }

    Status = NvAddAcpiTableGenerator (ParserHandle, &CmAcpiTableList[Index]);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add parser at index %lu\n", __FUNCTION__, Status, Index));
      goto CleanupAndReturn;
    }
  }

  // Add tables for additional sockets if needed
  if (ChipID == TH500_CHIP_ID) {
    UINT32                      MaxSocket;
    UINT32                      SocketId;
    CM_STD_OBJ_ACPI_TABLE_INFO  NewAcpiTable;

    Status = MpCoreInfoGetPlatformInfo (NULL, &MaxSocket, NULL, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get PlatformInfo\n", __FUNCTION__, Status));
      goto CleanupAndReturn;
    }

    NewAcpiTable.AcpiTableSignature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
    NewAcpiTable.AcpiTableRevision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
    NewAcpiTable.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
    NewAcpiTable.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
    NewAcpiTable.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
    NewAcpiTable.MinorRevision      = 0;

    for (SocketId = 1; SocketId <= MaxSocket; SocketId++) {
      Status = MpCoreInfoGetSocketInfo (SocketId, NULL, NULL, NULL, NULL);
      if (!EFI_ERROR (Status)) {
        NewAcpiTable.AcpiTableData = (EFI_ACPI_DESCRIPTION_HEADER *)AcpiTableArray_TH500[SocketId];

        Status = NvAddAcpiTableGenerator (ParserHandle, &NewAcpiTable);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the SSDT table for Socket %u\n", __FUNCTION__, Status, SocketId));
          goto CleanupAndReturn;
        }
      }
    }
  }

CleanupAndReturn:
  return Status;
}

REGISTER_PARSER_FUNCTION (AcpiTableListParser, NULL)
