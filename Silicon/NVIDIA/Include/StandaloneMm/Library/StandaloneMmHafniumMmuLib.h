/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __SMM_HAFNIUM_MMU_LIB_H__
#define __SMM_HAFNIUM_MMU_LIB_H__

#include <Library/StandaloneMmArmLib.h>

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

EFI_STATUS
StMmSetMemoryAttributes (
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                Length,
  IN UINT64                Attributes,
  IN UINT64                AttributeMask
  );

#endif //__SMM_HAFNIUM_MMU_LIB_H__
