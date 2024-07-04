/** @file

  Android Boot Config Driver

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include "AndroidBootConfig.h"

/*
 * Simple checksum for a buffer.
 *
 * @param Addr pointer to the start of the buffer.
 * @param Size size of the buffer in bytes.
 * @return check sum result.
 */
STATIC
UINT32
CheckSum (
  IN CONST CHAR8  *Buffer,
  IN UINT32       Size
  )
{
  UINT32  Sum   = 0;
  UINT32  Index = 0;

  for (Index = 0; Index < Size; Index++) {
    Sum += Buffer[Index];
  }

  return Sum;
}

/*
 * Check if the bootconfig trailer is present within the bootconfig section.
 *
 * @param BootConfigEndAddr address of the end of the bootconfig section. If
 *        the trailer is present, it will be directly preceding this address.
 * @return true if the trailer is present, false if not.
 */
STATIC
BOOLEAN
IsTrailerPresent (
  IN UINT64  BootConfigEndAddr
  )
{
  return !AsciiStrnCmp (
            (CHAR8 *)(BootConfigEndAddr - BOOTCONFIG_MAGIC_SIZE),
            BOOTCONFIG_MAGIC,
            BOOTCONFIG_MAGIC_SIZE
            );
}

/*
 * Add a string of boot config parameters to memory appended by the trailer.
 */
EFI_STATUS
AddBootConfigParameters (
  IN CHAR8   *Params,
  IN UINT32  ParamsSize,
  IN UINT64  BootConfigStartAddr,
  UINT32     BootConfigSize
  )
{
  if (!Params || !BootConfigStartAddr) {
    return EFI_INVALID_PARAMETER;
  }

  if (ParamsSize == 0) {
    return EFI_INVALID_PARAMETER;
  }

  INT32   AppliedBytes = 0;
  INT32   NewSize      = 0;
  UINT64  End          = BootConfigStartAddr + BootConfigSize;

  if (IsTrailerPresent (End)) {
    End          -= BOOTCONFIG_TRAILER_SIZE;
    AppliedBytes -= BOOTCONFIG_TRAILER_SIZE;
    CopyMem (&NewSize, (VOID *)End, BOOTCONFIG_SIZE_SIZE);
  } else {
    NewSize = BootConfigSize;
  }

  // params
  CopyMem ((VOID *)End, Params, ParamsSize);

  AppliedBytes += ParamsSize;
  AppliedBytes += AddBootConfigTrailer (
                    BootConfigStartAddr,
                    BootConfigSize + AppliedBytes
                    );

  return EFI_SUCCESS;
}

/*
 * Add boot config trailer.
 */
EFI_STATUS
AddBootConfigTrailer (
  IN UINT64  BootConfigStartAddr,
  IN UINT32  BootConfigSize
  )
{
  if (!BootConfigStartAddr) {
    return EFI_INVALID_PARAMETER;
  }

  if (BootConfigSize == 0) {
    return EFI_SUCCESS;
  }

  UINT64  End = BootConfigStartAddr + BootConfigSize;

  if (IsTrailerPresent (End)) {
    // no need to overwrite the current trailers
    return EFI_SUCCESS;
  }

  // size
  CopyMem ((VOID *)(End), &BootConfigSize, BOOTCONFIG_SIZE_SIZE);

  // checksum
  UINT32  Sum =
    CheckSum ((CHAR8 *)BootConfigStartAddr, BootConfigSize);

  CopyMem (
    (VOID *)(End + BOOTCONFIG_SIZE_SIZE),
    &Sum,
    BOOTCONFIG_CHECKSUM_SIZE
    );

  // magic
  CopyMem (
    (VOID *)(End + BOOTCONFIG_SIZE_SIZE + BOOTCONFIG_CHECKSUM_SIZE),
    BOOTCONFIG_MAGIC,
    BOOTCONFIG_MAGIC_SIZE
    );

  return EFI_SUCCESS;
}
