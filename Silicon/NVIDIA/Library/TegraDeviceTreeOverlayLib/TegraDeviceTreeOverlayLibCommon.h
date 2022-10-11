/** @file
*
*  Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __TEGRA_DEVICE_TREE_OVERLAY_LIB_COMMON_H__
#define __TEGRA_DEVICE_TREE_OVERLAY_LIB_COMMON_H__

#include <Uefi/UefiBaseType.h>
#include <Library/PlatformResourceLib.h>

typedef struct {
  UINTN                 FuseBaseAddr;
  TEGRA_FUSE_INFO       *FuseList;
  UINTN                 FuseCount;
  EEPROM_PART_NUMBER    *ProductIds;
  UINTN                 IdCount;
} OVERLAY_BOARD_INFO;

EFI_STATUS
ApplyTegraDeviceTreeOverlayCommon (
  VOID                *FdtBase,
  VOID                *FdtOverlay,
  CHAR8               *SWModule,
  OVERLAY_BOARD_INFO  *BoardInfo
  );

#endif //__TEGRA_DEVICE_TREE_OVERLAY_LIB_COMMON_H__
