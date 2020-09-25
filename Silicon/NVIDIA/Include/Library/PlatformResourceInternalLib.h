/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __PLATFORM_RESOURCE_INTERNAL_LIB_H__
#define __PLATFORM_RESOURCE_INTERNAL_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Retrieve Tegra UART Base Address

**/
BOOLEAN
EFIAPI
GetTegraUARTBaseAddressInternal (
  OUT UINTN   *TegraUARTBaseAddress
);

/**
  Retrieve CPU BL Address

**/
BOOLEAN
EFIAPI
GetCPUBLBaseAddressInternal (
  OUT UINTN *CpuBootloaderAddress
);

/**
  Retrieve DTB Address

**/
BOOLEAN
EFIAPI
GetDTBBaseAddressInternal (
  OUT UINT64 *DTBBaseAddress
);

/**
  Retrieve RCM Blob Address

**/
BOOLEAN
EFIAPI
GetRCMBaseAddressInternal (
  OUT UINT64 *DTBBaseAddress
);

/**
  Retrieve Boot Type

**/
BOOLEAN
EFIAPI
GetBootTypeInternal (
  OUT TEGRA_BOOT_TYPE *BootType
);

/**
  Retrieve Resource Config

**/
EFI_STATUS
EFIAPI
GetResourceConfigInternal (
  IN  UINTN               CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO *PlatformInfo
);

#endif //__PLATFORM_RESOURCE_INTERNAL_LIB_H__
