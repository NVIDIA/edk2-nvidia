/** @file

  NV Display Controller Driver - T264

  SPDX-FileCopyrightText: Copyright (c) 2021-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include "NvDisplay.h"
#include "NvDisplayController.h"

/**
  Assert or deassert T264 display resets.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.
  @param[in] Assert            Assert/deassert the reset signal.

  @retval EFI_SUCCESS  Operation successful.
  @retval others       Error(s) occurred.
*/
STATIC
EFI_STATUS
AssertResets (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Assert
  )
{
  STATIC CONST CHAR8 *CONST  Resets[] = {
    "dpaux0_reset",
    NULL
  };

  return NvDisplayAssertResets (
           DriverHandle,
           ControllerHandle,
           Resets,
           Assert
           );
}

/**
  Enable or disable required T264 display clocks.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.
  @param[in] Enable            Enable/disable the clocks.

  @return EFI_SUCCESS    Clocks successfully enabled/disabled.
  @return !=EFI_SUCCESS  An error occurred.
*/
STATIC
EFI_STATUS
EnableClocks (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable
  )
{
  STATIC CONST CHAR8 *CONST  Clocks[] = {
    "nvdisplay_disp_clk",
    "nvdisplayhub_clk",
    "maud_clk",
    "aza_2xbit_clk",
    "dpaux0_clk",
    NULL
  };
  STATIC CONST CHAR8 *CONST  ClockParents[][2] = {
    { "disp_root",        "disppll_clk"        },
    { "nvdisplayhub_clk", "sppll0_clkoutb_clk" },
    { NULL,               NULL                 }
  };

  return NvDisplayEnableClocks (
           DriverHandle,
           ControllerHandle,
           Clocks,
           ClockParents,
           Enable
           );
}

/**
  Enables or disables T264 display hardware.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.
  @param[in] Enable            TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Operation failed.
*/
STATIC
EFI_STATUS
EnableHwT264 (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable
  )
{
  EFI_STATUS  Status, Status1;
  BOOLEAN     ResetsDeasserted = !Enable;
  BOOLEAN     ClocksEnabled    = !Enable;

  if (Enable) {
    Status = AssertResets (DriverHandle, ControllerHandle, FALSE);
    if (EFI_ERROR (Status)) {
      goto Disable;
    }

    ResetsDeasserted = TRUE;

    Status = EnableClocks (DriverHandle, ControllerHandle, TRUE);
    if (EFI_ERROR (Status)) {
      goto Disable;
    }

    ClocksEnabled = TRUE;
  } else {
    /* Shutdown display HW if and only if we were called to disable
       the display. */
    Status = NvDisplayHwShutdown (DriverHandle, ControllerHandle);

Disable:
    if (ClocksEnabled) {
      Status1 = EnableClocks (DriverHandle, ControllerHandle, FALSE);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      ClocksEnabled = FALSE;
    }

    if (ResetsDeasserted) {
      Status1 = AssertResets (DriverHandle, ControllerHandle, TRUE);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      ResetsDeasserted = FALSE;
    }
  }

  return Status;
}

/**
  Starts the NV T264 display controller driver on the given
  controller handle.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.

  @retval EFI_SUCCESS          Operation successful.
  @retval EFI_ALREADY_STARTED  Driver has already been started on the given handle.
  @retval !=EFI_SUCCESS        Operation failed.
*/
EFI_STATUS
NvDisplayControllerStartT264 (
  IN EFI_HANDLE  DriverHandle,
  IN EFI_HANDLE  ControllerHandle
  )
{
  return NvDisplayControllerStart (
           DriverHandle,
           ControllerHandle,
           EnableHwT264
           );
}
