/** @file
  HBM Memory Proximity domain config

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>

#include <TH500/TH500Definitions.h>

UINT64  EnabledHbmBitMap;

UINTN
EFIAPI
GetMaxHbmPxmDomains (
  VOID
  )
{
  UINTN  MaxDmns;
  UINT8  BitMapIdx;

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

Exit:
  FreePool (Handles);

  return Status;
}
