/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __PLATFORM_RESOURCE_CONFIG_H_INCL__
#define __PLATFORM_RESOURCE_CONFIG_H_INCL__

/**
   Appends a memory region to a memory region list. The caller must
   guarantee the list has sufficient capacity.

   @param[in]     Regions      The memory region list.
   @param[in,out] RegionCount  Number of memory regions in the list.
   @param[in]     BaseAddress  Base of the memory region to add.
   @param[in]     Length       Length of the memory region to add.
*/
VOID
PlatformResourceAddMemoryRegion (
  IN     NVDA_MEMORY_REGION    *Regions,
  IN OUT UINTN                 *RegionCount,
  IN     EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN     UINT64                Length
  );

/**
   Adds retired DRAM pages to a memory region list.

   @param[in]     Regions               The list of memory regions.
   @param[in,out] RegionCount           Number of regions in the list.
   @param[in]     RetiredDramPageList   List of retired DRAM page addresses.
   @param[in]     RetiredDramPageCount  Number of retired DRAM page addresses.
   @param[in]     RetiredDramPageSize   Size of a retired DRAM page.
*/
VOID
PlatformResourceAddRetiredDramPages (
  IN     NVDA_MEMORY_REGION          *Regions,
  IN OUT UINTN                       *RegionCount,
  IN     CONST EFI_PHYSICAL_ADDRESS  *RetiredDramPageList,
  IN     UINTN                       RetiredDramPageCount,
  IN     UINT64                      RetiredDramPageSize
  );

/**
   Adds retired DRAM page indices to a memory region list.

   @param[in]     Regions                    The list of memory regions.
   @param[in,out] RegionCount                Number of regions in the list.
   @param[in]     RetiredDramPageIndexList   List of retired DRAM page indices.
   @param[in]     RetiredDramPageIndexCount  Number of retired DRAM page indices.
   @param[in]     RetiredDramPageSize        Size of a retired DRAM page.
*/
VOID
PlatformResourceAddRetiredDramPageIndices (
  IN     NVDA_MEMORY_REGION  *Regions,
  IN OUT UINTN               *RegionCount,
  IN     CONST UINT32        *RetiredDramPageIndexList,
  IN     UINTN               RetiredDramPageIndexCount,
  IN     UINT64              RetiredDramPageSize
  );

#endif // __PLATFORM_RESOURCE_CONFIG_H_INCL__
