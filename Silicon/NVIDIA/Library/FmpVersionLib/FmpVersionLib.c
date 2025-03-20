/** @file

  FMP version library

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/FmpVersionLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>

EFI_STATUS  mFmpVersionStatus  = EFI_UNSUPPORTED;
UINT32      mFmpVersion        = 0;
CHAR16      *mFmpVersionString = NULL;

EFI_STATUS
EFIAPI
FmpVersionGet (
  OUT UINT32 *Version, OPTIONAL
  OUT CHAR16  **VersionString   OPTIONAL
  )
{
  UINTN  VersionStringSize;

  if (EFI_ERROR (mFmpVersionStatus)) {
    DEBUG ((DEBUG_ERROR, "%a: bad status: %r\n", __FUNCTION__, mFmpVersionStatus));
    return mFmpVersionStatus;
  }

  if (Version != NULL) {
    *Version = mFmpVersion;
  }

  if (VersionString != NULL) {
    // version string must be in allocated pool memory that caller frees
    VersionStringSize = StrSize (mFmpVersionString);
    *VersionString    = (CHAR16 *)AllocateRuntimeCopyPool (
                                    VersionStringSize,
                                    mFmpVersionString
                                    );
    if (*VersionString == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: version 0x%08x (%s)\n", __FUNCTION__, mFmpVersion, mFmpVersionString));

  return EFI_SUCCESS;
}
