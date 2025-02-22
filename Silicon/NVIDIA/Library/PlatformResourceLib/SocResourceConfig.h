/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __SOC_RESOURCE_CONFIG_H__
#define __SOC_RESOURCE_CONFIG_H__

#include <Library/PlatformResourceLib.h>

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
SocGetPlatformResourceInformation (
  IN UINTN                         CpuBootloaderAddress,
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  IN BOOLEAN                       InMm
  );

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
SocGetEnabledCoresBitMap (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

/**
  Update info in Platform Resource Information

**/
EFI_STATUS
EFIAPI
SocUpdatePlatformResourceInformation (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

/**
  Get Socket Mask

**/
UINT32
EFIAPI
SocGetSocketMask (
  IN UINTN  CpuBootloaderAddress
  );

#endif
