/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __T194_RESOURCE_CONFIG_H__
#define __T194_RESOURCE_CONFIG_H__

#include <Library/PlatformResourceLib.h>

UINT64
T194GetDTBBaseAddress (
  IN UINTN CpuBootloaderAddress
);

EFI_STATUS
EFIAPI
T194GetCarveoutInfo (
  IN UINTN               CpuBootloaderAddress,
  IN TEGRA_CARVEOUT_TYPE Type,
  IN UINTN               *Base,
  IN UINT32              *Size
);

TEGRA_BOOT_TYPE
T194GetBootType (
  IN UINTN CpuBootloaderAddress
);

UINT64
T194GetGRBlobBaseAddress (
  IN UINTN CpuBootloaderAddress
);

/**
  Retrieve GR Output Base and Size

**/
BOOLEAN
T194GetGROutputBaseAndSize (
  IN  UINTN CpuBootloaderAddress,
  OUT UINTN *Base,
  OUT UINTN *Size
);

/**
  Retrieve Active Boot Chain Information

**/
EFI_STATUS
EFIAPI
T194GetActiveBootChain(
  IN  UINTN   CpuBootloaderAddress,
  OUT UINT32  *BootChain
);

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
T194ValidateActiveBootChain(
  IN  UINTN   CpuBootloaderAddress
);

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
T194GetPlatformResourceInformation(
  IN UINTN                        CpuBootloaderAddress,
  IN TEGRA_PLATFORM_RESOURCE_INFO *PlatformResourceInfo
);

/**
  Get Rootfs Status Register Value

**/
EFI_STATUS
EFIAPI
T194GetRootfsStatusReg(
  IN UINTN                        CpuBootloaderAddress,
  IN UINT32                       *RegisterValue
);

/**
  Set Rootfs Status Register

**/
EFI_STATUS
EFIAPI
T194SetRootfsStatusReg(
  IN UINTN                        CpuBootloaderAddress,
  IN UINT32                       RegisterValue
);

#endif //__T194_RESOURCE_CONFIG_H__
