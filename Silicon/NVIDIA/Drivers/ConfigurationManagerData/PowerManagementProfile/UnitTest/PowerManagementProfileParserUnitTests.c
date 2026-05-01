/** @file

  Host-based unit tests for PowerManagementProfileParser().

  Tests cover the directly reachable branches of the top-level
  PowerManagementProfileParser function:

    1. NULL ParserHandle triggers ASSERT.
    2. Valid ParserHandle -> NvAddSingleCmObj is called and its status
       is propagated.

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

#include <Library/NvCmObjectDescUtility.h>

#include "../PowerManagementProfileParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "PowerManagementProfileParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static EFI_STATUS  mAddSingleObjStatus = EFI_SUCCESS;

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
TestPowerMgmtProfile_NullHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_EXPECT_ASSERT_FAILURE (PowerManagementProfileParser (NULL, 0), NULL);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestPowerMgmtProfile_ValidHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mAddSingleObjStatus = EFI_SUCCESS;
  UT_ASSERT_STATUS_EQUAL (
    PowerManagementProfileParser (&mHandleSentinel, 0),
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
             "PowerManagementProfileParser",
             "PowerMgmtProfile.Parser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "NULL handle triggers ASSERT", "NullHandle", TestPowerMgmtProfile_NullHandle, NULL, NULL, NULL);
  AddTestCase (Suite, "Valid handle returns EFI_SUCCESS", "ValidHandle", TestPowerMgmtProfile_ValidHandle, NULL, NULL, NULL);

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
