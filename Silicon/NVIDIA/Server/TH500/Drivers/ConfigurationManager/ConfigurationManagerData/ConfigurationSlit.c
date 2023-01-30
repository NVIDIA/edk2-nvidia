/** @file

  Configuration Manager Data of Static Locality Information Table

  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/FloorSweepingLib.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <ConfigurationManagerDataPrivate.h>

#include <TH500/TH500Definitions.h>

// Normalized Distances
#define NORMALIZED_LOCAL_DISTANCE              PcdGet32 (PcdLocalDistance)
#define NORMALIZED_UNREACHABLE_DISTANCE        PcdGet32 (PcdUnreachableDistance)
#define NORMALIZED_CPU_TO_REMOTE_CPU_DISTANCE  PcdGet32 (PcdCpuToRemoteCpuDistance)
#define NORMALIZED_GPU_TO_REMOTE_GPU_DISTANCE  PcdGet32 (PcdGpuToRemoteGpuDistance)
#define NORMALIZED_CPU_TO_LOCAL_HBM_DISTANCE   PcdGet32 (PcdCpuToLocalHbmDistance)
#define NORMALIZED_CPU_TO_REMOTE_HBM_DISTANCE  PcdGet32 (PcdCpuToRemoteHbmDistance)
#define NORMALIZED_HBM_TO_LOCAL_CPU_DISTANCE   PcdGet32 (PcdHbmToLocalCpuDistance)
#define NORMALIZED_HBM_TO_REMOTE_CPU_DISTANCE  PcdGet32 (PcdHbmToRemoteCpuDistance)
#define NORMALIZED_GPU_TO_LOCAL_HBM_DISTANCE   PcdGet32 (PcdGpuToLocalHbmDistance)
#define NORMALIZED_GPU_TO_REMOTE_HBM_DISTANCE  PcdGet32 (PcdGpuToRemoteHbmDistance)
#define NORMALIZED_HBM_TO_LOCAL_GPU_DISTANCE   PcdGet32 (PcdHbmToLocalGpuDistance)
#define NORMALIZED_HBM_TO_REMOTE_GPU_DISTANCE  PcdGet32 (PcdHbmToRemoteGpuDistance)

EFI_STATUS
EFIAPI
InstallStaticLocalityInformationTable (
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepositoryInfo
  )
{
  CM_STD_OBJ_ACPI_TABLE_INFO                                      *NewAcpiTables;
  UINTN                                                           Index;
  UINTN                                                           ProximityDomains;
  UINT8                                                           *Distance;
  UINT32                                                          RowIndex;
  UINT32                                                          ColIndex;
  UINT32                                                          MaxEnabledHbmDmns;
  EFI_ACPI_6_4_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_HEADER  *SlitHeader;

  // Create a 2D distance matrix with locality information across all possible proximity domains
  // Each CPU, GPU and GPU HBM is a proximity domain

  MaxEnabledHbmDmns = GetMaxHbmPxmDomains ();
  ProximityDomains  = (TH500_GPU_HBM_PXM_DOMAIN_START > MaxEnabledHbmDmns) ? TH500_GPU_HBM_PXM_DOMAIN_START : MaxEnabledHbmDmns;

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
      if (RowIndex == ColIndex) {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_LOCAL_DISTANCE;
      } else {
        // Assign 0xFF by default to indicate unreachability
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_UNREACHABLE_DISTANCE;
      }
    }
  }

  // Assign adjacent memory distance for all CPU to other CPU and GPU domains
  for (RowIndex = 0; RowIndex < PcdGet32 (PcdTegraMaxSockets); RowIndex++ ) {
    if (!IsSocketEnabled (RowIndex)) {
      continue;
    }

    // CPU to other CPU domains
    for (ColIndex = 0; ColIndex < PcdGet32 (PcdTegraMaxSockets); ColIndex++ ) {
      if (!IsSocketEnabled (ColIndex)) {
        continue;
      }

      if (RowIndex != ColIndex) {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_CPU_TO_REMOTE_CPU_DISTANCE;
      }
    }

    // CPU to GPU HBM domains
    for (ColIndex = TH500_GPU_HBM_PXM_DOMAIN_START; ColIndex < ProximityDomains; ColIndex++ ) {
      if (!IsSocketEnabled ((ColIndex - TH500_GPU_HBM_PXM_DOMAIN_START)/TH500_GPU_MAX_NR_MEM_PARTITIONS)) {
        continue;
      }

      if ((ColIndex >= TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (RowIndex)) &&
          (ColIndex < (TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (RowIndex) + TH500_GPU_MAX_NR_MEM_PARTITIONS)))
      {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_CPU_TO_LOCAL_HBM_DISTANCE;
      } else {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_CPU_TO_REMOTE_HBM_DISTANCE;
      }
    }
  }

  // Assign adjacent memory distance for all GPU to other GPU domains and GPU HBM domains
  for (RowIndex = TH500_GPU_PXM_DOMAIN_START; RowIndex < ProximityDomains; RowIndex++ ) {
    if (!IsSocketEnabled (RowIndex - TH500_GPU_PXM_DOMAIN_START)) {
      continue;
    }

    // GPU to GPU domains only
    for (ColIndex = TH500_GPU_PXM_DOMAIN_START; ColIndex < (TH500_GPU_PXM_DOMAIN_START + PcdGet32 (PcdTegraMaxSockets)); ColIndex++ ) {
      if (!IsSocketEnabled (ColIndex - TH500_GPU_PXM_DOMAIN_START)) {
        continue;
      }

      if ((RowIndex) != ColIndex) {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_GPU_TO_REMOTE_GPU_DISTANCE;
      }
    }

    // Assign adjacent memory distance for all GPU to GPU HBM domains only
    for (ColIndex = TH500_GPU_HBM_PXM_DOMAIN_START; ColIndex < ProximityDomains; ColIndex++ ) {
      if (!IsSocketEnabled ((ColIndex - TH500_GPU_HBM_PXM_DOMAIN_START)/TH500_GPU_MAX_NR_MEM_PARTITIONS)) {
        continue;
      }

      // check for local vs remote HBM partitions
      if ((ColIndex >= TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (RowIndex - TH500_GPU_PXM_DOMAIN_START)) &&
          (ColIndex < (TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (RowIndex - TH500_GPU_PXM_DOMAIN_START) + TH500_GPU_MAX_NR_MEM_PARTITIONS)))
      {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_GPU_TO_LOCAL_HBM_DISTANCE;
      } else {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_GPU_TO_REMOTE_HBM_DISTANCE;
      }
    }
  }

  // GPU HBM to CPU domains and GPU domains
  for (RowIndex = TH500_GPU_HBM_PXM_DOMAIN_START; RowIndex < ProximityDomains; RowIndex++ ) {
    if (!IsSocketEnabled ((RowIndex - TH500_GPU_HBM_PXM_DOMAIN_START)/TH500_GPU_MAX_NR_MEM_PARTITIONS)) {
      continue;
    }

    // HBM to CPU domains
    for (ColIndex = 0; ColIndex < PcdGet32 (PcdTegraMaxSockets); ColIndex++ ) {
      if (!IsSocketEnabled (ColIndex)) {
        continue;
      }

      // check for local vs remote HBM partitions
      if ((RowIndex >= TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (ColIndex)) &&
          (RowIndex < (TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (ColIndex) + TH500_GPU_MAX_NR_MEM_PARTITIONS)))
      {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_HBM_TO_LOCAL_CPU_DISTANCE;
      } else {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_HBM_TO_REMOTE_CPU_DISTANCE;
      }
    }

    // HBM to GPU domains
    for (ColIndex = TH500_GPU_PXM_DOMAIN_START; ColIndex < (TH500_GPU_PXM_DOMAIN_START + PcdGet32 (PcdTegraMaxSockets)); ColIndex++ ) {
      if (!IsSocketEnabled (ColIndex - TH500_GPU_PXM_DOMAIN_START)) {
        continue;
      }

      // check if it is a local GPU domain
      if (((RowIndex - TH500_GPU_HBM_PXM_DOMAIN_START)/TH500_GPU_MAX_NR_MEM_PARTITIONS) == (ColIndex - TH500_GPU_PXM_DOMAIN_START)) {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_HBM_TO_LOCAL_GPU_DISTANCE;
      } else {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_HBM_TO_REMOTE_GPU_DISTANCE;
      }
    }
  }

  SlitHeader->Header.Length = sizeof (EFI_ACPI_6_4_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_HEADER) +
                              sizeof (UINT8) * ProximityDomains * ProximityDomains;

  // Install Table
  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (PlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (
                                                      PlatformRepositoryInfo[Index].CmObjectSize +
                                                      (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)),
                                                      PlatformRepositoryInfo[Index].CmObjectPtr
                                                      );

      if (NewAcpiTables == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      PlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[PlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_SYSTEM_LOCALITY_INFORMATION_TABLE_SIGNATURE;
      NewAcpiTables[PlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_6_4_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_REVISION;
      NewAcpiTables[PlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdRaw);
      NewAcpiTables[PlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)SlitHeader;
      NewAcpiTables[PlatformRepositoryInfo[Index].CmObjectCount].OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
      NewAcpiTables[PlatformRepositoryInfo[Index].CmObjectCount].OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      PlatformRepositoryInfo[Index].CmObjectCount++;
      PlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (PlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  return EFI_SUCCESS;
}
