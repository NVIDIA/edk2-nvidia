/** @file

  NV Display Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NV_DISPLAY_CONTROLLER_H__
#define __NV_DISPLAY_CONTROLLER_H__

/**
  Enables or disables chip-specific display hardware.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.
  @param[in] Enable            TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Operation failed.
*/
typedef
EFI_STATUS
(*NV_DISPLAY_CONTROLLER_HW_ENABLE)(
  IN EFI_HANDLE  DriverHandle,
  IN EFI_HANDLE  ControllerHandle,
  IN BOOLEAN     Enable
  );

/**
  Starts the NV display controller driver on the given controller
  handle.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.
  @param[in] HwEnable          Chip-specific display HW control function.

  @retval EFI_SUCCESS          Operation successful.
  @retval EFI_ALREADY_STARTED  Driver has already been started on the given handle.
  @retval !=EFI_SUCCESS        Operation failed.
*/
EFI_STATUS
NvDisplayControllerStart (
  IN EFI_HANDLE                       DriverHandle,
  IN EFI_HANDLE                       ControllerHandle,
  IN NV_DISPLAY_CONTROLLER_HW_ENABLE  HwEnable
  );

/**
  Starts the NV T234 display controller driver on the given
  controller handle.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.

  @retval EFI_SUCCESS          Operation successful.
  @retval EFI_ALREADY_STARTED  Driver has already been started on the given handle.
  @retval !=EFI_SUCCESS        Operation failed.
*/
EFI_STATUS
NvDisplayControllerStartT234 (
  IN EFI_HANDLE  DriverHandle,
  IN EFI_HANDLE  ControllerHandle
  );

/**
  Stops the NV display controller driver on the given controller
  handle.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Operation failed.
*/
EFI_STATUS
NvDisplayControllerStop (
  IN EFI_HANDLE  DriverHandle,
  IN EFI_HANDLE  ControllerHandle
  );

/**
  Handles the ExitBootServices event within the NV display controller
  driver started on the given controller handle.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.

  @retval EFI_SUCCESS  Display shut down.
  @retval EFI_ABORTED  Performing hand-off, display left running.
  @retval others       Operation failed.
*/
EFI_STATUS
NvDisplayControllerOnExitBootServices (
  IN EFI_HANDLE  DriverHandle,
  IN EFI_HANDLE  ControllerHandle
  );

#endif // __NV_DISPLAY_CONTROLLER_H__
