/** @file

  Service Processor Management Interface Table (SPMI) Parser

  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "SpmiParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

#include <IndustryStandard/ServiceProcessorManagementInterfaceTable.h>

extern BOOLEAN  mIpmiDevCmInstalled;

EFI_STATUS
EFIAPI
SpmiParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                  Status;
  CM_STD_OBJ_ACPI_TABLE_INFO  AcpiTableHeader;

  if (!mIpmiDevCmInstalled) {
    DEBUG ((DEBUG_INFO, "%a: No IPMI Device. Skip installing SPMI table.\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_SERVER_PLATFORM_MANAGEMENT_INTERFACE_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_SERVICE_PROCESSOR_MANAGEMENT_INTERFACE_5_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpmi);
  AcpiTableHeader.AcpiTableData      = NULL;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the SLIT SSDT table\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  return Status;
}

REGISTER_PARSER_FUNCTION (SpmiParser, "skip-spmi-table")
