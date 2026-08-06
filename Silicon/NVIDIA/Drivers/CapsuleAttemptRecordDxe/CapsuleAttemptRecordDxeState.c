/** @file
  Capsule attempt record DXE protocol state implementation.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

#include "CapsuleAttemptRecordDxeState.h"

VOID
CapsuleAttemptRecordStateBegin (
  OUT CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State,
  IN  CAPSULE_ATTEMPT_RECORD_SOURCE          Source,
  IN  UINT64                                 AttemptId
  )
{
  State->DiagnosticsValid = FALSE;
  State->AttemptActive    = TRUE;
  State->AttemptId        = AttemptId;
  State->AttemptSource    = Source;
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
  State->AttemptActive    = FALSE;
  State->AttemptId        = 0;
  State->AttemptSource    = CapsuleAttemptRecordSourceUnknown;
  State->DiagnosticsValid = FALSE;
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
  IN     CONST UINT32                           *Completion OPTIONAL
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
      ((Completion != NULL) && (*Completion > 100)))
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

  Diagnostics.Producer = CheckImage ? CapsuleAttemptRecordProducerCheckImage :
                         CapsuleAttemptRecordProducerSetImage;
  Diagnostics.FailedImage     = (FailedImageAscii[0] != '\0') ? FailedImageAscii : NULL;
  Diagnostics.RawStatus       = RawStatus;
  Diagnostics.NvidiaStatus    = NvidiaStatus;
  Diagnostics.PlatformSpec    = (PlatformSpecAscii[0] != '\0') ? PlatformSpecAscii : NULL;
  Diagnostics.TnSpec          = (TnSpecAscii[0] != '\0') ? TnSpecAscii : NULL;
  Diagnostics.Completion      = (Completion != NULL) ? *Completion : 0;
  Diagnostics.CompletionValid = (Completion != NULL);

  CopyMem (State->FailedImage, FailedImageAscii, sizeof (State->FailedImage));
  CopyMem (State->PlatformSpec, PlatformSpecAscii, sizeof (State->PlatformSpec));
  CopyMem (State->TnSpec, TnSpecAscii, sizeof (State->TnSpec));
  CopyMem (&State->Diagnostics, &Diagnostics, sizeof (State->Diagnostics));
  State->Diagnostics.FailedImage  = (State->FailedImage[0] != '\0') ? State->FailedImage : NULL;
  State->Diagnostics.PlatformSpec = (State->PlatformSpec[0] != '\0') ? State->PlatformSpec : NULL;
  State->Diagnostics.TnSpec       = (State->TnSpec[0] != '\0') ? State->TnSpec : NULL;
  State->DiagnosticsValid         = TRUE;

  return EFI_SUCCESS;
}

CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS *
CapsuleAttemptRecordStateGetDiagnostics (
  IN CONST CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State
  )
{
  return State->DiagnosticsValid ? &State->Diagnostics : NULL;
}
