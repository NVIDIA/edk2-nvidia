/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
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

#endif //__TH500_RESOURCE_CONFIG_H__
