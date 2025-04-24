/** @file
  Unit tests for the implementation of MmVarLib

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockMmStTableLib.h>
#include <GoogleTest/Library/MockNvNorFlashProto.h>
#include <GoogleTest/Library/MockNvVarIntLib.h>

extern "C" {
  #include <Library/MemoryAllocationLib.h>
  #include <Library/StandaloneMmOpteeDeviceMem.h>
  #include "../FvbPrivate.h"

  extern NVIDIA_NOR_FLASH_PROTOCOL  *MockNvNorFlash;
  extern NVIDIA_VAR_INT_PROTOCOL    *VarIntProto;
}

using namespace testing;

//////////////////////////////////////////////////////////////////////////////
class VarIntCheckTest : public Test {
protected:
  MockMmStTableLib MmstMock;
  MockNvNorFlashProto NvNorFlashProtoMock;
  MockNvVarIntLib NvVarIntLibMock;
  NVIDIA_VAR_INT_PROTOCOL MVarIntProto;
  NOR_FLASH_ATTRIBUTES FlashAttr;
  EFI_STATUS Status;
  UINT8 MeasBuf[33];
  UINT8 *FlashBuf;

  void
  SetUp (
    ) override
  {
    FlashAttr.MemoryDensity = 65536;
    FlashAttr.BlockSize     = 4096;
    MeasBuf[1]              = 0xAB;
  }

  void
  TestSetup (
    )
  {
    MVarIntProto.WriteNewMeasurement = NULL;
    MVarIntProto.NorFlashProtocol    = MockNvNorFlash;
    MVarIntProto.BlockSize           = VarIntProto->BlockSize;
    MVarIntProto.PartitionSize       = VarIntProto->PartitionSize;
    MVarIntProto.PartitionByteOffset = VarIntProto->PartitionByteOffset;
    MVarIntProto.MeasurementSize     = VarIntProto->MeasurementSize;
    MVarIntProto.CurMeasurement      = (UINT8 *)AllocateZeroPool (MVarIntProto.MeasurementSize);
  }
};

// Get a variable with the correct parameters.
TEST_F (VarIntCheckTest, VarIntCheck_TC0) {
  EXPECT_CALL (MmstMock, gMmst_MmInstallProtocolInterface)
    .WillOnce (Return (EFI_SUCCESS));

  Status = VarIntInit (
             0,
             8092,
             MockNvNorFlash,
             &FlashAttr
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
}

TEST_F (VarIntCheckTest, VarIntCheck_TC1) {
  TestSetup ();
  FlashBuf = (UINT8 *)AllocatePool (MVarIntProto.PartitionSize);
  SetMem (FlashBuf, MVarIntProto.PartitionSize, 0xFF);

  EXPECT_CALL (
    NvVarIntLibMock,
    ComputeVarMeasurement (
      _,
      _,
      0,
      _,
      0,
      NotNull ()
      )
    )
    .WillOnce (
       DoAll (
         SetArgBuffer<5>(&MeasBuf, sizeof (MeasBuf)),
         Return (EFI_SUCCESS)
         )
       );
  EXPECT_CALL (NvNorFlashProtoMock, NvNorFlashProto_Write)
    .WillRepeatedly (Return (EFI_SUCCESS));
  EXPECT_CALL (NvNorFlashProtoMock, NvNorFlashProto_Erase)
    .WillRepeatedly (Return (EFI_SUCCESS));
  EXPECT_CALL (NvNorFlashProtoMock, NvNorFlashProto_Read)
    .WillRepeatedly (Return (EFI_SUCCESS));
  EXPECT_CALL (
    NvNorFlashProtoMock,
    NvNorFlashProto_Read (
      NotNull (),
      MVarIntProto.PartitionByteOffset,
      MVarIntProto.PartitionSize,
      NotNull ()
      )
    )
    .WillOnce (
       DoAll (
         SetArgBuffer<3>(FlashBuf, MVarIntProto.PartitionSize),
         Return (EFI_SUCCESS)
         )
       );

  Status = VarIntValidate (&MVarIntProto);
  EXPECT_EQ (Status, EFI_SUCCESS);
  FreePool (FlashBuf);
}

// VarIntCheck_TC2, re-compute the measurement
// write the measurement
//
TEST_F (VarIntCheckTest, VarIntCheck_TC2) {
  Status = VarIntComputeMeasurement (
             &MVarIntProto,
             EFI_SECURE,
             &
             A
             &
             S
             );
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
