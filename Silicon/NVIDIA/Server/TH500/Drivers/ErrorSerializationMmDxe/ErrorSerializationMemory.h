/** @file
  NVIDIA ERST Driver memory manager header

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EERROR_SERIALIZATION_MEMORY_H_
#define _EERROR_SERIALIZATION_MEMORY_H_

#include <Uefi.h>

VOID *
ErstAllocatePoolRecord (
  UINTN  AllocationSize
  );

VOID
ErstFreePoolRecord (
  VOID  *Allocation
  );

VOID *
ErstAllocatePoolCperHeader (
  UINTN  AllocationSize
  );

VOID
ErstFreePoolCperHeader (
  VOID  *Allocation
  );

VOID *
ErstAllocatePoolBlock (
  UINTN  AllocationSize
  );

VOID
ErstFreePoolBlock (
  VOID  *Allocation
  );

VOID *
ErstAllocatePoolBlockInfo (
  UINTN  AllocationSize
  );

VOID
ErstFreePoolBlockInfo (
  VOID  *Allocation
  );

VOID *
ErstAllocatePoolRecordInfo (
  UINTN  AllocationSize
  );

VOID
ErstFreePoolRecordInfo (
  VOID  *Allocation
  );

EFI_STATUS
EFIAPI
ErstPreAllocateRuntimeMemory (
  IN UINTN  BlockPoolSize,
  IN UINTN  MaxRecordSize
  );

VOID
ErstFreeRuntimeMemory (
  );

VOID
ErstMemoryInit (
  );

#endif
