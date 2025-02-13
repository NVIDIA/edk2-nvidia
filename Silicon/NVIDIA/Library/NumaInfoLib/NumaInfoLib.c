/** @file

  Numa Information Library

  Note: NVIDIA GPUs have special hardware/connections and so they and their memories are treated as special cases
  vs general PCIE devices.

  SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/NumaInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/CxlEntityProtocol.h>
#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>

#include <IndustryStandard/Acpi.h>

#include <TH500/TH500Definitions.h>
#include <T410/T410Definitions.h>

// JDS TODO - remove this once development is done
#ifdef DEBUG_INFO
  #undef DEBUG_INFO
#define DEBUG_INFO  DEBUG_ERROR
#endif
#ifdef DEBUG_VERBOSE
  #undef DEBUG_VERBOSE
#define DEBUG_VERBOSE  DEBUG_ERROR
#endif

#define UNSUPPORTED_CXL_NUMA_DOMAIN_START    MAX_UINT32
#define UNSUPPORTED_HV_EGM_PXM_DOMAIN_START  MAX_UINT32

#include "NumaTransferInfo.h"

// The actual table values will be copied into this one
NUMA_TRANSFER_INFO  mNumaTransferInfoTable[INITIATOR_MAX][TARGET_MAX][LOCALITY_MAX];

typedef struct {
  UINT32    HvEgmDomainStart;
  UINT32    CxlDomainStart;
} NUMA_INFO_DOMAIN_START;

NUMA_INFO_DOMAIN_START  mT24xDomainStartInfo = {
  .HvEgmDomainStart = TH500_HV_EGM_PXM_DOMAIN_START,
  .CxlDomainStart   = UNSUPPORTED_CXL_NUMA_DOMAIN_START
};

NUMA_INFO_DOMAIN_START  mT41xDomainStartInfo = {
  .HvEgmDomainStart = UNSUPPORTED_HV_EGM_PXM_DOMAIN_START,
  .CxlDomainStart   = T410_CXL_NUMA_DOMAIN_START
};

// UINT32  mMemoryTransferReadLatency[MAX_MEMORY_TRANSFER_TYPES];
// UINT32  mMemoryTransferWriteLatency[MAX_MEMORY_TRANSFER_TYPES];
// UINT32  mMemoryTransferBandwidth[MAX_MEMORY_TRANSFER_TYPES];
// UINT8   mMemoryTransferNormalizedDistance[MAX_MEMORY_TRANSFER_TYPES];

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
  @param[out]   InitiatorType     Initiator type of the transfer
  @param[out]   TargetType        Target type of the transfer
  @param[out]   LocalityType      Locality type of the transfer

  @retval       EFI_SUCCESS     Locality type returned successfully
  @retval       EFI_UNSUPPORTED Locality type not supported
 */
STATIC
EFI_STATUS
EFIAPI
GetTransferType (
  IN  NUMA_INFO_DOMAIN_INFO  InitiatorInfo,
  IN  NUMA_INFO_DOMAIN_INFO  TargetInfo,
  OUT NUMA_INITIATOR_TYPE    *InitiatorType OPTIONAL,
  OUT NUMA_TARGET_TYPE       *TargetType OPTIONAL,
  OUT NUMA_LOCALITY_TYPE     *LocalityType OPTIONAL
  )
{
  NUMA_INITIATOR_TYPE  Initiator;
  NUMA_TARGET_TYPE     Target;
  NUMA_LOCALITY_TYPE   Locality;

  switch (InitiatorInfo.DeviceType) {
    case NUMA_INFO_TYPE_CPU:
      Initiator = INITIATOR_CPU;
      break;
    case NUMA_INFO_TYPE_GPU:
      Initiator = INITIATOR_GPU;
      break;
    case NUMA_INFO_TYPE_PCIE:
      Initiator = INITIATOR_PCIE;
      break;
    default:
      return EFI_UNSUPPORTED;
  }

  switch (TargetInfo.DeviceType) {
    case NUMA_INFO_TYPE_HV:
    case NUMA_INFO_TYPE_CPU:
      Target = TARGET_CPU;
      break;
    case NUMA_INFO_TYPE_GPU:
      Target = TARGET_GPU;
      break;
    case NUMA_INFO_TYPE_PCIE:
      Target = TARGET_PCIE;
      break;
    default:
      return EFI_UNSUPPORTED;
  }

  if (InitiatorInfo.SocketId == TargetInfo.SocketId) {
    // Local only if same sockt, same type, and same segment/bdf (if relevant)
    if (((Initiator == INITIATOR_CPU) && (Target == TARGET_CPU)) ||
        ((((Initiator == INITIATOR_GPU) && (Target == TARGET_GPU)) ||
          ((Initiator == INITIATOR_PCIE) && (Target == TARGET_PCIE))) &&
         (TargetInfo.DeviceHandle.Pci.PciSegment == InitiatorInfo.DeviceHandle.Pci.PciSegment) &&
         (TargetInfo.DeviceHandle.Pci.PciBdfNumber == InitiatorInfo.DeviceHandle.Pci.PciBdfNumber)))
    {
      Locality = LOCALITY_LOCAL;
    } else {
      Locality = LOCALITY_SAME_SOCKET;
    }
  } else {
    Locality = LOCALITY_REMOTE_SOCKET;
  }

  if (InitiatorType != NULL) {
    *InitiatorType = Initiator;
  }

  if (TargetType != NULL) {
    *TargetType = Target;
  }

  if (LocalityType != NULL) {
    *LocalityType = Locality;
  }

  return EFI_SUCCESS;
}

/**
  Returns the distance between two domains

  @param[in]    InitiatorDomain     Initiator domain number
  @param[in]    TargetDomain        Target domain number
  @param[out]   NormalizedDistance  Pointer to Normalized distance
  @param[out]   ReadLatency         Pointer to Read latency
  @param[out]   WriteLatency        Pointer to Write latency
  @param[out]   AccessBandwidth     Pointer to Access bandwidth

  @retval       EFI_NOT_FOUND   Domain not found
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
  EFI_STATUS             Status;
  NUMA_INFO_DOMAIN_INFO  InitiatorInfo;
  NUMA_INFO_DOMAIN_INFO  TargetInfo;
  UINT8                  LocalNormalizedDistance;
  UINT16                 LocalReadLatency;
  UINT16                 LocalWriteLatency;
  UINT16                 LocalAccessBandwidth;
  BOOLEAN                Unreachable;
  NUMA_INITIATOR_TYPE    InitiatorType;
  NUMA_TARGET_TYPE       TargetType;
  NUMA_LOCALITY_TYPE     LocalityType;
  NUMA_TRANSFER_INFO     *TransferInfoTemplate;

  Unreachable = FALSE;
  do {
    if (mNumaInfo == NULL) {
      Unreachable = TRUE;
      Status      = EFI_NOT_READY;
      break;
    }

    if (EFI_ERROR (NumaInfoGetDomainDetails (InitiatorDomain, &InitiatorInfo))) {
      Unreachable = TRUE;
      Status      = EFI_NOT_FOUND;
      break;
    }

    if (EFI_ERROR (NumaInfoGetDomainDetails (TargetDomain, &TargetInfo))) {
      Unreachable = TRUE;
      Status      = EFI_NOT_FOUND;
      break;
    }

    if ((InitiatorInfo.InitiatorDomain == FALSE) || (TargetInfo.TargetDomain == FALSE)) {
      Unreachable = TRUE;
      Status      = EFI_INVALID_PARAMETER;
      break;
    }

    Status = GetTransferType (InitiatorInfo, TargetInfo, &InitiatorType, &TargetType, &LocalityType);
    if (EFI_ERROR (Status)) {
      Unreachable = TRUE;
      break;
    }
  } while (FALSE);

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
    TransferInfoTemplate = &mNumaTransferInfoTable[InitiatorType][TargetType][LocalityType];

    if ((InitiatorType == INITIATOR_PCIE) || (TargetType == TARGET_PCIE)) {
      // JDS TODO - add in CDAT info for the initiator/target portions beyond PCIE root ports
      LocalNormalizedDistance = TransferInfoTemplate->NormalizedDistance;
      LocalReadLatency        = TransferInfoTemplate->ReadLatency;
      LocalWriteLatency       = TransferInfoTemplate->WriteLatency;
      LocalAccessBandwidth    = TransferInfoTemplate->AccessBandwidth;
    } else {
      LocalNormalizedDistance = TransferInfoTemplate->NormalizedDistance;
      LocalReadLatency        = TransferInfoTemplate->ReadLatency;
      LocalWriteLatency       = TransferInfoTemplate->WriteLatency;
      LocalAccessBandwidth    = TransferInfoTemplate->AccessBandwidth;
    }

    // Increment the distance if the domains are different but the access is local
    if ((InitiatorDomain != TargetDomain) && (LocalNormalizedDistance == NORMALIZED_DISTANCE)) {
      LocalNormalizedDistance += 1;
    }

    Status = EFI_SUCCESS;
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

  return Status;
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
  UINTN                                             NumberOfCxlGenericInitiators;
  UINTN                                             NumberOfCxlMemories;
  UINTN                                             NumberOfRootBridges;
  UINTN                                             GpuIndex;
  UINTN                                             RootBridgeIndex;
  UINT32                                            SocketIndex;
  UINTN                                             NumaIndex;
  UINTN                                             DomainIndex;
  UINTN                                             CxlDomainIndex;
  EFI_HANDLE                                        *RootBridgeHandles = NULL;
  EFI_HANDLE                                        *GpuHandleBuffer   = NULL;
  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *RootBridgeCfgIo;
  EFI_PCI_IO_PROTOCOL                               *PciIo;
  UINTN                                             SocketId;
  UINTN                                             SegmentNumber;
  UINTN                                             BusNumber;
  UINTN                                             DeviceNumber;
  UINTN                                             FunctionNumber;
  VOID                                              *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO                      *PlatformResourceInfo;
  EFI_HANDLE                                        *CxlEntityHandles;
  UINTN                                             CxlEntityHandleCount;
  UINTN                                             CxlEntityIndex;
  CXL_ENTITY_PROTOCOL                               *CxlEntityProtocol;
  NUMA_INFO_DOMAIN_START                            *DomainStart;
  UINTN                                             ChipId = TegraGetChipID ();

  // JDS TODO - this should probably be moved into PlatformResourceLib
  switch (ChipId) {
    case TH500_CHIP_ID:
      DomainStart = &mT24xDomainStartInfo;
      break;

    case T410_CHIP_ID:
      DomainStart = &mT41xDomainStartInfo;
      break;

    default:
      DEBUG ((DEBUG_ERROR, "Unsupported Tegra Chip ID:  0x%x\n", ChipId));
      return EFI_UNSUPPORTED;
  }

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
  MPCORE_FOR_EACH_ENABLED_SOCKET (SocketIndex) {
    NumberOfSockets++;
  }

  // Get all the GPUs
  NumberOfGpuDomains = 0;
  Status             = gBS->LocateHandleBuffer (
                              ByProtocol,
                              &gEfiNVIDIAGpuDSDAMLGenerationProtocolGuid,
                              NULL,
                              &NumberOfGpus,
                              &GpuHandleBuffer
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

  for (GpuIndex = 0; GpuIndex < NumberOfGpus; GpuIndex++) {
    Status = gBS->HandleProtocol (
                    GpuHandleBuffer[GpuIndex],
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

    for (RootBridgeIndex = 0; RootBridgeIndex < NumberOfRootBridges; RootBridgeIndex++) {
      Status = gBS->HandleProtocol (
                      RootBridgeHandles[RootBridgeIndex],
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

  // Get all of the CXL entities
  NumberOfCxlGenericInitiators = 0;
  NumberOfCxlMemories          = 0;
  CxlEntityHandleCount         = 0;
  if (DomainStart->CxlDomainStart != UNSUPPORTED_CXL_NUMA_DOMAIN_START) {
    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gNVIDIACxlEntityProtocolGuid,
                    NULL,
                    &CxlEntityHandleCount,
                    &CxlEntityHandles
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get Cxl Entity Protocol handles - %r\n", __FUNCTION__, Status));
      CxlEntityHandleCount = 0;
    }

    for (CxlEntityIndex = 0; CxlEntityIndex < CxlEntityHandleCount; CxlEntityIndex++) {
      // Make sure this entity isn't already one of the GPUs
      for (GpuIndex = 0; GpuIndex < NumberOfGpus; GpuIndex++) {
        if (GpuHandleBuffer[GpuIndex] == CxlEntityHandles[CxlEntityIndex]) {
          DEBUG ((DEBUG_VERBOSE, "%a: Cxl Entity already in GPU list - skipping it\n", __FUNCTION__));
          CxlEntityHandles[CxlEntityIndex] = NULL;
          break;
        }
      }

      if (CxlEntityHandles[CxlEntityIndex] == NULL) {
        continue;
      }

      Status = gBS->HandleProtocol (
                      CxlEntityHandles[CxlEntityIndex],
                      &gNVIDIACxlEntityProtocolGuid,
                      (VOID **)&CxlEntityProtocol
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to get Cxl Entity Protocol - %r\n", __FUNCTION__, Status));
        continue;
      }

      switch (CxlEntityProtocol->Type) {
        case CXL_TYPE2_DEVICE:
          NumberOfCxlGenericInitiators++;
          break;

        case CXL_TYPE3_DEVICE:
          NumberOfCxlMemories++;
          break;

        default:
          DEBUG ((DEBUG_VERBOSE, "%a: Unknown Cxl Entity type - %d\n", __FUNCTION__, CxlEntityProtocol->Type));
          CxlEntityHandles[CxlEntityIndex] = NULL;
          break;
      }
    }
  }

  DEBUG ((DEBUG_INFO, "%a: CxlEntityCount = %u, NumberOfCxlGenericInitiators = %u, NumberOfCxlMemories = %u\n", __FUNCTION__, CxlEntityHandleCount, NumberOfCxlGenericInitiators, NumberOfCxlMemories));

  mNumberOfDomains = NumberOfSockets + NumberOfGpuDomains + NumberOfCxlGenericInitiators + NumberOfCxlMemories;
  if (PlatformResourceInfo->HypervisorMode) {
    mNumberOfDomains += NumberOfSockets;
  }

  mNumaInfo = AllocateZeroPool (sizeof (NUMA_INFO_DOMAIN_INFO) * mNumberOfDomains);
  if (mNumaInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // CPUs
  NumaIndex = 0;
  MPCORE_FOR_EACH_ENABLED_SOCKET (SocketIndex) {
    mNumaInfo[NumaIndex].ProximityDomain               = SocketIndex;
    mNumaInfo[NumaIndex].SocketId                      = SocketIndex;
    mNumaInfo[NumaIndex].DeviceType                    = NUMA_INFO_TYPE_CPU;
    mNumaInfo[NumaIndex].DeviceHandleType              = 0;
    mNumaInfo[NumaIndex].DeviceHandle.Pci.PciBdfNumber = 0;
    mNumaInfo[NumaIndex].DeviceHandle.Pci.PciSegment   = 0;
    mNumaInfo[NumaIndex].InitiatorDomain               = TRUE;
    mNumaInfo[NumaIndex].TargetDomain                  = TRUE;
    NumaIndex++;
  }

  ASSERT (NumaIndex == NumberOfSockets);

  // HV EGMs
  if (PlatformResourceInfo->HypervisorMode && (DomainStart->HvEgmDomainStart != UNSUPPORTED_HV_EGM_PXM_DOMAIN_START)) {
    for (SocketIndex = 0; SocketIndex < PcdGet32 (PcdTegraMaxSockets); SocketIndex++) {
      if (IsSocketEnabled (SocketIndex)) {
        mNumaInfo[NumaIndex].ProximityDomain               = DomainStart->HvEgmDomainStart + SocketIndex;
        mNumaInfo[NumaIndex].SocketId                      = SocketIndex;
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

  // GPUs
  for (GpuIndex = 0; GpuIndex < NumberOfGpus; GpuIndex++) {
    Status = gBS->HandleProtocol (
                    GpuHandleBuffer[GpuIndex],
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

    for (RootBridgeIndex = 0; RootBridgeIndex < NumberOfRootBridges; RootBridgeIndex++) {
      Status = gBS->HandleProtocol (
                      RootBridgeHandles[RootBridgeIndex],
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
        mNumaInfo[NumaIndex].DeviceHandleType              = EFI_ACPI_6_6_PCI_DEVICE_HANDLE;
        mNumaInfo[NumaIndex].DeviceHandle.Pci.PciBdfNumber = SwapBytes16 (BusNumber << 8 | DeviceNumber << 3 | FunctionNumber);
        mNumaInfo[NumaIndex].DeviceHandle.Pci.PciSegment   = SegmentNumber;
        mNumaInfo[NumaIndex].InitiatorDomain               = TRUE;
        mNumaInfo[NumaIndex].TargetDomain                  = TRUE;
        NumaIndex++;
      }

      break;
    }
  }

  CxlDomainIndex = 0;

  // CXL entities
  if (NumberOfCxlGenericInitiators + NumberOfCxlMemories > 0) {
    for (CxlEntityIndex = 0; CxlEntityIndex < CxlEntityHandleCount; CxlEntityIndex++) {
      if (CxlEntityHandles[CxlEntityIndex] == NULL) {
        continue;
      }

      Status = gBS->HandleProtocol (
                      CxlEntityHandles[CxlEntityIndex],
                      &gNVIDIACxlEntityProtocolGuid,
                      (VOID **)&CxlEntityProtocol
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to get Cxl Entity Protocol - %r\n", __FUNCTION__, Status));
        continue;
      }

      Status = CxlEntityProtocol->PciIo->GetLocation (CxlEntityProtocol->PciIo, &SegmentNumber, &BusNumber, &DeviceNumber, &FunctionNumber);
      if (EFI_ERROR (Status)) {
        continue;
      }

      SocketId = -1;
      for (RootBridgeIndex = 0; RootBridgeIndex < NumberOfRootBridges; RootBridgeIndex++) {
        Status = gBS->HandleProtocol (
                        RootBridgeHandles[RootBridgeIndex],
                        &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                        (VOID **)&RootBridgeCfgIo
                        );
        if (EFI_ERROR (Status)) {
          continue;
        }

        if (SegmentNumber != RootBridgeCfgIo->SegmentNumber) {
          continue;
        }

        SocketId = RootBridgeCfgIo->SocketID;
        break;
      }

      if (SocketId == -1) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to determine SocketId for Cxl Entity - skipping\n", __FUNCTION__));
        continue;
      }

      mNumaInfo[NumaIndex].DeviceType = NUMA_INFO_TYPE_PCIE;
      // JDS TODO - should we read the cxl.cache (initiator) and cxl.memory (target) fields instead?
      switch (CxlEntityProtocol->Type) {
        case CXL_TYPE2_DEVICE:
          mNumaInfo[NumaIndex].InitiatorDomain = TRUE;
          mNumaInfo[NumaIndex].TargetDomain    = TRUE;
          break;

        case CXL_TYPE3_DEVICE:
          mNumaInfo[NumaIndex].InitiatorDomain = FALSE;
          mNumaInfo[NumaIndex].TargetDomain    = TRUE;
          break;

        default:
          continue;
          break;
      }

      if (CxlEntityProtocol->ProximityDomainValid) {
        NV_ASSERT_RETURN (CxlEntityProtocol->ProximityDomain >= DomainStart->CxlDomainStart, continue, "CxlEntityProtocol has an invalid ProximityDomain 0x%x, must be at least 0x%x\n", CxlEntityProtocol->ProximityDomain, DomainStart->CxlDomainStart);
        mNumaInfo[NumaIndex].ProximityDomain = CxlEntityProtocol->ProximityDomain;
      } else {
        mNumaInfo[NumaIndex].ProximityDomain    = DomainStart->CxlDomainStart + CxlDomainIndex;
        CxlEntityProtocol->ProximityDomain      = mNumaInfo[NumaIndex].ProximityDomain;
        CxlEntityProtocol->ProximityDomainValid = TRUE;
        CxlDomainIndex++;
      }

      mNumaInfo[NumaIndex].SocketId                      = SocketId;
      mNumaInfo[NumaIndex].DeviceHandleType              = EFI_ACPI_6_6_PCI_DEVICE_HANDLE;
      mNumaInfo[NumaIndex].DeviceHandle.Pci.PciBdfNumber = SwapBytes16 (BusNumber << 8 | DeviceNumber << 3 | FunctionNumber);
      mNumaInfo[NumaIndex].DeviceHandle.Pci.PciSegment   = SegmentNumber;
      NumaIndex++;
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

  // Read the memory transfer latency and bandwidth from DTB
  if (FALSE) {
    // TODO: Check DTB for a complete NumaTransferInfoTable and copy it to mNumaTransferInfoTable
  } else {
    // Fall back to Template + PCDs
    NUMA_TRANSFER_INFO (*TransferInfoTemplate)[TARGET_MAX][LOCALITY_MAX] = NULL;
    TEGRA_PLATFORM_TYPE  PlatformType = TegraGetPlatform ();

    // Select the template based on the platform type
    if (PlatformType == TEGRA_PLATFORM_VDK) {
      TransferInfoTemplate = mSimTransferInfoTemplate;
    }

    if (TransferInfoTemplate == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to determine an appropriate NUMA template to use. Using mSimTransferInfoTemplate\n", __FUNCTION__));
      TransferInfoTemplate = mSimTransferInfoTemplate;
    }

    CopyMem (mNumaTransferInfoTable, TransferInfoTemplate, sizeof (mNumaTransferInfoTable));

    #define UPDATE_TEMPLATE(INIT, TAR, LOC, FIELD, PCD) \
  do { \
    UINT32 Value = PcdGet32 (PCD); \
    if (Value != 0) { \
      mNumaTransferInfoTable[INITIATOR_##INIT][TARGET_##TAR][LOCALITY_##LOC].FIELD = Value; \
    } \
  } while (FALSE)

    // CPU initiator
    UPDATE_TEMPLATE (CPU, CPU, LOCAL, ReadLatency, PcdCpuToLocalCpuReadLatency);
    UPDATE_TEMPLATE (CPU, CPU, LOCAL, WriteLatency, PcdCpuToLocalCpuWriteLatency);
    UPDATE_TEMPLATE (CPU, CPU, LOCAL, AccessBandwidth, PcdCpuToLocalCpuAccessBandwidth);
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_CPU][LOCALITY_LOCAL].NormalizedDistance = NORMALIZED_DISTANCE;

    // All CPU memory is local rather than same socket to its CPU
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_CPU][LOCALITY_SAME_SOCKET].ReadLatency        = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_CPU][LOCALITY_SAME_SOCKET].WriteLatency       = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_CPU][LOCALITY_SAME_SOCKET].AccessBandwidth    = UNREACHABLE_BANDWIDTH;
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_CPU][LOCALITY_SAME_SOCKET].NormalizedDistance = UNREACHABLE_DISTANCE;

    UPDATE_TEMPLATE (CPU, CPU, REMOTE_SOCKET, ReadLatency, PcdCpuToRemoteCpuReadLatency);
    UPDATE_TEMPLATE (CPU, CPU, REMOTE_SOCKET, WriteLatency, PcdCpuToRemoteCpuWriteLatency);
    UPDATE_TEMPLATE (CPU, CPU, REMOTE_SOCKET, AccessBandwidth, PcdCpuToRemoteCpuAccessBandwidth);
    UPDATE_TEMPLATE (CPU, CPU, REMOTE_SOCKET, NormalizedDistance, PcdCpuToRemoteCpuDistance);

    // All GPU memory is not local to the CPU
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_GPU][LOCALITY_LOCAL].ReadLatency        = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_GPU][LOCALITY_LOCAL].WriteLatency       = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_GPU][LOCALITY_LOCAL].AccessBandwidth    = UNREACHABLE_BANDWIDTH;
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_GPU][LOCALITY_LOCAL].NormalizedDistance = UNREACHABLE_DISTANCE;

    UPDATE_TEMPLATE (CPU, GPU, SAME_SOCKET, ReadLatency, PcdCpuToLocalHbmReadLatency);
    UPDATE_TEMPLATE (CPU, GPU, SAME_SOCKET, WriteLatency, PcdCpuToLocalHbmWriteLatency);
    UPDATE_TEMPLATE (CPU, GPU, SAME_SOCKET, AccessBandwidth, PcdCpuToLocalHbmAccessBandwidth);
    UPDATE_TEMPLATE (CPU, GPU, SAME_SOCKET, NormalizedDistance, PcdCpuToLocalHbmDistance);

    UPDATE_TEMPLATE (CPU, GPU, REMOTE_SOCKET, ReadLatency, PcdCpuToRemoteHbmReadLatency);
    UPDATE_TEMPLATE (CPU, GPU, REMOTE_SOCKET, WriteLatency, PcdCpuToRemoteHbmWriteLatency);
    UPDATE_TEMPLATE (CPU, GPU, REMOTE_SOCKET, AccessBandwidth, PcdCpuToRemoteHbmAccessBandwidth);
    UPDATE_TEMPLATE (CPU, GPU, REMOTE_SOCKET, NormalizedDistance, PcdCpuToRemoteHbmDistance);

    // All PCIE memory is not local to the CPU
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_PCIE][LOCALITY_LOCAL].ReadLatency        = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_PCIE][LOCALITY_LOCAL].WriteLatency       = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_PCIE][LOCALITY_LOCAL].AccessBandwidth    = UNREACHABLE_BANDWIDTH;
    mNumaTransferInfoTable[INITIATOR_CPU][TARGET_PCIE][LOCALITY_LOCAL].NormalizedDistance = UNREACHABLE_DISTANCE;

    // GPU initiator
    // All CPU memory is not local to the GPU
    mNumaTransferInfoTable[INITIATOR_GPU][TARGET_CPU][LOCALITY_LOCAL].ReadLatency        = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_GPU][TARGET_CPU][LOCALITY_LOCAL].WriteLatency       = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_GPU][TARGET_CPU][LOCALITY_LOCAL].AccessBandwidth    = UNREACHABLE_BANDWIDTH;
    mNumaTransferInfoTable[INITIATOR_GPU][TARGET_CPU][LOCALITY_LOCAL].NormalizedDistance = UNREACHABLE_DISTANCE;

    UPDATE_TEMPLATE (GPU, CPU, SAME_SOCKET, ReadLatency, PcdGpuToLocalCpuReadLatency);
    UPDATE_TEMPLATE (GPU, CPU, SAME_SOCKET, WriteLatency, PcdGpuToLocalCpuWriteLatency);
    UPDATE_TEMPLATE (GPU, CPU, SAME_SOCKET, AccessBandwidth, PcdGpuToLocalCpuAccessBandwidth);
    UPDATE_TEMPLATE (GPU, CPU, SAME_SOCKET, NormalizedDistance, PcdHbmToLocalCpuDistance);

    UPDATE_TEMPLATE (GPU, CPU, REMOTE_SOCKET, ReadLatency, PcdGpuToRemoteCpuReadLatency);
    UPDATE_TEMPLATE (GPU, CPU, REMOTE_SOCKET, WriteLatency, PcdGpuToRemoteCpuWriteLatency);
    UPDATE_TEMPLATE (GPU, CPU, REMOTE_SOCKET, AccessBandwidth, PcdGpuToRemoteCpuAccessBandwidth);
    UPDATE_TEMPLATE (GPU, CPU, REMOTE_SOCKET, NormalizedDistance, PcdHbmToRemoteCpuDistance);

    UPDATE_TEMPLATE (GPU, GPU, LOCAL, ReadLatency, PcdGpuToLocalHbmReadLatency);
    UPDATE_TEMPLATE (GPU, GPU, LOCAL, WriteLatency, PcdGpuToLocalHbmWriteLatency);
    UPDATE_TEMPLATE (GPU, GPU, LOCAL, AccessBandwidth, PcdGpuToLocalHbmAccessBandwidth);
    mNumaTransferInfoTable[INITIATOR_GPU][TARGET_GPU][LOCALITY_LOCAL].NormalizedDistance = NORMALIZED_DISTANCE;

    // TODO: Need to update the latency and bandwidth for GPU to remote GPU_MEMORY memory on socket
    UPDATE_TEMPLATE (GPU, GPU, SAME_SOCKET, ReadLatency, PcdGpuToRemoteHbmReadLatency);
    UPDATE_TEMPLATE (GPU, GPU, SAME_SOCKET, WriteLatency, PcdGpuToRemoteHbmWriteLatency);
    UPDATE_TEMPLATE (GPU, GPU, SAME_SOCKET, AccessBandwidth, PcdGpuToRemoteHbmAccessBandwidth);
    UPDATE_TEMPLATE (GPU, GPU, SAME_SOCKET, NormalizedDistance, PcdGpuToRemoteHbmDistance);

    UPDATE_TEMPLATE (GPU, GPU, REMOTE_SOCKET, ReadLatency, PcdGpuToRemoteHbmReadLatency);
    UPDATE_TEMPLATE (GPU, GPU, REMOTE_SOCKET, WriteLatency, PcdGpuToRemoteHbmWriteLatency);
    UPDATE_TEMPLATE (GPU, GPU, REMOTE_SOCKET, AccessBandwidth, PcdGpuToRemoteHbmAccessBandwidth);
    UPDATE_TEMPLATE (GPU, GPU, REMOTE_SOCKET, NormalizedDistance, PcdGpuToRemoteHbmDistance);

    // All PCIE memory is not local to the GPU
    mNumaTransferInfoTable[INITIATOR_GPU][TARGET_PCIE][LOCALITY_LOCAL].ReadLatency        = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_GPU][TARGET_PCIE][LOCALITY_LOCAL].WriteLatency       = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_GPU][TARGET_PCIE][LOCALITY_LOCAL].AccessBandwidth    = UNREACHABLE_BANDWIDTH;
    mNumaTransferInfoTable[INITIATOR_GPU][TARGET_PCIE][LOCALITY_LOCAL].NormalizedDistance = UNREACHABLE_DISTANCE;

    // PCIE initiator
    // All CPU memory is not local to the PCIE
    mNumaTransferInfoTable[INITIATOR_PCIE][TARGET_CPU][LOCALITY_LOCAL].ReadLatency        = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_PCIE][TARGET_CPU][LOCALITY_LOCAL].WriteLatency       = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_PCIE][TARGET_CPU][LOCALITY_LOCAL].AccessBandwidth    = UNREACHABLE_BANDWIDTH;
    mNumaTransferInfoTable[INITIATOR_PCIE][TARGET_CPU][LOCALITY_LOCAL].NormalizedDistance = UNREACHABLE_DISTANCE;

    // All GPU memory is not local to the PCIE
    mNumaTransferInfoTable[INITIATOR_PCIE][TARGET_GPU][LOCALITY_LOCAL].ReadLatency        = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_PCIE][TARGET_GPU][LOCALITY_LOCAL].WriteLatency       = UNREACHABLE_LATENCY;
    mNumaTransferInfoTable[INITIATOR_PCIE][TARGET_GPU][LOCALITY_LOCAL].AccessBandwidth    = UNREACHABLE_BANDWIDTH;
    mNumaTransferInfoTable[INITIATOR_PCIE][TARGET_GPU][LOCALITY_LOCAL].NormalizedDistance = UNREACHABLE_DISTANCE;

    mNumaTransferInfoTable[INITIATOR_PCIE][TARGET_PCIE][LOCALITY_LOCAL].NormalizedDistance = NORMALIZED_DISTANCE;

    // Check if we're simply using the template table, or if we modified it with real values
    if (CompareMem (mNumaTransferInfoTable, TransferInfoTemplate, sizeof (mNumaTransferInfoTable)) == 0) {
      DEBUG ((
        DEBUG_ERROR,
        "*****************************************************************\n"
        "%a: WARNING: Using unmodified template for Numa Information\n"
        "         This may cause unexpected results\n"
        "*****************************************************************\n",
        __FUNCTION__
        ));
    }
  }

  for (UINT32 InitiatorIndex = 0; InitiatorIndex < INITIATOR_MAX; InitiatorIndex++) {
    for (UINT32 TargetIndex = 0; TargetIndex < TARGET_MAX; TargetIndex++) {
      for (UINT32 LocalityIndex = 0; LocalityIndex < LOCALITY_MAX; LocalityIndex++) {
        DEBUG ((
          DEBUG_INFO,
          "%a: mNumaTransferInfoTable[Init: %u][Tar: %u][Loc: %u] ReadLatency: %u, WriteLatency: %u, AccessBandwidth: %u, NormalizedDistance: %u\n",
          __FUNCTION__,
          InitiatorIndex,
          TargetIndex,
          LocalityIndex,
          mNumaTransferInfoTable[InitiatorIndex][TargetIndex][LocalityIndex].ReadLatency,
          mNumaTransferInfoTable[InitiatorIndex][TargetIndex][LocalityIndex].WriteLatency,
          mNumaTransferInfoTable[InitiatorIndex][TargetIndex][LocalityIndex].AccessBandwidth,
          mNumaTransferInfoTable[InitiatorIndex][TargetIndex][LocalityIndex].NormalizedDistance
          ));
      }
    }
  }

  if (GpuHandleBuffer != NULL) {
    FreePool (GpuHandleBuffer);
  }

  if (RootBridgeHandles != NULL) {
    FreePool (RootBridgeHandles);
  }

  return EFI_SUCCESS;
}
