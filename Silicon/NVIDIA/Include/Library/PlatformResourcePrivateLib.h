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

#ifndef __PLATFORM_RESOURCE_PRIVATE_LIB_H__
#define __PLATFORM_RESOURCE_PRIVATE_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Retrieve Tegra UART Base Address

**/
BOOLEAN
EFIAPI
GetTegraUARTBaseAddressPrivate (
  IN  BOOLEAN ConsolePort,
  OUT UINTN   *TegraUARTBaseAddress
);

/**
  Retrieve CPU BL Address

**/
BOOLEAN
EFIAPI
GetCPUBLBaseAddressPrivate (
  OUT UINTN *CpuBootloaderAddress
);

/**
  Retrieve DTB Address

**/
BOOLEAN
EFIAPI
GetDTBBaseAddressPrivate (
  OUT UINT64 *DTBBaseAddress
);

/**
  Retrieve Resource Config

**/
EFI_STATUS
EFIAPI
GetResourceConfigPrivate (
  IN  UINTN               CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO *PlatformInfo
);

#endif //__PLATFORM_RESOURCE_PRIVATE_LIB_H__
