/** @file

  BPMP IPC private structures

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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
extern EFI_DRIVER_BINDING_PROTOCOL   gBpmpIpcDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL   gBpmpIpcComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gBpmpIpcComponentName2;

// Amount to stall during timeout loops
#define TIMEOUT_STALL_US  10

// Time to poll in in 100ns intervals
#define BPMP_POLL_INTERVAL  1000// (100us)

/**
  This routine starts the HspDoorbell protocol on the device.

  @param NonDiscoverableProtocol  A pointer to the NonDiscoverableProtocol.

  @retval EFI_SUCCESS             This driver is added to this device.
  @retval EFI_ALREADY_STARTED     This driver is already running on this device.
  @retval other                   Some error occurs when binding this driver to this device.

**/
EFI_STATUS
EFIAPI
HspDoorbellProtocolInit (
  IN EFI_HANDLE               *Controller,
  IN NON_DISCOVERABLE_DEVICE  *NonDiscoverableProtocol
  );

/**
  This routine starts the BmpIpc protocol on the device.

  @param NonDiscoverableProtocol  A pointer to the NonDiscoverableProtocol.

  @retval EFI_SUCCESS           This driver is added to this device.
  @retval EFI_ALREADY_STARTED   This driver is already running on this device.
  @retval other                 Some error occurs when binding this driver to this device.

**/
EFI_STATUS
EFIAPI
BpmpIpcProtocolInit (
  IN EFI_HANDLE               *Controller,
  IN NON_DISCOVERABLE_DEVICE  *NonDiscoverableProtocol
  );

#endif
