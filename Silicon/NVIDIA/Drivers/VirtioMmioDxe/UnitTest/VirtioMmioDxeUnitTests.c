/** @file

  Host-based unit tests for VirtioMmioDxe.

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
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>

#include "../VirtioMmioDxe.c"

#define UNIT_TEST_NAME     "VirtioMmioDxe Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

#define TEST_DRIVER_HANDLE      ((EFI_HANDLE)(UINTN)0x1001)
#define TEST_CONTROLLER_HANDLE  ((EFI_HANDLE)(UINTN)0x1002)
#define TEST_MMIO_BASE          ((EFI_PHYSICAL_ADDRESS)0x0A000000ULL)
#define TEST_MMIO_SIZE          ((UINTN)0x200)

STATIC EFI_STATUS            mGetMmioRegionStatus;
STATIC EFI_PHYSICAL_ADDRESS  mMmioRegionBase;
STATIC UINTN                 mMmioRegionSize;
STATIC UINTN                 mGetMmioRegionCallCount;
STATIC EFI_STATUS            mInstallDeviceStatus;
STATIC UINTN                 mInstallDeviceCallCount;
STATIC EFI_PHYSICAL_ADDRESS  mLastInstallBaseAddress;
STATIC EFI_HANDLE            mLastInstallHandle;

EFI_STATUS
DeviceDiscoveryGetMmioRegion (
  IN  EFI_HANDLE            ControllerHandle,
  IN  UINTN                 Region,
  OUT EFI_PHYSICAL_ADDRESS  *RegionBase,
  OUT UINTN                 *RegionSize
  )
{
  mGetMmioRegionCallCount++;
  assert_ptr_equal (ControllerHandle, TEST_CONTROLLER_HANDLE);
  assert_int_equal (Region, 0);
  assert_non_null (RegionBase);
  assert_non_null (RegionSize);

  if (!EFI_ERROR (mGetMmioRegionStatus)) {
    *RegionBase = mMmioRegionBase;
    *RegionSize = mMmioRegionSize;
  }

  return mGetMmioRegionStatus;
}

EFI_STATUS
EFIAPI
VirtioMmioInstallDevice (
  IN PHYSICAL_ADDRESS  BaseAddress,
  IN EFI_HANDLE        Handle
  )
{
  mInstallDeviceCallCount++;
  mLastInstallBaseAddress = BaseAddress;
  mLastInstallHandle      = Handle;

  return mInstallDeviceStatus;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mGetMmioRegionStatus    = EFI_SUCCESS;
  mMmioRegionBase         = TEST_MMIO_BASE;
  mMmioRegionSize         = TEST_MMIO_SIZE;
  mGetMmioRegionCallCount = 0;
  mInstallDeviceStatus    = EFI_SUCCESS;
  mInstallDeviceCallCount = 0;
  mLastInstallBaseAddress = 0;
  mLastInstallHandle      = NULL;

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestUnhandledPhaseReturnsSuccess (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryDriverBindingSupported,
             TEST_DRIVER_HANDLE,
             TEST_CONTROLLER_HANDLE,
             NULL
             );

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (mGetMmioRegionCallCount, 0);
  UT_ASSERT_EQUAL (mInstallDeviceCallCount, 0);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestMissingMmioRegionReturnsUnsupported (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  mGetMmioRegionStatus = EFI_NOT_FOUND;

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryDriverBindingStart,
             TEST_DRIVER_HANDLE,
             TEST_CONTROLLER_HANDLE,
             NULL
             );

  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);
  UT_ASSERT_EQUAL (mGetMmioRegionCallCount, 1);
  UT_ASSERT_EQUAL (mInstallDeviceCallCount, 0);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestInstallFailurePropagatesError (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  mInstallDeviceStatus = EFI_DEVICE_ERROR;

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryDriverBindingStart,
             TEST_DRIVER_HANDLE,
             TEST_CONTROLLER_HANDLE,
             NULL
             );

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);
  UT_ASSERT_EQUAL (mGetMmioRegionCallCount, 1);
  UT_ASSERT_EQUAL (mInstallDeviceCallCount, 1);
  UT_ASSERT_EQUAL (mLastInstallBaseAddress, TEST_MMIO_BASE);
  UT_ASSERT_EQUAL ((UINTN)mLastInstallHandle, (UINTN)TEST_CONTROLLER_HANDLE);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestInstallSuccessReturnsSuccess (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryDriverBindingStart,
             TEST_DRIVER_HANDLE,
             TEST_CONTROLLER_HANDLE,
             NULL
             );

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (mGetMmioRegionCallCount, 1);
  UT_ASSERT_EQUAL (mInstallDeviceCallCount, 1);
  UT_ASSERT_EQUAL (mLastInstallBaseAddress, TEST_MMIO_BASE);
  UT_ASSERT_EQUAL ((UINTN)mLastInstallHandle, (UINTN)TEST_CONTROLLER_HANDLE);

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
             "VirtioMmioDxe",
             "VirtioMmioDxe",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  AddTestCase (
    Suite,
    "Return success for unhandled discovery phases",
    "UnhandledPhaseReturnsSuccess",
    TestUnhandledPhaseReturnsSuccess,
    TestSetup,
    NULL,
    NULL
    );
  AddTestCase (
    Suite,
    "Return EFI_UNSUPPORTED when MMIO region lookup fails",
    "MissingMmioRegionReturnsUnsupported",
    TestMissingMmioRegionReturnsUnsupported,
    TestSetup,
    NULL,
    NULL
    );
  AddTestCase (
    Suite,
    "Propagate error when VirtioMmioInstallDevice fails",
    "InstallFailurePropagatesError",
    TestInstallFailurePropagatesError,
    TestSetup,
    NULL,
    NULL
    );
  AddTestCase (
    Suite,
    "Install Virtio MMIO device on start phase",
    "InstallSuccessReturnsSuccess",
    TestInstallSuccessReturnsSuccess,
    TestSetup,
    NULL,
    NULL
    );

  Status = RunAllTestSuites (Framework);

Exit:
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
