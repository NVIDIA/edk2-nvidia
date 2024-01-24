/** @file
  Generic watchdog info parser.

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "GenericWatchdogInfoParser.h"
#include <Library/NVIDIADebugLib.h>
#include <Library/TegraPlatformInfoLib.h>

// Watchdog
#define SBSA_WATCHDOG_EDGE_TRIGGERED   \
          EFI_ACPI_6_4_GTDT_ARM_GENERIC_WATCHDOG_FLAG_TIMER_INTERRUPT_MODE
#define SBSA_WATCHDOG_LEVEL_TRIGGERED  0
#define SBSA_WATCHDOG_ACTIVE_LOW       \
          EFI_ACPI_6_4_GTDT_ARM_GENERIC_WATCHDOG_FLAG_TIMER_INTERRUPT_POLARITY
#define SBSA_WATCHDOG_ACTIVE_HIGH  0
#define SBSA_WATCHDOG_SECURE           \
          EFI_ACPI_6_4_GTDT_ARM_GENERIC_WATCHDOG_FLAG_SECURE_TIMER
#define SBSA_WATCHDOG_NON_SECURE  0

#define SBSA_WATCHDOG_FLAGS  (SBSA_WATCHDOG_NON_SECURE    |           \
                                          SBSA_WATCHDOG_ACTIVE_HIGH | \
                                          SBSA_WATCHDOG_EDGE_TRIGGERED)

/** Generic watchdog info parser function.

  The following structure is populated:
  EArmObjPlatformGenericWatchdogInfo

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
GenericWatchdogInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                    Status;
  CM_ARM_GENERIC_WATCHDOG_INFO  Watchdog;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (TegraGetPlatform () == TEGRA_PLATFORM_VDK) {
    return EFI_NOT_FOUND;
  }

  Watchdog.ControlFrameAddress = PcdGet64 (PcdGenericWatchdogControlBase);
  Watchdog.RefreshFrameAddress = PcdGet64 (PcdGenericWatchdogRefreshBase);
  Watchdog.TimerGSIV           = PcdGet32 (PcdGenericWatchdogEl2IntrNum);
  Watchdog.Flags               = SBSA_WATCHDOG_FLAGS;

  if ((Watchdog.ControlFrameAddress != 0) &&
      (Watchdog.RefreshFrameAddress != 0) &&
      (Watchdog.TimerGSIV != 0))
  {
    // Add the CmObj to the Configuration Manager.
    Status = NvAddSingleCmObj (
               ParserHandle,
               CREATE_CM_ARM_OBJECT_ID (EArmObjPlatformGenericWatchdogInfo),
               &Watchdog,
               sizeof (Watchdog),
               NULL
               );
  } else {
    Status = EFI_NOT_FOUND;
  }

  return Status;
}
