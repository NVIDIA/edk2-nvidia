/** @file

  Stubbable instance of Memory Allocation Library based on POSIX APIs

  Uses POSIX APIs malloc() and free() to allocate and free memory.  But also
  allow failures in malloc to be simulated.

  This can be used as a drop-in replacement for MemoryAllocationLibPosix.
  However, if Mock calls are used to simulate allocation failures, then
  MemoryAllocationStubLibInit() must be called to reset state.

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __MEMORY_ALLOCATION_STUB_LIB_H__
#define __MEMORY_ALLOCATION_STUB_LIB_H__

#include <Library/MemoryAllocationLib.h>

/**
  Set the number of allocations available before AllocatePool() begins failing.

  Allows tests to simulate memory allocation failures.

  @param[In]  AvailableAllocations  Number of allocations
 */
VOID
MockAllocatePool (
  UINT64  AvailableAllocations
  );

/**
  Initialize MemoryAllocationLib stub support.

  This should be called once before running tests to reset state after a test
  runs with Mock calls to this library.  If the test does not make Mock calls,
  this init is not necessary.

  @retval None

**/
VOID
MemoryAllocationStubLibInit (
  VOID
  );

#endif
