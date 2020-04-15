/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#ifndef __TEGRA_DEVICE_TREE_OVERLAY_LIB_H__
#define __TEGRA_DEVICE_TREE_OVERLAY_LIB_H__

#include <Uefi/UefiBaseType.h>

EFI_STATUS
EFIAPI
ApplyTegraDeviceTreeOverlay (
  VOID *FdtBase,
  VOID *FdtOverlay,
  CHAR8 *SWModule
);

#endif //__TEGRA_DEVICE_TREE_OVERLAY_LIB_H__
