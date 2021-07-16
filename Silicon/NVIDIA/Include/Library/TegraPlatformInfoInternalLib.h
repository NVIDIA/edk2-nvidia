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
*  Portions provided under the following terms:
*  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
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
