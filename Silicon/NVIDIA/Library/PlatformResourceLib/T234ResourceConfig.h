/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#ifndef __T234_RESOURCE_CONFIG_H__
#define __T234_RESOURCE_CONFIG_H__

#include <Library/PlatformResourceLib.h>

BOOLEAN
T234UARTInstanceInfo(
  IN  UINT32                SharedUARTInstanceId,
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
);

EFI_STATUS
T234ResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
);

UINT64
T234GetDTBBaseAddress (
  IN UINTN CpuBootloaderAddress
);

UINT64
T234GetRCMBaseAddress (
  IN UINTN CpuBootloaderAddress
);

TEGRA_BOOT_TYPE
T234GetBootType (
  IN UINTN CpuBootloaderAddress
);

UINT64
T234GetGRBlobBaseAddress (
  IN UINTN CpuBootloaderAddress
);

BOOLEAN
T234GetGROutputBaseAndSize (
  IN  UINTN CpuBootloaderAddress,
  OUT UINTN *Base,
  OUT UINTN *Size
);

BOOLEAN
T234GetFsiNsBaseAndSize (
  IN  UINTN CpuBootloaderAddress,
  OUT UINTN *Base,
  OUT UINTN *Size
);

/**
  Retrieve MMIO Base and Size

**/
TEGRA_MMIO_INFO*
EFIAPI
T234GetMmioBaseAndSize (
  VOID
);

/**
  Retrieve CVM EEPROM Data

**/
UINT32
EFIAPI
T234GetCvmEepromData (
  IN  UINTN CpuBootloaderAddress,
  OUT UINT8 **Data
);

#endif //__T234_RESOURCE_CONFIG_H__
