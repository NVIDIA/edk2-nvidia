/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __TH500_RESOURCE_CONFIG_H__
#define __TH500_RESOURCE_CONFIG_H__

#include <Library/PlatformResourceLib.h>

BOOLEAN
TH500UARTInstanceInfo (
  IN  UINT32                SharedUARTInstanceId,
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
  );

/**
  Retrieve Dram Page Blacklist Info Address

**/
NVDA_MEMORY_REGION *
TH500GetDramPageBlacklistInfoAddress (
  IN  UINTN  CpuBootloaderAddress
  );

UINT64
TH500GetDTBBaseAddress (
  IN UINTN  CpuBootloaderAddress
  );

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
TH500GetPlatformResourceInformation (
  IN UINTN                         CpuBootloaderAddress,
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  IN BOOLEAN                       InMm
  );

/**
 * Get Partition information.
**/

EFI_STATUS
EFIAPI
TH500GetPartitionInfo (
  IN  UINTN   CpuBootloaderAddress,
  IN  UINT32  PartitionIndex,
  OUT UINT16  *DeviceInstance,
  OUT UINT64  *PartitionStartByte,
  OUT UINT64  *PartitionSizeBytes
  );

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
TH500ValidateActiveBootChain (
  IN  UINTN  CpuBootloaderAddress
  );

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
TH500InValidateActiveBootChain (
  IN  UINTN  CpuBootloaderAddress
  );

/**
  Get Socket Mask

**/
UINT32
EFIAPI
TH500GetSocketMask (
  IN UINTN  CpuBootloaderAddress
  );

/**
  Register TPM Events

  This function copies and registers Pre-UEFI TPM Events into the GUID HOB list.

  @param  TpmLog  Physical address to Pre-UEFI TPM measurement data
**/
EFI_STATUS
EFIAPI
TH500BuildTcgEventHob (
  IN UINTN  TpmLogAddress
  );

/**
 * Check TPM Status
**/
BOOLEAN
EFIAPI
TH500IsTpmToBeEnabled (
  IN  UINTN  CpuBootloaderAddress
  );

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
TH500GetEnabledCoresBitMap (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

/**
  Update info in Platform Resource Information

**/
EFI_STATUS
EFIAPI
TH500UpdatePlatformResourceInformation (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

#endif //__TH500_RESOURCE_CONFIG_H__
