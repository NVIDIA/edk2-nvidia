/** @file

  Numa Information Library

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NumaInfoLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>

#include <IndustryStandard/Acpi65.h>

#include <TH500/TH500Definitions.h>

#define UNREACHABLE_LATENCY    0xFFFF
#define UNREACHABLE_BANDWIDTH  0xFFFF
#define NORMALIZED_DISTANCE    10
#define UNREACHABLE_DISTANCE   0xFF

typedef enum {
  CPU_TO_LOCAL_MEMORY = 0,
  CPU_TO_REMOTE_MEMORY,
  CPU_TO_LOCAL_HBM,
  CPU_TO_REMOTE_HBM,
  CPU_TRANSFER_MAX = CPU_TO_REMOTE_HBM,
  GPU_TO_LOCAL_MEMORY,
  GPU_TO_REMOTE_MEMORY,
  GPU_TO_LOCAL_HBM,
  GPU_TO_REMOTE_HBM,
  GPU_TO_REMOTE_HBM_SAME_SOCKET,
  GPU_TRANSFER_MAX = GPU_TO_REMOTE_HBM_SAME_SOCKET,
  MAX_MEMORY_TRANSFER_TYPES
} MEMORY_TRANSFER_TYPE;

UINT32  mMemoryTransferReadLatency[MAX_MEMORY_TRANSFER_TYPES];
UINT32  mMemoryTransferWriteLatency[MAX_MEMORY_TRANSFER_TYPES];
UINT32  mMemoryTransferBandwidth[MAX_MEMORY_TRANSFER_TYPES];
UINT8   mMemoryTransferNormalizedDistance[MAX_MEMORY_TRANSFER_TYPES];

STATIC UINTN                  mNumberOfDomains = 0;
STATIC NUMA_INFO_DOMAIN_INFO  *mNumaInfo       = NULL;

/**
  Returns limits of the proximity domains

  @param[out]   MaxProximityDomain        Pointer to Maximum Numa domain number
  @param[out]   NumberOfInitiatorDomains  Pointer to Number of Initiator domains
  @param[out]   NumberOfTargetDomains     Pointer to Number of Target domains

  @retval       EFI_SUCCESS     Limits returned successfully
  @retval       EFI_NOT_FOUND   Limits not found

**/
EFI_STATUS
EFIAPI
NumaInfoGetDomainLimits (
  OUT UINT32  *MaxProximityDomain,
  OUT UINT32  *NumberOfInitiatorDomains,
  OUT UINT32  *NumberOfTargetDomains
  )
{
  if (mNumaInfo == NULL) {
    return EFI_NOT_FOUND;
  }

  *MaxProximityDomain       = 0;
  *NumberOfInitiatorDomains = 0;
  *NumberOfTargetDomains    = 0;

  for (UINTN Index = 0; Index < mNumberOfDomains; Index++) {
    if (mNumaInfo[Index].ProximityDomain > *MaxProximityDomain) {
      *MaxProximityDomain = mNumaInfo[Index].ProximityDomain;
    }

    if (mNumaInfo[Index].InitiatorDomain) {
      (*NumberOfInitiatorDomains)++;
    }

    if (mNumaInfo[Index].TargetDomain) {
      (*NumberOfTargetDomains)++;
    }
  }

  return EFI_SUCCESS;
}

/**
  Returns the Numa info for a given domain

  @param[in]    ProximityDomain     Proximity domain number
  @param[out]   DomainInfo          Pointer to NUMA_INFO_DOMAIN_INFO structure

  @retval       EFI_SUCCESS     Info returned successfully
  @retval       EFI_NOT_FOUND   Domain not found

**/
EFI_STATUS
EFIAPI
NumaInfoGetDomainDetails (
  IN  UINT32                 ProximityDomain,
  OUT NUMA_INFO_DOMAIN_INFO  *DomainInfo
  )
{
  if (mNumaInfo == NULL) {
    return EFI_NOT_FOUND;
  }

  for (UINTN Index = 0; Index < mNumberOfDomains; Index++) {
    if (mNumaInfo[Index].ProximityDomain == ProximityDomain) {
      CopyMem (DomainInfo, &mNumaInfo[Index], sizeof (NUMA_INFO_DOMAIN_INFO));
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Get type of transfer between two domains

  @param[in]    InitiatorInfo     Initiator domain info
  @param[in]    TargetInfo        Target domain info

  @retval       MEMORY_TRANSFER_TYPE     Type of transfer
 */
STATIC
MEMORY_TRANSFER_TYPE
EFIAPI
GetMemoryTransferType (
  IN  NUMA_INFO_DOMAIN_INFO  InitiatorInfo,
  IN  NUMA_INFO_DOMAIN_INFO  TargetInfo
  )
{
  if (InitiatorInfo.DeviceType != NUMA_INFO_TYPE_GPU) {
    if (TargetInfo.DeviceType != NUMA_INFO_TYPE_GPU) {
      if (InitiatorInfo.SocketId == TargetInfo.SocketId) {
        return CPU_TO_LOCAL_MEMORY;
      } else {
        return CPU_TO_REMOTE_MEMORY;
      }
    } else {
      if (InitiatorInfo.SocketId == TargetInfo.SocketId) {
        return CPU_TO_LOCAL_HBM;
      } else {
        return CPU_TO_REMOTE_HBM;
      }
    }
  } else {
    if (TargetInfo.DeviceType != NUMA_INFO_TYPE_GPU) {
      if (InitiatorInfo.SocketId == TargetInfo.SocketId) {
        return GPU_TO_LOCAL_MEMORY;
      } else {
        return GPU_TO_REMOTE_MEMORY;
      }
    } else {
      if (InitiatorInfo.SocketId == TargetInfo.SocketId) {
        if ((TargetInfo.DeviceHandle.Pci.PciSegment == InitiatorInfo.DeviceHandle.Pci.PciSegment) &&
            (TargetInfo.DeviceHandle.Pci.PciBdfNumber == InitiatorInfo.DeviceHandle.Pci.PciBdfNumber))
        {
          return GPU_TO_LOCAL_HBM;
        } else {
          return GPU_TO_REMOTE_HBM_SAME_SOCKET;
        }
      } else {
        return GPU_TO_REMOTE_HBM;
      }
    }
  }
}

/**
  Returns the distance between two domains

  @param[in]    InitiatorDomain     Initiator domain number
  @param[in]    TargetDomain        Target domain number
  @param[out]   NormalizedDistance  Pointer to Normalized distance
  @param[out]   ReadLatency         Pointer to Read latency
  @param[out]   WriteLatency        Pointer to Write latency
  @param[out]   AccessBandwidth     Pointer to Access bandwidth

  @retval       EFI_SUCCESS     Distance returned successfully

 */
EFI_STATUS
EFIAPI
NumaInfoGetDistances (
  IN  UINT32  InitiatorDomain,
  IN  UINT32  TargetDomain,
  OUT UINT8   *NormalizedDistance OPTIONAL,
  OUT UINT16  *ReadLatency OPTIONAL,
  OUT UINT16  *WriteLatency OPTIONAL,
  OUT UINT16  *AccessBandwidth OPTIONAL
  )
{
  NUMA_INFO_DOMAIN_INFO  InitiatorInfo;
  NUMA_INFO_DOMAIN_INFO  TargetInfo;
  UINT8                  LocalNormalizedDistance;
  UINT16                 LocalReadLatency;
  UINT16                 LocalWriteLatency;
  UINT16                 LocalAccessBandwidth;
  BOOLEAN                Unreachable;
  MEMORY_TRANSFER_TYPE   MemoryTranferType;

  Unreachable = FALSE;
  if (mNumaInfo == NULL) {
    Unreachable = TRUE;
  }

  if (EFI_ERROR (NumaInfoGetDomainDetails (InitiatorDomain, &InitiatorInfo))) {
    Unreachable = TRUE;
  }

  if (EFI_ERROR (NumaInfoGetDomainDetails (TargetDomain, &TargetInfo))) {
    Unreachable = TRUE;
  }

  if ((InitiatorInfo.InitiatorDomain == FALSE) || (TargetInfo.TargetDomain == FALSE)) {
    Unreachable = TRUE;
  }

  if (Unreachable) {
    LocalAccessBandwidth = UNREACHABLE_BANDWIDTH;
    LocalReadLatency     = UNREACHABLE_LATENCY;
    LocalWriteLatency    = UNREACHABLE_LATENCY;
    if (InitiatorDomain == TargetDomain) {
      LocalNormalizedDistance = NORMALIZED_DISTANCE;
    } else {
      LocalNormalizedDistance = UNREACHABLE_DISTANCE;
    }
  } else {
    MemoryTranferType       = GetMemoryTransferType (InitiatorInfo, TargetInfo);
    LocalAccessBandwidth    = mMemoryTransferBandwidth[MemoryTranferType];
    LocalReadLatency        = mMemoryTransferReadLatency[MemoryTranferType];
    LocalWriteLatency       = mMemoryTransferWriteLatency[MemoryTranferType];
    LocalNormalizedDistance = mMemoryTransferNormalizedDistance[MemoryTranferType];
    // Increment the distance if the domains are different but the access is local
    if ((InitiatorDomain != TargetDomain) && (LocalNormalizedDistance == NORMALIZED_DISTANCE)) {
      LocalNormalizedDistance += 1;
    }
  }

  if (NormalizedDistance != NULL) {
    *NormalizedDistance = LocalNormalizedDistance;
  }

  if (ReadLatency != NULL) {
    *ReadLatency = LocalReadLatency;
  }

  if (WriteLatency != NULL) {
    *WriteLatency = LocalWriteLatency;
  }

  if (AccessBandwidth != NULL) {
    *AccessBandwidth = LocalAccessBandwidth;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NumaInfoLibConstructor (
  VOID
  )
{
  EFI_STATUS                                        Status;
  UINTN                                             NumberOfSockets;
  UINTN                                             NumberOfGpus;
  UINTN                                             NumberOfGpuDomains;
  UINTN                                             NumberOfRootBridges;
  UINTN                                             Index;
  UINTN                                             Index2;
  UINTN                                             NumaIndex;
  UINTN                                             DomainIndex;
  EFI_HANDLE                                        *RootBridgeHandles;
  EFI_HANDLE                                        *HandleBuffer;
  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *RootBridgeCfgIo;
  EFI_PCI_IO_PROTOCOL                               *PciIo;
  UINTN                                             SegmentNumber;
  UINTN                                             BusNumber;
  UINTN                                             DeviceNumber;
  UINTN                                             FunctionNumber;
  VOID                                              *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO                      *PlatformResourceInfo;

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

  // Get all the Sockets
  NumberOfSockets = 0;
  for (Index = 0; Index < PcdGet32 (PcdTegraMaxSockets); Index++) {
    if (IsSocketEnabled (Index)) {
      NumberOfSockets++;
    }
  }

  // Get all the GPUs
  NumberOfGpuDomains = 0;
  Status             = gBS->LocateHandleBuffer (
                              ByProtocol,
                              &gEfiNVIDIAGpuDSDAMLGenerationProtocolGuid,
                              NULL,
                              &NumberOfGpus,
                              &HandleBuffer
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get GPU DSDAMLGeneration Protocol handles - %r\n", Status));
    NumberOfGpus = 0;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                  NULL,
                  &NumberOfRootBridges,
                  &RootBridgeHandles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get Root Bridge Configuration IO Protocol handles - %r\n", Status));
    NumberOfRootBridges = 0;
    NumberOfGpus        = 0;
  }

  for (Index = 0; Index < NumberOfGpus; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiPciIoProtocolGuid,
                    (VOID **)&PciIo
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = PciIo->GetLocation (PciIo, &SegmentNumber, &BusNumber, &DeviceNumber, &FunctionNumber);
    if (EFI_ERROR (Status)) {
      continue;
    }

    for (Index2 = 0; Index2 < NumberOfRootBridges; Index2++) {
      Status = gBS->HandleProtocol (
                      RootBridgeHandles[Index2],
                      &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                      (VOID **)&RootBridgeCfgIo
                      );
      if (EFI_ERROR (Status)) {
        continue;
      }

      if (SegmentNumber != RootBridgeCfgIo->SegmentNumber) {
        continue;
      }

      NumberOfGpuDomains += RootBridgeCfgIo->NumProximityDomains;
      break;
    }
  }

  mNumberOfDomains = NumberOfSockets + NumberOfGpuDomains;
  if (PlatformResourceInfo->HypervisorMode) {
    mNumberOfDomains += NumberOfSockets;
  }

  mNumaInfo = AllocateZeroPool (sizeof (NUMA_INFO_DOMAIN_INFO) * mNumberOfDomains);
  if (mNumaInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  NumaIndex = 0;
  for (Index = 0; Index < PcdGet32 (PcdTegraMaxSockets); Index++) {
    if (IsSocketEnabled (Index)) {
      mNumaInfo[NumaIndex].ProximityDomain               = Index;
      mNumaInfo[NumaIndex].SocketId                      = Index;
      mNumaInfo[NumaIndex].DeviceType                    = NUMA_INFO_TYPE_CPU;
      mNumaInfo[NumaIndex].DeviceHandleType              = 0;
      mNumaInfo[NumaIndex].DeviceHandle.Pci.PciBdfNumber = 0;
      mNumaInfo[NumaIndex].DeviceHandle.Pci.PciSegment   = 0;
      mNumaInfo[NumaIndex].InitiatorDomain               = TRUE;
      mNumaInfo[NumaIndex].TargetDomain                  = TRUE;
      NumaIndex++;
    }
  }

  ASSERT (NumaIndex == NumberOfSockets);

  if (PlatformResourceInfo->HypervisorMode) {
    for (Index = 0; Index < PcdGet32 (PcdTegraMaxSockets); Index++) {
      if (IsSocketEnabled (Index)) {
        mNumaInfo[NumaIndex].ProximityDomain               = TH500_HV_EGM_PXM_DOMAIN_START + Index;
        mNumaInfo[NumaIndex].SocketId                      = Index;
        mNumaInfo[NumaIndex].DeviceType                    = NUMA_INFO_TYPE_HV;
        mNumaInfo[NumaIndex].DeviceHandleType              = 0;
        mNumaInfo[NumaIndex].DeviceHandle.Pci.PciBdfNumber = 0;
        mNumaInfo[NumaIndex].DeviceHandle.Pci.PciSegment   = 0;
        mNumaInfo[NumaIndex].InitiatorDomain               = FALSE;
        mNumaInfo[NumaIndex].TargetDomain                  = TRUE;
        NumaIndex++;
      }
    }

    ASSERT (NumaIndex == (2 * NumberOfSockets));
  }

  for (Index = 0; Index < NumberOfGpus; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiPciIoProtocolGuid,
                    (VOID **)&PciIo
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = PciIo->GetLocation (PciIo, &SegmentNumber, &BusNumber, &DeviceNumber, &FunctionNumber);
    if (EFI_ERROR (Status)) {
      continue;
    }

    for (Index2 = 0; Index2 < NumberOfRootBridges; Index2++) {
      Status = gBS->HandleProtocol (
                      RootBridgeHandles[Index2],
                      &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                      (VOID **)&RootBridgeCfgIo
                      );
      if (EFI_ERROR (Status)) {
        continue;
      }

      if (SegmentNumber != RootBridgeCfgIo->SegmentNumber) {
        continue;
      }

      for (DomainIndex = 0; DomainIndex < RootBridgeCfgIo->NumProximityDomains; DomainIndex++) {
        mNumaInfo[NumaIndex].ProximityDomain               = RootBridgeCfgIo->ProximityDomainStart + DomainIndex;
        mNumaInfo[NumaIndex].SocketId                      = RootBridgeCfgIo->SocketID;
        mNumaInfo[NumaIndex].DeviceType                    = NUMA_INFO_TYPE_GPU;
        mNumaInfo[NumaIndex].DeviceHandleType              = EFI_ACPI_6_5_PCI_DEVICE_HANDLE;
        mNumaInfo[NumaIndex].DeviceHandle.Pci.PciBdfNumber = SwapBytes16 (BusNumber << 8 | DeviceNumber << 3 | FunctionNumber);
        mNumaInfo[NumaIndex].DeviceHandle.Pci.PciSegment   = SegmentNumber;
        mNumaInfo[NumaIndex].InitiatorDomain               = FALSE;
        mNumaInfo[NumaIndex].TargetDomain                  = TRUE;
        NumaIndex++;
      }

      break;
    }
  }

  for (DomainIndex = 0; DomainIndex < mNumberOfDomains; DomainIndex++) {
    DEBUG ((
      DEBUG_INFO,
      "ProximityDomain: %u, SocketId: %u, DeviceType: %d, DeviceHandleType: %u, DeviceHandle.Pci.PciSegment: %u, DeviceHandle.Pci.PciBdfNumber: %u\n",
      mNumaInfo[DomainIndex].ProximityDomain,
      mNumaInfo[DomainIndex].SocketId,
      mNumaInfo[DomainIndex].DeviceType,
      mNumaInfo[DomainIndex].DeviceHandleType,
      mNumaInfo[DomainIndex].DeviceHandle.Pci.PciSegment,
      mNumaInfo[DomainIndex].DeviceHandle.Pci.PciBdfNumber
      ));
    DEBUG ((
      DEBUG_INFO,
      "InitiatorDomain: %u, TargetDomain: %u\n",
      mNumaInfo[DomainIndex].InitiatorDomain,
      mNumaInfo[DomainIndex].TargetDomain
      ));
  }

  // Read the memory transfer latency and bandwidth from PCDs
  mMemoryTransferReadLatency[CPU_TO_LOCAL_MEMORY]        = PcdGet32 (PcdCpuToLocalCpuReadLatency);
  mMemoryTransferWriteLatency[CPU_TO_LOCAL_MEMORY]       = PcdGet32 (PcdCpuToLocalCpuWriteLatency);
  mMemoryTransferBandwidth[CPU_TO_LOCAL_MEMORY]          = PcdGet32 (PcdCpuToLocalCpuAccessBandwidth);
  mMemoryTransferNormalizedDistance[CPU_TO_LOCAL_MEMORY] = NORMALIZED_DISTANCE;

  mMemoryTransferReadLatency[CPU_TO_REMOTE_MEMORY]        = PcdGet32 (PcdCpuToRemoteCpuReadLatency);
  mMemoryTransferWriteLatency[CPU_TO_REMOTE_MEMORY]       = PcdGet32 (PcdCpuToRemoteCpuWriteLatency);
  mMemoryTransferBandwidth[CPU_TO_REMOTE_MEMORY]          = PcdGet32 (PcdCpuToRemoteCpuAccessBandwidth);
  mMemoryTransferNormalizedDistance[CPU_TO_REMOTE_MEMORY] = PcdGet32 (PcdCpuToRemoteCpuDistance);

  mMemoryTransferReadLatency[CPU_TO_LOCAL_HBM]        = PcdGet32 (PcdCpuToLocalHbmReadLatency);
  mMemoryTransferWriteLatency[CPU_TO_LOCAL_HBM]       = PcdGet32 (PcdCpuToLocalHbmWriteLatency);
  mMemoryTransferBandwidth[CPU_TO_LOCAL_HBM]          = PcdGet32 (PcdCpuToLocalHbmAccessBandwidth);
  mMemoryTransferNormalizedDistance[CPU_TO_LOCAL_HBM] = PcdGet32 (PcdCpuToLocalHbmDistance);

  mMemoryTransferReadLatency[CPU_TO_REMOTE_HBM]        = PcdGet32 (PcdCpuToRemoteHbmReadLatency);
  mMemoryTransferWriteLatency[CPU_TO_REMOTE_HBM]       = PcdGet32 (PcdCpuToRemoteHbmWriteLatency);
  mMemoryTransferBandwidth[CPU_TO_REMOTE_HBM]          = PcdGet32 (PcdCpuToRemoteHbmAccessBandwidth);
  mMemoryTransferNormalizedDistance[CPU_TO_REMOTE_HBM] = PcdGet32 (PcdCpuToRemoteHbmDistance);

  mMemoryTransferReadLatency[GPU_TO_LOCAL_MEMORY]        = PcdGet32 (PcdGpuToLocalCpuReadLatency);
  mMemoryTransferWriteLatency[GPU_TO_LOCAL_MEMORY]       = PcdGet32 (PcdGpuToLocalCpuWriteLatency);
  mMemoryTransferBandwidth[GPU_TO_LOCAL_MEMORY]          = PcdGet32 (PcdGpuToLocalCpuAccessBandwidth);
  mMemoryTransferNormalizedDistance[GPU_TO_LOCAL_MEMORY] = PcdGet32 (PcdHbmToLocalCpuDistance);

  mMemoryTransferReadLatency[GPU_TO_REMOTE_MEMORY]        = PcdGet32 (PcdGpuToRemoteCpuReadLatency);
  mMemoryTransferWriteLatency[GPU_TO_REMOTE_MEMORY]       = PcdGet32 (PcdGpuToRemoteCpuWriteLatency);
  mMemoryTransferBandwidth[GPU_TO_REMOTE_MEMORY]          = PcdGet32 (PcdGpuToRemoteCpuAccessBandwidth);
  mMemoryTransferNormalizedDistance[GPU_TO_REMOTE_MEMORY] = PcdGet32 (PcdHbmToRemoteCpuDistance);

  mMemoryTransferReadLatency[GPU_TO_LOCAL_HBM]        = PcdGet32 (PcdGpuToLocalHbmReadLatency);
  mMemoryTransferWriteLatency[GPU_TO_LOCAL_HBM]       = PcdGet32 (PcdGpuToLocalHbmWriteLatency);
  mMemoryTransferBandwidth[GPU_TO_LOCAL_HBM]          = PcdGet32 (PcdGpuToLocalHbmAccessBandwidth);
  mMemoryTransferNormalizedDistance[GPU_TO_LOCAL_HBM] = NORMALIZED_DISTANCE;

  mMemoryTransferReadLatency[GPU_TO_REMOTE_HBM]        = PcdGet32 (PcdGpuToRemoteHbmReadLatency);
  mMemoryTransferWriteLatency[GPU_TO_REMOTE_HBM]       = PcdGet32 (PcdGpuToRemoteHbmWriteLatency);
  mMemoryTransferBandwidth[GPU_TO_REMOTE_HBM]          = PcdGet32 (PcdGpuToRemoteHbmAccessBandwidth);
  mMemoryTransferNormalizedDistance[GPU_TO_REMOTE_HBM] = PcdGet32 (PcdGpuToRemoteHbmDistance);

  // TODO: Need to update the latency and bandwidth for GPU to remote HBM memory on socket
  mMemoryTransferReadLatency[GPU_TO_REMOTE_HBM_SAME_SOCKET]        = PcdGet32 (PcdGpuToRemoteHbmReadLatency);
  mMemoryTransferWriteLatency[GPU_TO_REMOTE_HBM_SAME_SOCKET]       = PcdGet32 (PcdGpuToRemoteHbmWriteLatency);
  mMemoryTransferBandwidth[GPU_TO_REMOTE_HBM_SAME_SOCKET]          = PcdGet32 (PcdGpuToRemoteHbmAccessBandwidth);
  mMemoryTransferNormalizedDistance[GPU_TO_REMOTE_HBM_SAME_SOCKET] = PcdGet32 (PcdGpuToRemoteHbmDistance);

  for (Index = 0; Index < MAX_MEMORY_TRANSFER_TYPES; Index++) {
    DEBUG ((
      DEBUG_INFO,
      "MemoryTransferType: %u, ReadLatency: %u, WriteLatency: %u, Bandwidth: %u, NormalizedDistance: %u\n",
      Index,
      mMemoryTransferReadLatency[Index],
      mMemoryTransferWriteLatency[Index],
      mMemoryTransferBandwidth[Index],
      mMemoryTransferNormalizedDistance[Index]
      ));
  }

  return EFI_SUCCESS;
}
