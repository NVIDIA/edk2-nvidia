/** @file

  Host-based unit tests for DeviceDiscoveryDriverLib.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <PiDxe.h>
#include <Uefi.h>
#include <HostBasedTestStubLib/UefiBootServicesTableStubLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DeviceDiscoveryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NvidiaDriverModelProtocolLib.h>
#include <Library/SystemContextLib.h>
#include <Library/TimerLib.h>
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>
#include <Protocol/ArmScmiClock2Protocol.h>
#include <Protocol/AsyncDriverStatus.h>
#include <Protocol/ClockNodeProtocol.h>
#include <Protocol/ClockParents.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include <Protocol/DeviceTreeNode.h>
#include <Protocol/DriverHealth.h>
#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/PowerGateNodeProtocol.h>
#include <Protocol/ResetNodeProtocol.h>

#define UNIT_TEST_NAME     "DeviceDiscoveryDriverLib Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

#define TEST_IMAGE_HANDLE       ((EFI_HANDLE)(UINTN)0xD150)
#define TEST_CONTROLLER_HANDLE  ((EFI_HANDLE)(UINTN)0xD151)
#define TEST_DTB                ((VOID *)(UINTN)0xD7000000)
#define TEST_NODE_OFFSET        0x24
#define TEST_COMPATIBLE         "nvidia,test-device"

STATIC EFI_GUID  mTestDeviceGuid = {
  0x7d9e9e91, 0xe677, 0x4709, { 0x8e, 0x27, 0x17, 0xf3, 0xde, 0x1e, 0xc4, 0xd1 }
};

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { TEST_COMPATIBLE, &mTestDeviceGuid },
  { NULL,            NULL             }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig;

STATIC EFI_STATUS                        mGetSupportedStatus[4];
STATIC UINT32                            mGetSupportedCount[4];
STATIC UINTN                             mGetSupportedSequenceLength;
STATIC UINTN                             mGetSupportedCallCount;
STATIC EFI_STATUS                        mProcessDeviceStatus;
STATIC UINTN                             mProcessDeviceCallCount;
STATIC NON_DISCOVERABLE_DEVICE           *mLastProcessDevice;
STATIC EFI_STATUS                        mOpenProtocolStatus;
STATIC UINTN                             mOpenProtocolCallCount;
STATIC UINTN                             mCloseProtocolCallCount;
STATIC EFI_STATUS                        mHandleNodeStatus;
STATIC UINTN                             mHandleProtocolCallCount;
STATIC EFI_STATUS                        mInstallMultipleStatus;
STATIC UINTN                             mInstallMultipleCallCount;
STATIC VOID                              *mInstalledDiscoveryContext;
STATIC EFI_STATUS                        mBootAllocatePoolStatus;
STATIC UINTN                             mBootAllocatePoolCallCount;
STATIC UINTN                             mAllocateZeroPoolCallCount;
STATIC UINTN                             mAllocateZeroPoolFailOnCall;
STATIC EFI_STATUS                        mCompatibilityStatus;
STATIC UINTN                             mCompatibilityCallCount;
STATIC EFI_STATUS                        mNotifyStatus[DeviceDiscoveryMax];
STATIC UINTN                             mNotifyPhaseCount[DeviceDiscoveryMax];
STATIC UINTN                             mNotifyCallCount;
STATIC UINTN                             mSetHealthStatusCallCount;
STATIC UINTN                             mSetHealthFromEfiStatusCallCount;
STATIC EFI_STATUS                        mLastHealthEfiStatus;
STATIC EFI_DRIVER_HEALTH_STATUS          mDriverHealthStatus;
STATIC NVIDIA_DEVICE_TREE_NODE_PROTOCOL  mDeviceTreeNode;
STATIC NON_DISCOVERABLE_DEVICE           mNonDiscoverableDevice;
STATIC SCMI_CLOCK2_PROTOCOL              mScmiClockProtocol;
STATIC NVIDIA_CLOCK_PARENTS_PROTOCOL     mClockParentsProtocol;

STATIC
VOID
QueueGetSupportedStatus (
  IN EFI_STATUS  Status,
  IN UINT32      DeviceCount
  )
{
  assert_true (mGetSupportedSequenceLength < ARRAY_SIZE (mGetSupportedStatus));

  mGetSupportedStatus[mGetSupportedSequenceLength] = Status;
  mGetSupportedCount[mGetSupportedSequenceLength]  = DeviceCount;
  mGetSupportedSequenceLength++;
}

STATIC
VOID *
EFIAPI
UnitTestAllocateZeroPool (
  IN UINTN  AllocationSize
  )
{
  mAllocateZeroPoolCallCount++;
  if (mAllocateZeroPoolFailOnCall == mAllocateZeroPoolCallCount) {
    return NULL;
  }

  return AllocateZeroPool (AllocationSize);
}

EFI_STATUS
EFIAPI
UnitTestDeviceDiscoveryNotify (
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  );

#define AllocateZeroPool       UnitTestAllocateZeroPool
#define DeviceDiscoveryNotify  UnitTestDeviceDiscoveryNotify
#undef STATIC
#define STATIC
#include "../DeviceDiscoveryDriverLib.c"
#undef DeviceDiscoveryNotify
#undef AllocateZeroPool
#undef STATIC
#define STATIC  static

UINTN
EFIAPI
MicroSecondDelay (
  IN UINTN  MicroSeconds
  )
{
  return MicroSeconds;
}

VOID
EFIAPI
GetSystemContext (
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  )
{
  ZeroMem (SystemContext.SystemContextAArch64, sizeof (*SystemContext.SystemContextAArch64));
}

EFI_STATUS
EFIAPI
SwapSystemContext (
  IN OUT EFI_SYSTEM_CONTEXT  CurrentSystemContext,
  IN     EFI_SYSTEM_CONTEXT  NewSystemContext
  )
{
  return EFI_SUCCESS;
}

VOID *
EFIAPI
GetFirstGuidHob (
  IN CONST EFI_GUID  *Guid
  )
{
  return NULL;
}

EFI_STATUS
EFIAPI
NvidiaInstallComponentName2AndDriverHealth (
  IN EFI_HANDLE    DriverHandle,
  IN CONST CHAR16  *DriverName,
  IN CONST CHAR16  *ControllerName OPTIONAL,
  IN EFI_GUID      *ControllerProtocolGuid OPTIONAL
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
  mSetHealthStatusCallCount++;
  mDriverHealthStatus = HealthStatus;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NvidiaSetDriverHealthStatusFromEfiStatus (
  IN EFI_STATUS  Status
  )
{
  mSetHealthFromEfiStatusCallCount++;
  mLastHealthEfiStatus = Status;
  mDriverHealthStatus  = EFI_ERROR (Status) ?
                         EfiDriverHealthStatusFailed :
                         EfiDriverHealthStatusHealthy;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
DeviceTreeCheckNodeSingleCompatibility (
  IN CONST CHAR8  *Compatible,
  IN INT32        NodeOffset
  )
{
  mCompatibilityCallCount++;
  assert_string_equal (Compatible, TEST_COMPATIBLE);
  assert_int_equal (NodeOffset, TEST_NODE_OFFSET);

  return mCompatibilityStatus;
}

EFI_STATUS
EFIAPI
UnitTestDeviceDiscoveryNotify (
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  assert_true (Phase < DeviceDiscoveryMax);

  mNotifyCallCount++;
  mNotifyPhaseCount[Phase]++;

  return mNotifyStatus[Phase];
}

EFI_STATUS
GetSupportedDeviceTreeNodes (
  IN  VOID *DeviceTreeBase, OPTIONAL
  IN  DEVICE_TREE_NODE_SUPPORTED  IsNodeSupported,
  IN OUT UINT32                   *DeviceCount,
  OUT NVIDIA_DT_NODE_INFO         *DTNodeInfo
  )
{
  EFI_STATUS  Status;
  UINT32      Index;

  assert_true (mGetSupportedCallCount < mGetSupportedSequenceLength);
  assert_non_null (IsNodeSupported);
  assert_non_null (DeviceCount);
  assert_null (DeviceTreeBase);

  Status       = mGetSupportedStatus[mGetSupportedCallCount];
  *DeviceCount = mGetSupportedCount[mGetSupportedCallCount];
  mGetSupportedCallCount++;

  if (!EFI_ERROR (Status) && (DTNodeInfo != NULL)) {
    for (Index = 0; Index < *DeviceCount; Index++) {
      DTNodeInfo[Index].DeviceTreeBase  = TEST_DTB;
      DTNodeInfo[Index].NodeOffset      = TEST_NODE_OFFSET + Index;
      DTNodeInfo[Index].DeviceType      = &mTestDeviceGuid;
      DTNodeInfo[Index].PciIoInitialize = NULL;
      DTNodeInfo[Index].Phandle         = Index + 1;
    }
  }

  return Status;
}

EFI_STATUS
ProcessDeviceTreeNodeWithHandle (
  IN      NVIDIA_DT_NODE_INFO      *DeviceInfo,
  IN      NON_DISCOVERABLE_DEVICE  *Device,
  IN      EFI_HANDLE               DriverHandle,
  IN OUT  EFI_HANDLE               *DeviceHandle
  )
{
  mProcessDeviceCallCount++;
  assert_non_null (DeviceInfo);
  assert_non_null (Device);
  assert_ptr_equal (DriverHandle, TEST_IMAGE_HANDLE);
  assert_non_null (DeviceHandle);

  if (EFI_ERROR (mProcessDeviceStatus)) {
    return mProcessDeviceStatus;
  }

  mLastProcessDevice = Device;
  *DeviceHandle      = TEST_CONTROLLER_HANDLE;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
UnitTestOpenProtocol (
  IN  EFI_HANDLE  Handle,
  IN  EFI_GUID    *Protocol,
  OUT VOID        **Interface OPTIONAL,
  IN  EFI_HANDLE  AgentHandle,
  IN  EFI_HANDLE  ControllerHandle,
  IN  UINT32      Attributes
  )
{
  mOpenProtocolCallCount++;
  assert_ptr_equal (Handle, TEST_CONTROLLER_HANDLE);
  assert_true (CompareGuid (Protocol, &gNVIDIANonDiscoverableDeviceProtocolGuid));
  assert_ptr_equal (AgentHandle, TEST_IMAGE_HANDLE);
  assert_ptr_equal (ControllerHandle, TEST_CONTROLLER_HANDLE);
  assert_int_equal (Attributes, EFI_OPEN_PROTOCOL_BY_DRIVER);

  if (!EFI_ERROR (mOpenProtocolStatus)) {
    assert_non_null (Interface);
    *Interface = &mNonDiscoverableDevice;
  }

  return mOpenProtocolStatus;
}

STATIC
EFI_STATUS
EFIAPI
UnitTestCloseProtocol (
  IN EFI_HANDLE  Handle,
  IN EFI_GUID    *Protocol,
  IN EFI_HANDLE  AgentHandle,
  IN EFI_HANDLE  ControllerHandle
  )
{
  mCloseProtocolCallCount++;
  assert_ptr_equal (Handle, TEST_CONTROLLER_HANDLE);
  assert_true (CompareGuid (Protocol, &gNVIDIANonDiscoverableDeviceProtocolGuid));
  assert_ptr_equal (AgentHandle, TEST_IMAGE_HANDLE);
  assert_ptr_equal (ControllerHandle, TEST_CONTROLLER_HANDLE);

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
  mHandleProtocolCallCount++;
  assert_ptr_equal (Handle, TEST_CONTROLLER_HANDLE);
  assert_non_null (Interface);

  if (CompareGuid (Protocol, &gNVIDIADeviceTreeNodeProtocolGuid)) {
    if (!EFI_ERROR (mHandleNodeStatus)) {
      *Interface = &mDeviceTreeNode;
    }

    return mHandleNodeStatus;
  }

  return EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
EFIAPI
UnitTestInstallMultipleProtocolInterfaces (
  IN OUT EFI_HANDLE  *Handle,
  ...
  )
{
  VA_LIST   Args;
  EFI_GUID  *ProtocolGuid;
  VOID      *Interface;

  mInstallMultipleCallCount++;
  assert_non_null (Handle);
  assert_ptr_equal (*Handle, TEST_CONTROLLER_HANDLE);

  VA_START (Args, Handle);
  while (TRUE) {
    ProtocolGuid = VA_ARG (Args, EFI_GUID *);
    if (ProtocolGuid == NULL) {
      break;
    }

    Interface = VA_ARG (Args, VOID *);
    if (CompareGuid (ProtocolGuid, &gNVIDIADeviceDiscoveryContextGuid)) {
      mInstalledDiscoveryContext = Interface;
    }
  }

  VA_END (Args);

  return mInstallMultipleStatus;
}

STATIC
EFI_STATUS
EFIAPI
UnitTestBootAllocatePool (
  IN  EFI_MEMORY_TYPE  PoolType,
  IN  UINTN            Size,
  OUT VOID             **Buffer
  )
{
  mBootAllocatePoolCallCount++;
  assert_int_equal (PoolType, EfiBootServicesData);
  assert_int_equal (Size, sizeof (NVIDIA_DEVICE_DISCOVERY_CONTEXT));
  assert_non_null (Buffer);

  if (EFI_ERROR (mBootAllocatePoolStatus)) {
    return mBootAllocatePoolStatus;
  }

  *Buffer = AllocateZeroPool (Size);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
UnitTestBootFreePool (
  IN VOID  *Buffer
  )
{
  if (Buffer == mInstalledDiscoveryContext) {
    mInstalledDiscoveryContext = NULL;
  }

  FreePool (Buffer);
  return EFI_SUCCESS;
}

STATIC
VOID
EFIAPI
UnitTestSetMem (
  IN VOID   *Buffer,
  IN UINTN  Size,
  IN UINT8  Value
  )
{
  SetMem (Buffer, Size, Value);
}

STATIC
VOID
ResetTestState (
  VOID
  )
{
  UINTN  Index;

  UefiBootServicesTableInit ();
  gBS->AllocatePool                      = UnitTestBootAllocatePool;
  gBS->CloseProtocol                     = UnitTestCloseProtocol;
  gBS->FreePool                          = UnitTestBootFreePool;
  gBS->HandleProtocol                    = UnitTestHandleProtocol;
  gBS->InstallMultipleProtocolInterfaces = UnitTestInstallMultipleProtocolInterfaces;
  gBS->OpenProtocol                      = UnitTestOpenProtocol;
  gBS->SetMem                            = UnitTestSetMem;

  ZeroMem (&gDeviceDiscoverDriverConfig, sizeof (gDeviceDiscoverDriverConfig));
  gDeviceDiscoverDriverConfig.DriverName                                 = L"NVIDIA Device Discovery Test";
  gDeviceDiscoverDriverConfig.SkipEdkiiNondiscoverableInstall            = TRUE;
  gDeviceDiscoverDriverConfig.SkipAutoDeinitControllerOnExitBootServices = TRUE;

  ZeroMem (mGetSupportedStatus, sizeof (mGetSupportedStatus));
  ZeroMem (mGetSupportedCount, sizeof (mGetSupportedCount));
  ZeroMem (mNotifyStatus, sizeof (mNotifyStatus));
  ZeroMem (mNotifyPhaseCount, sizeof (mNotifyPhaseCount));
  ZeroMem (&mDeviceTreeNode, sizeof (mDeviceTreeNode));
  ZeroMem (&mNonDiscoverableDevice, sizeof (mNonDiscoverableDevice));
  ZeroMem (&mScmiClockProtocol, sizeof (mScmiClockProtocol));
  ZeroMem (&mClockParentsProtocol, sizeof (mClockParentsProtocol));

  for (Index = 0; Index < ARRAY_SIZE (mNotifyStatus); Index++) {
    mNotifyStatus[Index] = EFI_SUCCESS;
  }

  mGetSupportedSequenceLength      = 0;
  mGetSupportedCallCount           = 0;
  mProcessDeviceStatus             = EFI_SUCCESS;
  mProcessDeviceCallCount          = 0;
  mLastProcessDevice               = NULL;
  mOpenProtocolStatus              = EFI_SUCCESS;
  mOpenProtocolCallCount           = 0;
  mCloseProtocolCallCount          = 0;
  mHandleNodeStatus                = EFI_SUCCESS;
  mHandleProtocolCallCount         = 0;
  mInstallMultipleStatus           = EFI_SUCCESS;
  mInstallMultipleCallCount        = 0;
  mInstalledDiscoveryContext       = NULL;
  mBootAllocatePoolStatus          = EFI_SUCCESS;
  mBootAllocatePoolCallCount       = 0;
  mAllocateZeroPoolCallCount       = 0;
  mAllocateZeroPoolFailOnCall      = 0;
  mCompatibilityStatus             = EFI_SUCCESS;
  mCompatibilityCallCount          = 0;
  mNotifyCallCount                 = 0;
  mSetHealthStatusCallCount        = 0;
  mSetHealthFromEfiStatusCallCount = 0;
  mLastHealthEfiStatus             = EFI_SUCCESS;
  mDriverHealthStatus              = EfiDriverHealthStatusHealthy;

  mDeviceTreeNode.DeviceTreeBase = TEST_DTB;
  mDeviceTreeNode.NodeOffset     = TEST_NODE_OFFSET;

  gScmiClockProtocol    = &mScmiClockProtocol;
  gClockParentsProtocol = &mClockParentsProtocol;
  mImageHandle          = TEST_IMAGE_HANDLE;
  EnumerationCompleted  = FALSE;
  SubThreadsRunning     = 0;
  AsyncProtocol         = NULL;
  CurrentThread         = NULL;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ResetTestState ();
  return UNIT_TEST_PASSED;
}

STATIC
VOID
EFIAPI
TestTeardown (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  if (mInstalledDiscoveryContext != NULL) {
    FreePool (mInstalledDiscoveryContext);
    mInstalledDiscoveryContext = NULL;
  }

  if (mLastProcessDevice != NULL) {
    FreePool (mLastProcessDevice);
    mLastProcessDevice = NULL;
  }

  UefiBootServicesTableDeinit ();
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestDeviceTreeIsSupportedMapsCompatibility (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                    Status;
  EFI_GUID                      *DeviceType;
  NON_DISCOVERABLE_DEVICE_INIT  PciIoInitialize;

  DeviceType      = NULL;
  PciIoInitialize = (NON_DISCOVERABLE_DEVICE_INIT)(UINTN)0x1234;

  Status = DeviceTreeIsSupported (
             NULL,
             &mDeviceTreeNode,
             &DeviceType,
             &PciIoInitialize
             );

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_TRUE (DeviceType == &mTestDeviceGuid);
  UT_ASSERT_TRUE (PciIoInitialize == NULL);
  UT_ASSERT_EQUAL (mCompatibilityCallCount, 1);
  UT_ASSERT_EQUAL (mNotifyPhaseCount[DeviceDiscoveryDeviceTreeCompatibility], 1);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestEnumerateNoNodesReportsHealthy (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  QueueGetSupportedStatus (EFI_NOT_FOUND, 0);

  Status = DeviceDiscoveryEnumerateDevices ();

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_TRUE (EnumerationCompleted);
  UT_ASSERT_EQUAL (mGetSupportedCallCount, 1);
  UT_ASSERT_EQUAL (mNotifyPhaseCount[DeviceDiscoveryEnumerationCompleted], 1);
  UT_ASSERT_EQUAL (mSetHealthStatusCallCount, 1);
  UT_ASSERT_EQUAL (mDriverHealthStatus, EfiDriverHealthStatusHealthy);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestEnumerateDiscoveryFailureReportsFailed (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  QueueGetSupportedStatus (EFI_DEVICE_ERROR, 0);

  Status = DeviceDiscoveryEnumerateDevices ();

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);
  UT_ASSERT_FALSE (EnumerationCompleted);
  UT_ASSERT_EQUAL (mSetHealthFromEfiStatusCallCount, 1);
  UT_ASSERT_EQUAL (mLastHealthEfiStatus, EFI_DEVICE_ERROR);
  UT_ASSERT_EQUAL (mDriverHealthStatus, EfiDriverHealthStatusFailed);
  UT_ASSERT_EQUAL (mNotifyPhaseCount[DeviceDiscoveryEnumerationCompleted], 0);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestEnumerateNodeInfoAllocationFailureReportsFailed (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  QueueGetSupportedStatus (EFI_SUCCESS, 1);
  mAllocateZeroPoolFailOnCall = 1;

  Status = DeviceDiscoveryEnumerateDevices ();

  UT_ASSERT_STATUS_EQUAL (Status, EFI_OUT_OF_RESOURCES);
  UT_ASSERT_EQUAL (mAllocateZeroPoolCallCount, 1);
  UT_ASSERT_EQUAL (mSetHealthStatusCallCount, 1);
  UT_ASSERT_EQUAL (mDriverHealthStatus, EfiDriverHealthStatusFailed);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestEnumerateSecondPassFailureReportsFailed (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  QueueGetSupportedStatus (EFI_SUCCESS, 1);
  QueueGetSupportedStatus (EFI_DEVICE_ERROR, 1);

  Status = DeviceDiscoveryEnumerateDevices ();

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);
  UT_ASSERT_EQUAL (mGetSupportedCallCount, 2);
  UT_ASSERT_EQUAL (mSetHealthFromEfiStatusCallCount, 1);
  UT_ASSERT_EQUAL (mLastHealthEfiStatus, EFI_DEVICE_ERROR);
  UT_ASSERT_EQUAL (mDriverHealthStatus, EfiDriverHealthStatusFailed);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestEnumerateProcessFailureReportsFailed (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  QueueGetSupportedStatus (EFI_SUCCESS, 1);
  QueueGetSupportedStatus (EFI_SUCCESS, 1);
  mProcessDeviceStatus = EFI_DEVICE_ERROR;

  Status = DeviceDiscoveryEnumerateDevices ();

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_TRUE (EnumerationCompleted);
  UT_ASSERT_EQUAL (mProcessDeviceCallCount, 1);
  UT_ASSERT_EQUAL (mSetHealthStatusCallCount, 1);
  UT_ASSERT_EQUAL (mDriverHealthStatus, EfiDriverHealthStatusFailed);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestEnumerateStartFailureReportsFailed (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  QueueGetSupportedStatus (EFI_SUCCESS, 1);
  QueueGetSupportedStatus (EFI_SUCCESS, 1);
  mOpenProtocolStatus = EFI_NOT_FOUND;

  Status = DeviceDiscoveryEnumerateDevices ();

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_TRUE (EnumerationCompleted);
  UT_ASSERT_EQUAL (mOpenProtocolCallCount, 1);
  UT_ASSERT_EQUAL (mCloseProtocolCallCount, 0);
  UT_ASSERT_EQUAL (mSetHealthStatusCallCount, 1);
  UT_ASSERT_EQUAL (mDriverHealthStatus, EfiDriverHealthStatusFailed);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestEnumerateSuccessfulDeviceReportsHealthy (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  QueueGetSupportedStatus (EFI_SUCCESS, 1);
  QueueGetSupportedStatus (EFI_SUCCESS, 1);

  Status = DeviceDiscoveryEnumerateDevices ();

  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_TRUE (EnumerationCompleted);
  UT_ASSERT_EQUAL (mProcessDeviceCallCount, 1);
  UT_ASSERT_EQUAL (mOpenProtocolCallCount, 1);
  UT_ASSERT_EQUAL (mHandleProtocolCallCount, 1);
  UT_ASSERT_EQUAL (mBootAllocatePoolCallCount, 1);
  UT_ASSERT_EQUAL (mInstallMultipleCallCount, 1);
  UT_ASSERT_NOT_NULL (mInstalledDiscoveryContext);
  UT_ASSERT_EQUAL (mNotifyPhaseCount[DeviceDiscoveryDriverBindingSupported], 1);
  UT_ASSERT_EQUAL (mNotifyPhaseCount[DeviceDiscoveryDriverBindingStart], 1);
  UT_ASSERT_EQUAL (mNotifyPhaseCount[DeviceDiscoveryEnumerationCompleted], 1);
  UT_ASSERT_EQUAL (mSetHealthStatusCallCount, 1);
  UT_ASSERT_EQUAL (mDriverHealthStatus, EfiDriverHealthStatusHealthy);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestEnumerateCompletionFailureReportsFailed (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  QueueGetSupportedStatus (EFI_NOT_FOUND, 0);
  mNotifyStatus[DeviceDiscoveryEnumerationCompleted] = EFI_DEVICE_ERROR;

  Status = DeviceDiscoveryEnumerateDevices ();

  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);
  UT_ASSERT_TRUE (EnumerationCompleted);
  UT_ASSERT_EQUAL (mNotifyPhaseCount[DeviceDiscoveryEnumerationCompleted], 1);
  UT_ASSERT_EQUAL (mSetHealthStatusCallCount, 1);
  UT_ASSERT_EQUAL (mDriverHealthStatus, EfiDriverHealthStatusFailed);

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
             "DeviceDiscoveryDriverLib",
             "DeviceDiscoveryDriverLib",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  AddTestCase (
    Suite,
    "Map compatible DT nodes to device GUIDs",
    "DeviceTreeIsSupportedMapsCompatibility",
    TestDeviceTreeIsSupportedMapsCompatibility,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Report healthy when no supported nodes are present",
    "EnumerateNoNodesReportsHealthy",
    TestEnumerateNoNodesReportsHealthy,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Report failed when supported-node discovery fails",
    "EnumerateDiscoveryFailureReportsFailed",
    TestEnumerateDiscoveryFailureReportsFailed,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Report failed when node-info allocation fails",
    "EnumerateNodeInfoAllocationFailureReportsFailed",
    TestEnumerateNodeInfoAllocationFailureReportsFailed,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Report failed when second discovery pass fails",
    "EnumerateSecondPassFailureReportsFailed",
    TestEnumerateSecondPassFailureReportsFailed,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Report failed when processing a node fails",
    "EnumerateProcessFailureReportsFailed",
    TestEnumerateProcessFailureReportsFailed,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Report failed when controller start fails",
    "EnumerateStartFailureReportsFailed",
    TestEnumerateStartFailureReportsFailed,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Report healthy when controller start succeeds",
    "EnumerateSuccessfulDeviceReportsHealthy",
    TestEnumerateSuccessfulDeviceReportsHealthy,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Report failed when enumeration completion notification fails",
    "EnumerateCompletionFailureReportsFailed",
    TestEnumerateCompletionFailureReportsFailed,
    TestSetup,
    TestTeardown,
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
