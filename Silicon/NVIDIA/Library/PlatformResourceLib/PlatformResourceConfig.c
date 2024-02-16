/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>

#include <Library/DramCarveoutLib.h>
#include <Library/NVIDIADebugLib.h>

#include "PlatformResourceConfig.h"

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
  IN     NVDA_MEMORY_REGION  *CONST  Regions,
  IN OUT UINTN               *CONST  RegionCount,
  IN     CONST EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN     CONST UINT64                Length
  )
{
  NV_ASSERT_RETURN (
    Regions != NULL,
    return ,
    "%a: Regions is NULL\r\n",
    __FUNCTION__
    );
  NV_ASSERT_RETURN (
    RegionCount != NULL,
    return ,
    "%a: RegionCount is NULL\r\n",
    __FUNCTION__
    );

  if ((BaseAddress != 0) && (Length != 0)) {
    Regions[*RegionCount].MemoryBaseAddress = BaseAddress;
    Regions[*RegionCount].MemoryLength      = Length;
    (*RegionCount)++;
  }
}

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
  IN     NVDA_MEMORY_REGION         *CONST  Regions,
  IN OUT UINTN                      *CONST  RegionCount,
  IN     CONST EFI_PHYSICAL_ADDRESS *CONST  RetiredDramPageList,
  IN     CONST UINTN                        RetiredDramPageCount,
  IN     CONST UINT64                       RetiredDramPageSize
  )
{
  UINTN                 Index;
  EFI_PHYSICAL_ADDRESS  Base;

  NV_ASSERT_RETURN (
    (RetiredDramPageCount == 0) || (RetiredDramPageList != NULL),
    return ,
    "%a: RetiredDramPageCount is non-zero, but RetiredDramPageList is NULL\r\n",
    __FUNCTION__
    );
  NV_ASSERT_RETURN (
    (RetiredDramPageCount == 0) || (RetiredDramPageSize > 0),
    return ,
    "%a: RetiredDramPageCount is non-zero, but RetiredDramPageSize is zero\r\n",
    __FUNCTION__
    );

  for (Index = 0; Index < RetiredDramPageCount; Index++) {
    Base = RetiredDramPageList[Index];
    if (Base == 0) {
      break;
    }

    PlatformResourceAddMemoryRegion (
      Regions,
      RegionCount,
      Base,
      RetiredDramPageSize
      );
  }
}

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
  IN     NVDA_MEMORY_REGION *CONST  Regions,
  IN OUT UINTN              *CONST  RegionCount,
  IN     CONST UINT32       *CONST  RetiredDramPageIndexList,
  IN     CONST UINTN                RetiredDramPageIndexCount,
  IN     CONST UINT64               RetiredDramPageSize
  )
{
  UINTN   Index;
  UINT32  PageIndex;

  NV_ASSERT_RETURN (
    (RetiredDramPageIndexCount == 0) || (RetiredDramPageIndexList != NULL),
    return ,
    "%a: RetiredDramPageIndexCount is non-zero, but RetiredDramPageIndexList is NULL\r\n",
    __FUNCTION__
    );
  NV_ASSERT_RETURN (
    (RetiredDramPageIndexCount == 0) || (RetiredDramPageSize > 0),
    return ,
    "%a: RetiredDramPageIndexCount is non-zero, but RetiredDramPageSize is zero\r\n",
    __FUNCTION__
    );

  for (Index = 0; Index < RetiredDramPageIndexCount; ++Index) {
    PageIndex = RetiredDramPageIndexList[Index];
    if (PageIndex == 0) {
      break;
    }

    PlatformResourceAddMemoryRegion (
      Regions,
      RegionCount,
      (UINT64)PageIndex * RetiredDramPageSize,
      RetiredDramPageSize
      );
  }
}
