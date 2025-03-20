/** @file

  FMP version library using PCD versions

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/FmpVersionLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>

extern EFI_STATUS  mFmpVersionStatus;
extern UINT32      mFmpVersion;
extern CHAR16      *mFmpVersionString;

STATIC
EFI_STATUS
EFIAPI
FmpVersionPcdGetInfo (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       VersionStrLen;
  UINT64      Version64;
  CHAR16      *VersionStr;

  VersionStrLen     = StrSize ((CHAR16 *)PcdGetPtr (PcdUefiVersionString));
  mFmpVersionString = (CHAR16 *)AllocateRuntimePool (VersionStrLen);
  if (mFmpVersionString == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: string alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  StrCpyS (mFmpVersionString, VersionStrLen / sizeof (CHAR16), (CHAR16 *)PcdGetPtr (PcdUefiVersionString));

  VersionStr = (CHAR16 *)PcdGetPtr (PcdUefiHexVersionNumber);
  if (VersionStr == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Version data doesn't exist\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  if (VersionStr[0] == 0x0) {
    Version64 = 0;
  } else {
    Status = StrHexToUint64S (VersionStr, NULL, &Version64);
    if (EFI_ERROR (Status) || (Version64 > MAX_UINT32)) {
      DEBUG ((DEBUG_ERROR, "%a: Version data invalid\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }
  }

  mFmpVersion       = Version64;
  mFmpVersionStatus = EFI_SUCCESS;

  DEBUG ((DEBUG_ERROR, "%a: got version=0x%x (%s)\n", __FUNCTION__, mFmpVersion, mFmpVersionString));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FmpVersionLibInit (
  UINT32                      ActiveBootChain,
  FMP_VERSION_READY_CALLBACK  Callback
  )
{
  EFI_STATUS  Status;

  if (Callback == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = FmpVersionPcdGetInfo ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: PcdGetInfo failed: %r\n", __FUNCTION__, Status));
  }

  Callback (mFmpVersionStatus);

  return EFI_SUCCESS;
}
