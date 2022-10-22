/** @file
  Configuration Manager Data of Static Resource Affinity Table

  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/FloorSweepingLib.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include <TH500/TH500Definitions.h>

#define PLATFORM_MAX_SOCKETS  (PcdGet32 (PcdTegraMaxSockets))

UINT32
EFIAPI
GetAffinityDomain (
  IN UINT64  BaseAddress
  )
{
  VOID                 *Hob;
  TEGRA_RESOURCE_INFO  *ResourceInfo;
  UINT32               Count;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    ResourceInfo = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ResourceInfo;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting affinity domain\n",
      __FUNCTION__
      ));
    ASSERT (FALSE);
    return MAX_UINT32;
  }

  for (Count = 0; Count < ResourceInfo->DramRegionsCount; Count++) {
    if ((BaseAddress >= ResourceInfo->InputDramRegions[Count].MemoryBaseAddress) &&
        (BaseAddress < ResourceInfo->InputDramRegions[Count].MemoryBaseAddress +
         ResourceInfo->InputDramRegions[Count].MemoryLength))
    {
      break;
    }
  }

  ASSERT (Count < ResourceInfo->DramRegionsCount);

  return Count;
}

EFI_STATUS
EFIAPI
InstallStaticResourceAffinityTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd
  )
{
  UINTN                            Index;
  UINTN                            Socket;
  EFI_STATUS                       Status;
  EDKII_PLATFORM_REPOSITORY_INFO   *Repo;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *Descriptors;
  UINTN                            DescriptorCount;
  CM_ARM_MEMORY_AFFINITY_INFO      *MemoryAffinityInfo;
  UINTN                            MemoryAffinityInfoCount;
  UINTN                            MemoryAffinityInfoIndex;
  UINTN                            GpuMemoryAffinityId;
  UINT8                            NumEnabledSockets;

  Repo = *PlatformRepositoryInfo;

  MemoryAffinityInfoCount = 0;
  NumEnabledSockets       = 0;

  Status = gDS->GetMemorySpaceMap (&DescriptorCount, &Descriptors);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get Memory Space Map: %r\r\n", Status));
    return EFI_DEVICE_ERROR;
  }

  for (Index = 0; Index < DescriptorCount; Index++) {
    if (Descriptors[Index].GcdMemoryType == EfiGcdMemoryTypeSystemMemory) {
      MemoryAffinityInfoCount++;
    }
  }

  // Should be no way to get this far in boot without system memory
  ASSERT (MemoryAffinityInfoCount != 0);

  for (Socket = 0; Socket < PLATFORM_MAX_SOCKETS; Socket++) {
    if (IsSocketEnabled (Socket)) {
      NumEnabledSockets++;
    }
  }

  // Increment to hold dummy entry for GPU memory
  MemoryAffinityInfoCount += TH500_GPU_MAX_NR_MEM_PARTITIONS * NumEnabledSockets;

  MemoryAffinityInfo = (CM_ARM_MEMORY_AFFINITY_INFO *)AllocatePool (sizeof (CM_ARM_MEMORY_AFFINITY_INFO) * MemoryAffinityInfoCount);
  if (MemoryAffinityInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory affinity info\r\n"));
    return EFI_DEVICE_ERROR;
  }

  MemoryAffinityInfoIndex = 0;
  for (Index = 0; Index < DescriptorCount; Index++) {
    if (Descriptors[Index].GcdMemoryType == EfiGcdMemoryTypeSystemMemory) {
      MemoryAffinityInfo[MemoryAffinityInfoIndex].ProximityDomain = GetAffinityDomain (Descriptors[Index].BaseAddress);
      MemoryAffinityInfo[MemoryAffinityInfoIndex].BaseAddress     = Descriptors[Index].BaseAddress;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].Length          = Descriptors[Index].Length;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].Flags           = EFI_ACPI_6_4_MEMORY_ENABLED;
      MemoryAffinityInfoIndex++;
    }
  }

  FreePool (Descriptors);

  // Placeholder node for all domains, actual entries will be present in DSDT
  for (Socket = 0; Socket < PLATFORM_MAX_SOCKETS; Socket++) {
    if (!IsSocketEnabled (Socket)) {
      continue;
    }

    for (GpuMemoryAffinityId = 0; GpuMemoryAffinityId < TH500_GPU_MAX_NR_MEM_PARTITIONS; GpuMemoryAffinityId++) {
      MemoryAffinityInfo[MemoryAffinityInfoIndex].ProximityDomain = TH500_GPU_PXM_START (Socket) + GpuMemoryAffinityId;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].BaseAddress     = 0;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].Length          = 0;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].Flags           = EFI_ACPI_6_4_MEMORY_ENABLED|EFI_ACPI_6_4_MEMORY_HOT_PLUGGABLE;
      MemoryAffinityInfoIndex++;
    }
  }

  ASSERT (MemoryAffinityInfoIndex == MemoryAffinityInfoCount);

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjMemoryAffinityInfo);
  Repo->CmObjectSize  = sizeof (CM_ARM_MEMORY_AFFINITY_INFO) * MemoryAffinityInfoCount;
  Repo->CmObjectCount = MemoryAffinityInfoCount;
  Repo->CmObjectPtr   = MemoryAffinityInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);

  *PlatformRepositoryInfo = Repo;

  return EFI_SUCCESS;
}
