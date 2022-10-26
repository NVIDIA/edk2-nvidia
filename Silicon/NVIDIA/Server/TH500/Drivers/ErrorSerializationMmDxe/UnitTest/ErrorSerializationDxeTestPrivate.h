/** @file
  Unit test definitions for the Error Serialization driver.

  Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _ERROR_SERIALIZATION_DXE_TEST_PRIVATE_H_
#define _ERROR_SERIALIZATION_DXE_TEST_PRIVATE_H_

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>

#include <HostBasedTestStubLib/NorFlashStubLib.h>

// Test context for Read, Write, and Erase tests
typedef struct {
  UINTN         ErstOffset;     // Offset to ERST in flash
  UINTN         Offset;         // Input Offset
  UINTN         TestValue;      // NumBytes, Length, Status, etc.
  EFI_STATUS    ExpectedStatus; // Expected Status returned
} COMMON_TEST_CONTEXT;

#endif
