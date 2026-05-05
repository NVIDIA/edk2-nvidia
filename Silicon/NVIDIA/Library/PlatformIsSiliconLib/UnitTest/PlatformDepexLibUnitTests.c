/** @file

  Host-based unit tests for platform depex helper constructors.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>

#define UNIT_TEST_NAME     "Platform Depex Lib Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

#include "../PlatformIsSiliconLib.c"
#include "../../PlatformIsPresiliconLib/PlatformIsPresiliconLib.c"
#include "../../PlatformHasConfigManagerLib/PlatformHasConfigManagerLib.c"

STATIC
UNIT_TEST_STATUS
EFIAPI
TestPlatformIsSiliconConstructorReturnsSuccess (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_ASSERT_STATUS_EQUAL (PlatformIsSiliconInitialize (), RETURN_SUCCESS);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestPlatformIsPresiliconConstructorReturnsSuccess (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_ASSERT_STATUS_EQUAL (PlatformIsPresiliconInitialize (), RETURN_SUCCESS);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestPlatformHasConfigManagerConstructorReturnsSuccess (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_ASSERT_NOT_EFI_ERROR (PlatformHasConfigManagerInitialize ());

  return UNIT_TEST_PASSED;
}

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
  Suite     = NULL;

  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CreateUnitTestSuite (&Suite, Framework, "Platform depex constructors", "PlatformDepexLib", NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "Silicon depex constructor returns success", "SiliconConstructor", TestPlatformIsSiliconConstructorReturnsSuccess, NULL, NULL, NULL);
  AddTestCase (Suite, "Presilicon depex constructor returns success", "PresiliconConstructor", TestPlatformIsPresiliconConstructorReturnsSuccess, NULL, NULL, NULL);
  AddTestCase (Suite, "ConfigManager depex constructor returns success", "ConfigManagerConstructor", TestPlatformHasConfigManagerConstructorReturnsSuccess, NULL, NULL, NULL);

  Status = RunAllTestSuites (Framework);

Done:
  if (Framework != NULL) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

int
main (
  void
  )
{
  return (int)UefiTestMain ();
}
