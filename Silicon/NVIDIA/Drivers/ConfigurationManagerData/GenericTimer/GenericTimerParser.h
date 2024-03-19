/** @file
  Generic timer parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef GENERIC_TIMER_PARSER_H_
#define GENERIC_TIMER_PARSER_H_

#include <Library/NvCmObjectDescUtility.h>

#define GTDT_GLOBAL_FLAGS_MAPPED      \
          EFI_ACPI_6_4_GTDT_GLOBAL_FLAG_MEMORY_MAPPED_BLOCK_PRESENT
#define GTDT_GLOBAL_FLAGS_NOT_MAPPED  0
#define GTDT_GLOBAL_FLAGS_EDGE        \
          EFI_ACPI_6_4_GTDT_GLOBAL_FLAG_INTERRUPT_MODE
#define GTDT_GLOBAL_FLAGS_LEVEL  0

/*
  Note: We could have a build flag that switches between memory
  mapped/non-memory mapped timer
*/
#ifdef SYSTEM_COUNTER_BASE_ADDRESS
#define GTDT_GLOBAL_FLAGS  (GTDT_GLOBAL_FLAGS_MAPPED |            \
                                         GTDT_GLOBAL_FLAGS_LEVEL)
#else
#define GTDT_GLOBAL_FLAGS            (GTDT_GLOBAL_FLAGS_NOT_MAPPED |  \
                                         GTDT_GLOBAL_FLAGS_LEVEL)
#define SYSTEM_COUNTER_BASE_ADDRESS  0xFFFFFFFFFFFFFFFF
#define SYSTEM_COUNTER_READ_BASE     0xFFFFFFFFFFFFFFFF
#endif

#define GTDT_TIMER_EDGE_TRIGGERED   \
          EFI_ACPI_6_4_GTDT_TIMER_FLAG_TIMER_INTERRUPT_MODE
#define GTDT_TIMER_LEVEL_TRIGGERED  0
#define GTDT_TIMER_ACTIVE_LOW       \
          EFI_ACPI_6_4_GTDT_TIMER_FLAG_TIMER_INTERRUPT_POLARITY
#define GTDT_TIMER_ACTIVE_HIGH  0
#define GTDT_TIMER_SAVE_CONTEXT     \
          EFI_ACPI_6_4_GTDT_TIMER_FLAG_ALWAYS_ON_CAPABILITY
#define GTDT_TIMER_LOSE_CONTEXT  0

#define GTDT_GTIMER_FLAGS  (GTDT_TIMER_ACTIVE_LOW |          \
                                       GTDT_TIMER_LEVEL_TRIGGERED)

#define GTDT_GTIMER_FLAGS_SAVE  (GTDT_TIMER_SAVE_CONTEXT |          \
                                       GTDT_TIMER_ACTIVE_LOW | \
                                       GTDT_TIMER_LEVEL_TRIGGERED)

/** Generic timer parser function

  The following structure is populated:
  typedef struct CmArmGenericTimerInfo {
    /// The physical base address for the counter control frame
    UINT64    CounterControlBaseAddress;

    /// The physical base address for the counter read frame
    UINT64    CounterReadBaseAddress;

    /// The secure PL1 timer interrupt
    UINT32    SecurePL1TimerGSIV;

    /// The secure PL1 timer flags
    UINT32    SecurePL1TimerFlags;

    /// The non-secure PL1 timer interrupt
    UINT32    NonSecurePL1TimerGSIV;

    /// The non-secure PL1 timer flags
    UINT32    NonSecurePL1TimerFlags;

    /// The virtual timer interrupt
    UINT32    VirtualTimerGSIV;

    /// The virtual timer flags
    UINT32    VirtualTimerFlags;

    /// The non-secure PL2 timer interrupt
    UINT32    NonSecurePL2TimerGSIV;

    /// The non-secure PL2 timer flags
    UINT32    NonSecurePL2TimerFlags;

    /// GSIV for the virtual EL2 timer
    UINT32    VirtualPL2TimerGSIV;

    /// Flags for the virtual EL2 timer
    UINT32    VirtualPL2TimerFlags;
  } CM_ARM_GENERIC_TIMER_INFO;

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
GenericTimerParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif // GENERIC_TIMER_PARSER_H_
