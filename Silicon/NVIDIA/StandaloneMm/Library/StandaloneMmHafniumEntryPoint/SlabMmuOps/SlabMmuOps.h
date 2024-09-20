/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _SLAB_MMU_OPS_H_
#define _SLAB_MMU_OPS_H_

#include "../StandaloneMmArmLib.h"

#define PAGE_ALIGN(Address, PageSize)  ((UINT64) (Address) & ~(PageSize - 1))

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
  IN  STMM_ARM_MEMORY_REGION_DESCRIPTOR  *MemoryTable,
  OUT VOID                               **TranslationTableBase OPTIONAL,
  OUT UINTN                              *TranslationTableSize OPTIONAL
  );

#endif /* _SLAB_MMU_OPS_H_ */
