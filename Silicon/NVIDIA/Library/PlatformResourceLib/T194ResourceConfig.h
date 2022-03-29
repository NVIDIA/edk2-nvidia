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

EFI_STATUS
T194ResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
);

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
  Retrieve MMIO Base and Size

**/
TEGRA_MMIO_INFO*
EFIAPI
T194GetMmioBaseAndSize (
  VOID
);

/**
  Retrieve EEPROM Data

**/
TEGRABL_EEPROM_DATA*
EFIAPI
T194GetEepromData (
  IN  UINTN CpuBootloaderAddress
);

/**
  Retrieve Board Information

**/
BOOLEAN
EFIAPI
T194GetBoardInfo(
  IN  UINTN            CpuBootloaderAddress,
  OUT TEGRA_BOARD_INFO *BoardInfo
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
  Validate Boot Chain

**/
BOOLEAN
EFIAPI
T194BootChainIsValid(
  VOID
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

#endif //__T194_RESOURCE_CONFIG_H__
