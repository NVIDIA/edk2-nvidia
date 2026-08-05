/** @file
  Null capsule attempt record client library.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#include <Library/CapsuleAttemptRecordLib.h>

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
