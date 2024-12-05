/** @file
  Windows SMM Security Mitigation Table (WSMT) Parser.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "WsmtParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>

#include <IndustryStandard/WindowsSmmSecurityMitigationTable.h>

EFI_STATUS
EFIAPI
WsmtParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                  Status;
  CM_STD_OBJ_ACPI_TABLE_INFO  CmAcpiTableInfo;
  EFI_ACPI_WSMT_TABLE         *WsmtTable;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  //
  // Allocate and zero out TPM2 Interface Info
  //
  WsmtTable = AllocateZeroPool (sizeof (*WsmtTable));
  if (WsmtTable == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate WSMT table.\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  // Populate table
  WsmtTable->Header.Signature = EFI_ACPI_WINDOWS_SMM_SECURITY_MITIGATION_TABLE_SIGNATURE;
  WsmtTable->Header.Revision  = EFI_WSMT_TABLE_REVISION;
  CopyMem (WsmtTable->Header.OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (WsmtTable->Header.OemId));
  WsmtTable->Header.OemTableId      = PcdGet64 (PcdAcpiDefaultOemTableId);
  WsmtTable->Header.OemRevision     = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  WsmtTable->Header.CreatorId       = FixedPcdGet64 (PcdAcpiDefaultCreatorId);
  WsmtTable->Header.CreatorRevision = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  WsmtTable->Header.Length          = sizeof (EFI_ACPI_WSMT_TABLE);

  WsmtTable->ProtectionFlags = EFI_WSMT_PROTECTION_FLAGS_FIXED_COMM_BUFFERS |
                               EFI_WSMT_PROTECTION_FLAGS_COMM_BUFFER_NESTED_PTR_PROTECTION |
                               EFI_WSMT_PROTECTION_FLAGS_SYSTEM_RESOURCE_PROTECTION;

  //
  // Create a CM ACPI Table Entry for WSMT
  //
  CmAcpiTableInfo.AcpiTableSignature = WsmtTable->Header.Signature;
  CmAcpiTableInfo.AcpiTableRevision  = WsmtTable->Header.Revision;
  CmAcpiTableInfo.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdRaw);
  CmAcpiTableInfo.AcpiTableData      = &(WsmtTable->Header);
  CmAcpiTableInfo.OemTableId         = WsmtTable->Header.OemTableId;
  CmAcpiTableInfo.OemRevision        = WsmtTable->Header.OemRevision;
  CmAcpiTableInfo.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &CmAcpiTableInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the WSMT table\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  return Status;
}

REGISTER_PARSER_FUNCTION (WsmtParser, "skip-wsmt-table")
