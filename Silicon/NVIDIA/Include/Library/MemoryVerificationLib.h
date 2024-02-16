/** @file

  MemoryVerificationLib

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef MEMORY_VERIFICATION_LIB_H__
#define MEMORY_VERIFICATION_LIB_H__

#include <Uefi/UefiBaseType.h>

typedef enum {
  MemoryTestWalking1Bit,
  MemoryTestAddressCheck,
  MemoryTestMovingInversions01,
  MemoryTestMovingInversions8Bit,
  MemoryTestMovingInversionsRandom,
  // MemoryTestBlockMode,
  MemoryTestMovingInversions64Bit,
  MemoryTestRandomNumberSequence,
  MemoryTestModulo20Random,
  MemoryTestBitFadeTest,
  MemoryTestMaxTest
} MEMORY_TEST_MODE;

/**
 * @brief Return cache line length of system.
 *
 * @return         Cache line length
 */
UINTN
EFIAPI
MemoryVerificationGetCacheLineLength (
  VOID
  );

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
  );

#endif
