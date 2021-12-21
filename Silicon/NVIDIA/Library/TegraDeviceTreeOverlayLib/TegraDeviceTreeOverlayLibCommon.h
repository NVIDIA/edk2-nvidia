/** @file
*
*  Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
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
*  Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.

*  SPDX-FileCopyrightText: Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
**/

#ifndef __TEGRA_DEVICE_TREE_OVERLAY_LIB_COMMON_H__
#define __TEGRA_DEVICE_TREE_OVERLAY_LIB_COMMON_H__

#include <Uefi/UefiBaseType.h>
#include <Library/PlatformResourceLib.h>

typedef struct {
  UINTN                     FuseBaseAddr;
  TEGRA_FUSE_INFO           *FuseList;
  UINTN                     FuseCount;
  TEGRA_EEPROM_PART_NUMBER  *ProductIds;
  UINTN                     IdCount;
} OVERLAY_BOARD_INFO;

EFI_STATUS
ApplyTegraDeviceTreeOverlayCommon (
  VOID *FdtBase,
  VOID *FdtOverlay,
  CHAR8 *SWModule,
  OVERLAY_BOARD_INFO *BoardInfo
);

#endif //__TEGRA_DEVICE_TREE_OVERLAY_LIB_COMMON_H__
