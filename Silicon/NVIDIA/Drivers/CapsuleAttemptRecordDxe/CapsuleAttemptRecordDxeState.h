/** @file
  Capsule attempt record DXE protocol state definitions.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CAPSULE_ATTEMPT_RECORD_DXE_STATE_H__
#define __CAPSULE_ATTEMPT_RECORD_DXE_STATE_H__

#include <Uefi.h>
#include <Protocol/CapsuleAttemptRecordProtocol.h>

typedef struct {
  BOOLEAN                          AttemptActive;
  UINT64                           AttemptId;
  CAPSULE_ATTEMPT_RECORD_SOURCE    AttemptSource;
} CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE;

VOID
CapsuleAttemptRecordStateBegin (
  OUT CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  *State,
  IN  CAPSULE_ATTEMPT_RECORD_SOURCE          Source,
  IN  UINT64                                 AttemptId
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

#endif
