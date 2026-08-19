/** @file
  Capsule attempt record DXE protocol state definitions.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CAPSULE_ATTEMPT_RECORD_DXE_STATE_H__
#define __CAPSULE_ATTEMPT_RECORD_DXE_STATE_H__

#include "CapsuleAttemptRecordDxeInternal.h"

typedef struct {
  BOOLEAN                               AttemptActive;
  UINT64                                AttemptId;
  CAPSULE_ATTEMPT_RECORD_SOURCE         AttemptSource;
  EFI_TIME                              AttemptTimestamp;
  BOOLEAN                               AttemptTimestampValid;
  UINT64                                AttemptStartCounter;
  BOOLEAN                               AttemptStartCounterValid;
  BOOLEAN                               DiagnosticsValid;
  CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS    Diagnostics;
  BOOLEAN                               FmpImageDigestValid;
  UINT8                                 FmpImageDigest[CAPSULE_ATTEMPT_RECORD_SHA256_DIGEST_SIZE];
  CHAR8                                 FailedImage[CAPSULE_ATTEMPT_RECORD_IMAGE_NAME_MAX + 1];
  CHAR8                                 PlatformSpec[CAPSULE_ATTEMPT_RECORD_PLATFORM_SPEC_MAX + 1];
  CHAR8                                 TnSpec[CAPSULE_ATTEMPT_RECORD_PLATFORM_SPEC_MAX + 1];
} CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE;

VOID
CapsuleAttemptRecordStateBegin (
  OUT CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State,
  IN  CAPSULE_ATTEMPT_RECORD_SOURCE          Source,
  IN  UINT64                                 AttemptId,
  IN  CONST EFI_TIME                         *Timestamp OPTIONAL,
  IN  UINT64                                 StartCounter
  );

BOOLEAN
CapsuleAttemptRecordStateMatch (
  IN  CONST CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State,
  IN  CAPSULE_ATTEMPT_RECORD_SOURCE                Source,
  OUT UINT64                                       *AttemptId
  );

VOID
CapsuleAttemptRecordStateConsume (
  IN OUT CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State
  );

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
  IN     CONST UINT32                           *Completion OPTIONAL
  );

CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS *
CapsuleAttemptRecordStateGetDiagnostics (
  IN CONST CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State
  );

#endif
