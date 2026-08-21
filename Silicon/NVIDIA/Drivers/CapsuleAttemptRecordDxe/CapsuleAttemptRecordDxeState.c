/** @file
  Capsule attempt record DXE protocol state implementation.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>

#include "CapsuleAttemptRecordDxeState.h"

VOID
CapsuleAttemptRecordStateBegin (
  OUT CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State,
  IN  CAPSULE_ATTEMPT_RECORD_SOURCE          Source,
  IN  UINT64                                 AttemptId,
  IN  CONST EFI_TIME                         *Timestamp OPTIONAL,
  IN  CONST UINT32                           *ActiveBootChain OPTIONAL,
  IN  UINT64                                 StartCounter
  )
{
  State->AttemptActive            = TRUE;
  State->AttemptId                = AttemptId;
  State->AttemptSource            = Source;
  State->ActiveBootChain          = (ActiveBootChain != NULL) ? *ActiveBootChain : 0;
  State->ActiveBootChainValid     = (BOOLEAN)(ActiveBootChain != NULL);
  State->AttemptTimestampValid    = (BOOLEAN)(Timestamp != NULL);
  State->AttemptStartCounter      = StartCounter;
  State->AttemptStartCounterValid = TRUE;
  State->DiagnosticsValid         = FALSE;
  State->FmpImageDigestValid      = FALSE;
  if (Timestamp != NULL) {
    CopyMem (&State->AttemptTimestamp, Timestamp, sizeof (State->AttemptTimestamp));
  } else {
    ZeroMem (&State->AttemptTimestamp, sizeof (State->AttemptTimestamp));
  }
}

BOOLEAN
CapsuleAttemptRecordStateMatch (
  IN  CONST CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State,
  IN  CAPSULE_ATTEMPT_RECORD_SOURCE                Source,
  OUT UINT64                                       *AttemptId
  )
{
  *AttemptId = 0;
  if (!State->AttemptActive || (State->AttemptSource != Source)) {
    return FALSE;
  }

  *AttemptId = State->AttemptId;
  return TRUE;
}

VOID
CapsuleAttemptRecordStateConsume (
  IN OUT CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State
  )
{
  State->AttemptActive            = FALSE;
  State->AttemptId                = 0;
  State->AttemptSource            = CapsuleAttemptRecordSourceUnknown;
  State->ActiveBootChain          = 0;
  State->ActiveBootChainValid     = FALSE;
  State->AttemptTimestampValid    = FALSE;
  State->AttemptStartCounter      = 0;
  State->AttemptStartCounterValid = FALSE;
  State->DiagnosticsValid         = FALSE;
  State->FmpImageDigestValid      = FALSE;
  ZeroMem (&State->AttemptTimestamp, sizeof (State->AttemptTimestamp));
}

EFI_STATUS
CapsuleAttemptRecordStateUpdateDiagnostics (
  IN OUT CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State,
  IN     BOOLEAN                                CheckImage,
  IN     CONST CHAR16                           *FailedImage OPTIONAL,
  IN     EFI_STATUS                             RawStatus,
  IN     UINT32                                 NvidiaStatus,
  IN     CONST CHAR8                            *PlatformSpec OPTIONAL,
  IN     CONST CHAR8                            *TnSpec OPTIONAL,
  IN     CONST UINT8                            *FmpImageDigest OPTIONAL,
  IN     CONST UINT32                           *Completion OPTIONAL,
  IN     CONST UINT32                           *TargetBootChain OPTIONAL
  )
{
  EFI_STATUS                          Status;
  CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  Diagnostics;
  CHAR8                               FailedImageAscii[CAPSULE_ATTEMPT_RECORD_IMAGE_NAME_MAX + 1];
  CHAR8                               PlatformSpecAscii[CAPSULE_ATTEMPT_RECORD_PLATFORM_SPEC_MAX + 1];
  CHAR8                               TnSpecAscii[CAPSULE_ATTEMPT_RECORD_PLATFORM_SPEC_MAX + 1];

  if ((State == NULL) ||
      ((FailedImage != NULL) &&
       (StrnLenS (FailedImage, ARRAY_SIZE (FailedImageAscii)) >= ARRAY_SIZE (FailedImageAscii))) ||
      ((PlatformSpec != NULL) &&
       (AsciiStrnLenS (PlatformSpec, sizeof (PlatformSpecAscii)) >= sizeof (PlatformSpecAscii))) ||
      ((TnSpec != NULL) &&
       (AsciiStrnLenS (TnSpec, sizeof (TnSpecAscii)) >= sizeof (TnSpecAscii))) ||
      ((Completion != NULL) && (*Completion > 100)) ||
      ((TargetBootChain != NULL) &&
       (CheckImage || (*TargetBootChain >= BOOT_CHAIN_COUNT))))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (!State->AttemptActive) {
    return EFI_NOT_READY;
  }

  ZeroMem (&Diagnostics, sizeof (Diagnostics));
  ZeroMem (FailedImageAscii, sizeof (FailedImageAscii));
  ZeroMem (PlatformSpecAscii, sizeof (PlatformSpecAscii));
  ZeroMem (TnSpecAscii, sizeof (TnSpecAscii));

  if (FailedImage != NULL) {
    Status = UnicodeStrToAsciiStrS (
               FailedImage,
               FailedImageAscii,
               sizeof (FailedImageAscii)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (PlatformSpec != NULL) {
    AsciiStrCpyS (PlatformSpecAscii, sizeof (PlatformSpecAscii), PlatformSpec);
  }

  if (TnSpec != NULL) {
    AsciiStrCpyS (TnSpecAscii, sizeof (TnSpecAscii), TnSpec);
  }

  if (!State->FmpImageDigestValid && (FmpImageDigest != NULL)) {
    CopyMem (
      State->FmpImageDigest,
      FmpImageDigest,
      sizeof (State->FmpImageDigest)
      );
    State->FmpImageDigestValid = TRUE;
  }

  Diagnostics.Producer = CheckImage ? CapsuleAttemptRecordProducerCheckImage :
                         CapsuleAttemptRecordProducerSetImage;
  Diagnostics.FailedImage          = (FailedImageAscii[0] != '\0') ? FailedImageAscii : NULL;
  Diagnostics.RawStatus            = RawStatus;
  Diagnostics.NvidiaStatus         = NvidiaStatus;
  Diagnostics.PlatformSpec         = (PlatformSpecAscii[0] != '\0') ? PlatformSpecAscii : NULL;
  Diagnostics.TnSpec               = (TnSpecAscii[0] != '\0') ? TnSpecAscii : NULL;
  Diagnostics.FmpImageDigest       = State->FmpImageDigestValid ? State->FmpImageDigest : NULL;
  Diagnostics.Completion           = (Completion != NULL) ? *Completion : 0;
  Diagnostics.CompletionValid      = (Completion != NULL);
  Diagnostics.TargetBootChain      = (TargetBootChain != NULL) ? *TargetBootChain : 0;
  Diagnostics.TargetBootChainValid = (BOOLEAN)(TargetBootChain != NULL);

  CopyMem (State->FailedImage, FailedImageAscii, sizeof (State->FailedImage));
  CopyMem (State->PlatformSpec, PlatformSpecAscii, sizeof (State->PlatformSpec));
  CopyMem (State->TnSpec, TnSpecAscii, sizeof (State->TnSpec));
  CopyMem (&State->Diagnostics, &Diagnostics, sizeof (State->Diagnostics));
  State->Diagnostics.FailedImage    = (State->FailedImage[0] != '\0') ? State->FailedImage : NULL;
  State->Diagnostics.PlatformSpec   = (State->PlatformSpec[0] != '\0') ? State->PlatformSpec : NULL;
  State->Diagnostics.TnSpec         = (State->TnSpec[0] != '\0') ? State->TnSpec : NULL;
  State->Diagnostics.FmpImageDigest = State->FmpImageDigestValid ? State->FmpImageDigest : NULL;
  State->DiagnosticsValid           = TRUE;

  return EFI_SUCCESS;
}

CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS *
CapsuleAttemptRecordStateGetDiagnostics (
  IN CONST CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State
  )
{
  return State->DiagnosticsValid ? &State->Diagnostics : NULL;
}
