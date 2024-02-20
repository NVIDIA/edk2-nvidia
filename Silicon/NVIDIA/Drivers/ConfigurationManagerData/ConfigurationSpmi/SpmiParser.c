/** @file

  Service Processor Management Interface Table (SPMI) Parser

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "NvCmObjectDescUtility.h"
#include "SpmiParser.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

#include <IndustryStandard/ServiceProcessorManagementInterfaceTable.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

EFI_STATUS
EFIAPI
SpmiParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                  Status;
  CM_STD_OBJ_ACPI_TABLE_INFO  AcpiTableHeader;
  CM_OBJ_DESCRIPTOR           Desc;

  Status = IpmiParser (ParserHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: No IPMI Device. Skip installing SPMI table.\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_SERVER_PLATFORM_MANAGEMENT_INTERFACE_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_SERVICE_PROCESSOR_MANAGEMENT_INTERFACE_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpmi);
  AcpiTableHeader.AcpiTableData      = NULL;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Desc.ObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
  Desc.Size     = sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
  Desc.Count    = 1;
  Desc.Data     = &AcpiTableHeader;

  Status = NvExtendCmObj (ParserHandle, &Desc, CM_NULL_TOKEN, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  return Status;
}
