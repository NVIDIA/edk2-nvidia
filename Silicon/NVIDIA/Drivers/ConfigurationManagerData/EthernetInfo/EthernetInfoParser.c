/** @file
  Ethernet info parser.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "EthernetInfoParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>

// #include "SsdtEth_TH500.hex"
extern unsigned char  ssdteth_th500_aml_code[];

/** Ethernet info parser function.

  The Ethernet SSDT is added to the ACPI table list.

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
EthernetInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                  Status;
  CM_STD_OBJ_ACPI_TABLE_INFO  NewAcpiTable;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (TegraGetPlatform () != TEGRA_PLATFORM_VDK) {
    return EFI_SUCCESS;
  }

  NewAcpiTable.AcpiTableSignature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
  NewAcpiTable.AcpiTableRevision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
  NewAcpiTable.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
  NewAcpiTable.AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)ssdteth_th500_aml_code;
  NewAcpiTable.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  NewAcpiTable.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  NewAcpiTable.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &NewAcpiTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the Ethernet SSDT table\n", __FUNCTION__, Status));
    return Status;
  }

  return Status;
}

REGISTER_PARSER_FUNCTION (EthernetInfoParser, NULL)
