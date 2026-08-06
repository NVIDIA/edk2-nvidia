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

STATIC CONST CHAR16  mFirstFailedImage[]  = { 'A', '_', 'c', 'p', 'u', 0 };
STATIC CONST CHAR16  mSecondFailedImage[] = { 'B', '_', 'c', 'p', 'u', 0 };

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

TEST (CapsuleAttemptRecordDxeStateTest, UpdateWithoutAttemptIsRejected) {
  CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  State = { };

  EXPECT_EQ (
    EFI_NOT_READY,
    CapsuleAttemptRecordStateUpdateDiagnostics (
      &State,
      TRUE,
      mFirstFailedImage,
      EFI_ABORTED,
      1,
      "platform",
      "tnspec",
      nullptr
      )
    );
  EXPECT_EQ (nullptr, CapsuleAttemptRecordStateGetDiagnostics (&State));
}

TEST (CapsuleAttemptRecordDxeStateTest, UpdateReplacesCachedDiagnostics) {
  CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE     State = { };
  CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  *Diagnostics;
  UINT32                                    Completion;

  CapsuleAttemptRecordStateBegin (
    &State,
    CapsuleAttemptRecordSourceAndroidFastboot,
    42
    );

  Completion = 25;
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordStateUpdateDiagnostics (
      &State,
      TRUE,
      mFirstFailedImage,
      EFI_ABORTED,
      1,
      "platform-1",
      "tnspec-1",
      &Completion
      )
    );
  Completion = 75;
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordStateUpdateDiagnostics (
      &State,
      FALSE,
      mSecondFailedImage,
      EFI_DEVICE_ERROR,
      2,
      "platform-2",
      "tnspec-2",
      &Completion
      )
    );

  Diagnostics = CapsuleAttemptRecordStateGetDiagnostics (&State);
  ASSERT_NE (nullptr, Diagnostics);
  EXPECT_EQ (CapsuleAttemptRecordProducerSetImage, Diagnostics->Producer);
  EXPECT_STREQ ("B_cpu", Diagnostics->FailedImage);
  EXPECT_EQ (EFI_DEVICE_ERROR, Diagnostics->RawStatus);
  EXPECT_EQ (2U, Diagnostics->NvidiaStatus);
  EXPECT_STREQ ("platform-2", Diagnostics->PlatformSpec);
  EXPECT_STREQ ("tnspec-2", Diagnostics->TnSpec);
  EXPECT_EQ (75U, Diagnostics->Completion);
  EXPECT_TRUE (Diagnostics->CompletionValid);
}

TEST (CapsuleAttemptRecordDxeStateTest, InvalidUpdatePreservesCachedDiagnostics) {
  CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE     State = { };
  CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  *Diagnostics;
  UINT32                                    InvalidCompletion;

  CapsuleAttemptRecordStateBegin (
    &State,
    CapsuleAttemptRecordSourceAndroidFastboot,
    42
    );
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordStateUpdateDiagnostics (
      &State,
      TRUE,
      mFirstFailedImage,
      EFI_ABORTED,
      1,
      "platform",
      "tnspec",
      nullptr
      )
    );

  InvalidCompletion = 101;
  EXPECT_EQ (
    EFI_INVALID_PARAMETER,
    CapsuleAttemptRecordStateUpdateDiagnostics (
      &State,
      FALSE,
      mSecondFailedImage,
      EFI_DEVICE_ERROR,
      2,
      nullptr,
      nullptr,
      &InvalidCompletion
      )
    );

  Diagnostics = CapsuleAttemptRecordStateGetDiagnostics (&State);
  ASSERT_NE (nullptr, Diagnostics);
  EXPECT_EQ (CapsuleAttemptRecordProducerCheckImage, Diagnostics->Producer);
  EXPECT_STREQ ("A_cpu", Diagnostics->FailedImage);
  EXPECT_EQ (EFI_ABORTED, Diagnostics->RawStatus);
  EXPECT_EQ (1U, Diagnostics->NvidiaStatus);
}

TEST (CapsuleAttemptRecordDxeStateTest, BeginClearsCachedDiagnostics) {
  CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  State = { };

  CapsuleAttemptRecordStateBegin (
    &State,
    CapsuleAttemptRecordSourceAndroidFastboot,
    42
    );
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordStateUpdateDiagnostics (
      &State,
      TRUE,
      nullptr,
      EFI_ABORTED,
      1,
      nullptr,
      nullptr,
      nullptr
      )
    );
  ASSERT_NE (nullptr, CapsuleAttemptRecordStateGetDiagnostics (&State));

  CapsuleAttemptRecordStateBegin (
    &State,
    CapsuleAttemptRecordSourceCapsuleOnDisk,
    43
    );

  EXPECT_EQ (nullptr, CapsuleAttemptRecordStateGetDiagnostics (&State));
}

TEST (CapsuleAttemptRecordDxeStateTest, ConsumeClearsCachedDiagnostics) {
  CAPSULE_ATTEMPT_RECORD_PROTOCOL_STATE  State = { };

  CapsuleAttemptRecordStateBegin (
    &State,
    CapsuleAttemptRecordSourceAndroidFastboot,
    42
    );
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordStateUpdateDiagnostics (
      &State,
      TRUE,
      nullptr,
      EFI_ABORTED,
      1,
      nullptr,
      nullptr,
      nullptr
      )
    );

  CapsuleAttemptRecordStateConsume (&State);

  EXPECT_EQ (nullptr, CapsuleAttemptRecordStateGetDiagnostics (&State));
}
