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
*  Portions provided under the following terms:
*  Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
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
  OUT EFI_PHYSICAL_ADDRESS  *TegraUARTBaseAddress
)
{
  return FALSE;
}

/**
  Retrieve the type and address of UART based on the instance Number

**/
BOOLEAN
EFIAPI
GetUARTInstanceInfoInternal (
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
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

/**
  Retrieve MMIO Base and Size

**/
TEGRA_MMIO_INFO*
EFIAPI
GetMmioBaseAndSizeInternal (
  VOID
)
{
  return NULL;
}

/**
  Retrieve Board Information

**/
BOOLEAN
EFIAPI
GetBoardInfoInternal (
  OUT TEGRA_BOARD_INFO *BoardInfo
)
{
  return FALSE;
}

/**
  Retrieve Active Boot Chain Information

**/
BOOLEAN
EFIAPI
GetActiveBootChainInternal (
  OUT UINT32 *BootChain
)
{
  return FALSE;
}

/**
  Validate Active Boot Chain

**/
BOOLEAN
EFIAPI
ValidateActiveBootChainInternal (
  VOID
)
{
  return FALSE;
}

/**
  Get Ramloaded OS Base and Size

**/
BOOLEAN
EFIAPI
GetRamdiskOSBaseAndSizeInternal (
  OUT UINTN *Base,
  OUT UINTN *Size
)
{
  return FALSE;
}
