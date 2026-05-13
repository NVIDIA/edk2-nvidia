/** @file

  Host-based unit tests for NvidiaDriverModelProtocolLib.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#include <HostBasedTestStubLib/UefiBootServicesTableStubLib.h>
#include <Library/NvidiaDriverModelProtocolLib.h>
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>
#include <Protocol/DriverHealth.h>

#define UNIT_TEST_NAME      "NvidiaDriverModelProtocolLib Unit Tests"
#define UNIT_TEST_VERSION   "1.0"
#define TEST_DRIVER_HANDLE  ((EFI_HANDLE)(UINTN)0x11110000)

STATIC
UNIT_TEST_STATUS
EFIAPI
TestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UefiBootServicesTableInit ();
  NvidiaSetDriverHealthStatus (EfiDriverHealthStatusHealthy);
  return UNIT_TEST_PASSED;
}

STATIC
VOID
EFIAPI
TestTeardown (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UefiBootServicesTableDeinit ();
}

STATIC
EFI_DRIVER_HEALTH_PROTOCOL *
InstallDriverHealthProtocol (
  VOID
  )
{
  EFI_STATUS                  Status;
  EFI_DRIVER_HEALTH_PROTOCOL  *DriverHealth;

  DriverHealth = NULL;
  MockInstallMultipleProtocolInterfaces (
    EFI_SUCCESS,
    &gEfiDriverHealthProtocolGuid,
    &DriverHealth,
    NULL
    );

  Status = NvidiaInstallDriverHealthProtocol (TEST_DRIVER_HANDLE, NULL, NULL);
  assert_int_equal (Status, EFI_SUCCESS);
  assert_non_null (DriverHealth);

  return DriverHealth;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestInstallDefaultsToHealthy (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                  Status;
  EFI_DRIVER_HEALTH_PROTOCOL  *DriverHealth;
  EFI_DRIVER_HEALTH_STATUS    HealthStatus;

  DriverHealth = InstallDriverHealthProtocol ();
  HealthStatus = EfiDriverHealthStatusFailed;

  Status = DriverHealth->GetHealthStatus (
                           DriverHealth,
                           NULL,
                           NULL,
                           &HealthStatus,
                           NULL,
                           NULL
                           );
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (HealthStatus, EfiDriverHealthStatusHealthy);

  HealthStatus = EfiDriverHealthStatusFailed;
  Status       = NvidiaGetDriverHealthStatus (&HealthStatus);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (HealthStatus, EfiDriverHealthStatusHealthy);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestSetStatusUpdatesReportedHealth (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                  Status;
  EFI_DRIVER_HEALTH_PROTOCOL  *DriverHealth;
  EFI_DRIVER_HEALTH_STATUS    HealthStatus;

  DriverHealth = InstallDriverHealthProtocol ();

  Status = NvidiaSetDriverHealthStatus (EfiDriverHealthStatusFailed);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  Status = DriverHealth->GetHealthStatus (
                           DriverHealth,
                           NULL,
                           NULL,
                           &HealthStatus,
                           NULL,
                           NULL
                           );
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (HealthStatus, EfiDriverHealthStatusFailed);

  Status = NvidiaSetDriverHealthStatus (EfiDriverHealthStatusRebootRequired);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  Status = NvidiaGetDriverHealthStatus (&HealthStatus);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (HealthStatus, EfiDriverHealthStatusRebootRequired);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestEfiStatusMappingUpdatesReportedHealth (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_DRIVER_HEALTH_STATUS  HealthStatus;

  InstallDriverHealthProtocol ();

  Status = NvidiaSetDriverHealthStatusFromEfiStatus (EFI_DEVICE_ERROR);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  Status = NvidiaGetDriverHealthStatus (&HealthStatus);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (HealthStatus, EfiDriverHealthStatusFailed);

  Status = NvidiaSetDriverHealthStatusFromEfiStatus (EFI_SUCCESS);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  Status = NvidiaGetDriverHealthStatus (&HealthStatus);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (HealthStatus, EfiDriverHealthStatusHealthy);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestInvalidParametersAreRejected (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                  Status;
  EFI_DRIVER_HEALTH_PROTOCOL  *DriverHealth;
  EFI_DRIVER_HEALTH_STATUS    HealthStatus;

  DriverHealth = InstallDriverHealthProtocol ();

  Status = NvidiaSetDriverHealthStatus (
             (EFI_DRIVER_HEALTH_STATUS)(EfiDriverHealthStatusRebootRequired + 1)
             );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = NvidiaGetDriverHealthStatus (NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = DriverHealth->GetHealthStatus (
                           DriverHealth,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL
                           );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = NvidiaGetDriverHealthStatus (&HealthStatus);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (HealthStatus, EfiDriverHealthStatusHealthy);

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

  Status = InitUnitTestFramework (
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
             "Driver Health State",
             "NvidiaDriverModelProtocolLib",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "Install defaults health to healthy", "InstallHealthy", TestInstallDefaultsToHealthy, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "SetStatus updates reported health", "SetStatus", TestSetStatusUpdatesReportedHealth, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "EFI status mapping updates reported health", "StatusMapping", TestEfiStatusMappingUpdatesReportedHealth, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Invalid parameters are rejected", "InvalidParameters", TestInvalidParametersAreRejected, TestSetup, TestTeardown, NULL);

  Status = RunAllTestSuites (Framework);

Done:
  if (Framework != NULL) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

int
main (
  VOID
  )
{
  return UefiTestMain ();
}
