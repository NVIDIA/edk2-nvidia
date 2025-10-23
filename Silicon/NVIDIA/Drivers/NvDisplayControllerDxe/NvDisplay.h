/** @file

  NV Display Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2024-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NV_DISPLAY_H__
#define __NV_DISPLAY_H__

#include <Protocol/GraphicsOutput.h>

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
  Retrieve rates of the given clock with specified parent clocks, then
  reset the clock parent and rate to safe osc clock.

  @param[in]  DriverHandle      Handle to the driver.
  @param[in]  ControllerHandle  Handle to the controller.
  @param[in]  ClockName         Name of the clock.
  @param[in]  ParentClockNames  Name of the parent clocks.
  @param[out] RatesKhz          Rates of the clock with corresponding parents.

  @retval EFI_SUCCESS    Rates retrieved successfully.
  @retval EFI_NOT_FOUND  The osc clock was not found on the controller.
  @retval EFI_NOT_FOUND  The clock name not found on controller.
  @retval EFI_NOT_FOUND  Parent clock name not found on controller.
  @retval others         Other errors occurred.
*/
EFI_STATUS
NvDisplayGetClockRatesWithParentsAndReset (
  IN  EFI_HANDLE          DriverHandle,
  IN  EFI_HANDLE          ControllerHandle,
  IN  CONST CHAR8         *ClockName,
  IN  CONST CHAR8 *CONST  *ParentClockNames,
  OUT UINT32              *RatesKhz
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

/**
  Shutdown active display HW before reset to prevent a lingering bad
  state.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Error(s) occurred.
*/
EFI_STATUS
NvDisplayHwShutdown (
  IN EFI_HANDLE  DriverHandle,
  IN EFI_HANDLE  ControllerHandle
  );

/**
  Locates a child handle with an active GOP instance installed.

  This function does not allocate any memory, hence it is safe to call
  during ExitBootServices.

  @param[in]  DriverHandle      Handle of the driver.
  @param[in]  ControllerHandle  Handle of the controller.
  @param[out] Protocol          The located active GOP instance.

  @retval EFI_SUCCESS    Child handle found successfully.
  @retval !=EFI_SUCCESS  Error occurred.
*/
EFI_STATUS
NvDisplayLocateActiveChildGop (
  IN  EFI_HANDLE                    DriverHandle,
  IN  EFI_HANDLE                    ControllerHandle,
  OUT EFI_GRAPHICS_OUTPUT_PROTOCOL  **Protocol  OPTIONAL
  );

/**
  Update the Device Tree with mode and framebuffer info using an
  active GOP instance installed on a child handle.

  @param[in] DriverHandle      Handle of the driver.
  @param[in] ControllerHandle  Handle of the controller.

  @return TRUE   Device Tree updated successfully.
  @return FALSE  No Device Tree was found.
  @return FALSE  No GOP child handle was found.
  @return FALSE  The GOP child handle was inactive.
  @return FALSE  Could not retrieve the framebuffer region.
  @return FALSE  Failed to update the Device Tree.
*/
BOOLEAN
NvDisplayUpdateFdtTableActiveChildGop (
  IN EFI_HANDLE  DriverHandle,
  IN EFI_HANDLE  ControllerHandle
  );

/**
  Enable the EFIFB driver if there is an active GOP instance with a
  suitable framebuffer installed on a child handle.

  @param[in] DriverHandle      Handle of the driver.
  @param[in] ControllerHandle  Handle of the controller.

  @return TRUE   EFIFB driver enabled successfully.
  @return FALSE  No GOP child handle was found.
  @return FALSE  The GOP child handle was inactive.
  @return FALSE  The EFI framebuffer is not suitable for EFIFB driver.
 */
BOOLEAN
NvDisplayEnableEfifbActiveChildGop (
  IN EFI_HANDLE  DriverHandle,
  IN EFI_HANDLE  ControllerHandle
  );

#endif // __NV_DISPLAY_H__
