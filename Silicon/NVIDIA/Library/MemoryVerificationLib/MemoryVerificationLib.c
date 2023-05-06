/** @file

  MemoryVerificationLib

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryVerificationLib.h>

// Array of test patterns of size of the data cache length
VOID   **mTestPatterns       = NULL;
UINTN  mNumberOfTestPatterns = 0;

/**
  Compares the contents of two buffers.

  This function compares Length bytes of SourceBuffer to Length bytes of DestinationBuffer.
  If all Length bytes of the two buffers are identical, then 0 is returned.  Otherwise, the
  value returned is the first mismatched byte in SourceBuffer subtracted from the first
  mismatched byte in DestinationBuffer.

  @param[in] DestinationBuffer The pointer to the destination buffer to compare.
  @param[in] SourceBuffer      The pointer to the source buffer to compare.
  @param[in] Length            The number of bytes to compare.

  @retval TRUE                 Buffers Match
  @retval FALSE                Buffers do not match

**/
BOOLEAN
EFIAPI
CompareMemWithoutCheckArgument (
  IN      CONST VOID  *DestinationBuffer,
  IN      CONST VOID  *SourceBuffer,
  IN      UINTN       Length
  )
{
  while (Length != 0) {
    if ((((UINTN)DestinationBuffer & 0x7) == 0) &&
        (((UINTN)SourceBuffer & 0x7) == 0))
    {
      while (Length >= sizeof (UINT64)) {
        if (*(UINT64 *)DestinationBuffer != *(UINT64 *)SourceBuffer) {
          break;
        }

        DestinationBuffer += sizeof (UINT64);
        SourceBuffer      += sizeof (UINT64);
        Length            -= sizeof (UINT64);
      }
    }

    if ((((UINTN)DestinationBuffer & 0x3) == 0) &&
        (((UINTN)SourceBuffer & 0x3) == 0))
    {
      while (Length >= sizeof (UINT32)) {
        if (*(UINT32 *)DestinationBuffer != *(UINT32 *)SourceBuffer) {
          break;
        }

        DestinationBuffer += sizeof (UINT32);
        SourceBuffer      += sizeof (UINT32);
        Length            -= sizeof (UINT32);
      }
    }

    if ((((UINTN)DestinationBuffer & 0x1) == 0) &&
        (((UINTN)SourceBuffer & 0x1) == 0))
    {
      while (Length >= sizeof (UINT16)) {
        if (*(UINT16 *)DestinationBuffer != *(UINT16 *)SourceBuffer) {
          break;
        }

        DestinationBuffer += sizeof (UINT16);
        SourceBuffer      += sizeof (UINT16);
        Length            -= sizeof (UINT16);
      }
    }

    if (Length >= sizeof (UINT8)) {
      if (*(UINT8 *)DestinationBuffer != *(UINT8 *)SourceBuffer) {
        break;
      }

      DestinationBuffer += sizeof (UINT8);
      SourceBuffer      += sizeof (UINT8);
      Length            -= sizeof (UINT8);
    }
  }

  return (Length == 0);
}

/**
 * @brief Return cache line length of system.
 *
 * @return         Cache line length
 */
UINTN
EFIAPI
MemoryVerificationGetCacheLineLength (
  VOID
  )
{
  return ArmDataCacheLineLength ();
}

/**
 * @brief Sets the memory test pattern, will be repeated and copied to a buffer
 * of cache line length
 *
 * @param[in] TestPattern    Test pattern to use
 * @param[in] TestPatterSize size of the test pattern buffer
 *
 * @retval EFI_SUCCESS Test pattern initialized
 * @retval others error
 */
EFI_STATUS
EFIAPI
MemoryVerificationAddPattern (
  IN VOID   *TestPattern,
  IN UINTN  TestPatternSize
  )
{
  VOID   **NewTestArray;
  VOID   *TestData;
  UINTN  Location;
  UINTN  CacheLineLength = MemoryVerificationGetCacheLineLength ();

  if ((TestPattern == NULL)  ||
      (TestPatternSize == 0) ||
      (TestPatternSize > CacheLineLength))
  {
    return EFI_INVALID_PARAMETER;
  }

  // Allocate a new entry in pointer array
  NewTestArray = ReallocatePool (
                   mNumberOfTestPatterns * sizeof (VOID *),
                   (mNumberOfTestPatterns+1) * sizeof (VOID *),
                   mTestPatterns
                   );
  if (NewTestArray == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Allocate test data
  TestData = AllocatePool (CacheLineLength);
  if (TestData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Location = 0;
  while (Location < CacheLineLength) {
    CopyMem (TestData + Location, TestPattern, MIN (TestPatternSize, CacheLineLength));
    Location += TestPatternSize;
  }

  NewTestArray[mNumberOfTestPatterns] = TestData;
  mTestPatterns                       = NewTestArray;
  mNumberOfTestPatterns++;
  return EFI_SUCCESS;
}

/**
 * @brief Runs the memory test over the specified memory
 *
 * @param[in]  TestAddress         Base address to start testing at
 * @param[in]  TestLength          Length of memory to test
 * @param[in]  TestSpan            Span between memory tests
 * @param[out] FailedMemoryAddress Memory address where failure occured, optional
 *
 * @retval EFI_SUCCESS        No errors detected
 * @retval EFI_DEVICE_ERROR   Memory device error occurred *
 */
EFI_STATUS
EFIAPI
MemoryVerificationTestRegion (
  IN EFI_PHYSICAL_ADDRESS   TestAddress,
  IN UINTN                  TestLength,
  IN UINTN                  TestSpan,
  OUT EFI_PHYSICAL_ADDRESS  *FailedMemoryAddress OPTIONAL
  )
{
  UINTN  TestPattern;
  UINTN  Location;
  UINTN  Length;
  UINTN  CacheLineLength = MemoryVerificationGetCacheLineLength ();

  if (TestSpan < CacheLineLength) {
    TestSpan = CacheLineLength;
  }

  for (TestPattern = 0; TestPattern < mNumberOfTestPatterns; TestPattern++) {
    Location = 0;
    while (Location < TestLength) {
      Length = MIN (CacheLineLength, TestLength - Location);
      CopyMem ((VOID *)(TestAddress + Location), mTestPatterns[TestPattern], Length);
      Location += TestSpan;
    }

    WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)TestAddress, (UINTN)TestLength);

    Location = 0;
    while (Location < TestLength) {
      Length = MIN (CacheLineLength, TestLength - Location);
      if (!CompareMemWithoutCheckArgument ((VOID *)(TestAddress + Location), mTestPatterns[TestPattern], Length)) {
        if (FailedMemoryAddress != NULL) {
          *FailedMemoryAddress = TestAddress + Location;
          return EFI_DEVICE_ERROR;
        }
      }

      Location += TestSpan;
    }
  }

  return EFI_SUCCESS;
}
