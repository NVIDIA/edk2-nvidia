/** @file

  Tegra Platform Info Library's Private Structures.

  SPDX-FileCopyrightText: Copyright (c) 2019-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFI_TEGRA_PLATFORM_INFO_LIB_PRIVATE_H__
#define __EFI_TEGRA_PLATFORM_INFO_LIB_PRIVATE_H__

#include <IndustryStandard/ArmStdSmc.h>

#define SMCCC_ARCH_SOC_ID_GET_SOC_VERSION   0
#define SMCCC_ARCH_SOC_ID_GET_SOC_REVISION  1

#define HIDREV_OFFSET                     0x4
#define SOC_ID_VERSION_CHIPID_SHIFT       4
#define SOC_ID_VERSION_CHIPID_MASK        0xfff
#define SOC_ID_VERSION_MAJORVER_SHIFT     0
#define SOC_ID_VERSION_MAJORVER_MASK      0xf
#define SOC_ID_REVISION_MINORVER_SHIFT    4
#define SOC_ID_REVISION_MINORVER_MASK     0xf
#define SOC_ID_REVISION_OPT_SUBREV_SHIFT  0
#define SOC_ID_REVISION_OPT_SUBREV_MASK   0xf
#define HIDREV_PRE_SI_PLAT_SHIFT          0x14
#define HIDREV_PRE_SI_PLAT_MASK           0xf
#define HIDREV_ADDRESS                    (NV_ADDRESS_MAP_MISC_BASE + HIDREV_OFFSET)

#define DEFAULT_BLINFO_LOCATION_ADDRESS  0x0C390154

#define TEGRA_SYSTEM_MEMORY_BASE  0X80000000

#define TEGRA_COMBINED_UART_RX_MAILBOX  0X03C10000
#define TEGRA_COMBINED_UART_TX_MAILBOX  0X0C168000

#endif // __EFI_TEGRA_PLATFORM_INFO_LIB_PRIVATE_H__
