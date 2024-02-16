/** @file
*
*  Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __COMMON_RESOURCE_CONFIG_H__
#define __COMMON_RESOURCE_CONFIG_H__

#include <Uefi/UefiBaseType.h>
#include <Library/PlatformResourceLib.h>

typedef struct {
  UINTN      MaxCoreDisableWords;
  BOOLEAN    SatMcSupported;

  UINT32     SatMcCore;
  UINT64     *SocketScratchBaseAddr;
  UINT32     *CoreDisableScratchOffset;
  UINT32     *CoreDisableScratchMask;
} COMMON_RESOURCE_CONFIG_INFO;

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
CommonConfigGetEnabledCoresBitMap (
  IN CONST COMMON_RESOURCE_CONFIG_INFO  *ConfigInfo,
  IN TEGRA_PLATFORM_RESOURCE_INFO       *PlatformResourceInfo
  );

#endif
