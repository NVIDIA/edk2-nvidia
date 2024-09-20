/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __SMM_SLAB_MEMORY_ALLOCATION_LIB_H__
#define __SMM_SLAB_MEMORY_ALLOCATION_LIB_H__

VOID
EFIAPI
SlabArmSetEntriesSlab (
  IN  UINT64  EntriesBase,
  IN  UINT64  EntriesPages
  );

VOID *
EFIAPI
AllocatePagesSlabMmSt (
  IN UINTN  Pages
  );

VOID
EFIAPI
FreePagesSlabMmst (
  IN VOID   *Buffer,
  IN UINTN  Pages
  );

#endif //__SMM_SLAB_MEMORY_ALLOCATION_LIB_H__
