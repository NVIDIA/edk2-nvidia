/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
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
*  Copyright (c) 2020-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2022 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
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

EFI_STATUS
EFIAPI
T234GetCarveoutInfo (
  IN UINTN               CpuBootloaderAddress,
  IN TEGRA_CARVEOUT_TYPE Type,
  IN UINTN               *Base,
  IN UINT32              *Size
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
  Retrieve EEPROM Data

**/
TEGRABL_EEPROM_DATA*
EFIAPI
T234GetEepromData (
  IN  UINTN CpuBootloaderAddress
);

/**
  Retrieve Board Information

**/
BOOLEAN
EFIAPI
T234GetBoardInfo(
  IN  UINTN            CpuBootloaderAddress,
  OUT TEGRA_BOARD_INFO *BoardInfo
);

/**
  Retrieve Active Boot Chain Information

**/
EFI_STATUS
EFIAPI
T234GetActiveBootChain(
  IN  UINTN   CpuBootloaderAddress,
  OUT UINT32  *BootChain
);

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
T234ValidateActiveBootChain(
  IN  UINTN   CpuBootloaderAddress
);

#endif //__T234_RESOURCE_CONFIG_H__
