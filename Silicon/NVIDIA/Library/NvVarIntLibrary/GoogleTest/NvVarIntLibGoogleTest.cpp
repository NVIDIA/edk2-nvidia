/** @file
  Unit tests for the implementation of NvVarIntLib

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockMmStTableLib.h>
#include <GoogleTest/Library/MockMmVarLib.h>
#include <GoogleTest/Library/MockHashApiLib.h>

extern "C" {
  #include <Library/MemoryAllocationLib.h>
  #include <Protocol/SmmVariable.h>
  #include <Library/NvVarIntLib.h>

  extern EFI_SMM_VARIABLE_PROTOCOL  *MockSmmVar;
}

using namespace testing;

//////////////////////////////////////////////////////////////////////////////
class NvVarIntLibTest : public Test {
protected:
  MockMmVarLib MmVarLibMock;
  MockHashApiLib MmHashApiLibMock;
  EFI_STATUS Status;
  UINTN ExpectedBootCount;
  UINT16 *ExpectedBootOrder;
  UINT32 ExpectedAttr;
  UINT8 ExpectedSecureMode;

  void
  SetUp (
    ) override
  {
  }
};

// NvVarIntLibTest_TC0 BootOrder doesn't exist
TEST_F (NvVarIntLibTest, MeasureBootVars_TC0) {
  EXPECT_CALL (MmVarLibMock, MmGetVariable3)
    .WillOnce (Return (EFI_NOT_FOUND));

  Status = MeasureBootVars (
             NULL,
             NULL,
             0,
             NULL,
             0
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
}

// NvVarIntLibTest MeasureBootVars_TC1, Success case.
TEST_F (NvVarIntLibTest, MeasureBootVars_TC1) {
  ExpectedBootCount = 1 * sizeof (UINT16);
  ExpectedBootOrder = (UINT16 *)AllocateZeroPool (ExpectedBootCount * sizeof (UINT16));
  ExpectedAttr      = 0x40;
  EXPECT_CALL (
    MmVarLibMock,
    MmGetVariable3 (
      Char16StrEq (EFI_BOOT_ORDER_VARIABLE_NAME),
      BufferEq (&gEfiGlobalVariableGuid, sizeof (EFI_GUID)),
      NotNull (),
      NotNull (),
      NotNull ()
      )
    )
    .WillOnce (
       DoAll (
         SetArgBuffer<2>(&ExpectedBootOrder, sizeof (ExpectedBootOrder)),
         SetArgBuffer<3>(&ExpectedBootCount, sizeof (ExpectedBootCount)),
         SetArgBuffer<4>(&ExpectedAttr, sizeof (ExpectedAttr)),
         Return (EFI_SUCCESS)
         )
       );
  EXPECT_CALL (MmHashApiLibMock, HashApiUpdate)
    .WillRepeatedly (Return (TRUE));
  EXPECT_CALL (
    MmVarLibMock,
    MmGetVariable3 (
      Char16StrEq (L"Boot0000"),
      BufferEq (&gEfiGlobalVariableGuid, sizeof (EFI_GUID)),
      NotNull (),
      NotNull (),
      NotNull ()
      )
    )
    .WillOnce (
       DoAll (
         SetArgBuffer<2>(&ExpectedBootOrder, sizeof (ExpectedBootOrder)),
         SetArgBuffer<3>(&ExpectedBootCount, sizeof (ExpectedBootCount)),
         SetArgBuffer<4>(&ExpectedAttr, sizeof (ExpectedAttr)),
         Return (EFI_NOT_FOUND)
         )
       );

  Status = MeasureBootVars (
             NULL,
             NULL,
             0,
             NULL,
             0
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
}

// NvVarIntLibTest MeasureBootVars_TC2 HashUpdate Failed
TEST_F (NvVarIntLibTest, MeasureBootVars_TC2) {
  ExpectedBootCount = 1 * sizeof (UINT16);
  ExpectedBootOrder = (UINT16 *)AllocateZeroPool (ExpectedBootCount * sizeof (UINT16));
  ExpectedAttr      = 0x40;

  EXPECT_CALL (
    MmVarLibMock,
    MmGetVariable3 (
      Char16StrEq (EFI_BOOT_ORDER_VARIABLE_NAME),
      BufferEq (&gEfiGlobalVariableGuid, sizeof (EFI_GUID)),
      NotNull (),
      NotNull (),
      NotNull ()
      )
    )
    .WillOnce (
       DoAll (
         SetArgBuffer<2>(&ExpectedBootOrder, sizeof (ExpectedBootOrder)),
         SetArgBuffer<3>(&ExpectedBootCount, sizeof (ExpectedBootCount)),
         SetArgBuffer<4>(&ExpectedAttr, sizeof (ExpectedAttr)),
         Return (EFI_SUCCESS)
         )
       );
  EXPECT_CALL (MmHashApiLibMock, HashApiUpdate)
    .WillRepeatedly (Return (FALSE));

  Status = MeasureBootVars (
             NULL,
             NULL,
             0,
             NULL,
             0
             );
  EXPECT_EQ (Status, EFI_UNSUPPORTED);
}

// NvVarIntLibTest MeasureSecureDbVars_TC0, No Vars.
TEST_F (NvVarIntLibTest, MeasureSecureDbVars_TC0) {
  EXPECT_CALL (MmVarLibMock, DoesVariableExist)
    .WillRepeatedly (Return (FALSE));
  EXPECT_CALL (MmHashApiLibMock, HashApiUpdate)
    .WillRepeatedly (Return (TRUE));

  Status = MeasureSecureDbVars (
             NULL,
             NULL,
             0,
             NULL,
             0
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
}

// NvVarIntLibTest MeasureSecureDbVars_TC1, Hash Fail.
TEST_F (NvVarIntLibTest, MeasureSecureDbVars_TC1) {
  EXPECT_CALL (MmHashApiLibMock, HashApiUpdate)
    .WillRepeatedly (Return (FALSE));

  Status = MeasureSecureDbVars (
             NULL,
             NULL,
             0,
             NULL,
             0
             );
  EXPECT_EQ (Status, EFI_UNSUPPORTED);
}

// NvVarIntLibTest MeasureSecureDbVars_TC1, Volatile.
TEST_F (NvVarIntLibTest, MeasureSecureDbVars_TC2) {
  ExpectedBootCount  = 1 * sizeof (UINT8);
  ExpectedSecureMode = 0x1;
  ExpectedAttr       = 0x6;

  EXPECT_CALL (MmVarLibMock, DoesVariableExist)
    .WillRepeatedly (Return (FALSE));
  EXPECT_CALL (
    MmVarLibMock,
    DoesVariableExist (
      Char16StrEq (EFI_SECURE_BOOT_MODE_NAME),
      BufferEq (&gEfiGlobalVariableGuid, sizeof (EFI_GUID)),
      NotNull (),
      NotNull ()
      )
    )
    .WillOnce (
       DoAll (
         SetArgBuffer<2>(&ExpectedBootCount, sizeof (ExpectedBootCount)),
         SetArgBuffer<3>(&ExpectedAttr, sizeof (ExpectedAttr)),
         Return (EFI_SUCCESS)
         )
       );
  EXPECT_CALL (MmHashApiLibMock, HashApiUpdate)
    .WillRepeatedly (Return (TRUE));

  Status = MeasureSecureDbVars (
             NULL,
             NULL,
             0,
             NULL,
             0
             );
  EXPECT_EQ (Status, EFI_SUCCESS);
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
