/** @file

  Host-based unit tests for BrBctUpdateDeviceLib.

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
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/BrBctUpdateDeviceLib.h>
#include <Library/MemoryAllocationLib.h>
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>

#define UNIT_TEST_NAME     "BrBctUpdateDeviceLib Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

STATIC FW_PARTITION_DEVICE_INFO   mBctDeviceInfo;
STATIC FW_PARTITION_DEVICE_INFO   mBackupDeviceInfo;
STATIC FW_PARTITION_PRIVATE_DATA  mBctPartition;
STATIC FW_PARTITION_PRIVATE_DATA  mBackupPartition;
STATIC UINT32                     mUnitTestPcdBrBctDataSize;
STATIC UINT32                     mUnitTestPcdBrBctLogicalSlotSize;
STATIC BOOLEAN                    mUnitTestPcdBrBctVerifyUpdateBeforeWrite;
STATIC BOOLEAN                    mUnitTestPcdFwImageEnableBPartitions;
STATIC BOOLEAN                    mUnitTestPcdOverwriteActiveFwPartition;
STATIC BOOLEAN                    mUnitTestPcdBootChainIsMarkerBased;
STATIC UINTN                      mReadCallCount;
STATIC UINT64                     mLastReadOffset;
STATIC UINTN                      mLastReadBytes;
STATIC EFI_STATUS                 mReadStatus;
STATIC UINTN                      mWriteCallCount;
STATIC UINTN                      mNextBootChainCallCount;
STATIC UINT32                     mNextBootChain;

STATIC
BOOLEAN
UnitTestPcdGetBool_PcdBrBctVerifyUpdateBeforeWrite (
  VOID
  )
{
  return mUnitTestPcdBrBctVerifyUpdateBeforeWrite;
}

STATIC
BOOLEAN
UnitTestPcdGetBool_PcdFwImageEnableBPartitions (
  VOID
  )
{
  return mUnitTestPcdFwImageEnableBPartitions;
}

STATIC
BOOLEAN
UnitTestPcdGetBool_PcdOverwriteActiveFwPartition (
  VOID
  )
{
  return mUnitTestPcdOverwriteActiveFwPartition;
}

STATIC
BOOLEAN
UnitTestPcdGetBool_PcdBootChainIsMarkerBased (
  VOID
  )
{
  return mUnitTestPcdBootChainIsMarkerBased;
}

STATIC
UINT32
UnitTestPcdGet32_PcdBrBctDataSize (
  VOID
  )
{
  return mUnitTestPcdBrBctDataSize;
}

STATIC
UINT32
UnitTestPcdGet32_PcdBrBctLogicalSlotSize (
  VOID
  )
{
  return mUnitTestPcdBrBctLogicalSlotSize;
}

#undef PcdGetBool
#define PcdGetBool(TokenName)  UnitTestPcdGetBool_##TokenName ()
#undef PcdGet32
#define PcdGet32(TokenName)  UnitTestPcdGet32_##TokenName ()
#undef STATIC
#define STATIC
#include "../BrBctUpdateDeviceLib.c"
#undef STATIC
#define STATIC  static
#undef PcdGetBool
#undef PcdGet32

STATIC
EFI_STATUS
EFIAPI
TestPartitionRead (
  IN  NVIDIA_FW_PARTITION_PROTOCOL  *This,
  IN  UINT64                        Offset,
  IN  UINTN                         Bytes,
  OUT VOID                          *Buffer
  )
{
  mReadCallCount++;
  mLastReadOffset = Offset;
  mLastReadBytes  = Bytes;

  if (EFI_ERROR (mReadStatus)) {
    return mReadStatus;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  SetMem (Buffer, Bytes, 0xA5);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
TestPartitionWrite (
  IN  NVIDIA_FW_PARTITION_PROTOCOL  *This,
  IN  UINT64                        Offset,
  IN  UINTN                         Bytes,
  IN  CONST VOID                    *Buffer
  )
{
  mWriteCallCount++;

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
InitPartition (
  IN OUT FW_PARTITION_PRIVATE_DATA  *Partition,
  IN OUT FW_PARTITION_DEVICE_INFO   *DeviceInfo,
  IN     CONST CHAR16               *Name
  )
{
  ZeroMem (Partition, sizeof (*Partition));
  ZeroMem (DeviceInfo, sizeof (*DeviceInfo));

  Partition->Signature              = FW_PARTITION_PRIVATE_DATA_SIGNATURE;
  Partition->DeviceInfo             = DeviceInfo;
  Partition->PartitionInfo.Bytes    = 0x1000;
  Partition->Protocol.PartitionName = Partition->PartitionInfo.Name;
  Partition->Protocol.Read          = TestPartitionRead;
  Partition->Protocol.Write         = TestPartitionWrite;
  DeviceInfo->BlockSize             = 0x100;
  DeviceInfo->EraseBlockSize        = 0x100;
  StrCpyS (Partition->PartitionInfo.Name, FW_PARTITION_NAME_LENGTH, Name);
}

FW_PARTITION_PRIVATE_DATA *
EFIAPI
FwPartitionFindByName (
  IN CONST CHAR16  *Name
  )
{
  if (StrCmp (Name, L"BCT") == 0) {
    return &mBctPartition;
  }

  if (StrCmp (Name, BR_BCT_BACKUP_PARTITION_NAME) == 0) {
    return &mBackupPartition;
  }

  return NULL;
}

EFI_STATUS
EFIAPI
SetNextBootChain (
  IN UINT32  BootChain
  )
{
  mNextBootChainCallCount++;
  mNextBootChain = BootChain;

  return EFI_SUCCESS;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BrBctUpdateDeviceLibDeinit ();

  InitPartition (&mBctPartition, &mBctDeviceInfo, L"BCT");
  InitPartition (
    &mBackupPartition,
    &mBackupDeviceInfo,
    BR_BCT_BACKUP_PARTITION_NAME
    );

  mUnitTestPcdBrBctDataSize                = 0x100;
  mUnitTestPcdBrBctLogicalSlotSize         = 0x100;
  mUnitTestPcdBrBctVerifyUpdateBeforeWrite = FALSE;
  mUnitTestPcdFwImageEnableBPartitions     = TRUE;
  mUnitTestPcdOverwriteActiveFwPartition   = FALSE;
  mUnitTestPcdBootChainIsMarkerBased       = FALSE;
  mReadCallCount                           = 0;
  mLastReadOffset                          = 0;
  mLastReadBytes                           = 0;
  mReadStatus                              = EFI_SUCCESS;
  mWriteCallCount                          = 0;
  mNextBootChainCallCount                  = 0;
  mNextBootChain                           = MAX_UINT32;

  return UNIT_TEST_PASSED;
}

STATIC
VOID
EFIAPI
TestTeardown (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BrBctUpdateDeviceLibDeinit ();
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestInitRejectsInvalidActiveBootChain (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_ASSERT_STATUS_EQUAL (
    BrBctUpdateDeviceLibInit (BOOT_CHAIN_COUNT, 0x100),
    EFI_INVALID_PARAMETER
    );
  UT_ASSERT_EQUAL (mReadCallCount, 0);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestInitRejectsZeroSlotSize (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mUnitTestPcdBrBctLogicalSlotSize = 0;

  UT_ASSERT_STATUS_EQUAL (
    BrBctUpdateDeviceLibInit (BOOT_CHAIN_A, 0),
    EFI_INVALID_PARAMETER
    );
  UT_ASSERT_EQUAL ((UINTN)mVerifyBuffer, (UINTN)NULL);
  UT_ASSERT_EQUAL ((UINTN)mBackupPartitionBuffer, (UINTN)NULL);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestUpdateFwChainRejectsInvalidNewChain (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA  *Private;

  UT_ASSERT_NOT_EFI_ERROR (BrBctUpdateDeviceLibInit (BOOT_CHAIN_A, 0x100));
  Private = BrBctUpdateGetPrivate ();

  UT_ASSERT_STATUS_EQUAL (
    Private->Protocol.UpdateFwChain (&Private->Protocol, BOOT_CHAIN_COUNT),
    EFI_INVALID_PARAMETER
    );
  UT_ASSERT_EQUAL (mReadCallCount, 0);
  UT_ASSERT_EQUAL (mWriteCallCount, 0);
  UT_ASSERT_EQUAL (mNextBootChainCallCount, 0);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestUpdateBackupRejectsInvalidActiveChain (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA  *Private;
  UINT8                       BackupData[0x1000];

  mUnitTestPcdFwImageEnableBPartitions   = FALSE;
  mUnitTestPcdOverwriteActiveFwPartition = TRUE;

  UT_ASSERT_NOT_EFI_ERROR (BrBctUpdateDeviceLibInit (BOOT_CHAIN_A, 0x100));
  Private          = BrBctUpdateGetPrivate ();
  mActiveBootChain = MAX_UINT32;
  SetMem (BackupData, sizeof (BackupData), 0x5A);

  UT_ASSERT_STATUS_EQUAL (
    Private->Protocol.UpdateBackupPartition (&Private->Protocol, BackupData),
    EFI_INVALID_PARAMETER
    );
  UT_ASSERT_EQUAL (mReadCallCount, 0);
  UT_ASSERT_EQUAL (mWriteCallCount, 0);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestVerifySlotRoundsReadToBlockSize (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA  *Private;
  UINT8                       ExpectedData[0x20];

  UT_ASSERT_NOT_EFI_ERROR (BrBctUpdateDeviceLibInit (BOOT_CHAIN_A, 0x100));
  Private = BrBctUpdateGetPrivate ();
  SetMem (ExpectedData, sizeof (ExpectedData), 0xA5);

  UT_ASSERT_NOT_EFI_ERROR (
    BrBctVerifySlot (Private, &mBctPartition, 1, sizeof (ExpectedData), ExpectedData)
    );
  UT_ASSERT_EQUAL (mReadCallCount, 1);
  UT_ASSERT_EQUAL (mLastReadOffset, 0x100);
  UT_ASSERT_EQUAL (mLastReadBytes, 0x100);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestVerifySlotRejectsZeroBlockSize (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA  *Private;
  UINT8                       ExpectedData[0x20];

  UT_ASSERT_NOT_EFI_ERROR (BrBctUpdateDeviceLibInit (BOOT_CHAIN_A, 0x100));
  Private                  = BrBctUpdateGetPrivate ();
  mBctDeviceInfo.BlockSize = 0;

  UT_ASSERT_STATUS_EQUAL (
    BrBctVerifySlot (Private, &mBctPartition, 0, sizeof (ExpectedData), ExpectedData),
    EFI_INVALID_PARAMETER
    );
  UT_ASSERT_EQUAL (mReadCallCount, 0);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestVerifySlotRejectsAlignmentOverflow (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA  *Private;
  UINT8                       ExpectedData[0x20];

  UT_ASSERT_NOT_EFI_ERROR (BrBctUpdateDeviceLibInit (BOOT_CHAIN_A, 0x100));
  Private                  = BrBctUpdateGetPrivate ();
  mBctDeviceInfo.BlockSize = 4;

  UT_ASSERT_STATUS_EQUAL (
    BrBctVerifySlot (
      Private,
      &mBctPartition,
      0,
      (UINTN)(MAX_UINTN - 1),
      ExpectedData
      ),
    EFI_INVALID_PARAMETER
    );
  UT_ASSERT_EQUAL (mReadCallCount, 0);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestDeinitReleasesMarkerInvalidateBuffer (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mUnitTestPcdBootChainIsMarkerBased = TRUE;

  UT_ASSERT_NOT_EFI_ERROR (BrBctUpdateDeviceLibInit (BOOT_CHAIN_A, 0x100));
  UT_ASSERT_NOT_EQUAL ((UINTN)mInvalidateBuffer, (UINTN)NULL);

  BrBctUpdateDeviceLibDeinit ();

  UT_ASSERT_EQUAL ((UINTN)mInvalidateBuffer, (UINTN)NULL);
  UT_ASSERT_EQUAL ((UINTN)mVerifyBuffer, (UINTN)NULL);
  UT_ASSERT_EQUAL ((UINTN)mBackupPartitionBuffer, (UINTN)NULL);

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
             "BR-BCT update validation",
             "BrBctUpdateDeviceLib.Validation",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (
    Suite,
    "Init rejects invalid active boot chain",
    "InitInvalidActiveChain",
    TestInitRejectsInvalidActiveBootChain,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Init rejects zero slot size",
    "InitZeroSlotSize",
    TestInitRejectsZeroSlotSize,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "UpdateFwChain rejects invalid new chain",
    "UpdateFwChainInvalidNewChain",
    TestUpdateFwChainRejectsInvalidNewChain,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "UpdateBackupPartition rejects invalid active chain",
    "UpdateBackupInvalidActiveChain",
    TestUpdateBackupRejectsInvalidActiveChain,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Verify slot rounds read to block size",
    "VerifySlotRoundsRead",
    TestVerifySlotRoundsReadToBlockSize,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Verify slot rejects zero block size",
    "VerifySlotZeroBlockSize",
    TestVerifySlotRejectsZeroBlockSize,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Verify slot rejects alignment overflow",
    "VerifySlotAlignmentOverflow",
    TestVerifySlotRejectsAlignmentOverflow,
    TestSetup,
    TestTeardown,
    NULL
    );
  AddTestCase (
    Suite,
    "Deinit releases marker invalidation buffer",
    "DeinitMarkerInvalidateBuffer",
    TestDeinitReleasesMarkerInvalidateBuffer,
    TestSetup,
    TestTeardown,
    NULL
    );

  Status = RunAllTestSuites (Framework);

Done:
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
