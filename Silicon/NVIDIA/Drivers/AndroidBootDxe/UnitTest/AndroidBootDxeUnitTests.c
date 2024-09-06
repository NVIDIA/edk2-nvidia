/** @file
  Unit tests of AndroidBootDxe Driver.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <AndroidBootDxeUnitTestPrivate.h>

#define UNIT_TEST_APP_NAME     "AndroidBootDxe Unit Test Application"
#define UNIT_TEST_APP_VERSION  "0.2"

STATIC EFI_BOOT_SERVICES  mBS = { 0 };

/**
  Initialze the unit test framework, suite, and unit tests for the AndroidBoot
  driver and run the unit tests.

  @retval  EFI_SUCCESS           All test cases were dispatched.
**/
STATIC
EFI_STATUS
EFIAPI
UnitTestingEntry (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Fw;
  UNIT_TEST_SUITE_HANDLE      BootImgHeaderTestSuite;
  UNIT_TEST_SUITE_HANDLE      UpdateKernelArgsTestSuite;

  Fw = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  // Configure a stubby gBS.
  gBS          = &mBS;
  gBS->CopyMem = (EFI_COPY_MEM)CopyMem;

  // Start setting up the test framework for running the tests.
  Status = InitUnitTestFramework (
             &Fw,
             UNIT_TEST_APP_NAME,
             gEfiCallerBaseName,
             UNIT_TEST_APP_VERSION
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in InitUnitTestFramework. Status = %r\n",
       Status)
      );
    goto EXIT;
  }

  // Populate the Boot Image Header Test Suite.
  Status = CreateUnitTestSuite (
             &BootImgHeaderTestSuite,
             Fw,
             "Boot Image Header Tests",
             "AndroidBootDxe.BootImgHeaderTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for BootImageTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  BootImgHeader_PopulateSuite (BootImgHeaderTestSuite);

  // Populate the UpdateKernelArgs Test Suite.
  Status = CreateUnitTestSuite (
             &UpdateKernelArgsTestSuite,
             Fw,
             "Update Kernel Args Tests",
             "AndroidBootDxe.UpdateKernelArgsTestSuite",
             Suite_UpdateKernelArgs_Setup,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for UpdateKernelArgsTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  UpdateKernelArgs_PopulateSuite (UpdateKernelArgsTestSuite);

  // Execute the tests.
  Status = RunAllTestSuites (Fw);

EXIT:
  return Status;
}

/**
  Standard UEFI entry point for target based unit test execution from UEFI
  Shell.
**/
EFI_STATUS
EFIAPI
BaseLibUnitTestAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "Called BaseLibUnitTestAppEntry\n"));
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
  DEBUG ((DEBUG_INFO, "Called main\n"));
  return UnitTestingEntry ();
}
