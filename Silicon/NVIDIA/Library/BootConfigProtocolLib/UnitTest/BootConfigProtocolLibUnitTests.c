/** @file

  Host-based unit tests for the PMC RST_SOURCE -> canonical boot reason decode
  (T23xPmcSourceToReason). Exhaustively covers all 64 RST_SOURCE values, including
  the bucketed reasons (watchdog / shutdown,thermal / shutdown,undervoltage)
  that cannot be safely induced on real silicon.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UnitTestLib.h>

#include "../T23xPmcBootReason.h"

#define TEST_CASE(Name)  #Name, "", Name
#define UNIT_TEST_APP_NAME     "BootConfigProtocolLib Unit Test Application"
#define UNIT_TEST_APP_VERSION  "1.0"

//
// Independent oracle: the expected canonical reason for every RST_SOURCE value
// 0..63, written out by hand from the t23x PMC_IMPL_RST_STATUS_0 spec (NOT
// derived from the switch under test, so a copy/paste error in either the code
// or this table is caught).
//
STATIC CONST CHAR8  *mExpectedReason[64] = {
  /* 0  */ "cold",                  // SYS_RESET_N
  /* 1  */ "watchdog",
  /* 2  */ "reboot",                // unassigned -> default
  /* 3  */ "watchdog",
  /* 4  */ "watchdog",
  /* 5  */ "watchdog",
  /* 6  */ "reboot",           // unassigned -> default
  /* 7  */ "reboot",           // unassigned -> default
  /* 8  */ "shutdown,thermal", // SENSOR
  /* 9  */ "shutdown,thermal", // AOTAG
  /* 10 */ "shutdown,thermal", // VFSENSOR
  /* 11 */ "warm",             // MAINSWRST
  /* 12 */ "warm",             // SC7
  /* 13 */ "reboot",           // HSM
  /* 14 */ "warm",             // CSITE
  /* 15 */ "watchdog",
  /* 16 */ "watchdog",
  /* 17 */ "watchdog",
  /* 18 */ "reboot",
  /* 19 */ "warm",               // BPMPBOOT
  /* 20 */ "reboot",             // FUSECRC
  /* 21 */ "watchdog",
  /* 22 */ "watchdog",
  /* 23 */ "warm",                  // PSC_SW
  /* 24 */ "warm",                  // CSITE_SW
  /* 25 */ "reboot",                // unassigned -> default
  /* 26 */ "reboot",                // unassigned -> default
  /* 27 */ "shutdown,undervoltage", // VREFRO/POD
  /* 28 */ "reboot",                // unassigned -> default
  /* 29 */ "reboot",                // unassigned -> default
  /* 30 */ "watchdog",
  /* 31 */ "shutdown,thermal",      // FSI_THERM
  /* 32 */ "watchdog",
  /* 33 */ "watchdog",
  /* 34 */ "watchdog",
  /* 35 */ "watchdog",
  /* 36 */ "reboot",
  /* 37 */ "shutdown,undervoltage", // VMON
  /* 38 */ "watchdog",
  /* 39 */ "watchdog",
  /* 40 */ "watchdog",
  /* 41 */ "watchdog",
  /* 42 */ "watchdog",
  /* 43 */ "watchdog",
  /* 44 */ "watchdog",
  /* 45 */ "reboot",
  /* 46 */ "reboot",            // SCPM_RTC_XTAL (bug 5166812)
  /* 47 */ "reboot",
  /* 48 */ "reboot",
  /* 49 */ "reboot",            // FMON_32K (bug 5166812)
  /* 50 */ "reboot",
  /* 51 */ "shutdown,undervoltage",
  /* 52 */ "shutdown,undervoltage",
  /* 53 */ "shutdown,undervoltage",
  /* 54 */ "shutdown,undervoltage",
  /* 55 */ "shutdown,undervoltage",
  /* 56 */ "shutdown,undervoltage",
  /* 57 */ "reboot",                // unassigned -> default
  /* 58 */ "shutdown,undervoltage",
  /* 59 */ "shutdown,undervoltage",
  /* 60 */ "shutdown,undervoltage",
  /* 61 */ "shutdown,undervoltage",
  /* 62 */ "shutdown,undervoltage",
  /* 63 */ "shutdown,undervoltage",
};

/**
  Every RST_SOURCE value 0..63 decodes to the expected canonical reason.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
AllSourcesDecodeTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN        Source;
  CONST CHAR8  *Actual;

  for (Source = 0; Source < 64; Source++) {
    Actual = T23xPmcSourceToReason ((UINT8)Source);
    UT_ASSERT_TRUE (Actual != NULL);
    UT_LOG_INFO ("source %u -> \"%a\" (expected \"%a\")\n", (UINT32)Source, Actual, mExpectedReason[Source]);
    UT_ASSERT_EQUAL (AsciiStrCmp (Actual, mExpectedReason[Source]), 0);
  }

  return UNIT_TEST_PASSED;
}

/**
  RST_SOURCE is a 6-bit field; the caller masks to 0..63, but the decode must
  still be total for any UINT8 - out-of-range values fall through to "reboot".
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
OutOfRangeDefaultsToRebootTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT16  Source;

  for (Source = 64; Source <= 255; Source++) {
    UT_ASSERT_EQUAL (AsciiStrCmp (T23xPmcSourceToReason ((UINT8)Source), "reboot"), 0);
  }

  return UNIT_TEST_PASSED;
}

STATIC
EFI_STATUS
EFIAPI
UnitTestingEntry (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      TestSuite;

  Framework = NULL;
  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  Status = InitUnitTestFramework (
             &Framework,
             UNIT_TEST_APP_NAME,
             gEfiCallerBaseName,
             UNIT_TEST_APP_VERSION
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "InitUnitTestFramework failed: %r\n", Status));
    goto EXIT;
  }

  Status = CreateUnitTestSuite (
             &TestSuite,
             Framework,
             "T23xPmcSourceToReason",
             "",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CreateUnitTestSuite failed: %r\n", Status));
    goto EXIT;
  }

  AddTestCase (TestSuite, TEST_CASE (AllSourcesDecodeTest), NULL, NULL, NULL);
  AddTestCase (TestSuite, TEST_CASE (OutOfRangeDefaultsToRebootTest), NULL, NULL, NULL);

  Status = RunAllTestSuites (Framework);

EXIT:
  if (Framework) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

/**
  Standard UEFI entry point for target-based unit test execution.
**/
EFI_STATUS
EFIAPI
BootConfigUnitTestAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return UnitTestingEntry ();
}

/**
  Standard POSIX C entry point for host-based unit test execution.
**/
int
main (
  int   argc,
  char  *argv[]
  )
{
  return UnitTestingEntry ();
}
