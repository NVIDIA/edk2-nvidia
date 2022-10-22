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
#define NORMALIZED_LOCAL_DISTANCE             PcdGet32 (PcdLocalDistance)
#define NORMALIZED_UNREACHABLE_DISTANCE       PcdGet32 (PcdUnreachableDistance)
#define NORMALIZED_CPU_TO_CPU_DISTANCE        PcdGet32 (PcdCpuToCpuDistance)
#define NORMALIZED_GPU_TO_GPU_DISTANCE        PcdGet32 (PcdGpuToGpuDistance)
#define NORMALIZED_CPU_TO_OWN_GPU_DISTANCE    PcdGet32 (PcdCpuToOwnGpuDistance)
#define NORMALIZED_CPU_TO_OTHER_GPU_DISTANCE  PcdGet32 (PcdCpuToOtherGpuDistance)
#define NORMALIZED_GPU_TO_OWN_CPU_DISTANCE    PcdGet32 (PcdGpuToOwnCpuDistance)
#define NORMALIZED_GPU_TO_OTHER_CPU_DISTANCE  PcdGet32 (PcdGpuToOtherCpuDistance)

EFI_STATUS
EFIAPI
InstallStaticLocalityInformationTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CM_ARM_SYSTEM_LOCALITY_INFO     *SysLocalityInfo;
  UINTN                           ProximityDomains;
  UINT8                           *Distance;
  UINT32                          RowIndex;
  UINT32                          ColIndex;

  Repo = *PlatformRepositoryInfo;

  SysLocalityInfo = (CM_ARM_SYSTEM_LOCALITY_INFO *)AllocateZeroPool (sizeof (CM_ARM_SYSTEM_LOCALITY_INFO));

  // Create a 2D distance matrix with locality information across all possible proximity domains
  // Each CPU and GPU Socket is a proximity domain

  ProximityDomains = TH500_GPU_DOMAIN_START + PcdGet32 (PcdTegraMaxSockets);

  Distance = (UINT8 *)AllocateZeroPool (sizeof (UINT8) * ProximityDomains * ProximityDomains);
  if (Distance == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate distance matrix entry\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

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

  // Assign adjacent memory distance for all CPU to other domains (CPU and GPU)
  for (RowIndex = 0; RowIndex < PcdGet32 (PcdTegraMaxSockets); RowIndex++ ) {
    if (!IsSocketEnabled (RowIndex)) {
      continue;
    }

    // CPU to CPU domains only
    for (ColIndex = 0; ColIndex < PcdGet32 (PcdTegraMaxSockets); ColIndex++ ) {
      if (!IsSocketEnabled (ColIndex)) {
        continue;
      }

      if (RowIndex != ColIndex) {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_CPU_TO_CPU_DISTANCE;
      }
    }

    // CPU to GPU domains only
    for (ColIndex = TH500_GPU_DOMAIN_START; ColIndex < (TH500_GPU_DOMAIN_START + PcdGet32 (PcdTegraMaxSockets)); ColIndex++ ) {
      if (!IsSocketEnabled (ColIndex - TH500_GPU_DOMAIN_START)) {
        continue;
      }

      if (RowIndex == (ColIndex - TH500_GPU_DOMAIN_START)) {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_CPU_TO_OWN_GPU_DISTANCE;
      } else {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_CPU_TO_OTHER_GPU_DISTANCE;
      }
    }
  }

  // Assign adjacent memory distance for all GPU to other domains (CPU and GPU)
  for (RowIndex = TH500_GPU_DOMAIN_START; RowIndex < (TH500_GPU_DOMAIN_START + PcdGet32 (PcdTegraMaxSockets)); RowIndex++ ) {
    if (!IsSocketEnabled (RowIndex - TH500_GPU_DOMAIN_START)) {
      continue;
    }

    // GPU to CPU domains only
    for (ColIndex = 0; ColIndex < PcdGet32 (PcdTegraMaxSockets); ColIndex++ ) {
      if (!IsSocketEnabled (ColIndex)) {
        continue;
      }

      if ((RowIndex-TH500_GPU_DOMAIN_START) == ColIndex) {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_GPU_TO_OWN_CPU_DISTANCE;
      } else {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_GPU_TO_OTHER_CPU_DISTANCE;
      }
    }

    // GPU to GPU domains only
    for (ColIndex = TH500_GPU_DOMAIN_START; ColIndex < (TH500_GPU_DOMAIN_START+ PcdGet32 (PcdTegraMaxSockets)); ColIndex++ ) {
      if (!IsSocketEnabled (ColIndex - TH500_GPU_DOMAIN_START)) {
        continue;
      }

      if (RowIndex != ColIndex) {
        Distance[RowIndex * ProximityDomains + ColIndex] = NORMALIZED_GPU_TO_GPU_DISTANCE;
      }
    }
  }

  SysLocalityInfo->NumSystemLocalities = ProximityDomains;
  SysLocalityInfo->Distance            = Distance;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjSystemLocalityInfo);
  Repo->CmObjectSize  = sizeof (CM_ARM_SYSTEM_LOCALITY_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = SysLocalityInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);

  *PlatformRepositoryInfo = Repo;

  return EFI_SUCCESS;
}
