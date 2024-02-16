/** @file
  Configuration Manager Data of Static Resource Affinity Table

  SPDX-FileCopyrightText: Copyright (c) 2020-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include <TH500/TH500Definitions.h>
#include "ConfigurationManagerDataPrivate.h"

typedef struct {
  UINT32    PxmDmn;
  UINT64    HbmSize;
  UINT64    HbmBase;
} HBM_MEMORY_INFO;

EFI_STATUS
EFIAPI
InstallStaticResourceAffinityTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  )
{
  UINTN                           Index;
  UINTN                           Socket;
  EFI_STATUS                      Status;
  CM_STD_OBJ_ACPI_TABLE_INFO      *NewAcpiTables;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CM_ARM_MEMORY_AFFINITY_INFO     *MemoryAffinityInfo;
  HBM_MEMORY_INFO                 *HbmMemInfo;
  UINTN                           MemoryAffinityInfoCount;
  UINTN                           MemoryAffinityInfoIndex;
  UINTN                           GpuMemoryAffinityId;
  UINT8                           NumEnabledSockets;
  UINT8                           NumGpuEnabledSockets;
  EFI_HANDLE                      *Handles = NULL;
  UINTN                           NumberOfHandles;
  UINTN                           HandleIdx;
  VOID                            *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO    *PlatformResourceInfo;

  // Get platform resource info
  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to get PlatformResourceInfo\n"));
    return EFI_NOT_FOUND;
  }

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

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_SYSTEM_RESOURCE_AFFINITY_TABLE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_6_4_SYSTEM_RESOURCE_AFFINITY_TABLE_REVISION;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSrat);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = NULL;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].MinorRevision      = 0;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  Repo = *PlatformRepositoryInfo;

  NumEnabledSockets       = 0;
  NumGpuEnabledSockets    = 0;
  MemoryAffinityInfoCount = PlatformResourceInfo->ResourceInfo->DramRegionsCount;

  for (Socket = 0; Socket < PLATFORM_MAX_SOCKETS; Socket++) {
    if (IsSocketEnabled (Socket)) {
      NumEnabledSockets++;
    }

    if (IsGpuEnabledOnSocket (Socket)) {
      NumGpuEnabledSockets++;
    }
  }

  // Increment to hold entries for EGM memory in case of hypervisor
  if (PlatformResourceInfo->HypervisorMode) {
    MemoryAffinityInfoCount += NumEnabledSockets;
  }

  // Increment to hold entries for GPU memory
  MemoryAffinityInfoCount += TH500_GPU_MAX_NR_MEM_PARTITIONS * NumGpuEnabledSockets;

  MemoryAffinityInfo = (CM_ARM_MEMORY_AFFINITY_INFO *)AllocateZeroPool (sizeof (CM_ARM_MEMORY_AFFINITY_INFO) * MemoryAffinityInfoCount);
  if (MemoryAffinityInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory affinity info\r\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  for (MemoryAffinityInfoIndex = 0; MemoryAffinityInfoIndex < PlatformResourceInfo->ResourceInfo->DramRegionsCount; MemoryAffinityInfoIndex++) {
    MemoryAffinityInfo[MemoryAffinityInfoIndex].ProximityDomain = TH500_AMAP_GET_SOCKET (PlatformResourceInfo->ResourceInfo->DramRegions[MemoryAffinityInfoIndex].MemoryBaseAddress);
    MemoryAffinityInfo[MemoryAffinityInfoIndex].BaseAddress     = PlatformResourceInfo->ResourceInfo->DramRegions[MemoryAffinityInfoIndex].MemoryBaseAddress;
    MemoryAffinityInfo[MemoryAffinityInfoIndex].Length          = PlatformResourceInfo->ResourceInfo->DramRegions[MemoryAffinityInfoIndex].MemoryLength;
    MemoryAffinityInfo[MemoryAffinityInfoIndex].Flags           = EFI_ACPI_6_4_MEMORY_ENABLED;
  }

  // Allocate space to save EGM info in case of hypervisor
  if (PlatformResourceInfo->HypervisorMode) {
    for (Socket = 0; Socket < PLATFORM_MAX_SOCKETS; Socket++) {
      if (!IsSocketEnabled (Socket)) {
        continue;
      }

      MemoryAffinityInfo[MemoryAffinityInfoIndex].ProximityDomain = TH500_HV_EGM_PXM_DOMAIN_START + Socket;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].BaseAddress     = PlatformResourceInfo->EgmMemoryInfo[Socket].Base;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].Length          = PlatformResourceInfo->EgmMemoryInfo[Socket].Size;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].Flags           = EFI_ACPI_6_4_MEMORY_ENABLED;
      MemoryAffinityInfoIndex++;
    }
  }

  // Allocate space to save HBM info
  HbmMemInfo = (HBM_MEMORY_INFO *)AllocateZeroPool (sizeof (HBM_MEMORY_INFO) * TH500_TOTAL_PROXIMITY_DOMAINS);
  if (HbmMemInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate HBM memory info\r\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  // Retrieve HBM memory info from PCI Root Bridge Protocol
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate root bridge protocols, %r.\r\n", __FUNCTION__, NumberOfHandles));
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  for (HandleIdx = 0; HandleIdx < NumberOfHandles; HandleIdx++) {
    NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *PciRbCfg = NULL;
    Status = gBS->HandleProtocol (
                    Handles[HandleIdx],
                    &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                    (VOID **)&PciRbCfg
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get protocol for handle %p, %r.\r\n",
        __FUNCTION__,
        Handles[HandleIdx],
        Status
        ));
      goto Exit;
    }

    if (PciRbCfg->NumProximityDomains > 0) {
      // found the GPU HBM info
      for (UINTN Idx = 0; Idx < PciRbCfg->NumProximityDomains; Idx++ ) {
        HbmMemInfo[PciRbCfg->ProximityDomainStart + Idx].PxmDmn  = PciRbCfg->ProximityDomainStart + Idx;
        HbmMemInfo[PciRbCfg->ProximityDomainStart + Idx].HbmSize = PciRbCfg->HbmRangeSize / PciRbCfg->NumProximityDomains;
        HbmMemInfo[PciRbCfg->ProximityDomainStart + Idx].HbmBase = PciRbCfg->HbmRangeStart +
                                                                   (PciRbCfg->HbmRangeSize / PciRbCfg->NumProximityDomains * Idx);
      }
    }
  }

  // Placeholder node for all domains, actual entries will be present in DSDT
  // Create structure entries for enabled GPUs
  for (Socket = 0; Socket < PLATFORM_MAX_SOCKETS; Socket++) {
    if (!IsGpuEnabledOnSocket (Socket)) {
      continue;
    }

    for (GpuMemoryAffinityId = 0; GpuMemoryAffinityId < TH500_GPU_MAX_NR_MEM_PARTITIONS; GpuMemoryAffinityId++) {
      MemoryAffinityInfo[MemoryAffinityInfoIndex].ProximityDomain = TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (Socket) + GpuMemoryAffinityId;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].Flags           = EFI_ACPI_6_4_MEMORY_ENABLED|EFI_ACPI_6_4_MEMORY_HOT_PLUGGABLE;
      MemoryAffinityInfoIndex++;
    }
  }

  FreePool (HbmMemInfo);

  ASSERT (MemoryAffinityInfoIndex == MemoryAffinityInfoCount);

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjMemoryAffinityInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_ARM_MEMORY_AFFINITY_INFO) * MemoryAffinityInfoCount;
  Repo->CmObjectCount = MemoryAffinityInfoCount;
  Repo->CmObjectPtr   = MemoryAffinityInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);

  *PlatformRepositoryInfo = Repo;
Exit:
  if (Handles != NULL) {
    FreePool (Handles);
  }

  return Status;
}
