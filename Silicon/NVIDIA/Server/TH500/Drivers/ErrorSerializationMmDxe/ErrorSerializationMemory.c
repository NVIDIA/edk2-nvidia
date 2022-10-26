/** @file
  NVIDIA ERST Driver memory manager

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "ErrorSerializationMemory.h"
#include <Uefi.h>
#include <Library/DebugLib.h>            // ASSERT
#include <Library/MemoryAllocationLib.h> // AllocatePool
#include <Guid/Cper.h>                   // From MdePkg

typedef struct {
  VOID       *Memory;
  UINTN      Size;
  BOOLEAN    InUse;
} ERST_MEMORY_POOL_INFO;

#define MAX_RECORD_POOLS  4

enum {
  ERST_POOL_CPER_HEADER,
  ERST_POOL_BLOCK,
  ERST_POOL_BLOCK_INFO,
  ERST_POOL_RECORD_INFO,
  ERST_POOL_RECORDS,
  ERST_POOLS_COUNT = ERST_POOL_RECORDS + MAX_RECORD_POOLS
};

ERST_MEMORY_POOL_INFO  ErstPools[ERST_POOLS_COUNT];

STATIC VOID *
ErstAllocatePool (
  ERST_MEMORY_POOL_INFO  *PoolInfo,
  UINTN                  AllocationSize
  )
{
  UINTN  PoolIndex;

  if (PoolInfo->InUse || ((PoolInfo->Memory != NULL) && (PoolInfo->Size < AllocationSize))) {
    PoolIndex = ((__UINTPTR_TYPE__)PoolInfo - (__UINTPTR_TYPE__)ErstPools)/sizeof (ERST_MEMORY_POOL_INFO);
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failing to allocate 0x%p bytes [PoolInfo[%u]: InUse=%d, Memory=0x%p, Size=0x%p]\n",
      __FUNCTION__,
      (VOID *)AllocationSize,
      PoolIndex,
      PoolInfo->InUse ? 1 : 0,
      PoolInfo->Memory,
      (VOID *)PoolInfo->Size
      ));
    return NULL;
  }

  if (PoolInfo->Memory == NULL) {
    PoolInfo->Memory = AllocateRuntimePool (AllocationSize);
    if (PoolInfo->Memory == NULL) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failing to allocate 0x%u bytes for new pool\n",
        __FUNCTION__,
        AllocationSize
        ));
      return NULL;
    }

    PoolInfo->Size = AllocationSize;
  }

  PoolInfo->InUse = TRUE;
  return PoolInfo->Memory;
}

STATIC VOID
ErstFreePool (
  ERST_MEMORY_POOL_INFO  *PoolInfo,
  VOID                   *Allocation
  )
{
  UINTN  PoolIndex;

  if ((Allocation != PoolInfo->Memory) || !PoolInfo->InUse) {
    PoolIndex = ((__UINTPTR_TYPE__)PoolInfo - (__UINTPTR_TYPE__)ErstPools)/sizeof (ERST_MEMORY_POOL_INFO);
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failing to free address 0x%p [PoolInfo[%u]: InUse=%d, Memory=0x%p, Size=0x%p]\n",
      __FUNCTION__,
      Allocation,
      PoolIndex,
      PoolInfo->InUse ? 1 : 0,
      PoolInfo->Memory,
      (VOID *)PoolInfo->Size
      ));
    ASSERT (Allocation == PoolInfo->Memory);
    ASSERT (PoolInfo->InUse);
  }

  PoolInfo->InUse = FALSE;
}

VOID *
ErstAllocatePoolRecord (
  UINTN  AllocationSize
  )
{
  UINTN  PoolIndex;
  VOID   *Allocation;

  for (PoolIndex = 0; PoolIndex < MAX_RECORD_POOLS; PoolIndex++) {
    Allocation = ErstAllocatePool (&ErstPools[ERST_POOL_RECORDS + PoolIndex], AllocationSize);
    if (Allocation != NULL) {
      return Allocation;
    }
  }

  return NULL;
}

VOID
ErstFreePoolRecord (
  VOID  *Allocation
  )
{
  UINTN  PoolIndex;

  for (PoolIndex = 0; PoolIndex < MAX_RECORD_POOLS; PoolIndex++) {
    if (Allocation == ErstPools[ERST_POOL_RECORDS + PoolIndex].Memory) {
      ErstFreePool (&ErstPools[ERST_POOL_RECORDS + PoolIndex], Allocation);
      return;
    }
  }

  ASSERT (0 && "UNABLE TO FREE RECORD POOL");
}

#define GENERATE_POOL_ALLOCATE_FREE_FOR(PoolName, PoolIndex) \
VOID *ErstAllocatePool##PoolName(UINTN AllocationSize) { \
  return ErstAllocatePool(&ErstPools[PoolIndex], AllocationSize); \
} \
\
VOID ErstFreePool##PoolName(VOID *Allocation) { \
  DEBUG ((DEBUG_VERBOSE, "%a: trying to free 0x%p for pool %d\n", __FUNCTION__, Allocation, PoolIndex)); \
  ErstFreePool(&ErstPools[PoolIndex], Allocation); \
} \

GENERATE_POOL_ALLOCATE_FREE_FOR (CperHeader, ERST_POOL_CPER_HEADER)
GENERATE_POOL_ALLOCATE_FREE_FOR (Block, ERST_POOL_BLOCK)
GENERATE_POOL_ALLOCATE_FREE_FOR (BlockInfo, ERST_POOL_BLOCK_INFO)
GENERATE_POOL_ALLOCATE_FREE_FOR (RecordInfo, ERST_POOL_RECORD_INFO)

EFI_STATUS
EFIAPI
ErstPreAllocateRuntimeMemory (
  IN UINTN  BlockPoolSize,
  IN UINTN  MaxRecordSize
  )
{
  UINTN  PoolIndex;
  VOID   *Allocation;

  DEBUG ((DEBUG_VERBOSE, "%a(BlockPoolSize = 0x%p, MaxRecordSize = 0x%p) called\n", __FUNCTION__, BlockPoolSize, MaxRecordSize));

  // Pre-allocate and free the Record pool
  for (PoolIndex = 0; PoolIndex < MAX_RECORD_POOLS; PoolIndex++) {
    Allocation = ErstAllocatePool (&ErstPools[ERST_POOL_RECORDS + PoolIndex], MaxRecordSize);
    if (Allocation == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  for (PoolIndex = 0; PoolIndex < MAX_RECORD_POOLS; PoolIndex++) {
    ErstFreePool (&ErstPools[ERST_POOL_RECORDS + PoolIndex], ErstPools[ERST_POOL_RECORDS + PoolIndex].Memory);
  }

  // Pre-allocate and free the CperHeader pool
  Allocation = ErstAllocatePool (&ErstPools[ERST_POOL_CPER_HEADER], sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  if (Allocation == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ErstFreePool (&ErstPools[ERST_POOL_CPER_HEADER], ErstPools[ERST_POOL_CPER_HEADER].Memory);

  // Pre-allocate and free the Block pool
  Allocation = ErstAllocatePool (&ErstPools[ERST_POOL_BLOCK], BlockPoolSize);
  if (Allocation == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ErstFreePool (&ErstPools[ERST_POOL_BLOCK], ErstPools[ERST_POOL_BLOCK].Memory);

  // Note: BlockInfo and RecordInfo pools will be allocated at first init time

  return EFI_SUCCESS;
}

VOID
ErstFreeRuntimeMemory (
  )
{
  UINTN  PoolIndex;

  for (PoolIndex = 0; PoolIndex < ERST_POOLS_COUNT; PoolIndex++) {
    if (ErstPools[PoolIndex].Memory) {
      FreePool (ErstPools[PoolIndex].Memory);
      ErstPools[PoolIndex].Memory = NULL;
      ErstPools[PoolIndex].Size   = 0;
      ErstPools[PoolIndex].InUse  = FALSE;
    }
  }
}

VOID
ErstMemoryInit (
  )
{
  UINTN  PoolIndex;

  for (PoolIndex = 0; PoolIndex < ERST_POOLS_COUNT; PoolIndex++) {
    ErstPools[PoolIndex].Memory = NULL;
    ErstPools[PoolIndex].Size   = 0;
    ErstPools[PoolIndex].InUse  = FALSE;
  }
}
