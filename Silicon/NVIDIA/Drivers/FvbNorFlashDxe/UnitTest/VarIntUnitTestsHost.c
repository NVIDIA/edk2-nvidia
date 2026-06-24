/** @file
  Unit tests for the Var Store Integrity module of FvbNorFlashStandaloneMm.c

  SPDX-FileCopyrightText: Copyright (c) 2024-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>
#include <Guid/GlobalVariable.h>

#include <HostBasedTestStubLib/NorFlashStubLib.h>
#include <HostBasedTestStubLib/NorFlashStubLib.h>
#include <Library/MmVarLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <HostBasedTestStubLib/MmServicesTableStubLib.h>
#include <HostBasedTestStubLib/NvVarIntStubLib.h>
#include <HostBasedTestStubLib/ArmSvcStubLib.h>
#include <HostBasedTestStubLib/StandaloneMmOpteeStubLib.h>

// So that we can reference FvbPrivate declarations
#include "../FvbPrivate.h"
#include "Uefi/UefiBaseType.h"

#define UNIT_TEST_APP_NAME     "VarInt Unit Test Application"
#define UNIT_TEST_APP_VERSION  "0.1"

#define BLOCK_SIZE        4096
#define TOTAL_BLOCKS      4
#define PARTITION_BLOCKS  2
#define MOCK_ATTRIBUTES   0xFF
#define MEAS_SZ           32
typedef struct {
  CHAR16          *VarName;
  EFI_GUID        *VarGuid;
  UINT32          VarAttr;
  UINT8           *VarData;
  UINTN           VarSize;
  UINT8           *VarMeas;
  UINT8           *ReadMeas;
  UINTN           MeasSz;
  EFI_STATUS      ComputeReturnStatus;
  ARM_SVC_ARGS    *TestArgs;
  UINTN           NumIterations;
} VAR_INT_TEST_CONTEXT;

extern NVIDIA_VAR_INT_PROTOCOL    *VarIntProto;
STATIC NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashStub;
STATIC NOR_FLASH_ATTRIBUTES       NorFlashAttr;
STATIC UINT32                     *Handle;
STATIC UINTN                      FlushDeferredMeasurementCount;
STATIC EFI_STATUS                 FlushDeferredMeasurementStatus;

STATIC
EFI_STATUS
EFIAPI
MockFlushDeferredMeasurement (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FlushDeferredMeasurementCount++;
  if (!EFI_ERROR (FlushDeferredMeasurementStatus)) {
    This->BootstrapDeferred = FALSE;
  }

  return FlushDeferredMeasurementStatus;
}

STATIC
BOOLEAN
EFIAPI
MockVarIntIsDeferred (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  if (This == NULL) {
    return FALSE;
  }

  return This->BootstrapDeferred;
}

STATIC
VOID
InitDeferredMeasurementTest (
  OUT NVIDIA_VAR_INT_PROTOCOL  *TestVarInt,
  IN  BOOLEAN                  BootstrapDeferred,
  IN  EFI_STATUS               FlushStatus
  )
{
  ZeroMem (TestVarInt, sizeof (*TestVarInt));
  TestVarInt->FlushDeferredMeasurement = MockFlushDeferredMeasurement;
  TestVarInt->IsDeferred               = MockVarIntIsDeferred;
  TestVarInt->BootstrapDeferred        = BootstrapDeferred;
  FlushDeferredMeasurementCount        = 0;
  FlushDeferredMeasurementStatus       = FlushStatus;
  MockNvVarIntResetExitBootServicesNotify ();
}

STATIC
UNIT_TEST_STATUS
EFIAPI
RunReadyToBootDeferredMeasurementTest (
  IN EFI_STATUS  FlushStatus,
  IN EFI_STATUS  ExpectedStatus
  )
{
  EFI_STATUS               Status;
  NVIDIA_VAR_INT_PROTOCOL  TestVarInt;

  InitDeferredMeasurementTest (&TestVarInt, TRUE, FlushStatus);

  Status = VarIntFlushDeferredMeasurementAtReadyToBoot (&TestVarInt);

  UT_ASSERT_STATUS_EQUAL (Status, ExpectedStatus);
  UT_ASSERT_EQUAL (FlushDeferredMeasurementCount, 1);
  UT_ASSERT_EQUAL (MockNvVarIntGetExitBootServicesNotifyCount (), 0);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
RunExitBootServicesDeferredMeasurementTest (
  IN BOOLEAN     BootstrapDeferred,
  IN EFI_STATUS  ExpectedStatus
  )
{
  EFI_STATUS               Status;
  NVIDIA_VAR_INT_PROTOCOL  TestVarInt;

  InitDeferredMeasurementTest (&TestVarInt, BootstrapDeferred, EFI_DEVICE_ERROR);

  Status = VarIntNotifyExitBootServicesPreserveOnly (&TestVarInt);

  UT_ASSERT_STATUS_EQUAL (Status, ExpectedStatus);
  UT_ASSERT_EQUAL (FlushDeferredMeasurementCount, 0);
  UT_ASSERT_EQUAL (MockNvVarIntGetExitBootServicesNotifyCount (), 1);

  return UNIT_TEST_PASSED;
}

/**
  Mock implementation of CpuDeadLoop for unit testing using cmocka.

  This function overrides the BaseLib CpuDeadLoop to use cmocka's
  function_called() mechanism for verification in tests.

**/
VOID
EFIAPI
__wrap_CpuDeadLoop (
  VOID
  )
{
  function_called ();
  // Mock implementation - do nothing in unit tests
  // This prevents infinite loops during test execution
}

/* Assume that the reserved partition is the first partition on the Flash
   Device we mock.
  */
STATIC UINT8   *FlashDevice;
STATIC UINT32  ResPartitionSize   = (PARTITION_BLOCKS * BLOCK_SIZE);
STATIC UINT8   ResPartitionOffset = 0;
STATIC UINT8   TestMeasBuf[32];

STATIC
VOID
MockSuccessfulOpteeSignatures (
  IN ARM_SVC_ARGS  *Args,
  IN UINTN         Count
  )
{
  UINTN  Index;

  for (Index = 0; Index < Count; Index++) {
    MockArmCallSvc (Args);
    MockIsOpteePresent (TRUE);
  }
}

/*
  Test Data Fields:
          *VarName;
          *VarGuid;
          VarAttr;
          *VarData;
          VarSize;
          *VarMeas;
          *ReadMeas;
          MeasSz;
          ComputeReturnStatus;
          *TestArgs;
          NumIterations;
*/
STATIC VAR_INT_TEST_CONTEXT  VarIntComputeTestData_1 = {
  EFI_BOOT_ORDER_VARIABLE_NAME,
  &gEfiGlobalVariableGuid,
  0,
  NULL,
  3,
  TestMeasBuf,
  TestMeasBuf,
  MEAS_SZ,
  EFI_SUCCESS,
  NULL,
  1
};

STATIC VAR_INT_TEST_CONTEXT  VarIntComputeTestData_2 = {
  EFI_BOOT_ORDER_VARIABLE_NAME,
  &gEfiGlobalVariableGuid,
  0,
  NULL,
  3,
  TestMeasBuf,
  NULL,
  MEAS_SZ,
  EFI_SUCCESS,
  NULL,
  1
};

STATIC VAR_INT_TEST_CONTEXT  VarIntComputeTestData_3 = {
  EFI_BOOT_ORDER_VARIABLE_NAME,
  &gEfiGlobalVariableGuid,
  0,
  NULL,
  8,
  TestMeasBuf,
  TestMeasBuf,
  MEAS_SZ,
  EFI_SUCCESS,
  NULL,
  200
};

STATIC VAR_INT_TEST_CONTEXT  VarIntComputeTestData_4 = {
  EFI_BOOT_ORDER_VARIABLE_NAME,
  &gEfiGlobalVariableGuid,
  0,
  NULL,
  10,
  TestMeasBuf,
  TestMeasBuf,
  MEAS_SZ,
  EFI_SUCCESS,
  NULL,
  2000
};

STATIC VAR_INT_TEST_CONTEXT  VarIntComputeTestData_5 = {
  EFI_BOOT_ORDER_VARIABLE_NAME,
  &gEfiGlobalVariableGuid,
  0,
  NULL,
  43,
  TestMeasBuf,
  NULL,
  MEAS_SZ,
  EFI_SUCCESS,
  NULL,
  1
};

STATIC VAR_INT_TEST_CONTEXT  VarIntComputeTestData_6 = {
  EFI_BOOT_ORDER_VARIABLE_NAME,
  &gEfiGlobalVariableGuid,
  0,
  NULL,
  3,
  TestMeasBuf,
  TestMeasBuf,
  MEAS_SZ,
  EFI_SUCCESS,
  NULL,
  1
};

STATIC VAR_INT_TEST_CONTEXT  VarIntComputeTestData_7 = {
  EFI_BOOT_ORDER_VARIABLE_NAME,
  &gEfiGlobalVariableGuid,
  0,
  NULL,
  3,
  TestMeasBuf,
  TestMeasBuf,
  MEAS_SZ,
  EFI_SUCCESS,
  NULL,
  1
};

/*=============================Test Cases================================*/

/*
 * VarIntComputeTest_1
 * "Simple Compute Test 1: Compute/Store/Validate adding BootOrder.",
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_1 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;

  MockComputeVarMeasurement (
    TestData->VarName,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->ComputeNewMeasurement (
                          VarIntProto,
                          TestData->VarName,
                          TestData->VarGuid,
                          TestData->VarAttr,
                          TestData->VarData,
                          TestData->VarSize
                          );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  Status = VarIntProto->WriteNewMeasurement (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  Status = VarIntProto->InvalidateLast (
                          VarIntProto,
                          EFI_BOOT_ORDER_VARIABLE_NAME,
                          &gEfiGlobalVariableGuid,
                          EFI_SUCCESS
                          );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (VarIntProto->MeasurementSize, MEAS_SZ + 1);
  UT_ASSERT_EQUAL (((UINT8 *)VarIntProto->PartitionData)[0], VAR_INT_V1_VALID);

  MockComputeVarMeasurement (
    NULL,
    TestData->ReadMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_6
 * "Simple Compute Test 6: Validate migrates a matching V0 record to V1.",
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_6 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 V1MeasBuf[MEAS_SZ];
  UINT8                 *PartitionData;

  TestData      = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData = (UINT8 *)VarIntProto->PartitionData;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  PartitionData[0] = VAR_INT_VALID;
  CopyMem (&PartitionData[1], TestData->ReadMeas, TestData->MeasSz);
  CopyMem (FlashDevice, PartitionData, VarIntProto->MeasurementSize);

  SetMem (V1MeasBuf, sizeof (V1MeasBuf), 0x4);

  MockComputeVarMeasurement (
    NULL,
    V1MeasBuf,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockComputeVarMeasurementV0 (
    NULL,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 2);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[0], VAR_INT_INVALID);
  UT_ASSERT_EQUAL (PartitionData[VarIntProto->MeasurementSize], VAR_INT_V1_VALID);

  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_7
 * "Simple Compute Test 7: Validate promotes matching V1 pending record.",
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_7 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 OldMeasBuf[MEAS_SZ];
  UINT8                 *PartitionData;

  TestData      = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData = (UINT8 *)VarIntProto->PartitionData;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  SetMem (OldMeasBuf, sizeof (OldMeasBuf), 0x5);
  PartitionData[0] = VAR_INT_V1_VALID;
  CopyMem (&PartitionData[1], OldMeasBuf, sizeof (OldMeasBuf));

  PartitionData[VarIntProto->MeasurementSize] = VAR_INT_V1_PENDING;
  CopyMem (
    &PartitionData[VarIntProto->MeasurementSize + 1],
    TestData->VarMeas,
    TestData->MeasSz
    );
  CopyMem (FlashDevice, PartitionData, VarIntProto->MeasurementSize * 2);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[0], VAR_INT_V1_INVALID);
  UT_ASSERT_EQUAL (PartitionData[VarIntProto->MeasurementSize], VAR_INT_V1_VALID);

  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_8
 * "Simple Compute Test 8: Mixed zero/erased measurement partition is blank.",
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_8 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  BOOLEAN     IsBlank;

  (VOID)Context;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  SetMem (FlashDevice, BLOCK_SIZE / 8, 0);
  IsBlank = IsMeasurementPartitionErasedOrZero (
              NorFlashStub,
              ResPartitionOffset,
              ResPartitionSize
              );
  UT_ASSERT_EQUAL (IsBlank, TRUE);

  FlashDevice[0] = VAR_INT_VALID;
  IsBlank        = IsMeasurementPartitionErasedOrZero (
                     NorFlashStub,
                     ResPartitionOffset,
                     ResPartitionSize
                     );
  UT_ASSERT_EQUAL (IsBlank, FALSE);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_9 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  (VOID)Context;

  return RunReadyToBootDeferredMeasurementTest (EFI_SUCCESS, EFI_SUCCESS);
}

STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_10 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  (VOID)Context;

  return RunReadyToBootDeferredMeasurementTest (EFI_DEVICE_ERROR, EFI_DEVICE_ERROR);
}

STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_11 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  (VOID)Context;

  return RunExitBootServicesDeferredMeasurementTest (TRUE, EFI_NOT_READY);
}

STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_12 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  (VOID)Context;

  return RunExitBootServicesDeferredMeasurementTest (FALSE, EFI_SUCCESS);
}

STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_13 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  (VOID)Context;

  expect_function_call (__wrap_CpuDeadLoop);
  Status = VarIntFatalBootstrapFailure (EFI_DEVICE_ERROR, "UnitTest VarInt bootstrap");
  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTestSetup_1 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;
  UT_ASSERT_NOT_NULL (VarIntProto);
  SetMem (TestData->VarMeas, sizeof (TestMeasBuf), 0x8);
  TestData->VarData = AllocateZeroPool (TestData->VarSize);
  SetMem (TestData->VarData, TestData->VarSize, 1);
  TestData->TestArgs = AllocateZeroPool (sizeof (ARM_SVC_ARGS));

  return UNIT_TEST_PASSED;
}

STATIC
VOID
EFIAPI
VarIntComputeTestCleanup_1 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;

  SetMem (TestData->VarMeas, sizeof (TestMeasBuf), 0);

  if (TestData->VarData != NULL) {
    FreePool (TestData->VarData);
  }
}

/*
 * VarIntComputeTest_2
 * "Simple Compute Test 2: Compute/Store/Validate-fail due to invalid measurement",
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_2 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;

  MockComputeVarMeasurement (
    TestData->VarName,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->ComputeNewMeasurement (
                          VarIntProto,
                          TestData->VarName,
                          TestData->VarGuid,
                          TestData->VarAttr,
                          TestData->VarData,
                          TestData->VarSize
                          );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  Status = VarIntProto->WriteNewMeasurement (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  Status = VarIntProto->InvalidateLast (
                          VarIntProto,
                          EFI_BOOT_ORDER_VARIABLE_NAME,
                          &gEfiGlobalVariableGuid,
                          EFI_SUCCESS
                          );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  MockComputeVarMeasurement (
    NULL,
    TestData->ReadMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockComputeVarMeasurementV0 (
    NULL,
    TestData->ReadMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 2);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTestSetup_2 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;
  UT_ASSERT_NOT_NULL (VarIntProto);
  TestData->VarData = AllocateZeroPool (TestData->VarSize);
  SetMem (TestData->VarData, TestData->VarSize, 1);
  SetMem (TestData->VarMeas, sizeof (TestMeasBuf), 0x8);
  TestData->ReadMeas = AllocatePool (TestData->MeasSz);
  SetMem (TestData->ReadMeas, TestData->MeasSz, 0x4);
  TestData->TestArgs = AllocateZeroPool (sizeof (ARM_SVC_ARGS));

  return UNIT_TEST_PASSED;
}

STATIC
VOID
EFIAPI
VarIntComputeTestCleanup_2 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;
  SetMem (TestData->VarMeas, sizeof (TestMeasBuf), 0);
  FreePool (TestData->VarData);
  FreePool (TestData->ReadMeas);
}

/*
 * VarIntComputeTest_3
 * "Simple Compute Test 3: 2000 Compute/Store/Validate test block logic",
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_3 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINTN                 Index;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;

  for (Index = 0; Index < TestData->NumIterations; Index++ ) {
    MockComputeVarMeasurement (
      TestData->VarName,
      TestData->VarMeas,
      TestData->MeasSz,
      TestData->ComputeReturnStatus
      );
    MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

    Status = VarIntProto->ComputeNewMeasurement (
                            VarIntProto,
                            TestData->VarName,
                            TestData->VarGuid,
                            TestData->VarAttr,
                            TestData->VarData,
                            TestData->VarSize
                            );
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

    Status = VarIntProto->WriteNewMeasurement (VarIntProto);
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

    Status = VarIntProto->InvalidateLast (
                            VarIntProto,
                            EFI_BOOT_ORDER_VARIABLE_NAME,
                            &gEfiGlobalVariableGuid,
                            EFI_SUCCESS
                            );
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  }

  MockComputeVarMeasurement (
    NULL,
    TestData->ReadMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTestSetup_3 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;
  UT_ASSERT_NOT_NULL (VarIntProto);
  TestData->VarData = AllocateZeroPool (TestData->VarSize);
  SetMem (TestData->VarData, TestData->VarSize, 1);
  SetMem (TestData->VarMeas, sizeof (TestMeasBuf), 0xB);
  TestData->TestArgs = AllocateZeroPool (sizeof (ARM_SVC_ARGS));
  return UNIT_TEST_PASSED;
}

STATIC
VOID
EFIAPI
VarIntComputeTestCleanup_3 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;
  SetMem (TestData->VarMeas, sizeof (TestMeasBuf), 0);
  FreePool (TestData->VarData);
}

/*
 * VarIntComputeTest_4
 * "Simple Compute Test 4: 3000 Compute/Store/Validate test wrap around logic",
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_4 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINTN                 Index;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;

  for (Index = 0; Index < TestData->NumIterations; Index++ ) {
    MockComputeVarMeasurement (
      TestData->VarName,
      TestData->VarMeas,
      TestData->MeasSz,
      TestData->ComputeReturnStatus
      );
    MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

    Status = VarIntProto->ComputeNewMeasurement (
                            VarIntProto,
                            TestData->VarName,
                            TestData->VarGuid,
                            TestData->VarAttr,
                            TestData->VarData,
                            TestData->VarSize
                            );
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

    Status = VarIntProto->WriteNewMeasurement (VarIntProto);
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

    Status = VarIntProto->InvalidateLast (
                            VarIntProto,
                            EFI_BOOT_ORDER_VARIABLE_NAME,
                            &gEfiGlobalVariableGuid,
                            EFI_SUCCESS
                            );
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  }

  MockComputeVarMeasurement (
    NULL,
    TestData->ReadMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTestSetup_4 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;
  UT_ASSERT_NOT_NULL (VarIntProto);
  TestData->VarData = AllocateZeroPool (TestData->VarSize);
  SetMem (TestData->VarData, TestData->VarSize, 9);
  SetMem (TestData->VarMeas, sizeof (TestMeasBuf), 0xD);
  TestData->TestArgs = AllocateZeroPool (sizeof (ARM_SVC_ARGS));
  return UNIT_TEST_PASSED;
}

STATIC
VOID
EFIAPI
VarIntComputeTestCleanup_4 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;
  SetMem (TestData->VarMeas, sizeof (TestMeasBuf), 0);
  FreePool (TestData->VarData);
}

/*
 * VarIntComputeTest_5
 * "Simple Compute Test 5: Compute/Store/Validate-fail due to failed OPTEE command"
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_5 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;

  // Expect CpuDeadLoop to be called when OPTEE command fails
  expect_function_call (__wrap_CpuDeadLoop);

  MockComputeVarMeasurement (
    TestData->VarName,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockArmCallSvc (TestData->TestArgs);
  MockIsOpteePresent (TRUE);

  Status = VarIntProto->ComputeNewMeasurement (
                          VarIntProto,
                          TestData->VarName,
                          TestData->VarGuid,
                          TestData->VarAttr,
                          TestData->VarData,
                          TestData->VarSize
                          );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTestSetup_5 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;

  UT_ASSERT_NOT_NULL (VarIntProto);
  TestData->VarData = AllocateZeroPool (TestData->VarSize);
  SetMem (TestData->VarData, TestData->VarSize, 1);
  SetMem (TestData->VarMeas, sizeof (TestMeasBuf), 0x8);

  TestData->TestArgs       = AllocateZeroPool (sizeof (ARM_SVC_ARGS));
  TestData->TestArgs->Arg3 = 0xf;

  return UNIT_TEST_PASSED;
}

STATIC
VOID
EFIAPI
VarIntComputeTestCleanup_5 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VAR_INT_TEST_CONTEXT  *TestData;

  TestData = (VAR_INT_TEST_CONTEXT *)Context;
  SetMem (TestData->VarMeas, sizeof (TestMeasBuf), 0);
  FreePool (TestData->VarData);
}

/*=============================================================================*/

/*================Test Setup/Cleanup===========================================*/

/**
  Initializes data that will be used for the Suite.

  Calls VarIntInit to initialize and allocate the NVIDIA_VAR_INT_PROTOCOL.
**/
STATIC
VOID
EFIAPI
InitSuiteTestData (
  VOID
  )
{
  UINT32      *Handle;
  EFI_STATUS  Status;

  Handle  = AllocatePool (sizeof (UINT32));
  *Handle = 0xABCD;
  MockMmInstallProtocolInterface (
    &gNVIDIAVarIntGuid,
    (EFI_HANDLE)Handle,
    EFI_SUCCESS
    );
  MockIsOpteePresent (TRUE);

  Status = VarIntInit (
             ResPartitionOffset,
             ResPartitionSize,
             NorFlashStub,
             &NorFlashAttr
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Initialize VarInt module %r\n", Status));
    assert_false (EFI_ERROR (Status));
  }
}

STATIC
VOID
EFIAPI
CleanupSuiteTestData (
  VOID
  )
{
  FreePool (VarIntProto);
  FreePool (Handle);
  VarIntProto = NULL;
}

/**
  Initializes data that will be used for the Fvb tests.

  Allocates space for flash storage, in-memory variable partition, and a buffer
  used for testing. Sets up a flash device stub and then initializes the
  NVIDIA_FVB_PRIVATE_DATA used in the Fvb functions.
**/
STATIC
EFI_STATUS
InitTestData (
  VOID
  )
{
  EFI_STATUS  Status;

  VarIntProto = (NVIDIA_VAR_INT_PROTOCOL *)
                AllocatePool (sizeof (NVIDIA_FVB_PRIVATE_DATA));

  FlashDevice = AllocateAlignedPages (TOTAL_BLOCKS, BLOCK_SIZE);

  MmServicesTableInit ();

  Status = VirtualNorFlashInitialize (
             FlashDevice,
             (TOTAL_BLOCKS * BLOCK_SIZE),
             BLOCK_SIZE,
             &NorFlashStub
             );
  if (EFI_ERROR (Status)) {
    goto ExitInitTestData;
  }

  Status = NorFlashStub->GetAttributes (NorFlashStub, &NorFlashAttr);
  if (EFI_ERROR (Status)) {
    goto ExitInitTestData;
  }

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  if (EFI_ERROR (Status)) {
    goto ExitInitTestData;
  }

ExitInitTestData:
  return Status;
}

/**
  Cleans up the data used by the Fvb tests.

  Deallocates the flash stub and the memory used for the flash storage,
  in-memory partition, and the test buffer.
**/
STATIC
VOID
CleanUpTestData (
  VOID
  )
{
  if (FlashDevice != NULL) {
    FreeAlignedPages (FlashDevice, TOTAL_BLOCKS);
  }
}

/**
  Initialze the unit test framework, suite, and unit tests for the
  Fvb driver and run the unit tests.

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
  UNIT_TEST_FRAMEWORK_HANDLE  Fw;
  UNIT_TEST_SUITE_HANDLE      VarIntComputeSuite;

  Fw = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  Status = InitTestData ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Failed to setup Test Data %r\n",
      Status
      ));
    goto ExitUnitTestingEntry;
  }

  // Start setting up the test framework for running the tests.
  Status = InitUnitTestFramework (
             &Fw,
             UNIT_TEST_APP_NAME,
             gEfiCallerBaseName,
             UNIT_TEST_APP_VERSION
             );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in InitUnitTestFramework. Status = %r\n",
       Status)
      );
    goto ExitUnitTestingEntry;
  }

  // Populate the Fvb Getter/Setter Unit Test Suite.
  Status = CreateUnitTestSuite (
             &VarIntComputeSuite,
             Fw,
             "VarInt Compute Tests",
             "VarInt.VarIntComputeTestSuite",
             InitSuiteTestData,
             CleanupSuiteTestData
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for VarInt\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitUnitTestingEntry;
  }

  // AddTestCase Args:
  //  Suite | Description
  //  Class Name | Function
  //  Pre | Post | Context

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 1: Compute/Store/Validate adding BootOrder.",
    "SimpleComputeTest1",
    VarIntComputeTest_1,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_1
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 6: Validate migrates a matching V0 record to V1.",
    "SimpleComputeTest6",
    VarIntComputeTest_6,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 7: Validate promotes matching V1 pending record.",
    "SimpleComputeTest7",
    VarIntComputeTest_7,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_7
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 8: Mixed zero/erased measurement partition is blank.",
    "SimpleComputeTest8",
    VarIntComputeTest_8,
    NULL,
    NULL,
    NULL
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 9: ReadyToBoot flushes deferred measurement.",
    "SimpleComputeTest9",
    VarIntComputeTest_9,
    NULL,
    NULL,
    NULL
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 10: ReadyToBoot reports deferred flush failure.",
    "SimpleComputeTest10",
    VarIntComputeTest_10,
    NULL,
    NULL,
    NULL
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 11: EBS deferred bootstrap does not flush.",
    "SimpleComputeTest11",
    VarIntComputeTest_11,
    NULL,
    NULL,
    NULL
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 12: EBS completed bootstrap only enters runtime mode.",
    "SimpleComputeTest12",
    VarIntComputeTest_12,
    NULL,
    NULL,
    NULL
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 13: Final bootstrap failure deadloops.",
    "SimpleComputeTest13",
    VarIntComputeTest_13,
    NULL,
    NULL,
    NULL
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 2: Compute/Store/Validate-fail due to invalid measurement",
    "SimpleComputeTest2",
    VarIntComputeTest_2,
    VarIntComputeTestSetup_2,
    VarIntComputeTestCleanup_2,
    &VarIntComputeTestData_2
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 3: 200 Compute/Store/Validate test block traverse logic",
    "SimpleComputeTest3",
    VarIntComputeTest_3,
    VarIntComputeTestSetup_3,
    VarIntComputeTestCleanup_3,
    &VarIntComputeTestData_3
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 4: 2000 Compute/Store/Validate test partition wrap around logic",
    "SimpleComputeTest3",
    VarIntComputeTest_4,
    VarIntComputeTestSetup_4,
    VarIntComputeTestCleanup_4,
    &VarIntComputeTestData_4
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 5: Compute/Store/Validate-fail due to failed OPTEE command",
    "SimpleComputeTest5",
    VarIntComputeTest_5,
    VarIntComputeTestSetup_5,
    VarIntComputeTestCleanup_5,
    &VarIntComputeTestData_5
    );

  // Execute the tests.
  Status = RunAllTestSuites (Fw);

ExitUnitTestingEntry:
  if (Fw) {
    FreeUnitTestFramework (Fw);
  }

  CleanUpTestData ();

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
