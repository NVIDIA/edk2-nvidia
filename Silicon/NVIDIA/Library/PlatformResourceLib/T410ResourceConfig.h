/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __T410_RESOURCE_CONFIG_H__
#define __T410_RESOURCE_CONFIG_H__

#include <Library/PlatformResourceLib.h>

// copied from TH500ResourceConfigPrivate.h but a T410 version

#define T410_BOOT_CHAIN_SCRATCH_OFFSET  0x908C
typedef enum {
  T410MemoryModeNormal,
  T410MemoryModeEgmNoHv,
  T410MemoryModeEgmWithHv,
  T410MemoryModeMax
} T410_MEMORY_MODE;

BOOLEAN
T410UARTInstanceInfo (
  IN  UINT32                SharedUARTInstanceId,
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
  );

/**
  Retrieve Dram Page Blacklist Info Address

**/
NVDA_MEMORY_REGION *
T410GetDramPageBlacklistInfoAddress (
  IN  UINTN  CpuBootloaderAddress
  );

UINT64
T410GetDTBBaseAddress (
  IN UINTN  CpuBootloaderAddress
  );

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
T410GetPlatformResourceInformation (
  IN UINTN                         CpuBootloaderAddress,
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  IN BOOLEAN                       InMm
  );

/**
 * Get Partition information.
**/

EFI_STATUS
EFIAPI
T410GetPartitionInfo (
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
T410ValidateActiveBootChain (
  IN  UINTN  CpuBootloaderAddress
  );

/**
  Get Socket Mask

**/
UINT32
EFIAPI
T410GetSocketMask (
  IN UINTN  CpuBootloaderAddress
  );

/**
  Register TPM Events

  This function copies and registers Pre-UEFI TPM Events into the GUID HOB list.

  @param  TpmLog  Physical address to Pre-UEFI TPM measurement data
**/
EFI_STATUS
EFIAPI
T410BuildTcgEventHob (
  IN UINTN  TpmLogAddress
  );

/**
 * Check TPM Status
**/
BOOLEAN
EFIAPI
T410IsTpmToBeEnabled (
  IN  UINTN  CpuBootloaderAddress
  );

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
T410GetEnabledCoresBitMap (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

/**
  Update info in Platform Resource Information

**/
EFI_STATUS
EFIAPI
T410UpdatePlatformResourceInformation (
  IN  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

#endif //__T410_RESOURCE_CONFIG_H__
