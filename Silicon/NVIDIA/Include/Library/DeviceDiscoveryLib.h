/** @file
*
*  Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
*  Portions provided under the following terms:
*  Copyright (c) 2018-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2022 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#ifndef __DEVICE_DISCOVERY_LIB_H__
#define __DEVICE_DISCOVERY_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/NonDiscoverableDevice.h>

typedef struct {
  VOID                          *DeviceTreeBase;
  INT32                         NodeOffset;
  EFI_GUID                      *DeviceType;
  NON_DISCOVERABLE_DEVICE_INIT  PciIoInitialize;
} NVIDIA_DT_NODE_INFO;

/**
 * @brief API used to check if node is supported
 *
 * @param DeviceInfo     - Info regarding device tree base address,node offset,
 *                         device type and init function.
 * @return EFI_STATUS    - EFI_SUCCESS if supported, others for error
 **/
typedef
EFI_STATUS
(EFIAPI * DEVICE_TREE_NODE_SUPPORTED) (
  IN OUT  NVIDIA_DT_NODE_INFO  *DeviceInfo
);

/**
 * @brief Process a given device node, this creates the memory map for it and registers support protocols.
 *
 * @param DeviceInfo     - Info regarding device tree base address,node offset,
 *                         device type and init function.
 * @param Device         - Device structure that contains memory information
 * @param DriverHandle   - Handle of the driver that is connecting to the device
 * @param DeviceHandle   - Handle of the device that was registered
 * @return EFI_STATUS    - EFI_SUCCESS on success, others for error
 **/
EFI_STATUS
ProcessDeviceTreeNodeWithHandle(
  IN      NVIDIA_DT_NODE_INFO        *DeviceInfo,
  IN      NON_DISCOVERABLE_DEVICE    *Device,
  IN      EFI_HANDLE                 DriverHandle,
  IN OUT  EFI_HANDLE                 *DeviceHandle
  );

/**
 * @brief Get the Next Supported Device Tree Node object
 *
 * @param IsNodeSupported - Function to check if this driver supports a given node
 * @param DeviceInfo      - Info regarding node offset, device type and init function.
 *                          device type and init function.
 * @return EFI_STATUS     - EFI_SUCCESS if node found, EFI_NOT_FOUND for no more remaining, others for error
 **/
EFI_STATUS
GetNextSupportedDeviceTreeNode (
  IN  DEVICE_TREE_NODE_SUPPORTED IsNodeSupported,
  IN OUT NVIDIA_DT_NODE_INFO     *DeviceInfo
  );

/**
 * @brief Get all Supported Device Tree Node objects
 *
 * @param DeviceTreeBase  - Pointer to the base of the device tree of the system
 * @param IsNodeSupported - Function to check if this driver supports a given node
 * @param DeviceCount     - Number of matching nodes/devices.
 * @param DTNodeInfo      - Device type and offsets of all nodes that was matched.
 * @return EFI_STATUS     - EFI_SUCCESS if node found, EFI_NOT_FOUND for no more remaining, others for error
 **/
EFI_STATUS
GetSupportedDeviceTreeNodes (
  IN  VOID                        *DeviceTreeBase,  OPTIONAL
  IN  DEVICE_TREE_NODE_SUPPORTED  IsNodeSupported,
  IN OUT UINT32                   *DeviceCount,
  OUT NVIDIA_DT_NODE_INFO         *DTNodeInfo
  );
#endif //__DEVICE_DISCOVERY_LIB_H__
