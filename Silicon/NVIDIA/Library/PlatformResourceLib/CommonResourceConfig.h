/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/**
  Get disable register for each socket

**/
EFI_STATUS
EFIAPI
GetDisableRegArray (
  IN UINT32   SocketMask,
  IN UINT64   SocketOffset,
  IN UINT64   DisableRegAddr,
  IN UINT32   DisableRegMask,
  IN UINT32   DisableRegShift,
  OUT UINT32  *DisableRegArray
  );

#endif
