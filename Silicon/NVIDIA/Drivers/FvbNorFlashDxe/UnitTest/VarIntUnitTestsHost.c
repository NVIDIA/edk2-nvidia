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

#define BLOCK_SIZE          4096
#define TOTAL_BLOCKS        4
#define PARTITION_BLOCKS    4
#define MOCK_ATTRIBUTES     0xFF
#define MEAS_SZ             32
#define RECORD_HEADER_SIZE  1
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
STATIC NOR_FLASH_ERASE            OriginalNorFlashErase;
STATIC UINT32                     EraseFailureLba;
STATIC BOOLEAN                    EraseFailureArmed;
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
EFI_STATUS
EFIAPI
FailSelectedEraseOnce (
  IN NVIDIA_NOR_FLASH_PROTOCOL  *This,
  IN UINT32                     Lba,
  IN UINT32                     NumLba
  )
{
  if ((EraseFailureArmed == TRUE) && (Lba == EraseFailureLba)) {
    EraseFailureArmed = FALSE;
    return EFI_DEVICE_ERROR;
  }

  return OriginalNorFlashErase (This, Lba, NumLba);
}

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
  }
}

STATIC
VOID
MockSuccessfulFfaSignatures (
  IN ARM_SVC_ARGS  *Args,
  IN UINTN         Count
  )
{
  UINTN  Index;

  for (Index = 0; Index < Count; Index++) {
    MockFfaGetOpteeVmId (EFI_SUCCESS, (UINT16 *)(UINTN)0x8001);
    MockArmCallSvc (Args);
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
  UINT32                LegacyRecordSize;

  TestData         = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData    = (UINT8 *)VarIntProto->PartitionData;
  LegacyRecordSize = VarIntProto->MeasurementSize + 1;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  // Exercise migration after the circular log has touched every block.
  SetMem (PartitionData, VarIntProto->PartitionSize, 0);

  PartitionData[0] = VAR_INT_VALID;
  CopyMem (&PartitionData[1], TestData->ReadMeas, TestData->MeasSz);
  PartitionData[VarIntProto->MeasurementSize] = 0;

  //
  // A tentative V1 scan walks this legacy partition with a 33-byte stride.
  // Put V1-looking header bytes at three of those boundaries.  They are
  // ordinary payload bytes in the 34-byte V0 layout and must not prevent the
  // correctly strided V0 scan from finding the matching record above.
  //
  PartitionData[LegacyRecordSize]                 = VAR_INT_INVALID;
  PartitionData[LegacyRecordSize * 2]             = VAR_INT_INVALID;
  PartitionData[LegacyRecordSize * 3]             = VAR_INT_INVALID;
  PartitionData[VarIntProto->MeasurementSize * 2] = VAR_INT_V1_VALID;
  PartitionData[VarIntProto->MeasurementSize * 3] = VAR_INT_V1_VALID;
  PartitionData[VarIntProto->MeasurementSize * 4] = VAR_INT_V1_VALID;

  //
  // Leave a nonmatching live V0 record at the fallback candidate and put a
  // V1-looking byte in its payload.  The migration fallback may safely erase
  // and reuse this block, and its cached V0 scan result must not invalidate
  // the replacement written at the same offset.
  //
  PartitionData[BLOCK_SIZE] = VAR_INT_PENDING;
  SetMem (
    &PartitionData[BLOCK_SIZE + 1],
    VarIntProto->MeasurementSize,
    0xA5
    );
  PartitionData[BLOCK_SIZE + VarIntProto->MeasurementSize] = VAR_INT_V1_VALID;
  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

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
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], VAR_INT_V1_VALID);

  MockComputeVarMeasurement (
    NULL,
    V1MeasBuf,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], VAR_INT_V1_VALID);

  //
  // A byte inside the legacy layout can coincide with a canonical record
  // boundary and look like a live V0 header.  Runtime V1 updates must ignore
  // it after migration.
  //
  PartitionData[VarIntProto->MeasurementSize * 2] = VAR_INT_VALID;
  FlashDevice[VarIntProto->MeasurementSize * 2]   = VAR_INT_VALID;

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
                          TestData->VarName,
                          TestData->VarGuid,
                          EFI_SUCCESS
                          );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

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
 * VarIntComputeTest_14
 * "Simple Compute Test 14: False V1 headers do not authorize V0 migration."
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_14 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 V1MeasBuf[MEAS_SZ];
  UINT8                 *PartitionData;
  UINT32                LegacyRecordSize;

  TestData         = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData    = (UINT8 *)VarIntProto->PartitionData;
  LegacyRecordSize = VarIntProto->MeasurementSize + 1;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, 0);

  PartitionData[0] = VAR_INT_VALID;
  SetMem (&PartitionData[1], TestData->MeasSz, 0xA5);
  PartitionData[LegacyRecordSize]                 = VAR_INT_INVALID;
  PartitionData[LegacyRecordSize * 2]             = VAR_INT_INVALID;
  PartitionData[LegacyRecordSize * 3]             = VAR_INT_INVALID;
  PartitionData[VarIntProto->MeasurementSize * 2] = VAR_INT_V1_VALID;
  PartitionData[VarIntProto->MeasurementSize * 3] = VAR_INT_V1_VALID;
  PartitionData[VarIntProto->MeasurementSize * 4] = VAR_INT_V1_VALID;
  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

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
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);
  UT_ASSERT_EQUAL (PartitionData[0], VAR_INT_VALID);
  UT_ASSERT_EQUAL (FlashDevice[0], VAR_INT_VALID);

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_15
 * "Simple Compute Test 15: Overflow recovery relocates a matching V1 record."
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_15 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 *PartitionData;
  UINT32                RecordOffset;

  TestData      = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData = (UINT8 *)VarIntProto->PartitionData;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  PartitionData[0] = VAR_INT_V1_VALID;
  CopyMem (&PartitionData[1], TestData->VarMeas, TestData->MeasSz);
  for (RecordOffset = VarIntProto->MeasurementSize;
       RecordOffset <= (VarIntProto->MeasurementSize * 3);
       RecordOffset += VarIntProto->MeasurementSize)
  {
    PartitionData[RecordOffset] = VAR_INT_V1_VALID;
    SetMem (&PartitionData[RecordOffset + 1], TestData->MeasSz, 0xA5);
  }

  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], VAR_INT_V1_VALID);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

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
                          TestData->VarName,
                          TestData->VarGuid,
                          EFI_SUCCESS
                          );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_16
 * "Simple Compute Test 16: Overflow recovery resumes after relocation reset."
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_16 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 *PartitionData;
  UINT32                RecordOffset;

  TestData      = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData = (UINT8 *)VarIntProto->PartitionData;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  PartitionData[0] = VAR_INT_V1_VALID;
  CopyMem (&PartitionData[1], TestData->VarMeas, TestData->MeasSz);
  for (RecordOffset = VarIntProto->MeasurementSize;
       RecordOffset <= (VarIntProto->MeasurementSize * 3);
       RecordOffset += VarIntProto->MeasurementSize)
  {
    PartitionData[RecordOffset] = VAR_INT_V1_VALID;
    SetMem (&PartitionData[RecordOffset + 1], TestData->MeasSz, 0xA5);
  }

  //
  // Model reset after relocation wrote the matching replacement as pending,
  // but before the contaminated source block was erased.
  //
  PartitionData[BLOCK_SIZE] = VAR_INT_V1_PENDING;
  CopyMem (
    &PartitionData[BLOCK_SIZE + RECORD_HEADER_SIZE],
    TestData->VarMeas,
    TestData->MeasSz
    );
  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], VAR_INT_V1_VALID);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_18
 * "Simple Compute Test 18: Valid V1 resumes interrupted legacy cleanup."
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_18 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 *PartitionData;
  UINT32                LegacyRecordSize;

  TestData         = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData    = (UINT8 *)VarIntProto->PartitionData;
  LegacyRecordSize = VarIntProto->MeasurementSize + 1;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  //
  // Model reset or erase failure after the replacement V1 became valid but
  // before its matching V0 source and another legacy record were erased.  The
  // V1 scan does not overflow, so recovery must not depend on that condition.
  //
  PartitionData[0] = VAR_INT_VALID;
  CopyMem (
    &PartitionData[RECORD_HEADER_SIZE],
    TestData->VarMeas,
    TestData->MeasSz
    );
  PartitionData[LegacyRecordSize - 1] = 0;
  PartitionData[LegacyRecordSize]     = VAR_INT_VALID;
  SetMem (
    &PartitionData[LegacyRecordSize + RECORD_HEADER_SIZE],
    TestData->MeasSz,
    0x5A
    );
  PartitionData[BLOCK_SIZE] = VAR_INT_V1_VALID;
  CopyMem (&PartitionData[BLOCK_SIZE + RECORD_HEADER_SIZE], TestData->VarMeas, TestData->MeasSz);
  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
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
  UT_ASSERT_EQUAL (PartitionData[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[LegacyRecordSize], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[LegacyRecordSize], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], VAR_INT_V1_VALID);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], VAR_INT_V1_VALID);

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_20
 * "A matching V1 sharing a block with V0 is relocated before cleanup."
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_20 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 *PartitionData;
  UINT32                V0RecordSize;

  TestData      = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData = (UINT8 *)VarIntProto->PartitionData;
  V0RecordSize  = VarIntProto->MeasurementSize + 1;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  PartitionData[0] = VAR_INT_V1_VALID;
  CopyMem (
    &PartitionData[RECORD_HEADER_SIZE],
    TestData->VarMeas,
    TestData->MeasSz
    );
  PartitionData[V0RecordSize] = VAR_INT_VALID;
  CopyMem (
    &PartitionData[V0RecordSize + RECORD_HEADER_SIZE],
    TestData->VarMeas,
    TestData->MeasSz
    );
  PartitionData[(V0RecordSize * 2) - 1] = 0;
  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
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
  UT_ASSERT_EQUAL (PartitionData[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_MEM_EQUAL (
    &PartitionData[BLOCK_SIZE + RECORD_HEADER_SIZE],
    TestData->VarMeas,
    TestData->MeasSz
    );

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
    TestData->MeasSz,
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], VAR_INT_V1_VALID);

  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_21
 * "Resumed cleanup retains the matching V0 source until other erases succeed."
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_21 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 *PartitionData;

  TestData      = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData = (UINT8 *)VarIntProto->PartitionData;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  //
  // Model a resumed migration with the matching V0 source in block 0, the
  // durable V1 replacement in block 1, and stale V0 data in blocks 2 and 3.
  // A failure erasing block 3 must not erase the matching source in block 0.
  //
  PartitionData[0] = VAR_INT_VALID;
  CopyMem (&PartitionData[RECORD_HEADER_SIZE], TestData->VarMeas, TestData->MeasSz);
  PartitionData[VarIntProto->MeasurementSize] = 0;
  PartitionData[BLOCK_SIZE]                   = VAR_INT_V1_VALID;
  CopyMem (
    &PartitionData[BLOCK_SIZE + RECORD_HEADER_SIZE],
    TestData->VarMeas,
    TestData->MeasSz
    );
  PartitionData[BLOCK_SIZE * 2] = VAR_INT_VALID;
  SetMem (&PartitionData[(BLOCK_SIZE * 2) + RECORD_HEADER_SIZE], TestData->MeasSz, 0x5A);
  PartitionData[BLOCK_SIZE * 3] = VAR_INT_VALID;
  SetMem (&PartitionData[(BLOCK_SIZE * 3) + RECORD_HEADER_SIZE], TestData->MeasSz, 0xA5);
  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
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

  OriginalNorFlashErase = NorFlashStub->Erase;
  EraseFailureLba       = 3;
  EraseFailureArmed     = TRUE;
  NorFlashStub->Erase   = FailSelectedEraseOnce;

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);
  UT_ASSERT_EQUAL (PartitionData[0], VAR_INT_VALID);
  UT_ASSERT_EQUAL (FlashDevice[0], VAR_INT_VALID);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE * 2], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE * 2], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE * 3], VAR_INT_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE * 3], VAR_INT_VALID);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
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
  UT_ASSERT_EQUAL (PartitionData[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE * 3], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE * 3], FVB_ERASED_BYTE);

  NorFlashStub->Erase   = OriginalNorFlashErase;
  OriginalNorFlashErase = NULL;
  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_22
 * "Relocation preserves the matching V0 source until cleanup completes."
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_22 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 *PartitionData;
  UINT32                RecordOffset;

  TestData      = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData = (UINT8 *)VarIntProto->PartitionData;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  PartitionData[0] = VAR_INT_V1_VALID;
  CopyMem (&PartitionData[RECORD_HEADER_SIZE], TestData->VarMeas, TestData->MeasSz);
  for (RecordOffset = VarIntProto->MeasurementSize;
       RecordOffset <= (VarIntProto->MeasurementSize * 3);
       RecordOffset += VarIntProto->MeasurementSize)
  {
    PartitionData[RecordOffset] = VAR_INT_V1_VALID;
    SetMem (&PartitionData[RecordOffset + RECORD_HEADER_SIZE], TestData->MeasSz, 0x3C);
  }

  PartitionData[BLOCK_SIZE] = VAR_INT_VALID;
  CopyMem (
    &PartitionData[BLOCK_SIZE + RECORD_HEADER_SIZE],
    TestData->VarMeas,
    TestData->MeasSz
    );
  PartitionData[BLOCK_SIZE + VarIntProto->MeasurementSize] = 0;
  PartitionData[BLOCK_SIZE * 2]                            = VAR_INT_VALID;
  SetMem (&PartitionData[(BLOCK_SIZE * 2) + RECORD_HEADER_SIZE], TestData->MeasSz, 0x5A);
  PartitionData[BLOCK_SIZE * 3] = VAR_INT_VALID;
  SetMem (&PartitionData[(BLOCK_SIZE * 3) + RECORD_HEADER_SIZE], TestData->MeasSz, 0xA5);
  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
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

  OriginalNorFlashErase = NorFlashStub->Erase;
  EraseFailureLba       = 3;
  EraseFailureArmed     = TRUE;
  NorFlashStub->Erase   = FailSelectedEraseOnce;

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], VAR_INT_VALID);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE * 2], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE * 2], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE * 3], VAR_INT_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE * 3], VAR_INT_VALID);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
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
  UT_ASSERT_EQUAL (PartitionData[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[0], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE * 2], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE * 2], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE * 3], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (FlashDevice[BLOCK_SIZE * 3], FVB_ERASED_BYTE);

  NorFlashStub->Erase   = OriginalNorFlashErase;
  OriginalNorFlashErase = NULL;
  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_23
 * "Relocation fails safely when no unprotected destination exists."
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_23 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 *PartitionData;
  UINT32                RecordOffset;

  TestData      = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData = (UINT8 *)VarIntProto->PartitionData;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  PartitionData[0] = VAR_INT_V1_VALID;
  CopyMem (&PartitionData[RECORD_HEADER_SIZE], TestData->VarMeas, TestData->MeasSz);
  for (RecordOffset = VarIntProto->MeasurementSize;
       RecordOffset <= (VarIntProto->MeasurementSize * 3);
       RecordOffset += VarIntProto->MeasurementSize)
  {
    PartitionData[RecordOffset] = VAR_INT_V1_VALID;
    SetMem (&PartitionData[RecordOffset + RECORD_HEADER_SIZE], TestData->MeasSz, 0x3C);
  }

  PartitionData[BLOCK_SIZE] = VAR_INT_VALID;
  CopyMem (
    &PartitionData[BLOCK_SIZE + RECORD_HEADER_SIZE],
    TestData->VarMeas,
    TestData->MeasSz
    );
  PartitionData[BLOCK_SIZE + VarIntProto->MeasurementSize] = 0;
  PartitionData[BLOCK_SIZE * 2]                            = VAR_INT_V1_VALID;
  SetMem (&PartitionData[(BLOCK_SIZE * 2) + RECORD_HEADER_SIZE], TestData->MeasSz, 0x5A);
  PartitionData[BLOCK_SIZE * 3] = VAR_INT_V1_VALID;
  SetMem (&PartitionData[(BLOCK_SIZE * 3) + RECORD_HEADER_SIZE], TestData->MeasSz, 0xA5);
  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

  MockComputeVarMeasurement (
    NULL,
    TestData->VarMeas,
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
  UT_ASSERT_STATUS_EQUAL (Status, EFI_OUT_OF_RESOURCES);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_VALID);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE * 2], VAR_INT_V1_VALID);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE * 3], VAR_INT_V1_VALID);
  UT_ASSERT_MEM_EQUAL (PartitionData, FlashDevice, VarIntProto->PartitionSize);

  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_19
 * "Simple Compute Test 19: V1 append skips a partially programmed slot."
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_19 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 NewMeasBuf[MEAS_SZ];
  UINT8                 *PartitionData;
  UINT32                DirtySlotOffset;
  UINT32                NewRecordOffset;

  TestData        = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData   = (UINT8 *)VarIntProto->PartitionData;
  DirtySlotOffset = VarIntProto->MeasurementSize;
  NewRecordOffset = VarIntProto->MeasurementSize * 2;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);
  SetMem (NewMeasBuf, sizeof (NewMeasBuf), 0x3C);

  PartitionData[0] = VAR_INT_V1_VALID;
  CopyMem (
    &PartitionData[RECORD_HEADER_SIZE],
    TestData->VarMeas,
    TestData->MeasSz
    );
  SetMem (
    &PartitionData[DirtySlotOffset + RECORD_HEADER_SIZE],
    TestData->MeasSz,
    0x5A
    );
  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

  MockComputeVarMeasurement (
    TestData->VarName,
    NewMeasBuf,
    sizeof (NewMeasBuf),
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
  UT_ASSERT_EQUAL (PartitionData[DirtySlotOffset], FVB_ERASED_BYTE);
  UT_ASSERT_EQUAL (PartitionData[NewRecordOffset], VAR_INT_V1_PENDING);
  UT_ASSERT_MEM_EQUAL (PartitionData, FlashDevice, VarIntProto->PartitionSize);

  Status = VarIntProto->InvalidateLast (
                          VarIntProto,
                          TestData->VarName,
                          TestData->VarGuid,
                          EFI_SUCCESS
                          );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[NewRecordOffset], VAR_INT_V1_VALID);
  UT_ASSERT_MEM_EQUAL (PartitionData, FlashDevice, VarIntProto->PartitionSize);

  MockComputeVarMeasurement (
    NULL,
    NewMeasBuf,
    sizeof (NewMeasBuf),
    TestData->ComputeReturnStatus
    );
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[NewRecordOffset], VAR_INT_V1_VALID);
  UT_ASSERT_MEM_EQUAL (PartitionData, FlashDevice, VarIntProto->PartitionSize);

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

  return UNIT_TEST_PASSED;
}

/*
 * VarIntComputeTest_17
 * "Simple Compute Test 17: FF-A V0 signing preserves the migrated V1 payload."
 *
 */
STATIC
UNIT_TEST_STATUS
EFIAPI
VarIntComputeTest_17 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  VAR_INT_TEST_CONTEXT  *TestData;
  UINT8                 V0MeasBuf[MEAS_SZ];
  UINT8                 V1MeasBuf[MEAS_SZ];
  UINT8                 *PartitionData;

  TestData      = (VAR_INT_TEST_CONTEXT *)Context;
  PartitionData = (UINT8 *)VarIntProto->PartitionData;

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);
  SetMem (V0MeasBuf, sizeof (V0MeasBuf), 0x8);
  SetMem (V1MeasBuf, sizeof (V1MeasBuf), 0x4);

  PartitionData[0] = VAR_INT_VALID;
  CopyMem (&PartitionData[RECORD_HEADER_SIZE], V0MeasBuf, sizeof (V0MeasBuf));
  CopyMem (FlashDevice, PartitionData, VarIntProto->PartitionSize);

  TestData->TestArgs->Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_RESP;
  TestData->TestArgs->Arg3 = FFA_OK;
  MockComputeVarMeasurement (
    NULL,
    V1MeasBuf,
    sizeof (V1MeasBuf),
    TestData->ComputeReturnStatus
    );
  MockComputeVarMeasurementV0 (
    NULL,
    V0MeasBuf,
    sizeof (V0MeasBuf),
    TestData->ComputeReturnStatus
    );
  MockSuccessfulFfaSignatures (TestData->TestArgs, 2);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (PartitionData[BLOCK_SIZE], VAR_INT_V1_VALID);
  UT_ASSERT_MEM_EQUAL (
    &PartitionData[BLOCK_SIZE + RECORD_HEADER_SIZE],
    V1MeasBuf,
    sizeof (V1MeasBuf)
    );

  MockComputeVarMeasurement (
    NULL,
    V1MeasBuf,
    sizeof (V1MeasBuf),
    TestData->ComputeReturnStatus
    );
  MockSuccessfulFfaSignatures (TestData->TestArgs, 1);

  Status = VarIntProto->Validate (VarIntProto);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  Status = NorFlashStub->Erase (NorFlashStub, 0, TOTAL_BLOCKS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  SetMem (PartitionData, VarIntProto->PartitionSize, FVB_ERASED_BYTE);

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

  if (TestData->TestArgs != NULL) {
    FreePool (TestData->TestArgs);
    TestData->TestArgs = NULL;
  }

  if (OriginalNorFlashErase != NULL) {
    NorFlashStub->Erase   = OriginalNorFlashErase;
    OriginalNorFlashErase = NULL;
  }

  EraseFailureArmed = FALSE;
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
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);
  MockSuccessfulOpteeSignatures (TestData->TestArgs, 1);

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
  if (TestData->TestArgs != NULL) {
    FreePool (TestData->TestArgs);
    TestData->TestArgs = NULL;
  }
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
  if (TestData->TestArgs != NULL) {
    FreePool (TestData->TestArgs);
    TestData->TestArgs = NULL;
  }
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
  if (TestData->TestArgs != NULL) {
    FreePool (TestData->TestArgs);
    TestData->TestArgs = NULL;
  }
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
  if (TestData->TestArgs != NULL) {
    FreePool (TestData->TestArgs);
    TestData->TestArgs = NULL;
  }
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
InitFfaSuiteTestData (
  VOID
  )
{
  EFI_STATUS  Status;

  Handle  = AllocatePool (sizeof (UINT32));
  *Handle = 0xABCD;
  MockMmInstallProtocolInterface (
    &gNVIDIAVarIntGuid,
    (EFI_HANDLE)Handle,
    EFI_SUCCESS
    );
  MockIsOpteePresent (FALSE);
  MockFfaGetOpteeVmId (EFI_SUCCESS, (UINT16 *)(UINTN)0x8001);
  MockFfaGetMmVmId (EFI_SUCCESS, (UINT16 *)(UINTN)0x8002);
  MockFfaGetTxRxBuffer (
    EFI_SUCCESS,
    (UINT64 *)(UINTN)0x1000,
    (UINT32 *)(UINTN)EFI_PAGE_SIZE,
    (UINT64 *)(UINTN)0x2000,
    (UINT32 *)(UINTN)EFI_PAGE_SIZE
    );
  MockPrepareFfaMemoryDescriptor (EFI_SUCCESS, (UINT32 *)(UINTN)0x100);
  MockFfaSendShareCommand (EFI_SUCCESS, (UINT64 *)(UINTN)0x1234);

  Status = VarIntInit (
             ResPartitionOffset,
             ResPartitionSize,
             NorFlashStub,
             &NorFlashAttr
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Initialize FF-A VarInt module %r\n", Status));
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
  UNIT_TEST_SUITE_HANDLE      VarIntFfaSuite;

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

  Status = CreateUnitTestSuite (
             &VarIntFfaSuite,
             Fw,
             "VarInt FF-A Tests",
             "VarInt.VarIntFfaTestSuite",
             InitFfaSuiteTestData,
             CleanupSuiteTestData
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for VarInt FF-A\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitUnitTestingEntry;
  }

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 14: False V1 headers do not authorize V0 migration.",
    "SimpleComputeTest14",
    VarIntComputeTest_14,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 15: Overflow recovery relocates a matching V1 record.",
    "SimpleComputeTest15",
    VarIntComputeTest_15,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 16: Overflow recovery resumes after relocation reset.",
    "SimpleComputeTest16",
    VarIntComputeTest_16,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 18: Valid V1 resumes interrupted legacy cleanup.",
    "SimpleComputeTest18",
    VarIntComputeTest_18,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 20: Mixed V0/V1 block is relocated before cleanup.",
    "SimpleComputeTest20",
    VarIntComputeTest_20,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 21: Resumed cleanup erases the matching V0 source last.",
    "SimpleComputeTest21",
    VarIntComputeTest_21,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 22: Relocation preserves the matching V0 source.",
    "SimpleComputeTest22",
    VarIntComputeTest_22,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 23: Relocation fails safely without a destination.",
    "SimpleComputeTest23",
    VarIntComputeTest_23,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
    );

  AddTestCase (
    VarIntComputeSuite,
    "Simple Compute Test 19: V1 append skips a partially programmed slot.",
    "SimpleComputeTest19",
    VarIntComputeTest_19,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
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

  AddTestCase (
    VarIntFfaSuite,
    "Simple Compute Test 17: FF-A V0 signing preserves the migrated V1 payload.",
    "SimpleComputeTest17",
    VarIntComputeTest_17,
    VarIntComputeTestSetup_1,
    VarIntComputeTestCleanup_1,
    &VarIntComputeTestData_6
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
