/** @file
*
*  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
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
*  Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#ifndef __TEGRA_PLATFORM_INFO_LIB_H__
#define __TEGRA_PLATFORM_INFO_LIB_H__

#define UEFI_DECLARE_ALIGNED(var, size) var __attribute__ ((aligned (size)))

#define T194_CHIP_ID      0x19
#define T234_CHIP_ID      0x23

#define T194_SKU          1
#define T234_SKU          2
#define T234SLT_SKU       3

#define SYSIMG_EMMC_MAGIC_OFFSET 0x4
#define SYSIMG_EMMC_MAGIC 0xEAAAAAAC
#define SYSIMG_DEFAULT_MAGIC 0xE0000000

#ifndef __ASSEMBLY__

typedef enum {
  TEGRA_PLATFORM_SILICON = 0,
  TEGRA_PLATFORM_QT,
  TEGRA_PLATFORM_SYSTEM_FPGA,
  TEGRA_PLATFORM_UNIT_FPGA,
  TEGRA_PLATFORM_ASIM_QT,
  TEGRA_PLATFORM_ASIM_LINSIM,
  TEGRA_PLATFORM_DSIM_ASIM_LINSIM,
  TEGRA_PLATFORM_VERIFICATION_SIMULATION,
  TEGRA_PLATFORM_VDK,
  TEGRA_PLATFORM_UNKNOWN
} TEGRA_PLATFORM_TYPE;

/**
  Returns chip id of the tegra platform.

  This function returns the chip id of the underlying tegra platform.

  @retval MAX_UINT32  ERROR
  @retval <MAX_UINT32 Tegra Chip ID
**/
extern UINT32 TegraGetChipID (VOID);

/**
  Returns system memory base address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              System memory base address.

**/
extern UINT64 TegraGetSystemMemoryBaseAddress (UINT32 ChipID);

/**
  Returns bootloader info location address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Address of bootloader info location.

**/
extern UINT64 TegraGetBLInfoLocationAddress (UINT32 ChipID);

/**
  Returns bootloader carveout info location address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Address of bootloader carveout info location.

**/
extern UINT64 TegraGetBLCarveoutInfoLocationAddress (UINT32 ChipID);

/**
  Returns gic distributor base address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Gic distributor base address.

**/
extern UINT64 TegraGetGicDistributorBaseAddress (UINT32 ChipID);

/**
  Returns gic redistributor base address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Gic redistributor base address.

**/
extern UINT64 TegraGetGicRedistributorBaseAddress (UINT32 ChipID);

/**
  Returns gic interrupt interface base address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Gic interrupt interface base address.

**/
extern UINT64 TegraGetGicInterruptInterfaceBaseAddress (UINT32 ChipID);

/**
  Returns tegra platform type.

  This Function returns the type of the underlying tegra platform.

  @retval TEGRA_PLATFORM_TYPE

**/
TEGRA_PLATFORM_TYPE
TegraGetPlatform (
  VOID
  );

#endif /* !__ASSEMBLY */

#endif //__TEGRA_PLATFORM_INFO_LIB_H__
