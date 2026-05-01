/** @file

  Host-based unit tests for GenericTimerParser().

  Tests cover the directly reachable branches of the top-level
  GenericTimerParser function:

    1. NULL ParserHandle triggers ASSERT.
    2. Unsupported ChipID -> parser returns EFI_UNSUPPORTED.
    3. Supported ChipID (T234) -> NvAddSingleCmObj is called and its
       status is propagated.

  All external library functions are stubbed inline.

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Library/DebugLib.h>
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

#include <Library/TegraPlatformInfoLib.h>
#include <Library/NvCmObjectDescUtility.h>

#include "../GenericTimerParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "GenericTimerParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static UINT32      mChipID             = 0xDEADBEEF;
static EFI_STATUS  mAddSingleObjStatus = EFI_SUCCESS;

// ---------------------------------------------------------------------------
// TegraPlatformInfoLib stub.
// ---------------------------------------------------------------------------

UINT32
TegraGetChipID (
  VOID
  )
{
  return mChipID;
}

// ---------------------------------------------------------------------------
// NvCmObjectDescUtility stub.
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
NvAddSingleCmObj (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        CM_OBJECT_ID           ObjectId,
  IN        VOID                   *Data,
  IN        UINT32                 DataSize,
  OUT       CM_OBJECT_TOKEN        *Token OPTIONAL
  )
{
  return mAddSingleObjStatus;
}

// ---------------------------------------------------------------------------
// ConfigurationManagerDataRepoLib stub.
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
ConfigManagerDataRepoRegisterParser (
  PARSER_INFO  *Parser,
  CHAR8        *ParserSkipString
  )
{
  return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Dummy parser handle (HW_INFO_PARSER_HANDLE is VOID *).
// ---------------------------------------------------------------------------

static UINTN  mHandleSentinel = 0xA1C9E53F;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

UNIT_TEST_STATUS
EFIAPI
TestGenericTimer_NullHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_EXPECT_ASSERT_FAILURE (GenericTimerParser (NULL, 0), NULL);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestGenericTimer_UnsupportedChipID (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mChipID = 0xDEADBEEF;
  UT_ASSERT_STATUS_EQUAL (
    GenericTimerParser (&mHandleSentinel, 0),
    EFI_UNSUPPORTED
    );
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestGenericTimer_T234ChipID (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mChipID             = T234_CHIP_ID;
  mAddSingleObjStatus = EFI_SUCCESS;
  UT_ASSERT_STATUS_EQUAL (
    GenericTimerParser (&mHandleSentinel, 0),
    EFI_SUCCESS
    );
  return UNIT_TEST_PASSED;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

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

  Status = CreateUnitTestSuite (
             &Suite,
             Framework,
             "GenericTimerParser",
             "GenericTimer.Parser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "NULL handle triggers ASSERT", "NullHandle", TestGenericTimer_NullHandle, NULL, NULL, NULL);
  AddTestCase (Suite, "Unsupported ChipID returns EFI_UNSUPPORTED", "UnsupportedChipID", TestGenericTimer_UnsupportedChipID, NULL, NULL, NULL);
  AddTestCase (Suite, "T234 ChipID returns EFI_SUCCESS", "T234ChipID", TestGenericTimer_T234ChipID, NULL, NULL, NULL);

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
