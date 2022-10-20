/** @file
*
*  Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _SLAB_MMU_OPS_H_
#define _SLAB_MMU_OPS_H_

#define PAGE_ALIGN(Address, PageSize)  ((UINT64) (Address) & ~(PageSize - 1))

/**
 * Initialize the slab allocator for stage-1 page table entries.
 *
 * @param EntriesBase   Base address of the location to store entries
 * @param EntriesPages  Number of pages available
 */
VOID
EFIAPI
SlabArmSetEntriesSlab (
  IN  UINT64  EntriesBase,
  IN  UINT64  EntriesPages
  );

/**
 * Configure stage-1 page table entries using the provided table of memory entries.
 *
 * @param MemoryTable           Memory entries describing the memory to map in stage-1
 * @param TranslationTableBase  Not used
 * @param TranslationTableSize  Not used
 *
 * @return EFI_STATUS
 */
EFI_STATUS
EFIAPI
SlabArmConfigureMmu (
  IN  ARM_MEMORY_REGION_DESCRIPTOR  *MemoryTable,
  OUT VOID                          **TranslationTableBase OPTIONAL,
  OUT UINTN                         *TranslationTableSize OPTIONAL
  );

#endif /* _SLAB_MMU_OPS_H_ */
