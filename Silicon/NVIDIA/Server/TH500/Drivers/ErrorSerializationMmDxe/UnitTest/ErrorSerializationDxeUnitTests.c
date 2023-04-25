/** @file
  Unit tests of ErrorSerializationMmDxe Driver.

  Tests are run using a flash stub, including tests for both
  a working flash device and a faulty flash device.

  Copyright (c) 2020-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>
#include <Library/IoLib.h> // MMIO calls

// So that we can reference ErrorSerialization declarations
#include "../ErrorSerializationMm.h"
#include "../ErrorSerializationMemory.h"

#include <HostBasedTestStubLib/NorFlashStubLib.h>
#include <HostBasedTestStubLib/HobStubLib.h>
#include <HostBasedTestStubLib/PlatformResourceStubLib.h>
#include <HostBasedTestStubLib/StandaloneMmOpteeStubLib.h>

#include <ErrorSerializationDxeTestPrivate.h>

#include <Library/StandaloneMmOpteeDeviceMem.h>
typedef struct {
  EFI_HOB_GUID_TYPE    GUID;
  STMM_COMM_BUFFERS    Buffers;
} STMM_COMM_BUFFERS_DATA;

STATIC   STMM_COMM_BUFFERS_DATA  StmmCommBuffersData;

#define UNIT_TEST_APP_NAME     "ErrorSerializationMmDxe Unit Test Application"
#define UNIT_TEST_APP_VERSION  "0.1"

#define BLOCK_SIZE                  SIZE_64KB
#define NUM_BLOCKS                  8
#define TOTAL_NOR_FLASH_SIZE        (NUM_BLOCKS * BLOCK_SIZE)
#define ERROR_LOG_INFO_BUFFER_SIZE  SIZE_16KB
#define ERST_BUFFER_SIZE            (sizeof(ERST_COMM_STRUCT) + ERROR_LOG_INFO_BUFFER_SIZE)

void
PrintCper (
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper,
  char                            *Header
  );

EFI_PHYSICAL_ADDRESS  MockCpuBlAddr;
UINT32                MockNorErstOffset;
UINT32                MockNorErstSize;

UNIT_TEST_STATUS
UnitTestMockNorFlashProtocol (
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  UINT32                     NorErstOffset,
  UINT32                     NorErstSize
  )
{
  EFI_STATUS  Status;

  Status = MockGetSocketNorFlashProtocol (0, NorFlashProtocol);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  MockGetCpuBlParamsAddrStMm (&MockCpuBlAddr, EFI_SUCCESS);

  Status = MockGetPartitionInfoStMm ((UINTN)&MockCpuBlAddr, TEGRABL_ERST, 0, NorErstOffset, NorErstSize, EFI_SUCCESS);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

BOOLEAN
IsBufferValue (
  UINT8   *Buffer,
  UINT32  Size,
  UINT8   Value
  )
{
  while (Size > 0) {
    if (*Buffer != Value) {
      return FALSE;
    }

    ++Buffer;
    --Size;
  }

  return TRUE;
}

// Untested but potentially testable?
// 428-430 ReclaimBlock - when outgoing present and RelocateOutgoing fails
// *** 501 FindFreeSpace - when most wasted block isn't first wasted block found
// 521 FindFreeSpace - when EraseBlock of the free block fails
// 1607 RelocateOutgoing - when RelocateRecord(OUTGOING) fails even after marking block as Reclaim
// 1613 RelocateOutgoing - when ReclaimBlock fails
// 1731 CollectBlockInfo - when a block is marked for Reclaim (ie. Invalid but not fully wasted) and it fails reclaim

// * Collecting Block with invalid header

extern ERST_PRIVATE_INFO  mErrorSerialization;

// STATIC UINT8 *TestVariablePartition;
STATIC UINT8                      *TestFlashStorage;
STATIC UINT8                      *TestBuffer;
STATIC NVIDIA_NOR_FLASH_PROTOCOL  *TestNorFlashProtocol;
STATIC NVIDIA_NOR_FLASH_PROTOCOL  *FaultyNorFlashProtocol;
STATIC UINT8                      *TestErstBuffer;

#define MAX_PAYLOAD_SIZES  15
// Usually less than 256, but some up to 3KB
// Note: sizes are tuned so that index 0 2Block test fills the block exactly
STATIC CONST UINT32  PayloadSizes[MAX_PAYLOAD_SIZES] = {
  0,
  SIZE_1KB,
  SIZE_2KB,
  SIZE_4KB,
  512,
  128,
  156,
  24,
  245,
  256,
  3096,
  1,
  78,
  129,
  527,
};

// COMMON_TEST_CONTEXT fields:
//  ErstOffset
//  Offset
//  TestValue
//  ExpectedStatus

// RW Tests
STATIC COMMON_TEST_CONTEXT  RW_e0_o0_s0 = {
  0,
  0,
  0,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_o0_s1 = {
  0,
  0,
  1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_o0_sHalf = {
  0,
  0,
  BLOCK_SIZE/2,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_o0_sLarge = {
  0,
  0,
  BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_o0_sMax = {
  0,
  0,
  BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_o0_sTooBig = {
  0,
  0,
  TOTAL_NOR_FLASH_SIZE+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oHalf_s0 = {
  0,
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oHalf_s1 = {
  0,
  TOTAL_NOR_FLASH_SIZE/2,
  1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oHalf_sHalf = {
  0,
  TOTAL_NOR_FLASH_SIZE/2,
  BLOCK_SIZE/2,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oHalf_sLarge = {
  0,
  TOTAL_NOR_FLASH_SIZE/2,
  BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oHalf_sMax = {
  0,
  TOTAL_NOR_FLASH_SIZE/2,
  BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oHalf_sTooBig = {
  0,
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/2+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oLast_s0 = {
  0,
  TOTAL_NOR_FLASH_SIZE - BLOCK_SIZE,
  0,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oLast_s1 = {
  0,
  TOTAL_NOR_FLASH_SIZE - BLOCK_SIZE,
  1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oLast_sHalf = {
  0,
  TOTAL_NOR_FLASH_SIZE - BLOCK_SIZE,
  BLOCK_SIZE/2,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oLast_sLarge = {
  0,
  TOTAL_NOR_FLASH_SIZE - BLOCK_SIZE,
  BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oLast_sMax = {
  0,
  TOTAL_NOR_FLASH_SIZE - BLOCK_SIZE,
  BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_e0_oLast_sTooBig = {
  0,
  TOTAL_NOR_FLASH_SIZE - BLOCK_SIZE,
  BLOCK_SIZE+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_o0_s0 = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  0,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_o0_s1 = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_o0_sHalf = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  BLOCK_SIZE/2,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_o0_sLarge = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_o0_sMax = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_o0_sTooBig = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  TOTAL_NOR_FLASH_SIZE/2+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oHalf_s0 = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/4,
  0,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oHalf_s1 = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/4,
  1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oHalf_sHalf = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/4,
  BLOCK_SIZE/2,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oHalf_sLarge = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/4,
  BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oHalf_sMax = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/4,
  BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oHalf_sTooBig = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/4,
  TOTAL_NOR_FLASH_SIZE/4+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oLast_s0 = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/2 - BLOCK_SIZE,
  0,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oLast_s1 = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/2 - BLOCK_SIZE,
  1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oLast_sHalf = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/2 - BLOCK_SIZE,
  BLOCK_SIZE/2,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oLast_sLarge = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/2 - BLOCK_SIZE,
  BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oLast_sMax = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/2 - BLOCK_SIZE,
  BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eHalf_oLast_sTooBig = {
  TOTAL_NOR_FLASH_SIZE/2,
  TOTAL_NOR_FLASH_SIZE/2 - BLOCK_SIZE,
  BLOCK_SIZE+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_o0_s0 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  0,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_o0_s1 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_o0_sHalf = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  BLOCK_SIZE/2,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_o0_sLarge = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_o0_sMax = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_o0_sTooBig = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  TOTAL_NOR_FLASH_SIZE/2+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_oHalf_s0 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  BLOCK_SIZE,
  0,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_oHalf_s1 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  BLOCK_SIZE,
  1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_oHalf_sHalf = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  BLOCK_SIZE,
  BLOCK_SIZE/2,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_oHalf_sLarge = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  BLOCK_SIZE,
  BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_oHalf_sMax = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  BLOCK_SIZE,
  BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_oHalf_sTooBig = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  BLOCK_SIZE,
  BLOCK_SIZE+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_oEnd_s0 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  BLOCK_SIZE *2,
  0,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  RW_eLast_oEnd_s1 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  BLOCK_SIZE *2,
  1,
  EFI_INVALID_PARAMETER
};

//// STATIC COMMON_TEST_CONTEXT  RW_eLast_oTooBig_s0 = {
////  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
////  BLOCK_SIZE*3,
////  0,
////  EFI_INVALID_PARAMETER
//// };

STATIC COMMON_TEST_CONTEXT  STATUS_e0_o0_sFree = {
  0,
  0,
  ERST_RECORD_STATUS_FREE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  STATUS_e0_o1024_sDeleted = {
  0,
  1024,
  ERST_RECORD_STATUS_DELETED,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  STATUS_e0_o9000_sIncoming = {
  0,
  9000,
  ERST_RECORD_STATUS_INCOMING,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  STATUS_eHalf_oBlock_sInvalid = {
  TOTAL_NOR_FLASH_SIZE/2,
  BLOCK_SIZE,
  ERST_RECORD_STATUS_INVALID,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  STATUS_eLast_o0_sOutgoing = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  ERST_RECORD_STATUS_OUTGOING,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  STATUS_eLast_o500_sValid = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  500,
  ERST_RECORD_STATUS_VALID,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_e0_s0 = {
  0,
  0,
  0,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_e0_s1 = {
  0,
  0,
  1,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_e0_sBlock = {
  0,
  0,
  BLOCK_SIZE,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_e0_sBlock2 = {
  0,
  0,
  2*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_e0_sBlock3 = {
  0,
  0,
  3*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_e0_sUnaligned = {
  0,
  0,
  2*BLOCK_SIZE-1,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_e0_sUnaligned2 = {
  0,
  0,
  3*BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_e0_sMax = {
  0,
  0,
  TOTAL_NOR_FLASH_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_e0_sTooBig = {
  0,
  0,
  TOTAL_NOR_FLASH_SIZE+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  IP_e0_sTooBig2 = {
  0,
  0,
  TOTAL_NOR_FLASH_SIZE + BLOCK_SIZE,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  IP_eBlock_s0 = {
  BLOCK_SIZE,
  0,
  0,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eBlock_s1 = {
  BLOCK_SIZE,
  0,
  1,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eBlock_sBlock = {
  BLOCK_SIZE,
  0,
  BLOCK_SIZE,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eBlock_sBlock2 = {
  BLOCK_SIZE,
  0,
  2*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_eBlock_sBlock3 = {
  BLOCK_SIZE,
  0,
  3*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_eBlock_sUnaligned = {
  BLOCK_SIZE,
  0,
  2*BLOCK_SIZE-1,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eBlock_sUnaligned2 = {
  BLOCK_SIZE,
  0,
  3*BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_eBlock_sMax = {
  BLOCK_SIZE,
  0,
  TOTAL_NOR_FLASH_SIZE - BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_eBlock_sTooBig = {
  BLOCK_SIZE,
  0,
  TOTAL_NOR_FLASH_SIZE+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  IP_eBlock_sTooBig2 = {
  BLOCK_SIZE,
  0,
  TOTAL_NOR_FLASH_SIZE + BLOCK_SIZE,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  IP_eHalf_s0 = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  0,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eHalf_s1 = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  1,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eHalf_sBlock = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  BLOCK_SIZE,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eHalf_sBlock2 = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  2*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_eHalf_sBlock3 = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  3*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_eHalf_sUnaligned = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  2*BLOCK_SIZE-1,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eHalf_sUnaligned2 = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  3*BLOCK_SIZE-1,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_eHalf_sMax = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  TOTAL_NOR_FLASH_SIZE/2,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_eHalf_sTooBig = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  TOTAL_NOR_FLASH_SIZE/2+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  IP_eHalf_sTooBig2 = {
  TOTAL_NOR_FLASH_SIZE/2,
  0,
  TOTAL_NOR_FLASH_SIZE/2 + BLOCK_SIZE,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  IP_eLast_s0 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  0,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eLast_s1 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  1,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eLast_sBlock = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  BLOCK_SIZE,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eLast_sBlock2 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  2*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_eLast_sBlock3 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  3*BLOCK_SIZE,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  IP_eLast_sUnaligned = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  2*BLOCK_SIZE-1,
  EFI_BUFFER_TOO_SMALL
};

STATIC COMMON_TEST_CONTEXT  IP_eLast_sUnaligned2 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  3*BLOCK_SIZE-1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  IP_eLast_sMax = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  2*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  IP_eLast_sTooBig = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  2*BLOCK_SIZE+1,
  EFI_INVALID_PARAMETER
};

STATIC COMMON_TEST_CONTEXT  IP_eLast_sTooBig2 = {
  TOTAL_NOR_FLASH_SIZE - 2*BLOCK_SIZE,
  0,
  3*BLOCK_SIZE,
  EFI_INVALID_PARAMETER
};

// Erst Offset
// PayloadSize Starting Index
// Erst Size
// ExpectedStatus (unused)
STATIC COMMON_TEST_CONTEXT  E2E_e0_i0_s2Block = {
  0,
  0,
  2*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  E2E_e0_i0_s3Block = {
  0,
  0,
  3*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  E2E_e0_i0_sMax = {
  0,
  0,
  TOTAL_NOR_FLASH_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  E2E_e0_i1_s2Block = {
  0,
  1,
  2*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  E2E_e0_i1_s3Block = {
  0,
  1,
  3*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  E2E_e0_i1_sMax = {
  0,
  1,
  TOTAL_NOR_FLASH_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  E2E_e0_iHalf_s2Block = {
  0,
  MAX_PAYLOAD_SIZES/2,
  2*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  E2E_e0_iHalf_s3Block = {
  0,
  MAX_PAYLOAD_SIZES/2,
  3*BLOCK_SIZE,
  EFI_SUCCESS
};

STATIC COMMON_TEST_CONTEXT  E2E_e0_iHalf_sMax = {
  0,
  MAX_PAYLOAD_SIZES/2,
  TOTAL_NOR_FLASH_SIZE,
  EFI_SUCCESS
};

UINT32
GetStatus (
  ERST_COMM_STRUCT  *ErstComm
  )
{
  return ErstComm->Status >> ERST_STATUS_BIT_OFFSET;
}

ERST_CPER_INFO *
GetLastEntryCperInfo (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ERST_CPER_INFO  *CperInfo;
  ERST_CPER_INFO  *LastEntryCperInfo = &mErrorSerialization.CperInfo[0];

  if (mErrorSerialization.RecordCount == 0) {
    return NULL;
  }

  for (int i = 1; i < mErrorSerialization.RecordCount; i++) {
    CperInfo = &mErrorSerialization.CperInfo[i];
    //    DEBUG ((DEBUG_ERROR, "Cper[0x%x] offset=%x, last offset=%x\n", CperInfo->RecordId, CperInfo->RecordOffset, LastEntryCperInfo->RecordOffset));
    if (CperInfo->RecordOffset > LastEntryCperInfo->RecordOffset) {
      LastEntryCperInfo = CperInfo;
    }
  }

  return LastEntryCperInfo;
}

UNIT_TEST_STATUS
SanityCheckTracking (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ERST_CPER_INFO                  *CperInfo;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  CPER_ERST_PERSISTENCE_INFO      *CperPI;
  ERST_COMM_STRUCT                *ErstComm;
  COMMON_TEST_CONTEXT             *TestInfo;
  UINT32                          BlockOffset;
  UINT32                          BlockSizeLeft;
  UINT32                          RecordCount;
  UINT32                          BlockNum;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;
  ErstComm = (ERST_COMM_STRUCT *)TestErstBuffer;
  UT_ASSERT_TRUE (BLOCK_SIZE >= mErrorSerialization.BlockSize);
  SetMem (TestBuffer, mErrorSerialization.BlockSize, 0xFF);

  // Read the flash and compare it to the tracking information
  BlockOffset   = 0;
  BlockSizeLeft = mErrorSerialization.BlockSize;
  BlockNum      = 0;
  RecordCount   = 0;
  do {
    Cper   = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + BlockOffset);
    CperPI = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;

    DEBUG ((DEBUG_INFO, "Checking ID 0x%llx with status 0x%x\n", Cper->RecordID, CperPI->Status));

    switch (CperPI->Status) {
      case ERST_RECORD_STATUS_FREE:
        // Free space should fill the rest of the block
        UT_ASSERT_MEM_EQUAL (Cper, TestBuffer, BlockSizeLeft);
        BlockOffset  += BlockSizeLeft;
        BlockSizeLeft = 0;
        break;

      case ERST_RECORD_STATUS_INCOMING:
        // Incoming should have the correct record offset
        UT_ASSERT_NOT_NULL (mErrorSerialization.IncomingCperInfo);
        UT_ASSERT_EQUAL (mErrorSerialization.IncomingCperInfo->RecordOffset, BlockOffset);
        RecordCount++;
        // Incoming should be the last record in the block
        BlockOffset  += BlockSizeLeft;
        BlockSizeLeft = 0;
        break;

      case ERST_RECORD_STATUS_VALID:
        // Tracking info should point to this record
        CperInfo = ErstFindRecord (Cper->RecordID);
        if (CperInfo == NULL) {
          PrintCper (Cper, "Couldn't find CPER:");
        }

        UT_ASSERT_NOT_NULL (CperInfo);
        UT_ASSERT_EQUAL (CperInfo->RecordOffset, BlockOffset);
        UT_ASSERT_EQUAL (CperInfo->RecordLength, Cper->RecordLength);
        RecordCount++;
        // Look at next record
        BlockOffset   += Cper->RecordLength;
        BlockSizeLeft -= Cper->RecordLength;
        break;

      case ERST_RECORD_STATUS_OUTGOING:
        // Tracking info shouldn't point to this record
        CperInfo = ErstFindRecord (Cper->RecordID);
        if (CperInfo != NULL) {
          UT_ASSERT_NOT_EQUAL (CperInfo->RecordOffset, BlockOffset);
        }

        // But outgoing should
        CperInfo = mErrorSerialization.OutgoingCperInfo;
        UT_ASSERT_NOT_NULL (CperInfo);
        UT_ASSERT_EQUAL (CperInfo->RecordOffset, BlockOffset);
        UT_ASSERT_EQUAL (CperInfo->RecordLength, Cper->RecordLength);
        RecordCount++;
        // Look at next record
        BlockOffset   += Cper->RecordLength;
        BlockSizeLeft -= Cper->RecordLength;
        break;

      case ERST_RECORD_STATUS_DELETED:
        // Tracking info shouldn't point to this record
        CperInfo = ErstFindRecord (Cper->RecordID);
        if (CperInfo != NULL) {
          UT_ASSERT_NOT_EQUAL (CperInfo->RecordOffset, BlockOffset);
        }

        // Look at next record
        BlockOffset   += Cper->RecordLength;
        BlockSizeLeft -= Cper->RecordLength;
        break;

      case ERST_RECORD_STATUS_INVALID:
        // Tracking info shouldn't point to this record
        CperInfo = ErstFindRecord (Cper->RecordID);
        if (CperInfo != NULL) {
          UT_ASSERT_NOT_EQUAL (CperInfo->RecordOffset, BlockOffset);
        }

        // Invalid should be the last record in the block
        BlockOffset  += BlockSizeLeft;
        BlockSizeLeft = 0;
        break;

      default:
        UT_ASSERT_EQUAL (0, CperPI->Status);
    }

    // Go to next block if not enough space for another header
    if (BlockSizeLeft < sizeof (EFI_COMMON_ERROR_RECORD_HEADER)) {
      BlockOffset += BlockSizeLeft;
      BlockNum++;
      BlockSizeLeft = mErrorSerialization.BlockSize;
    }
  } while (BlockNum < mErrorSerialization.NumBlocks);

  UT_ASSERT_EQUAL (RecordCount, mErrorSerialization.RecordCount);
  UT_ASSERT_EQUAL (RecordCount, ErstComm->RecordCount);

  return UNIT_TEST_PASSED;
}

void
PrintCper (
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper,
  char                            *Header
  )
{
  DEBUG ((DEBUG_INFO, "%a\n", Header));

  DEBUG ((
    DEBUG_INFO,
    "ID: 0x%llx Len: 0x%llx\nSigStart:0x%x Rev:0x%x SigEnd:0x%x\n",
    Cper->RecordID,
    Cper->RecordLength,
    Cper->SignatureStart,
    Cper->Revision,
    Cper->SignatureEnd
    ));

  for (int i = 0; i < Cper->RecordLength; i++) {
    if ((i % 16) == 0) {
      DEBUG ((DEBUG_INFO, "\n0x%08x ", i));
    }

    DEBUG ((DEBUG_INFO, "%02x ", ((UINT8 *)Cper)[i]));
  }

  DEBUG ((DEBUG_INFO, "\n\n"));
}

/**
  Sets up an empty (all 0xFF) flash for E2E tests

  @param Context                      Used for Erst offset

  @retval UNIT_TEST_PASSED            Setup succeeded.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2EEmptyFlashSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS           Status;
  UNIT_TEST_STATUS     UTStatus;
  COMMON_TEST_CONTEXT  *TestInfo;
  UINT32               ErstSize;
  ERST_COMM_STRUCT     *TestErstComm;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;
  ErstSize = TestInfo->TestValue;

  // Empty Spinor
  SetMem (TestFlashStorage, TOTAL_NOR_FLASH_SIZE, 0xFF);
  SetMem (TestErstBuffer, ERST_BUFFER_SIZE, 0xFF);

  MockNorErstOffset = TestInfo->ErstOffset;
  MockNorErstSize   = ErstSize;
  UTStatus          = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);

  ErstMemoryInit ();
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  UT_ASSERT_NOT_NULL (mErrorSerialization.BlockInfo);
  UT_ASSERT_NOT_NULL (mErrorSerialization.CperInfo);
  UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);

  UT_ASSERT_EQUAL (mErrorSerialization.BufferInfo.ErstBase, (PHYSICAL_ADDRESS)TestErstBuffer);
  UT_ASSERT_EQUAL (mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase, (UINT64)(TestErstBuffer+sizeof (ERST_COMM_STRUCT)));
  UT_ASSERT_EQUAL (mErrorSerialization.BufferInfo.ErrorLogInfo.Length, ERROR_LOG_INFO_BUFFER_SIZE);

  TestErstComm = (ERST_COMM_STRUCT *)TestErstBuffer;
  UT_ASSERT_STATUS_EQUAL (GetStatus (TestErstComm), EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (TestErstComm->Timings, ((UINT64)ERST_DEFAULT_TIMING << ERST_MAX_TIMING_SHIFT) | ERST_DEFAULT_TIMING);
  // Note: ReInit restores these fields to their previous value, so don't check them
  //  UT_ASSERT_EQUAL (TestErstComm->Operation, ERST_OPERATION_INVALID);
  //  UT_ASSERT_EQUAL (TestErstComm->RecordOffset, 0);
  //  UT_ASSERT_EQUAL (TestErstComm->RecordID, ERST_INVALID_RECORD_ID);
  UT_ASSERT_MEM_EQUAL (&TestErstComm->ErrorLogAddressRange, &mErrorSerialization.BufferInfo.ErrorLogInfo, sizeof (ERST_ERROR_LOG_INFO));

  //  DEBUG ((DEBUG_ERROR, "Done with E2EEmptyFlashSetup\n"));
  return UTStatus;
}

/**
  Write a single record end to end

  @param Context                      Used for Erst offset
  @param RecordId                     Id to write in the record
  @param RecordOffset                 Offset within the buffer to use
  @param PayloadSize                  Size of the payload to create
  @param PayloadData                  Fill value for uncheck header field and payload
  @param ExpectedStatus               Expectation on write success/failure

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2EWrite (
  IN UNIT_TEST_CONTEXT  Context,
  IN UINT64             RecordId,
  IN UINT64             RecordOffset,
  IN UINT32             PayloadSize,
  IN UINT8              PayloadData,
  IN UINT32             ExpectedStatus
  )
{
  COMMON_TEST_CONTEXT             *TestInfo;
  ERST_COMM_STRUCT                *ErstComm;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  EFI_COMMON_ERROR_RECORD_HEADER  *StoredCper;
  CPER_ERST_PERSISTENCE_INFO      *CperPI;
  CPER_ERST_PERSISTENCE_INFO      *StoredCperPI;
  ERST_CPER_INFO                  *CperInfo;
  ERST_BLOCK_INFO                 *BlockInfo;
  UINT8                           *Payload;
  UINT8                           *StoredPayload;
  UINT32                          RecordCount;
  UINTN                           IsANewRecord;

  //  DEBUG ((DEBUG_INFO, "Inside E2EWrite: Id 0x%x Offset 0x%x Payload size 0x%x Payload %x Expected %d\n",
  //          RecordId, RecordOffset, PayloadSize, (UINT32)PayloadData, ExpectedStatus));

  TestInfo = (COMMON_TEST_CONTEXT *)Context;

  ErstComm = (ERST_COMM_STRUCT *)TestErstBuffer;

  RecordCount = mErrorSerialization.RecordCount;
  UT_ASSERT_EQUAL (ErstComm->RecordCount, RecordCount);

  if (ExpectedStatus == EFI_ACPI_6_4_ERST_STATUS_SUCCESS) {
    if (ErstFindRecord (RecordId)) {
      IsANewRecord = 0;
    } else {
      IsANewRecord = 1;
    }
  }

  //  DEBUG ((DEBUG_ERROR, "Before Step 1\n"));
  // 1. Initializes the error record's serialization info. OSPM must fill in the Signature.
  if (RecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER) <= ErstComm->ErrorLogAddressRange.Length) {
    Cper = (EFI_COMMON_ERROR_RECORD_HEADER *)(ErstComm->ErrorLogAddressRange.PhysicalBase + RecordOffset);
    SetMem (Cper, sizeof (EFI_COMMON_ERROR_RECORD_HEADER), PayloadData);

    Cper->RecordID       = RecordId;
    Cper->RecordLength   = PayloadSize + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
    Cper->SignatureStart = EFI_ERROR_RECORD_SIGNATURE_START;
    Cper->Revision       = EFI_ERROR_RECORD_REVISION;
    Cper->SignatureEnd   = EFI_ERROR_RECORD_SIGNATURE_END;
  }

  //  DEBUG ((DEBUG_ERROR, "Before Step 2\n"));
  // 2. Writes the error record to be persisted into the Error Log Address Range.
  if (RecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize <= ErstComm->ErrorLogAddressRange.Length) {
    Payload = (UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
    SetMem (Payload, PayloadSize, PayloadData);
  }

  //  DEBUG ((DEBUG_ERROR, "Before Step 3\n"));
  // 3. Executes the BEGIN_WRITE_OPERATION action to notify the platform that a record write operation is beginning.
  ErstComm->Operation = ERST_OPERATION_WRITE;
  //  DEBUG ((DEBUG_INFO, "Testcase: ErstComm is at 0x%llx, operation is 0x%x\n", ErstComm, ErstComm->Operation));

  //  DEBUG ((DEBUG_ERROR, "Before Step 4\n"));
  // 4-5. Executes the SET_RECORD_OFFSET action to inform the platform where in the Error Log Address Range the error record resides.
  ErstComm->RecordOffset = RecordOffset;

  // 6. Executes the EXECUTE_OPERATION action to instruct the platform to begin the write operation.
  // Note: IoStubLib routes all reads/writes to the same UINT32, so write a 0 to the SET address because
  // the driver will write a 1 to the CLEAR address in an attempt to clear it. Then check for this 1 to
  // indicate that the busy was "cleared".
  //  MmioWrite32 (TH500_SW_IO6_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_SET_0_OFFSET, 0);
  //  DEBUG ((DEBUG_ERROR, "Before MmioWrite\n"));
  MmioWrite32 (0, 0);
  //  DEBUG ((DEBUG_ERROR, "Before ES Event Handler\n"));
  ErrorSerializationEventHandler (NULL, NULL, NULL, NULL);
  //  DEBUG ((DEBUG_ERROR, "After ES Event Handler\n"));

  // 7. Busy waits by continually executing CHECK_BUSY_STATUS action until FALSE is returned.
  //  UT_ASSERT_EQUAL(MmioRead32 (TH500_SW_IO6_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_0_OFFSET), 1);
  UT_ASSERT_EQUAL (MmioRead32 (0), 1);

  // 8-9. Executes a GET_COMMAND_STATUS action to determine the status of the write operation. If an error is indicated, the OSPM
  //      may retry the operation.
  UT_ASSERT_STATUS_EQUAL (GetStatus (ErstComm), ExpectedStatus);

  // 10. Executes an END_OPERATION action to notify the platform tha the record write operation is complete.
  ErstComm->Operation = ERST_OPERATION_INVALID;

  // Check the results
  if (ExpectedStatus == EFI_ACPI_6_4_ERST_STATUS_SUCCESS) {
    // Record Count updated
    UT_ASSERT_EQUAL (ErstComm->RecordCount, RecordCount+IsANewRecord);

    // Payload written to Flash correctly
    CperInfo = ErstFindRecord (RecordId);
    UT_ASSERT_NOT_NULL (CperInfo);
    StoredCper    = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
    StoredPayload = (UINT8 *)StoredCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
    UT_ASSERT_MEM_EQUAL (Payload, StoredPayload, PayloadSize);

    // Header written to Flash correctly
    CperPI       = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
    StoredCperPI = (CPER_ERST_PERSISTENCE_INFO *)&StoredCper->PersistenceInfo;
    UT_ASSERT_EQUAL (StoredCperPI->Major, ERST_RECORD_VERSION_MAJOR);
    UT_ASSERT_EQUAL (StoredCperPI->Minor, ERST_RECORD_VERSION_MINOR);
    UT_ASSERT_EQUAL (StoredCperPI->Signature, ERST_RECORD_SIGNATURE);
    UT_ASSERT_EQUAL (StoredCperPI->Status, ERST_RECORD_STATUS_VALID);
    CopyMem (CperPI, StoredCperPI, sizeof (CPER_ERST_PERSISTENCE_INFO)); // Copy so that we can just compare the whole headers
    UT_ASSERT_MEM_EQUAL (Cper, StoredCper, sizeof (EFI_COMMON_ERROR_RECORD_HEADER));

    // Check that record tracking data makes sense
    UT_ASSERT_EQUAL (mErrorSerialization.RecordCount, RecordCount+IsANewRecord);
    BlockInfo = ErstGetBlockOfRecord (CperInfo);
    //    DEBUG ((DEBUG_INFO, "Block Base(0x%x): ValidEntries: 0x%x, UsedSize: 0x%x, WastedSize: 0x%x\n",
    //           BlockInfo->Base, BlockInfo->ValidEntries, BlockInfo->UsedSize, BlockInfo->WastedSize));
    UT_ASSERT_NOT_NULL (BlockInfo);
    UT_ASSERT_TRUE (BlockInfo->ValidEntries > 0);
    UT_ASSERT_TRUE (BlockInfo->UsedSize >= (sizeof (EFI_COMMON_ERROR_RECORD_HEADER)*BlockInfo->ValidEntries + PayloadSize));
    UT_ASSERT_TRUE (BlockInfo->WastedSize <= mErrorSerialization.BlockSize);
    UT_ASSERT_TRUE (BlockInfo->UsedSize   <= mErrorSerialization.BlockSize);
    UT_ASSERT_TRUE (BlockInfo->WastedSize <= BlockInfo->UsedSize);

    UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);
  } else {
    UT_ASSERT_EQUAL (ErstComm->RecordCount, RecordCount);
  }

  //  DEBUG ((DEBUG_ERROR, "Exiting E2EWrite\n"));

  return UNIT_TEST_PASSED;
}

/**
  Dummy Write a single record end to end

  @param Context                      Used for Erst offset
  @param RecordId                     Id to write in the record
  @param RecordOffset                 Offset within the buffer to use
  @param PayloadSize                  Size of the payload to create
  @param PayloadData                  Fill value for uncheck header field and payload
  @param ExpectedStatus               Expectation on write success/failure

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2EDummyWrite (
  IN UNIT_TEST_CONTEXT  Context,
  IN UINT64             RecordId,
  IN UINT64             RecordOffset,
  IN UINT32             PayloadSize,
  IN UINT8              PayloadData,
  IN UINT32             ExpectedStatus
  )
{
  ERST_COMM_STRUCT                *ErstComm;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  ERST_CPER_INFO                  *CperInfo;
  UINT8                           *Payload;
  UINT32                          RecordCount;

  //  DEBUG ((DEBUG_INFO, "Inside E2EDummyWrite: Id 0x%x Offset 0x%x Payload size 0x%x Payload %x Expected %d\n",
  //          RecordId, RecordOffset, PayloadSize, (UINT32)PayloadData, ExpectedStatus));

  ErstComm = (ERST_COMM_STRUCT *)TestErstBuffer;

  RecordCount = mErrorSerialization.RecordCount;
  UT_ASSERT_EQUAL (ErstComm->RecordCount, RecordCount);

  //  DEBUG ((DEBUG_ERROR, "Before Step 1\n"));
  // 1. Initializes the error record's serialization info. OSPM must fill in the Signature.
  Cper = (EFI_COMMON_ERROR_RECORD_HEADER *)(ErstComm->ErrorLogAddressRange.PhysicalBase + RecordOffset);
  SetMem (Cper, sizeof (EFI_COMMON_ERROR_RECORD_HEADER), PayloadData);

  Cper->RecordID       = RecordId;
  Cper->RecordLength   = PayloadSize + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  Cper->SignatureStart = EFI_ERROR_RECORD_SIGNATURE_START;
  Cper->Revision       = EFI_ERROR_RECORD_REVISION;
  Cper->SignatureEnd   = EFI_ERROR_RECORD_SIGNATURE_END;

  //  DEBUG ((DEBUG_ERROR, "Before Step 2\n"));
  // 2. Writes the error record to be persisted into the Error Log Address Range.
  Payload = (UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  SetMem (Payload, PayloadSize, PayloadData);

  //  DEBUG ((DEBUG_ERROR, "Before Step 3\n"));
  // 3. Executes the BEGIN_WRITE_OPERATION action to notify the platform that a record write operation is beginning.
  ErstComm->Operation = ERST_OPERATION_DUMMY_WRITE;
  //  DEBUG ((DEBUG_INFO, "Testcase: ErstComm is at 0x%llx, operation is 0x%x\n", ErstComm, ErstComm->Operation));

  //  DEBUG ((DEBUG_ERROR, "Before Step 4\n"));
  // 4-5. Executes the SET_RECORD_OFFSET action to inform the platform where in the Error Log Address Range the error record resides.
  ErstComm->RecordOffset = RecordOffset;

  // 6. Executes the EXECUTE_OPERATION action to instruct the platform to begin the write operation.
  // Note: IoStubLib routes all reads/writes to the same UINT32, so write a 0 to the SET address because
  // the driver will write a 1 to the CLEAR address in an attempt to clear it. Then check for this 1 to
  // indicate that the busy was "cleared".
  //  MmioWrite32 (TH500_SW_IO6_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_SET_0_OFFSET, 0);
  //  DEBUG ((DEBUG_ERROR, "Before MmioWrite\n"));
  MmioWrite32 (0, 0);
  //  DEBUG ((DEBUG_ERROR, "Before ES Event Handler\n"));
  ErrorSerializationEventHandler (NULL, NULL, NULL, NULL);
  //  DEBUG ((DEBUG_ERROR, "After ES Event Handler\n"));

  // 7. Busy waits by continually executing CHECK_BUSY_STATUS action until FALSE is returned.
  //  UT_ASSERT_EQUAL(MmioRead32 (TH500_SW_IO6_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_0_OFFSET), 1);
  UT_ASSERT_EQUAL (MmioRead32 (0), 1);

  // 8-9. Executes a GET_COMMAND_STATUS action to determine the status of the write operation. If an error is indicated, the OSPM
  //      may retry the operation.
  UT_ASSERT_STATUS_EQUAL (GetStatus (ErstComm), ExpectedStatus);

  // 10. Executes an END_OPERATION action to notify the platform tha the record write operation is complete.
  ErstComm->Operation = ERST_OPERATION_INVALID;

  // Check the results
  // Record Count NOT updated
  UT_ASSERT_EQUAL (ErstComm->RecordCount, RecordCount);

  // Payload NOT written to Flash
  CperInfo = ErstFindRecord (RecordId);
  UT_ASSERT_TRUE (CperInfo == NULL);

  // Check that record tracking data makes sense
  UT_ASSERT_EQUAL (mErrorSerialization.RecordCount, RecordCount);
  //  DEBUG ((DEBUG_ERROR, "Exiting E2EDummyWrite\n"));

  return UNIT_TEST_PASSED;
}

/**
  End2End Fill test

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2ESimpleFillTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT             *TestInfo;
  UINT32                          ErstSize;
  UINT32                          PayloadSize;
  UINT8                           PayloadData;
  UINT64                          RecordOffset;
  UINT64                          RecordId;
  UINT32                          OffsetMax;
  UINT32                          SizeIndex;
  UINT32                          RemainingSizeInBlock;
  UINT32                          RemainingBlocks;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  ERST_CPER_INFO                  *CperInfo;
  EFI_STATUS                      Status;

  //  DEBUG ((DEBUG_INFO, "Inside E2ESimpleFillTest\n"));

  TestInfo  = (COMMON_TEST_CONTEXT *)Context;
  ErstSize  = TestInfo->TestValue;
  SizeIndex = TestInfo->Offset;

  // Should fail to write Id 0
  E2EWrite (
    Context,
    ERST_FIRST_RECORD_ID,
    0,
    0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_FAILED
    );
  UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);

  // Should fail to write invalid ID
  E2EWrite (
    Context,
    ERST_INVALID_RECORD_ID,
    0,
    0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_FAILED
    );
  UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);

  // Should be able to do a dummy write, and not affect real writes later
  E2EDummyWrite (Context, 0x1, 0x0, 0x0, 0xaa, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);

  // Should fail a write that has an offset too large for header
  E2EWrite (
    Context,
    0x1,
    mErrorSerialization.BufferInfo.ErrorLogInfo.Length - sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + 1,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_FAILED
    );
  UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);

  RemainingBlocks      = ErstSize/mErrorSerialization.BlockSize;
  RemainingSizeInBlock = mErrorSerialization.BlockSize;
  RecordId             = SizeIndex + ErstSize; // Pseudo-random value

  while (RemainingBlocks > 1) {
    while (RemainingSizeInBlock >= (PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES] + sizeof (EFI_COMMON_ERROR_RECORD_HEADER))) {
      //      DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
      PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
      OffsetMax    = ERROR_LOG_INFO_BUFFER_SIZE - PayloadSize - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
      RecordOffset = OffsetMax; // JDS TODO - figure out a better way to do this
      PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
      E2EWrite (
        Context,
        RecordId,
        RecordOffset,
        PayloadSize,
        PayloadData,
        EFI_ACPI_6_4_ERST_STATUS_SUCCESS
        );
      RecordId++;
      SizeIndex++;
      RemainingSizeInBlock -= (sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize);
    }

    RemainingBlocks--;
    RemainingSizeInBlock = mErrorSerialization.BlockSize;
  }

  //  DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
  PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
  RecordOffset = 0;
  PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
  E2EWrite (
    Context,
    RecordId,
    RecordOffset,
    PayloadSize,
    PayloadData,
    EFI_ACPI_6_4_ERST_STATUS_NOT_ENOUGH_SPACE
    );
  UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);

  // If there's no space for a real write, a Dummy write should fail too
  E2EDummyWrite (
    Context,
    RecordId,
    RecordOffset,
    PayloadSize,
    PayloadData,
    EFI_ACPI_6_4_ERST_STATUS_NOT_ENOUGH_SPACE
    );
  UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);

  // Sanity check the headers in the records
  for (int i = 0; i < mErrorSerialization.RecordCount; i++) {
    CperInfo = &mErrorSerialization.CperInfo[i];
    Cper     = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
    Status   = ErstValidateRecord (Cper, CperInfo->RecordId, CperInfo->RecordLength);
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  }

  return UNIT_TEST_PASSED;
}

/**
  Read a single record end to end

  @param Context                      Used for Erst offset
  @param RecordId                     Id to read
  @param RecordOffset                 Offset within the buffer to use
  @param PayloadSize                  Expected size of the payload
  @param PayloadData                  Expected data in the payload
  @param ExpectedStatus               Expectation on read success/failure

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2ERead (
  IN UNIT_TEST_CONTEXT  Context,
  IN UINT64             RecordId,
  IN UINT64             RecordOffset,
  IN UINT32             PayloadSize,
  IN UINT8              PayloadData,
  IN UINT32             ExpectedStatus
  )
{
  COMMON_TEST_CONTEXT             *TestInfo;
  ERST_COMM_STRUCT                *ErstComm;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  EFI_COMMON_ERROR_RECORD_HEADER  *TestCper;
  EFI_COMMON_ERROR_RECORD_HEADER  *StoredCper;
  CPER_ERST_PERSISTENCE_INFO      *CperPI;
  ERST_CPER_INFO                  *CperInfo;
  UINT8                           *Payload;
  UINT8                           *StoredPayload;
  UINT32                          RecordCount;

  //  DEBUG ((DEBUG_INFO, "Inside E2ERead: Id 0x%x Offset 0x%x Payload size 0x%x Payload %x Expected %d\n",
  //          RecordId, RecordOffset, PayloadSize, (UINT32)PayloadData, ExpectedStatus));

  TestInfo = (COMMON_TEST_CONTEXT *)Context;

  ErstComm = (ERST_COMM_STRUCT *)TestErstBuffer;

  RecordCount = mErrorSerialization.RecordCount;
  UT_ASSERT_EQUAL (ErstComm->RecordCount, RecordCount);

  Cper    = (EFI_COMMON_ERROR_RECORD_HEADER *)(ErstComm->ErrorLogAddressRange.PhysicalBase + RecordOffset);
  Payload = (UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  if (RecordOffset < ErstComm->ErrorLogAddressRange.Length) {
    if (RecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER) < ErstComm->ErrorLogAddressRange.Length) {
      //      DEBUG ((DEBUG_ERROR, "Setting Cper (0x%d) to 0x%x\n", Cper, ~PayloadData));
      SetMem (Cper, sizeof (EFI_COMMON_ERROR_RECORD_HEADER), ~PayloadData);
      if (RecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize < ErstComm->ErrorLogAddressRange.Length) {
        SetMem (Payload, PayloadSize, ~PayloadData);
      }
    }
  }

  //  DEBUG ((DEBUG_ERROR, "Before Step 1\n"));
  // 1. Executes the BEGIN_ READ_OPERATION action to notify the platform that a record read operation is beginning.
  ErstComm->Operation = ERST_OPERATION_READ;

  //  DEBUG ((DEBUG_ERROR, "Before Step 2\n"));
  // 2. Executes the SET_ RECORD_OFFSET action to inform the platform at what offset in the Error Log Address Range the error record is to be transferred.
  ErstComm->RecordOffset = RecordOffset;

  //  DEBUG ((DEBUG_ERROR, "Before Step 3\n"));
  // 3. Executes the SET_RECORD_IDENTIFER action to inform the platform which error record is to be read from its persistent store.
  ErstComm->RecordID = RecordId;

  //  DEBUG ((DEBUG_ERROR, "Before Step 4\n"));
  // 4. Executes the EXECUTE_OPERATION action to instruct the platform to begin the read operation.
  // Note: IoStubLib routes all reads/writes to the same UINT32, so write a 0 to the SET address because
  // the driver will write a 1 to the CLEAR address in an attempt to clear it. Then check for this 1 to
  // indicate that the busy was "cleared".
  //  MmioWrite32 (TH500_SW_IO6_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_SET_0_OFFSET, 0);
  //  DEBUG ((DEBUG_ERROR, "Before MmioWrite\n"));
  MmioWrite32 (0, 0);
  //  DEBUG ((DEBUG_ERROR, "Before ES Event Handler\n"));
  ErrorSerializationEventHandler (NULL, NULL, NULL, NULL);
  //  DEBUG ((DEBUG_ERROR, "After ES Event Handler\n"));

  // 5. Busy waits by continually executing CHECK_BUSY_STATUS action until FALSE is returned.
  //  UT_ASSERT_EQUAL(MmioRead32 (TH500_SW_IO6_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_0_OFFSET), 1);
  UT_ASSERT_EQUAL (MmioRead32 (0), 1);

  // 6. Executes a GET_COMMAND_STATUS action to determine the status of the read operation.
  UT_ASSERT_STATUS_EQUAL (GetStatus (ErstComm), ExpectedStatus);

  // 7. Execute an END_OPERATION to notify the platform that the record read operation is complete.
  ErstComm->Operation = ERST_OPERATION_INVALID;

  // Check the results
  if (ExpectedStatus == EFI_ACPI_6_4_ERST_STATUS_SUCCESS) {
    // Check fields that write should have updated
    UT_ASSERT_TRUE ((RecordId == ERST_FIRST_RECORD_ID) || (Cper->RecordID == RecordId));
    UT_ASSERT_EQUAL (Cper->RecordLength, PayloadSize + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
    UT_ASSERT_EQUAL (Cper->SignatureStart, EFI_ERROR_RECORD_SIGNATURE_START);
    UT_ASSERT_EQUAL (Cper->Revision, EFI_ERROR_RECORD_REVISION);
    UT_ASSERT_EQUAL (Cper->SignatureEnd, EFI_ERROR_RECORD_SIGNATURE_END);

    // Payload read from Flash correctly?
    SetMem (TestBuffer, BLOCK_SIZE, PayloadData);
    CperInfo = ErstFindRecord (Cper->RecordID);
    UT_ASSERT_NOT_NULL (CperInfo);
    StoredCper    = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
    StoredPayload = (UINT8 *)StoredCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
    UT_ASSERT_MEM_EQUAL (Payload, StoredPayload, PayloadSize);
    UT_ASSERT_MEM_EQUAL (Payload, TestBuffer, PayloadSize);

    // Header read from Flash correctly
    CperPI = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
    UT_ASSERT_EQUAL (CperPI->Major, ERST_RECORD_VERSION_MAJOR);
    UT_ASSERT_EQUAL (CperPI->Minor, ERST_RECORD_VERSION_MINOR);
    UT_ASSERT_EQUAL (CperPI->Signature, ERST_RECORD_SIGNATURE);
    UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_VALID);
    UT_ASSERT_MEM_EQUAL (Cper, StoredCper, sizeof (EFI_COMMON_ERROR_RECORD_HEADER));

    // Check all the other CPER fields the lazy way, by copying the fields already checked into TestBuffer
    // and then comparing the whole buffers
    TestCper = (EFI_COMMON_ERROR_RECORD_HEADER *)TestBuffer;
    CopyMem (&TestCper->PersistenceInfo, CperPI, sizeof (CPER_ERST_PERSISTENCE_INFO));
    TestCper->RecordID       = Cper->RecordID;
    TestCper->RecordLength   = Cper->RecordLength;
    TestCper->SignatureStart = Cper->SignatureStart;
    TestCper->Revision       = Cper->Revision;
    TestCper->SignatureEnd   = Cper->SignatureEnd;
    //    PrintCper(Cper, "Read Result:");
    //    PrintCper(TestCper, "Expected Result:");
    UT_ASSERT_MEM_EQUAL (Cper, TestCper, sizeof (EFI_COMMON_ERROR_RECORD_HEADER));

    // Make sure RecordID points to a new record if possible
    if (ErstComm->RecordCount == 1) {
      UT_ASSERT_EQUAL (ErstComm->RecordID, RecordId);
    } else {
      UT_ASSERT_NOT_EQUAL (ErstComm->RecordID, RecordId);
    }
  } else {
    // Make sure we didn't read anything
    if (RecordOffset < ErstComm->ErrorLogAddressRange.Length) {
      //      DEBUG ((DEBUG_ERROR, "Setting TestBuffer (0x%d) to 0x%x\n", TestBuffer, ~PayloadData));
      SetMem (TestBuffer, BLOCK_SIZE, ~PayloadData);
      if (RecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER) < ErstComm->ErrorLogAddressRange.Length) {
        UT_ASSERT_MEM_EQUAL (Cper, TestBuffer, sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
        if (RecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize < ErstComm->ErrorLogAddressRange.Length) {
          UT_ASSERT_MEM_EQUAL (Payload, TestBuffer, PayloadSize);
        }
      }
    }
  }

  // If it's not found then it's not empty, so ErstComm should indicate a valid record number
  if (ExpectedStatus == EFI_ACPI_6_4_ERST_STATUS_RECORD_NOT_FOUND) {
    UT_ASSERT_TRUE (
      (ErstComm->RecordID != RecordId) &&
      (ErstComm->RecordID != ERST_INVALID_RECORD_ID)
      );
    CperInfo = ErstFindRecord (ErstComm->RecordID);
    UT_ASSERT_NOT_NULL (CperInfo);
  }

  //  DEBUG ((DEBUG_ERROR, "Exiting E2ERead\n"));

  return UNIT_TEST_PASSED;
}

/**
  Clear a single record end to end

  @param Context                      Used for Erst offset
  @param RecordId                     Id to read
  @param RecordOffset                 Offset within the buffer to use
  @param PayloadSize                  Expected size of the payload
  @param PayloadData                  Expected data in the payload
  @param ExpectedStatus               Expectation on read success/failure

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2EClear (
  IN UNIT_TEST_CONTEXT  Context,
  IN UINT64             RecordId,
  IN UINT64             RecordOffset,
  IN UINT32             PayloadSize,
  IN UINT8              PayloadData,
  IN UINT32             ExpectedStatus
  )
{
  COMMON_TEST_CONTEXT             *TestInfo;
  ERST_COMM_STRUCT                *ErstComm;
  EFI_COMMON_ERROR_RECORD_HEADER  *StoredCper;
  CPER_ERST_PERSISTENCE_INFO      *StoredCperPI;
  ERST_CPER_INFO                  *CperInfo;
  ERST_CPER_INFO                  OriginalCperInfo;
  ERST_BLOCK_INFO                 *BlockInfo;
  ERST_BLOCK_INFO                 OriginalBlockInfo;
  UINT32                          RecordCount;

  //  DEBUG ((DEBUG_INFO, "Inside E2EClear: Id 0x%x Offset 0x%x Payload size 0x%x Payload %x Expected %d\n",
  //          RecordId, RecordOffset, PayloadSize, (UINT32)PayloadData, ExpectedStatus));

  TestInfo = (COMMON_TEST_CONTEXT *)Context;

  ErstComm = (ERST_COMM_STRUCT *)TestErstBuffer;

  RecordCount = mErrorSerialization.RecordCount;
  UT_ASSERT_EQUAL (ErstComm->RecordCount, RecordCount);

  if (ExpectedStatus == EFI_ACPI_6_4_ERST_STATUS_SUCCESS) {
    CperInfo = ErstFindRecord (RecordId);
    UT_ASSERT_NOT_NULL (CperInfo);
    BlockInfo = ErstGetBlockOfRecord (CperInfo);
    UT_ASSERT_NOT_NULL (BlockInfo);
    CopyMem (&OriginalCperInfo, CperInfo, sizeof (ERST_CPER_INFO));
    CopyMem (&OriginalBlockInfo, BlockInfo, sizeof (ERST_BLOCK_INFO));
  }

  //  DEBUG ((DEBUG_ERROR, "Before Step 1\n"));
  // 1. Executes a BEGIN_ CLEAR_OPERATION action to notify the platform that a record clear operation is beginning.
  ErstComm->Operation = ERST_OPERATION_CLEAR;

  //  DEBUG ((DEBUG_ERROR, "Before Step 2\n"));
  // Executes a SET_RECORD_IDENTIFER action to inform the platform which error record is to be cleared. This value must not be set to 0x0 (unspecified).
  ErstComm->RecordID = RecordId;

  //  DEBUG ((DEBUG_ERROR, "Before Step 3\n"));
  // 3. Executes an EXECUTE_OPERATION action to instruct the platform to begin the clear operation.
  // Note: IoStubLib routes all reads/writes to the same UINT32, so write a 0 to the SET address because
  // the driver will write a 1 to the CLEAR address in an attempt to clear it. Then check for this 1 to
  // indicate that the busy was "cleared".
  //  MmioWrite32 (TH500_SW_IO6_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_SET_0_OFFSET, 0);
  //  DEBUG ((DEBUG_ERROR, "Before MmioWrite\n"));
  MmioWrite32 (0, 0);
  //  DEBUG ((DEBUG_ERROR, "Before ES Event Handler\n"));
  ErrorSerializationEventHandler (NULL, NULL, NULL, NULL);
  //  DEBUG ((DEBUG_ERROR, "After ES Event Handler\n"));

  //  DEBUG ((DEBUG_ERROR, "Before Step 4\n"));
  // 4. Busy waits by continually executing CHECK_BUSY_STATUS action until FALSE is returned.
  //  UT_ASSERT_EQUAL(MmioRead32 (TH500_SW_IO6_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_0_OFFSET), 1);
  if (mErrorSerialization.ErstLicSwIoBase != 0) {
    UT_ASSERT_EQUAL (MmioRead32 (0), 1);
  }

  // 5. Executes a GET_COMMAND_STATUS action to determine the status of the clear operation.
  //  DEBUG ((DEBUG_ERROR, "AfterClear got status %d, expected %d\n", GetStatus(ErstComm), ExpectedStatus));
  UT_ASSERT_STATUS_EQUAL (GetStatus (ErstComm), ExpectedStatus);

  // 6. Execute an END_OPERATION to notify the platform that the record read operation is complete.
  ErstComm->Operation = ERST_OPERATION_INVALID;

  // Check the results
  if (ExpectedStatus == EFI_ACPI_6_4_ERST_STATUS_SUCCESS) {
    StoredCper   = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + OriginalCperInfo.RecordOffset);
    StoredCperPI = (CPER_ERST_PERSISTENCE_INFO *)&StoredCper->PersistenceInfo;

    // Check fields that clear should have updated
    UT_ASSERT_EQUAL (StoredCperPI->Status, ERST_RECORD_STATUS_DELETED);

    // Check that record tracking data was updated correctly
    UT_ASSERT_EQUAL (mErrorSerialization.RecordCount, RecordCount-1);
    UT_ASSERT_TRUE (ErstFindRecord (RecordId) == NULL);
    //    DEBUG ((DEBUG_INFO, "Block Base(0x%x): ValidEntries: 0x%x, UsedSize: 0x%x, WastedSize: 0x%x\n",
    //           BlockInfo->Base, BlockInfo->ValidEntries, BlockInfo->UsedSize, BlockInfo->WastedSize));
    UT_ASSERT_EQUAL (BlockInfo->Base, OriginalBlockInfo.Base);
    UT_ASSERT_EQUAL (BlockInfo->UsedSize, OriginalBlockInfo.UsedSize);
    UT_ASSERT_EQUAL (BlockInfo->WastedSize, OriginalBlockInfo.WastedSize + StoredCper->RecordLength);
    UT_ASSERT_EQUAL (BlockInfo->ValidEntries, OriginalBlockInfo.ValidEntries - 1);

    // Make sure ErstComm was updated correctly
    UT_ASSERT_EQUAL (ErstComm->RecordCount, RecordCount-1);

    // Make sure RecordId was updated
    UT_ASSERT_TRUE ((ErstComm->RecordID != RecordId) || (ErstComm->RecordID == ERST_INVALID_RECORD_ID));
  } else {
    // Make sure we didn't update anything, but did update RecordId to point to valid if possible
    UT_ASSERT_EQUAL (ErstComm->RecordCount, RecordCount);
    UT_ASSERT_TRUE (
      (ErstComm->RecordID != RecordId) || (ErstComm->RecordID == ERST_INVALID_RECORD_ID) ||
      (ErstComm->RecordID == mErrorSerialization.CperInfo[0].RecordId)
      );
  }

  UT_ASSERT_TRUE (
    ((ErstComm->RecordCount == 0) && (ErstComm->RecordID == ERST_INVALID_RECORD_ID)) ||
    ((ErstComm->RecordCount != 0) && (ErstFindRecord (ErstComm->RecordID) != NULL))
    );

  //  DEBUG ((DEBUG_ERROR, "Exiting E2EClear\n"));

  return UNIT_TEST_PASSED;
}

/**
  End2End Write, Read, Clear test

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2EWriteReadClearTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT  *TestInfo;
  UINT32               ErstSize;
  UINT32               PayloadSize;
  UINT8                PayloadData;
  UINT64               RecordOffset;
  UINT64               RecordId;
  UINT32               OffsetMax;
  UINT32               SizeIndex;
  UINT32               RemainingSizeInBlock;
  UINT32               RemainingBlocks;

  //  DEBUG ((DEBUG_INFO, "Inside E2EWriteReadClearTest\n"));

  TestInfo  = (COMMON_TEST_CONTEXT *)Context;
  ErstSize  = TestInfo->TestValue;
  SizeIndex = TestInfo->Offset;

  // Since we recover cleared blocks,  we should never run out
  RemainingBlocks      = 2*ErstSize/mErrorSerialization.BlockSize;
  RemainingSizeInBlock = mErrorSerialization.BlockSize;
  RecordId             = SizeIndex + ErstSize; // Pseudo-random value

  while (RemainingBlocks > 0) {
    while (RemainingSizeInBlock >= (PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES] + sizeof (EFI_COMMON_ERROR_RECORD_HEADER))) {
      //      DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
      PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
      OffsetMax    = ERROR_LOG_INFO_BUFFER_SIZE - PayloadSize - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
      RecordOffset = OffsetMax; // JDS TODO - figure out a better way to do this
      PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
      E2EWrite (
        Context,
        RecordId,
        RecordOffset,
        PayloadSize,
        PayloadData,
        EFI_ACPI_6_4_ERST_STATUS_SUCCESS
        );
      E2ERead (
        Context,
        RecordId,
        RecordOffset,
        PayloadSize,
        PayloadData,
        EFI_ACPI_6_4_ERST_STATUS_SUCCESS
        );
      E2EClear (
        Context,
        RecordId,
        RecordOffset,
        PayloadSize,
        PayloadData,
        EFI_ACPI_6_4_ERST_STATUS_SUCCESS
        );
      RecordId++;
      SizeIndex++;
      RemainingSizeInBlock -= (sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize);
    }

    RemainingBlocks--;
    RemainingSizeInBlock = mErrorSerialization.BlockSize;
  }

  //  DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
  // Since we've cleared each write, we should be able to reclaim cleared blocks to make space for this write
  PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
  RecordOffset = 0;
  PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
  E2EWrite (
    Context,
    RecordId,
    RecordOffset,
    PayloadSize,
    PayloadData,
    EFI_ACPI_6_4_ERST_STATUS_SUCCESS
    );

  return UNIT_TEST_PASSED;
}

/**
  End2End Empty Flash Read test

  @param Context                      Unused

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2EEmptyFlashReadTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  //  DEBUG ((DEBUG_INFO, "Inside E2ESEmptyFlashReadTest\n"));

  // Test Empty RecordStore (with "First" RecordId)
  E2ERead (
    Context,
    ERST_FIRST_RECORD_ID,
    0x0,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_RECORD_STORE_EMPTY
    );

  // Test Empty RecordStore (with "Invalid" RecordId)
  E2ERead (
    Context,
    ERST_INVALID_RECORD_ID,
    0x0,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_RECORD_STORE_EMPTY
    );

  // Test Empty RecordStore (with a valid RecordId)
  E2ERead (
    Context,
    0x123,       // A "valid" ID
    0x0,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_RECORD_STORE_EMPTY
    );

  // Test Empty RecordStore (with invalid buffer parameters)
  E2ERead (
    Context,
    ERST_FIRST_RECORD_ID,
    MAX_UINT64,       // An invalid offset
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_RECORD_STORE_EMPTY
    );

  return UNIT_TEST_PASSED;
}

/**
  End2End Empty Flash Clear test

  @param Context                      Unused

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2EEmptyFlashClearTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  //  DEBUG ((DEBUG_INFO, "Inside E2ESEmptyFlashClearTest\n"));

  // Test Empty RecordStore (with "First" RecordId)
  E2EClear (
    Context,
    ERST_FIRST_RECORD_ID,
    0x0,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_FAILED
    );

  // Test Empty RecordStore (with "Invalid" RecordId)
  E2EClear (
    Context,
    ERST_INVALID_RECORD_ID,
    0x0,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_RECORD_STORE_EMPTY
    );

  // Test Empty RecordStore (with a valid RecordId)
  E2EClear (
    Context,
    0x123,       // A "valid" ID
    0x0,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_RECORD_STORE_EMPTY
    );

  return UNIT_TEST_PASSED;
}

/**
  End2End Read test

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2ESimpleReadTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT  *TestInfo;
  UINT32               ErstSize;
  UINT32               PayloadSize;
  UINT8                PayloadData;
  UINT64               RecordOffset;
  UINT64               RecordId;
  UINT32               OffsetMax;
  UINT32               SizeIndex;
  UINT32               RemainingSizeInBlock;
  UINT32               RemainingBlocks;
  ERST_COMM_STRUCT     *ErstComm;

  //  DEBUG ((DEBUG_INFO, "Inside E2ESimpleReadTest\n"));

  TestInfo  = (COMMON_TEST_CONTEXT *)Context;
  ErstSize  = TestInfo->TestValue;
  SizeIndex = TestInfo->Offset;
  ErstComm  = (ERST_COMM_STRUCT *)TestErstBuffer;

  // Fill the SPINOR with data to read out if empty
  if (ErstComm->RecordCount == 0) {
    //    DEBUG ((DEBUG_INFO, "Filling Flash\n"));
    E2ESimpleFillTest (Context);
  }

  // Should fail a read that has an offset too large for header
  E2ERead (
    Context,
    0x1,

    mErrorSerialization.BufferInfo.ErrorLogInfo.Length - sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + 1,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_NOT_ENOUGH_SPACE
    );
  UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);

  RemainingBlocks      = ErstSize/mErrorSerialization.BlockSize;
  RemainingSizeInBlock = mErrorSerialization.BlockSize;
  RecordId             = SizeIndex + ErstSize; // Pseudo-random value

  //  DEBUG ((DEBUG_INFO, "Reading Flash\n"));
  while (RemainingBlocks > 1) {
    while (RemainingSizeInBlock >= PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES]) {
      //      DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
      PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
      OffsetMax    = ERROR_LOG_INFO_BUFFER_SIZE - PayloadSize - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
      RecordOffset = OffsetMax; // JDS TODO - figure out a better way to do this
      PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
      E2ERead (
        Context,
        RecordId,
        RecordOffset,
        PayloadSize,
        PayloadData,
        EFI_ACPI_6_4_ERST_STATUS_SUCCESS
        );
      RecordId++;
      SizeIndex++;
      RemainingSizeInBlock -= (sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize);
    }

    RemainingBlocks--;
    RemainingSizeInBlock = mErrorSerialization.BlockSize;
  }

  //  DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
  PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
  RecordOffset = 0;
  PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
  E2ERead (
    Context,
    RecordId,
    RecordOffset,
    PayloadSize,
    PayloadData,
    EFI_ACPI_6_4_ERST_STATUS_RECORD_NOT_FOUND
    );

  return UNIT_TEST_PASSED;
}

/**
  End2End Recovery Read test

  Fill the SPINOR, then mark it as out of sync, and then read the entries out,
  triggering a recovery before the reads.

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2ESimpleRecoveryReadTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT  *TestInfo;
  UINT32               ErstSize;
  UINT32               PayloadSize;
  UINT8                PayloadData;
  UINT64               RecordOffset;
  UINT64               RecordId;
  UINT32               OffsetMax;
  UINT32               SizeIndex;
  UINT32               RemainingSizeInBlock;
  UINT32               RemainingBlocks;
  UNIT_TEST_STATUS     UTStatus;

  //  DEBUG ((DEBUG_INFO, "Inside E2ESimpleRecoveryReadTest\n"));

  TestInfo  = (COMMON_TEST_CONTEXT *)Context;
  ErstSize  = TestInfo->TestValue;
  SizeIndex = TestInfo->Offset;

  // Fill the SPINOR with data to read out
  //  DEBUG ((DEBUG_INFO, "Filling Flash\n"));
  E2ESimpleFillTest (Context);

  // Mark it as out of sync
  //  DEBUG ((DEBUG_INFO, "DeSyncing Flash\n"));
  mErrorSerialization.UnsyncedSpinorChanges = 1;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  RemainingBlocks      = ErstSize/mErrorSerialization.BlockSize;
  RemainingSizeInBlock = mErrorSerialization.BlockSize;
  RecordId             = SizeIndex + ErstSize; // Pseudo-random value

  //  DEBUG ((DEBUG_INFO, "Reading Flash\n"));
  while (RemainingBlocks > 1) {
    while (RemainingSizeInBlock >= PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES]) {
      //      DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
      PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
      OffsetMax    = ERROR_LOG_INFO_BUFFER_SIZE - PayloadSize - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
      RecordOffset = OffsetMax; // JDS TODO - figure out a better way to do this
      PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
      E2ERead (
        Context,
        RecordId,
        RecordOffset,
        PayloadSize,
        PayloadData,
        EFI_ACPI_6_4_ERST_STATUS_SUCCESS
        );
      RecordId++;
      SizeIndex++;
      RemainingSizeInBlock -= (sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize);
    }

    RemainingBlocks--;
    RemainingSizeInBlock = mErrorSerialization.BlockSize;
  }

  //  DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
  PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
  RecordOffset = 0;
  PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
  E2ERead (
    Context,
    RecordId,
    RecordOffset,
    PayloadSize,
    PayloadData,
    EFI_ACPI_6_4_ERST_STATUS_RECORD_NOT_FOUND
    );

  return UTStatus;
}

/**
  End2End "Boot" Read test

  This test reads out the first record using ERST_FIRST_RECORD_ID, and then
  using the ERSTComm->RecordID field to get the rest of the IDs, until it has
  read out the whole SPINOR

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2ESimpleBootTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT  *TestInfo;
  UINT32               ErstSize;
  UINT32               PayloadSize;
  UINT8                PayloadData;
  UINT64               RecordOffset;
  UINT64               RecordId;
  UINT64               ReadRecordId;
  UINT64               FirstRecordId;
  UINT32               OffsetMax;
  UINT32               SizeIndex;
  UINT32               RemainingSizeInBlock;
  UINT32               RemainingBlocks;
  ERST_COMM_STRUCT     *ErstComm;

  //  DEBUG ((DEBUG_INFO, "Inside E2ESimpleBootTest\n"));

  TestInfo  = (COMMON_TEST_CONTEXT *)Context;
  ErstSize  = TestInfo->TestValue;
  SizeIndex = TestInfo->Offset;
  ErstComm  = (ERST_COMM_STRUCT *)TestErstBuffer;

  // Fill the SPINOR with data to read out
  //  DEBUG ((DEBUG_INFO, "Filling Flash\n"));
  E2ESimpleFillTest (Context);

  RemainingBlocks      = ErstSize/mErrorSerialization.BlockSize;
  RemainingSizeInBlock = mErrorSerialization.BlockSize;
  RecordId             = SizeIndex + ErstSize; // Pseudo-random value
  ReadRecordId         = ERST_FIRST_RECORD_ID;
  FirstRecordId        = RecordId;

  while (RemainingBlocks > 1) {
    while (RemainingSizeInBlock >= PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES]) {
      //      DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
      PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
      OffsetMax    = ERROR_LOG_INFO_BUFFER_SIZE - PayloadSize - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
      RecordOffset = OffsetMax; // JDS TODO - figure out a better way to do this
      PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
      E2ERead (
        Context,
        ReadRecordId,
        RecordOffset,
        PayloadSize,
        PayloadData,
        EFI_ACPI_6_4_ERST_STATUS_SUCCESS
        );
      RecordId++;
      SizeIndex++;
      RemainingSizeInBlock -= (sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize);
      ReadRecordId          = ErstComm->RecordID;
    }

    RemainingBlocks--;
    RemainingSizeInBlock = mErrorSerialization.BlockSize;
  }

  UT_ASSERT_EQUAL (ErstComm->RecordID, FirstRecordId);
  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
SimFailTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  DEBUG ((DEBUG_INFO, "Inside SimFailTest\n"));

  E2EWrite (Context, 0x327b23c6643c9869, 0, 0x3f80, 0x3, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2EWrite (Context, 0x19495cff2ae8944a, 0, 0x3f80, 0xc, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2EWrite (Context, 0x46e87ccd3d1b58ba, 0, 0x3f80, 0xb, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2EWrite (Context, 0x41b71efb79e2a9e3, 0, 0x3f80, 0x6, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2EWrite (Context, 0x5bd062c212200854, 0, 0x3f80, 0x8, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2EWrite (Context, 0x1f16e9e81190cde7, 0, 0x3f80, 0xd, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2EWrite (Context, 0x3352255a109cf92e, 0, 0x3f80, 0x3, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2EWrite (Context, 0x1befd79f41a7c4c9, 0, 0x3f80, 0xa, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);

  E2ERead (Context, 0x327b23c6643c9869, 0, 0x3f80, 0x3, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2ERead (Context, 0x19495cff2ae8944a, 0, 0x3f80, 0xc, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2ERead (Context, 0x46e87ccd3d1b58ba, 0, 0x3f80, 0xb, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2ERead (Context, 0x41b71efb79e2a9e3, 0, 0x3f80, 0x6, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2ERead (Context, 0x5bd062c212200854, 0, 0x3f80, 0x8, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2ERead (Context, 0x1f16e9e81190cde7, 0, 0x3f80, 0xd, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2ERead (Context, 0x3352255a109cf92e, 0, 0x3f80, 0x3, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2ERead (Context, 0x1befd79f41a7c4c9, 0, 0x3f80, 0xa, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  End2End Clear test

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
E2ESimpleClearTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT  *TestInfo;
  UINT32               ErstSize;
  UINT32               PayloadSize;
  UINT8                PayloadData;
  UINT64               RecordOffset;
  UINT64               RecordId;
  UINT32               OffsetMax;
  UINT32               SizeIndex;
  UINT32               RemainingSizeInBlock;
  UINT32               RemainingBlocks;

  //  DEBUG ((DEBUG_INFO, "Inside E2ESimpleClearTest\n"));

  TestInfo  = (COMMON_TEST_CONTEXT *)Context;
  ErstSize  = TestInfo->TestValue;
  SizeIndex = TestInfo->Offset;

  // Fill the SPINOR with data to read out
  //  DEBUG ((DEBUG_INFO, "Filling Flash\n"));
  E2ESimpleFillTest (Context);

  // Test Full RecordStore (with "First" RecordId)
  E2EClear (
    Context,
    ERST_FIRST_RECORD_ID,
    0x0,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_FAILED
    );

  // Test Full RecordStore (with "Invalid" RecordId)
  E2EClear (
    Context,
    ERST_INVALID_RECORD_ID,
    0x0,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_FAILED
    );

  // Test Full RecordStore (with a valid but missing RecordId)
  E2EClear (
    Context,
    0x1,       // A "valid" ID
    0x0,
    0x0,
    0xaa,
    EFI_ACPI_6_4_ERST_STATUS_RECORD_NOT_FOUND
    );

  RemainingBlocks      = ErstSize/mErrorSerialization.BlockSize;
  RemainingSizeInBlock = mErrorSerialization.BlockSize;
  RecordId             = SizeIndex + ErstSize; // Pseudo-random value

  //  DEBUG ((DEBUG_INFO, "Clearing Flash\n"));
  while (RemainingBlocks > 1) {
    while (RemainingSizeInBlock >= PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES]) {
      //      DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
      PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
      OffsetMax    = ERROR_LOG_INFO_BUFFER_SIZE - PayloadSize - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
      RecordOffset = OffsetMax; // JDS TODO - figure out a better way to do this
      PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
      E2EClear (
        Context,
        RecordId,
        RecordOffset,
        PayloadSize,
        PayloadData,
        EFI_ACPI_6_4_ERST_STATUS_SUCCESS
        );
      RecordId++;
      SizeIndex++;
      RemainingSizeInBlock -= (sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize);
    }

    RemainingBlocks--;
    RemainingSizeInBlock = mErrorSerialization.BlockSize;
  }

  //  DEBUG ((DEBUG_INFO, "Remaining blocks: %d RemainingSizeInBlock: 0x%x\n", RemainingBlocks, RemainingSizeInBlock));
  PayloadSize  = PayloadSizes[SizeIndex%MAX_PAYLOAD_SIZES];
  RecordOffset = 0;
  PayloadData  = (PayloadSize + RecordId + SizeIndex)%MAX_UINT8;
  E2EClear (
    Context,
    RecordId,
    RecordOffset,
    PayloadSize,
    PayloadData,
    EFI_ACPI_6_4_ERST_STATUS_RECORD_STORE_EMPTY
    );

  return UNIT_TEST_PASSED;
}

/**
  Tests ErstEraseBlock from CollectBlock

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
EraseBlockWhileCollectingTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS           Status;
  COMMON_TEST_CONTEXT  *TestInfo;
  UNIT_TEST_STATUS     UTStatus;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;

  // Create blocks full of deleted entries
  //  DEBUG ((DEBUG_INFO, "Before Clear\n"));
  E2ESimpleClearTest (Context);

  //  DEBUG ((DEBUG_INFO, "Before ReInit\n"));
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  SetMem (TestBuffer, BLOCK_SIZE, 0xFF);
  for (int i = 0; i < NUM_BLOCKS; i++) {
    UT_ASSERT_MEM_EQUAL (TestBuffer, TestFlashStorage+ TestInfo->ErstOffset + i*BLOCK_SIZE, BLOCK_SIZE);
  }

  return UTStatus;
}

/**
  Various invalid input tests

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
InvalidInputTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                      Status;
  COMMON_TEST_CONTEXT             *TestInfo;
  ERST_CPER_INFO                  CperInfo;
  UINT32                          RecordCount;
  ERST_BLOCK_INFO                 *BlockInfo;
  UINT16                          BlockIndex;
  UINT32                          RecordOffset;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  CPER_ERST_PERSISTENCE_INFO      *CperPI;
  UINT64                          Offset;
  UINT8                           CperStatus;
  ERST_COMM_STRUCT                *ErstComm;
  NVIDIA_NOR_FLASH_PROTOCOL       *BadNorFlashProtocol;
  UNIT_TEST_STATUS                UTStatus;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;

  Cper = (EFI_COMMON_ERROR_RECORD_HEADER *)TestBuffer;
  SetMem (Cper, sizeof (EFI_COMMON_ERROR_RECORD_HEADER), 0xaa);
  Cper->SignatureStart = EFI_ERROR_RECORD_SIGNATURE_START;
  Cper->Revision       = EFI_ERROR_RECORD_REVISION;
  Cper->SignatureEnd   = EFI_ERROR_RECORD_SIGNATURE_END;

  // Make sure there's valid data
  E2ESimpleFillTest (Context);

  CperInfo.RecordId     = mErrorSerialization.CperInfo[0].RecordId;
  CperInfo.RecordLength = mErrorSerialization.CperInfo[0].RecordLength;
  CperInfo.RecordOffset = TOTAL_NOR_FLASH_SIZE;

  // Invalid Operation
  ErstComm            = (ERST_COMM_STRUCT *)TestErstBuffer;
  ErstComm->Operation = ERST_OPERATION_INVALID;
  MmioWrite32 (0, 0);
  ErrorSerializationEventHandler (NULL, NULL, NULL, NULL);
  UT_ASSERT_EQUAL (MmioRead32 (0), 1);
  UT_ASSERT_STATUS_EQUAL (GetStatus (ErstComm), EFI_ACPI_6_4_ERST_STATUS_FAILED);

  // Unknown Operation
  ErstComm->Operation = 0xaa;
  MmioWrite32 (0, 0);
  ErrorSerializationEventHandler (NULL, NULL, NULL, NULL);
  UT_ASSERT_EQUAL (MmioRead32 (0), 1);
  UT_ASSERT_STATUS_EQUAL (GetStatus (ErstComm), EFI_ACPI_6_4_ERST_STATUS_FAILED);

  // Try to get block of record that's not present
  BlockInfo = ErstGetBlockOfRecord (&CperInfo);
  UT_ASSERT_TRUE (BlockInfo == NULL);

  // Try to get block index and not find record
  BlockIndex = ErstGetBlockIndexOfRecord (&CperInfo);
  UT_ASSERT_EQUAL (BlockIndex, 0);

  // Prepare a NULL record
  Status = ErstPrepareNewRecord (CperInfo.RecordId, CperInfo.RecordLength, NULL, FALSE);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // * ErstClearRecord and not find it in CperInfo
  Status = ErstClearRecord (&CperInfo);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  // * ErstClearRecord and not find the block for the record
  RecordOffset                                 = mErrorSerialization.CperInfo[0].RecordOffset;
  mErrorSerialization.CperInfo[0].RecordOffset = TOTAL_NOR_FLASH_SIZE;
  Status                                       = ErstClearRecord (&CperInfo);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);
  mErrorSerialization.CperInfo[0].RecordOffset = RecordOffset;

  // ** ErstWriteRecord and pass in NULL NewRecord
  Status = ErstWriteRecord (Cper, NULL, NULL, FALSE);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // *** ErstWriteRecord with ErstValidateCperHeader failing on it
  Cper->SignatureStart = EFI_ERROR_RECORD_SIGNATURE_END;
  Status               = ErstWriteRecord (Cper, NULL, &CperInfo, FALSE);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INCOMPATIBLE_VERSION);
  Cper->SignatureStart = EFI_ERROR_RECORD_SIGNATURE_START;

  // ErstWriteRecord with ErstAllocateNewRecord failing on it with max record count
  RecordCount                     = mErrorSerialization.RecordCount;
  mErrorSerialization.RecordCount = mErrorSerialization.MaxRecords;
  Status                          = ErstWriteRecord (Cper, NULL, &CperInfo, FALSE);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_OUT_OF_RESOURCES);
  mErrorSerialization.RecordCount = RecordCount;

  // *** ErstReadRecord with an offset too large for the record length
  Status = ErstReadRecord (CperInfo.RecordId, Cper, 1);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_OUT_OF_RESOURCES);

  // UndoAllocateRecord NULL input
  Status = ErstUndoAllocateRecord (NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // UndoAllocateRecord Invalid Record
  Status = ErstUndoAllocateRecord (&CperInfo);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  Status = ErstFreeRecord (NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = ErstFreeRecord (&CperInfo);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  Status = ErstDeallocateRecord (NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = ErstCollectBlock (NULL, 0, 0);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Erase w/ bad offset
  Status = ErstEraseSpiNor (1, 0);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Erase w/ bad length
  Status = ErstEraseSpiNor (0, 1);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Erase past end
  Status = ErstEraseSpiNor (0, TOTAL_NOR_FLASH_SIZE + BLOCK_SIZE);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // ErstCollectBlock when can't AddCperToList due to MaxRecords reached
  RecordCount                     = mErrorSerialization.RecordCount;
  mErrorSerialization.RecordCount = mErrorSerialization.MaxRecords;
  Status                          = ErstCollectBlock ((ERST_BLOCK_INFO *)TestBuffer, 0, 0);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_OUT_OF_RESOURCES);
  mErrorSerialization.RecordCount = RecordCount;

  // Handler when Init failed
  mErrorSerialization.InitStatus = EFI_OUT_OF_RESOURCES;
  Status                         = ErrorSerializationEventHandler (NULL, NULL, NULL, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS); // Handler always returns success
  UT_ASSERT_STATUS_EQUAL (GetStatus (ErstComm), EFI_ACPI_6_4_ERST_STATUS_NOT_ENOUGH_SPACE);
  mErrorSerialization.InitStatus = EFI_SUCCESS;

  // NOTE: The below tests break the tracking data

  // ErstRelocateOutgoing when Incoming != NULL
  mErrorSerialization.OutgoingCperInfo = &mErrorSerialization.CperInfo[0];
  mErrorSerialization.IncomingCperInfo = &mErrorSerialization.CperInfo[0];
  Status                               = ErstRelocateOutgoing ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);

  // ErstRelocateOutgoing when Outgoing == NULL
  mErrorSerialization.OutgoingCperInfo = NULL;
  mErrorSerialization.IncomingCperInfo = &mErrorSerialization.CperInfo[0];
  Status                               = ErstRelocateOutgoing ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);
  mErrorSerialization.IncomingCperInfo = NULL;

  // ErstCollectBlockInfo when CollectBlock fails
  // *** Initialize when gatherspinordata fails when ErstCollectBlockInfo fails when CollectBlock fails
  // Note: this is no longer an error condition, as we mark the block invalid and move on instead of erroring out
  // Was EFI_COMPROMISED_DATA
  CperPI         = (CPER_ERST_PERSISTENCE_INFO *)&((EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset))->PersistenceInfo;
  CperStatus     = CperPI->Status;
  CperPI->Status = 0xaa;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  CperPI->Status = CperStatus;

  // Failing to collect the blocks
  // Note: this is no longer an error condition, as we mark the block invalid and move on instead of erroring out
  // Was EFI_INCOMPATIBLE_VERSION
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);
  SetMem (TestFlashStorage, TOTAL_NOR_FLASH_SIZE, ERST_RECORD_STATUS_VALID);
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_STATUS_EQUAL (mErrorSerialization.InitStatus, EFI_SUCCESS);
  E2ERead (Context, ERST_FIRST_RECORD_ID, 0, 0, 0, EFI_ACPI_6_4_ERST_STATUS_RECORD_STORE_EMPTY); // Was EFI_ACPI_6_4_ERST_STATUS_FAILED

  // *** InitProtocol when ERST offset isn't a multiple of Nor block size
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset-1, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // InitProtocol when Nor's BlockSize is < MIN_BLOCK_SIZE and isn't a factor of MIN_BLOCK_SIZE
  Status = VirtualNorFlashInitialize (TestFlashStorage, TOTAL_NOR_FLASH_SIZE - BLOCK_SIZE, NUM_BLOCKS-1, &BadNorFlashProtocol);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UTStatus = UnitTestMockNorFlashProtocol (BadNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);
  VirtualNorFlashStubDestroy (BadNorFlashProtocol);

  // GatherBufferData when size < ERST_COMM_STRUCT
  Offset                                            = StmmCommBuffersData.Buffers.NsErstUncachedBufSize;
  StmmCommBuffersData.Buffers.NsErstUncachedBufSize = sizeof (ERST_COMM_STRUCT)-1;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  Status = ErrorSerializationGatherBufferData ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_BUFFER_TOO_SMALL);
  StmmCommBuffersData.Buffers.NsErstUncachedBufSize = Offset;

  // GatherBufferData when memory region can't hold CPER header
  // *** Initialize when gatherbudfferdata fails when memory region is too small?
  Offset                                          = StmmCommBuffersData.Buffers.NsErstCachedBufSize;
  StmmCommBuffersData.Buffers.NsErstCachedBufSize = sizeof (EFI_COMMON_ERROR_RECORD_HEADER)-1;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_BUFFER_TOO_SMALL);
  StmmCommBuffersData.Buffers.NsErstCachedBufSize = Offset;

  // Handler when NOR wasn't found
  Status = MockGetSocketNorFlashProtocol (0, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NO_MEDIA);
  UT_ASSERT_STATUS_EQUAL (mErrorSerialization.InitStatus, EFI_NO_MEDIA);
  E2ERead (Context, ERST_FIRST_RECORD_ID, 0, 0, 0, EFI_ACPI_6_4_ERST_STATUS_HARDWARE_NOT_AVAILABLE);

  return UNIT_TEST_PASSED;
}

/**
  Various Faulty Flash tests

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
FaultyFlashTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                      Status;
  COMMON_TEST_CONTEXT             *TestInfo;
  ERST_CPER_INFO                  *CperInfo;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  CPER_ERST_PERSISTENCE_INFO      *CperPI;
  UINT8                           PayloadData;
  ERST_COMM_STRUCT                *ErstComm;
  UNIT_TEST_STATUS                UTStatus;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;
  ErstComm = (ERST_COMM_STRUCT *)TestErstBuffer;

  // Test Writing with a broken flash while it's empty
  mErrorSerialization.NorFlashProtocol = FaultyNorFlashProtocol;
  E2EWrite (Context, 0x1, 0x0, 0x0, 0xaa, EFI_ACPI_6_4_ERST_STATUS_FAILED);

  // Test Dummy Writing with a broken flash while it's empty
  mErrorSerialization.NorFlashProtocol = FaultyNorFlashProtocol;
  E2EDummyWrite (Context, 0x1, 0x0, 0x0, 0xaa, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);

  // Make sure there's valid data, written with the good protocol
  mErrorSerialization.NorFlashProtocol = TestNorFlashProtocol;
  E2ESimpleFillTest (Context);

  // Gather info about a real entry
  CperInfo    = &mErrorSerialization.CperInfo[0];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));

  // E2E Read broken flash
  mErrorSerialization.NorFlashProtocol = FaultyNorFlashProtocol;
  E2ERead (
    Context,
    CperInfo->RecordId,
    CperInfo->RecordOffset,
    CperInfo->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER),
    PayloadData,
    EFI_ACPI_6_4_ERST_STATUS_FAILED
    );

  // E2E Clear broken flash
  mErrorSerialization.NorFlashProtocol = FaultyNorFlashProtocol;
  E2EClear (
    Context,
    CperInfo->RecordId,
    CperInfo->RecordOffset,
    CperInfo->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER),
    PayloadData,
    EFI_ACPI_6_4_ERST_STATUS_FAILED
    );

  // ErstWriteCperStatus
  mErrorSerialization.NorFlashProtocol = FaultyNorFlashProtocol;
  Status                               = ErstWriteCperStatus (&CperPI->Status, CperInfo);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  // ErstCopyOutgoingToincomingCper when ReadSpinor fails
  mErrorSerialization.NorFlashProtocol = FaultyNorFlashProtocol;
  Status                               = ErstCopyOutgoingToIncomingCper (&mErrorSerialization.CperInfo[0], &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount-1]);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  // ErstFindFreeSpace when Erase is required
  // 1. Clear the block
  // 2. Fill it again
  // 3. Clear an entry
  // 4. Break flash
  // 5. Write an entry
  mErrorSerialization.NorFlashProtocol = TestNorFlashProtocol;
  while (ErstComm->RecordID != ERST_INVALID_RECORD_ID) {
    E2EClear (Context, ErstComm->RecordID, 0x0, 0x0, 0x0, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  }

  E2ESimpleFillTest (Context);
  E2EClear (Context, ErstComm->RecordID, 0x0, 0x0, 0x0, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  mErrorSerialization.NorFlashProtocol = FaultyNorFlashProtocol;
  E2EWrite (Context, 0xabcd, 0x0, 0x0, 0xbb, EFI_ACPI_6_4_ERST_STATUS_FAILED);

  // Make sure initprotocol fails when we can't get flash attributes
  // WARNING: This clears the tracking information, so will break subequent tests
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (FaultyNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  // E2E Op broken flash when out of sync
  // Note: can't check anything, since mErrorSerialization is left in a bad state
  mErrorSerialization.NorFlashProtocol = FaultyNorFlashProtocol;
  mErrorSerialization.UnsyncedSpinorChanges++;
  ErrorSerializationEventHandler (NULL, NULL, NULL, NULL);

  // ErstCollectBlock when ReadSpinor fails
  mErrorSerialization.NorFlashProtocol = FaultyNorFlashProtocol;
  Status                               = ErstCollectBlock ((ERST_BLOCK_INFO *)TestBuffer, 0, 0);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_DEVICE_ERROR);

  return UNIT_TEST_PASSED;
}

/**
  Testing INCOMING/OUTGOING/INVALID scenarios

  Write Sequence is:
   - Write INCOMING status
   - Write INCOMING data
   - Write OUTGOING status
   - Write VALID status for INCOMING
   - Write DELETED status for OUTGOING

   Possible states for the RecordID throughout the write sequence are:
   - FREE
   - INCOMING
   - VALID
   or
   - VALID
   - INCOMING + VALID
   - INCOMING + OUTGOING
   - VALID + OUTGOING
   - VALID + DELETED

   INCOMING can become INVALID on Init if there's no corresponding OUTGOING, so
   we can have:
   - INVALID
   - INVALID + VALID
   - INVALID + OUTGOING (non-matching)

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
IncomingOutgoingInvalidTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT             *TestInfo;
  ERST_CPER_INFO                  *CperInfo;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  CPER_ERST_PERSISTENCE_INFO      *CperPI;
  UINT8                           PayloadData;
  UINT32                          PayloadSize;
  UINT64                          RecordId;
  ERST_CPER_INFO                  *OutgoingCperInfo;
  EFI_COMMON_ERROR_RECORD_HEADER  *OutgoingCper;
  CPER_ERST_PERSISTENCE_INFO      *OutgoingCperPI;
  UINT8                           OutgoingPayloadData;
  UINT32                          OutgoingPayloadSize;
  UINT64                          OutgoingRecordId;
  ERST_COMM_STRUCT                *ErstComm;
  UINT32                          RecordCount;
  EFI_STATUS                      Status;
  UINT8                           *Payload;
  UINT32                          CopySize;
  UINT32                          RecordIndex;
  UNIT_TEST_STATUS                UTStatus;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;
  ErstComm = (ERST_COMM_STRUCT *)TestErstBuffer;

  E2ESimpleFillTest (Context);

  RecordCount = ErstComm->RecordCount;

  // Test Gather cleaning up INVALID
  DEBUG ((DEBUG_INFO, "Testing Init with INVALID entry\n"));

  // Gather info about the last entry in the block (the only entry that can be incoming or invalid)
  CperInfo    = &mErrorSerialization.CperInfo[RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordId    = Cper->RecordID;

  // Mark it as invalid, and out of sync
  CperPI->Status = ERST_RECORD_STATUS_INVALID;
  mErrorSerialization.UnsyncedSpinorChanges++;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  // Try to read the entry, triggering a reinit, and confirm it's not there
  E2ERead (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_RECORD_NOT_FOUND);
  // Create it again
  E2EWrite (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  // Confirm all the data is correct via E2ESimpleReadTest
  E2ESimpleReadTest (Context);

  SanityCheckTracking (Context);

  // Test Gather cleaning up INCOMING without a corresponding OUTGOING
  // simulating having written the STATUS but nothing else for the CPER
  DEBUG ((DEBUG_INFO, "Testing Init with INCOMING entry (Status Only)\n"));

  // Gather info about the last entry in the block (the only entry that can be incoming or invalid)
  CperInfo    = &mErrorSerialization.CperInfo[RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordId    = Cper->RecordID;

  // Pretend we've only written the status field, and the rest is still unwritten
  SetMem (Cper, CperInfo->RecordLength, 0xff);
  // Mark it as incoming, and out of sync
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  // Try to read the entry, triggering a reinit, and confirm it's not there
  E2ERead (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_RECORD_NOT_FOUND);
  // Create it again
  E2EWrite (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  // Confirm all the data is correct via E2ESimpleReadTest
  E2ESimpleReadTest (Context);

  SanityCheckTracking (Context);

  // Test Gather cleaning up INCOMING without a corresponding OUTGOING
  // simulating having written the STATUS and the rest of the CPER but not set it to VALID yet
  DEBUG ((DEBUG_INFO, "Testing Init with INCOMING entry (Cper written)\n"));

  // Gather info about the last entry in the block (the only entry that can be incoming or invalid)
  CperInfo    = &mErrorSerialization.CperInfo[RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordId    = Cper->RecordID;

  // Mark it as incoming, and out of sync
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  // Try to read the entry, triggering a reinit, and confirm it's not there
  E2ERead (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_RECORD_NOT_FOUND);
  // Create it again
  E2EWrite (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  // Confirm all the data is correct via E2ESimpleReadTest
  E2ESimpleReadTest (Context);

  SanityCheckTracking (Context);

  // Test Gather cleaning up OUTGOING without a corresponding INCOMING or VALID
  // simulating having written the STATUS but not started the copy
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING entry (Status updated but no copy, last entry)\n"));

  // Gather info about the last entry in the block
  CperInfo    = &mErrorSerialization.CperInfo[RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordId    = Cper->RecordID;

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx\n", RecordId));
  CperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = CperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  // Confirm all the data is correct via E2ESimpleReadTest
  E2ESimpleReadTest (Context);

  SanityCheckTracking (Context);

  // Test Gather cleaning up OUTGOING without a corresponding INCOMING or VALID
  // simulating having written the STATUS but not started the copy
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING entry (Status updated but no copy, middle entry)\n"));

  // Gather info about the middle entry in the block
  CperInfo    = &mErrorSerialization.CperInfo[RecordCount/2];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordId    = Cper->RecordID;

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx\n", RecordId));
  CperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = CperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  // Confirm all the data is correct via E2ESimpleReadTest
  E2ESimpleReadTest (Context);

  SanityCheckTracking (Context);

  // Test Gather cleaning up OUTGOING without a corresponding INCOMING or VALID
  // simulating having written the STATUS but not started the copy
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING entry (Status updated but no copy, first entry)\n"));

  // Gather info about the first entry in the block
  CperInfo    = &mErrorSerialization.CperInfo[0];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordId    = Cper->RecordID;

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx\n", RecordId));
  CperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = CperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  // Confirm all the data is correct via E2ESimpleReadTest
  E2ESimpleReadTest (Context);

  SanityCheckTracking (Context);

  // Test OUTGOING with a corresponding VALID but no INCOMING
  //  simulating having copied the entry but not updated the old one to DELETED
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and VALID entry (\"Copied\" but not deleted, first entry)\n"));

  // Gather info about the first entry in the block
  CperInfo = &mErrorSerialization.CperInfo[0];
  Cper     = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI   = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  RecordId = Cper->RecordID;

  // Mark it as OUTGOING, and out of sync
  DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx\n", RecordId));
  CperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = CperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  // Gather info about the middle entry in the block
  // and change it's RecordID to the outgoing one's
  CperInfo = &mErrorSerialization.CperInfo[ErstComm->RecordCount/2];
  Cper     = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  UT_ASSERT_NOT_EQUAL (CperInfo->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER), PayloadSize);
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  DEBUG ((DEBUG_INFO, "VALID entry had ID 0x%llx\n", Cper->RecordID));
  Cper->RecordID     = RecordId;
  CperInfo->RecordId = RecordId;

  // Confirm that we get the VALID rather than the OUTGOING data when reading
  // And that the OUTGOING record has been deleted
  E2ERead (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_DELETED);

  SanityCheckTracking (Context);

  // Test OUTGOING with a corresponding VALID but no INCOMING
  //  simulating having copied the entry but not updated the old one to DELETED
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and VALID entry (\"Copied\" but not deleted, middle entry)\n"));

  // Gather info about the middle entry in the block
  CperInfo    = &mErrorSerialization.CperInfo[ErstComm->RecordCount/2];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordId    = Cper->RecordID;

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx\n", RecordId));
  CperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = CperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  // Gather info about the last entry in the block
  // and change it's RecordID to the outgoing one's
  CperInfo = &mErrorSerialization.CperInfo[ErstComm->RecordCount-1];
  Cper     = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  UT_ASSERT_NOT_EQUAL (CperInfo->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER), PayloadSize);
  PayloadData        = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize        = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  Cper->RecordID     = RecordId;
  CperInfo->RecordId = RecordId;

  // Confirm that we get the VALID rather than the OUTGOING data when reading
  // And that the OUTGOING record has been deleted
  E2ERead (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_DELETED);

  SanityCheckTracking (Context);

  // Test OUTGOING with a corresponding VALID but no INCOMING
  //  simulating having copied the entry but not updated the old one to DELETED
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and VALID entry (\"Copied\" but not deleted, last entry)\n"));

  // Gather info about the last entry in the block
  CperInfo    = &mErrorSerialization.CperInfo[ErstComm->RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordId    = Cper->RecordID;

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx\n", RecordId));
  CperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = CperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  // Gather info about the first entry in the block
  // and change it's RecordID to the outgoing one's
  CperInfo = &mErrorSerialization.CperInfo[0];
  Cper     = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  UT_ASSERT_NOT_EQUAL (CperInfo->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER), PayloadSize);
  PayloadData        = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize        = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  Cper->RecordID     = RecordId;
  CperInfo->RecordId = RecordId;

  // Confirm that we get the VALID rather than the OUTGOING data when reading
  // And that the OUTGOING record has been deleted
  E2ERead (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_DELETED);

  SanityCheckTracking (Context);

  // Test OUTGOING without a corresponding VALID but an INCOMING
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and Incompatible INCOMING entry (\"Copy in progress\", different ID)\n"));

  // Gather info about the last entry in the block
  CperInfo    = &mErrorSerialization.CperInfo[ErstComm->RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordId    = Cper->RecordID;

  // Mark it as INCOMING, and out of sync
  DEBUG ((DEBUG_INFO, "INCOMING entry has ID 0x%llx\n", RecordId));
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  // Gather info about the middle entry in the block
  CperInfo            = &mErrorSerialization.CperInfo[ErstComm->RecordCount/2];
  Cper                = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI              = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  OutgoingPayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  OutgoingPayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER) - 1);
  OutgoingRecordId    = Cper->RecordID;

  // Mark it as OUTGOING, and out of sync
  DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx\n", OutgoingRecordId));
  CperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = CperInfo;

  // Confirm that INCOMING was invalidated and OUTGOING was moved
  E2ERead (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_RECORD_NOT_FOUND);
  E2ERead (Context, OutgoingRecordId, 0x0, OutgoingPayloadSize, OutgoingPayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);

  SanityCheckTracking (Context);

  // Test OUTGOING without a corresponding VALID but an INCOMING
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and Incompatible INCOMING entry (\"Copy in progress\", different size)\n"));

  // Gather info about the middle entry in the block
  OutgoingCperInfo    = &mErrorSerialization.CperInfo[ErstComm->RecordCount/2-1];
  OutgoingCper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + OutgoingCperInfo->RecordOffset);
  OutgoingCperPI      = (CPER_ERST_PERSISTENCE_INFO *)&OutgoingCper->PersistenceInfo;
  OutgoingPayloadData = *((UINT8 *)OutgoingCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  OutgoingPayloadSize = OutgoingCper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  OutgoingRecordId    = OutgoingCper->RecordID;
  UT_ASSERT_TRUE (OutgoingPayloadSize > 0); // Need to manually adjust the Cper selected if this fails
  UT_ASSERT_TRUE (OutgoingCperPI->Status == ERST_RECORD_STATUS_VALID);

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx and length 0x%llx\n", OutgoingRecordId, OutgoingCper->RecordLength));
  OutgoingCperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = OutgoingCperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  SanityCheckTracking (Context);

  // Gather info about the last entry in the block
  CperInfo           = &mErrorSerialization.CperInfo[ErstComm->RecordCount-1];
  Cper               = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI             = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  PayloadData        = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  PayloadSize        = MIN (OutgoingPayloadSize-1, Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  Cper->RecordLength = sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize;
  Cper->RecordID     = OutgoingRecordId;
  RecordId           = Cper->RecordID;

  // Mark it as INCOMING, and out of sync, and smaller than outgoing
  // DEBUG ((DEBUG_INFO, "INCOMING entry has ID 0x%llx and length 0x%llx\n", RecordId, Cper->RecordLength));
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;

  SanityCheckTracking (Context);

  // Confirm that INCOMING was invalidated and OUTGOING was moved
  // DEBUG ((DEBUG_INFO, "INCOMING 0x%p (0x%x) OUTGOING 0x%p (0x%x)\n", mErrorSerialization.IncomingCperInfo, CperPI->Status, mErrorSerialization.OutgoingCperInfo, OutgoingCperPI->Status));
  E2ERead (Context, OutgoingRecordId, 0x0, OutgoingPayloadSize, OutgoingPayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);

  SanityCheckTracking (Context);

  // Test OUTGOING without a corresponding VALID but an INCOMING
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and INCOMING entry (\"Copy in progress\", Completed but not marked valid)\n"));

  // Gather info about the middle entry in the block
  OutgoingCperInfo    = &mErrorSerialization.CperInfo[ErstComm->RecordCount/2];
  OutgoingCper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + OutgoingCperInfo->RecordOffset);
  OutgoingCperPI      = (CPER_ERST_PERSISTENCE_INFO *)&OutgoingCper->PersistenceInfo;
  OutgoingPayloadData = *((UINT8 *)OutgoingCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  OutgoingPayloadSize = OutgoingCper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  OutgoingRecordId    = OutgoingCper->RecordID;
  UT_ASSERT_TRUE (OutgoingPayloadSize > 0);
  UT_ASSERT_TRUE (OutgoingCperPI->Status == ERST_RECORD_STATUS_VALID);

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx and length 0x%llx\n", OutgoingRecordId, OutgoingCper->RecordLength));
  OutgoingCperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = OutgoingCperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  SanityCheckTracking (Context);

  // Gather info about the last entry in the block
  CperInfo    = GetLastEntryCperInfo (Context);// &mErrorSerialization.CperInfo[ErstComm->RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  Payload     = (UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);

  // DEBUG ((DEBUG_INFO, "INCOMING entry has ID 0x%llx and length 0x%llx\n", Cper->RecordID, Cper->RecordLength));
  // DEBUG ((DEBUG_INFO, "Cper 0x%p, Payload 0x%p, Space 0x%p\n", Cper, Payload, &Payload[PayloadSize]));

  // Copy all of the OUTGOING CPER to it and erase the rest
  if (OutgoingPayloadSize <= PayloadSize) {
    CopyMem (Cper, OutgoingCper, OutgoingCper->RecordLength);
    if (OutgoingPayloadSize < PayloadSize) {
      // DEBUG ((DEBUG_INFO, "Cper 0x%p, Payload 0x%p, NewSpace 0x%p\n", Cper, Payload, &Payload[OutgoingPayloadSize]));
      SetMem (&Payload[OutgoingPayloadSize], PayloadSize - OutgoingPayloadSize, 0xFF);
    }
  } else {
    // DEBUG ((DEBUG_INFO, "OUTGOING length 0x%llx can't fit in INCOMING length 0x%llx\n", OutgoingCper->RecordLength, Cper->RecordLength));
    UT_ASSERT_TRUE (0);
  }

  // Mark it as INCOMING, and out of sync
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;

  SanityCheckTracking (Context);

  // Confirm that OUTGOING was copied to INCOMING
  // DEBUG ((DEBUG_INFO, "Before mES.RecordCount 0x%x\n", mErrorSerialization.RecordCount));
  E2ERead (Context, OutgoingRecordId, 0x0, OutgoingPayloadSize, OutgoingPayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_VALID);
  UT_ASSERT_EQUAL (Cper->RecordID, OutgoingRecordId);
  UT_ASSERT_EQUAL (OutgoingCperPI->Status, ERST_RECORD_STATUS_DELETED);
  // DEBUG ((DEBUG_INFO, "After mES.RecordCount 0x%x\n", mErrorSerialization.RecordCount));

  SanityCheckTracking (Context);

  // Test OUTGOING without a corresponding VALID but an INCOMING
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and INCOMING entry (\"Copy in progress\", Partial Copy)\n"));

  // Gather info about the last entry in the block that can be used for INCOMING
  CperInfo    = GetLastEntryCperInfo (Context);// &mErrorSerialization.CperInfo[ErstComm->RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  RecordId    = Cper->RecordID;
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  Payload     = (UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);

  // DEBUG ((DEBUG_INFO, "INCOMING entry has ID 0x%llx and length 0x%llx\n", RecordId, Cper->RecordLength));

  // Find a suitable entry for OUTGOING toward the middle
  RecordIndex      = ErstComm->RecordCount/2-1;
  OutgoingCperInfo = &mErrorSerialization.CperInfo[RecordIndex];
  while (OutgoingCperInfo->RecordLength > CperInfo->RecordLength) {
    RecordIndex--;
    if (RecordIndex == 0) {
      RecordIndex = ErstComm->RecordCount-2;
    }

    OutgoingCperInfo = &mErrorSerialization.CperInfo[RecordIndex];
  }

  OutgoingCper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + OutgoingCperInfo->RecordOffset);
  OutgoingCperPI      = (CPER_ERST_PERSISTENCE_INFO *)&OutgoingCper->PersistenceInfo;
  OutgoingPayloadData = *((UINT8 *)OutgoingCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  OutgoingPayloadSize = OutgoingCper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  OutgoingRecordId    = OutgoingCper->RecordID;
  UT_ASSERT_TRUE (OutgoingPayloadSize > 0);
  UT_ASSERT_TRUE (OutgoingCperPI->Status == ERST_RECORD_STATUS_VALID);

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx and length 0x%llx\n", OutgoingRecordId, OutgoingCper->RecordLength));
  OutgoingCperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = OutgoingCperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  SanityCheckTracking (Context);

  // Copy half of the OUTGOING CPER to INCOMING and erase the rest
  CopySize = OutgoingCper->RecordLength/2;
  if (OutgoingPayloadSize <= PayloadSize) {
    CopyMem (Cper, OutgoingCper, CopySize);
    SetMem (&((UINT8 *)Cper)[CopySize], sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize - CopySize, 0xFF);
  } else {
    // DEBUG ((DEBUG_INFO, "OUTGOING length 0x%llx can't fit in INCOMING length 0x%llx\n", OutgoingCper->RecordLength, Cper->RecordLength));
    UT_ASSERT_TRUE (0);
  }

  // Mark INCOMING as INCOMING, and out of sync
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;

  SanityCheckTracking (Context);

  // Confirm that OUTGOING was copied to INCOMING
  E2ERead (Context, OutgoingRecordId, 0x0, OutgoingPayloadSize, OutgoingPayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_VALID);
  UT_ASSERT_EQUAL (Cper->RecordID, OutgoingRecordId);
  UT_ASSERT_EQUAL (OutgoingCperPI->Status, ERST_RECORD_STATUS_DELETED);

  SanityCheckTracking (Context);

  // Test OUTGOING without a corresponding VALID but an INCOMING
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and INCOMING entry (\"Copy in progress\", Nothing Copied yet)\n"));

  // Gather info about the last entry in the block that can be used for INCOMING
  CperInfo    = GetLastEntryCperInfo (Context);// &mErrorSerialization.CperInfo[ErstComm->RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  RecordId    = Cper->RecordID;
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  Payload     = (UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);

  // DEBUG ((DEBUG_INFO, "INCOMING entry has ID 0x%llx and length 0x%llx\n", RecordId, Cper->RecordLength));

  // Find a suitable entry for OUTGOING toward the middle
  RecordIndex      = ErstComm->RecordCount/2-1;
  OutgoingCperInfo = &mErrorSerialization.CperInfo[RecordIndex];
  while ((OutgoingCperInfo->RecordLength > CperInfo->RecordLength) ||
         (OutgoingCperInfo->RecordLength == sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))
  {
    RecordIndex--;
    if (RecordIndex == 0) {
      RecordIndex = ErstComm->RecordCount-2;
    }

    OutgoingCperInfo = &mErrorSerialization.CperInfo[RecordIndex];
  }

  OutgoingCper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + OutgoingCperInfo->RecordOffset);
  OutgoingCperPI      = (CPER_ERST_PERSISTENCE_INFO *)&OutgoingCper->PersistenceInfo;
  OutgoingPayloadData = *((UINT8 *)OutgoingCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  OutgoingPayloadSize = OutgoingCper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  OutgoingRecordId    = OutgoingCper->RecordID;
  UT_ASSERT_TRUE (OutgoingPayloadSize > 0);
  UT_ASSERT_TRUE (OutgoingCperPI->Status == ERST_RECORD_STATUS_VALID);

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx and length 0x%llx\n", OutgoingRecordId, OutgoingCper->RecordLength));
  OutgoingCperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = OutgoingCperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  SanityCheckTracking (Context);

  // Copy nothing and Erase Incoming
  CopySize = 0;
  if (OutgoingPayloadSize <= PayloadSize) {
    //    CopyMem(Cper, OutgoingCper, CopySize);
    SetMem (&((UINT8 *)Cper)[CopySize], sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize - CopySize, 0xFF);
  } else {
    // DEBUG ((DEBUG_INFO, "OUTGOING length 0x%llx can't fit in INCOMING length 0x%llx\n", OutgoingCper->RecordLength, Cper->RecordLength));
    UT_ASSERT_TRUE (0);
  }

  // Mark INCOMING as INCOMING, and out of sync
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;

  SanityCheckTracking (Context);

  // Confirm that OUTGOING was copied to INCOMING
  E2ERead (Context, OutgoingRecordId, 0x0, OutgoingPayloadSize, OutgoingPayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_VALID);
  UT_ASSERT_EQUAL (Cper->RecordID, OutgoingRecordId);
  UT_ASSERT_EQUAL (OutgoingCperPI->Status, ERST_RECORD_STATUS_DELETED);

  SanityCheckTracking (Context);

  // Test OUTGOING without a corresponding VALID but an INCOMING when data isn't compatible
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and INCOMING entry (\"Copy in progress\", Partial Incompatible Copy)\n"));

  // Gather info about the last entry in the block that can be used for INCOMING
  CperInfo    = GetLastEntryCperInfo (Context);// &mErrorSerialization.CperInfo[ErstComm->RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  RecordId    = Cper->RecordID;
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  Payload     = (UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);

  // DEBUG ((DEBUG_INFO, "INCOMING entry has ID 0x%llx and length 0x%llx\n", RecordId, Cper->RecordLength));

  // Find a suitable entry for OUTGOING toward the middle
  RecordIndex      = ErstComm->RecordCount/2-1;
  OutgoingCperInfo = &mErrorSerialization.CperInfo[RecordIndex];
  while ((OutgoingCperInfo->RecordLength > CperInfo->RecordLength) ||
         (OutgoingCperInfo->RecordId == CperInfo->RecordId))
  {
    RecordIndex--;
    if (RecordIndex == 0) {
      RecordIndex = ErstComm->RecordCount-1;
    }

    OutgoingCperInfo = &mErrorSerialization.CperInfo[RecordIndex];
  }

  OutgoingCper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + OutgoingCperInfo->RecordOffset);
  OutgoingCperPI      = (CPER_ERST_PERSISTENCE_INFO *)&OutgoingCper->PersistenceInfo;
  OutgoingPayloadSize = OutgoingCper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  if (OutgoingPayloadSize > 0) {
    OutgoingPayloadData = *((UINT8 *)OutgoingCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  } else {
    OutgoingPayloadData = *((UINT8 *)OutgoingCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)-1);
  }

  OutgoingRecordId = OutgoingCper->RecordID;
  //  UT_ASSERT_TRUE(OutgoingPayloadSize > 0);
  UT_ASSERT_TRUE (OutgoingCperPI->Status == ERST_RECORD_STATUS_VALID);

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx and length 0x%llx\n", OutgoingRecordId, OutgoingCper->RecordLength));
  OutgoingCperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = OutgoingCperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  SanityCheckTracking (Context);

  // Copy half of the OUTGOING CPER to INCOMING and erase the rest
  CopySize = OutgoingCper->RecordLength/2;
  if (OutgoingPayloadSize <= PayloadSize) {
    CopyMem (Cper, OutgoingCper, CopySize);
    SetMem (&((UINT8 *)Cper)[CopySize], sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize - CopySize, 0xFF);
    // Corrupt a byte
    ((UINT8 *)Cper)[0] = ~((UINT8 *)Cper)[0];
  } else {
    // DEBUG ((DEBUG_INFO, "OUTGOING length 0x%llx can't fit in INCOMING length 0x%llx\n", OutgoingCper->RecordLength, Cper->RecordLength));
    UT_ASSERT_TRUE (0);
  }

  // Mark INCOMING as INCOMING, and out of sync
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;

  SanityCheckTracking (Context);

  // Confirm that OUTGOING was moved and INCOMING was INVALIDATED (note: both blocks get freed after the move)
  E2ERead (Context, OutgoingRecordId, 0x0, OutgoingPayloadSize, OutgoingPayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_FREE);
  UT_ASSERT_EQUAL (OutgoingCperPI->Status, ERST_RECORD_STATUS_FREE);

  SanityCheckTracking (Context);

  // Test OUTGOING without a corresponding VALID but an INCOMING
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and INCOMING entry (\"Copy in progress\", Partial Incompatible Copy, end)\n"));

  // Gather info about the last entry in the block that can be used for INCOMING
  CperInfo    = GetLastEntryCperInfo (Context);// &mErrorSerialization.CperInfo[ErstComm->RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  RecordId    = Cper->RecordID;
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  Payload     = (UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);

  // DEBUG ((DEBUG_INFO, "INCOMING entry has ID 0x%llx and length 0x%llx\n", RecordId, Cper->RecordLength));

  // Find a suitable entry for OUTGOING toward the middle
  RecordIndex      = ErstComm->RecordCount/2-1;
  OutgoingCperInfo = &mErrorSerialization.CperInfo[RecordIndex];
  while ((OutgoingCperInfo->RecordLength > CperInfo->RecordLength) ||
         (OutgoingCperInfo->RecordId == CperInfo->RecordId))
  {
    RecordIndex--;
    if (RecordIndex == 0) {
      RecordIndex = ErstComm->RecordCount-1;
    }

    OutgoingCperInfo = &mErrorSerialization.CperInfo[RecordIndex];
  }

  OutgoingCper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + OutgoingCperInfo->RecordOffset);
  OutgoingCperPI      = (CPER_ERST_PERSISTENCE_INFO *)&OutgoingCper->PersistenceInfo;
  OutgoingPayloadSize = OutgoingCper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  if (OutgoingPayloadSize > 0) {
    OutgoingPayloadData = *((UINT8 *)OutgoingCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  } else {
    OutgoingPayloadData = *((UINT8 *)OutgoingCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)-1);
  }

  OutgoingRecordId = OutgoingCper->RecordID;
  //  UT_ASSERT_TRUE(OutgoingPayloadSize > 0);
  UT_ASSERT_TRUE (OutgoingCperPI->Status == ERST_RECORD_STATUS_VALID);

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx and length 0x%llx\n", OutgoingRecordId, OutgoingCper->RecordLength));
  OutgoingCperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = OutgoingCperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  SanityCheckTracking (Context);

  // Copy half of the OUTGOING CPER to INCOMING and erase the rest
  CopySize = OutgoingCper->RecordLength/2;
  if (OutgoingPayloadSize <= PayloadSize) {
    CopyMem (Cper, OutgoingCper, CopySize);
    SetMem (&((UINT8 *)Cper)[CopySize], sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize - CopySize, 0xFF);
    // Corrupt a byte
    ((UINT8 *)Cper)[OutgoingCper->RecordLength-1] = ~((UINT8 *)Cper)[OutgoingCper->RecordLength-1];
  } else {
    // DEBUG ((DEBUG_INFO, "OUTGOING length 0x%llx can't fit in INCOMING length 0x%llx\n", OutgoingCper->RecordLength, Cper->RecordLength));
    UT_ASSERT_TRUE (0);
  }

  // Mark INCOMING as INCOMING, and out of sync
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;

  SanityCheckTracking (Context);

  // Confirm that OUTGOING was moved and INCOMING was INVALIDATED
  E2ERead (Context, OutgoingRecordId, 0x0, OutgoingPayloadSize, OutgoingPayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_FREE);
  UT_ASSERT_EQUAL (OutgoingCperPI->Status, ERST_RECORD_STATUS_FREE);

  SanityCheckTracking (Context);

  // Test OUTGOING without a corresponding VALID but an INCOMING when space after INCOMING isn't completely FREE
  DEBUG ((DEBUG_INFO, "Testing Init with OUTGOING and INCOMING entry (\"Copy in progress\", After INCOMING not entirely FREE)\n"));

  // Gather info about the last entry in the block that can be used for INCOMING
  CperInfo    = GetLastEntryCperInfo (Context);// &mErrorSerialization.CperInfo[ErstComm->RecordCount-1];
  Cper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  RecordId    = Cper->RecordID;
  CperPI      = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  Payload     = (UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  PayloadSize = Cper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);

  // DEBUG ((DEBUG_INFO, "INCOMING entry has ID 0x%llx and length 0x%llx\n", RecordId, Cper->RecordLength));

  // Find a suitable entry for OUTGOING toward the middle
  RecordIndex      = ErstComm->RecordCount/2-1;
  OutgoingCperInfo = &mErrorSerialization.CperInfo[RecordIndex];
  while ((OutgoingCperInfo->RecordLength > CperInfo->RecordLength) ||
         (OutgoingCperInfo->RecordId == CperInfo->RecordId))
  {
    RecordIndex--;
    if (RecordIndex == 0) {
      RecordIndex = ErstComm->RecordCount-1;
    }

    OutgoingCperInfo = &mErrorSerialization.CperInfo[RecordIndex];
  }

  OutgoingCper        = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + OutgoingCperInfo->RecordOffset);
  OutgoingCperPI      = (CPER_ERST_PERSISTENCE_INFO *)&OutgoingCper->PersistenceInfo;
  OutgoingPayloadSize = OutgoingCper->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  if (OutgoingPayloadSize > 0) {
    OutgoingPayloadData = *((UINT8 *)OutgoingCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  } else {
    OutgoingPayloadData = *((UINT8 *)OutgoingCper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)-1);
  }

  OutgoingRecordId = OutgoingCper->RecordID;
  //  UT_ASSERT_TRUE(OutgoingPayloadSize > 0);
  UT_ASSERT_TRUE (OutgoingCperPI->Status == ERST_RECORD_STATUS_VALID);

  // Mark it as OUTGOING, and out of sync
  // DEBUG ((DEBUG_INFO, "OUTGOING entry has ID 0x%llx and length 0x%llx\n", OutgoingRecordId, OutgoingCper->RecordLength));
  OutgoingCperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = OutgoingCperInfo;
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  SanityCheckTracking (Context);

  // Copy half of the OUTGOING CPER to INCOMING and erase the rest
  CopySize = OutgoingCper->RecordLength/2;
  if (OutgoingPayloadSize <= PayloadSize) {
    CopyMem (Cper, OutgoingCper, CopySize);
    SetMem (&((UINT8 *)Cper)[CopySize], sizeof (EFI_COMMON_ERROR_RECORD_HEADER) + PayloadSize - CopySize, 0xFF);
  } else {
    // DEBUG ((DEBUG_INFO, "OUTGOING length 0x%llx can't fit in INCOMING length 0x%llx\n", OutgoingCper->RecordLength, Cper->RecordLength));
    UT_ASSERT_TRUE (0);
  }

  // Corrupt the last byte in the block
  UINT8  *LastByte = (UINT8 *)Cper + (mErrorSerialization.BlockSize - CperInfo->RecordOffset%mErrorSerialization.BlockSize - 1);

  // DEBUG ((DEBUG_INFO, "Cper 0x%p, LastByte 0x%p\n", Cper, LastByte));
  *LastByte = ~*LastByte;

  // Mark INCOMING as INCOMING, and out of sync
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;

  SanityCheckTracking (Context);

  // Confirm that OUTGOING was moved and INCOMING was INVALIDATED
  E2ERead (Context, OutgoingRecordId, 0x0, OutgoingPayloadSize, OutgoingPayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_FREE);
  UT_ASSERT_EQUAL (OutgoingCperPI->Status, ERST_RECORD_STATUS_FREE);

  // NOTE: Tests below don't do E2E, so can't use ErstComm

  // Impossible E2E scenario of deallocation that results in moving the INCOMING record
  // Gather info about the last entry in the block
  CperInfo = &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount-1];
  Cper     = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI   = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;

  // Mark it as INCOMING
  // DEBUG ((DEBUG_INFO, "INCOMING address 0x%p\n", CperInfo));
  CperPI->Status = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.IncomingCperInfo = CperInfo;
  Status                               = ErstDeallocateRecord (&mErrorSerialization.CperInfo[0]);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  //  DEBUG ((DEBUG_INFO, "INCOMING address 0x%p, Deleted address 0x%p\n", mErrorSerialization.IncomingCperInfo, &mErrorSerialization.CperInfo[0]));
  UT_ASSERT_EQUAL (mErrorSerialization.IncomingCperInfo, CperInfo-1);

  // Impossible E2E scenario of deallocation that results in moving the OUTGOING record
  // Gather info about the last entry in the block
  CperInfo = &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount-1];
  Cper     = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI   = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;

  // Mark it as OUTGOING
  // DEBUG ((DEBUG_INFO, "OUTGOING address 0x%p\n", CperInfo));
  CperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = CperInfo;
  Status                               = ErstDeallocateRecord (&mErrorSerialization.CperInfo[0]);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  //  DEBUG ((DEBUG_INFO, "OUTGOING address 0x%p, Deleted address 0x%p\n", mErrorSerialization.OutgoingCperInfo, &mErrorSerialization.CperInfo[0]));
  UT_ASSERT_EQUAL (mErrorSerialization.OutgoingCperInfo, CperInfo-1);

  // Impossible E2E scenario of Write for non-outgoing record
  // Gather info about the last entry in the block
  CperInfo = &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount-1];
  Cper     = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI   = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;

  // Mark it as OUTGOING
  // DEBUG ((DEBUG_INFO, "OUTGOING address 0x%p\n", CperInfo));
  CperPI->Status = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.UnsyncedSpinorChanges++;
  mErrorSerialization.OutgoingCperInfo = CperInfo;
  Status                               = ErstWriteRecord (Cper, NULL, CperInfo, FALSE);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);
  Status = ErstWriteRecord (Cper, &mErrorSerialization.CperInfo[0], CperInfo, FALSE);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);

  return UNIT_TEST_PASSED;
}

/**
  Various Reclaim tests

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
ReclaimTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                      Status;
  COMMON_TEST_CONTEXT             *TestInfo;
  ERST_CPER_INFO                  *CperInfo;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  CPER_ERST_PERSISTENCE_INFO      *CperPI;
  UINT8                           PayloadData;
  UINT32                          PayloadSize;
  UINT32                          RecordOffset;
  UINT64                          RecordId;
  UINT32                          OutgoingOffset;
  UINT64                          OutgoingRecordId;
  ERST_COMM_STRUCT                *ErstComm;
  ERST_BLOCK_INFO                 *BlockInfo;
  UNIT_TEST_STATUS                UTStatus;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;
  ErstComm = (ERST_COMM_STRUCT *)TestErstBuffer;

  E2ESimpleFillTest (Context);

  UINT64  EntryIndexList[3] = {
    ErstComm->RecordCount-1,                           // Last entry
    ErstComm->RecordCount/2,                           // Middle entry
    0
  };                                                   // First entry

  // Creating new entries when near full
  for (int i = 0; i < (sizeof (EntryIndexList)/sizeof (EntryIndexList[0])); i++) {
    DEBUG ((DEBUG_INFO, "Testing writing an entry when near full [%d]\n", i));
    // Gather info about the entry
    CperInfo = &mErrorSerialization.CperInfo[EntryIndexList[i]];
    Cper     = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
    //    CperPI = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
    PayloadData = *((UINT8 *)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
    PayloadSize = CperInfo->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);

    // Can't overwrite entry due to lack of space
    E2EWrite (Context, CperInfo->RecordId, 0x0, 0x0, ~PayloadData, EFI_ACPI_6_4_ERST_STATUS_NOT_ENOUGH_SPACE);
    UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);

    // Remove the entry to make space to write something
    E2EClear (Context, CperInfo->RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);

    // Write an entry with the same size as the deleted entry
    E2EWrite (Context, 0x1234 + i, 0x0, PayloadSize, ~PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);

    // Can't write another entry due to lack of space
    E2EWrite (Context, 0x1235 + i, 0x0, 0x0, ~PayloadData, EFI_ACPI_6_4_ERST_STATUS_NOT_ENOUGH_SPACE);
    UT_ASSERT_EQUAL (mErrorSerialization.UnsyncedSpinorChanges, 0);
  }

  // Replacing existing entries when near full

  // First, clear two entries to make at least 2*sizeof(CperHeader) space
  E2EClear (Context, ErstComm->RecordID, 0x0, 0x0, 0x0, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  E2EClear (Context, ErstComm->RecordID, 0x0, 0x0, 0x0, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);

  // Next, Replace an entry and set its payload to sizeof(CperHeader)
  DEBUG ((DEBUG_INFO, "Testing Replacing an entry with payload size of a header\n"));
  RecordId     = ErstComm->RecordID;
  CperInfo     = ErstFindRecord (RecordId);
  PayloadSize  = CperInfo->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordOffset = CperInfo->RecordOffset;
  E2EWrite (Context, RecordId, 0x0, sizeof (EFI_COMMON_ERROR_RECORD_HEADER), PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  CperInfo = ErstFindRecord (RecordId);
  UT_ASSERT_NOT_EQUAL (RecordOffset, CperInfo->RecordOffset);

  // Now, replace it without a payload
  DEBUG ((DEBUG_INFO, "Testing Replacing an entry with payload size 0\n"));
  RecordOffset = CperInfo->RecordOffset;
  E2EWrite (Context, RecordId, 0x0, 0x0, ~PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  CperInfo = ErstFindRecord (RecordId);
  UT_ASSERT_NOT_EQUAL (RecordOffset, CperInfo->RecordOffset);

  // Finally, replace it with original payload size
  DEBUG ((DEBUG_INFO, "Testing Replacing an entry with original payload size\n"));
  RecordOffset = CperInfo->RecordOffset;
  E2EWrite (Context, RecordId, 0x0, PayloadSize, PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  CperInfo = ErstFindRecord (RecordId);
  UT_ASSERT_NOT_EQUAL (RecordOffset, CperInfo->RecordOffset);

  // Throw an OUTGOING into the mix, to make sure reclaim moves the OUTGOING first
  // DEBUG ((DEBUG_INFO, "Trying to write a record when OUTGOING already exists\n"));
  DEBUG ((DEBUG_INFO, "Testing Replacing an entry while OUTGOING exists\n"));
  CperInfo                             = &mErrorSerialization.CperInfo[ErstComm->RecordCount/2];
  Cper                                 = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI                               = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  CperPI->Status                       = ERST_RECORD_STATUS_OUTGOING;
  OutgoingOffset                       = CperInfo->RecordOffset;
  OutgoingRecordId                     = CperInfo->RecordId;
  mErrorSerialization.OutgoingCperInfo = CperInfo;
  // DEBUG ((DEBUG_INFO, "TestCase -  Outgoing=0x%p, ID=0x%llx, Len=0x%llx, Offset=0x%llx\n", CperInfo, CperInfo->RecordId, CperInfo->RecordLength, CperInfo->RecordOffset));

  RecordId     = ErstComm->RecordID;
  CperInfo     = ErstFindRecord (RecordId);
  PayloadSize  = CperInfo->RecordLength - sizeof (EFI_COMMON_ERROR_RECORD_HEADER);
  RecordOffset = CperInfo->RecordOffset;

  // Make sure there's enough space to replace
  BlockInfo   = ErstGetBlockOfRecord (CperInfo);
  PayloadSize = MIN (PayloadSize, mErrorSerialization.BlockSize - BlockInfo->UsedSize - sizeof (EFI_COMMON_ERROR_RECORD_HEADER));

  // DEBUG ((DEBUG_INFO, "TestCase -   Writing=0x%p, ID=0x%llx, Len=0x%llx, Offset=0x%llx\n", CperInfo, CperInfo->RecordId, CperInfo->RecordLength, CperInfo->RecordOffset));
  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);
  UTStatus = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);
  E2EWrite (Context, RecordId, 0x0, PayloadSize, ~PayloadData, EFI_ACPI_6_4_ERST_STATUS_SUCCESS);
  CperInfo = ErstFindRecord (RecordId);
  UT_ASSERT_NOT_EQUAL (RecordOffset, CperInfo->RecordOffset);

  CperInfo = ErstFindRecord (OutgoingRecordId);
  UT_ASSERT_NOT_EQUAL (OutgoingOffset, CperInfo->RecordOffset);
  Cper   = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  UT_ASSERT_EQUAL (CperPI->Status, ERST_RECORD_STATUS_VALID);

  // Impossible E2E scenario where Reclaim is called and can't find all the CperInfo for the block
  DEBUG ((DEBUG_INFO, "Testing Reclaim when it can't find all the CperInfo for the block\n"));
  CperInfo  = &mErrorSerialization.CperInfo[ErstComm->RecordCount/2];
  BlockInfo = ErstGetBlockOfRecord (CperInfo);
  UT_ASSERT_NOT_NULL (BlockInfo);
  BlockInfo->ValidEntries++;
  Status = ErstReclaimBlock (BlockInfo);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  return UNIT_TEST_PASSED;
}

/**
  Performs setup for WriteCperStatus tests.

  Zero out the flash and init the driver

  @param Context                      Used for Erst offset

  @retval UNIT_TEST_PASSED            Setup succeeded.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
WriteCperStatusTestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT  *TestInfo;
  EFI_STATUS           Status;
  UNIT_TEST_STATUS     UTStatus;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;

  SetMem (TestFlashStorage, TOTAL_NOR_FLASH_SIZE, 0xFF);

  MockNorErstOffset = TestInfo->ErstOffset;
  MockNorErstSize   = TOTAL_NOR_FLASH_SIZE - TestInfo->ErstOffset;
  UTStatus          = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);

  ErstMemoryInit ();
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UTStatus;
}

/**
  Tests ErstWriteCperStatus writes the CPER status correctly

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
WriteCperStatusTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                      Status;
  COMMON_TEST_CONTEXT             *TestInfo;
  EFI_COMMON_ERROR_RECORD_HEADER  *TestCper;
  CPER_ERST_PERSISTENCE_INFO      *TestCperPI;
  UINTN                           OffsetOfStatus;
  UINT8                           StatusVal;
  ERST_CPER_INFO                  CperInfo;
  UINTN                           BlockNum;
  UINTN                           CheckSize;
  UINTN                           UnwrittenBeginBytes;
  UINTN                           UnwrittenEndBytes;

  TestInfo   = (COMMON_TEST_CONTEXT *)Context;
  TestCper   = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + TestInfo->Offset);
  TestCperPI = (CPER_ERST_PERSISTENCE_INFO *)&TestCper->PersistenceInfo;
  CperInfo   = (ERST_CPER_INFO) { };

  StatusVal = (UINT8)TestInfo->TestValue;

  if ((StatusVal == ERST_RECORD_STATUS_INCOMING) ||
      (StatusVal == ERST_RECORD_STATUS_OUTGOING))
  {
    mErrorSerialization.IncomingCperInfo = NULL;
    mErrorSerialization.OutgoingCperInfo = NULL;
  } else {
    mErrorSerialization.IncomingCperInfo = &CperInfo;
    mErrorSerialization.OutgoingCperInfo = &CperInfo;
  }

  CperInfo.RecordOffset = TestInfo->Offset;
  Status                = ErstWriteCperStatus (&StatusVal, &CperInfo);

  UT_ASSERT_EQUAL (Status, TestInfo->ExpectedStatus);

  SetMem (TestBuffer, BLOCK_SIZE, 0xFF);
  if (Status == EFI_SUCCESS) {
    OffsetOfStatus = (UINT8 *)&TestCperPI->Status - TestFlashStorage;

    // Check we didn't touch bytes before Status
    UnwrittenBeginBytes = OffsetOfStatus;
    BlockNum            = 0;
    while (UnwrittenBeginBytes > 0) {
      CheckSize = MIN (BLOCK_SIZE, UnwrittenBeginBytes);
      UT_ASSERT_MEM_EQUAL (TestBuffer, TestFlashStorage + BlockNum*BLOCK_SIZE, CheckSize);
      UnwrittenBeginBytes -= CheckSize;
      BlockNum++;
    }

    // Check Status
    UT_ASSERT_EQUAL (TestCperPI->Status, StatusVal);

    // Check we didn't touch bytes after Status
    UnwrittenEndBytes = TOTAL_NOR_FLASH_SIZE - OffsetOfStatus - sizeof (TestCperPI->Status);
    BlockNum          = 0;
    while (UnwrittenEndBytes > 0) {
      CheckSize = MIN (BLOCK_SIZE, UnwrittenEndBytes);
      UT_ASSERT_MEM_EQUAL (TestBuffer, TestFlashStorage + OffsetOfStatus + sizeof (TestCperPI->Status) + BlockNum*BLOCK_SIZE, CheckSize);
      UnwrittenEndBytes -= CheckSize;
      BlockNum++;
    }

    if (StatusVal == ERST_RECORD_STATUS_INCOMING) {
      UT_ASSERT_NOT_NULL (mErrorSerialization.IncomingCperInfo);
    } else {
      UT_ASSERT_EQUAL (mErrorSerialization.IncomingCperInfo, NULL);
    }

    if (StatusVal == ERST_RECORD_STATUS_OUTGOING) {
      UT_ASSERT_NOT_NULL (mErrorSerialization.OutgoingCperInfo);
    } else {
      UT_ASSERT_EQUAL (mErrorSerialization.OutgoingCperInfo, NULL);
    }
  } else {
    for (BlockNum = 0; BlockNum < NUM_BLOCKS; BlockNum++) {
      UT_ASSERT_MEM_EQUAL (TestBuffer, TestFlashStorage + BlockNum*BLOCK_SIZE, BLOCK_SIZE);
    }
  }

  return UNIT_TEST_PASSED;
}

/**
  Tests ErstWriteCperStatus detects errors correctly

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
WriteCperStatusErrorTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS           Status;
  COMMON_TEST_CONTEXT  *TestInfo;
  UINT8                StatusVal;
  ERST_CPER_INFO       CperInfo;
  ERST_CPER_INFO       OtherCperInfo;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;
  CperInfo = (ERST_CPER_INFO) { };

  CperInfo.RecordOffset      = TestInfo->Offset;
  OtherCperInfo.RecordOffset = TestInfo->Offset + 1;

  // Make sure overwriting existing INCOMING entry works
  StatusVal                            = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.IncomingCperInfo = &CperInfo;
  Status                               = ErstWriteCperStatus (&StatusVal, &CperInfo);
  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);

  // But that if INCOMING is already a different CPER, it fails
  StatusVal                            = ERST_RECORD_STATUS_INCOMING;
  mErrorSerialization.IncomingCperInfo = &OtherCperInfo;
  Status                               = ErstWriteCperStatus (&StatusVal, &CperInfo);
  UT_ASSERT_EQUAL (Status, EFI_UNSUPPORTED);

  // Make sure overwriting existing OUTGOING entry works
  StatusVal                            = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.OutgoingCperInfo = &CperInfo;
  Status                               = ErstWriteCperStatus (&StatusVal, &CperInfo);
  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);

  // But that if OUTGOING is already a different CPER, it fails
  StatusVal                            = ERST_RECORD_STATUS_OUTGOING;
  mErrorSerialization.OutgoingCperInfo = &OtherCperInfo;
  Status                               = ErstWriteCperStatus (&StatusVal, &CperInfo);
  UT_ASSERT_EQUAL (Status, EFI_UNSUPPORTED);

  return UNIT_TEST_PASSED;
}

/**
  Performs setup for InitProtocol tests

  Init the protocols

  @param Context                      Unused

  @retval UNIT_TEST_PASSED            Setup succeeded.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
InitProtocolTestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ZeroMem (&mErrorSerialization, sizeof (mErrorSerialization));

  return UNIT_TEST_PASSED;
}

/**
  Tests that ErrorSerializationInitProtocol detects problems correctly

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
InitProtocolTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS           Status;
  COMMON_TEST_CONTEXT  *TestInfo;
  UINT32               ErstSize;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;
  ErstSize = TestInfo->TestValue;
  Status   = ErrorSerializationInitProtocol (TestNorFlashProtocol, TestInfo->ErstOffset, ErstSize);

  UT_ASSERT_EQUAL (Status, TestInfo->ExpectedStatus);

  if (Status == EFI_SUCCESS) {
    UT_ASSERT_TRUE (mErrorSerialization.BlockSize != 0);
    UT_ASSERT_TRUE (mErrorSerialization.NumBlocks != 0);
    UT_ASSERT_TRUE (mErrorSerialization.MaxRecords != 0);
    UT_ASSERT_EQUAL (mErrorSerialization.NorAttributes.BlockSize, BLOCK_SIZE);
    UT_ASSERT_EQUAL (mErrorSerialization.NorAttributes.MemoryDensity, TOTAL_NOR_FLASH_SIZE);
    UT_ASSERT_EQUAL (mErrorSerialization.NorErstOffset, TestInfo->ErstOffset);
    UT_ASSERT_TRUE (mErrorSerialization.BlockSize * mErrorSerialization.NumBlocks <= ErstSize);
  }

  return UNIT_TEST_PASSED;
}

/**
  Tests ErstValidateRecord (and ErstValidateCperHeader)

  @param Context                      Used for the offsets and status value

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
ValidateRecordTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                      Status;
  COMMON_TEST_CONTEXT             *TestInfo;
  ERST_CPER_INFO                  *CperInfo;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  CPER_ERST_PERSISTENCE_INFO      *CperPI;
  UINT64                          RecordId;
  UINT32                          PayloadSize;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;

  RecordId    = TestInfo->TestValue;
  PayloadSize = 0x10;

  // Create an entry in the spinor to corrupt
  E2EWrite (
    Context,
    RecordId,
    TestInfo->Offset,
    PayloadSize,
    0xaa,  // PayloadData
    EFI_ACPI_6_4_ERST_STATUS_SUCCESS
    );

  CperInfo = ErstFindRecord (RecordId);
  UT_ASSERT_NOT_NULL (CperInfo);
  Cper   = (EFI_COMMON_ERROR_RECORD_HEADER *)(TestFlashStorage + TestInfo->ErstOffset + CperInfo->RecordOffset);
  CperPI = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;

  // Pass in FIRST record ID
  Status = ErstValidateRecord (Cper, ERST_FIRST_RECORD_ID, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Pass in INVALID record ID
  Status = ErstValidateRecord (Cper, ERST_INVALID_RECORD_ID, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Pass in wrong record ID
  Status = ErstValidateRecord (Cper, RecordId-1, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_COMPROMISED_DATA);

  // Pass in wrong record length
  Status = ErstValidateRecord (Cper, RecordId, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER)-1);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_COMPROMISED_DATA);

  // Corrupt SignatureStart
  Cper->SignatureStart = ~Cper->SignatureStart;
  Status               = ErstValidateRecord (Cper, RecordId, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INCOMPATIBLE_VERSION);
  Cper->SignatureStart = ~Cper->SignatureStart;

  // Corrupt RecordRevision
  Cper->Revision = ~Cper->Revision;
  Status         = ErstValidateRecord (Cper, RecordId, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INCOMPATIBLE_VERSION);
  Cper->Revision = ~Cper->Revision;

  // Corrupt SignatureEnd
  Cper->SignatureEnd = ~Cper->SignatureEnd;
  Status             = ErstValidateRecord (Cper, RecordId, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INCOMPATIBLE_VERSION);
  Cper->SignatureEnd = ~Cper->SignatureEnd;

  // Corrupt RecordId to FIRST
  Cper->RecordID = ERST_FIRST_RECORD_ID;
  Status         = ErstValidateRecord (Cper, RecordId, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_COMPROMISED_DATA);
  Status = ErstValidateCperHeader (Cper);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_COMPROMISED_DATA);
  Cper->RecordID = RecordId;

  // Corrupt RecordId to INVALID
  Cper->RecordID = ERST_INVALID_RECORD_ID;
  Status         = ErstValidateRecord (Cper, RecordId, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_COMPROMISED_DATA);
  Status = ErstValidateCperHeader (Cper);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_COMPROMISED_DATA);
  Cper->RecordID = RecordId;

  // Corrupt PI->Signature
  CperPI->Signature = ~CperPI->Signature;
  Status            = ErstValidateRecord (Cper, RecordId, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INCOMPATIBLE_VERSION);
  CperPI->Signature = ~CperPI->Signature;

  // Corrupt PI->Major
  CperPI->Major = ~CperPI->Major;
  Status        = ErstValidateRecord (Cper, RecordId, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INCOMPATIBLE_VERSION);
  CperPI->Major = ~CperPI->Major;

  // Corrupt PI->Minor
  CperPI->Minor = ~CperPI->Minor;
  Status        = ErstValidateRecord (Cper, RecordId, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INCOMPATIBLE_VERSION);
  CperPI->Minor = ~CperPI->Minor;

  // Corrupt PI->Status
  CperPI->Status = ~CperPI->Status;
  Status         = ErstValidateRecord (Cper, RecordId, PayloadSize+sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  UT_ASSERT_STATUS_EQUAL (Status, EFI_COMPROMISED_DATA);
  CperPI->Status = ~CperPI->Status;

  // RelocateRecord when ValidateRecord fails
  CperPI->Status = ~CperPI->Status;
  Status         = ErstRelocateRecord (CperInfo);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_COMPROMISED_DATA);
  CperPI->Status = ~CperPI->Status;

  return UNIT_TEST_PASSED;
}

/**
  Performs setup for SpinorRead tests.

  Zeroes out the in-memory buffer. Sets the flash memory
  to be filled with the value 0xFF, and the target flash memory
  to 0x55. This is so that we can determine what
  parts of memory are/aren't read when testing.

  @param Context            Contains the range information

  @retval UNIT_TEST_PASSED  Setup finished successfully.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
SpinorReadTestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT  *TestInfo;
  EFI_STATUS           Status;
  UNIT_TEST_STATUS     UTStatus;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;

  SetMem (TestFlashStorage, TOTAL_NOR_FLASH_SIZE, 0xFF);

  MockNorErstOffset = TestInfo->ErstOffset;
  MockNorErstSize   = TOTAL_NOR_FLASH_SIZE - TestInfo->ErstOffset;
  UTStatus          = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);

  ErstMemoryInit ();
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  ZeroMem (TestBuffer, BLOCK_SIZE);

  return UTStatus;
}

/**
  Tests Read functionality.

  For the given test case, check that exactly correct number of bytes are
  read from the correct location, and that the return status is correct.

  Assumes SpinorReadTestSetup was called before this function.

  @param Context                      A pointer to a COMMON_TEST_CONTEXT that
                                      describes the case to be tested as well as
                                      the expected values for returns.

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
SpinorReadTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT  *TestInfo;
  EFI_STATUS           Status;
  UINTN                NumBytes;
  UINT8                *ReadStartAddress;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;

  NumBytes         = TestInfo->TestValue;
  ReadStartAddress = TestFlashStorage + TestInfo->ErstOffset + TestInfo->Offset;

  Status = ErstReadSpiNor (
             TestBuffer,
             TestInfo->Offset,
             NumBytes
             );
  // DEBUG ((DEBUG_INFO, "Status = %d, expected %d\n", Status, TestInfo->ExpectedStatus));

  if (Status != EFI_SUCCESS) {
    NumBytes = 0;
  }

  UT_ASSERT_STATUS_EQUAL (Status, TestInfo->ExpectedStatus);
  UT_ASSERT_MEM_EQUAL (TestBuffer, ReadStartAddress, NumBytes);
  if (NumBytes < BLOCK_SIZE) {
    UT_ASSERT_TRUE (IsZeroBuffer (TestBuffer+NumBytes, BLOCK_SIZE-NumBytes));
  }

  return UNIT_TEST_PASSED;
}

/**
  Performs setup for SpinorWrite tests.

  Zeroes out the flash memory. Sets the test buffer
  to be filled with the value 0x55 for the written part and 0xFF
  for the rest. This is so that we can determine what
  parts of memory are/aren't written when testing.

  @param Context            Contains the range information

  @retval UNIT_TEST_PASSED  Setup finished successfully.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
SpinorWriteTestSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT  *TestInfo;
  UINTN                NumBytes;
  EFI_STATUS           Status;
  UNIT_TEST_STATUS     UTStatus;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;
  NumBytes = TestInfo->TestValue;

  SetMem (TestFlashStorage, TOTAL_NOR_FLASH_SIZE, 0xFF);

  MockNorErstOffset = TestInfo->ErstOffset;
  MockNorErstSize   = TOTAL_NOR_FLASH_SIZE - TestInfo->ErstOffset;
  UTStatus          = UnitTestMockNorFlashProtocol (TestNorFlashProtocol, MockNorErstOffset, MockNorErstSize);
  UT_ASSERT_STATUS_EQUAL (UTStatus, UNIT_TEST_PASSED);

  MockGetFirstGuidHob (&gNVIDIAStMMBuffersGuid, &StmmCommBuffersData);

  ErstMemoryInit ();
  Status = ErrorSerializationReInit ();
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  SetMem (TestBuffer, MIN (NumBytes, BLOCK_SIZE), (UINT8)0x55);
  if (NumBytes < BLOCK_SIZE) {
    SetMem (TestBuffer, BLOCK_SIZE-NumBytes, 0xFF);
  }

  return UTStatus;
}

/**
  Tests Write functionality.

  For the given test case, check that exactly correct number of bytes are
  written to the correct location, and that the return status is correct.

  Assumes SpinorWriteTestSetup was called before this function.

  @param Context                      A pointer to a COMMON_TEST_CONTEXT that
                                      describes the case to be tested as well as
                                      the expected values for returns.

  @retval UNIT_TEST_PASSED            All assertions passed.
  @retval UNIT_TEST_ERROR_TEST_FAILED An assertion failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
SpinorWriteTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  COMMON_TEST_CONTEXT  *TestInfo;
  EFI_STATUS           Status;
  UINTN                NumBytes;
  UINTN                UnwrittenBeginBytes;
  UINTN                UnwrittenEndBytes;
  UINT8                *WriteStartAddress;
  UINTN                BlockNum;
  UINTN                CheckSize;

  TestInfo = (COMMON_TEST_CONTEXT *)Context;

  NumBytes          = TestInfo->TestValue;
  WriteStartAddress = TestFlashStorage + TestInfo->ErstOffset + TestInfo->Offset;

  Status = ErstWriteSpiNor (
             TestBuffer,
             TestInfo->Offset,
             NumBytes
             );
  // DEBUG ((DEBUG_INFO, "Status = %d, expected %d\n", Status, TestInfo->ExpectedStatus));

  if (Status != EFI_SUCCESS) {
    NumBytes = 0;
  }

  UT_ASSERT_STATUS_EQUAL (Status, TestInfo->ExpectedStatus);
  UT_ASSERT_MEM_EQUAL (TestBuffer, WriteStartAddress, NumBytes);

  // Double check that any space before write region was not written
  UnwrittenBeginBytes = TestInfo->ErstOffset + TestInfo->Offset;
  SetMem (TestBuffer, BLOCK_SIZE, 0xFF);
  BlockNum = 0;
  while (UnwrittenBeginBytes > 0) {
    CheckSize = MIN (BLOCK_SIZE, UnwrittenBeginBytes);
    UT_ASSERT_MEM_EQUAL (TestFlashStorage + BlockNum*BLOCK_SIZE, TestBuffer, CheckSize);
    UnwrittenBeginBytes -= CheckSize;
    BlockNum++;
  }

  // Double check no extra bytes were written after the write region
  UnwrittenEndBytes = (TOTAL_NOR_FLASH_SIZE)-(TestInfo->ErstOffset + TestInfo->Offset + NumBytes);
  BlockNum          = 0;
  while (UnwrittenEndBytes > 0) {
    CheckSize = MIN (BLOCK_SIZE, UnwrittenEndBytes);
    UT_ASSERT_MEM_EQUAL (WriteStartAddress + NumBytes + BlockNum*BLOCK_SIZE, TestBuffer, CheckSize);
    UnwrittenEndBytes -= CheckSize;
    BlockNum++;
  }

  return UNIT_TEST_PASSED;
}

/**
  Initializes data that will be used for the Error Serialization tests.

  Allocates space for flash storage and a buffer
  used for testing. Sets up a working flash device stub
**/
STATIC
VOID
InitTestData (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT64      ErstUncachedBufferBase;
  UINT64      ErstUncachedBufferSize;
  UINT64      ErstCachedBufferBase;
  UINT64      ErstCachedBufferSize;

  TestFlashStorage = AllocatePool (TOTAL_NOR_FLASH_SIZE);
  TestBuffer       = AllocatePool (BLOCK_SIZE);
  TestErstBuffer   = AllocatePool (ERST_BUFFER_SIZE);
  ASSERT (TestFlashStorage != NULL);
  ASSERT (TestBuffer != NULL);
  ASSERT (TestErstBuffer != NULL);

  ErstUncachedBufferBase = (UINT64)TestErstBuffer;
  ErstUncachedBufferSize = sizeof (ERST_COMM_STRUCT);
  ErstCachedBufferBase   = ErstUncachedBufferBase + ErstUncachedBufferSize;
  ErstCachedBufferSize   = ERST_BUFFER_SIZE - ErstUncachedBufferSize;

  SetMem (TestFlashStorage, TOTAL_NOR_FLASH_SIZE, 0xFF);
  ZeroMem (TestBuffer, BLOCK_SIZE);
  ZeroMem (TestErstBuffer, ERST_BUFFER_SIZE);

  Status = VirtualNorFlashInitialize (TestFlashStorage, TOTAL_NOR_FLASH_SIZE, BLOCK_SIZE, &TestNorFlashProtocol);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Failed to Initialize the VirtualNorFlash\n"));
  }

  ASSERT (Status == EFI_SUCCESS);

  Status = FaultyNorFlashInitialize (TestFlashStorage, TOTAL_NOR_FLASH_SIZE, BLOCK_SIZE, &FaultyNorFlashProtocol);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Failed to Initialize the FaultyNorFlash\n"));
  }

  ASSERT (Status == EFI_SUCCESS);

  PlatformResourcesStubLibInit ();

  StandaloneMmOpteeStubLibInitialize ();
  Status = MockGetSocketNorFlashProtocol (0, TestNorFlashProtocol);
  ASSERT (Status == EFI_SUCCESS);

  StmmCommBuffersData.Buffers.NsErstUncachedBufAddr = ErstUncachedBufferBase;
  StmmCommBuffersData.Buffers.NsErstUncachedBufSize = ErstUncachedBufferSize;
  DEBUG ((DEBUG_INFO, "Erst Uncached Base=0x%llx Size=0x%llx\n", StmmCommBuffersData.Buffers.NsErstUncachedBufAddr, StmmCommBuffersData.Buffers.NsErstUncachedBufSize));
  StmmCommBuffersData.Buffers.NsErstCachedBufAddr = ErstCachedBufferBase;
  StmmCommBuffersData.Buffers.NsErstCachedBufSize = ErstCachedBufferSize;
  DEBUG ((DEBUG_INFO, "Erst Cached Base=0x%llx Size=0x%llx\n", StmmCommBuffersData.Buffers.NsErstCachedBufAddr, StmmCommBuffersData.Buffers.NsErstCachedBufSize));
}

/**
  Cleans up the data used by the tests.

  Deallocates the flash stub and the memory used for the flash storage
  and the test buffer.
**/
STATIC
VOID
CleanUpTestData (
  VOID
  )
{
  PlatformResourcesStubLibDeinit ();
  StandaloneMmOpteeStubLibDestroy ();

  VirtualNorFlashStubDestroy (TestNorFlashProtocol);
  VirtualNorFlashStubDestroy (FaultyNorFlashProtocol);
  TestNorFlashProtocol   = NULL;
  FaultyNorFlashProtocol = NULL;

  if (TestFlashStorage != NULL) {
    FreePool (TestFlashStorage);
  }

  if (TestBuffer != NULL) {
    FreePool (TestBuffer);
  }
}

STATIC
EFIAPI
VOID
DefaultUnitTestCleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ErstFreeRuntimeMemory ();
  mErrorSerialization.BlockInfo = NULL;
  mErrorSerialization.CperInfo  = NULL;
}

/**
  Initialze the unit test framework, suite, and unit tests for the
  ErrorSerialization driver and run the unit tests.

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
  UNIT_TEST_SUITE_HANDLE      SpinorReadTestSuite;
  UNIT_TEST_SUITE_HANDLE      SpinorWriteTestSuite;
  UNIT_TEST_SUITE_HANDLE      CperStatusTestSuite;
  UNIT_TEST_SUITE_HANDLE      EraseBlockTestSuite;
  UNIT_TEST_SUITE_HANDLE      InitProtocolTestSuite;
  UNIT_TEST_SUITE_HANDLE      E2ETestSuite;
  UNIT_TEST_SUITE_HANDLE      ValidateRecordTestSuite;
  UNIT_TEST_SUITE_HANDLE      InvalidInputTestSuite;
  UNIT_TEST_SUITE_HANDLE      FaultyFlashTestSuite;
  UNIT_TEST_SUITE_HANDLE      ReclaimTestSuite;
  UNIT_TEST_SUITE_HANDLE      IncomingOutgoingInvalidTestSuite;
  UNIT_TEST_SUITE_HANDLE      SimFailTestSuite;

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
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in InitUnitTestFramework. Status = %r\n",
       Status)
      );
    goto EXIT;
  }

  // Populate the Read Unit Test Suite.
  Status = CreateUnitTestSuite (
             &SpinorReadTestSuite,
             Fw,
             "Spinor Read Tests",
             "ErrorSerializationMmDxe.SpinorReadTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for SpinorReadTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  // AddTestCase Args:
  //  Suite | Description
  //  Class Name | Function
  //  Pre | Post | Context
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset 0 size = 0",
    "RW_e0_o0_s0",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_s0
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset 0 size = 1",
    "RW_e0_o0_s1",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_s1
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset 0 size = half",
    "RW_e0_o0_sHalf",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_sHalf
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset 0 size = large",
    "RW_e0_o0_sLarge",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_sLarge
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset 0 size = max",
    "RW_e0_o0_sMax",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_sMax
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset 0 size = too big",
    "RW_e0_o0_sTooBig",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_sTooBig
    );

  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset half size = 0",
    "RW_e0_oHalf_s0",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_s0
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset half size = 1",
    "RW_e0_oHalf_s1",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_s1
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset half size = half",
    "RW_e0_oHalf_sHalf",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_sHalf
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset half size = large",
    "RW_e0_oHalf_sLarge",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_sLarge
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset half size = max",
    "RW_e0_oHalf_sMax",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_sMax
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset half size = too big",
    "RW_e0_oHalf_sTooBig",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_sTooBig
    );

  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset last size = 0",
    "RW_e0_oLast_s0",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_s0
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset last size = 1",
    "RW_e0_oLast_s1",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_s1
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset last size = half",
    "RW_e0_oLast_sHalf",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_sHalf
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset last size = large",
    "RW_e0_oLast_sLarge",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_sLarge
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset last size = max",
    "RW_e0_oLast_sMax",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_sMax
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset 0 offset last size = too big",
    "RW_e0_oLast_sTooBig",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_sTooBig
    );

  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset 0 size = 0",
    "RW_eHalf_o0_s0",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_s0
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset 0 size = 1",
    "RW_eHalf_o0_s1",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_s1
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset 0 size = half",
    "RW_eHalf_o0_sHalf",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_sHalf
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset 0 size = large",
    "RW_eHalf_o0_sLarge",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_sLarge
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset 0 size = max",
    "RW_eHalf_o0_sMax",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_sMax
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset 0 size = too big",
    "RW_eHalf_o0_sTooBig",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_sTooBig
    );

  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset half size = 0",
    "RW_eHalf_oHalf_s0",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_s0
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset half size = 1",
    "RW_eHalf_oHalf_s1",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_s1
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset half size = half",
    "RW_eHalf_oHalf_sHalf",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_sHalf
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset half size = large",
    "RW_eHalf_oHalf_sLarge",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_sLarge
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset half size = max",
    "RW_eHalf_oHalf_sMax",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_sMax
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset half size = too big",
    "RW_eHalf_oHalf_sTooBig",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_sTooBig
    );

  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset last size = 0",
    "RW_eHalf_oLast_s0",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_s0
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset last size = 1",
    "RW_eHalf_oLast_s1",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_s1
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset last size = half",
    "RW_eHalf_oLast_sHalf",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_sHalf
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset last size = large",
    "RW_eHalf_oLast_sLarge",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_sLarge
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset last size = max",
    "RW_eHalf_oLast_sMax",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_sMax
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset half offset last size = too big",
    "RW_eHalf_oLast_sTooBig",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_sTooBig
    );

  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset 0 size = 0",
    "RW_eLast_o0_s0",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_s0
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset 0 size = 1",
    "RW_eLast_o0_s1",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_s1
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset 0 size = half",
    "RW_eLast_o0_sHalf",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_sHalf
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset 0 size = large",
    "RW_eLast_o0_sLarge",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_sLarge
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset 0 size = max",
    "RW_eLast_o0_sMax",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_sMax
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset 0 size = too big",
    "RW_eLast_o0_sTooBig",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_sTooBig
    );

  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset half size = 0",
    "RW_eLast_oHalf_s0",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_s0
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset half size = 1",
    "RW_eLast_oHalf_s1",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_s1
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset half size = half",
    "RW_eLast_oHalf_sHalf",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_sHalf
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset half size = large",
    "RW_eLast_oHalf_sLarge",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_sLarge
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset half size = max",
    "RW_eLast_oHalf_sMax",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_sMax
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset half size = too big",
    "RW_eLast_oHalf_sTooBig",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_sTooBig
    );

  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset end size = 0",
    "RW_eLast_oEnd_s0",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oEnd_s0
    );
  AddTestCase (
    SpinorReadTestSuite,
    "Read Test erst offset last offset end size = 1",
    "RW_eLast_oEnd_s1",
    SpinorReadTest,
    SpinorReadTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oEnd_s1
    );
  // AddTestCase(SpinorReadTestSuite, "Read Test erst offset last offset too big size = 0",
  //            "RW_eLast_oTooBig_s0", SpinorReadTest,
  //            SpinorReadTestSetup, DefaultUnitTestCleanup, &RW_eLast_oTooBig_s0);

  // Populate the Write Unit Test Suite.
  Status = CreateUnitTestSuite (
             &SpinorWriteTestSuite,
             Fw,
             "Spinor Write Tests",
             "ErrorSerializationMmDxe.SpinorWriteTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for SpinorWriteTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  // AddTestCase Args:
  //  Suite | Description
  //  Class Name | Function
  //  Pre | Post | Context
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset 0 size = 0",
    "RW_e0_o0_s0",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_s0
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset 0 size = 1",
    "RW_e0_o0_s1",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_s1
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset 0 size = half",
    "RW_e0_o0_sHalf",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_sHalf
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset 0 size = large",
    "RW_e0_o0_sLarge",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_sLarge
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset 0 size = max",
    "RW_e0_o0_sMax",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_sMax
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset 0 size = too big",
    "RW_e0_o0_sTooBig",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_o0_sTooBig
    );

  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset half size = 0",
    "RW_e0_oHalf_s0",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_s0
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset half size = 1",
    "RW_e0_oHalf_s1",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_s1
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset half size = half",
    "RW_e0_oHalf_sHalf",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_sHalf
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset half size = large",
    "RW_e0_oHalf_sLarge",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_sLarge
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset half size = max",
    "RW_e0_oHalf_sMax",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_sMax
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset half size = too big",
    "RW_e0_oHalf_sTooBig",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oHalf_sTooBig
    );

  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset last size = 0",
    "RW_e0_oLast_s0",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_s0
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset last size = 1",
    "RW_e0_oLast_s1",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_s1
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset last size = half",
    "RW_e0_oLast_sHalf",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_sHalf
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset last size = large",
    "RW_e0_oLast_sLarge",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_sLarge
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset last size = max",
    "RW_e0_oLast_sMax",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_sMax
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset 0 offset last size = too big",
    "RW_e0_oLast_sTooBig",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_e0_oLast_sTooBig
    );

  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset 0 size = 0",
    "RW_eHalf_o0_s0",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_s0
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset 0 size = 1",
    "RW_eHalf_o0_s1",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_s1
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset 0 size = half",
    "RW_eHalf_o0_sHalf",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_sHalf
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset 0 size = large",
    "RW_eHalf_o0_sLarge",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_sLarge
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset 0 size = max",
    "RW_eHalf_o0_sMax",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_sMax
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset 0 size = too big",
    "RW_eHalf_o0_sTooBig",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_o0_sTooBig
    );

  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset half size = 0",
    "RW_eHalf_oHalf_s0",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_s0
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset half size = 1",
    "RW_eHalf_oHalf_s1",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_s1
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset half size = half",
    "RW_eHalf_oHalf_sHalf",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_sHalf
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset half size = large",
    "RW_eHalf_oHalf_sLarge",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_sLarge
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset half size = max",
    "RW_eHalf_oHalf_sMax",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_sMax
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset half size = too big",
    "RW_eHalf_oHalf_sTooBig",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oHalf_sTooBig
    );

  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset last size = 0",
    "RW_eHalf_oLast_s0",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_s0
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset last size = 1",
    "RW_eHalf_oLast_s1",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_s1
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset last size = half",
    "RW_eHalf_oLast_sHalf",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_sHalf
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset last size = large",
    "RW_eHalf_oLast_sLarge",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_sLarge
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset last size = max",
    "RW_eHalf_oLast_sMax",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_sMax
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset half offset last size = too big",
    "RW_eHalf_oLast_sTooBig",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eHalf_oLast_sTooBig
    );

  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset 0 size = 0",
    "RW_eLast_o0_s0",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_s0
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset 0 size = 1",
    "RW_eLast_o0_s1",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_s1
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset 0 size = half",
    "RW_eLast_o0_sHalf",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_sHalf
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset 0 size = large",
    "RW_eLast_o0_sLarge",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_sLarge
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset 0 size = max",
    "RW_eLast_o0_sMax",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_sMax
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset 0 size = too big",
    "RW_eLast_o0_sTooBig",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_o0_sTooBig
    );

  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset half size = 0",
    "RW_eLast_oHalf_s0",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_s0
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset half size = 1",
    "RW_eLast_oHalf_s1",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_s1
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset half size = half",
    "RW_eLast_oHalf_sHalf",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_sHalf
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset half size = large",
    "RW_eLast_oHalf_sLarge",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_sLarge
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset half size = max",
    "RW_eLast_oHalf_sMax",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_sMax
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset half size = too big",
    "RW_eLast_oHalf_sTooBig",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oHalf_sTooBig
    );

  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset end size = 0",
    "RW_eLast_oEnd_s0",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oEnd_s0
    );
  AddTestCase (
    SpinorWriteTestSuite,
    "Write Test erst offset last offset end size = 1",
    "RW_eLast_oEnd_s1",
    SpinorWriteTest,
    SpinorWriteTestSetup,
    DefaultUnitTestCleanup,
    &RW_eLast_oEnd_s1
    );
  // AddTestCase(SpinorWriteTestSuite, "Write Test erst offset last offset too big size = 0",
  //            "RW_eLast_oTooBig_s0", SpinorWriteTest,
  //            SpinorWriteTestSetup, DefaultUnitTestCleanup, &RW_eLast_oTooBig_s0);

  // Populate the Cper Status Unit Test Suite.
  Status = CreateUnitTestSuite (
             &CperStatusTestSuite,
             Fw,
             "Cper Status Tests",
             "ErrorSerializationMmDxe.CperStatusTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for CperStatusTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    CperStatusTestSuite,
    "CperStatus Test erst offset 0 offset 0 status Free",
    "STATUS_e0_o0_sFree",
    WriteCperStatusTest,
    WriteCperStatusTestSetup,
    DefaultUnitTestCleanup,
    &STATUS_e0_o0_sFree
    );
  AddTestCase (
    CperStatusTestSuite,
    "CperStatus Test erst offset 0 offset 1024 status Deleted",
    "STATUS_e0_o1024_sDeleted",
    WriteCperStatusTest,
    WriteCperStatusTestSetup,
    DefaultUnitTestCleanup,
    &STATUS_e0_o1024_sDeleted
    );
  AddTestCase (
    CperStatusTestSuite,
    "CperStatus Test erst offset 0 offset 9000 status Incoming",
    "STATUS_e0_o9000_sIncoming",
    WriteCperStatusTest,
    WriteCperStatusTestSetup,
    DefaultUnitTestCleanup,
    &STATUS_e0_o9000_sIncoming
    );
  AddTestCase (
    CperStatusTestSuite,
    "CperStatus Test erst offset Half offset Block status Invalid",
    "STATUS_eHalf_oBlock_sInvalid",
    WriteCperStatusTest,
    WriteCperStatusTestSetup,
    DefaultUnitTestCleanup,
    &STATUS_eHalf_oBlock_sInvalid
    );
  AddTestCase (
    CperStatusTestSuite,
    "CperStatus Test erst offset Last offset 0 status Outgoing",
    "STATUS_eLast_o0_sOutgoing",
    WriteCperStatusTest,
    WriteCperStatusTestSetup,
    DefaultUnitTestCleanup,
    &STATUS_eLast_o0_sOutgoing
    );
  AddTestCase (
    CperStatusTestSuite,
    "CperStatus Test erst offset Last offset 500 status Valid",
    "STATUS_eLast_o500_sValid",
    WriteCperStatusTest,
    WriteCperStatusTestSetup,
    DefaultUnitTestCleanup,
    &STATUS_eLast_o500_sValid
    );

  AddTestCase (
    CperStatusTestSuite,
    "CperStatusError Test erst offset Last offset 500 status Valid",
    "STATUS_eLast_o500_sValid",
    WriteCperStatusErrorTest,
    WriteCperStatusTestSetup,
    DefaultUnitTestCleanup,
    &STATUS_eLast_o500_sValid
    );

  // Populate the EraseBlock Unit Test Suite.
  Status = CreateUnitTestSuite (
             &EraseBlockTestSuite,
             Fw,
             "Erase Block Tests",
             "ErrorSerializationMmDxe.EraseBlockTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for EraseBlockTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    EraseBlockTestSuite,
    "EraseBlockWhileCollecting Test",
    "EB while collecting",
    EraseBlockWhileCollectingTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  // Populate the InitProtocol Unit Test Suite.
  Status = CreateUnitTestSuite (
             &InitProtocolTestSuite,
             Fw,
             "InitProtocol Tests",
             "ErrorSerializationMmDxe.InitProtocolTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for InitProtocolTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset 0 size 0",
    "IP_e0_s0",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_e0_s0
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset 0 size 1",
    "IP_e0_s1",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_e0_s1
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset 0 size Block",
    "IP_e0_sBlock",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_e0_sBlock
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset 0 size Block2",
    "IP_e0_sBlock2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_e0_sBlock2
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset 0 size Block3",
    "IP_e0_sBlock3",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_e0_sBlock3
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset 0 size Unaligned",
    "IP_e0_sUnaligned",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_e0_sUnaligned
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset 0 size Unaligned2",
    "IP_e0_sUnaligned2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_e0_sUnaligned2
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset 0 size Max",
    "IP_e0_sMax",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_e0_sMax
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset 0 size TooBig",
    "IP_e0_sTooBig",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_e0_sTooBig
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset 0 size TooBig2",
    "IP_e0_sTooBig2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_e0_sTooBig2
    );

  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Block size 0",
    "IP_eBlock_s0",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eBlock_s0
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Block size 1",
    "IP_eBlock_s1",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eBlock_s1
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Block size Block",
    "IP_eBlock_sBlock",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eBlock_sBlock
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Block size Block2",
    "IP_eBlock_sBlock2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eBlock_sBlock2
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Block size Block3",
    "IP_eBlock_sBlock3",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eBlock_sBlock3
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Block size Unaligned",
    "IP_eBlock_sUnaligned",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eBlock_sUnaligned
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Block size Unaligned2",
    "IP_eBlock_sUnaligned2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eBlock_sUnaligned2
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Block size Max",
    "IP_eBlock_sMax",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eBlock_sMax
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Block size TooBig",
    "IP_eBlock_sTooBig",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eBlock_sTooBig
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Block TooBig2",
    "IP_eBlock_sTooBig2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eBlock_sTooBig2
    );

  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Half size 0",
    "IP_eHalf_s0",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eHalf_s0
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Half size 1",
    "IP_eHalf_s1",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eHalf_s1
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Half size Block",
    "IP_eHalf_sBlock",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eHalf_sBlock
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Half size Block2",
    "IP_eHalf_sBlock2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eHalf_sBlock2
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Half size Block3",
    "IP_eHalf_sBlock3",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eHalf_sBlock3
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Half size Unaligned",
    "IP_eHalf_sUnaligned",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eHalf_sUnaligned
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Half size Unaligned2",
    "IP_eHalf_sUnaligned2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eHalf_sUnaligned2
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Half size Max",
    "IP_eHalf_sMax",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eHalf_sMax
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Half size TooBig",
    "IP_eHalf_sTooBig",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eHalf_sTooBig
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Half TooBig2",
    "IP_eHalf_sTooBig2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eHalf_sTooBig2
    );

  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Last size 0",
    "IP_eLast_s0",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eLast_s0
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Last size 1",
    "IP_eLast_s1",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eLast_s1
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Last size Block",
    "IP_eLast_sBlock",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eLast_sBlock
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Last size Block2",
    "IP_eLast_sBlock2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eLast_sBlock2
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Last size Block3",
    "IP_eLast_sBlock3",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eLast_sBlock3
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Last size Unaligned",
    "IP_eLast_sUnaligned",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eLast_sUnaligned
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Last size Unaligned2",
    "IP_eLast_sUnaligned2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eLast_sUnaligned2
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Last size Max",
    "IP_eLast_sMax",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eLast_sMax
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Last size TooBig",
    "IP_eLast_sTooBig",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eLast_sTooBig
    );
  AddTestCase (
    InitProtocolTestSuite,
    "InitProtocol Test erst offset Last TooBig2",
    "IP_eLast_sTooBig2",
    InitProtocolTest,
    InitProtocolTestSetup,
    DefaultUnitTestCleanup,
    &IP_eLast_sTooBig2
    );

  // Populate the EndToEnd Unit Test Suite.
  Status = CreateUnitTestSuite (
             &E2ETestSuite,
             Fw,
             "EndToEnd Tests",
             "ErrorSerializationMmDxe.E2ETestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for E2ETestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleFill erst offset 0 index 0 size 2Block",
    "E2E_e0_i0_s2Block",
    E2ESimpleFillTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRead erst offset 0 index 0 size 2Block",
    "E2E_e0_i0_s2Block",
    E2ESimpleReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashRead erst offset 0 index 0 size 2Block",
    "E2E_e0_i0_s2Block",
    E2EEmptyFlashReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleClear erst offset 0 index 0 size 2Block",
    "E2E_e0_i0_s2Block",
    E2ESimpleClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashClear erst offset 0 index 0 size 2Block",
    "E2E_e0_i0_s2Block",
    E2EEmptyFlashClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleFill erst offset 0 index 0 size 3Block",
    "E2E_e0_i0_s3Block",
    E2ESimpleFillTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRead erst offset 0 index 0 size 3Block",
    "E2E_e0_i0_s3Block",
    E2ESimpleReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashRead erst offset 0 index 0 size 3Block",
    "E2E_e0_i0_s3Block",
    E2EEmptyFlashReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleClear erst offset 0 index 0 size 3Block",
    "E2E_e0_i0_s3Block",
    E2ESimpleClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashClear erst offset 0 index 0 size 3Block",
    "E2E_e0_i0_s3Block",
    E2EEmptyFlashClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleFill erst offset 0 index 0 size Max",
    "E2E_e0_i0_sMax",
    E2ESimpleFillTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRead erst offset 0 index 0 size Max",
    "E2E_e0_i0_sMax",
    E2ESimpleReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashRead erst offset 0 index 0 size Max",
    "E2E_e0_i0_sMax",
    E2EEmptyFlashReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleClear erst offset 0 index 0 size Max",
    "E2E_e0_i0_sMax",
    E2ESimpleClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashClear erst offset 0 index 0 size Max",
    "E2E_e0_i0_sMax",
    E2EEmptyFlashClearTest,

    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleFill erst offset 0 index 1 size 2Block",
    "E2E_e0_i1_s2Block",
    E2ESimpleFillTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRead erst offset 0 index 1 size 2Block",
    "E2E_e0_i1_s2Block",
    E2ESimpleReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashRead erst offset 0 index 1 size 2Block",
    "E2E_e0_i1_s2Block",
    E2EEmptyFlashReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleClear erst offset 0 index 1 size 2Block",
    "E2E_e0_i1_s2Block",
    E2ESimpleClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashClear erst offset 0 index 1 size 2Block",
    "E2E_e0_i1_s2Block",
    E2EEmptyFlashClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleFill erst offset 0 index 1 size 3Block",
    "E2E_e0_i1_s3Block",
    E2ESimpleFillTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRead erst offset 0 index 1 size 3Block",
    "E2E_e0_i1_s3Block",
    E2ESimpleReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashRead erst offset 0 index 1 size 3Block",
    "E2E_e0_i1_s3Block",
    E2EEmptyFlashReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleClear erst offset 0 index 1 size 3Block",
    "E2E_e0_i1_s3Block",
    E2ESimpleClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashClear erst offset 0 index 1 size 3Block",
    "E2E_e0_i1_s3Block",
    E2EEmptyFlashClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleFill erst offset 0 index 1 size Max",
    "E2E_e0_i1_sMax",
    E2ESimpleFillTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRead erst offset 0 index 1 size Max",
    "E2E_e0_i1_sMax",
    E2ESimpleReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashRead erst offset 0 index 1 size Max",
    "E2E_e0_i1_sMax",
    E2EEmptyFlashReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleClear erst offset 0 index 1 size Max",
    "E2E_e0_i1_sMax",
    E2ESimpleClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashClear erst offset 0 index 1 size Max",
    "E2E_e0_i1_sMax",
    E2EEmptyFlashClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleFill erst offset 0 index Half size 2Block",
    "E2E_e0_iHalf_s2Block",
    E2ESimpleFillTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRead erst offset 0 index Half size 2Block",
    "E2E_e0_iHalf_s2Block",
    E2ESimpleReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashRead erst offset 0 index Half size 2Block",
    "E2E_e0_iHalf_s2Block",
    E2EEmptyFlashReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleClear erst offset 0 index Half size 2Block",
    "E2E_e0_iHalf_s2Block",
    E2ESimpleClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashClear erst offset 0 index Half size 2Block",
    "E2E_e0_iHalf_s2Block",
    E2EEmptyFlashClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleFill erst offset 0 index Half size 3Block",
    "E2E_e0_iHalf_s3Block",
    E2ESimpleFillTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRead erst offset 0 index Half size 3Block",
    "E2E_e0_iHalf_s3Block",
    E2ESimpleReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashRead erst offset 0 index Half size 3Block",
    "E2E_e0_iHalf_s3Block",
    E2EEmptyFlashReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleClear erst offset 0 index Half size 3Block",
    "E2E_e0_iHalf_s3Block",
    E2ESimpleClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashClear erst offset 0 index Half size 3Block",
    "E2E_e0_iHalf_s3Block",
    E2EEmptyFlashClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleFill erst offset 0 index Half size Max",
    "E2E_e0_iHalf_sMax",
    E2ESimpleFillTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRead erst offset 0 index Half size Max",
    "E2E_e0_iHalf_sMax",
    E2ESimpleReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashRead erst offset 0 index Half size Max",
    "E2E_e0_iHalf_sMax",
    E2EEmptyFlashReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleClear erst offset 0 index Half size Max",
    "E2E_e0_iHalf_sMax",
    E2ESimpleClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E EmptyFlashClear erst offset 0 index Half size Max",
    "E2E_e0_iHalf_sMax",
    E2EEmptyFlashClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleBoot erst offset 0 index 0 size 2Block",
    "E2E_e0_i0_s2Block",
    E2ESimpleBootTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleBoot erst offset 0 index 0 size 3Block",
    "E2E_e0_i0_s3Block",
    E2ESimpleBootTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleBoot erst offset 0 index 0 size Max",
    "E2E_e0_i0_sMax",
    E2ESimpleBootTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleBoot erst offset 0 index 1 size 2Block",
    "E2E_e0_i1_s2Block",
    E2ESimpleBootTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleBoot erst offset 0 index 1 size 3Block",
    "E2E_e0_i1_s3Block",
    E2ESimpleBootTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleBoot erst offset 0 index 1 size Max",
    "E2E_e0_i1_sMax",
    E2ESimpleBootTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleBoot erst offset 0 index Half size 2Block",
    "E2E_e0_iHalf_s2Block",
    E2ESimpleBootTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleBoot erst offset 0 index Half size 3Block",
    "E2E_e0_iHalf_s3Block",
    E2ESimpleBootTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleBoot erst offset 0 index Half size Max",
    "E2E_e0_iHalf_sMax",
    E2ESimpleBootTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRecoveryRead erst offset 0 index 0 size 2Block",
    "E2E_e0_i0_s2Block",
    E2ESimpleRecoveryReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRecoveryRead erst offset 0 index 0 size 3Block",
    "E2E_e0_i0_s3Block",
    E2ESimpleRecoveryReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRecoveryRead erst offset 0 index 0 size Max",
    "E2E_e0_i0_sMax",
    E2ESimpleRecoveryReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRecoveryRead erst offset 0 index 1 size 2Block",
    "E2E_e0_i1_s2Block",
    E2ESimpleRecoveryReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRecoveryRead erst offset 0 index 1 size 3Block",
    "E2E_e0_i1_s3Block",
    E2ESimpleRecoveryReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRecoveryRead erst offset 0 index 1 size Max",
    "E2E_e0_i1_sMax",
    E2ESimpleRecoveryReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRecoveryRead erst offset 0 index Half size 2Block",
    "E2E_e0_iHalf_s2Block",
    E2ESimpleRecoveryReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRecoveryRead erst offset 0 index Half size 3Block",
    "E2E_e0_iHalf_s3Block",
    E2ESimpleRecoveryReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E SimpleRecoveryRead erst offset 0 index Half size Max",
    "E2E_e0_iHalf_sMax",
    E2ESimpleRecoveryReadTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E WriteReadClear erst offset 0 index 0 size 2Block",
    "E2E_e0_i0_s2Block",
    E2EWriteReadClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E WriteReadClear erst offset 0 index 0 size 3Block",
    "E2E_e0_i0_s3Block",
    E2EWriteReadClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E WriteReadClear erst offset 0 index 0 size Max",
    "E2E_e0_i0_sMax",
    E2EWriteReadClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E WriteReadClear erst offset 0 index 1 size 2Block",
    "E2E_e0_i1_s2Block",
    E2EWriteReadClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E WriteReadClear erst offset 0 index 1 size 3Block",
    "E2E_e0_i1_s3Block",
    E2EWriteReadClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E WriteReadClear erst offset 0 index 1 size Max",
    "E2E_e0_i1_sMax",
    E2EWriteReadClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i1_sMax
    );

  AddTestCase (
    E2ETestSuite,
    "E2E WriteReadClear erst offset 0 index Half size 2Block",
    "E2E_e0_iHalf_s2Block",
    E2EWriteReadClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s2Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E WriteReadClear erst offset 0 index Half size 3Block",
    "E2E_e0_iHalf_s3Block",
    E2EWriteReadClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_s3Block
    );

  AddTestCase (
    E2ETestSuite,
    "E2E WriteReadClear erst offset 0 index Half size Max",
    "E2E_e0_iHalf_sMax",
    E2EWriteReadClearTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_iHalf_sMax
    );

  // Populate the ValidateRecord Unit Test Suite.
  Status = CreateUnitTestSuite (
             &ValidateRecordTestSuite,
             Fw,
             "ValidateRecord Tests",
             "ErrorSerializationMmDxe.ValidateRecordTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for ValidateRecordTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    ValidateRecordTestSuite,
    "ValidateRecord erst offset 0 offset 0 s2Block",
    "E2E_e0_i0_s2Block",
    ValidateRecordTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  // Populate the InvalidInput Unit Test Suite.
  Status = CreateUnitTestSuite (
             &InvalidInputTestSuite,
             Fw,
             "InvalidInput Tests",
             "ErrorSerializationMmDxe.InvalidInputTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for InvalidInputTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    InvalidInputTestSuite,
    "InvalidInput erst offset 0 offset 0 s2Block",
    "E2E_e0_i0_s2Block",
    InvalidInputTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  // Populate the FaultyFlash Unit Test Suite.
  Status = CreateUnitTestSuite (
             &FaultyFlashTestSuite,
             Fw,
             "FaultyFlash Tests",
             "ErrorSerializationMmDxe.FaultyFlashdTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for FaultyFlashTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    FaultyFlashTestSuite,
    "FaultyFlash erst offset 0 offset 0 s2Block",
    "E2E_e0_i0_s2Block",
    FaultyFlashTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  // Populate the Reclaim Unit Test Suite.
  Status = CreateUnitTestSuite (
             &ReclaimTestSuite,
             Fw,
             "Reclaim Tests",
             "ErrorSerializationMmDxe.ReclaimTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for ReclaimTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    ReclaimTestSuite,
    "Reclaim erst offset 0 offset 0 s2Block",
    "E2E_e0_i0_s2Block",
    ReclaimTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  // Populate the IncomingOutgoingInvalid Unit Test Suite.
  Status = CreateUnitTestSuite (
             &IncomingOutgoingInvalidTestSuite,
             Fw,
             "IncomingOutgoingInvalid Tests",
             "ErrorSerializationMmDxe.IncomingOutgoingInvalidTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for IncomingOutgoingInvalidSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    IncomingOutgoingInvalidTestSuite,
    "IncomingOutgoingInvalid erst offset 0 offset 0 s2Block",
    "E2E_e0_i0_s2Block",
    IncomingOutgoingInvalidTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_s2Block
    );

  // Populate the SimFail Unit Test Suite.
  Status = CreateUnitTestSuite (
             &SimFailTestSuite,
             Fw,
             "SimFail Tests",
             "ErrorSerializationMmDxe.SimFailTestSuite",
             NULL,
             NULL
             );
  if (Status != EFI_SUCCESS) {
    DEBUG (
      (DEBUG_ERROR,
       "Failed in CreateUnitTestSuite for SimFailTestSuite\n")
      );
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (
    SimFailTestSuite,
    "SimFailTest erst offset 0 offset 0 sMax",
    "E2E_e0_i0_sMax",
    SimFailTest,
    E2EEmptyFlashSetup,
    DefaultUnitTestCleanup,
    &E2E_e0_i0_sMax
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
  DEBUG ((DEBUG_INFO, "Called BaseLibUnitTestAppEntry\n"));
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
  DEBUG ((DEBUG_INFO, "Called main\n"));
  return UnitTestingEntry ();
}
