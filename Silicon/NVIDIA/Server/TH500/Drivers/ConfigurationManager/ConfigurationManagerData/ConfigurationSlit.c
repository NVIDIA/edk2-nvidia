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

#include <IndustryStandard/Slit.h>
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
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CM_STD_OBJ_ACPI_TABLE_INFO      *NewAcpiTables;
  UINTN                           Index;
  CM_ARM_SYSTEM_LOCALITY_INFO     *SysLocalityInfo;
  UINTN                           ProximityDomains;
  UINT8                           *Distance;
  UINT32                          RowIndex;
  UINT32                          ColIndex;
  UINT32                          MaxEnabledHbmDmns;

  // Create a ACPI Table Entry
  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (
                                                      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize +
                                                      (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)),
                                                      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr
                                                      );

      if (NewAcpiTables == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_SYSTEM_LOCALITY_INFORMATION_TABLE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_6_4_SYSTEM_LOCALITY_INFORMATION_TABLE_REVISION;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSlit);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = NULL;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId         = 0;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  Repo = *PlatformRepositoryInfo;

  SysLocalityInfo = (CM_ARM_SYSTEM_LOCALITY_INFO *)AllocateZeroPool (sizeof (CM_ARM_SYSTEM_LOCALITY_INFO));

  // Create a 2D distance matrix with locality information across all possible proximity domains
  // Each CPU, GPU and GPU HBM is a proximity domain

  MaxEnabledHbmDmns = GetMaxHbmPxmDomains ();
  ProximityDomains  = (TH500_GPU_HBM_PXM_DOMAIN_START > MaxEnabledHbmDmns) ? TH500_GPU_HBM_PXM_DOMAIN_START : MaxEnabledHbmDmns;

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

  SysLocalityInfo->NumSystemLocalities = ProximityDomains;
  SysLocalityInfo->Distance            = Distance;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjSystemLocalityInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_ARM_SYSTEM_LOCALITY_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = SysLocalityInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);

  *PlatformRepositoryInfo = Repo;

  return EFI_SUCCESS;
}
