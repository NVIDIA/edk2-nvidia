/** @file
  NVIDIA Device Tree Compatibility Protocol

  Copyright (c) 2018, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL_H__
#define __NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL_H__

#include <Uefi/UefiSpec.h>
#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/DeviceTreeNode.h>

#define NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL_GUID \
  { \
  0x1e710608, 0x28a3, 0x4c0b, { 0x9b, 0xec, 0x1c, 0x75, 0x49, 0xa7, 0x0d, 0x90 } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL;

/**
  This function is invoked to allow the system to check if this implementation supports
  the device tree node. If EFI_SUCCESS is returned then handle will be created and driver binding
  will occur.

  @param[in]  This                   The instance of the NVIDIA_DEVICE_TREE_BINDING_PROTOCOL.
  @param[in]  Node                   The pointer to the requested node info structure.
  @param[out] DeviceType             Pointer to allow the return of the device type
  @param[out] PciIoInitialize        Pointer to allow return of function that will be called
                                       when the PciIo subsystem connects to this device.
                                       Note that this will may not be called if the device
                                       is not in the boot path.

  @return EFI_SUCCESS               The node is supported by this instance
  @return EFI_UNSUPPORTED           The node is not supported by this instance
**/
typedef
EFI_STATUS
(EFIAPI *DEVICE_TREE_COMPATIBILITY_SUPPORTED)(
  IN NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL   *This,
  IN CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL      *Node,
  OUT EFI_GUID                                   **DeviceType,
  OUT NON_DISCOVERABLE_DEVICE_INIT               *PciIoInitialize
  );

/// NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL protocol structure.
struct _NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL {
  DEVICE_TREE_COMPATIBILITY_SUPPORTED    Supported;
};

extern EFI_GUID  gNVIDIADeviceTreeCompatibilityProtocolGuid;

#endif
