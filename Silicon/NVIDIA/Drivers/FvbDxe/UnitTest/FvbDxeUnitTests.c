/** @file
  Unit tests of FvbDxe Driver. Primarily tests
  the EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL as well as some of the
  header initialize and validation functions.

  Tests are run using a flash stub, including tests for both
  a working flash device and a faulty flash device.

  Copyright (c) 2020-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>

#include <HostBasedTestStubLib/FlashStubLib.h>

#include <FvbDxeTestPrivate.h>

// So that we can reference FvbPrivate declarations
#include "../FvbPrivate.h"

#define UNIT_TEST_APP_NAME     "FvbDxe Unit Test Application"
#define UNIT_TEST_APP_VERSION  "0.1"

#define BLOCK_SIZE       512
#define NUM_BLOCKS       10// EraseBlock tests rely on this being >= 4 blocks
#define IO_ALIGN         1
#define MOCK_ATTRIBUTES  0xFF

extern NVIDIA_FVB_PRIVATE_DATA  *Private;

STATIC UINT8  *TestVariablePartition;
STATIC UINT8  *TestFlashStorage;
STATIC UINT8  *TestBuffer;

STATIC GUID  ZeroGuid = {
  0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }
};

// RW_TEST_CONTEXT fields:
//  Lba
//  Offset
//  NumBytes
//  ExpectedStatus
//  ExpectedNumBytes

// Low Lba Tests
STATIC RW_TEST_CONTEXT  SimpleTest1LbaLo = {
  0,
  0,
  BLOCK_SIZE / 2,
  EFI_SUCCESS,
  BLOCK_SIZE / 2
};
STATIC RW_TEST_CONTEXT  SimpleTest2LbaLo = {
  0,
  BLOCK_SIZE / 4,
  BLOCK_SIZE / 2,
  EFI_SUCCESS,
  BLOCK_SIZE / 2
};
STATIC RW_TEST_CONTEXT  FullBlockTestLbaLo = {
  0,
  0,
  BLOCK_SIZE,
  EFI_SUCCESS,
  BLOCK_SIZE
};
STATIC RW_TEST_CONTEXT  ZeroByteTestLbaLo = {
  0,
  0,
  0,
  EFI_BAD_BUFFER_SIZE,
  0
};
STATIC RW_TEST_CONTEXT  CrossBoundaryTestLbaLo = {
  0,
  BLOCK_SIZE - (BLOCK_SIZE / 2),
  BLOCK_SIZE,
  EFI_BAD_BUFFER_SIZE,
  BLOCK_SIZE / 2
};
STATIC RW_TEST_CONTEXT  BadOffsetTestLbaLo = {
  0,
  BLOCK_SIZE,
  1,
  EFI_BAD_BUFFER_SIZE,
  0
};

// Middle Lba Tests
STATIC RW_TEST_CONTEXT  SimpleTest1LbaMid = {
  NUM_BLOCKS / 2,
  0,
  BLOCK_SIZE / 2,
  EFI_SUCCESS,
  BLOCK_SIZE / 2
};
STATIC RW_TEST_CONTEXT  SimpleTest2LbaMid = {
  NUM_BLOCKS / 2,
  BLOCK_SIZE / 4,
  BLOCK_SIZE / 2,
  EFI_SUCCESS,
  BLOCK_SIZE / 2
};
STATIC RW_TEST_CONTEXT  FullBlockTestLbaMid = {
  NUM_BLOCKS / 2,
  0,
  BLOCK_SIZE,
  EFI_SUCCESS,
  BLOCK_SIZE
};
STATIC RW_TEST_CONTEXT  ZeroByteTestLbaMid = {
  NUM_BLOCKS / 2,
  0,
  0,
  EFI_BAD_BUFFER_SIZE,
  0
};
STATIC RW_TEST_CONTEXT  CrossBoundaryTestLbaMid = {
  NUM_BLOCKS / 2,
  BLOCK_SIZE - (BLOCK_SIZE / 2),
  BLOCK_SIZE,
  EFI_BAD_BUFFER_SIZE,
  BLOCK_SIZE / 2
};
STATIC RW_TEST_CONTEXT  BadOffsetTestLbaMid = {
  NUM_BLOCKS / 2,
  BLOCK_SIZE,
  1,
  EFI_BAD_BUFFER_SIZE,
  0
};

// High Lba Tests
STATIC RW_TEST_CONTEXT  SimpleTest1LbaHi = {
  NUM_BLOCKS - 1,
  0,
  BLOCK_SIZE / 2,
  EFI_SUCCESS,
  BLOCK_SIZE / 2
};
STATIC RW_TEST_CONTEXT  SimpleTest2LbaHi = {
  NUM_BLOCKS - 1,
  BLOCK_SIZE / 4,
  BLOCK_SIZE / 2,
  EFI_SUCCESS,
  BLOCK_SIZE / 2
};
STATIC RW_TEST_CONTEXT  FullBlockTestLbaHi = {
  NUM_BLOCKS - 1,
  0,
  BLOCK_SIZE,
  EFI_SUCCESS,
  BLOCK_SIZE
};
STATIC RW_TEST_CONTEXT  ZeroByteTestLbaHi = {
  NUM_BLOCKS - 1,
  0,
  0,
  EFI_BAD_BUFFER_SIZE,
  0
};
STATIC RW_TEST_CONTEXT  CrossBoundaryTestLbaHi = {
  NUM_BLOCKS - 1,
  BLOCK_SIZE - (BLOCK_SIZE / 2),
  BLOCK_SIZE,
  EFI_BAD_BUFFER_SIZE,
  BLOCK_SIZE / 2
};
STATIC RW_TEST_CONTEXT  BadOffsetTestLbaHi = {
  NUM_BLOCKS - 1,
  BLOCK_SIZE - (BLOCK_SIZE / 2),
  BLOCK_SIZE,
  EFI_BAD_BUFFER_SIZE,
  BLOCK_SIZE / 2
};

// Using Lba that is out of bounds
STATIC RW_TEST_CONTEXT  BadLbaTest = {
  NUM_BLOCKS,
  0,
  1,
  EFI_BAD_BUFFER_SIZE,
  0
};

/**
  Performs setup for Fvb GetAttributes tests.

  Sets the attributes portion of the TestVariablePartition to MOCK_ATTRIBUTES.
  Does not set the corresponding portion of the TestFlashStorage.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            Setup succeeded.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbGetAttributesTestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_FIRMWARE_VOLUME_HEADER  *TestVariableVH;

  TestVariableVH             = (EFI_FIRMWARE_VOLUME_HEADER *)TestVariablePartition;
  TestVariableVH->Attributes = MOCK_ATTRIBUTES;

  return UNIT_TEST_PASSED;
}

/**
  Tests that GetAttributes checks for invalid inputs correctly.

  Note: the functionality that is tested by these tests is not specified
  by the UEFI spec but is based on the implementation of the FvbDxe driver.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbGetAttributesInvalidTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  Status = Private->FvbInstance.GetAttributes (&Private->FvbInstance, NULL);

  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Test GetAttributes returns correct value.

  Assumes FvbGetAttributesTestSetup was called before this test.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbGetAttributesTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  EFI_FVB_ATTRIBUTES_2  Attributes;

  Status = Private->FvbInstance.GetAttributes (
                                  &Private->FvbInstance,
                                  &Attributes
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (MOCK_ATTRIBUTES, Attributes);

  return UNIT_TEST_PASSED;
}

/**
  Tests SetAttributes functionality.

  Note: NVIDIA's FvbDxe does not currently support SetAttributes

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbSetAttributesTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  // Right now SetAttributes is not supported by FvbDxe
  UT_ASSERT_STATUS_EQUAL (
    Private->FvbInstance.SetAttributes (&Private->FvbInstance, NULL),
    EFI_UNSUPPORTED
    );

  return UNIT_TEST_PASSED;
}

/**
  Tests that GetPhysicalAddress checks for invalid inputs correctly.

  Note: the functionality that is tested by these tests is not specified
  by the UEFI spec but is based on the implementation of the Fvb driver.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbGetPhysicalAddressInvalidTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  Status = Private->FvbInstance.GetPhysicalAddress (&Private->FvbInstance, NULL);

  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Tests GetPhysicalAddress functionality.

  GetPhysicalAddress should return a pointer to the start of the in-memory
  buffer used by the FvbDxe driver.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbGetPhysicalAddressTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  Address;

  // Make sure the variable partition was correctly memory mapped
  Status = Private->FvbInstance.GetPhysicalAddress (
                                  &Private->FvbInstance,
                                  &Address
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL ((EFI_PHYSICAL_ADDRESS)(TestVariablePartition), Address);

  return UNIT_TEST_PASSED;
}

/**
  Tests that GetBlockSize checks for invalid inputs correctly.

  Note: the functionality that is tested by these tests is not specified
  by the UEFI spec but is based on the implementation of the Fvb driver.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbGetBlockSizeInvalidTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINTN       BlockSize;
  UINTN       NumberOfBlocks;

  // Test when only NumberOfBlocks is NULL
  Status = Private->FvbInstance.GetBlockSize (
                                  &Private->FvbInstance,
                                  0,
                                  &BlockSize,
                                  NULL
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Test when only BlockSize is NULL
  Status = Private->FvbInstance.GetBlockSize (
                                  &Private->FvbInstance,
                                  0,
                                  NULL,
                                  &NumberOfBlocks
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Test when both NumberOfBlocks and BlockSize are NULL
  Status = Private->FvbInstance.GetBlockSize (
                                  &Private->FvbInstance,
                                  0,
                                  NULL,
                                  NULL
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Test when LBA is out of bounds
  Status = Private->FvbInstance.GetBlockSize (
                                  &Private->FvbInstance,
                                  NUM_BLOCKS,
                                  &BlockSize,
                                  &NumberOfBlocks
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Tests GetBlockSize functionality.

  GetBlockSize returns the block size used by the flash device and
  also how many blocks follow the Lba given, so we need to check that
  the function returns both parts correctly.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbGetBlockSizeTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINTN       BlockSize;
  UINTN       NumberOfBlocks;

  // Check that we get the correct size/numblocks from the start
  Status = Private->FvbInstance.GetBlockSize (
                                  &Private->FvbInstance,
                                  0,
                                  &BlockSize,
                                  &NumberOfBlocks
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (BlockSize, BLOCK_SIZE);
  UT_ASSERT_EQUAL (NumberOfBlocks, NUM_BLOCKS);

  // NumberOfBlocks returned should be the number from the given LBA to the
  // end of the partition (including the block w/the given LBA)
  // Test a middle block
  Status = Private->FvbInstance.GetBlockSize (
                                  &Private->FvbInstance,
                                  NUM_BLOCKS - (NUM_BLOCKS / 2),
                                  &BlockSize,
                                  &NumberOfBlocks
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (BlockSize, BLOCK_SIZE);
  UT_ASSERT_EQUAL (NumberOfBlocks, NUM_BLOCKS / 2);

  // Test the last block
  Status = Private->FvbInstance.GetBlockSize (
                                  &Private->FvbInstance,
                                  NUM_BLOCKS - 1,
                                  &BlockSize,
                                  &NumberOfBlocks
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (BlockSize, BLOCK_SIZE);
  UT_ASSERT_EQUAL (NumberOfBlocks, 1);

  return UNIT_TEST_PASSED;
}

/**
  Performs setup for FvbRead tests.

  Sets the given ExpectedNumBytes of the flash stub and in-memory buffer
  starting at the given offset and given Lba to 0x55, and the rest of the
  memory to 0. This is to indicate the portion of memory that should
  be read so that test functions can verify the correect memory was read.

  Also zeroes out TestBuffer to make it easy to determine what parts of
  the buffer were overwritten during the read.

  @param Context            Pointer to a RW_TEST_CONTEXT struct containing
                            the ExpectedNumBytes, Offset, and Lba used in setup.

  @retval UNIT_TEST_PASSED  Setup finished successfully.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbReadTestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  RW_TEST_CONTEXT  *TestInfo;
  UINT8            *VariableStartAddress;
  UINT8            *FlashStartAddress;

  TestInfo = (RW_TEST_CONTEXT *)Context;

  // Only set areas that should be read so
  // that we know the correct blocks are being used
  VariableStartAddress = TestVariablePartition
                         + MultU64x32 (TestInfo->Lba, BLOCK_SIZE)
                         + TestInfo->Offset;
  FlashStartAddress = TestFlashStorage
                      + MultU64x32 (TestInfo->Lba, BLOCK_SIZE)
                      + TestInfo->Offset;

  // Set both the flash memory as well as the partition buffer used by Private
  ZeroMem (TestFlashStorage, BLOCK_SIZE * NUM_BLOCKS);
  ZeroMem (TestVariablePartition, BLOCK_SIZE * NUM_BLOCKS);
  SetMem (FlashStartAddress, TestInfo->ExpectedNumBytes, (UINT8)0x55);
  SetMem (VariableStartAddress, TestInfo->ExpectedNumBytes, (UINT8)0x55);
  ZeroMem (TestBuffer, BLOCK_SIZE);

  return UNIT_TEST_PASSED;
}

/**
  Tests that FvbRead checks for invalid inputs correctly.

  Note: the functionality that is tested by these tests is not specified
  by the UEFI

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbReadInvalidTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_LBA     Lba;
  UINTN       NumBytes;
  UINTN       Offset;

  // Defaults that should always be valid
  Lba      = 0;
  NumBytes = 1;
  Offset   = 1;

  // Check when Buffer is NULL
  Status = Private->FvbInstance.Read (
                                  &Private->FvbInstance,
                                  Lba,
                                  Offset,
                                  &NumBytes,
                                  NULL
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Check when NumBytes is NULL
  Status = Private->FvbInstance.Read (
                                  &Private->FvbInstance,
                                  Lba,
                                  Offset,
                                  NULL,
                                  TestBuffer
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Check when both NumBytes and Buffer are NULL
  Status = Private->FvbInstance.Read (
                                  &Private->FvbInstance,
                                  Lba,
                                  Offset,
                                  NULL,
                                  NULL
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Check when Offset would cause overlow
  Status = Private->FvbInstance.Read (
                                  &Private->FvbInstance,
                                  Lba,
                                  MAX_UINT64,
                                  &NumBytes,
                                  TestBuffer
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Check when NumBytes would cause overlow
  NumBytes = MAX_UINT64;
  Status   = Private->FvbInstance.Read (
                                    &Private->FvbInstance,
                                    Lba,
                                    Offset,
                                    &NumBytes,
                                    TestBuffer
                                    );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Check when both are at the max
  Status = Private->FvbInstance.Read (
                                  &Private->FvbInstance,
                                  Lba,
                                  MAX_UINT64,
                                  &NumBytes,
                                  TestBuffer
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Tests FvbRead functionality.

  For the given test case, check that exactly correct number of bytes are read
  from the correct location, and that the return status is correct.

  Assumes FvbReadTestSetup was called before this function with the same
  context given to both functions.

  @param Context                      A pointer to a RW_TEST_CONTEXT that
                                      describes the case to be tested as well as
                                      the expected values for returns.

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbReadTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  RW_TEST_CONTEXT  *TestInfo;
  EFI_STATUS       Status;
  UINTN            NumBytes;
  UINTN            UnreadBytes;
  UINT8            *VariableStartAddress;
  UINT8            *FlashStartAddress;

  TestInfo = (RW_TEST_CONTEXT *)Context;

  NumBytes             = TestInfo->NumBytes;
  VariableStartAddress = TestVariablePartition
                         + MultU64x32 (TestInfo->Lba, BLOCK_SIZE)
                         + TestInfo->Offset;
  FlashStartAddress = TestFlashStorage
                      + MultU64x32 (TestInfo->Lba, BLOCK_SIZE)
                      + TestInfo->Offset;

  Status = Private->FvbInstance.Read (
                                  &Private->FvbInstance,
                                  TestInfo->Lba,
                                  TestInfo->Offset,
                                  &NumBytes,
                                  TestBuffer
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, TestInfo->ExpectedStatus);
  UT_ASSERT_EQUAL (NumBytes, TestInfo->ExpectedNumBytes);
  // Need to check both VariableParition private memory buffer
  // And the memory used by the flash stub
  UT_ASSERT_MEM_EQUAL (TestBuffer, VariableStartAddress, NumBytes);
  UT_ASSERT_MEM_EQUAL (TestBuffer, FlashStartAddress, NumBytes);

  // Check that extra data wasn't copied if we had to stop at
  // a block boundary. Even though the returned NumBytes implies this wouldn't
  // be valid data for the client, this is to make sure the implementation
  // isn't performing unnecessary flash reads
  UnreadBytes = TestInfo->NumBytes - NumBytes;
  if (UnreadBytes > 0) {
    UT_ASSERT_TRUE (IsZeroBuffer (TestBuffer + NumBytes, BLOCK_SIZE - NumBytes));
  }

  return UNIT_TEST_PASSED;
}

/**
  Performs setup for FvbWrite tests.

  Zeroes out the flash memory and the in-memory buffer. Sets the test buffer
  to be filled with the value 0x55. This is so that we can determine what
  parts of memory are/aren't written when testing.

  @param Context            Not used by this function

  @retval UNIT_TEST_PASSED  Setup finished successfully.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbWriteTestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  SetMem (TestBuffer, BLOCK_SIZE, (UINT8)0x55);
  ZeroMem (TestFlashStorage, BLOCK_SIZE * NUM_BLOCKS);
  ZeroMem (TestVariablePartition, BLOCK_SIZE * NUM_BLOCKS);

  return UNIT_TEST_PASSED;
}

/**
  Tests that FvbWrite checks for invalid inputs correctly.

  Note: the functionality that is tested by these tests is not specified
  by the UEFI spec but is based on the implementation of the Fvb driver.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbWriteInvalidTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_LBA     Lba;
  UINTN       NumBytes;
  UINTN       Offset;

  // Defaults that should always be valid
  Lba      = 0;
  NumBytes = 1;
  Offset   = 1;

  // Check when Buffer is NULL
  Status = Private->FvbInstance.Write (
                                  &Private->FvbInstance,
                                  Lba,
                                  Offset,
                                  &NumBytes,
                                  NULL
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Check when NumBytes is NULL
  Status = Private->FvbInstance.Write (
                                  &Private->FvbInstance,
                                  Lba,
                                  Offset,
                                  NULL,
                                  TestBuffer
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Check when both NumBytes and Buffer are NULL
  Status = Private->FvbInstance.Write (
                                  &Private->FvbInstance,
                                  Lba,
                                  Offset,
                                  NULL,
                                  NULL
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Check when Offset would cause overlow
  Status = Private->FvbInstance.Write (
                                  &Private->FvbInstance,
                                  Lba,
                                  MAX_UINT64,
                                  &NumBytes,
                                  TestBuffer
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Check when NumBytes would cause overlow
  NumBytes = MAX_UINT64;
  Status   = Private->FvbInstance.Write (
                                    &Private->FvbInstance,
                                    Lba,
                                    Offset,
                                    &NumBytes,
                                    TestBuffer
                                    );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Check when both are at the max
  Status = Private->FvbInstance.Write (
                                  &Private->FvbInstance,
                                  Lba,
                                  MAX_UINT64,
                                  &NumBytes,
                                  TestBuffer
                                  );
  UT_ASSERT_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Tests FvbWrite functionality.

  For the given test case, check that exactly correct number of bytes are
  written to the correct location, and that the return status is correct.

  Assumes FvbWriteTestSetup was called before this function.

  @param Context                      A pointer to a RW_TEST_CONTEXT that
                                      describes the case to be tested as well as
                                      the expected values for returns.

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbWriteTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  RW_TEST_CONTEXT  *TestInfo;
  EFI_STATUS       Status;
  UINTN            FvbOffset;
  UINTN            NumBytes;
  UINTN            UnwrittenEndBytes;
  UINT8            *VariableStartAddress;
  UINT8            *FlashStartAddress;

  TestInfo = (RW_TEST_CONTEXT *)Context;

  NumBytes             = TestInfo->NumBytes;
  FvbOffset            = MultU64x32 (TestInfo->Lba, BLOCK_SIZE) + TestInfo->Offset;
  VariableStartAddress = TestVariablePartition + FvbOffset;
  FlashStartAddress    = TestFlashStorage + FvbOffset;

  Status = Private->FvbInstance.Write (
                                  &Private->FvbInstance,
                                  TestInfo->Lba,
                                  TestInfo->Offset,
                                  &NumBytes,
                                  TestBuffer
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, TestInfo->ExpectedStatus);
  UT_ASSERT_EQUAL (NumBytes, TestInfo->ExpectedNumBytes);
  // This is implementation specific, make sure the write was flushed
  // to the flash and that the Private partition buffer was correctly updated.
  UT_ASSERT_MEM_EQUAL (TestBuffer, VariableStartAddress, NumBytes);
  UT_ASSERT_MEM_EQUAL (TestBuffer, FlashStartAddress, NumBytes);

  // Double check that any space before write region was not written
  if (FvbOffset != 0) {
    UT_ASSERT_TRUE (IsZeroBuffer (TestVariablePartition, FvbOffset));
    UT_ASSERT_TRUE (IsZeroBuffer (TestVariablePartition, FvbOffset));
  }

  // Double check no extra bytes were written after the write region
  UnwrittenEndBytes = (BLOCK_SIZE * NUM_BLOCKS) - (FvbOffset + NumBytes);
  if (UnwrittenEndBytes > 0) {
    UT_ASSERT_TRUE (
      IsZeroBuffer (
        FlashStartAddress + NumBytes,
        UnwrittenEndBytes
        )
      );
    UT_ASSERT_TRUE (
      IsZeroBuffer (
        VariableStartAddress + NumBytes,
        UnwrittenEndBytes
        )
      );
  }

  return UNIT_TEST_PASSED;
}

/**
  Performs setup for FvbEraseBlocks tests.

  Zeroes out the flash memory and the in-memory buffer so that we can determine
  which sections were erased (erased sections are filled with 0xFF). Fills
  TestBuffer with 0xFF to be used to check for which sections are erased.

  @param Context            Not used by this function

  @retval UNIT_TEST_PASSED  Setup finished successfully.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbEraseBlocksTestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  SetMem (TestBuffer, BLOCK_SIZE, (UINT8)0xFF);
  ZeroMem (TestFlashStorage, BLOCK_SIZE * NUM_BLOCKS);
  ZeroMem (TestVariablePartition, BLOCK_SIZE * NUM_BLOCKS);

  return UNIT_TEST_PASSED;
}

/**
  Tests that EraseBlocks checks for invalid inputs correctly.

  If any of the inputs to EraseBlocks are invalid, then none of the regions
  given should be erased.

  Note: the functionality that is tested by these tests is not specified
  by the UEFI spec but is based on the implementation of the Fvb driver.

  Assumes FvbEraseBlocksTestSetup was called before this test.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbEraseBlocksFailureTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  // Check no arguments
  Status = Private->FvbInstance.EraseBlocks (
                                  &Private->FvbInstance,
                                  EFI_LBA_LIST_TERMINATOR
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);
  UT_ASSERT_TRUE (IsZeroBuffer (TestVariablePartition, NUM_BLOCKS * BLOCK_SIZE));
  UT_ASSERT_TRUE (IsZeroBuffer (TestFlashStorage, NUM_BLOCKS * BLOCK_SIZE));

  // Check completely invalid LBA start entry
  Status = Private->FvbInstance.EraseBlocks (
                                  &Private->FvbInstance,
                                  (EFI_LBA)NUM_BLOCKS,
                                  (UINTN)1,
                                  EFI_LBA_LIST_TERMINATOR
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);
  UT_ASSERT_TRUE (IsZeroBuffer (TestVariablePartition, NUM_BLOCKS * BLOCK_SIZE));
  UT_ASSERT_TRUE (IsZeroBuffer (TestFlashStorage, NUM_BLOCKS * BLOCK_SIZE));

  // Check completely invalid num blocks entry
  Status = Private->FvbInstance.EraseBlocks (
                                  &Private->FvbInstance,
                                  (EFI_LBA)0,
                                  (UINTN)0,
                                  EFI_LBA_LIST_TERMINATOR
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);
  UT_ASSERT_TRUE (IsZeroBuffer (TestVariablePartition, NUM_BLOCKS * BLOCK_SIZE));
  UT_ASSERT_TRUE (IsZeroBuffer (TestFlashStorage, NUM_BLOCKS * BLOCK_SIZE));

  // Check when part of LBA range is valid
  Status = Private->FvbInstance.EraseBlocks (
                                  &Private->FvbInstance,
                                  (EFI_LBA)(NUM_BLOCKS - 1),
                                  (UINTN)2,
                                  EFI_LBA_LIST_TERMINATOR
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);
  UT_ASSERT_TRUE (IsZeroBuffer (TestVariablePartition, NUM_BLOCKS * BLOCK_SIZE));
  UT_ASSERT_TRUE (IsZeroBuffer (TestFlashStorage, NUM_BLOCKS * BLOCK_SIZE));

  // Check when one LBA range is valid but the other is not
  Status = Private->FvbInstance.EraseBlocks (
                                  &Private->FvbInstance,
                                  (EFI_LBA)0,
                                  (UINTN)1,
                                  (EFI_LBA)(NUM_BLOCKS - 1),
                                  (UINTN)2,
                                  EFI_LBA_LIST_TERMINATOR
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);
  UT_ASSERT_TRUE (IsZeroBuffer (TestVariablePartition, NUM_BLOCKS * BLOCK_SIZE));
  UT_ASSERT_TRUE (IsZeroBuffer (TestFlashStorage, NUM_BLOCKS * BLOCK_SIZE));

  // Check failure without EFI_LBA_LIST_TERMINATOR
  Status = Private->FvbInstance.EraseBlocks (
                                  &Private->FvbInstance,
                                  (EFI_LBA)0,
                                  (UINTN)1
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);
  UT_ASSERT_TRUE (IsZeroBuffer (TestVariablePartition, NUM_BLOCKS * BLOCK_SIZE));
  UT_ASSERT_TRUE (IsZeroBuffer (TestFlashStorage, NUM_BLOCKS * BLOCK_SIZE));

  return UNIT_TEST_PASSED;
}

/**
  Tests that EraseBlocks works for blocks on the edge of the flash partition.

  Verifies that only the specified blocks were erased and all other blocks
  were left alone.

  Assumes FvbEraseBlocksTestSetup was called before this test.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbEraseBlocksSuccessEdgeTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  // Check blocks at the edge of the range (first and last)
  Status = Private->FvbInstance.EraseBlocks (
                                  &Private->FvbInstance,
                                  (EFI_LBA)0,
                                  (UINTN)1,
                                  (EFI_LBA)(NUM_BLOCKS - 1),
                                  (UINTN)1,
                                  EFI_LBA_LIST_TERMINATOR
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  // Make sure in between blocks are still 0 (not cleared)
  UT_ASSERT_TRUE (
    IsZeroBuffer (
      TestVariablePartition + BLOCK_SIZE,
      BLOCK_SIZE * (NUM_BLOCKS - 2)
      )
    );
  UT_ASSERT_TRUE (
    IsZeroBuffer (
      TestFlashStorage + BLOCK_SIZE,
      BLOCK_SIZE * (NUM_BLOCKS - 2)
      )
    );
  // Make sure that first and last block were cleared (should be set to 0xFF)
  UT_ASSERT_MEM_EQUAL (TestBuffer, TestVariablePartition, BLOCK_SIZE);
  UT_ASSERT_MEM_EQUAL (TestBuffer, TestFlashStorage, BLOCK_SIZE);
  UT_ASSERT_MEM_EQUAL (
    TestBuffer,
    TestVariablePartition + BLOCK_SIZE * (NUM_BLOCKS - 1),
    BLOCK_SIZE
    );
  UT_ASSERT_MEM_EQUAL (
    TestBuffer,
    TestFlashStorage + BLOCK_SIZE * (NUM_BLOCKS - 1),
    BLOCK_SIZE
    );

  return UNIT_TEST_PASSED;
}

/**
  Tests that EraseBlocks works for a region in the middle of flash partition.

  Verifies that only the specified blocks were erased and all other blocks
  were left alone.

  Assumes FvbEraseBlocksTestSetup was called before this test.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FvbEraseBlocksSuccessGeneralTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  // Check a multi block middle range
  Status = Private->FvbInstance.EraseBlocks (
                                  &Private->FvbInstance,
                                  (EFI_LBA)1,
                                  (UINTN)2,
                                  EFI_LBA_LIST_TERMINATOR
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  // Make sure first block and remaining blocks are still 0 (not cleared)
  UT_ASSERT_TRUE (IsZeroBuffer (TestVariablePartition, BLOCK_SIZE));
  UT_ASSERT_TRUE (IsZeroBuffer (TestFlashStorage, BLOCK_SIZE));
  UT_ASSERT_TRUE (
    IsZeroBuffer (
      TestVariablePartition + BLOCK_SIZE * 3,
      BLOCK_SIZE * (NUM_BLOCKS - 3)
      )
    );
  UT_ASSERT_TRUE (
    IsZeroBuffer (
      TestFlashStorage + BLOCK_SIZE * 3,
      BLOCK_SIZE * (NUM_BLOCKS - 3)
      )
    );
  // Make sure that middle range was cleared (should be set to 0xFF)
  UT_ASSERT_MEM_EQUAL (
    TestBuffer,
    TestVariablePartition + BLOCK_SIZE,
    BLOCK_SIZE
    );
  UT_ASSERT_MEM_EQUAL (TestBuffer, TestFlashStorage + BLOCK_SIZE, BLOCK_SIZE);
  UT_ASSERT_MEM_EQUAL (
    TestBuffer,
    TestVariablePartition + BLOCK_SIZE * 2,
    BLOCK_SIZE
    );
  UT_ASSERT_MEM_EQUAL (
    TestBuffer,
    TestFlashStorage + BLOCK_SIZE * 2,
    BLOCK_SIZE
    );

  return UNIT_TEST_PASSED;
}

/**
  Tests that InitializeFvAndVariableStoreHeaders
  checks for invalid inputs correctly.

  Note: the functionality that is tested by these tests is not specified
  by the UEFI spec but is based on the implementation of the Fvb driver.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
InitializeFvAndVariableStoreHeadersInvalidTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                  Status;
  UINT32                      OriginalPcdValue;
  EFI_FIRMWARE_VOLUME_HEADER  *VariableVH;

  VariableVH       = (EFI_FIRMWARE_VOLUME_HEADER *)Private->VariablePartition;
  OriginalPcdValue = PcdGet32 (PcdFlashNvStorageVariableSize);

  Status = InitializeFvAndVariableStoreHeaders (NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  PcdSet32S (PcdFlashNvStorageVariableSize, 0);
  Status = InitializeFvAndVariableStoreHeaders (VariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_OUT_OF_RESOURCES);

  PcdSet32S (PcdFlashNvStorageVariableSize, BLOCK_SIZE - 1);
  Status = InitializeFvAndVariableStoreHeaders (VariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_OUT_OF_RESOURCES);

  PcdSet32S (PcdFlashNvStorageVariableSize, OriginalPcdValue);

  return UNIT_TEST_PASSED;
}

/**
  Tests that InitializeFvAndVariableStoreHeaders correctly initializes the
  firmware volume and variable partition headers.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
InitializeFvAndVariableStoreHeadersTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                  Status;
  EFI_FIRMWARE_VOLUME_HEADER  *TestVariableVH;
  EFI_FIRMWARE_VOLUME_HEADER  *PrivateVariableVH;
  VARIABLE_STORE_HEADER       *TestVariableVSH;
  EFI_FVB_ATTRIBUTES_2        ExpectedAttributes;

  PrivateVariableVH = (EFI_FIRMWARE_VOLUME_HEADER *)Private->VariablePartition;

  Status = InitializeFvAndVariableStoreHeaders (PrivateVariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  // Check the Volume Header Settings
  TestVariableVH = (EFI_FIRMWARE_VOLUME_HEADER *)TestVariablePartition;

  // Taken from FvbDxe.c
  ExpectedAttributes = (
                        EFI_FVB2_READ_ENABLED_CAP   |
                        EFI_FVB2_READ_STATUS        |
                        EFI_FVB2_STICKY_WRITE       |
                        EFI_FVB2_MEMORY_MAPPED      |
                        EFI_FVB2_ERASE_POLARITY     |
                        EFI_FVB2_WRITE_STATUS       |
                        EFI_FVB2_WRITE_ENABLED_CAP
                        );

  UT_ASSERT_TRUE (
    CompareGuid (
      &TestVariableVH->FileSystemGuid,
      &gEfiSystemNvDataFvGuid
      )
    );
  UT_ASSERT_EQUAL (TestVariableVH->FvLength, BLOCK_SIZE * NUM_BLOCKS);
  UT_ASSERT_EQUAL (TestVariableVH->Signature, EFI_FVH_SIGNATURE);
  UT_ASSERT_EQUAL (TestVariableVH->Attributes, ExpectedAttributes);
  UT_ASSERT_EQUAL (
    TestVariableVH->HeaderLength,
    sizeof (EFI_FIRMWARE_VOLUME_HEADER) + sizeof (EFI_FV_BLOCK_MAP_ENTRY)
    );
  UT_ASSERT_EQUAL (TestVariableVH->Revision, EFI_FVH_REVISION);
  UT_ASSERT_EQUAL (TestVariableVH->BlockMap[0].NumBlocks, NUM_BLOCKS);
  UT_ASSERT_EQUAL (TestVariableVH->BlockMap[0].Length, BLOCK_SIZE);
  UT_ASSERT_EQUAL (TestVariableVH->BlockMap[1].NumBlocks, 0);
  UT_ASSERT_EQUAL (TestVariableVH->BlockMap[1].Length, 0);
  UT_ASSERT_EQUAL (TestVariableVH->Checksum, (UINT16)0xE3FE);

  // Check the Variable Store Header Settings
  TestVariableVSH = (VARIABLE_STORE_HEADER *)((UINTN)TestVariableVH
                                              + TestVariableVH->HeaderLength);
  UT_ASSERT_TRUE (
    CompareGuid (
      &TestVariableVSH->Signature,
      &gEfiAuthenticatedVariableGuid
      )
    );
  UT_ASSERT_EQUAL (
    TestVariableVSH->Size,
    BLOCK_SIZE * NUM_BLOCKS - TestVariableVH->HeaderLength
    );
  UT_ASSERT_EQUAL (TestVariableVSH->Format, VARIABLE_STORE_FORMATTED);
  UT_ASSERT_EQUAL (TestVariableVSH->State, VARIABLE_STORE_HEALTHY);

  // Make sure everything was flushed to the flash device correctly
  UT_ASSERT_MEM_EQUAL (
    TestVariablePartition,
    TestFlashStorage,
    TestVariableVH->HeaderLength + TestVariableVSH->Size
    );

  return UNIT_TEST_PASSED;
}

/**
  Performs setup for FvHeader validation tests.

  Calls InitializeFvAndVariableStoreHeaders on the variable partition.

  @param Context            Not used by this function

  @retval UNIT_TEST_PASSED  Setup finished successfully.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
ValidateFvHeaderTestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                  Status;
  EFI_FIRMWARE_VOLUME_HEADER  *PrivateVariableVH;

  PrivateVariableVH = (EFI_FIRMWARE_VOLUME_HEADER *)Private->VariablePartition;
  Status            = InitializeFvAndVariableStoreHeaders (PrivateVariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  Tests that ValidateFvHeader correctly validates/invalidates headers.

  First tests that a valid header is successfully validated, then tests a
  variety of invalid headers to make sure they are invalidated.

  Assumes ValidateFvHeaderTestSetup was called before this test.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
ValidateFvHeaderTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                  Status;
  EFI_FIRMWARE_VOLUME_HEADER  *TestVariableVH;
  EFI_FIRMWARE_VOLUME_HEADER  *TestFlashVH;
  VARIABLE_STORE_HEADER       *TestVariableVSH;
  VARIABLE_STORE_HEADER       *TestFlashVSH;

  TestVariableVH = (EFI_FIRMWARE_VOLUME_HEADER *)Private->VariablePartition;
  TestFlashVH    = (EFI_FIRMWARE_VOLUME_HEADER *)TestFlashStorage;

  // Should be valid after InitializeFvAndVariableStoreHeaders runs
  Status = ValidateFvHeader (TestVariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  // Test that when we corrupt various header values, the
  // validation fails by returning EFI_NOT_FOUND.
  // Each value is restored after being corrupted
  // to test the values independently
  TestVariableVH->Revision = 0;
  Status                   = ValidateFvHeader (TestVariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);
  TestVariableVH->Revision = TestFlashVH->Revision;

  TestVariableVH->Signature = 0;
  Status                    = ValidateFvHeader (TestVariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);
  TestVariableVH->Signature = TestFlashVH->Signature;

  TestVariableVH->FvLength = BLOCK_SIZE * NUM_BLOCKS - 1;
  Status                   = ValidateFvHeader (TestVariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);
  TestVariableVH->FvLength = TestFlashVH->FvLength;

  TestVariableVH->FileSystemGuid = ZeroGuid;
  Status                         = ValidateFvHeader (TestVariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);
  TestVariableVH->FileSystemGuid = TestFlashVH->FileSystemGuid;

  TestVariableVSH = (VARIABLE_STORE_HEADER *)((UINTN)TestVariableVH
                                              + TestVariableVH->HeaderLength);
  TestFlashVSH = (VARIABLE_STORE_HEADER *)((UINTN)TestFlashVH
                                           + TestFlashVH->HeaderLength);

  TestVariableVSH->Signature = ZeroGuid;
  Status                     = ValidateFvHeader (TestVariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);
  TestVariableVSH->Signature = TestFlashVSH->Signature;

  TestVariableVSH->Size = BLOCK_SIZE * NUM_BLOCKS
                          - TestVariableVH->HeaderLength
                          - 1;
  Status = ValidateFvHeader (TestVariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  return UNIT_TEST_PASSED;
}

/**
  Performs setup for tests that use the faulty flash mock.

  Deallocates the working flash stub and allocates the faulty flash stub.
**/
STATIC
VOID
EFIAPI
FaultyFlashSetup (
  VOID
  )
{
  FlashStubDestroy (Private->BlockIo);
  FaultyFlashStubInitialize (
    TestFlashStorage,
    NUM_BLOCKS * BLOCK_SIZE,
    BLOCK_SIZE,
    IO_ALIGN,
    &Private->BlockIo
    );
}

/**
  Performs cleanup for tests that use the faulty flash mock.

  Deallocates the faulty flash stub and allocates the working flash stub.
**/
STATIC
VOID
EFIAPI
FaultyFlashCleanup (
  VOID
  )
{
  FaultyFlashStubDestroy (Private->BlockIo);
  FlashStubInitialize (
    TestFlashStorage,
    NUM_BLOCKS * BLOCK_SIZE,
    BLOCK_SIZE,
    IO_ALIGN,
    &Private->BlockIo
    );
}

/**
  Tests that GetAttributes properly deals with a faulty flash device.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FaultyFlashGetAttributesTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  EFI_FVB_ATTRIBUTES_2  Attributes;

  // GetAttributes doesn't interact with the flash device so it should be ok
  Status = Private->FvbInstance.GetAttributes (
                                  &Private->FvbInstance,
                                  &Attributes
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (MOCK_ATTRIBUTES, Attributes);

  return UNIT_TEST_PASSED;
}

/**
  Tests that SetAttributes properly deals with a faulty flash device.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FaultyFlashSetAttributesTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  // Right now SetAttributes is not supported by FvbDxe
  UT_ASSERT_STATUS_EQUAL (
    Private->FvbInstance.SetAttributes (&Private->FvbInstance, NULL),
    EFI_UNSUPPORTED
    );

  return UNIT_TEST_PASSED;
}

/**
  Tests that GetPhysicalAddress properly deals with a faulty flash device.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FaultyFlashGetPhysicalAddressTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  Address;

  // GetPhysicalAddress doesn't interact with the flash device so should be ok
  Status = Private->FvbInstance.GetPhysicalAddress (
                                  &Private->FvbInstance,
                                  &Address
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL ((EFI_PHYSICAL_ADDRESS)(TestVariablePartition), Address);

  return UNIT_TEST_PASSED;
}

/**
  Tests that GetBlockSize properly deals with a faulty flash device.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FaultyFlashGetBlockSizeTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINTN       BlockSize;
  UINTN       NumberOfBlocks;

  // GetBlockSize doesn't interact with the flash device so it should be ok
  Status = Private->FvbInstance.GetBlockSize (
                                  &Private->FvbInstance,
                                  0,
                                  &BlockSize,
                                  &NumberOfBlocks
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (BlockSize, BLOCK_SIZE);
  UT_ASSERT_EQUAL (NumberOfBlocks, NUM_BLOCKS);

  return UNIT_TEST_PASSED;
}

/**
  Tests that FvbRead properly deals with a faulty flash device.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FaultyFlashReadTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINTN       NumBytes;

  NumBytes = 1;

  // Reading doesn't currently interact with flash so it should be ok
  Status = Private->FvbInstance.Read (
                                  &Private->FvbInstance,
                                  0,
                                  0,
                                  &NumBytes,
                                  TestBuffer
                                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  Tests that FvbWrite properly deals with a faulty flash device.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FaultyFlashWriteTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN  NumBytes;

  NumBytes = 1;

  // Write tries to flush to the flash device so we should get an error
  UT_EXPECT_ASSERT_FAILURE (
    Private->FvbInstance.Write (
                           &Private->FvbInstance,
                           0,
                           0,
                           &NumBytes,
                           TestBuffer
                           ),
    NULL
    );

  return UNIT_TEST_PASSED;
}

/**
  Tests that EraseBlocks properly deals with a faulty flash device.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FaultyFlashEraseBlocksTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  // EraseBlocks tries to flush to the flash device so we should get an error
  UT_EXPECT_ASSERT_FAILURE (
    Private->FvbInstance.EraseBlocks (
                           &Private->FvbInstance,
                           (EFI_LBA)0,
                           (UINTN)1,
                           EFI_LBA_LIST_TERMINATOR
                           ),
    NULL
    );

  return UNIT_TEST_PASSED;
}

/**
  Tests that InitializeFvAndVariableStoreHeaders
  properly deals with a faulty flash device.

  @param Context                      Not used by this function

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FaultyFlashInitializeFvHeaderTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                  Status;
  EFI_FIRMWARE_VOLUME_HEADER  *PrivateVariableVH;

  PrivateVariableVH = (EFI_FIRMWARE_VOLUME_HEADER *)Private->VariablePartition;

  // Initializing the header tries to flush
  // to the flash device so we should get an error
  Status = InitializeFvAndVariableStoreHeaders (PrivateVariableVH);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  Initializes data that will be used for the Fvb tests.

  Allocates space for flash storage, in-memory variable partition, and a buffer
  used for testing. Sets up a flash device stub and then initializes the
  NVIDIA_FVB_PRIVATE_DATA used in the Fvb functions.
**/
STATIC
VOID
InitTestData (
  VOID
  )
{
  Private = (NVIDIA_FVB_PRIVATE_DATA *)
            AllocatePool (sizeof (NVIDIA_FVB_PRIVATE_DATA));

  TestFlashStorage      = AllocatePool (NUM_BLOCKS * BLOCK_SIZE);
  TestVariablePartition = AllocatePool (NUM_BLOCKS * BLOCK_SIZE);
  TestBuffer            = AllocatePool (BLOCK_SIZE);

  ZeroMem (TestFlashStorage, NUM_BLOCKS * BLOCK_SIZE);
  ZeroMem (TestVariablePartition, NUM_BLOCKS * BLOCK_SIZE);
  ZeroMem (TestBuffer, BLOCK_SIZE);

  FlashStubInitialize (
    TestFlashStorage,
    NUM_BLOCKS * BLOCK_SIZE,
    BLOCK_SIZE,
    IO_ALIGN,
    &Private->BlockIo
    );
  PcdSet32S (PcdFlashNvStorageVariableSize, NUM_BLOCKS * BLOCK_SIZE);

  Private->VariablePartition = TestVariablePartition;

  Private->NumBlocks            = NUM_BLOCKS;
  Private->PartitionStartingLBA = 0;

  Private->FvbInstance.GetAttributes      = FvbGetAttributes;
  Private->FvbInstance.SetAttributes      = FvbSetAttributes;
  Private->FvbInstance.GetPhysicalAddress = FvbGetPhysicalAddress;
  Private->FvbInstance.GetBlockSize       = FvbGetBlockSize;
  Private->FvbInstance.Read               = FvbRead;
  Private->FvbInstance.Write              = FvbWrite;
  Private->FvbInstance.EraseBlocks        = FvbEraseBlocks;
  Private->FvbInstance.ParentHandle       = NULL;
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
  if (Private != NULL) {
    FlashStubDestroy (Private->BlockIo);
    FreePool (Private);
  }

  if (TestFlashStorage != NULL) {
    FreePool (TestFlashStorage);
  }

  if (TestVariablePartition != NULL) {
    FreePool (TestVariablePartition);
  }

  if (TestBuffer != NULL) {
    FreePool (TestBuffer);
  }

  Private = NULL;
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
  UNIT_TEST_SUITE_HANDLE      FvbGetSetTestSuite;
  UNIT_TEST_SUITE_HANDLE      FvbReadTestSuite;
  UNIT_TEST_SUITE_HANDLE      FvbWriteTestSuite;
  UNIT_TEST_SUITE_HANDLE      FvbEraseBlocksTestSuite;
  UNIT_TEST_SUITE_HANDLE      FvbFvHeaderTestSuite;
  UNIT_TEST_SUITE_HANDLE      FvbFaultyFlashTestSuite;

  Fw = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  InitTestData ();

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
    goto EXIT;
  }

  // Populate the Fvb Getter/Setter Unit Test Suite.
  Status = CreateUnitTestSuite (
             &FvbGetSetTestSuite,
             Fw,
             "Fvb Getter/Setter Tests",
             "FvbDxe.FvbGetSetTestSuite",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for FvbTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  // AddTestCase Args:
  //  Suite | Description
  //  Class Name | Function
  //  Pre | Post | Context
  AddTestCase (
    FvbGetSetTestSuite,
    "GetAttributes Test",
    "FvbGetAttributesTest",
    FvbGetAttributesTest,
    FvbGetAttributesTestSetup,
    NULL,
    NULL
    );
  AddTestCase (
    FvbGetSetTestSuite,
    "GetAttributes Invalid Test",
    "FvbGetAttributesInvalidTest",
    FvbGetAttributesInvalidTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbGetSetTestSuite,
    "SetAttributes Test",
    "FvbSetAttributesTest",
    FvbSetAttributesTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbGetSetTestSuite,
    "GetPhysicalAddress Test",
    "FvbGetPhysicalAddressTest",
    FvbGetPhysicalAddressTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbGetSetTestSuite,
    "GetPhysicalAddress Invalid Test",
    "FvbGetPhysicalAddressInvalidTest",
    FvbGetPhysicalAddressInvalidTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbGetSetTestSuite,
    "GetBlockSize Test",
    "FvbGetBlockSizeTest",
    FvbGetBlockSizeTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbGetSetTestSuite,
    "GetBlockSize Invalid Test",
    "FvbGetBlockSizeInvalidTest",
    FvbGetBlockSizeInvalidTest,
    NULL,
    NULL,
    NULL
    );

  // Populate the Fvb REad Unit Test Suite.
  Status = CreateUnitTestSuite (
             &FvbReadTestSuite,
             Fw,
             "Fvb Read Tests",
             "FvbDxe.FvbReadTestSuite",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for FvbReadTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  // AddTestCase Args:
  //  Suite | Description
  //  Class Name | Function
  //  Pre | Post | Context
  AddTestCase (
    FvbReadTestSuite,
    "Simple Read Test 1 - Lowest Lba",
    "SimpleTest1LbaLo",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &SimpleTest1LbaLo
    );
  AddTestCase (
    FvbReadTestSuite,
    "Simple Read Test 2 - Lowest Lba",
    "SimpleTest2LbaLo",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &SimpleTest2LbaLo
    );
  AddTestCase (
    FvbReadTestSuite,
    "Full Block Read Test - Lowest Lba",
    "FullBlockTestLbaLo",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &FullBlockTestLbaLo
    );
  AddTestCase (
    FvbReadTestSuite,
    "Zero Byte Read - Lowest Lba",
    "ZeroByteTestLbaLo",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &ZeroByteTestLbaLo
    );
  AddTestCase (
    FvbReadTestSuite,
    "Cross Boundary Read Test - Lowest Lba",
    "CrossBoundaryTestLbaLo",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &CrossBoundaryTestLbaLo
    );
  AddTestCase (
    FvbReadTestSuite,
    "Bad Offset Read Test - Lowest Lba",
    "BadOffsetTestLbaLo",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &BadOffsetTestLbaLo
    );

  AddTestCase (
    FvbReadTestSuite,
    "Simple Read Test 1 - Middle Lba",
    "SimpleTest1LbaMid",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &SimpleTest1LbaMid
    );
  AddTestCase (
    FvbReadTestSuite,
    "Simple Read Test 2 - Middle Lba",
    "SimpleTest2LbaMid",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &SimpleTest2LbaMid
    );
  AddTestCase (
    FvbReadTestSuite,
    "Full Block Read Test - Middle Lba",
    "FullBlockTestLbaMid",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &FullBlockTestLbaMid
    );
  AddTestCase (
    FvbReadTestSuite,
    "Zero Byte Read - Middle Lba",
    "ZeroByteTestLbaMid",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &ZeroByteTestLbaMid
    );
  AddTestCase (
    FvbReadTestSuite,
    "Cross Boundary Read Test - Middle Lba",
    "CrossBoundaryTestLbaMid",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &CrossBoundaryTestLbaMid
    );
  AddTestCase (
    FvbReadTestSuite,
    "Bad Offset Read Test - Middle Lba",
    "BadOffsetTestLbaMid",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &BadOffsetTestLbaMid
    );

  AddTestCase (
    FvbReadTestSuite,
    "Simple Read Test 1 - Highest Lba",
    "SimpleTest1LbaHi",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &SimpleTest1LbaHi
    );
  AddTestCase (
    FvbReadTestSuite,
    "Simple Read Test 2 - Highest Lba",
    "SimpleTest2LbaHi",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &SimpleTest2LbaHi
    );
  AddTestCase (
    FvbReadTestSuite,
    "Full Block Read Test - Highest Lba",
    "FullBlockTestLbaHi",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &FullBlockTestLbaHi
    );
  AddTestCase (
    FvbReadTestSuite,
    "Zero Byte Read - Highest Lba",
    "ZeroByteTestLbaHi",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &ZeroByteTestLbaHi
    );
  AddTestCase (
    FvbReadTestSuite,
    "Cross Boundary Read Test - Highest Lba",
    "CrossBoundaryTestLbaHi",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &CrossBoundaryTestLbaHi
    );
  AddTestCase (
    FvbReadTestSuite,
    "Bad Offset Read Test - Highest Lba",
    "BadOffsetTestLbaHi",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &BadOffsetTestLbaHi
    );

  AddTestCase (
    FvbReadTestSuite,
    "Bad Lba Read Test",
    "BadLbaTest",
    FvbReadTest,
    FvbReadTestSetup,
    NULL,
    &BadLbaTest
    );
  AddTestCase (
    FvbReadTestSuite,
    "Read Invalid Test",
    "FvbReadInvalidTest",
    FvbReadInvalidTest,
    NULL,
    NULL,
    NULL
    );

  // Populate the Fvb Write Unit Test Suite.
  Status = CreateUnitTestSuite (
             &FvbWriteTestSuite,
             Fw,
             "Fvb Write Tests",
             "FvbDxe.FvbWriteTestSuite",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for FvbWriteTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  // AddTestCase Args:
  //  Suite | Description
  //  Class Name | Function
  //  Pre | Post | Context
  AddTestCase (
    FvbWriteTestSuite,
    "Simple Write Test 1 - Lowest Lba",
    "SimpleTest1LbaLo",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &SimpleTest1LbaLo
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Simple Write Test 2 - Lowest Lba",
    "SimpleTest2LbaLo",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &SimpleTest2LbaLo
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Full Block Write Test - Lowest Lba",
    "FullBlockTestLbaLo",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &FullBlockTestLbaLo
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Zero Byte Write - Lowest Lba",
    "ZeroByteTestLbaLo",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &ZeroByteTestLbaLo
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Cross Boundary Write Test - Lowest Lba",
    "CrossBoundaryTestLbaLo",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &CrossBoundaryTestLbaLo
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Bad Offset Write Test - Lowest Lba",
    "BadOffsetTestLbaLo",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &BadOffsetTestLbaLo
    );

  AddTestCase (
    FvbWriteTestSuite,
    "Simple Write Test 1 - Middle Lba",
    "SimpleTest1LbaMid",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &SimpleTest1LbaMid
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Simple Write Test 2 - Middle Lba",
    "SimpleTest2LbaMid",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &SimpleTest2LbaMid
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Full Block Write Test - Middle Lba",
    "FullBlockTestLbaMid",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &FullBlockTestLbaMid
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Zero Byte Write - Middle Lba",
    "ZeroByteTestLbaMid",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &ZeroByteTestLbaMid
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Cross Boundary Write Test - Middle Lba",
    "CrossBoundaryTestLbaMid",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &CrossBoundaryTestLbaMid
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Bad Offset Write Test - Middle Lba",
    "BadOffsetTestLbaMid",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &BadOffsetTestLbaMid
    );

  AddTestCase (
    FvbWriteTestSuite,
    "Simple Write Test 1 - Highest Lba",
    "SimpleTest1LbaHi",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &SimpleTest1LbaHi
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Simple Write Test 2 - Highest Lba",
    "SimpleTest2LbaHi",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &SimpleTest2LbaHi
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Full Block Write Test - Highest Lba",
    "FullBlockTestLbaHi",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &FullBlockTestLbaHi
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Zero Byte Write - Highest Lba",
    "ZeroByteTestLbaHi",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &ZeroByteTestLbaHi
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Cross Boundary Write Test - Highest Lba",
    "CrossBoundaryTestLbaHi",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &CrossBoundaryTestLbaHi
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Bad Offset Write Test - Highest Lba",
    "BadOffsetTestLbaHi",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &BadOffsetTestLbaHi
    );

  AddTestCase (
    FvbWriteTestSuite,
    "Bad Lba Write Test",
    "BadLbaTest",
    FvbWriteTest,
    FvbWriteTestSetup,
    NULL,
    &BadLbaTest
    );
  AddTestCase (
    FvbWriteTestSuite,
    "Write Invalid Test",
    "FvbWriteInvalidTest",
    FvbWriteInvalidTest,
    NULL,
    NULL,
    NULL
    );

  // Populate the Fvb Erase Blocks Unit Test Suite.
  Status = CreateUnitTestSuite (
             &FvbEraseBlocksTestSuite,
             Fw,
             "Fvb EraseBlocks Tests",
             "FvbDxe.FvbEraseBlocksTestSuite",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for FvbEraseBlocksTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    FvbEraseBlocksTestSuite,
    "EraseBlocks Failure Tests",
    "FvbEraseBlocksFailureTest",
    FvbEraseBlocksFailureTest,
    FvbEraseBlocksTestSetup,
    NULL,
    NULL
    );
  AddTestCase (
    FvbEraseBlocksTestSuite,
    "EraseBlocks Success Edge Tests",
    "FvbEraseBlocksSuccessEdgeTest",
    FvbEraseBlocksSuccessEdgeTest,
    FvbEraseBlocksTestSetup,
    NULL,
    NULL
    );
  AddTestCase (
    FvbEraseBlocksTestSuite,
    "EraseBlocks Success General Tests",
    "FvbEraseBlocksSuccessGeneralTest",
    FvbEraseBlocksSuccessGeneralTest,
    FvbEraseBlocksTestSetup,
    NULL,
    NULL
    );

  // Populate the Fvb Fv Header Unit Test Suite.
  Status = CreateUnitTestSuite (
             &FvbFvHeaderTestSuite,
             Fw,
             "Fvb Fv Header Tests",
             "FvbDxe.FvbFvHeaderTestSuite",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for FvbFvHeaderTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    FvbFvHeaderTestSuite,
    "Initialize Fv Header Invalid Tests",
    "InitializeFvAndVariableStoreHeadersInvalidTest",
    InitializeFvAndVariableStoreHeadersInvalidTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbFvHeaderTestSuite,
    "Initialize Fv Header Tests",
    "InitializeFvAndVariableStoreHeadersTest",
    InitializeFvAndVariableStoreHeadersTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbFvHeaderTestSuite,
    "Validate Fv Header Tests",
    "ValidateFvHeaderTest",
    ValidateFvHeaderTest,
    ValidateFvHeaderTestSetup,
    NULL,
    NULL
    );

  // Populate the Fvb Faulty Flash Unit Test Suite.
  Status = CreateUnitTestSuite (
             &FvbFaultyFlashTestSuite,
             Fw,
             "Fvb Faulty Flash Tests",
             "FvbDxe.FvbFaultyFlashTestSuite",
             FaultyFlashSetup,
             FaultyFlashCleanup
             );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for FvbFaultyFlashTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    FvbFaultyFlashTestSuite,
    "Faulty Flash GetAttributes Test",
    "FaultyFlashGetAttributesTest",
    FaultyFlashGetAttributesTest,
    FvbGetAttributesTestSetup,
    NULL,
    NULL
    );
  AddTestCase (
    FvbFaultyFlashTestSuite,
    "Faulty Flash SetAttributes Test",
    "FaultyFlashSetAttributesTest",
    FaultyFlashSetAttributesTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbFaultyFlashTestSuite,
    "Faulty Flash GetPhysicalAddress Test",
    "FaultyFlashGetPhysicalAddressTest",
    FaultyFlashGetPhysicalAddressTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbFaultyFlashTestSuite,
    "Faulty Flash GetBlockSize Test",
    "FaultyFlashGetBlockSizeTest",
    FaultyFlashGetBlockSizeTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbFaultyFlashTestSuite,
    "Faulty Flash Read Test",
    "FaultyFlashReadTest",
    FaultyFlashReadTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbFaultyFlashTestSuite,
    "Faulty Flash Write Test",
    "FaultyFlashWriteTest",
    FaultyFlashWriteTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbFaultyFlashTestSuite,
    "Faulty Flash EraseBlocks Test",
    "FaultyFlashEraseBlocksTest",
    FaultyFlashEraseBlocksTest,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    FvbFaultyFlashTestSuite,
    "Faulty Flash Initialize FvHeader Test",
    "FaultyFlashInitializeFvHeaderTest",
    FaultyFlashInitializeFvHeaderTest,
    NULL,
    NULL,
    NULL
    );

  // Execute the tests.
  Status = RunAllTestSuites (Fw);

EXIT:
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
