/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __TEGRA_PLATFORM_INFO_LIB_H__
#define __TEGRA_PLATFORM_INFO_LIB_H__

#define UEFI_DECLARE_ALIGNED(var, size)  var __attribute__ ((aligned (size)))

#define T234_CHIP_ID    0x23
#define T234_MAJOR_REV  0x4
#define T264_CHIP_ID    0x26
#define TH500_CHIP_ID   0x24
/* CS definitions for the platforms */
#define NOR_FLASH_CHIP_SELECT_JETSON        0
#define NOR_FLASH_CHIP_SELECT_TH500_PRESIL  0
#define NOR_FLASH_CHIP_SELECT_TH500_SIL     0

// Jetson SKUs
#define T234_SKU         2
#define T234SLT_SKU      3
#define T234_PRESIL_SKU  255
#define T264_SKU         4
#define T264_PRESIL_SKU  254

// Server SKUs
#define TH500_SKU         5
#define TH500_PRESIL_SKU  253

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
  TEGRA_PLATFORM_VSP,
  TEGRA_PLATFORM_UNKNOWN
} TEGRA_PLATFORM_TYPE;

/**
  Returns chip id of the tegra platform.

  This function returns the chip id of the underlying tegra platform.

  @retval MAX_UINT32  ERROR
  @retval <MAX_UINT32 Tegra Chip ID
**/
extern UINT32
TegraGetChipID (
  VOID
  );

/**
  Returns system memory base address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              System memory base address.

**/
extern UINT64
TegraGetSystemMemoryBaseAddress (
  UINT32  ChipID
  );

/**
  Returns bootloader info location address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Address of bootloader info location.

**/
extern UINT64
TegraGetBLInfoLocationAddress (
  UINT32  ChipID
  );

/**
  Returns bootloader carveout info location address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Address of bootloader carveout info location.

**/
extern UINT64
TegraGetBLCarveoutInfoLocationAddress (
  UINT32  ChipID
  );

/**
  Returns tegra platform type.

  This Function returns the type of the underlying tegra platform.

  @retval TEGRA_PLATFORM_TYPE

**/
TEGRA_PLATFORM_TYPE
TegraGetPlatform (
  VOID
  );

/**
  Returns major version of the tegra platform.

  This function returns the major version of the underlying tegra platform.
**/
UINT32
TegraGetMajorVersion (
  VOID
  );

/**
  Returns the Minor Chip Rev of the of the tegra platform.

  This function returns the chip id of the underlying tegra platform.

  @retval MAX_UINT32  ERROR
  @retval <MAX_UINT32 Tegra Chip ID
**/
CHAR8 *
TegraGetMinorVersion (
  VOID
  );

#endif /* !__ASSEMBLY */

#endif //__TEGRA_PLATFORM_INFO_LIB_H__
