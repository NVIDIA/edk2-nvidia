/** @file
  Unit tests of the UpdateKernelArgs function for the AndroidBootDxe Driver.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <AndroidBootDxeUnitTestPrivate.h>

CHAR16  mMaxLengthInitial[ANDROID_BOOTIMG_KERNEL_ARGS_SIZE];
CHAR16  mMaxLengthNew[ANDROID_BOOTIMG_KERNEL_ARGS_SIZE];

/**
  Test Plan for UpdateKernelArgs:
  Pass in an invalid Protocol
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_NullProtocol = {
  .InitialKernelArgs = L"blah console:xyz",
  .NewKernelArgs     = L"new args",
  .InvalidProtocol   = TRUE,
  .ExpectedReturn    = EFI_INVALID_PARAMETER
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in Null kernel args for initial and new
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_NullKernelArgs = {
  .InitialKernelArgs = NULL,
  .NewKernelArgs     = NULL,
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in Null kernel arg for initial
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_NullInitialKernelArgs = {
  .InitialKernelArgs = NULL,
  .NewKernelArgs     = L"new args",
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in Null kernel arg for new
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_NullNewKernelArgs = {
  .InitialKernelArgs = L"blah console:xyz",
  .NewKernelArgs     = NULL,
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in Null kernel arg for new, empty for initial
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_NullNewEmptyInitialKernelArgs = {
  .InitialKernelArgs = L"",
  .NewKernelArgs     = NULL,
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in Null kernel arg for initial, empty for new
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_NullInitialEmptyNewKernelArgs = {
  .InitialKernelArgs = NULL,
  .NewKernelArgs     = L"",
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in Null kernel arg for new, max for initial
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_NullNewMaxInitialKernelArgs = {
  .InitialKernelArgs = mMaxLengthInitial,
  .NewKernelArgs     = NULL,
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in Null kernel arg for initial, max for new
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_NullInitialMaxNewKernelArgs = {
  .InitialKernelArgs = NULL,
  .NewKernelArgs     = mMaxLengthNew,
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in Null kernel arg for initial and new
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_NullBothKernelArgs = {
  .InitialKernelArgs = NULL,
  .NewKernelArgs     = NULL,
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in max kernel arg for initial, max for new
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_MaxInitialMaxNewKernelArgs = {
  .InitialKernelArgs = mMaxLengthInitial,
  .NewKernelArgs     = mMaxLengthNew,
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in shorter new arg
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_ShorterNewKernelArgs = {
  .InitialKernelArgs = L"blah console:wxyz",
  .NewKernelArgs     = L"new args shorter",
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in longer new arg
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_LongerNewKernelArgs = {
  .InitialKernelArgs = L"blah console:x",
  .NewKernelArgs     = L"longer new args",
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in same size new arg
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_SameSizeKernelArgs = {
  .InitialKernelArgs = L"same size ",
  .NewKernelArgs     = L"equivalent",
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in empty initial arg
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_EmptyInitialKernelArgs = {
  .InitialKernelArgs = L"",
  .NewKernelArgs     = L"not empty",
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in empty new arg
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_EmptyNewKernelArgs = {
  .InitialKernelArgs = L"not empty",
  .NewKernelArgs     = L"",
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in empty new and initial arg
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_EmptyBothKernelArgs = {
  .InitialKernelArgs = L"",
  .NewKernelArgs     = L"",
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in max initial arg
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_MaxInitialKernelArgs = {
  .InitialKernelArgs = mMaxLengthInitial,
  .NewKernelArgs     = L"not empty",
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Pass in max new arg
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_MaxNewKernelArgs = {
  .InitialKernelArgs = L"not empty",
  .NewKernelArgs     = mMaxLengthNew,
  .ExpectedReturn    = EFI_SUCCESS
};

/**
  Test Plan for UpdateKernelArgs:
  Failing allocation
 */
TEST_PLAN_UPDATE_KERNEL_ARGS  TP_AllocFail = {
  .InitialKernelArgs = L"small",
  .NewKernelArgs     = L"larger",
  .FailAllocation    = TRUE,
  .ExpectedReturn    = EFI_OUT_OF_RESOURCES
};

extern EFI_STATUS
UpdateKernelArgs (
  IN NVIDIA_KERNEL_ARGS_PROTOCOL  *This,
  IN CONST CHAR16                 *NewArgs
  );

NVIDIA_KERNEL_ARGS_PROTOCOL  mProtocol = {
  NULL,
  NULL
};

/**
  Test UpdateKernelArgs function.

  Depends on an instance of TEST_PLAN_UPDATE_KERNEL_ARGS
  to drive the test.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
Test_UpdateKernelArgs (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  TEST_PLAN_UPDATE_KERNEL_ARGS  *TestPlan;
  EFI_STATUS                    Status;
  CONST CHAR16                  *ExpectedKernelArgs;
  NVIDIA_KERNEL_ARGS_PROTOCOL   *Protocol = NULL;

  TestPlan = (TEST_PLAN_UPDATE_KERNEL_ARGS *)Context;

  // Set up Mock
  if (TestPlan->FailAllocation) {
    MockAllocatePool (0);
  }

  // Set up Protocol
  if (!TestPlan->InvalidProtocol) {
    Protocol = &mProtocol;
  }

  // Run the code under test
  Status = UpdateKernelArgs (Protocol, TestPlan->NewKernelArgs);

  // Verify the results
  UT_ASSERT_EQUAL (TestPlan->ExpectedReturn, Status);

  if (Protocol != NULL) {
    if (TestPlan->ExpectedReturn == EFI_SUCCESS) {
      ExpectedKernelArgs = TestPlan->NewKernelArgs;
    } else {
      ExpectedKernelArgs = TestPlan->InitialKernelArgs;
    }

    if (ExpectedKernelArgs != NULL) {
      UT_ASSERT_NOT_NULL (Protocol->KernelArgs);
      UT_ASSERT_MEM_EQUAL (ExpectedKernelArgs, Protocol->KernelArgs, StrSize (ExpectedKernelArgs));
    } else {
      UT_ASSERT_TRUE (Protocol->KernelArgs == NULL);
    }
  }

  return UNIT_TEST_PASSED;
}

/**
  Prepare for Test_UpdateKernelArgs.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
Test_UpdateKernelArgs_Prepare (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  TEST_PLAN_UPDATE_KERNEL_ARGS  *TestPlan;

  TestPlan = (TEST_PLAN_UPDATE_KERNEL_ARGS *)Context;

  MemoryAllocationStubLibInit ();

  if (TestPlan->InitialKernelArgs != NULL) {
    mProtocol.KernelArgs = AllocateCopyPool (StrSize (TestPlan->InitialKernelArgs), TestPlan->InitialKernelArgs);
  }

  return UNIT_TEST_PASSED;
}

/**
  Cleanup after Test_UpdateKernelArgs.
**/
STATIC
VOID
EFIAPI
Test_UpdateKernelArgs_Cleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  if (mProtocol.KernelArgs != NULL) {
    FreePool (mProtocol.KernelArgs);
    mProtocol.KernelArgs = NULL;
  }
}

/**
  Set up the UpdateKernelArgs test suite.
 */
VOID
EFIAPI
Suite_UpdateKernelArgs_Setup (
  VOID
  )
{
  SetMem16 (mMaxLengthInitial, sizeof (CHAR16) * (ANDROID_BOOTIMG_KERNEL_ARGS_SIZE-1), L'I');
  mMaxLengthInitial[ANDROID_BOOTIMG_KERNEL_ARGS_SIZE-1] = L'\0';
  SetMem16 (mMaxLengthNew, sizeof (CHAR16) * (ANDROID_BOOTIMG_KERNEL_ARGS_SIZE-1), L'N');
  mMaxLengthNew[ANDROID_BOOTIMG_KERNEL_ARGS_SIZE-1] = L'\0';
}

/**
  Populate the test suite.
**/
VOID
UpdateKernelArgs_PopulateSuite (
  UNIT_TEST_SUITE_HANDLE  Suite
  )
{
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_NullProtocol);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_NullKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_NullInitialKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_NullNewKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_NullNewEmptyInitialKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_NullInitialEmptyNewKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_NullNewMaxInitialKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_NullInitialMaxNewKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_NullBothKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_MaxInitialMaxNewKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_ShorterNewKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_LongerNewKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_SameSizeKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_EmptyInitialKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_EmptyNewKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_EmptyBothKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_MaxInitialKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_MaxNewKernelArgs);
  ADD_TEST_CASE (Test_UpdateKernelArgs, TP_AllocFail);
}
