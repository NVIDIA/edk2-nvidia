/** @file
*
*  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2021, NUVIA Inc. All rights reserved.
*  Copyright (c) 2015, Hisilicon Limited. All rights reserved.
*  Copyright (c) 2015, Linaro Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef SMBIOS_MISC_LIB_H_
#define SMBIOS_MISC_LIB_H_

#include <Uefi.h>
#include <IndustryStandard/SmBios.h>

/** Fetches the location of the physical memory array (Type16).

  @return The array location.
**/
MEMORY_ARRAY_LOCATION
EFIAPI
OemGetPhysMemArrayLocation (
  VOID
  );

/** Fetches the usage of the physical memory (Type16).

  @return The array use.
**/
MEMORY_ARRAY_USE
EFIAPI
OemGetPhysMemArrayUse (
  VOID
  );

/** Fetches the error correction type used in the physical memory array (Type16).

  @return The error correction used.
**/

MEMORY_ERROR_CORRECTION
EFIAPI
OemGetPhysMemErrCorrection (
  VOID
  );

/** Fetches the number of physical memory devices (Type16).

  @return The number of physical memory devices.
**/

UINT16
EFIAPI
OemGetPhysMemNumDevices (
  VOID
  );

/** Fetches the max capacity across all physical memory devices (Type16).

  @return The max capacity across physical memory devices.
**/

UINT64
EFIAPI
OemGetPhysMemExtendedMaxCapacity (
  VOID
  );

/** Fetches the handle/instance of the last err detected (Type16).

  @return The err/handle of the last err detected OR 0xFFFE if this
          isn't reported..
**/

UINT16
EFIAPI
OemGetPhysMemErrInfoHandle (
  VOID
  );

#endif //SMBIOS_MISC_LIB_H
