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

#include <Library/PlatformResourceLib.h>
#include <Library/PlatformResourceInternalLib.h>


/**
  Retrieve Tegra UART Base Address

**/
BOOLEAN
EFIAPI
GetTegraUARTBaseAddressInternal (
  OUT UINTN   *TegraUARTBaseAddress
)
{
  return FALSE;
}

/**
  Retrieve CPU BL Address

**/
BOOLEAN
EFIAPI
GetCPUBLBaseAddressInternal (
  OUT UINTN *CpuBootloaderAddress
)
{
  return FALSE;
}

/**
  Retrieve DTB Address

**/
BOOLEAN
EFIAPI
GetDTBBaseAddressInternal (
  OUT UINT64 *DTBBaseAddress
)
{
  return FALSE;
}

/**
  Retrieve RCM Blob Address

**/
BOOLEAN
EFIAPI
GetRCMBaseAddressInternal (
  OUT UINT64 *DTBBaseAddress
)
{
  return FALSE;
}

/**
  Retrieve Boot Type

**/
BOOLEAN
EFIAPI
GetBootTypeInternal (
  OUT TEGRA_BOOT_TYPE *BootType
)
{
  return FALSE;
}

/**
  Retrieve Resource Config

**/
EFI_STATUS
EFIAPI
GetResourceConfigInternal (
  IN  UINTN               CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO *PlatformInfo
)
{
  return EFI_UNSUPPORTED;
}

/**
  Retrieve GR Blob Address

**/
BOOLEAN
EFIAPI
GetGRBlobBaseAddressInternal (
  OUT UINT64 *GRBlobBaseAddress
)
{
  return FALSE;
}

/**
  Retrieve GR Output Base and Size

**/
BOOLEAN
GetGROutputBaseAndSizeInternal (
  OUT UINTN *Base,
  OUT UINTN *Size
)
{
  return FALSE;
}

/**
  Retrieve FSI NS Base and Size

**/
BOOLEAN
EFIAPI
GetFsiNsBaseAndSizeInternal (
  OUT UINTN *Base,
  OUT UINTN *Size
)
{
  return FALSE;
}
