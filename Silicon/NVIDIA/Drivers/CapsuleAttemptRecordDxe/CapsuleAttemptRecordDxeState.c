/** @file
  Capsule attempt record DXE protocol state implementation.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CapsuleAttemptRecordDxeState.h"

VOID
CapsuleAttemptRecordStateBegin (
  OUT CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State,
  IN  CAPSULE_ATTEMPT_RECORD_SOURCE          Source,
  IN  UINT64                                 AttemptId
  )
{
  State->AttemptActive = TRUE;
  State->AttemptId     = AttemptId;
  State->AttemptSource = Source;
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
  State->AttemptActive = FALSE;
  State->AttemptId     = 0;
  State->AttemptSource = CapsuleAttemptRecordSourceUnknown;
}
