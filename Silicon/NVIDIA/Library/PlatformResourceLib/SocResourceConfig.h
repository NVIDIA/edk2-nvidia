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

//
// Core Bitmap Info
//
typedef struct {
  UINT32    MaxPossibleSockets;
  UINT32    MaxPossibleClustersPerSystem;
  UINT32    MaxPossibleCoresPerCluster;
  UINT32    MaxPossibleCoresPerSystem;
  UINT64    EnabledCoresBitMap[ALIGN_VALUE (MAX_SUPPORTED_CORES, 64) / 64];
  UINT32    ThreadsPerCore;
} SOC_CORE_BITMAP_INFO;

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
  IN UINTN                     CpuBootloaderAddress,
  IN OUT SOC_CORE_BITMAP_INFO  *SocCoreBitmapInfo
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

/**
  Get Total Core Count in case system supports software core disable

  @param[in]  Socket              Socket Id
  @param[out] TotalCoreCount      Total Core Count

  @retval  EFI_SUCCESS             Max Core Count retrieved successfully.
  @retval  EFI_INVALID_PARAMETER   Invalid socket id.
  @retval  EFI_INVALID_PARAMETER   TotalCoreCount is NULL
  @retval  EFI_UNSUPPORTED         Unsupported feature
**/
EFI_STATUS
EFIAPI
SocSupportsSoftwareCoreDisable (
  IN UINT32   Socket,
  OUT UINT32  *TotalCoreCount
  );

#endif
