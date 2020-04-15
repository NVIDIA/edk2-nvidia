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

#ifndef __TEGRA_PLATFORM_INFO_PRIVATE_LIB_H__
#define __TEGRA_PLATFORM_INFO_PRIVATE_LIB_H__

#ifndef __ASSEMBLY__

/**
  Returns gic distributor base address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Gic distributor base address.

**/
extern UINT64 TegraGetGicDistributorBaseAddressPrivate (UINT32 ChipID);

/**
  Returns gic redistributor base address for a given chip.

  @param[in] ChipID    Tegra Chip ID

  @retval              Gic redistributor base address.

**/
extern UINT64 TegraGetGicRedistributorBaseAddressPrivate (UINT32 ChipID);

#endif /* !__ASSEMBLY */

#endif //__TEGRA_PLATFORM_INFO_PRIVATE_LIB_H__
