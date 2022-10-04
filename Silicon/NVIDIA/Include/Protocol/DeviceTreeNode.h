/** @file
  NVIDIA Device Tree Node Protocol

  Copyright (c) 2018, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_DEVICE_TREE_NODE_PROTOCOL_H__
#define __NVIDIA_DEVICE_TREE_NODE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>
#include <Protocol/NonDiscoverableDevice.h>

#define NVIDIA_DEVICE_TREE_NODE_PROTOCOL_GUID \
  { \
  0x149670c5, 0xb07b, 0x407a, { 0xae, 0x57, 0x39, 0xd0, 0xca, 0x51, 0x37, 0x80 } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_DEVICE_TREE_NODE_PROTOCOL NVIDIA_DEVICE_TREE_NODE_PROTOCOL;

/// NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL protocol structure.
struct _NVIDIA_DEVICE_TREE_NODE_PROTOCOL {
  VOID     *DeviceTreeBase;
  INT32    NodeOffset;
};

extern EFI_GUID  gNVIDIADeviceTreeNodeProtocolGuid;

#endif
