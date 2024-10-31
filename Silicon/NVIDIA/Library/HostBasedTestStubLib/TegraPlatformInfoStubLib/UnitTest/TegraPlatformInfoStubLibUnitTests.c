/** @file

  Tegra Platform Info stub library unit tests

  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#include <HostBasedTestStubLib/TegraPlatformInfoStubLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>

#define TEST_CASE(Name)  #Name, "", Name
#define UNIT_TEST_APP_NAME     "TegraPlatformInfoStubLib Unit Test Application"
#define UNIT_TEST_APP_VERSION  "0.0"

STATIC
UNIT_TEST_STATUS
EFIAPI
TegraGetChipIDTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT32  ChipIdArray[] = {
    T234_CHIP_ID
  };
  UINTN   Index;

  for (Index = 0; Index < (sizeof (ChipIdArray) / sizeof (ChipIdArray[0])); Index++) {
    UINT32  ExpectedChipId = ChipIdArray[Index];
    UINT32  ChipId;

    MockTegraGetChipID (ExpectedChipId);
    ChipId = TegraGetChipID ();
    UT_ASSERT_EQUAL (ChipId, ExpectedChipId);
  }

  return UNIT_TEST_PASSED;
}

/**
  Initialize the unit test framework and run the unit tests

  @retval  EFI_SUCCESS           All test cases were dispatched.
  @retval  EFI_OUT_OF_RESOURCES  There are not enough resources available to
                                 initialize the unit tests.
**/
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
             "TegraPlatformInfoStubLib",
             "",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CreateUnitTestSuite failed: %r\n", Status));
    goto EXIT;
  }

  AddTestCase (
    TestSuite,
    TEST_CASE (TegraGetChipIDTest),
    NULL,
    NULL,
    NULL
    );

  Status = RunAllTestSuites (Framework);

EXIT:
  if (Framework) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

/**
  Standard UEFI entry point for target based
  unit test execution from UEFI Shell.
**/
EFI_STATUS
EFIAPI
BaseLibUnitTestAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return UnitTestingEntry ();
}

/**
  Standard POSIX C entry point for host based unit test execution.
**/
int
main (
  int   argc,
  char  *argv[]
  )
{
  return UnitTestingEntry ();
}
