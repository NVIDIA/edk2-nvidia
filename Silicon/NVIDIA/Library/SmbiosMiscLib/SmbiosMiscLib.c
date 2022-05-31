/** @file
*  SmbiosMiscLib.c
*
*  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2021, NUVIA Inc. All rights reserved.
*  Copyright (c) 2018, Hisilicon Limited. All rights reserved.
*  Copyright (c) 2018, Linaro Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <ConfigurationManagerObject.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SmbiosMiscLib.h>
#include <Library/PrintLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <libfdt.h>

/**
  SmbiosMiscLibConstructor
  The Constructor Function gets the Platform specific data which is installed
  by SOC specific Libraries

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/

/** Fetches the location of the physical memory array (Type16).

  @return The array location.
**/
MEMORY_ARRAY_LOCATION
EFIAPI
OemGetPhysMemArrayLocation (
  VOID
  )
{
  return MemoryArrayLocationUnknown;
}
/** Fetches the usage of the physical memory (Type16).

  @return The array use.
**/
MEMORY_ARRAY_USE
EFIAPI
OemGetPhysMemArrayUse (
  VOID
  )
{
  return MemoryArrayUseUnknown;
}

/** Fetches the error correction type used in the physical memory array (Type16).

  @return The error correction used.
**/

MEMORY_ERROR_CORRECTION
EFIAPI
OemGetPhysMemErrCorrection (
  VOID
  )
{
  return MemoryErrorCorrectionUnknown;
}

/** Fetches the number of physical memory devices (Type16).

  @return The number of physical memory devices.
**/

UINT16
EFIAPI
OemGetPhysMemNumDevices (
  VOID
 )
{
  return 0;
}

/** Fetches the max capacity across all physical memory devices (Type16).

  @return The max capacity across physical memory devices.
**/

UINT64
EFIAPI
OemGetPhysMemExtendedMaxCapacity (
  VOID
 )
{
  return 0;
}

/** Fetches the handle/instance of the last err detected (Type16).

  @return The err/handle of the last err detected OR 0xFFFE if this
          isn't reported..
**/

UINT16
EFIAPI
OemGetPhysMemErrInfoHandle (
  VOID
 )
{
  return 0xFFFE;
}

EFI_STATUS
EFIAPI
SmbiosMiscLibConstructor (
    VOID
  )
{
  return EFI_SUCCESS;
}
