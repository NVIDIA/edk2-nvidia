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

#endif // __NV_DISPLAY_H__
