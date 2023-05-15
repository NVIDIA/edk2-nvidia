/** @file
  Unit tests of the Boot Image header for the AndroidBootDxe Driver.

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <AndroidBootDxeUnitTestPrivate.h>

/**
  Media with a very large partition.
 */
EFI_BLOCK_IO_MEDIA  Media_Large = {
  .LastBlock = 0x8000,
  .BlockSize = 0x1000
};

/**
  Media with a small partition.
 */
EFI_BLOCK_IO_MEDIA  Media_Small = {
  .LastBlock = 0x0010,
  .BlockSize = 0x1000
};

/**
  Boot.img header:
  Valid type0
 */
ANDROID_BOOTIMG_TYPE0_HEADER  Hdr_Type0_Valid = {
  .BootMagic      = { 'A', 'N', 'D', 'R', 'O', 'I', 'D', '!' },
  .KernelSize     = 0x42000,
  .KernelAddress  = 0x4000,
  .RamdiskSize    = 0x64000,
  .RamdiskAddress = 0x88000,
  .PageSize       = 0x1000,
  .ProductName    = { 'E', 'V', 'E', '!' },
  .KernelArgs     = { 'B', 'O', 'O', 'M' }
};

/**
  Expected ANDROID_BOOT_DATA:
  Valid type0
 */
ANDROID_BOOT_DATA  ExpectedImgData_Type0_Valid = {
  .Offset      = 0,
  .KernelSize  = 0x42000,
  .RamdiskSize = 0x64000,
  .PageSize    = 0x1000
};

/**
  Expected ANDROID_BOOT_DATA:
  Valid type0 after signature
 */
ANDROID_BOOT_DATA  ExpectedImgData_Sig_Type0_Valid = {
  .Offset      = 0x1000,
  .KernelSize  = 0x42000,
  .RamdiskSize = 0x64000,
  .PageSize    = 0x1000
};

/**
 Expected KernelArgs:
 Valid type0
 */
CHAR16  ExpectedKernelArgs_Type0_Valid[] = { 'B', 'O', 'O', 'M', 0 };

/**
  Boot.img header:
  Signature, which is anything that's not a valid boot.img header.
 */
ANDROID_BOOTIMG_VERSION_HEADER  Hdr_Sig = {
  .BootMagic = { 'N', 'O', 'T', 'D', 'R', 'O', 'I', 'D' },
};

/**
  Boot.img header:
  Invalid with the wrong magic.
 */
ANDROID_BOOTIMG_VERSION_HEADER  Hdr_Invalid_Magic = {
  .BootMagic = { 'I', 'N', 'V', 'A', 'L', 'I', 'D', '!' },
};

/**
  Boot.img header:
  Invalid with a bad page size.
 */
ANDROID_BOOTIMG_TYPE0_HEADER  Hdr_Invalid_PageSize = {
  .BootMagic = { 'A', 'N', 'D', 'R', 'O', 'I', 'D', '!' },
  .PageSize  = 0x0010,
};

/**
  Boot.img header:
  Invalid type
 */
ANDROID_BOOTIMG_VERSION_HEADER  Hdr_Invalid_Version = {
  .BootMagic     = { 'A', 'N', 'D', 'R', 'O', 'I', 'D', '!' },
  .HeaderVersion = 0x42
};

/**
  Test Plan for AndroidBootRead:
  Read a signature with DiskIo protocol.
 */
TEST_PLAN_ANDROID_BOOT_READ  ABR_Sig_DiskIo = {
  .WithDiskIo     = TRUE,
  .ReadReturn     = EFI_SUCCESS,
  .ReadBuffer     = &Hdr_Sig,
  .ExpectedOffset = 0
};

/**
  Test Plan for AndroidBootRead:
  Read a Type0 header from DiskIo protocol.
 */
TEST_PLAN_ANDROID_BOOT_READ  ABR_Type0_DiskIo = {
  .WithDiskIo     = TRUE,
  .ReadReturn     = EFI_SUCCESS,
  .ReadBuffer     = &Hdr_Type0_Valid,
  .ExpectedOffset = 0
};

/**
  Test Plan for AndroidBootRead:
  Fail fead after sig
 */
TEST_PLAN_ANDROID_BOOT_READ  ABR_Failure = {
  .WithDiskIo     = TRUE,
  .ReadReturn     = EFI_ACCESS_DENIED,
  .ReadBuffer     = &Hdr_Type0_Valid,
  .ExpectedOffset = 0
};

/**
  Test Plan for AndroidBootRead:
  Read a Type0 header from DiskIo protocol.
 */
TEST_PLAN_ANDROID_BOOT_READ  ABR_Type0_DiskIo_After_Sig = {
  .WithDiskIo     = TRUE,
  .ReadReturn     = EFI_SUCCESS,
  .ReadBuffer     = &Hdr_Type0_Valid,
  .ExpectedOffset = 0x1000  // Must be value of PcdSignedImageHeaderSize
};

/**
  Test Plan for AndroidBootRead:
  Fail read after sig
 */
TEST_PLAN_ANDROID_BOOT_READ  ABR_Failure_After_Sig = {
  .WithDiskIo     = TRUE,
  .ReadReturn     = EFI_ACCESS_DENIED,
  .ReadBuffer     = &Hdr_Type0_Valid,
  .ExpectedOffset = 0x1000  // Must be value of PcdSignedImageHeaderSize
};

/**
  Test Plan for AndroidBootRead:
  Invalid magic after sig
 */
TEST_PLAN_ANDROID_BOOT_READ  ABR_Invalid_After_Sig = {
  .WithDiskIo     = TRUE,
  .ReadReturn     = EFI_SUCCESS,
  .ReadBuffer     = &Hdr_Invalid_Magic,
  .ExpectedOffset = 0x1000  // Must be value of PcdSignedImageHeaderSize
};

/**
  Test Plan for AndroidBootRead:
  Invalid page size.
 */
TEST_PLAN_ANDROID_BOOT_READ  ABR_Invalid_PageSize = {
  .WithDiskIo     = TRUE,
  .ReadReturn     = EFI_SUCCESS,
  .ReadBuffer     = &Hdr_Invalid_PageSize,
  .ExpectedOffset = 0
};

/**
  Test Plan for AndroidBootRead:
  Invalid header version
 */
TEST_PLAN_ANDROID_BOOT_READ  ABR_Invalid_Version = {
  .WithDiskIo     = TRUE,
  .ReadReturn     = EFI_SUCCESS,
  .ReadBuffer     = &Hdr_Invalid_Version,
  .ExpectedOffset = 0
};

/**
  Test Plan for AndroidBootRead:
  Read a Type0 header from RCM.
 */
TEST_PLAN_ANDROID_BOOT_READ  ABR_Type0_Rcm = {
  .ReadBuffer     = &Hdr_Type0_Valid,
  .ExpectedOffset = 0
};

/**
  Test Plan for AndroidBootRead:
  No header
 */
TEST_PLAN_ANDROID_BOOT_READ  ABR_No_Header = {
  .ReadBuffer     = NULL,
  .ExpectedOffset = 0
};

/**
  Test Plan for AndroidBootGetVerify:
  Read a Type0 header from Disk.
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Type0_Disk_Valid = {
  .WithBlockIo      = TRUE,
  .WithImgData      = TRUE,
  .WithKernelArgs   = TRUE,
  .AndroidBootReads = {
    &ABR_Type0_DiskIo,
    &ABR_Type0_DiskIo,
    NULL
  },
  .Media              = &Media_Large,
  .ExpectedImgData    = &ExpectedImgData_Type0_Valid,
  .ExpectedKernelArgs = ExpectedKernelArgs_Type0_Valid,
  .ExpectedReturn     = EFI_SUCCESS
};

/**
  Test Plan for AndroidBootGetVerify:
  Read a Type0 header from Disk.
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Fail_Alloc = {
  .FailAllocation = TRUE,
  .ExpectedReturn = EFI_OUT_OF_RESOURCES
};

/**
  Test Plan for AndroidBootGetVerify:
  Read a Type0 header from Disk that's too big for our media.
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Type0_Disk_Valid_Small = {
  .WithBlockIo      = TRUE,
  .AndroidBootReads = {
    &ABR_Type0_DiskIo,
    &ABR_Type0_DiskIo,
    NULL
  },
  .Media           = &Media_Small,
  .ExpectedImgData = &ExpectedImgData_Type0_Valid,
  .ExpectedReturn  = EFI_NOT_FOUND
};

/**
  Test Plan for AndroidBootGetVerify:
  Read a Type0 header from RCM.
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Type0_Rcm_Valid = {
  .WithBlockIo      = FALSE,
  .WithImgData      = TRUE,
  .PcdRcmKernelSize = 0xF5670,
  .AndroidBootReads = {
    &ABR_Type0_Rcm,
    &ABR_Type0_Rcm,
    NULL
  },
  .ExpectedImgData = &ExpectedImgData_Type0_Valid,
  .ExpectedReturn  = EFI_SUCCESS
};

/**
  Test Plan for AndroidBootGetVerify:
  Fail to read a Type0 header after reading version
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Type0_Fail = {
  .WithBlockIo      = TRUE,
  .AndroidBootReads = {
    &ABR_Type0_DiskIo,
    &ABR_Failure,
    NULL
  },
  .Media          = &Media_Large,
  .ExpectedReturn = EFI_ACCESS_DENIED
};

/**
  Test Plan for AndroidBootGetVerify:
  Fail to read header because of invalid version
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Invalid_Version = {
  .WithBlockIo      = TRUE,
  .AndroidBootReads = {
    &ABR_Invalid_Version,
    NULL
  },
  .Media          = &Media_Large,
  .ExpectedReturn = EFI_INCOMPATIBLE_VERSION
};

/**
  Test Plan for AndroidBootGetVerify:
  Cannot read header from Disk or RCM.
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Cannot_Read = {
  .PcdRcmKernelSize = 0xF5670,
  .AndroidBootReads = {
    &ABR_No_Header,
    NULL
  },
  .ExpectedImgData = NULL,
  .ExpectedReturn  = EFI_INVALID_PARAMETER
};

/**
  Test Plan for AndroidBootGetVerify:
  Read a Type0 header from Disk after reading a signature.
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Sig_Type0_Disk_Valid = {
  .WithBlockIo      = TRUE,
  .WithImgData      = TRUE,
  .AndroidBootReads = {
    &ABR_Sig_DiskIo,
    &ABR_Type0_DiskIo_After_Sig,
    &ABR_Type0_DiskIo_After_Sig,
    NULL
  },
  .Media           = &Media_Large,
  .ExpectedImgData = &ExpectedImgData_Sig_Type0_Valid,
  .ExpectedReturn  = EFI_SUCCESS
};

/**
  Test Plan for AndroidBootGetVerify:
  Fail to read from Disk after reading a signature.
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Sig_Fail = {
  .WithBlockIo      = TRUE,
  .AndroidBootReads = {
    &ABR_Sig_DiskIo,
    &ABR_Failure_After_Sig,
    NULL
  },
  .Media          = &Media_Large,
  .ExpectedReturn = EFI_ACCESS_DENIED
};

/**
  Test Plan for AndroidBootGetVerify:
  Missing header after reading a signature.
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Sig_Invalid = {
  .WithBlockIo      = TRUE,
  .AndroidBootReads = {
    &ABR_Sig_DiskIo,
    &ABR_Invalid_After_Sig,
    NULL
  },
  .Media          = &Media_Large,
  .ExpectedReturn = EFI_NOT_FOUND
};

/**
  Test Plan for AndroidBootGetVerify:
  Bad PageSize.
 */
TEST_PLAN_ANDROID_BOOT_GET_VERIFY  TP_Invalid_PageSize = {
  .WithBlockIo      = TRUE,
  .AndroidBootReads = {
    &ABR_Invalid_PageSize,
    &ABR_Invalid_PageSize,
    NULL
  },
  .Media          = &Media_Large,
  .ExpectedReturn = EFI_NOT_FOUND
};

/**
  Test AndroidBootGetVerify function.

  Depends on an instance of TEST_PLAN_ANDROID_BOOT_GET_VERIFY
  to drive the test.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
Test_AndroidBootGetVerify (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  TEST_PLAN_ANDROID_BOOT_GET_VERIFY  *TestPlan;
  TEST_PLAN_ANDROID_BOOT_READ        *ABR_TestPlan;
  EFI_STATUS                         Status;
  ANDROID_BOOT_DATA                  CaptureImgData, *ImgData;
  CHAR16                             *KernelArgs;
  UINT32                             Count;

  TestPlan = (TEST_PLAN_ANDROID_BOOT_GET_VERIFY *)Context;

  if (TestPlan->WithBlockIo) {
    TestPlan->BlockIo = MockBlockIoCreate (TestPlan->Media);
    TestPlan->DiskIo  = MockDiskIoCreate ();
  } else {
    TestPlan->BlockIo = NULL;
    TestPlan->DiskIo  = NULL;
  }

  if (TestPlan->WithImgData) {
    ImgData = &CaptureImgData;
  } else {
    ImgData = NULL;
  }

  if (TestPlan->WithKernelArgs) {
    KernelArgs = AllocateZeroPool (sizeof (CHAR16) * ANDROID_BOOTIMG_KERNEL_ARGS_SIZE);
  } else {
    KernelArgs = NULL;
  }

  if (TestPlan->FailAllocation) {
    MockAllocatePool (0);
  }

  // Fill our mocks with data from the context
  MockLibPcdGet64 (_PCD_TOKEN_PcdRcmKernelSize, TestPlan->PcdRcmKernelSize);

  Count = 0;
  while (TestPlan->AndroidBootReads[Count] != NULL) {
    ABR_TestPlan = TestPlan->AndroidBootReads[Count];
    Count++;

    if (ABR_TestPlan->WithDiskIo) {
      MockLibPcdGet64 (_PCD_TOKEN_PcdRcmKernelBase, 0);
      MockDiskIoReadDisk (
        ABR_TestPlan->ExpectedOffset,
        ABR_TestPlan->ReadBuffer,
        ABR_TestPlan->ReadReturn
        );
    } else {
      MockLibPcdGet64 (_PCD_TOKEN_PcdRcmKernelBase, (UINT64)ABR_TestPlan->ReadBuffer);
    }
  }

  // Run the code under test
  Status = AndroidBootGetVerify (TestPlan->BlockIo, TestPlan->DiskIo, ImgData, KernelArgs);

  // Verify the results
  UT_ASSERT_EQUAL (TestPlan->ExpectedReturn, Status);
  if (TestPlan->WithImgData) {
    UT_ASSERT_EQUAL (TestPlan->ExpectedImgData->Offset, ImgData->Offset);
    UT_ASSERT_EQUAL (TestPlan->ExpectedImgData->KernelSize, ImgData->KernelSize);
    UT_ASSERT_EQUAL (TestPlan->ExpectedImgData->RamdiskSize, ImgData->RamdiskSize);
    UT_ASSERT_EQUAL (TestPlan->ExpectedImgData->PageSize, ImgData->PageSize);
  }

  if (TestPlan->WithKernelArgs) {
    UT_ASSERT_MEM_EQUAL (
      TestPlan->ExpectedKernelArgs,
      KernelArgs,
      (StrLen (TestPlan->ExpectedKernelArgs) + 1) * sizeof (UINT16)
      );
  }

  if (KernelArgs) {
    FreePool (KernelArgs);
  }

  return UNIT_TEST_PASSED;
}

/**
  Prepare for Test_AndroidBootGetVerify.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
Test_AndroidBootGetVerify_Prepare (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  MemoryAllocationStubLibInit ();
  UefiPcdInit ();

  return UNIT_TEST_PASSED;
}

/**
  Cleanup after Test_AndroidBootGetVerify.
**/
STATIC
VOID
EFIAPI
Test_AndroidBootGetVerify_Cleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  TEST_PLAN_ANDROID_BOOT_GET_VERIFY  *TestPlan;

  TestPlan = (TEST_PLAN_ANDROID_BOOT_GET_VERIFY *)Context;

  if (TestPlan->BlockIo) {
    MockBlockIoDestroy (TestPlan->BlockIo);
  }

  if (TestPlan->DiskIo) {
    MockDiskIoDestroy (TestPlan->DiskIo);
  }
}

/**
  Populate the test suite.
**/
VOID
BootImgHeader_PopulateSuite (
  UNIT_TEST_SUITE_HANDLE  Suite
  )
{
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Type0_Disk_Valid);
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Type0_Fail);
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Invalid_Version);
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Fail_Alloc);
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Type0_Disk_Valid_Small);
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Type0_Rcm_Valid);
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Cannot_Read);
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Sig_Type0_Disk_Valid);
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Sig_Fail);
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Sig_Invalid);
  ADD_TEST_CASE (Test_AndroidBootGetVerify, TP_Invalid_PageSize);
}
