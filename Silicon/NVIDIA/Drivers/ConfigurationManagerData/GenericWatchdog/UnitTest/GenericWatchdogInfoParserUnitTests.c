/** @file

  Host-based unit tests for GenericWatchdogInfoParser().

  Tests cover the directly reachable branches of the top-level
  GenericWatchdogInfoParser function:

    1. NULL ParserHandle triggers ASSERT.
    2. VDK platform -> parser returns EFI_NOT_FOUND immediately
       (watchdog is not available on VDK).
    3. Valid watchdog PCD/resource values -> CM watchdog object is added.
    4. Missing control/refresh/GSIV values -> EFI_NOT_FOUND.
    5. NvAddSingleCmObj failures are propagated.

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
#include <IndustryStandard/Acpi.h>
#include <Library/DebugLib.h>
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

#include <Library/TegraPlatformInfoLib.h>
#include <Library/NvCmObjectDescUtility.h>

#include "../GenericWatchdogInfoParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "GenericWatchdogInfoParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

#define WATCHDOG_TEST_CONTROL_BASE  0x50000000ULL
#define WATCHDOG_TEST_REFRESH_BASE  0x50001000ULL
#define WATCHDOG_TEST_GSIV          42
#define WATCHDOG_TEST_FLAGS         EFI_ACPI_6_6_GTDT_ARM_GENERIC_WATCHDOG_FLAG_TIMER_INTERRUPT_MODE

typedef struct {
  UINT64    ControlBase;
  UINT64    RefreshBase;
  UINT32    Gsiv;
} WATCHDOG_PCD_CASE;

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static TEGRA_PLATFORM_TYPE           mPlatform = TEGRA_PLATFORM_VDK;
static EFI_STATUS                    mAddSingleObjStatus;
static UINTN                         mAddSingleObjCallCount;
static CM_OBJECT_ID                  mLastObjectId;
static UINT32                        mLastDataSize;
static CM_ARM_GENERIC_WATCHDOG_INFO  mLastWatchdog;
static UINT64                        mWatchdogControlBase;
static UINT64                        mWatchdogRefreshBase;
static UINT32                        mWatchdogGsiv;

static
VOID
ResetGenericWatchdogTestState (
  VOID
  )
{
  mPlatform           = TEGRA_PLATFORM_SILICON;
  mAddSingleObjStatus = EFI_SUCCESS;

  mAddSingleObjCallCount = 0;
  mLastObjectId          = 0;
  mLastDataSize          = 0;
  ZeroMem (&mLastWatchdog, sizeof (mLastWatchdog));

  mWatchdogControlBase = WATCHDOG_TEST_CONTROL_BASE;
  mWatchdogRefreshBase = WATCHDOG_TEST_REFRESH_BASE;
  mWatchdogGsiv        = WATCHDOG_TEST_GSIV;
}

UINT64
EFIAPI
GenericWatchdogTestPcdGet64_PcdGenericWatchdogControlBase (
  VOID
  )
{
  return mWatchdogControlBase;
}

UINT64
EFIAPI
GenericWatchdogTestPcdGet64_PcdGenericWatchdogRefreshBase (
  VOID
  )
{
  return mWatchdogRefreshBase;
}

UINT32
EFIAPI
GenericWatchdogTestPcdGet32_PcdGenericWatchdogEl2IntrNum (
  VOID
  )
{
  return mWatchdogGsiv;
}

// ---------------------------------------------------------------------------
// TegraPlatformInfoLib stub.
// ---------------------------------------------------------------------------

TEGRA_PLATFORM_TYPE
TegraGetPlatform (
  VOID
  )
{
  return mPlatform;
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
  mAddSingleObjCallCount++;
  assert_non_null (ParserHandle);
  assert_non_null (Data);

  mLastObjectId = ObjectId;
  mLastDataSize = DataSize;
  if (DataSize == sizeof (mLastWatchdog)) {
    CopyMem (&mLastWatchdog, Data, sizeof (mLastWatchdog));
  }

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
TestGenericWatchdog_NullHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ResetGenericWatchdogTestState ();
  UT_EXPECT_ASSERT_FAILURE (GenericWatchdogInfoParser (NULL, 0), NULL);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestGenericWatchdog_VdkPlatform (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ResetGenericWatchdogTestState ();
  mPlatform = TEGRA_PLATFORM_VDK;
  UT_ASSERT_STATUS_EQUAL (
    GenericWatchdogInfoParser (&mHandleSentinel, 0),
    EFI_NOT_FOUND
    );
  UT_ASSERT_EQUAL (mAddSingleObjCallCount, 0);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestGenericWatchdog_ValidParserSuccess (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ResetGenericWatchdogTestState ();

  UT_ASSERT_STATUS_EQUAL (
    GenericWatchdogInfoParser (&mHandleSentinel, 0),
    EFI_SUCCESS
    );

  UT_ASSERT_EQUAL (mAddSingleObjCallCount, 1);
  UT_ASSERT_EQUAL (mLastObjectId, CREATE_CM_ARM_OBJECT_ID (EArmObjPlatformGenericWatchdogInfo));
  UT_ASSERT_EQUAL (mLastDataSize, sizeof (CM_ARM_GENERIC_WATCHDOG_INFO));
  UT_ASSERT_EQUAL (mLastWatchdog.ControlFrameAddress, WATCHDOG_TEST_CONTROL_BASE);
  UT_ASSERT_EQUAL (mLastWatchdog.RefreshFrameAddress, WATCHDOG_TEST_REFRESH_BASE);
  UT_ASSERT_EQUAL (mLastWatchdog.TimerGSIV, WATCHDOG_TEST_GSIV);
  UT_ASSERT_EQUAL (mLastWatchdog.Flags, WATCHDOG_TEST_FLAGS);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestGenericWatchdog_BadPcdResource (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  WATCHDOG_PCD_CASE  *TestCase;

  TestCase = (WATCHDOG_PCD_CASE *)Context;
  ResetGenericWatchdogTestState ();
  mWatchdogControlBase = TestCase->ControlBase;
  mWatchdogRefreshBase = TestCase->RefreshBase;
  mWatchdogGsiv        = TestCase->Gsiv;

  UT_ASSERT_STATUS_EQUAL (
    GenericWatchdogInfoParser (&mHandleSentinel, 0),
    EFI_NOT_FOUND
    );
  UT_ASSERT_EQUAL (mAddSingleObjCallCount, 0);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestGenericWatchdog_CmAddFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ResetGenericWatchdogTestState ();
  mAddSingleObjStatus = EFI_DEVICE_ERROR;

  UT_ASSERT_STATUS_EQUAL (
    GenericWatchdogInfoParser (&mHandleSentinel, 0),
    EFI_DEVICE_ERROR
    );
  UT_ASSERT_EQUAL (mAddSingleObjCallCount, 1);
  UT_ASSERT_EQUAL (mLastObjectId, CREATE_CM_ARM_OBJECT_ID (EArmObjPlatformGenericWatchdogInfo));

  return UNIT_TEST_PASSED;
}

static WATCHDOG_PCD_CASE  mWatchdogMissingControlBase = {
  0,
  WATCHDOG_TEST_REFRESH_BASE,
  WATCHDOG_TEST_GSIV
};

static WATCHDOG_PCD_CASE  mWatchdogMissingRefreshBase = {
  WATCHDOG_TEST_CONTROL_BASE,
  0,
  WATCHDOG_TEST_GSIV
};

static WATCHDOG_PCD_CASE  mWatchdogMissingGsiv = {
  WATCHDOG_TEST_CONTROL_BASE,
  WATCHDOG_TEST_REFRESH_BASE,
  0
};

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
             "GenericWatchdogInfoParser",
             "GenericWatchdog.InfoParser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "NULL handle triggers ASSERT", "NullHandle", TestGenericWatchdog_NullHandle, NULL, NULL, NULL);
  AddTestCase (Suite, "VDK platform returns EFI_NOT_FOUND", "VdkPlatform", TestGenericWatchdog_VdkPlatform, NULL, NULL, NULL);
  AddTestCase (Suite, "Valid watchdog PCDs add CM object", "ValidParserSuccess", TestGenericWatchdog_ValidParserSuccess, NULL, NULL, NULL);
  AddTestCase (Suite, "Missing control frame PCD returns EFI_NOT_FOUND", "MissingControlBase", TestGenericWatchdog_BadPcdResource, NULL, NULL, &mWatchdogMissingControlBase);
  AddTestCase (Suite, "Missing refresh frame PCD returns EFI_NOT_FOUND", "MissingRefreshBase", TestGenericWatchdog_BadPcdResource, NULL, NULL, &mWatchdogMissingRefreshBase);
  AddTestCase (Suite, "Missing GSIV PCD returns EFI_NOT_FOUND", "MissingGsiv", TestGenericWatchdog_BadPcdResource, NULL, NULL, &mWatchdogMissingGsiv);
  AddTestCase (Suite, "NvAddSingleCmObj failure returns error", "CmAddFails", TestGenericWatchdog_CmAddFails, NULL, NULL, NULL);

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
