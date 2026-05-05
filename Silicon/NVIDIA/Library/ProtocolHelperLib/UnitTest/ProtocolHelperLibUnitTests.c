/** @file

  Host-based unit tests for ProtocolHelperLib.

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
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ProtocolHelperLib.h>
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>
#include <Protocol/DevicePath.h>

#define UNIT_TEST_NAME     "ProtocolHelperLib Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

#define MAX_HANDLE_PROTOCOL_CALLS  2

typedef struct {
  EFI_HANDLE    Handle;
  EFI_GUID      *ProtocolGuid;
  VOID          *Interface;
  EFI_STATUS    Status;
} HANDLE_PROTOCOL_CALL;

STATIC EFI_DEVICE_PATH_PROTOCOL  mDevicePath;
STATIC EFI_HANDLE                mParentHandle;
STATIC VOID                      *mParentProtocol;
STATIC EFI_GUID                  mParentProtocolGuid = {
  0xe160b326, 0x57d8, 0x41a5, { 0xb5, 0xef, 0x07, 0x13, 0x6d, 0xb6, 0xd9, 0x1d }
};

STATIC EFI_DEVICE_PATH_PROTOCOL  *mExpectedDevicePath;
STATIC EFI_STATUS                mLocateDevicePathStatus;
STATIC UINTN                     mLocateDevicePathCallCount;
STATIC HANDLE_PROTOCOL_CALL      mHandleProtocolCalls[MAX_HANDLE_PROTOCOL_CALLS];
STATIC UINTN                     mExpectedHandleProtocolCount;
STATIC UINTN                     mHandleProtocolCallCount;

STATIC
EFI_STATUS
EFIAPI
UnitTestLocateDevicePath (
  IN     EFI_GUID                  *Protocol,
  IN OUT EFI_DEVICE_PATH_PROTOCOL  **DevicePath,
  OUT    EFI_HANDLE                *Device
  )
{
  mLocateDevicePathCallCount++;
  assert_true (CompareGuid (Protocol, &mParentProtocolGuid));
  assert_ptr_equal (*DevicePath, mExpectedDevicePath);

  if (EFI_ERROR (mLocateDevicePathStatus)) {
    return mLocateDevicePathStatus;
  }

  *Device = mParentHandle;
  return EFI_SUCCESS;
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
  HANDLE_PROTOCOL_CALL  *ExpectedCall;

  assert_true (mHandleProtocolCallCount < mExpectedHandleProtocolCount);
  ExpectedCall = &mHandleProtocolCalls[mHandleProtocolCallCount++];

  assert_ptr_equal (Handle, ExpectedCall->Handle);
  assert_true (CompareGuid (Protocol, ExpectedCall->ProtocolGuid));

  if (EFI_ERROR (ExpectedCall->Status)) {
    return ExpectedCall->Status;
  }

  *Interface = ExpectedCall->Interface;
  return EFI_SUCCESS;
}

STATIC
VOID
ExpectHandleProtocolCall (
  IN EFI_HANDLE  Handle,
  IN EFI_GUID    *ProtocolGuid,
  IN VOID        *Interface,
  IN EFI_STATUS  Status
  )
{
  HANDLE_PROTOCOL_CALL  *ExpectedCall;

  assert_true (mExpectedHandleProtocolCount < MAX_HANDLE_PROTOCOL_CALLS);
  ExpectedCall               = &mHandleProtocolCalls[mExpectedHandleProtocolCount++];
  ExpectedCall->Handle       = Handle;
  ExpectedCall->ProtocolGuid = ProtocolGuid;
  ExpectedCall->Interface    = Interface;
  ExpectedCall->Status       = Status;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UefiBootServicesTableInit ();

  ZeroMem (&mDevicePath, sizeof (mDevicePath));
  ZeroMem (mHandleProtocolCalls, sizeof (mHandleProtocolCalls));

  mParentHandle                = (EFI_HANDLE)(UINTN)0x1234;
  mParentProtocol              = (VOID *)(UINTN)0x5678;
  mExpectedDevicePath          = &mDevicePath;
  mLocateDevicePathStatus      = EFI_SUCCESS;
  mLocateDevicePathCallCount   = 0;
  mExpectedHandleProtocolCount = 0;
  mHandleProtocolCallCount     = 0;

  gBS->LocateDevicePath = UnitTestLocateDevicePath;
  gBS->HandleProtocol   = UnitTestHandleProtocol;

  return UNIT_TEST_PASSED;
}

STATIC
VOID
EFIAPI
TestTeardown (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  assert_int_equal (mHandleProtocolCallCount, mExpectedHandleProtocolCount);
  UefiBootServicesTableDeinit ();
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetParentProtocolFromDevicePathSucceeds (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VOID  *Protocol;

  Protocol = NULL;
  ExpectHandleProtocolCall (mParentHandle, &mParentProtocolGuid, mParentProtocol, EFI_SUCCESS);

  UT_ASSERT_NOT_EFI_ERROR (
    ProtocolHelperGetParentProtocolFromDevicePath (
      &mDevicePath,
      &mParentProtocolGuid,
      &Protocol
      )
    );
  UT_ASSERT_EQUAL (Protocol, mParentProtocol);
  UT_ASSERT_EQUAL (mLocateDevicePathCallCount, 1);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetParentProtocolFromDevicePathReturnsLocateFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VOID  *Protocol;

  Protocol                = NULL;
  mLocateDevicePathStatus = EFI_NOT_FOUND;

  UT_ASSERT_STATUS_EQUAL (
    ProtocolHelperGetParentProtocolFromDevicePath (
      &mDevicePath,
      &mParentProtocolGuid,
      &Protocol
      ),
    EFI_NOT_FOUND
    );
  UT_ASSERT_EQUAL (Protocol, NULL);
  UT_ASSERT_EQUAL (mLocateDevicePathCallCount, 1);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetParentProtocolFromDevicePathReturnsHandleFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VOID  *Protocol;

  Protocol = NULL;
  ExpectHandleProtocolCall (mParentHandle, &mParentProtocolGuid, NULL, EFI_UNSUPPORTED);

  UT_ASSERT_STATUS_EQUAL (
    ProtocolHelperGetParentProtocolFromDevicePath (
      &mDevicePath,
      &mParentProtocolGuid,
      &Protocol
      ),
    EFI_UNSUPPORTED
    );
  UT_ASSERT_EQUAL (Protocol, NULL);
  UT_ASSERT_EQUAL (mLocateDevicePathCallCount, 1);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetParentProtocolFromHandleSucceeds (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_HANDLE  ChildHandle;
  VOID        *Protocol;

  ChildHandle = (EFI_HANDLE)(UINTN)0x8765;
  Protocol    = NULL;

  ExpectHandleProtocolCall (ChildHandle, &gEfiDevicePathProtocolGuid, &mDevicePath, EFI_SUCCESS);
  ExpectHandleProtocolCall (mParentHandle, &mParentProtocolGuid, mParentProtocol, EFI_SUCCESS);

  UT_ASSERT_NOT_EFI_ERROR (
    ProtocolHelperGetParentProtocolFromHandle (
      ChildHandle,
      &mParentProtocolGuid,
      &Protocol
      )
    );
  UT_ASSERT_EQUAL (Protocol, mParentProtocol);
  UT_ASSERT_EQUAL (mLocateDevicePathCallCount, 1);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestGetParentProtocolFromHandleReturnsDevicePathFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_HANDLE  ChildHandle;
  VOID        *Protocol;

  ChildHandle = (EFI_HANDLE)(UINTN)0x8765;
  Protocol    = NULL;

  ExpectHandleProtocolCall (ChildHandle, &gEfiDevicePathProtocolGuid, NULL, EFI_NOT_FOUND);

  UT_ASSERT_STATUS_EQUAL (
    ProtocolHelperGetParentProtocolFromHandle (
      ChildHandle,
      &mParentProtocolGuid,
      &Protocol
      ),
    EFI_NOT_FOUND
    );
  UT_ASSERT_EQUAL (Protocol, NULL);
  UT_ASSERT_EQUAL (mLocateDevicePathCallCount, 0);

  return UNIT_TEST_PASSED;
}

#include "../ProtocolHelperLib.c"

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

  Status = CreateUnitTestSuite (&Suite, Framework, "Protocol helpers", "ProtocolHelperLib", NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "Get parent protocol from device path", "ParentProtocolFromDevicePath", TestGetParentProtocolFromDevicePathSucceeds, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Return LocateDevicePath failure", "LocateDevicePathFailure", TestGetParentProtocolFromDevicePathReturnsLocateFailure, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Return parent HandleProtocol failure", "ParentHandleProtocolFailure", TestGetParentProtocolFromDevicePathReturnsHandleFailure, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Get parent protocol from handle", "ParentProtocolFromHandle", TestGetParentProtocolFromHandleSucceeds, TestSetup, TestTeardown, NULL);
  AddTestCase (Suite, "Return child DevicePath failure", "ChildDevicePathFailure", TestGetParentProtocolFromHandleReturnsDevicePathFailure, TestSetup, TestTeardown, NULL);

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
