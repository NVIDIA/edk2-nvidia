/** @file

  This driver implements supporting functions for SMMU client driver

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/FloorSweepingLib.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <Protocol/PciIo.h>
#include <Library/SmmuLib.h>

/**
  Convert the PciHandle to SourceId.
**/
EFI_STATUS
EFIAPI
GetSourceIdFromPciHandle (
  IN EFI_HANDLE  PciHandle,
  OUT SOURCE_ID  *SourceId
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  UINTN                Segment;
  UINTN                Bus;
  UINTN                Device;
  UINTN                Function;
  UINT32               SmmuV3pHandle;
  UINT16               Rid;
  UINT32               StreamId;
  UINT32               StreamIdBase;
  EFI_HANDLE           *Handles = NULL;
  UINTN                NumberOfHandles;
  UINTN                HandleIdx;

  SmmuV3pHandle = 0;
  StreamId      = 0;

  Status = gBS->HandleProtocol (
                  PciHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Cannot locate Pci IO protocol ", __FUNCTION__));
    goto CleanupAndReturn;
  }

  Status = PciIo->GetLocation (
                    PciIo,
                    &Segment,
                    &Bus,
                    &Device,
                    &Function
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Cannot find Segment and BDF ", __FUNCTION__));
    goto CleanupAndReturn;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Segment: %02x\t Bus: 0x%02x\t Device: 0x%02x\t Function: 0x%02x\n",
    __FUNCTION__,
    Segment,
    Bus,
    Device,
    Function
    ));

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate root bridge protocols, %r.\r\n", __FUNCTION__, NumberOfHandles));
    goto CleanupAndReturn;
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
      goto CleanupAndReturn;
    }

    // now compare the segment Id of each handle
    if (PciRbCfg->SegmentNumber == Segment) {
      // Extract the SmmuV3pHandle of this same matched PciRbprotocol
      SmmuV3pHandle = PciRbCfg->SmmuV3pHandle;
      DEBUG ((DEBUG_INFO, "%a:SmmuV3pHandle = 0x%X\r\n", __FUNCTION__, SmmuV3pHandle));

      // a RID is formatted such that:
      // Bits [15:8] are the Bus number.
      // Bits [7:3] are the Device number.
      // Bits [2:0] are the Function number.
      Rid = (UINT16)((Bus << 8) | (Device << 3) | Function);

      StreamIdBase = PciRbCfg->StreamIdBase;
      StreamId     =  StreamIdBase + Rid;
      break;
    }
  }

  if (HandleIdx == NumberOfHandles) {
    DEBUG ((DEBUG_ERROR, "%a: PCI Handle not found for Segment number %u\r\n", __FUNCTION__, Segment));
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  SourceId->StreamId      = StreamId;
  SourceId->SmmuV3pHandle = SmmuV3pHandle;

CleanupAndReturn:
  if (Handles != NULL) {
    gBS->FreePool (Handles);
  }

  return Status;
}
