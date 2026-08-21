/** @file
  Null capsule attempt record client library.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#include <Library/CapsuleAttemptRecordLib.h>

BOOLEAN
EFIAPI
CapsuleAttemptRecordCalculateFmpImageDigest (
  IN CONST VOID  *FmpImage,
  IN UINTN       FmpImageSize,
  OUT UINT8      FmpImageDigest[CAPSULE_ATTEMPT_RECORD_SHA256_DIGEST_SIZE]
  )
{
  return FALSE;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordBegin (
  IN CAPSULE_ATTEMPT_RECORD_SOURCE  Source
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordFinalize (
  IN CAPSULE_ATTEMPT_RECORD_SOURCE  Source
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordUpdateFmpDiagnostics (
  IN BOOLEAN       CheckImage,
  IN CONST CHAR16  *FailedImage OPTIONAL,
  IN EFI_STATUS    RawStatus,
  IN UINT32        NvidiaStatus,
  IN CONST CHAR8   *PlatformSpec OPTIONAL,
  IN CONST CHAR8   *TnSpec OPTIONAL,
  IN CONST UINT8   *FmpImageDigest OPTIONAL,
  IN CONST UINT32  *Completion OPTIONAL,
  IN CONST UINT32  *TargetBootChain OPTIONAL
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordUpdateFmpImageInfo (
  VOID
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordDumpLatest (
  VOID
  )
{
  return EFI_SUCCESS;
}
