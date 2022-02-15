/** @file

  Tegra Platform Info Library's Private Structures.

  Copyright (c) 2019-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFI_TEGRA_PLATFORM_INFO_LIB_PRIVATE_H__
#define __EFI_TEGRA_PLATFORM_INFO_LIB_PRIVATE_H__

/* Assumption: HIDREV register address and fields are constant across tegra chips */
#define HIDREV_OFFSET                            0x4
#define HIDREV_CHIPID_SHIFT                      8
#define HIDREV_CHIPID_MASK                       0xff
#define HIDREV_MAJORVER_SHIFT                    4
#define HIDREV_MAJORVER_MASK                     0xf
#define HIDREV_PRE_SI_PLAT_SHIFT                 0x14
#define HIDREV_PRE_SI_PLAT_MASK                  0xf
#define HIDREV_ADDRESS (NV_ADDRESS_MAP_MISC_BASE + HIDREV_OFFSET)

#define T194_BLINFO_LOCATION_ADDRESS             0x0C3903F8
#define DEFAULT_BLINFO_LOCATION_ADDRESS          0x0C390154

#define TEGRA_SYSTEM_MEMORY_BASE                 0X80000000

#define TEGRA_COMBINED_UART_RX_MAILBOX           0X03C10000
#define TEGRA_COMBINED_UART_TX_MAILBOX           0X0C168000

#endif // __EFI_TEGRA_PLATFORM_INFO_LIB_PRIVATE_H__
