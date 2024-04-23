/** @file

  Android Boot Config Driver

  SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <libfdt.h>
#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BootConfigProtocolLib.h>
#include <AndroidBootImgHeader.h>
#include "AndroidBootConfig.h"

#define ANDROIDBOOT_ARG_PREFIX    "androidboot."
#define MAX_ANDROIDBOOT_ARG_SIZE  32
#define MAX_ANDROIDBOOT_VAL_SIZE  256

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

CHAR16  *mLineBuffer = NULL;

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
  IN CHAR8    *Params,
  IN UINT32   ParamsSize,
  IN UINT64   BootConfigStartAddr,
  IN UINT32   BootConfigSize,
  OUT UINT32  *AppliedBytes
  )
{
  UINT32      _AppliedBytes = 0;
  INT32       NewSize       = 0;
  UINT64      End;
  EFI_STATUS  status;

  if (!Params || !BootConfigStartAddr || !AppliedBytes || (ParamsSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  End = BootConfigStartAddr + BootConfigSize;

  if (IsTrailerPresent (End)) {
    End           -= BOOTCONFIG_TRAILER_SIZE;
    _AppliedBytes -= BOOTCONFIG_TRAILER_SIZE;
    CopyMem (&NewSize, (VOID *)End, BOOTCONFIG_SIZE_SIZE);
  } else {
    NewSize = BootConfigSize;
  }

  // params
  CopyMem ((VOID *)End, Params, ParamsSize);

  _AppliedBytes += ParamsSize;
  *AppliedBytes  = _AppliedBytes;

  status = AddBootConfigTrailer (
             BootConfigStartAddr,
             BootConfigSize + _AppliedBytes,
             &_AppliedBytes
             );
  if (EFI_ERROR (status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error add trailer\n", __FUNCTION__));
  }

  *AppliedBytes += _AppliedBytes;

  return EFI_SUCCESS;
}

/*
 * Add boot config trailer.
 */
EFI_STATUS
AddBootConfigTrailer (
  IN UINT64   BootConfigStartAddr,
  IN UINT32   BootConfigSize,
  OUT UINT32  *TrailerSize
  )
{
  if (!BootConfigStartAddr || !TrailerSize) {
    return EFI_INVALID_PARAMETER;
  }

  *TrailerSize = 0;

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

  *TrailerSize = BOOTCONFIG_TRAILER_SIZE;

  return EFI_SUCCESS;
}

/*
 * Copy all "androidboot.<arg>" from cmdline and add to bootconfig protocol
 *
 * @param[in]  Cmdline cmdline buffer in uefi, which may include "androidboot.<arg>"
 * @return EFI_SUCCESS if success, else if not.
 */
EFI_STATUS
CopyAndroidBootArgsToBootConfig (
  IN CHAR8  *Cmdline
  )
{
  EFI_STATUS                         Status                             = EFI_SUCCESS;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigUpdate                  = NULL;
  CHAR8                              *AndroidBootArg                    = NULL;
  CHAR8                              *RemainingCmdline                  = NULL;
  CHAR8                              *TokenEndPtr                       = NULL;
  CHAR8                              *EndPtr                            = NULL;
  CHAR8                              Arg[MAX_ANDROIDBOOT_ARG_SIZE]      = { 0 };
  CHAR8                              ArgValue[MAX_ANDROIDBOOT_VAL_SIZE] = { 0 };
  UINT32                             Length;

  if (Cmdline == NULL) {
    Status = EFI_SUCCESS;
    goto Exit;
  }

  Status = GetBootConfigUpdateProtocol (&BootConfigUpdate);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %r to get BootConfigUpdateProtocol\n", __FUNCTION__, Status));
    goto Exit;
  }

  EndPtr           = Cmdline + AsciiStrLen (Cmdline);
  RemainingCmdline = Cmdline;

  do {
    AndroidBootArg = AsciiStrStr (RemainingCmdline, ANDROIDBOOT_ARG_PREFIX);
    if (AndroidBootArg == NULL) {
      break;
    }

    // Arg name
    AndroidBootArg += AsciiStrLen (ANDROIDBOOT_ARG_PREFIX);
    TokenEndPtr     = AsciiStrStr (AndroidBootArg, "=");
    if (TokenEndPtr == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Bad androidboot.args, missing a \'=\'\n", __FUNCTION__));
      Status = EFI_INVALID_PARAMETER;
      goto Exit;
    }

    Length = MIN (TokenEndPtr - AndroidBootArg, MAX_ANDROIDBOOT_ARG_SIZE - 1);
    AsciiStrnCpyS (Arg, MAX_ANDROIDBOOT_ARG_SIZE, AndroidBootArg, Length);
    Arg[Length] = '\0';
    if (Length < TokenEndPtr - AndroidBootArg) {
      DEBUG ((DEBUG_ERROR, "%a: Potential buffer overflow, may break arg=%a\n", __FUNCTION__, Arg));
    }

    // Arg Value
    AndroidBootArg = TokenEndPtr + 1;
    TokenEndPtr    = AsciiStrStr (AndroidBootArg, " ");
    TokenEndPtr    = (TokenEndPtr == NULL) ? EndPtr : TokenEndPtr;
    Length         = MIN (TokenEndPtr - AndroidBootArg, MAX_ANDROIDBOOT_VAL_SIZE - 1);
    AsciiStrnCpyS (ArgValue, MAX_ANDROIDBOOT_VAL_SIZE, AndroidBootArg, Length);
    ArgValue[Length] = '\0';
    if (Length < TokenEndPtr - AndroidBootArg) {
      DEBUG ((DEBUG_ERROR, "%a: Potential buffer overflow, may break argval=%a\n", __FUNCTION__, ArgValue));
    }

    Status = BootConfigUpdate->UpdateBootConfigs (BootConfigUpdate, Arg, ArgValue);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: %r to update BootConfigUpdateProtocol\n", __FUNCTION__, Status));
      goto Exit;
    }

    RemainingCmdline = TokenEndPtr;
  } while (RemainingCmdline < EndPtr);

Exit:
  return Status;
}

/*
 * Append bootconfig in dtb node to bootconfig memory
 */
EFI_STATUS
EFIAPI
AddBootConfigFromDtb (
  IN UINT64   BootConfigStartAddr,
  IN UINT32   BootConfigSize,
  OUT UINT32  *AppliedBytes
  )
{
  EFI_STATUS  Status;
  INT32       NodeOffset;
  VOID        *KernelDtb;
  CHAR8       *BootConfigEntry = NULL;
  INT32       BootConfigLength;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &KernelDtb);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %r to get kernel dtb from sys table\n", __FUNCTION__, Status));
    return EFI_NOT_FOUND;
  }

  NodeOffset = fdt_path_offset (KernelDtb, "/chosen");
  if (NodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find /chosen in DTB\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  BootConfigEntry = (CHAR8 *)fdt_getprop (KernelDtb, NodeOffset, "bootconfig", &BootConfigLength);
  if (NULL == BootConfigEntry) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find bootconfig node\n", __FUNCTION__));
    // Not a fatal issue as dtb bootconfig can be empty for some platforms
    *AppliedBytes = 0;
    return EFI_SUCCESS;
  }

  Status = AddBootConfigParameters (BootConfigEntry, BootConfigLength, BootConfigStartAddr, BootConfigSize, AppliedBytes);

  return Status;
}
