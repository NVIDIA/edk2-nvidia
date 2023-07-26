/** @file
  HBM Memory Proximity domain config

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/FloorSweepingLib.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>

#include <TH500/TH500Definitions.h>

UINT64  EnabledHbmBitMap;

UINT32
EFIAPI
GetMaxHbmPxmDomains (
  VOID
  )
{
  UINT32  MaxDmns;
  UINT8   BitMapIdx;

  MaxDmns = 0;
  for (UINT8 Idx = 0; Idx <= TH500_GPU_MAX_PXM_DOMAINS; Idx++) {
    BitMapIdx = TH500_GPU_MAX_PXM_DOMAINS - Idx;
    if (EnabledHbmBitMap & (1ULL << BitMapIdx)) {
      // bit position to number of bits adjustment, add 1
      return (BitMapIdx + 1);
    }
  }

  return 0;
}

UINT32
EFIAPI
GetMaxPxmDomains (
  VOID
  )
{
  UINT32  MaxPxmDmns;
  UINT32  MaxHbmDmns;
  UINT32  MaxCpuSocketEnabled;
  UINT8   SocketIdx;

  MaxPxmDmns          = 0;
  MaxHbmDmns          = 0;
  MaxCpuSocketEnabled = 0;

  for (UINT32 Socket = 0; Socket <= PLATFORM_MAX_SOCKETS; Socket++) {
    SocketIdx = PLATFORM_MAX_SOCKETS - Socket;
    if (IsSocketEnabled (SocketIdx)) {
      // bit position to number of bits adjustment, add 1
      MaxCpuSocketEnabled = SocketIdx + 1;
      break;
    }
  }

  MaxHbmDmns = GetMaxHbmPxmDomains ();
  MaxPxmDmns = (MaxCpuSocketEnabled > MaxHbmDmns) ? MaxCpuSocketEnabled : (TH500_GPU_HBM_PXM_DOMAIN_START + MaxHbmDmns);

  return MaxPxmDmns;
}

BOOLEAN
EFIAPI
IsGpuEnabledOnSocket (
  UINT32  SocketId
  )
{
  if (EnabledHbmBitMap & (1ULL << (SocketId * TH500_GPU_MAX_NR_MEM_PARTITIONS))) {
    return TRUE;
  }

  return FALSE;
}

BOOLEAN
EFIAPI
IsHbmDmnEnabled (
  UINT32  DmnIdx
  )
{
  if (EnabledHbmBitMap & (1ULL << DmnIdx)) {
    return TRUE;
  }

  return FALSE;
}

EFI_STATUS
EFIAPI
GenerateHbmMemPxmDmnMap (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles = NULL;
  UINTN       NumberOfHandles;
  UINTN       HandleIdx;
  UINTN       PxmIdx;

  // Retrieve HBM memory info from PCI Root Bridge Protocol
  // Generate a bit map of enabled HBM PXM domains
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to locate root bridge protocols, %r.\r\n", __FUNCTION__, NumberOfHandles));
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
        EFI_D_ERROR,
        "%a: Failed to get protocol for handle %p, %r.\r\n",
        __FUNCTION__,
        Handles[HandleIdx],
        Status
        ));
      goto Exit;
    }

    if (PciRbCfg->NumProximityDomains > 0) {
      // Populate bitmap using info here
      for (PxmIdx = PciRbCfg->ProximityDomainStart;
           PxmIdx < (PciRbCfg->ProximityDomainStart + PciRbCfg->NumProximityDomains);
           PxmIdx++)
      {
        EnabledHbmBitMap |= (1ULL << PxmIdx);
      }
    }
  }

  // account for non HBM pxm domain in the info obtained from PCI RB config
  EnabledHbmBitMap = EnabledHbmBitMap >> TH500_GPU_HBM_PXM_DOMAIN_START;

Exit:
  if (Handles != NULL) {
    FreePool (Handles);
  }

  return EFI_SUCCESS;
}
