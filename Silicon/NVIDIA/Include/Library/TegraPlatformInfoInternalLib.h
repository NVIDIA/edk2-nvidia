/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __TEGRA_PLATFORM_INFO_INTERNAL_LIB_H__
#define __TEGRA_PLATFORM_INFO_INTERNAL_LIB_H__

#ifndef __ASSEMBLY__

/**
  Returns CPUBL carveout affset for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              CPUBL carveout offset.

**/
extern UINT64 TegraGetBLCarveoutOffsetInternal (UINT32 ChipID);

/**
  Returns gic distributor base address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Gic distributor base address.

**/
extern UINT64 TegraGetGicDistributorBaseAddressInternal (UINT32 ChipID);

/**
  Returns gic redistributor base address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Gic redistributor base address.

**/
extern UINT64 TegraGetGicRedistributorBaseAddressInternal (UINT32 ChipID);

#endif /* !__ASSEMBLY */

#endif //__TEGRA_PLATFORM_INFO_INTERNAL_LIB_H__
