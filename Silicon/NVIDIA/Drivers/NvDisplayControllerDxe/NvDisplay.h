/** @file

  NV Display Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NV_DISPLAY_H__
#define __NV_DISPLAY_H__

/**
  Retrieves base and size of the framebuffer region.

  @param[out] Base  Base of the framebuffer region.
  @param[out] Size  Size of the framebuffer region.

  @retval EFI_SUCCESS    Region details retrieved successfully.
  @retval EFI_NOT_FOUND  The framebuffer region was not found.
*/
EFI_STATUS
NvDisplayGetFramebufferRegion (
  OUT EFI_PHYSICAL_ADDRESS  *Base,
  OUT UINTN                 *Size
  );

/**
  Retrieve address space descriptors of the NV display MMIO regions.

  On call, *Size must be the size of avaliable memory pointed to by
  Desc; if Desc is NULL, *Size must be 0.

  On return, *Size will contain the minimum size required for the
  descriptors.

  @param[in]     DriverHandle      Driver handle.
  @param[in]     ControllerHandle  ControllerHandle.
  @param[out]    Desc              Address space descriptors.
  @param[in,out] Size              Size of the descriptors.

  @retval EFI_SUCCESS            Operation successful.
  @retval EFI_INVALID_PARAMETER  Size is NULL.
  @retval EFI_INVALID_PARAMETER  Desc is NULL, but *Size is non-zero.
  @retval EFI_BUFFER_TOO_SMALL   Desc is not NULL, but *Size is too small.
  @retval EFI_OUT_OF_RESOURCES   Memory allocation failed.
  @retval EFI_NOT_FOUND          At least one display MMIO region was not found.
  @retval !=EFI_SUCCESS          Operation failed.
*/
EFI_STATUS
NvDisplayGetMmioRegions (
  IN  EFI_HANDLE                         DriverHandle,
  IN  EFI_HANDLE                         ControllerHandle,
  OUT EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Desc  OPTIONAL,
  OUT UINTN                              *Size
  );

/**
  Assert or deassert display resets.

  The Resets array must be terminated by a NULL entry.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.
  @param[in] Resets            Names of the resets.
  @param[in] Assert            Assert/deassert the reset signal.

  @retval EFI_SUCCESS  Operation successful.
  @retval others       Error(s) occurred.
*/
EFI_STATUS
NvDisplayAssertResets (
  IN EFI_HANDLE          DriverHandle,
  IN EFI_HANDLE          ControllerHandle,
  IN CONST CHAR8 *CONST  Resets[],
  IN BOOLEAN             Assert
  );

/**
  Enable or disable display clocks. In addition, set given clock
  parents before enable.

  Both Clocks and ClockParents arrays must be terminated by NULL
  entries.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.
  @param[in] Clocks            Names of the clocks.
  @param[in] ClockParents      Child-parent clock pairs to set.
  @param[in] Enable            Enable/disable the clocks.

  @return EFI_SUCCESS    Clocks successfully enabled/disabled.
  @return !=EFI_SUCCESS  An error occurred.
*/
EFI_STATUS
NvDisplayEnableClocks (
  IN EFI_HANDLE          DriverHandle,
  IN EFI_HANDLE          ControllerHandle,
  IN CONST CHAR8 *CONST  Clocks[],
  IN CONST CHAR8 *CONST  ClockParents[][2],
  IN BOOLEAN             Enable
  );

#endif // __NV_DISPLAY_H__
