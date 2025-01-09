/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __T234_RESOURCE_CONFIG_H__
#define __T234_RESOURCE_CONFIG_H__

#include <Library/PlatformResourceLib.h>

/**
  Retrieve Dram Page Blacklist Info Address

**/
NVDA_MEMORY_REGION *
T234GetDramPageBlacklistInfoAddress (
  IN  UINTN  CpuBootloaderAddress
  );

UINT64
T234GetDTBBaseAddress (
  IN UINTN  CpuBootloaderAddress
  );

UINT64
T234GetGRBlobBaseAddress (
  IN UINTN  CpuBootloaderAddress
  );

/**
  Retrieve Active Boot Chain Information

**/
EFI_STATUS
EFIAPI
T234GetActiveBootChain (
  IN  UINTN   CpuBootloaderAddress,
  OUT UINT32  *BootChain
  );

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
T234ValidateActiveBootChain (
  IN  UINTN  CpuBootloaderAddress
  );

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
T234GetPlatformResourceInformation (
  IN UINTN                         CpuBootloaderAddress,
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

/**
  Get Rootfs Status Register Value

**/
EFI_STATUS
EFIAPI
T234GetRootfsStatusReg (
  IN UINT32  *RegisterValue
  );

/**
  Set Rootfs Status Register

**/
EFI_STATUS
EFIAPI
T234SetRootfsStatusReg (
  IN UINT32  RegisterValue
  );

/**
  Set next boot chain

**/
EFI_STATUS
EFIAPI
T234SetNextBootChain (
  IN  UINT32  BootChain
  );

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
T234GetEnabledCoresBitMap (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

/**
  Set next boot into recovery

**/
VOID
EFIAPI
T234SetNextBootRecovery (
  IN  VOID
  );

/**
  Update info in Platform Resource Information

**/
EFI_STATUS
EFIAPI
T234UpdatePlatformResourceInformation (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

/**
  Retrieve T234 Active Boot Chain Information for StMm.

  @param[in]  ScratchBase           Base address of scratch register space.
  @param[out] BootChain             Active boot chain (0=A, 1=B).
 *
 * @retval  EFI_SUCCESS             Boot chain retrieved successfully.
 * @retval  others                  Error retrieving boot chain.
**/
EFI_STATUS
EFIAPI
T234GetActiveBootChainStMm (
  IN  UINTN   ScratchBase,
  OUT UINT32  *BootChain
  );

#endif //__T234_RESOURCE_CONFIG_H__
