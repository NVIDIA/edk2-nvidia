/** @file

  Tegra Platform Info Library's Private Structures.

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __EFI_TEGRA_PLATFORM_INFO_LIB_PRIVATE_H__
#define __EFI_TEGRA_PLATFORM_INFO_LIB_PRIVATE_H__

/* Assumption: HIDREV register address and fields are constant across tegra chips */
#define HIDREV_OFFSET                            0x4
#define HIDREV_CHIPID_SHIFT                      8
#define HIDREV_CHIPID_MASK                       0xff
#define HIDREV_PRE_SI_PLAT_SHIFT                 0x14
#define HIDREV_PRE_SI_PLAT_MASK                  0xf
#define HIDREV_ADDRESS (NV_ADDRESS_MAP_MISC_BASE + HIDREV_OFFSET)

#define DEFAULT_BLINFO_LOCATION_ADDRESS          0x0C3903F8

#define TEGRA_SYSTEM_MEMORY_BASE                 0X80000000

#define TEGRA_COMBINED_UART_RX_MAILBOX           0X03C10000
#define TEGRA_COMBINED_UART_TX_MAILBOX           0X0C168000

#endif // __EFI_TEGRA_PLATFORM_INFO_LIB_PRIVATE_H__
