/** @file
  Unit tests for the implementation of NvVarIntLib

  SPDX-FileCopyrightText: Copyright (c) 2024-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockMmStTableLib.h>
#include <GoogleTest/Library/MockMmVarLib.h>
#include <GoogleTest/Library/MockHashApiLib.h>
#include <GoogleTest/Library/MockSmmVarProto.h>

extern "C" {
  #include <IndustryStandard/Tpm20.h>
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include <Library/MemoryAllocationLib.h>
  #include <Protocol/SmmVariable.h>
  #include <Library/NvVarIntLib.h>

  extern EFI_SMM_VARIABLE_PROTOCOL  *MockSmmVar;
}

using namespace testing;

typedef struct {
  CHAR16      *Name;
  EFI_GUID    Guid;
  UINT32      Attributes;
  UINT8       Data;
  UINTN       DataSize;
} TEST_NV_VAR;

STATIC TEST_NV_VAR  *mTestVariables;
STATIC UINTN        mTestVariableCount;
STATIC TEST_NV_VAR  **mNextVariables;
STATIC UINTN        mNextVariableCount;
STATIC UINTN        mNextVariableIndex;
STATIC UINT8        mDigestValue;

STATIC
EFI_STATUS
ReturnNextVariable (
  IN TEST_NV_VAR   *Variable,
  IN OUT UINTN     *VariableNameSize,
  IN OUT CHAR16    *VariableName,
  IN OUT EFI_GUID  *VendorGuid
  )
{
  UINTN  RequiredSize;

  if ((Variable == NULL) || (VariableNameSize == NULL) ||
      (VariableName == NULL) || (VendorGuid == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  RequiredSize = StrSize (Variable->Name);
  if (*VariableNameSize < RequiredSize) {
    *VariableNameSize = RequiredSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (VariableName, Variable->Name, RequiredSize);
  CopyGuid (VendorGuid, &Variable->Guid);
  *VariableNameSize = RequiredSize;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
MockSmmGetNextVariableName (
  IN OUT UINTN     *VariableNameSize,
  IN OUT CHAR16    *VariableName,
  IN OUT EFI_GUID  *VendorGuid
  )
{
  TEST_NV_VAR  *Variable;

  if (mNextVariableIndex >= mNextVariableCount) {
    return EFI_NOT_FOUND;
  }

  Variable = mNextVariables[mNextVariableIndex];
  mNextVariableIndex++;
  if (Variable == NULL) {
    return EFI_NOT_FOUND;
  }

  return ReturnNextVariable (
           Variable,
           VariableNameSize,
           VariableName,
           VendorGuid
           );
}

STATIC
EFI_STATUS
MockSmmGetVariable (
  IN      CHAR16    *VariableName,
  IN      EFI_GUID  *VendorGuid,
  OUT     UINT32    *Attributes OPTIONAL,
  IN OUT  UINTN     *DataSize,
  OUT     VOID      *Data
  )
{
  UINTN  Index;

  if ((VariableName == NULL) || (VendorGuid == NULL) || (DataSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < mTestVariableCount; Index++) {
    if ((StrCmp (VariableName, mTestVariables[Index].Name) == 0) &&
        (CompareGuid (VendorGuid, &mTestVariables[Index].Guid) == TRUE))
    {
      if (Attributes != NULL) {
        *Attributes = mTestVariables[Index].Attributes;
      }

      if ((Data == NULL) || (*DataSize < mTestVariables[Index].DataSize)) {
        *DataSize = mTestVariables[Index].DataSize;
        return EFI_BUFFER_TOO_SMALL;
      }

      CopyMem (Data, &mTestVariables[Index].Data, mTestVariables[Index].DataSize);
      *DataSize = mTestVariables[Index].DataSize;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

STATIC
BOOLEAN
MockHashApiFinal (
  IN  HASH_API_CONTEXT  HashContext,
  OUT UINT8             *Digest
  )
{
  (VOID)HashContext;

  SetMem (Digest, SHA512_DIGEST_SIZE, mDigestValue++);
  return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
class NvVarIntLibTest : public Test {
protected:
  MockMmStTableLib MmstMock;
  MockMmVarLib MmVarLibMock;
  MockHashApiLib MmHashApiLibMock;
  MockSmmVarProto SmmVarMock;
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

TEST_F (NvVarIntLibTest, ComputeVarMeasurementV0HashFinalFailure) {
  UINT8  Meas[SHA512_DIGEST_SIZE];

  EXPECT_CALL (MmVarLibMock, MmGetVariable3)
    .WillRepeatedly (Return (EFI_NOT_FOUND));
  EXPECT_CALL (MmVarLibMock, DoesVariableExist)
    .WillRepeatedly (Return (FALSE));
  EXPECT_CALL (MmHashApiLibMock, HashApiGetContextSize)
    .WillRepeatedly (Return (sizeof (UINT64)));
  EXPECT_CALL (MmHashApiLibMock, HashApiInit)
    .WillOnce (Return (TRUE));
  EXPECT_CALL (MmHashApiLibMock, HashApiUpdate)
    .WillRepeatedly (Return (TRUE));
  EXPECT_CALL (MmHashApiLibMock, HashApiFinal)
    .WillOnce (Return (FALSE));

  Status = ComputeVarMeasurementV0 (
             NULL,
             NULL,
             0,
             NULL,
             0,
             Meas
             );
  EXPECT_EQ (Status, EFI_DEVICE_ERROR);
}

TEST_F (NvVarIntLibTest, ComputeVarMeasurementV1PreservesBootServiceOnlyAfterEbs) {
  TEST_NV_VAR  Variables[] = {
    {
      (CHAR16 *)L"BsOnlyVar",
      gEfiGlobalVariableGuid,
      EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
      0xA5,
      sizeof (UINT8)
    },
    {
      (CHAR16 *)L"RuntimeVar",
      gEfiGlobalVariableGuid,
      EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
      0x5A,
      sizeof (UINT8)
    }
  };
  TEST_NV_VAR  *NextVariables[] = {
    // First recompute: runtime variable only, so the boot-service-only component is empty.
    &Variables[1],
    NULL,
    &Variables[1],
    NULL,
    // Second recompute: seed a non-empty boot-service-only cache before EBS.
    &Variables[0],
    &Variables[1],
    NULL,
    &Variables[0],
    &Variables[1],
    NULL,
    // Post-EBS recompute: only the runtime component should enumerate.
    &Variables[1],
    NULL
  };
  UINT8        Meas[SHA512_DIGEST_SIZE];

  mTestVariables     = Variables;
  mTestVariableCount = ARRAY_SIZE (Variables);
  mNextVariables     = NextVariables;
  mNextVariableCount = ARRAY_SIZE (NextVariables);
  mNextVariableIndex = 0;
  mDigestValue       = 0x10;
  ZeroMem (Meas, sizeof (Meas));

  EXPECT_CALL (MmstMock, gMmst_MmLocateProtocol)
    .WillRepeatedly (
       DoAll (
         SetArgBuffer<2>(&MockSmmVar, sizeof (MockSmmVar)),
         Return (EFI_SUCCESS)
         )
       );

  EXPECT_CALL (MmHashApiLibMock, HashApiGetContextSize)
    .WillRepeatedly (Return (sizeof (UINT64)));
  EXPECT_CALL (MmHashApiLibMock, HashApiInit)
    .WillRepeatedly (Return (TRUE));
  EXPECT_CALL (MmHashApiLibMock, HashApiUpdate)
    .WillRepeatedly (Return (TRUE));
  // Two pre-EBS full recomputes finalize both components plus combined hash.
  // The post-EBS recompute finalizes only runtime plus combined hash.
  EXPECT_CALL (MmHashApiLibMock, HashApiFinal)
    .Times (8)
    .WillRepeatedly (Invoke (MockHashApiFinal));

  EXPECT_CALL (SmmVarMock, SmmVarProto_SmmGetVariable)
    .WillRepeatedly (Invoke (MockSmmGetVariable));
  EXPECT_CALL (SmmVarMock, SmmVarProto_SmmGetNextVariableName)
    .Times (ARRAY_SIZE (NextVariables))
    .WillRepeatedly (Invoke (MockSmmGetNextVariableName));

  Status = ComputeVarMeasurementV1 (
             NULL,
             NULL,
             0,
             NULL,
             0,
             Meas
             );
  EXPECT_EQ (Status, EFI_SUCCESS);

  Status = ComputeVarMeasurementV1 (
             NULL,
             NULL,
             0,
             NULL,
             0,
             Meas
             );
  EXPECT_EQ (Status, EFI_SUCCESS);

  NvVarIntNotifyExitBootServices ();
  Status = ComputeVarMeasurementV1 (
             NULL,
             NULL,
             0,
             NULL,
             0,
             Meas
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
