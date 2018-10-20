/** @file

  BPMP IPC private structures

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __BPMP_IPC_DXE_PRIVATE_H__
#define __BPMP_IPC_DXE_PRIVATE_H__

#include <PiDxe.h>
#include <Uefi.h>
#include <Protocol/ComponentName.h>
#include <Protocol/DriverBinding.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/NonDiscoverableDevice.h>

//
// Global Variables definitions
//
extern EFI_DRIVER_BINDING_PROTOCOL  gBpmpIpcDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL  gBpmpIpcComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL gBpmpIpcComponentName2;

//Amount to stall during timeout loops
#define TIMEOUT_STALL_US 10

//Time to poll in in 100ns intervals
#define BPMP_POLL_INTERVAL 1000 //(100us)

/**
  This routine is called right after the .Supported() called and
  Starts the HspDoorbell protocol on the device.

  @param This                     Protocol instance pointer.
  @param Controller               Handle of device to bind driver to.
  @param NonDiscoverableProtocol  A pointer to the NonDiscoverableProtocol.

  @retval EFI_SUCCESS             This driver is added to this device.
  @retval EFI_ALREADY_STARTED     This driver is already running on this device.
  @retval other                   Some error occurs when binding this driver to this device.

**/
EFI_STATUS
EFIAPI
HspDoorbellProtocolStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN NON_DISCOVERABLE_DEVICE        *NonDiscoverableProtocol
  );

/**
  This routine is called right after the .Supported() called and
  Start the BmpIpc protocol on the device.

  @param This                   Protocol instance pointer.
  @param Controller             Handle of device to bind driver to.
  @param NonDiscoverableProtocol  A pointer to the NonDiscoverableProtocol.

  @retval EFI_SUCCESS           This driver is added to this device.
  @retval EFI_ALREADY_STARTED   This driver is already running on this device.
  @retval other                 Some error occurs when binding this driver to this device.

**/
EFI_STATUS
EFIAPI
BpmpIpcProtocolStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN NON_DISCOVERABLE_DEVICE        *NonDiscoverableProtocol
  );

/**
  This function disconnects the HspDoorbell protocol from the specified controller.

  @param This                     Protocol instance pointer.
  @param Controller               Handle of device to disconnect driver from.
  @param NonDiscoverableProtocol  A pointer to the NonDiscoverableProtocol.

  @retval EFI_SUCCESS   This driver is removed from this device.
  @retval other         Some error occurs when removing this driver from this device.

**/
EFI_STATUS
EFIAPI
HspDoorbellProtocolStop (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN NON_DISCOVERABLE_DEVICE        *NonDiscoverableProtocol
  );

/**
  This function disconnects the BpmpIpc protocol from the specified controller.

  @param This                     Protocol instance pointer.
  @param Controller               Handle of device to disconnect driver from.
  @param NonDiscoverableProtocol  A pointer to the NonDiscoverableProtocol.

  @retval EFI_SUCCESS   This driver is removed from this device.
  @retval other         Some error occurs when removing this driver from this device.

**/
EFI_STATUS
EFIAPI
BpmpIpcProtocolStop (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN NON_DISCOVERABLE_DEVICE        *NonDiscoverableProtocol
  );

#endif
