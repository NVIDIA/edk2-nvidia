/** @file
  Host tests for CapsuleAttemptRecordDxe protocol state.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/GoogleTestLib.h>

extern "C" {
  #include <Uefi.h>
  #include "../CapsuleAttemptRecordDxeState.h"
}

TEST (CapsuleAttemptRecordDxeStateTest, BeginStoresAttempt) {
  CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  State = { };

  CapsuleAttemptRecordStateBegin (
    &State,
    CapsuleAttemptRecordSourceAndroidFastboot,
    42
    );

  EXPECT_TRUE (State.AttemptActive);
  EXPECT_EQ (42U, State.AttemptId);
  EXPECT_EQ (CapsuleAttemptRecordSourceAndroidFastboot, State.AttemptSource);
}

TEST (CapsuleAttemptRecordDxeStateTest, MatchReturnsOnlyMatchingAttempt) {
  CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  State = { };
  UINT64                                 AttemptId;

  CapsuleAttemptRecordStateBegin (
    &State,
    CapsuleAttemptRecordSourceAndroidFastboot,
    42
    );

  EXPECT_FALSE (
    CapsuleAttemptRecordStateMatch (
      &State,
      CapsuleAttemptRecordSourceCapsuleOnDisk,
      &AttemptId
      )
    );
  EXPECT_EQ (0U, AttemptId);
  EXPECT_TRUE (
    CapsuleAttemptRecordStateMatch (
      &State,
      CapsuleAttemptRecordSourceAndroidFastboot,
      &AttemptId
      )
    );
  EXPECT_EQ (42U, AttemptId);
}

TEST (CapsuleAttemptRecordDxeStateTest, ConsumeClearsAttempt) {
  CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  State = { };
  UINT64                                 AttemptId;

  CapsuleAttemptRecordStateBegin (
    &State,
    CapsuleAttemptRecordSourceAndroidFastboot,
    42
    );
  CapsuleAttemptRecordStateConsume (&State);

  EXPECT_FALSE (State.AttemptActive);
  EXPECT_EQ (0U, State.AttemptId);
  EXPECT_EQ (CapsuleAttemptRecordSourceUnknown, State.AttemptSource);
  EXPECT_FALSE (
    CapsuleAttemptRecordStateMatch (
      &State,
      CapsuleAttemptRecordSourceAndroidFastboot,
      &AttemptId
      )
    );
}
