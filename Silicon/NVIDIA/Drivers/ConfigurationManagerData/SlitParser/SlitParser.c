/** @file
  Static Locality Information Table Parser

  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "SlitParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/NumaInfoLib.h>
#include <TH500/TH500Definitions.h>

EFI_STATUS
EFIAPI
SlitParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                                                      Status;
  CM_STD_OBJ_ACPI_TABLE_INFO                                      AcpiTableHeader;
  UINTN                                                           ProximityDomains;
  UINT8                                                           *Distance;
  UINT32                                                          RowIndex;
  UINT32                                                          ColIndex;
  EFI_ACPI_6_4_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_HEADER  *SlitHeader;
  UINT32                                                          MaxProximityDomain;
  UINT32                                                          NumberOfInitiatorDomains;
  UINT32                                                          NumberOfTargetDomains;

  Status = NumaInfoGetDomainLimits (&MaxProximityDomain, &NumberOfInitiatorDomains, &NumberOfTargetDomains);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NumaInfoGetDomainLimits failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  ProximityDomains = MaxProximityDomain + 1;

  // Allocate table
  SlitHeader = AllocateZeroPool (
                 sizeof (EFI_ACPI_6_4_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_HEADER) +
                 sizeof (UINT8) * ProximityDomains * ProximityDomains
                 );
  if (SlitHeader == NULL) {
    ASSERT (FALSE);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  Distance = (UINT8 *)(SlitHeader + 1);
  // Populate header
  SlitHeader->Header.Signature = EFI_ACPI_6_4_SYSTEM_LOCALITY_INFORMATION_TABLE_SIGNATURE;
  SlitHeader->Header.Revision  = EFI_ACPI_6_4_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_REVISION;
  CopyMem (SlitHeader->Header.OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (SlitHeader->Header.OemId));
  SlitHeader->Header.OemTableId      = PcdGet64 (PcdAcpiDefaultOemTableId);
  SlitHeader->Header.OemRevision     = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  SlitHeader->Header.CreatorId       = FixedPcdGet64 (PcdAcpiDefaultCreatorId);
  SlitHeader->Header.CreatorRevision = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  // Populate entries
  SlitHeader->NumberOfSystemLocalities = ProximityDomains;
  // Assign normalized local and unreachable distances for all domains
  for (RowIndex = 0; RowIndex < ProximityDomains; RowIndex++ ) {
    for (ColIndex = 0; ColIndex < ProximityDomains; ColIndex++ ) {
      Status = NumaInfoGetDistances (
                 RowIndex,
                 ColIndex,
                 &Distance[RowIndex * ProximityDomains + ColIndex],
                 NULL,
                 NULL,
                 NULL
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: NumaInfoGetDistances(%u, %u) failed: %r\n",
          __FUNCTION__,
          RowIndex,
          ColIndex,
          Status
          ));
      }
    }
  }

  SlitHeader->Header.Length = sizeof (EFI_ACPI_6_4_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_HEADER) +
                              sizeof (UINT8) * ProximityDomains * ProximityDomains;
  // Install Table
  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_SYSTEM_LOCALITY_INFORMATION_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_6_4_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdRaw);
  AcpiTableHeader.AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)SlitHeader;
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

REGISTER_PARSER_FUNCTION (SlitParser, "skip-slit-table")
