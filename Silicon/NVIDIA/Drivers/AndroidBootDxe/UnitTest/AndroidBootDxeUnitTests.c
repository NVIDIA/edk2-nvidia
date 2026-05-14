/** @file
  Unit tests of AndroidBootDxe Driver.

  SPDX-FileCopyrightText: Copyright (c) 2023-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <AndroidBootDxeUnitTestPrivate.h>
#include <Protocol/DriverHealth.h>

#define UNIT_TEST_APP_NAME     "AndroidBootDxe Unit Test Application"
#define UNIT_TEST_APP_VERSION  "0.2"

STATIC EFI_BOOT_SERVICES         mBS = { 0 };
STATIC EFI_STATUS                mHandleProtocolStatus;
STATIC UINTN                     mHandleProtocolCallCount;
STATIC UINTN                     mSetDriverHealthStatusCallCount;
STATIC UINTN                     mSetDriverHealthStatusFromEfiStatusCallCount;
STATIC EFI_STATUS                mLastDriverHealthEfiStatus;
STATIC EFI_DRIVER_HEALTH_STATUS  mDriverHealthStatus;

#define TEST_CONTROLLER_HANDLE      ((EFI_HANDLE)(UINTN)0xA001)
#define TEST_DRIVER_BINDING_HANDLE  ((EFI_HANDLE)(UINTN)0xA002)

extern EFI_DRIVER_BINDING_PROTOCOL  mAndroidBootDriverBinding;

EFI_STATUS
EFIAPI
AndroidBootDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
AndroidBootDriverBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  );

STATIC
VOID
ResetDriverHealthTestState (
  VOID
  )
{
  mHandleProtocolStatus                        = EFI_SUCCESS;
  mHandleProtocolCallCount                     = 0;
  mSetDriverHealthStatusCallCount              = 0;
  mSetDriverHealthStatusFromEfiStatusCallCount = 0;
  mLastDriverHealthEfiStatus                   = EFI_SUCCESS;
  mDriverHealthStatus                          = EfiDriverHealthStatusHealthy;

  ZeroMem (&mBS, sizeof (mBS));
  gBS          = &mBS;
  gBS->CopyMem = (EFI_COPY_MEM)CopyMem;
}

STATIC
EFI_STATUS
EFIAPI
UnitTestHandleProtocol (
  IN  EFI_HANDLE  Handle,
  IN  EFI_GUID    *Protocol,
  OUT VOID        **Interface
  )
{
  mHandleProtocolCallCount++;

  UT_ASSERT_EQUAL ((UINTN)Handle, (UINTN)TEST_CONTROLLER_HANDLE);
  UT_ASSERT_TRUE (CompareGuid (Protocol, &gEfiDevicePathProtocolGuid));
  UT_ASSERT_NOT_NULL (Interface);

  if (EFI_ERROR (mHandleProtocolStatus)) {
    return mHandleProtocolStatus;
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
NvidiaInstallDriverModelProtocols (
  IN EFI_HANDLE                   ImageHandle,
  IN EFI_SYSTEM_TABLE             *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding,
  IN EFI_HANDLE                   DriverBindingHandle,
  IN CONST CHAR16                 *DriverName,
  IN CONST CHAR16                 *ControllerName OPTIONAL,
  IN EFI_GUID                     *ControllerProtocolGuid OPTIONAL
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NvidiaSetDriverHealthStatus (
  IN EFI_DRIVER_HEALTH_STATUS  HealthStatus
  )
{
  mSetDriverHealthStatusCallCount++;
  mDriverHealthStatus = HealthStatus;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NvidiaSetDriverHealthStatusFromEfiStatus (
  IN EFI_STATUS  Status
  )
{
  mSetDriverHealthStatusFromEfiStatusCallCount++;
  mLastDriverHealthEfiStatus = Status;
  mDriverHealthStatus        = EFI_ERROR (Status) ?
                               EfiDriverHealthStatusFailed :
                               EfiDriverHealthStatusHealthy;

  return EFI_SUCCESS;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestDriverStartReportsHandleProtocolHealthFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ResetDriverHealthTestState ();
  gBS->HandleProtocol                           = UnitTestHandleProtocol;
  mHandleProtocolStatus                         = EFI_DEVICE_ERROR;
  mAndroidBootDriverBinding.DriverBindingHandle = TEST_DRIVER_BINDING_HANDLE;

  UT_ASSERT_STATUS_EQUAL (
    AndroidBootDriverBindingStart (
      &mAndroidBootDriverBinding,
      TEST_CONTROLLER_HANDLE,
      NULL
      ),
    EFI_DEVICE_ERROR
    );
  UT_ASSERT_EQUAL (mHandleProtocolCallCount, 1);
  UT_ASSERT_EQUAL (mSetDriverHealthStatusCallCount, 0);
  UT_ASSERT_EQUAL (mSetDriverHealthStatusFromEfiStatusCallCount, 1);
  UT_ASSERT_EQUAL (mLastDriverHealthEfiStatus, EFI_DEVICE_ERROR);
  UT_ASSERT_EQUAL (mDriverHealthStatus, EfiDriverHealthStatusFailed);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestDriverStopRejectsChildHandlesWithoutHealthChange (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ResetDriverHealthTestState ();

  UT_ASSERT_STATUS_EQUAL (
    AndroidBootDriverBindingStop (
      &mAndroidBootDriverBinding,
      TEST_CONTROLLER_HANDLE,
      1,
      NULL
      ),
    EFI_UNSUPPORTED
    );
  UT_ASSERT_EQUAL (mSetDriverHealthStatusCallCount, 0);
  UT_ASSERT_EQUAL (mSetDriverHealthStatusFromEfiStatusCallCount, 0);
  UT_ASSERT_EQUAL (mDriverHealthStatus, EfiDriverHealthStatusHealthy);

  return UNIT_TEST_PASSED;
}

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
  UNIT_TEST_SUITE_HANDLE      DriverHealthTestSuite;

  Fw = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  // Configure a stubby gBS.
  ResetDriverHealthTestState ();

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

  // Populate the Driver Health Test Suite.
  Status = CreateUnitTestSuite (
             &DriverHealthTestSuite,
             Fw,
             "Driver Health Tests",
             "AndroidBootDxe.DriverHealthTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for DriverHealthTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    DriverHealthTestSuite,
    "DriverStart reports HandleProtocol failure through Driver Health",
    "DriverStartHandleProtocolHealth",
    TestDriverStartReportsHandleProtocolHealthFailure,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    DriverHealthTestSuite,
    "DriverStop child-handle rejection does not mutate Driver Health",
    "DriverStopChildHandleHealth",
    TestDriverStopRejectsChildHandlesWithoutHealthChange,
    NULL,
    NULL,
    NULL
    );

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
