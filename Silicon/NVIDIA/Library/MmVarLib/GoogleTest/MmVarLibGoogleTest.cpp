/** @file
  Unit tests for the implementation of MmVarLib

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockMmStTableLib.h>
#include <GoogleTest/Library/MockSmmVarProto.h>

extern "C" {
  #include <Library/MemoryAllocationLib.h>
  #include <Protocol/SmmVariable.h>
  #include <Library/MmVarLib.h>

  extern EFI_SMM_VARIABLE_PROTOCOL  *MockSmmVar;
  extern EFIAPI VOID TestHookMmVarLibClearPtr (VOID);
}

using namespace testing;

//////////////////////////////////////////////////////////////////////////////
class MmVarLibTest : public Test {
  protected:
    MockMmStTableLib MmstMock;
    MockSmmVarProto  SmmVarMock;
    BOOLEAN          Found;
    EFI_STATUS       Status;
    UINT8            Value;
    UINTN            Size;
    UINT32           Attr;
    UINT8            *ValPtr;
    UINTN            ExpectedSize;
    UINT32           ExpectedAttr;
    UINT8            ExpectedValue;

    void SetUp() override {
      TestHookMmVarLibClearPtr ();
      ExpectedSize = 0;
      ExpectedAttr = 0;
      Size = 0;
      Attr = 0;
      Value = 0;
      ExpectedValue = 0;
      ValPtr = NULL;
    }
};

// DoesVariableExist_TC0 SmmVarProto not found
TEST_F(MmVarLibTest, DoesVariableExist_TC0) {
  EXPECT_CALL (MmstMock,gMmst_MmLocateProtocol)
    .WillOnce(Return(EFI_NOT_FOUND));

  Found = DoesVariableExist ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                             &gEfiGlobalVariableGuid,
                             NULL,
                             NULL);
  EXPECT_FALSE(Found);
}

// DoesVariableExist_TC1, Var Not Found
TEST_F(MmVarLibTest, DoesVariableExist_TC1) {
  EXPECT_CALL (MmstMock,
    gMmst_MmLocateProtocol (
      BufferEq(&gEfiSmmVariableProtocolGuid, sizeof(EFI_GUID)),
      _,
      NotNull()))
      .WillOnce(DoAll(
            SetArgBuffer<2>(&MockSmmVar, sizeof (MockSmmVar)),
            Return(EFI_SUCCESS)));
  EXPECT_CALL (SmmVarMock, SmmVarProto_SmmGetVariable)
    .WillOnce(Return(EFI_NOT_FOUND));

  Found = DoesVariableExist ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                             &gEfiGlobalVariableGuid,
                             NULL,
                             NULL);
  EXPECT_FALSE(Found);
}

// DoesVariableExist_TC2, Var Found with Valid Size and Attributes.
TEST_F(MmVarLibTest, DoesVariableExist_TC2) {
  ExpectedSize = 2;
  ExpectedAttr = 0x40;

  EXPECT_CALL (MmstMock,
    gMmst_MmLocateProtocol (
      BufferEq(&gEfiSmmVariableProtocolGuid, sizeof(EFI_GUID)),
      _,
      NotNull()))
      .WillOnce(DoAll(
            SetArgBuffer<2>(&MockSmmVar, sizeof (MockSmmVar)),
            Return(EFI_SUCCESS)));
  EXPECT_CALL (SmmVarMock,
    SmmVarProto_SmmGetVariable (
      Char16StrEq(EFI_SECURE_BOOT_MODE_NAME),
      BufferEq(&gEfiGlobalVariableGuid, sizeof(EFI_GUID)),
      NotNull(),
      NotNull(),
      _))
    .WillOnce(DoAll (
              SetArgBuffer<3>(&ExpectedSize, sizeof(ExpectedSize)),
              SetArgBuffer<2>(&ExpectedAttr, sizeof(ExpectedAttr)),
              Return(EFI_BUFFER_TOO_SMALL)));

  Found = DoesVariableExist ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                             &gEfiGlobalVariableGuid,
                             &Size,
                             &Attr);
  EXPECT_EQ(Size, ExpectedSize);
  EXPECT_EQ(Attr, ExpectedAttr);
  EXPECT_TRUE(Found);
}


// MmGetVariable_TC0 SmmVar Proto Not Found
TEST_F(MmVarLibTest, MmGetVariable_TC0) {
  EXPECT_CALL (MmstMock,gMmst_MmLocateProtocol)
    .WillOnce(Return(EFI_NOT_FOUND));

  Status = MmGetVariable ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                           &gEfiGlobalVariableGuid,
                           &Value,
                           1);
  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

//MmGetVariable_TC1 NULL value pointer.
TEST_F(MmVarLibTest, MmGetVariable_TC1) {
  Status = MmGetVariable ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                           &gEfiGlobalVariableGuid,
                           NULL,
                           1);
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

//MmGetVariable_TC2 Mismatched Size.
TEST_F(MmVarLibTest, MmGetVariable_TC2) {
  ExpectedSize = 4;

  EXPECT_CALL (MmstMock,
    gMmst_MmLocateProtocol (
      BufferEq(&gEfiSmmVariableProtocolGuid, sizeof(EFI_GUID)),
      _,
      NotNull()))
      .WillOnce(DoAll(
            SetArgBuffer<2>(&MockSmmVar, sizeof (MockSmmVar)),
            Return(EFI_SUCCESS)));
  EXPECT_CALL (SmmVarMock,
    SmmVarProto_SmmGetVariable (
      Char16StrEq(EFI_SECURE_BOOT_MODE_NAME),
      BufferEq(&gEfiGlobalVariableGuid, sizeof(EFI_GUID)),
      _,
      NotNull(),
      _))
    .WillOnce(DoAll (
              SetArgBuffer<3>(&ExpectedSize, sizeof(ExpectedSize)),
              Return(EFI_BUFFER_TOO_SMALL)));
  Status = MmGetVariable ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                           &gEfiGlobalVariableGuid,
                           &Value,
                           sizeof(UINT8));
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}


//Get a variable with the correct parameters.
TEST_F(MmVarLibTest, MmGetVariable_TC3) {
  ExpectedSize  = 1;
  ExpectedValue = 0xAA;

  EXPECT_CALL (MmstMock,
    gMmst_MmLocateProtocol (
      BufferEq(&gEfiSmmVariableProtocolGuid, sizeof(EFI_GUID)),
      _,
      NotNull()))
      .WillOnce(DoAll(
            SetArgBuffer<2>(&MockSmmVar, sizeof (MockSmmVar)),
            Return(EFI_SUCCESS)));
  EXPECT_CALL (SmmVarMock,
    SmmVarProto_SmmGetVariable (
      Char16StrEq(EFI_SECURE_BOOT_MODE_NAME),
      BufferEq(&gEfiGlobalVariableGuid, sizeof(EFI_GUID)),
      _,
      NotNull(),
      _))
    .WillOnce(DoAll (
              SetArgBuffer<3>(&ExpectedSize, sizeof(ExpectedSize)),
              Return(EFI_BUFFER_TOO_SMALL)));
  EXPECT_CALL (SmmVarMock,
    SmmVarProto_SmmGetVariable (
      Char16StrEq(EFI_SECURE_BOOT_MODE_NAME),
      BufferEq(&gEfiGlobalVariableGuid, sizeof(EFI_GUID)),
      _,
      NotNull(),
      NotNull()))
    .WillOnce(DoAll (
              SetArgBuffer<3>(&ExpectedSize, sizeof(ExpectedSize)),
              SetArgBuffer<4>(&ExpectedValue, sizeof(ExpectedValue)),
              Return(EFI_SUCCESS)));
  Status = MmGetVariable ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                           &gEfiGlobalVariableGuid,
                           &Value,
                           sizeof(UINT8));
  EXPECT_EQ (Value, ExpectedValue);
  EXPECT_EQ (Status, EFI_SUCCESS);
}

TEST_F(MmVarLibTest, MmGetVariable3_TC0) {
  EXPECT_CALL (MmstMock,gMmst_MmLocateProtocol)
    .WillOnce(Return(EFI_NOT_FOUND));

  Status = MmGetVariable3 ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                           &gEfiGlobalVariableGuid,
                           (VOID **)&ValPtr,
                           NULL,
                           NULL);

  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

TEST_F(MmVarLibTest, MmGetVariable3_TC1) {
  Status = MmGetVariable3 ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                           &gEfiGlobalVariableGuid,
                           NULL,
                           NULL,
                           NULL);

  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

TEST_F(MmVarLibTest, MmGetVariable3_TC2) {
  ExpectedSize  = 1;
  ExpectedAttr  = 0x40;
  ExpectedValue = 0xAA;

  EXPECT_CALL (MmstMock,
    gMmst_MmLocateProtocol (
      BufferEq(&gEfiSmmVariableProtocolGuid, sizeof(EFI_GUID)),
      _,
      NotNull()))
      .WillOnce(DoAll(
            SetArgBuffer<2>(&MockSmmVar, sizeof (MockSmmVar)),
            Return(EFI_SUCCESS)));
  EXPECT_CALL (SmmVarMock,
    SmmVarProto_SmmGetVariable (
      Char16StrEq(EFI_SECURE_BOOT_MODE_NAME),
      BufferEq(&gEfiGlobalVariableGuid, sizeof(EFI_GUID)),
      _,
      NotNull(),
      _))
    .WillOnce(DoAll (
              SetArgBuffer<3>(&ExpectedSize, sizeof(ExpectedSize)),
              Return(EFI_BUFFER_TOO_SMALL)));
  EXPECT_CALL (SmmVarMock,
    SmmVarProto_SmmGetVariable (
      Char16StrEq(EFI_SECURE_BOOT_MODE_NAME),
      BufferEq(&gEfiGlobalVariableGuid, sizeof(EFI_GUID)),
      NotNull(),
      NotNull(),
      NotNull()))
    .WillOnce(DoAll (
              SetArgBuffer<2>(&ExpectedAttr, sizeof(ExpectedAttr)),
              SetArgBuffer<3>(&ExpectedSize, sizeof(ExpectedSize)),
              SetArgBuffer<4>(&ExpectedValue, sizeof(ExpectedValue)),
              Return(EFI_SUCCESS)));

  Status = MmGetVariable3 ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                           &gEfiGlobalVariableGuid,
                           (VOID **)&ValPtr,
                           &Size,
                           &Attr);

  EXPECT_EQ (Size, ExpectedSize);
  EXPECT_EQ (Attr, ExpectedAttr);
  EXPECT_EQ (*ValPtr, ExpectedValue);
  EXPECT_EQ (Status, EFI_SUCCESS);
}

TEST_F(MmVarLibTest, MmGetVariable2_TC0) {
  EXPECT_CALL (MmstMock,gMmst_MmLocateProtocol)
    .WillOnce(Return(EFI_NOT_FOUND));

  Status = MmGetVariable2 ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                           &gEfiGlobalVariableGuid,
                           (VOID **)&ValPtr,
                           NULL);

  EXPECT_EQ (Status, EFI_NOT_FOUND);
}

TEST_F(MmVarLibTest, MmGetVariable2_TC1) {
  Status = MmGetVariable2 ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                           &gEfiGlobalVariableGuid,
                           NULL,
                           NULL);

  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

TEST_F(MmVarLibTest, MmGetVariable2_TC2) {
  ExpectedSize  = 1;
  ExpectedValue = 0xAA;

  EXPECT_CALL (MmstMock,
    gMmst_MmLocateProtocol (
      BufferEq(&gEfiSmmVariableProtocolGuid, sizeof(EFI_GUID)),
      _,
      NotNull()))
      .WillOnce(DoAll(
            SetArgBuffer<2>(&MockSmmVar, sizeof (MockSmmVar)),
            Return(EFI_SUCCESS)));
  EXPECT_CALL (SmmVarMock,
    SmmVarProto_SmmGetVariable (
      Char16StrEq(EFI_SECURE_BOOT_MODE_NAME),
      BufferEq(&gEfiGlobalVariableGuid, sizeof(EFI_GUID)),
      _,
      NotNull(),
      _))
    .WillOnce(DoAll (
              SetArgBuffer<3>(&ExpectedSize, sizeof(ExpectedSize)),
              Return(EFI_BUFFER_TOO_SMALL)));
  EXPECT_CALL (SmmVarMock,
    SmmVarProto_SmmGetVariable (
      Char16StrEq(EFI_SECURE_BOOT_MODE_NAME),
      BufferEq(&gEfiGlobalVariableGuid, sizeof(EFI_GUID)),
      _,
      NotNull(),
      NotNull()))
    .WillOnce(DoAll (
              SetArgBuffer<3>(&ExpectedSize, sizeof(ExpectedSize)),
              SetArgBuffer<4>(&ExpectedValue, sizeof(ExpectedValue)),
              Return(EFI_SUCCESS)));

  Status = MmGetVariable2 ((CHAR16 *)EFI_SECURE_BOOT_MODE_NAME,
                           &gEfiGlobalVariableGuid,
                           (VOID **)&ValPtr,
                           &Size);

  EXPECT_EQ (Size, ExpectedSize);
  EXPECT_EQ (*ValPtr, ExpectedValue);
  EXPECT_EQ (Status, EFI_SUCCESS);
}


int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
