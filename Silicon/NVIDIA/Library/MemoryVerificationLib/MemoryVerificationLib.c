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
#include <Library/TimerLib.h>
#include <Library/NVIDIADebugLib.h>

#define MEMORY_TEST_MODULO  20

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
 * @brief Runs the Walking 1 Bit memory test over the specified memory
 *
 * @param[in]  Pattern             Pattern to check
 * @param[in]  WaitTime            Microseconds to wait between write and verification
 * @param[in]  TestAddress         Base address to start testing at
 * @param[in]  TestLength          Length of memory to test
 * @param[in]  TestSpan            Span between memory tests
 * @param[out] FailedMemoryAddress Memory address where failure occured, optional
 *
 * @retval EFI_SUCCESS        No errors detected
 * @retval EFI_DEVICE_ERROR   Memory device error occurred
 */
EFI_STATUS
EFIAPI
MemoryVerificationWalking1TestRegion (
  IN EFI_PHYSICAL_ADDRESS   TestAddress,
  IN UINTN                  TestLength,
  OUT EFI_PHYSICAL_ADDRESS  *FailedMemoryAddress OPTIONAL
  )
{
  UINT64   *TestValue1;
  UINT64   *TestValue2;
  UINT8    Pass;
  BOOLEAN  Invert = FALSE;
  UINT64   AddressMask1;
  UINT64   AddressMask2;
  UINT64   ExpectedValue;

  for (Pass = 0; Pass < 2; Pass++) {
    AddressMask1 = sizeof (UINT64);
    TestValue1   = (UINT64 *)(TestAddress | AddressMask1);
    while (TestValue1 < (UINT64 *)(TestAddress + TestLength)) {
      if (Invert) {
        ExpectedValue = ~(UINT64)TestValue1;
        *TestValue1   = ExpectedValue;
      } else {
        ExpectedValue = (UINT64)TestValue1;
        *TestValue1   = ExpectedValue;
      }

      AddressMask2 = sizeof (UINT64);
      TestValue2   = (UINT64 *)(TestAddress | AddressMask2);
      while (TestValue1 < (UINT64 *)(TestAddress + TestLength)) {
        if (TestValue1 != TestValue2) {
          if (Invert) {
            *TestValue2 = ~(UINT64)TestValue2;
          } else {
            *TestValue2 = (UINT64)TestValue2;
          }
        }

        if (*TestValue1 != (UINT64)ExpectedValue) {
          if (FailedMemoryAddress != NULL) {
            *FailedMemoryAddress = (UINTN)TestValue2;
          }

          return EFI_DEVICE_ERROR;
        }

        AddressMask2 <<= 1;
        TestValue2     = (UINT64 *)(TestAddress | AddressMask2);
      }

      AddressMask1 <<= 1;
      TestValue1     = (UINT64 *)(TestAddress | AddressMask1);
    }

    Invert = !Invert;
  }

  return EFI_SUCCESS;
}

/**
 * @brief Runs the Moving Inversions memory test over the specified memory
 *
 * @param[in]  Pattern             Pattern to check
 * @param[in]  RotatePattern       TRUE if pattern should be rotated
 * @param[in]  TestAddress         Base address to start testing at
 * @param[in]  TestLength          Length of memory to test
 * @param[in]  TestSpan            Span between memory tests
 * @param[out] FailedMemoryAddress Memory address where failure occured, optional
 *
 * @retval EFI_SUCCESS        No errors detected
 * @retval EFI_DEVICE_ERROR   Memory device error occurred
 */
EFI_STATUS
EFIAPI
MemoryVerificationMovingInversionsRegion (
  IN UINT64                 Pattern,
  IN BOOLEAN                RotatePattern,
  IN EFI_PHYSICAL_ADDRESS   TestAddress,
  IN UINTN                  TestLength,
  IN UINTN                  TestSpan,
  OUT EFI_PHYSICAL_ADDRESS  *FailedMemoryAddress OPTIONAL
  )
{
  UINTN   Location;
  UINT64  CurrentPattern;
  UINTN   Length;
  UINT64  *TestValue;
  UINTN   CacheLineLength = MemoryVerificationGetCacheLineLength ();

  // Fill out the initial memory
  CurrentPattern = Pattern;
  Location       = 0;
  while (Location < TestLength) {
    Length    = MIN (CacheLineLength, TestLength - Location);
    TestValue = (UINT64 *)(TestAddress + Location);
    while (Length != 0) {
      *TestValue = CurrentPattern;
      if (RotatePattern) {
        CurrentPattern = (CurrentPattern << 1) | (CurrentPattern >> 63);
      }

      Length -= sizeof (UINT64);
      TestValue++;
    }

    Location += TestSpan;
  }

  WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)TestAddress, (UINTN)TestLength);

  // Invert from bottom
  CurrentPattern = Pattern;
  Location       = 0;
  while (Location < TestLength) {
    Length    = MIN (CacheLineLength, TestLength - Location);
    TestValue = (UINT64 *)(TestAddress + Location);
    while (Length != 0) {
      if (*TestValue != CurrentPattern) {
        if (FailedMemoryAddress != NULL) {
          *FailedMemoryAddress = (UINTN)TestValue;
        }

        return EFI_DEVICE_ERROR;
      }

      *TestValue = ~CurrentPattern;
      if (RotatePattern) {
        CurrentPattern = (CurrentPattern << 1) | (CurrentPattern >> 63);
      }

      Length -= sizeof (UINT64);
      TestValue++;
    }

    Location += TestSpan;
  }

  WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)TestAddress, (UINTN)TestLength);

  // Invert from top
  // Current Pattern is rotated one past the last value
  Location -= TestSpan;
  do {
    Length    = MIN (CacheLineLength, TestLength - Location);
    TestValue = (UINT64 *)(TestAddress + Location + Length - sizeof (UINT64));
    while (Length != 0) {
      if (RotatePattern) {
        CurrentPattern = (CurrentPattern >> 1) | (CurrentPattern << 63);
      }

      if (*TestValue != ~CurrentPattern) {
        if (FailedMemoryAddress != NULL) {
          *FailedMemoryAddress = (UINTN)TestValue;
        }

        return EFI_DEVICE_ERROR;
      }

      *TestValue = CurrentPattern;

      Length -= sizeof (UINT64);
      TestValue--;
    }

    if (Location == 0) {
      break;
    }

    Location -= TestSpan;
  } while (TRUE);

  return EFI_SUCCESS;
}

/**
 * @brief XORShift64 PRNG
 *
 * @param[in]  Seed                Random Seed
 *
 * @returns Random value
 */
UINT64
EFIAPI
XorShift64 (
  IN UINT64  Seed
  )
{
  UINT64  Value = Seed;

  Value ^= Value << 13;
  Value ^= Value >> 7;
  Value ^= Value << 17;
  return Value;
}

/**
 * @brief Runs the Random Number Sequence memory test over the specified memory
 *
 * @param[in]  Seed                Random Seed
 * @param[in]  TestAddress         Base address to start testing at
 * @param[in]  TestLength          Length of memory to test
 * @param[in]  TestSpan            Span between memory tests
 * @param[out] FailedMemoryAddress Memory address where failure occured, optional
 *
 * @retval EFI_SUCCESS        No errors detected
 * @retval EFI_DEVICE_ERROR   Memory device error occurred
 */
EFI_STATUS
EFIAPI
MemoryVerificationRandomSequenceRegion (
  IN UINT64                 Seed,
  IN EFI_PHYSICAL_ADDRESS   TestAddress,
  IN UINTN                  TestLength,
  IN UINTN                  TestSpan,
  OUT EFI_PHYSICAL_ADDRESS  *FailedMemoryAddress OPTIONAL
  )
{
  UINTN    Location;
  UINT64   CurrentPattern;
  UINTN    Length;
  UINT64   *TestValue;
  UINTN    CacheLineLength = MemoryVerificationGetCacheLineLength ();
  UINTN    Pass;
  BOOLEAN  Invert = FALSE;
  UINT64   Expected;

  // Fill out the initial memory
  CurrentPattern = XorShift64 (Seed);
  Location       = 0;
  while (Location < TestLength) {
    Length    = MIN (CacheLineLength, TestLength - Location);
    TestValue = (UINT64 *)(TestAddress + Location);
    while (Length != 0) {
      *TestValue     = CurrentPattern;
      CurrentPattern = XorShift64 (CurrentPattern);

      Length -= sizeof (UINT64);
      TestValue++;
    }

    Location += TestSpan;
  }

  WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)TestAddress, (UINTN)TestLength);

  // Check and Invert
  for (Pass = 0; Pass < 2; Pass++) {
    CurrentPattern = XorShift64 (Seed);
    Location       = 0;
    while (Location < TestLength) {
      Length    = MIN (CacheLineLength, TestLength - Location);
      TestValue = (UINT64 *)(TestAddress + Location);
      while (Length != 0) {
        if (Invert) {
          Expected = ~CurrentPattern;
        } else {
          Expected = CurrentPattern;
        }

        if (*TestValue != Expected) {
          if (FailedMemoryAddress != NULL) {
            *FailedMemoryAddress = (UINTN)TestValue;
          }

          return EFI_DEVICE_ERROR;
        }

        *TestValue     = ~Expected;
        CurrentPattern = XorShift64 (CurrentPattern);

        Length -= sizeof (UINT64);
        TestValue++;
      }

      Location += TestSpan;
    }

    WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)TestAddress, (UINTN)TestLength);
    Invert = !Invert;
  }

  return EFI_SUCCESS;
}

/**
 * @brief Runs the Modulo 20, Random memory test over the specified memory
 *
 * @param[in]  Seed                Random Seed
 * @param[in]  TestAddress         Base address to start testing at
 * @param[in]  TestLength          Length of memory to test
 * @param[out] FailedMemoryAddress Memory address where failure occured, optional
 *
 * @retval EFI_SUCCESS        No errors detected
 * @retval EFI_DEVICE_ERROR   Memory device error occurred
 */
EFI_STATUS
EFIAPI
MemoryVerificationModulo20RandomRegion (
  IN UINT64                 Seed,
  IN EFI_PHYSICAL_ADDRESS   TestAddress,
  IN UINTN                  TestLength,
  OUT EFI_PHYSICAL_ADDRESS  *FailedMemoryAddress OPTIONAL
  )
{
  UINT64  CurrentPattern;
  UINT64  InvertPattern;
  UINT64  *TestValue;
  UINT64  *EndPtr;
  UINTN   Offset;
  UINTN   Index;

  CurrentPattern = Seed;

  EndPtr = (UINT64 *)(TestAddress + TestLength);
  for (Offset = 0; Offset < MEMORY_TEST_MODULO; Offset++) {
    // Fill out the initial memory
    CurrentPattern = XorShift64 (CurrentPattern);
    InvertPattern  = ~CurrentPattern;
    TestValue      = (UINT64 *)(TestAddress) + Offset;
    while ((TestValue + MEMORY_TEST_MODULO) < EndPtr) {
      TestValue[MEMORY_TEST_MODULO - 1] = CurrentPattern;
      TestValue                        += MEMORY_TEST_MODULO;
    }

    TestValue = (UINT64 *)(TestAddress) + Offset;
    while ((TestValue + MEMORY_TEST_MODULO) < EndPtr) {
      for (Index = 0; Index < (MEMORY_TEST_MODULO - 1); Index++) {
        TestValue[Index] = InvertPattern;
      }

      TestValue += MEMORY_TEST_MODULO;
    }

    WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)TestAddress, (UINTN)TestLength);

    TestValue = (UINT64 *)(TestAddress) + Offset;
    while ((TestValue + MEMORY_TEST_MODULO) < EndPtr) {
      if (TestValue[MEMORY_TEST_MODULO - 1] != CurrentPattern) {
        if (FailedMemoryAddress != NULL) {
          *FailedMemoryAddress = (UINTN)&TestValue[MEMORY_TEST_MODULO - 1];
        }

        return EFI_DEVICE_ERROR;
      }

      TestValue += MEMORY_TEST_MODULO;
    }
  }

  return EFI_SUCCESS;
}

/**
 * @brief Runs the bit fade memory test over the specified memory
 *
 * @param[in]  Pattern             Pattern to check
 * @param[in]  WaitTime            Microseconds to wait between write and verification
 * @param[in]  TestAddress         Base address to start testing at
 * @param[in]  TestLength          Length of memory to test
 * @param[in]  TestSpan            Span between memory tests
 * @param[out] FailedMemoryAddress Memory address where failure occured, optional
 *
 * @retval EFI_SUCCESS        No errors detected
 * @retval EFI_DEVICE_ERROR   Memory device error occurred
 */
EFI_STATUS
EFIAPI
MemoryVerificationBitFadeTestRegion (
  UINT64                    Pattern,
  UINT64                    WaitTime,
  IN EFI_PHYSICAL_ADDRESS   TestAddress,
  IN UINTN                  TestLength,
  IN UINTN                  TestSpan,
  OUT EFI_PHYSICAL_ADDRESS  *FailedMemoryAddress OPTIONAL
  )
{
  UINTN   Location;
  UINTN   Length;
  UINT64  *TestValue;
  UINTN   CacheLineLength = MemoryVerificationGetCacheLineLength ();

  Location = 0;
  while (Location < TestLength) {
    Length    = MIN (CacheLineLength, TestLength - Location);
    TestValue = (UINT64 *)(TestAddress + Location);
    while (Length != 0) {
      *TestValue = Pattern;
      Length    -= sizeof (UINT64);
      TestValue++;
    }

    Location += TestSpan;
  }

  WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)TestAddress, (UINTN)TestLength);
  if (WaitTime != 0) {
    MicroSecondDelay (WaitTime*1000);
  }

  Location = 0;
  while (Location < TestLength) {
    Length    = MIN (CacheLineLength, TestLength - Location);
    TestValue = (UINT64 *)(TestAddress + Location);
    while (Length != 0) {
      if (*TestValue != Pattern) {
        if (FailedMemoryAddress != NULL) {
          *FailedMemoryAddress = (UINTN)TestValue;
        }

        return EFI_DEVICE_ERROR;
      }

      Length -= sizeof (UINT64);
      TestValue++;
    }

    Location += TestSpan;
  }

  return EFI_SUCCESS;
}

/**
 * @brief Runs the Address check memory test over the specified memory
 *
 * @param[in]  Pattern             Pattern to check
 * @param[in]  WaitTime            Microseconds to wait between write and verification
 * @param[in]  TestAddress         Base address to start testing at
 * @param[in]  TestLength          Length of memory to test
 * @param[in]  TestSpan            Span between memory tests
 * @param[out] FailedMemoryAddress Memory address where failure occured, optional
 *
 * @retval EFI_SUCCESS        No errors detected
 * @retval EFI_DEVICE_ERROR   Memory device error occurred
 */
EFI_STATUS
EFIAPI
MemoryVerificationAddressCheckTestRegion (
  IN EFI_PHYSICAL_ADDRESS   TestAddress,
  IN UINTN                  TestLength,
  IN UINTN                  TestSpan,
  OUT EFI_PHYSICAL_ADDRESS  *FailedMemoryAddress OPTIONAL
  )
{
  UINTN   Location;
  UINTN   Length;
  UINT64  *TestValue;
  UINTN   CacheLineLength = MemoryVerificationGetCacheLineLength ();

  Location = 0;
  while (Location < TestLength) {
    Length    = MIN (CacheLineLength, TestLength - Location);
    TestValue = (UINT64 *)(TestAddress + Location);
    while (Length != 0) {
      *TestValue = (UINT64)TestValue;
      Length    -= sizeof (UINT64);
      TestValue++;
    }

    Location += TestSpan;
  }

  WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)TestAddress, (UINTN)TestLength);

  Location = 0;
  while (Location < TestLength) {
    Length    = MIN (CacheLineLength, TestLength - Location);
    TestValue = (UINT64 *)(TestAddress + Location);
    while (Length != 0) {
      if (*TestValue != (UINT64)TestValue) {
        if (FailedMemoryAddress != NULL) {
          *FailedMemoryAddress = (UINTN)TestValue;
        }

        return EFI_DEVICE_ERROR;
      }

      Length -= sizeof (UINT64);
      TestValue++;
    }

    Location += TestSpan;
  }

  return EFI_SUCCESS;
}

/**
 * @brief Runs the memory test over the specified memory
 *
 * @param[in]  TestMode            Which memory test mode to use
 * @param[in]  TestParameter1      Mode specific test parameter
 * @param[in]  TestParameter2      Mode specific test parameter
 * @param[in]  TestAddress         Base address to start testing at
 * @param[in]  TestLength          Length of memory to test
 * @param[in]  TestSpan            Span between memory tests
 * @param[out] FailedMemoryAddress Memory address where failure occured, optional
 *
 * @retval EFI_SUCCESS        No errors detected
 * @retval EFI_DEVICE_ERROR   Memory device error occurred
 */
EFI_STATUS
EFIAPI
MemoryVerificationTestRegion (
  MEMORY_TEST_MODE          TestMode,
  UINT64                    TestParameter1,
  UINT64                    TestParameter2,
  IN EFI_PHYSICAL_ADDRESS   TestAddress,
  IN UINTN                  TestLength,
  IN UINTN                  TestSpan,
  OUT EFI_PHYSICAL_ADDRESS  *FailedMemoryAddress OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINTN       CacheLineLength = MemoryVerificationGetCacheLineLength ();
  UINTN       Pass;

  if (TestSpan < CacheLineLength) {
    TestSpan = CacheLineLength;
  }

  TestLength = (TestLength / sizeof (UINT64)) * sizeof (UINT64);

  switch (TestMode) {
    case MemoryTestWalking1Bit:
      return MemoryVerificationWalking1TestRegion (
               TestAddress,
               TestLength,
               FailedMemoryAddress
               );
    case MemoryTestAddressCheck:
      return MemoryVerificationAddressCheckTestRegion (
               TestAddress,
               TestLength,
               TestSpan,
               FailedMemoryAddress
               );
    case MemoryTestMovingInversions01:
    case MemoryTestMovingInversions8Bit:
    case MemoryTestMovingInversionsRandom:
      return MemoryVerificationMovingInversionsRegion (
               TestParameter1,
               FALSE,
               TestAddress,
               TestLength,
               TestSpan,
               FailedMemoryAddress
               );
    case MemoryTestMovingInversions64Bit:
      for (Pass = 0; Pass < 64; Pass++) {
        Status = MemoryVerificationMovingInversionsRegion (
                   1ULL << Pass,
                   TRUE,
                   TestAddress,
                   TestLength,
                   TestSpan,
                   FailedMemoryAddress
                   );
        if (EFI_ERROR (Status)) {
          return Status;
        }
      }

      return EFI_SUCCESS;
    case MemoryTestRandomNumberSequence:
      return MemoryVerificationRandomSequenceRegion (
               TestParameter1,
               TestAddress,
               TestLength,
               TestSpan,
               FailedMemoryAddress
               );

    case MemoryTestModulo20Random:
      return MemoryVerificationModulo20RandomRegion (
               TestParameter1,
               TestAddress,
               TestLength,
               FailedMemoryAddress
               );

    case MemoryTestBitFadeTest:
      return MemoryVerificationBitFadeTestRegion (
               TestParameter1,
               TestParameter2,
               TestAddress,
               TestLength,
               TestSpan,
               FailedMemoryAddress
               );
    default:
      return EFI_UNSUPPORTED;
  }
}
