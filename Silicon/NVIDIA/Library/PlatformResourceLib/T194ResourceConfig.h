/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __T194_RESOURCE_CONFIG_H__
#define __T194_RESOURCE_CONFIG_H__

#include <Library/PlatformResourceLib.h>

/**
  Retrieve Dram Page Blacklist Info Address

**/
NVDA_MEMORY_REGION *
T194GetDramPageBlacklistInfoAddress (
  IN  UINTN  CpuBootloaderAddress
  );

UINT64
T194GetDTBBaseAddress (
  IN UINTN  CpuBootloaderAddress
  );

UINT64
T194GetGRBlobBaseAddress (
  IN UINTN  CpuBootloaderAddress
  );

/**
  Retrieve Active Boot Chain Information

**/
EFI_STATUS
EFIAPI
T194GetActiveBootChain (
  IN  UINTN   CpuBootloaderAddress,
  OUT UINT32  *BootChain
  );

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
T194ValidateActiveBootChain (
  IN  UINTN  CpuBootloaderAddress
  );

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
T194GetPlatformResourceInformation (
  IN UINTN                         CpuBootloaderAddress,
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

/**
  Get Rootfs Status Register Value

**/
EFI_STATUS
EFIAPI
T194GetRootfsStatusReg (
  IN UINTN   CpuBootloaderAddress,
  IN UINT32  *RegisterValue
  );

/**
  Set Rootfs Status Register

**/
EFI_STATUS
EFIAPI
T194SetRootfsStatusReg (
  IN UINTN   CpuBootloaderAddress,
  IN UINT32  RegisterValue
  );

/**
  Set next boot chain

**/
EFI_STATUS
EFIAPI
T194SetNextBootChain (
  IN  UINT32  BootChain
  );

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
T194GetEnabledCoresBitMap (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

/**
  Set next boot into recovery

**/
VOID
EFIAPI
T194SetNextBootRecovery (
  IN  VOID
  );

/**
  Update info in Platform Resource Information

**/
EFI_STATUS
EFIAPI
T194UpdatePlatformResourceInformation (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

#endif //__T194_RESOURCE_CONFIG_H__
