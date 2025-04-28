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
#include <Library/SmmuLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <Protocol/PciIo.h>
#include <Protocol/DevicePath.h>

STATIC
UINT16
GetBypassVendorIdFromDtb (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT16      BypassVendorId;
  INT32       NodeOffset;
  UINT32      BypassVendorIdValue;

  BypassVendorId = 0;

  // Get vendor IDs from DTB
  Status = DeviceTreeGetNodeByPath ("/firmware/uefi", &NodeOffset);
  if (Status == EFI_NOT_FOUND) {
    goto Exit;
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to check for %a\n", __FUNCTION__, Status, "/firmware/uefi"));
    goto Exit;
  } else if (NodeOffset >= 0) {
    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "skip-smmu-vid", &BypassVendorIdValue);
    if (Status == EFI_SUCCESS) {
      BypassVendorId = (UINT16)BypassVendorIdValue;
    }
  }

Exit:
  return BypassVendorId;
}

STATIC
BOOLEAN
CheckForBypassVendorId (
  IN EFI_HANDLE  Handle,
  IN UINT16      BypassVendorId
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *TempDevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *SubPath;
  EFI_DEVICE_PATH_PROTOCOL  *SubPathTrack;
  EFI_DEVICE_PATH_PROTOCOL  *EndNode;
  EFI_HANDLE                CurrentHandle;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  UINT16                    VendorId;
  BOOLEAN                   BypassNeeded;
  UINTN                     Size;

  BypassNeeded = FALSE;
  DevicePath   = NULL;
  SubPath      = NULL;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath
                  );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  TempDevicePath = DevicePath;

  while (!IsDevicePathEnd (TempDevicePath)) {
    Size = (UINTN)NextDevicePathNode (TempDevicePath) - (UINTN)DevicePath;

    SubPath = AllocateCopyPool (Size + END_DEVICE_PATH_LENGTH, DevicePath);
    if (SubPath == NULL) {
      break;
    }

    EndNode = (EFI_DEVICE_PATH_PROTOCOL *)((UINTN)SubPath + Size);
    SetDevicePathEndNode (EndNode);

    SubPathTrack = SubPath;
    Status       = gBS->LocateDevicePath (
                          &gEfiPciIoProtocolGuid,
                          &SubPathTrack,
                          &CurrentHandle
                          );

    if (!EFI_ERROR (Status)) {
      Status = gBS->HandleProtocol (
                      CurrentHandle,
                      &gEfiPciIoProtocolGuid,
                      (VOID **)&PciIo
                      );
      if (!EFI_ERROR (Status)) {
        Status = PciIo->Pci.Read (
                              PciIo,
                              EfiPciIoWidthUint16,
                              0,
                              1,
                              &VendorId
                              );
        if (!EFI_ERROR (Status)) {
          if (VendorId == BypassVendorId) {
            BypassNeeded = TRUE;
            FreePool (SubPath);
            SubPath = NULL;
            break;
          }
        }
      }
    }

    FreePool (SubPath);
    SubPath        = NULL;
    TempDevicePath = NextDevicePathNode (TempDevicePath);
  }

  return BypassNeeded;
}

/**
  Convert the PciHandle to SourceId.
**/
EFI_STATUS
EFIAPI
GetSourceIdFromPciHandle (
  IN EFI_HANDLE                 PciHandle,
  OUT SOURCE_ID                 *SourceId,
  OUT SMMU_V3_TRANSLATION_MODE  *TranslationMode
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
  UINT16               BypassVendorId;

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
    "%a: Device Info - Segment: %02x Bus: 0x%02x Device: 0x%02x Function: 0x%02x\n",
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
    DEBUG ((DEBUG_ERROR, "%a: PCI Handle not found for Segment number %lu\r\n", __FUNCTION__, Segment));
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  SourceId->StreamId      = StreamId;
  SourceId->SmmuV3pHandle = SmmuV3pHandle;

  // Check DTB for bypass vendor ID first
  BypassVendorId = GetBypassVendorIdFromDtb ();
  if (BypassVendorId == 0) {
    // No bypass vendor ID in DTB, use translate mode
    *TranslationMode = SMMU_V3_TRANSLATE;
    DEBUG ((DEBUG_INFO, "%a: Setting TRANSLATE mode (no bypass VID in DTB)\n", __FUNCTION__));
  } else if (CheckForBypassVendorId (PciHandle, BypassVendorId)) {
    // Found matching vendor ID, use bypass mode
    *TranslationMode = SMMU_V3_BYPASS;
    DEBUG ((DEBUG_INFO, "%a: Setting BYPASS mode due to VendorId 0x%04x\n", __FUNCTION__, BypassVendorId));
  } else {
    // No match found, use translate mode
    *TranslationMode = SMMU_V3_TRANSLATE;
    DEBUG ((DEBUG_INFO, "%a: Setting TRANSLATE mode\n", __FUNCTION__));
  }

CleanupAndReturn:
  if (Handles != NULL) {
    gBS->FreePool (Handles);
  }

  return Status;
}
