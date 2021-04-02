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

UINT64
T194GetRCMBaseAddress (
  IN UINTN CpuBootloaderAddress
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

#endif //__T194_RESOURCE_CONFIG_H__
