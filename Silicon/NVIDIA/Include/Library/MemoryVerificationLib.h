/** @file

  MemoryVerificationLib

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef MEMORY_VERIFICATION_LIB_H__
#define MEMORY_VERIFICATION_LIB_H__

#include <Uefi/UefiBaseType.h>

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
 * @brief Sets the memory test pattern, will be repeated and copied to a buffer
 * of cache line length
 *
 * @param[in] TestPattern    Test pattern to use
 * @param[in] TestPatterSize Size of the test pattern buffer
 *
 * @retval EFI_SUCCESS Test pattern initialized
 * @retval others error
 */
EFI_STATUS
EFIAPI
MemoryVerificationAddPattern (
  IN VOID   *TestPattern,
  IN UINTN  TestPatternSize
  );

/**
 * @brief Runs the memory test over the specified memory
 *
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
  IN EFI_PHYSICAL_ADDRESS   TestAddress,
  IN UINTN                  TestLength,
  IN UINTN                  TestSpan,
  OUT EFI_PHYSICAL_ADDRESS  *FailedMemoryAddress OPTIONAL
  );

#endif
