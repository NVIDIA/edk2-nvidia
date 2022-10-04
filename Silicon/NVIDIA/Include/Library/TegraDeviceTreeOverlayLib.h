/** @file
*
*  Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __TEGRA_DEVICE_TREE_OVERLAY_LIB_H__
#define __TEGRA_DEVICE_TREE_OVERLAY_LIB_H__

#include <Uefi/UefiBaseType.h>

EFI_STATUS
EFIAPI
ApplyTegraDeviceTreeOverlay (
  VOID   *FdtBase,
  VOID   *FdtOverlay,
  CHAR8  *SWModule
  );

#endif //__TEGRA_DEVICE_TREE_OVERLAY_LIB_H__
