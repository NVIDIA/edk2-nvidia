/** @file

  Copyright (c) 2020-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_H__
#define PLATFORM_H__

#define GTDT_GLOBAL_FLAGS_MAPPED      \
          EFI_ACPI_6_4_GTDT_GLOBAL_FLAG_MEMORY_MAPPED_BLOCK_PRESENT
#define GTDT_GLOBAL_FLAGS_NOT_MAPPED  0
#define GTDT_GLOBAL_FLAGS_EDGE        \
          EFI_ACPI_6_4_GTDT_GLOBAL_FLAG_INTERRUPT_MODE
#define GTDT_GLOBAL_FLAGS_LEVEL       0

/*
  Note: We could have a build flag that switches between memory
  mapped/non-memory mapped timer
*/
#ifdef SYSTEM_COUNTER_BASE_ADDRESS
#define GTDT_GLOBAL_FLAGS             (GTDT_GLOBAL_FLAGS_MAPPED | \
                                         GTDT_GLOBAL_FLAGS_LEVEL)
#else
#define GTDT_GLOBAL_FLAGS             (GTDT_GLOBAL_FLAGS_NOT_MAPPED | \
                                         GTDT_GLOBAL_FLAGS_LEVEL)
#define SYSTEM_COUNTER_BASE_ADDRESS   0xFFFFFFFFFFFFFFFF
#define SYSTEM_COUNTER_READ_BASE      0xFFFFFFFFFFFFFFFF
#endif

#define GTDT_TIMER_EDGE_TRIGGERED   \
          EFI_ACPI_6_4_GTDT_TIMER_FLAG_TIMER_INTERRUPT_MODE
#define GTDT_TIMER_LEVEL_TRIGGERED  0
#define GTDT_TIMER_ACTIVE_LOW       \
          EFI_ACPI_6_4_GTDT_TIMER_FLAG_TIMER_INTERRUPT_POLARITY
#define GTDT_TIMER_ACTIVE_HIGH      0

#define GTDT_GTIMER_FLAGS           (GTDT_TIMER_ACTIVE_LOW | \
                                       GTDT_TIMER_LEVEL_TRIGGERED)

// Watchdog
#define SBSA_WATCHDOG_EDGE_TRIGGERED   \
          EFI_ACPI_6_4_GTDT_ARM_GENERIC_WATCHDOG_FLAG_TIMER_INTERRUPT_MODE
#define SBSA_WATCHDOG_LEVEL_TRIGGERED  0
#define SBSA_WATCHDOG_ACTIVE_LOW       \
          EFI_ACPI_6_4_GTDT_ARM_GENERIC_WATCHDOG_FLAG_TIMER_INTERRUPT_POLARITY
#define SBSA_WATCHDOG_ACTIVE_HIGH      0
#define SBSA_WATCHDOG_SECURE           \
          EFI_ACPI_6_4_GTDT_ARM_GENERIC_WATCHDOG_FLAG_SECURE_TIMER
#define SBSA_WATCHDOG_NON_SECURE       0

#define SBSA_WATCHDOG_FLAGS            (SBSA_WATCHDOG_NON_SECURE    | \
                                          SBSA_WATCHDOG_ACTIVE_HIGH | \
                                          SBSA_WATCHDOG_EDGE_TRIGGERED)

#endif // PLATFORM_H__

