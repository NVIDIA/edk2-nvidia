/** @file
  NVIDIA Device Tree Node Protocol

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

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

  VOID  *DeviceTreeBase;
  INT32 NodeOffset;

};

extern EFI_GUID gNVIDIADeviceTreeNodeProtocolGuid;

#endif
