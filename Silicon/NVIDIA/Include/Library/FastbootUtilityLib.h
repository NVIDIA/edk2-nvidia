/** @file
  Fastboot Utility Library.

  Provides helpers used by the boot manager and other consumers to
  decide whether the system should enter Android Fastboot mode based
  on user input from USB HID devices (USB keyboards and the NVIDIA
  Thunderstrike controller).

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef FASTBOOT_UTILITY_LIB_H_
#define FASTBOOT_UTILITY_LIB_H_

#include <Uefi.h>

/**
  Polls connected USB HID devices for the Fastboot entry key combination
  (the A and B keys / buttons pressed simultaneously).

  Supported devices:
    - USB keyboards (HID boot protocol)
    - NVIDIA Thunderstrike controller (vendor 0x0955, product 0x7214)

  The function forces a USB enumeration (so newly attached gamepads are
  discovered), then polls for a bounded amount of time. It exits early
  when no candidate device is present so the boot path is not slowed
  down on systems without an HID device attached.

  @retval TRUE   The A+B key combination was detected.
  @retval FALSE  The combination was not detected within the polling window.
**/
BOOLEAN
EFIAPI
CheckFastbootKeyCombo (
  VOID
  );

#endif // FASTBOOT_UTILITY_LIB_H_
