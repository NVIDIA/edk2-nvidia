/** @file

  Host-based unit tests for BootChainDxe.

  Tests cover BootChainExecuteUpdate(), BootChainFinishUpdate(),
  BootChainCheckAndCancelUpdate(), and BrBctUpdateNeeded() -- the
  complete A/B boot-chain state machine.

  Stubs provided here (not available in existing stub libs):
    - ResetCold()            : records that a reset was requested
    - BootChainIsFailed()    : configurable via mBootChainIsFailedReturn
    - ClearUpdateBrBctFlag() : records that the flag was cleared

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>
#include <HostBasedTestStubLib/UefiRuntimeServicesTableStubLib.h>

#include "../BootChainDxePrivate.h"
EFI_STATUS
EFIAPI
BootChainExecuteUpdate (
  IN  NVIDIA_BOOT_CHAIN_PROTOCOL  *This
  );

EFI_STATUS
EFIAPI
BootChainCheckAndCancelUpdate (
  IN  NVIDIA_BOOT_CHAIN_PROTOCOL  *This,
  OUT BOOLEAN                     *Canceled
  );

#define UNIT_TEST_NAME     "BootChainDxe Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Globals declared in BootChainDxe.c (accessed directly for test setup)
// ---------------------------------------------------------------------------
extern UINT32                         mBootChain;
extern BOOLEAN                        mUpdateBrBctFlag;
extern NVIDIA_BR_BCT_UPDATE_PROTOCOL  *mBrBctUpdateProtocol;

// ---------------------------------------------------------------------------
// Test state
// ---------------------------------------------------------------------------
static BOOLEAN     mResetCalled             = FALSE;
static UINT32      mResetCalledBootChain    = MAX_UINT32;
static BOOLEAN     mBootChainIsFailedReturn = FALSE;
static BOOLEAN     mClearBrBctFlagCalled    = FALSE;
static BOOLEAN     mUpdateFwChainCalled     = FALSE;
static UINTN       mUpdateFwChainArg        = MAX_UINT32;
static EFI_STATUS  mUpdateFwChainStatus     = EFI_SUCCESS;

// ---------------------------------------------------------------------------
// Stubs for functions not covered by existing stub libraries
// ---------------------------------------------------------------------------

// ResetSystemLib: ResetCold() -- record that reset was triggered, then return
// so the state machine can be observed post-"reset" in unit tests.
VOID
EFIAPI
ResetCold (
  VOID
  )
{
  mResetCalled = TRUE;
}

// PlatformResourceLib: BootChainIsFailed() -- configurable per test
BOOLEAN
EFIAPI
BootChainIsFailed (
  IN UINT32  BootChain
  )
{
  return mBootChainIsFailedReturn;
}

// PlatformResourceLib: ClearUpdateBrBctFlag() -- record that it was called
VOID
EFIAPI
ClearUpdateBrBctFlag (
  VOID
  )
{
  mClearBrBctFlagCalled = TRUE;
}

// UefiLib: Print() -- sink console output in tests
UINTN
EFIAPI
Print (
  IN CONST CHAR16  *Format,
  ...
  )
{
  return 0;
}

// HobLib: GetFirstGuidHob() -- no HOB data in host tests
VOID *
EFIAPI
GetFirstGuidHob (
  IN CONST EFI_GUID  *Guid
  )
{
  return NULL;
}

// UefiLib: EfiCreateProtocolNotifyEvent() -- not exercised in tests
EFI_EVENT
EFIAPI
EfiCreateProtocolNotifyEvent (
  IN  EFI_GUID          *ProtocolGuid,
  IN  EFI_TPL           NotifyTpl,
  IN  EFI_EVENT_NOTIFY  NotifyFunction,
  IN  VOID              *NotifyContext   OPTIONAL,
  OUT VOID              **Registration
  )
{
  return NULL;
}

// BootChainInfoLib: GetBootChainPartitionName() -- not exercised in tests
EFI_STATUS
EFIAPI
GetBootChainPartitionName (
  IN  CONST CHAR16  *BasePartitionName,
  IN  UINTN         BootChain,
  OUT CHAR16        *BootChainPartitionName
  )
{
  return EFI_UNSUPPORTED;
}

// ---------------------------------------------------------------------------
// Mock BrBctUpdate protocol
// ---------------------------------------------------------------------------

static EFI_STATUS
EFIAPI
MockUpdateFwChain (
  IN  CONST NVIDIA_BR_BCT_UPDATE_PROTOCOL  *This,
  IN  UINTN                                NewFwChain
  )
{
  mUpdateFwChainCalled = TRUE;
  mUpdateFwChainArg    = NewFwChain;
  return mUpdateFwChainStatus;
}

static NVIDIA_BR_BCT_UPDATE_PROTOCOL  mMockBrBctProtocol = {
  .UpdateFwChain = MockUpdateFwChain,
};

// ---------------------------------------------------------------------------
// Helper: read a BC variable from the stub store
// ---------------------------------------------------------------------------

static EFI_STATUS
ReadBCVar (
  IN  BC_VARIABLE_INDEX  Idx,
  OUT UINT32             *Value
  )
{
  extern BC_VARIABLE  mBCVariables[];
  BC_VARIABLE         *V   = &mBCVariables[Idx];
  UINTN               Size = sizeof (UINT32);

  return gRT->GetVariable (V->Name, V->Guid, NULL, &Size, Value);
}

// ---------------------------------------------------------------------------
// Helper: seed a BC variable into the stub store
// ---------------------------------------------------------------------------

static VOID
SeedBCVar (
  IN  BC_VARIABLE_INDEX  Idx,
  IN  UINT32             Value
  )
{
  extern BC_VARIABLE  mBCVariables[];
  BC_VARIABLE         *V = &mBCVariables[Idx];

  gRT->SetVariable (V->Name, V->Guid, V->Attributes, sizeof (UINT32), &Value);
}

// ---------------------------------------------------------------------------
// Helper: assert BC_STATUS in stub store equals expected value
// ---------------------------------------------------------------------------

#define ASSERT_BC_STATUS(Expected) \
  do { \
    UINT32     _val  = MAX_UINT32; \
    EFI_STATUS _st   = ReadBCVar (BC_STATUS, &_val); \
    UT_ASSERT_NOT_EFI_ERROR (_st); \
    UT_ASSERT_EQUAL (_val, (Expected)); \
  } while (0)

// ---------------------------------------------------------------------------
// Suite setup / teardown
// ---------------------------------------------------------------------------

static UNIT_TEST_STATUS
EFIAPI
TestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UefiRuntimeServicesTableInit (FALSE);

  // Reset all test-state flags
  mResetCalled             = FALSE;
  mResetCalledBootChain    = MAX_UINT32;
  mBootChainIsFailedReturn = FALSE;
  mClearBrBctFlagCalled    = FALSE;
  mUpdateFwChainCalled     = FALSE;
  mUpdateFwChainArg        = MAX_UINT32;
  mUpdateFwChainStatus     = EFI_SUCCESS;

  // Default: booted on chain 0, protocol available, no BR-BCT flag
  mBootChain           = 0;
  mUpdateBrBctFlag     = FALSE;
  mBrBctUpdateProtocol = &mMockBrBctProtocol;

  return UNIT_TEST_PASSED;
}

static VOID
EFIAPI
TestTeardown (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UefiRuntimeServicesTableDeinit (FALSE);
}

// ===========================================================================
// Group 1: BootChainExecuteUpdate -- no pending update
// ===========================================================================

// No BC_NEXT variable present → boot OS directly, nothing written, no reset.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_NoUpdate (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT32      Val;

  Status = BootChainExecuteUpdate (&mProtocol);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  // BC_STATUS must not have been written
  UT_ASSERT_STATUS_EQUAL (ReadBCVar (BC_STATUS, &Val), EFI_NOT_FOUND);
  UT_ASSERT_FALSE (mResetCalled);
  UT_ASSERT_FALSE (mUpdateFwChainCalled);

  return UNIT_TEST_PASSED;
}

// BC_NEXT present, no update needed because it equals the current chain.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_SameChain (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  SeedBCVar (BC_NEXT, 0);   // BCNext == mBootChain (both 0)

  BootChainExecuteUpdate (&mProtocol);

  ASSERT_BC_STATUS (STATUS_ERROR_NO_OPERATION_REQUIRED);
  UT_ASSERT_FALSE (mResetCalled);

  return UNIT_TEST_PASSED;
}

// ===========================================================================
// Group 2: BootChainExecuteUpdate -- new update validation errors
// ===========================================================================

// BCNext is out of range.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_BadChainIndex (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  SeedBCVar (BC_NEXT, BOOT_CHAIN_COUNT);  // invalid: 0 and 1 are valid

  BootChainExecuteUpdate (&mProtocol);

  ASSERT_BC_STATUS (STATUS_ERROR_BAD_BOOT_CHAIN_NEXT);
  UT_ASSERT_FALSE (mResetCalled);

  return UNIT_TEST_PASSED;
}

// EFI BootNext variable exists -- update must be blocked.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_BootNextExists (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT16  BootNextVal = 0;

  SeedBCVar (BC_NEXT, 1);
  gRT->SetVariable (
         L"BootNext",
         &gEfiGlobalVariableGuid,
         EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
         sizeof (BootNextVal),
         &BootNextVal
         );

  BootChainExecuteUpdate (&mProtocol);

  ASSERT_BC_STATUS (STATUS_ERROR_BOOT_NEXT_EXISTS);
  UT_ASSERT_FALSE (mResetCalled);

  return UNIT_TEST_PASSED;
}

// BR-BCT update flag is set -- update must be blocked.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_BrBctFlagSet (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mUpdateBrBctFlag = TRUE;
  SeedBCVar (BC_NEXT, 1);

  BootChainExecuteUpdate (&mProtocol);

  ASSERT_BC_STATUS (STATUS_ERROR_UPDATE_BR_BCT_FLAG_SET);
  UT_ASSERT_FALSE (mResetCalled);

  return UNIT_TEST_PASSED;
}

// Target chain is already marked failed.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_ChainIsFailed (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mBootChainIsFailedReturn = TRUE;
  SeedBCVar (BC_NEXT, 1);

  BootChainExecuteUpdate (&mProtocol);

  ASSERT_BC_STATUS (STATUS_ERROR_BOOT_CHAIN_IS_FAILED);
  UT_ASSERT_FALSE (mResetCalled);

  return UNIT_TEST_PASSED;
}

// Reset count already at the maximum -- update must be blocked.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_MaxResetCount (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  SeedBCVar (BC_NEXT, 1);
  SeedBCVar (BC_RESET_COUNT, BOOT_CHAIN_MAX_RESET_COUNT);

  BootChainExecuteUpdate (&mProtocol);

  ASSERT_BC_STATUS (STATUS_ERROR_MAX_RESET_COUNT);
  UT_ASSERT_FALSE (mResetCalled);

  return UNIT_TEST_PASSED;
}

// UpdateFwChain returns an error -- update aborted.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_UpdateFwChainFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mUpdateFwChainStatus = EFI_DEVICE_ERROR;
  SeedBCVar (BC_NEXT, 1);

  BootChainExecuteUpdate (&mProtocol);

  ASSERT_BC_STATUS (STATUS_ERROR_UPDATING_FW_CHAIN);
  UT_ASSERT_FALSE (mResetCalled);

  return UNIT_TEST_PASSED;
}

// ===========================================================================
// Group 3: BootChainExecuteUpdate -- successful new update initiation
// ===========================================================================

// All preconditions met: sets PREVIOUS, IN_PROGRESS, increments RESET_COUNT,
// calls UpdateFwChain with BCNext, triggers reset.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_InitiatesUpdate (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT32  Val;

  // BootChainReset() → StatusRegReset() → StatusRegInitialize() → TegraGetChipID()
  // on the first call (mStatusRegInitialized=FALSE).  Use will_return_maybe so the
  // mock is silently ignored if a prior test already initialized StatusRegStubLib.
  will_return_maybe (TegraGetChipID, 0);

  SeedBCVar (BC_NEXT, 1);

  BootChainExecuteUpdate (&mProtocol);

  // UpdateFwChain must have been called with chain 1
  UT_ASSERT_TRUE (mUpdateFwChainCalled);
  UT_ASSERT_EQUAL (mUpdateFwChainArg, 1);

  // Reset must have been triggered
  UT_ASSERT_TRUE (mResetCalled);

  // BC_PREVIOUS must have been set to original chain (0)
  // Note: BootChainFinishUpdate deletes PREVIOUS after the "reset", so check
  // that it was set during the update path by inspecting what FinishUpdate saw.
  // Since ResetCold returns in tests, execution falls into FinishUpdate which
  // deletes PREVIOUS -- so verify it's gone (deleted as part of cleanup).
  UT_ASSERT_STATUS_EQUAL (ReadBCVar (BC_PREVIOUS, &Val), EFI_NOT_FOUND);

  // RESET_COUNT must have been incremented to 1 then deleted by FinishUpdate
  UT_ASSERT_STATUS_EQUAL (ReadBCVar (BC_RESET_COUNT, &Val), EFI_NOT_FOUND);

  return UNIT_TEST_PASSED;
}

// RESET_COUNT starts at 1 and is incremented to 2 -- still under max.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_ResetsAccumulate (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  will_return_maybe (TegraGetChipID, 0);

  SeedBCVar (BC_NEXT, 1);
  SeedBCVar (BC_RESET_COUNT, 1);

  BootChainExecuteUpdate (&mProtocol);

  UT_ASSERT_TRUE (mResetCalled);
  UT_ASSERT_TRUE (mUpdateFwChainCalled);

  return UNIT_TEST_PASSED;
}

// ===========================================================================
// Group 4: BootChainExecuteUpdate -- IN_PROGRESS resume paths
// ===========================================================================

// After reboot to new chain: BC_STATUS=IN_PROGRESS, mBootChain==BCNext → SUCCESS.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_InProgress_NewChainBooted (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mBootChain = 1;                        // we are now on chain 1
  SeedBCVar (BC_NEXT, 1);
  SeedBCVar (BC_STATUS, STATUS_IN_PROGRESS);

  BootChainExecuteUpdate (&mProtocol);

  ASSERT_BC_STATUS (STATUS_SUCCESS);

  return UNIT_TEST_PASSED;
}

// After reboot: BC_STATUS=IN_PROGRESS but mBootChain != BCNext → chain boot failed.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_InProgress_ChainBootFailed (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mBootChain = 0;                        // still on old chain despite reboot
  SeedBCVar (BC_NEXT, 1);
  SeedBCVar (BC_STATUS, STATUS_IN_PROGRESS);

  BootChainExecuteUpdate (&mProtocol);

  ASSERT_BC_STATUS (STATUS_ERROR_BOOT_CHAIN_FAILED);

  return UNIT_TEST_PASSED;
}

// ===========================================================================
// Group 5: BootChainFinishUpdate -- cleanup and rollback logic
// ===========================================================================

// STATUS_SUCCESS: PREVIOUS/NEXT/RESET_COUNT cleaned up, no rollback reset.
UNIT_TEST_STATUS
EFIAPI
TestFinishUpdate_Success_CleanupOnly (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT32  Val;

  SeedBCVar (BC_PREVIOUS, 0);
  SeedBCVar (BC_NEXT, 1);
  SeedBCVar (BC_RESET_COUNT, 1);

  mBootChain = 1;
  BootChainFinishUpdate (STATUS_SUCCESS);

  UT_ASSERT_STATUS_EQUAL (ReadBCVar (BC_PREVIOUS, &Val), EFI_NOT_FOUND);
  UT_ASSERT_STATUS_EQUAL (ReadBCVar (BC_NEXT, &Val), EFI_NOT_FOUND);
  UT_ASSERT_STATUS_EQUAL (ReadBCVar (BC_RESET_COUNT, &Val), EFI_NOT_FOUND);
  UT_ASSERT_FALSE (mResetCalled);

  return UNIT_TEST_PASSED;
}

// Error but mBootChain == BCPrevious: no rollback needed, just delete vars.
UNIT_TEST_STATUS
EFIAPI
TestFinishUpdate_Error_NoRollbackNeeded (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT32  Val;

  mBootChain = 0;
  SeedBCVar (BC_PREVIOUS, 0);  // same as current -- no rollback

  BootChainFinishUpdate (STATUS_ERROR_BOOT_CHAIN_FAILED);

  UT_ASSERT_STATUS_EQUAL (ReadBCVar (BC_PREVIOUS, &Val), EFI_NOT_FOUND);
  UT_ASSERT_FALSE (mResetCalled);

  return UNIT_TEST_PASSED;
}

// Error and mBootChain != BCPrevious: must roll back to previous chain.
UNIT_TEST_STATUS
EFIAPI
TestFinishUpdate_Error_RollbackToPrevious (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  will_return_maybe (TegraGetChipID, 0);

  mBootChain = 1;              // ended up on chain 1 unexpectedly
  SeedBCVar (BC_PREVIOUS, 0);  // should return to chain 0

  BootChainFinishUpdate (STATUS_ERROR_BOOT_CHAIN_FAILED);

  UT_ASSERT_TRUE (mUpdateFwChainCalled);
  UT_ASSERT_EQUAL (mUpdateFwChainArg, 0);  // rolled back to chain 0
  UT_ASSERT_TRUE (mResetCalled);

  return UNIT_TEST_PASSED;
}

// BC_PREVIOUS missing on error path: no rollback (can't determine origin chain).
UNIT_TEST_STATUS
EFIAPI
TestFinishUpdate_Error_PreviousMissing (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mBootChain = 1;
  // BC_PREVIOUS not seeded -- simulates variable store corruption

  BootChainFinishUpdate (STATUS_ERROR_UPDATING_FW_CHAIN);

  UT_ASSERT_FALSE (mResetCalled);
  UT_ASSERT_FALSE (mUpdateFwChainCalled);

  return UNIT_TEST_PASSED;
}

// ===========================================================================
// Group 6: BootChainCheckAndCancelUpdate
// ===========================================================================

// Nothing pending: Canceled must be FALSE.
UNIT_TEST_STATUS
EFIAPI
TestCheckAndCancel_NothingToCancel (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BOOLEAN  Canceled = TRUE;  // pre-set to non-FALSE to verify it's cleared

  BootChainCheckAndCancelUpdate (&mProtocol, &Canceled);

  UT_ASSERT_FALSE (Canceled);

  return UNIT_TEST_PASSED;
}

// BC_NEXT present: Canceled=TRUE, BC_NEXT deleted, STATUS_ERROR_CANCELED set.
UNIT_TEST_STATUS
EFIAPI
TestCheckAndCancel_CancelNext (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BOOLEAN  Canceled = FALSE;
  UINT32   Val;

  SeedBCVar (BC_NEXT, 1);

  BootChainCheckAndCancelUpdate (&mProtocol, &Canceled);

  UT_ASSERT_TRUE (Canceled);
  UT_ASSERT_STATUS_EQUAL (ReadBCVar (BC_NEXT, &Val), EFI_NOT_FOUND);
  ASSERT_BC_STATUS (STATUS_ERROR_CANCELED_FOR_FMP_CONFLICT);

  return UNIT_TEST_PASSED;
}

// BC_STATUS present (no BC_NEXT): Canceled=TRUE, STATUS_ERROR_CANCELED set.
UNIT_TEST_STATUS
EFIAPI
TestCheckAndCancel_CancelStatus (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BOOLEAN  Canceled = FALSE;

  SeedBCVar (BC_STATUS, STATUS_IN_PROGRESS);

  BootChainCheckAndCancelUpdate (&mProtocol, &Canceled);

  UT_ASSERT_TRUE (Canceled);
  ASSERT_BC_STATUS (STATUS_ERROR_CANCELED_FOR_FMP_CONFLICT);

  return UNIT_TEST_PASSED;
}

// Both BC_NEXT and BC_STATUS present: Canceled=TRUE.
UNIT_TEST_STATUS
EFIAPI
TestCheckAndCancel_CancelBoth (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BOOLEAN  Canceled = FALSE;
  UINT32   Val;

  SeedBCVar (BC_NEXT, 1);
  SeedBCVar (BC_STATUS, STATUS_IN_PROGRESS);

  BootChainCheckAndCancelUpdate (&mProtocol, &Canceled);

  UT_ASSERT_TRUE (Canceled);
  UT_ASSERT_STATUS_EQUAL (ReadBCVar (BC_NEXT, &Val), EFI_NOT_FOUND);

  return UNIT_TEST_PASSED;
}

// NULL Canceled pointer: must return EFI_INVALID_PARAMETER.
UNIT_TEST_STATUS
EFIAPI
TestCheckAndCancel_NullParam (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  Status = BootChainCheckAndCancelUpdate (&mProtocol, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

// ===========================================================================
// Group 7: BrBctUpdateNeeded
// ===========================================================================

// Flag TRUE, var = 1 → TRUE.
UNIT_TEST_STATUS
EFIAPI
TestBrBctUpdateNeeded_True (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mUpdateBrBctFlag = TRUE;
  SeedBCVar (AUTO_UPDATE_BR_BCT, 1);

  UT_ASSERT_TRUE (BrBctUpdateNeeded ());

  return UNIT_TEST_PASSED;
}

// Flag FALSE → FALSE regardless of variable value.
UNIT_TEST_STATUS
EFIAPI
TestBrBctUpdateNeeded_FlagFalse (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mUpdateBrBctFlag = FALSE;
  SeedBCVar (AUTO_UPDATE_BR_BCT, 1);

  UT_ASSERT_FALSE (BrBctUpdateNeeded ());

  return UNIT_TEST_PASSED;
}

// Flag TRUE, var = 0 → FALSE.
UNIT_TEST_STATUS
EFIAPI
TestBrBctUpdateNeeded_VarZero (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mUpdateBrBctFlag = TRUE;
  SeedBCVar (AUTO_UPDATE_BR_BCT, 0);

  UT_ASSERT_FALSE (BrBctUpdateNeeded ());

  return UNIT_TEST_PASSED;
}

// Flag TRUE, variable not present → FALSE.
UNIT_TEST_STATUS
EFIAPI
TestBrBctUpdateNeeded_VarMissing (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mUpdateBrBctFlag = TRUE;
  // AUTO_UPDATE_BR_BCT not seeded

  UT_ASSERT_FALSE (BrBctUpdateNeeded ());

  return UNIT_TEST_PASSED;
}

// ===========================================================================
// Group 8: Edge cases
// ===========================================================================

// Invalid 'This' pointer: must return EFI_INVALID_PARAMETER.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_InvalidThis (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  Status = BootChainExecuteUpdate (NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

// BrBctUpdateProtocol NULL: ExecuteUpdate must return EFI_NOT_READY.
UNIT_TEST_STATUS
EFIAPI
TestExecuteUpdate_NoProtocol (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  mBrBctUpdateProtocol = NULL;
  SeedBCVar (BC_NEXT, 1);

  Status = BootChainExecuteUpdate (&mProtocol);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_READY);

  return UNIT_TEST_PASSED;
}

// ===========================================================================
// Entry point
// ===========================================================================

EFI_STATUS
EFIAPI
UefiTestMain (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      Suite;

  Framework = NULL;
  Status    = InitUnitTestFramework (
                &Framework,
                UNIT_TEST_NAME,
                gEfiCallerBaseName,
                UNIT_TEST_VERSION
                );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // --- ExecuteUpdate: no-update and validation errors ---
  Status = CreateUnitTestSuite (
             &Suite,
             Framework,
             "ExecuteUpdate",
             "BootChain.ExecuteUpdate",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "No update requested", "NoUpdate", TestExecuteUpdate_NoUpdate, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "BCNext == current chain", "SameChain", TestExecuteUpdate_SameChain, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "BCNext index out of range", "BadChainIndex", TestExecuteUpdate_BadChainIndex, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "EFI BootNext variable present", "BootNextExists", TestExecuteUpdate_BootNextExists, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "BrBct update flag set", "BrBctFlagSet", TestExecuteUpdate_BrBctFlagSet, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Target chain is failed", "ChainIsFailed", TestExecuteUpdate_ChainIsFailed, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Reset count at maximum", "MaxResetCount", TestExecuteUpdate_MaxResetCount, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "UpdateFwChain returns error", "UpdateFwChainFails", TestExecuteUpdate_UpdateFwChainFails, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "New update initiated", "InitiatesUpdate", TestExecuteUpdate_InitiatesUpdate, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Reset count accumulates", "ResetsAccumulate", TestExecuteUpdate_ResetsAccumulate, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "IN_PROGRESS: new chain booted", "InProgress_Success", TestExecuteUpdate_InProgress_NewChainBooted, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "IN_PROGRESS: chain boot failed", "InProgress_Failed", TestExecuteUpdate_InProgress_ChainBootFailed, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Invalid This pointer", "InvalidThis", TestExecuteUpdate_InvalidThis, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "No BrBctUpdate protocol", "NoProtocol", TestExecuteUpdate_NoProtocol, TestSetup, TestTeardown, NULL);

  // --- FinishUpdate ---
  Status = CreateUnitTestSuite (
             &Suite,
             Framework,
             "FinishUpdate",
             "BootChain.FinishUpdate",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "Success: cleanup only, no reset", "Success", TestFinishUpdate_Success_CleanupOnly, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Error: current == previous, no rollback", "ErrorNoRollback", TestFinishUpdate_Error_NoRollbackNeeded, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Error: current != previous, rollback", "ErrorRollback", TestFinishUpdate_Error_RollbackToPrevious, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Error: BC_PREVIOUS missing", "ErrorPrevMissing", TestFinishUpdate_Error_PreviousMissing, TestSetup, TestTeardown, NULL);

  // --- CheckAndCancelUpdate ---
  Status = CreateUnitTestSuite (
             &Suite,
             Framework,
             "CheckAndCancelUpdate",
             "BootChain.CheckAndCancelUpdate",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "Nothing to cancel", "NothingToCancel", TestCheckAndCancel_NothingToCancel, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "BC_NEXT present", "CancelNext", TestCheckAndCancel_CancelNext, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "BC_STATUS present", "CancelStatus", TestCheckAndCancel_CancelStatus, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Both present", "CancelBoth", TestCheckAndCancel_CancelBoth, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "NULL Canceled pointer", "NullParam", TestCheckAndCancel_NullParam, TestSetup, TestTeardown, NULL);

  // --- BrBctUpdateNeeded ---
  Status = CreateUnitTestSuite (
             &Suite,
             Framework,
             "BrBctUpdateNeeded",
             "BootChain.BrBctUpdateNeeded",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "Flag TRUE, var = 1 → TRUE", "True", TestBrBctUpdateNeeded_True, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Flag FALSE → FALSE", "FlagFalse", TestBrBctUpdateNeeded_FlagFalse, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Flag TRUE, var = 0 → FALSE", "VarZero", TestBrBctUpdateNeeded_VarZero, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Flag TRUE, var missing → FALSE", "VarMissing", TestBrBctUpdateNeeded_VarMissing, TestSetup, TestTeardown, NULL);

  Status = RunAllTestSuites (Framework);

Done:
  if (Framework != NULL) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  return UefiTestMain ();
}
