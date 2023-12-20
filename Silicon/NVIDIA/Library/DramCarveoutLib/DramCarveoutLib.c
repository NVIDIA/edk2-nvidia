/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>
#include <Library/DramCarveoutLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Pi/PiHob.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Guid/MemoryTypeInformation.h>
#include <Library/NVIDIADebugLib.h>

// Linux can only handle up to 1024 memory regions
#define MAX_MEMORY_REGIONS  1024

typedef enum {
  ORDER_IS_CORRECT,
  ORDER_IS_EQUAL,
  ORDER_IS_SWAPPED
} COMPARE_RESULT;

/**
 * Function takes two pointers, compares them, and returns:
 *  ORDER_IS_CORRECT if A comes before B
 *  ORDER_IS_EQUAL   if A and B are equal
 *  ORDER_IS_SWAPPED if B comes before A
*/
typedef COMPARE_RESULT (*COMPARE_FUNC) (
  VOID  *A,
  VOID  *B
  );

COMPARE_RESULT
CompareRegionAddressLowToHigh (
  VOID  *A,
  VOID  *B
  )
{
  EFI_PHYSICAL_ADDRESS  ValA, ValB;

  NV_ASSERT_RETURN (((A != NULL) && (B != NULL)), return 0, "%a: NULL pointer found (A=%p, B=%p)\n", __FUNCTION__, A, B);

  ValA = ((NVDA_MEMORY_REGION *)A)->MemoryBaseAddress;
  ValB = ((NVDA_MEMORY_REGION *)B)->MemoryBaseAddress;

  if (ValA < ValB) {
    DEBUG ((DEBUG_VERBOSE, "%a: A=0x%lx, B=0x%lx, ORDER_IS_CORRECT\n", __FUNCTION__, ValA, ValB));
    return ORDER_IS_CORRECT;
  } else if (ValA == ValB) {
    DEBUG ((DEBUG_VERBOSE, "%a: A=0x%lx, B=0x%lx, ORDER_IS_EQUAL\n", __FUNCTION__, ValA, ValB));
    return ORDER_IS_EQUAL;
  } else {
    DEBUG ((DEBUG_VERBOSE, "%a: A=0x%lx, B=0x%lx, ORDER_IS_SWAPPED\n", __FUNCTION__, ValA, ValB));
    return ORDER_IS_SWAPPED;
  }
}

COMPARE_RESULT
CompareRegionSizeHighToLow (
  VOID  *A,
  VOID  *B
  )
{
  UINT64  ValA, ValB;

  NV_ASSERT_RETURN (((A != NULL) && (B != NULL)), return 0, "%a: NULL pointer found (A=%p, B=%p)\n", __FUNCTION__, A, B);

  ValA = ((NVDA_MEMORY_REGION *)A)->MemoryLength;
  ValB = ((NVDA_MEMORY_REGION *)B)->MemoryLength;

  if (ValA > ValB) {
    DEBUG ((DEBUG_VERBOSE, "%a: A=0x%lx, B=0x%lx, ORDER_IS_CORRECT\n", __FUNCTION__, ValA, ValB));
    return ORDER_IS_CORRECT;
  } else if (ValA == ValB) {
    DEBUG ((DEBUG_VERBOSE, "%a: A=0x%lx, B=0x%lx, ORDER_IS_EQUAL\n", __FUNCTION__, ValA, ValB));
    return ORDER_IS_EQUAL;
  } else {
    DEBUG ((DEBUG_VERBOSE, "%a: A=0x%lx, B=0x%lx, ORDER_IS_SWAPPED\n", __FUNCTION__, ValA, ValB));
    return ORDER_IS_SWAPPED;
  }
}

/**
  Insert an element to a sorted list, dropping the rightmost if needed

  @param Regions [IN, OUT]      Array of SORTED regions to insert into
  @param RegionsCount [IN, OUT] Number of regions in array
  @param NewRegion      [IN]    Region to insert. Must not be in the array!
  @param RegionCountMax [IN]    Maximum number of regions to keep
  @param CompareFunc    [IN]    Function to use to determine ordering
**/
STATIC
VOID
MemoryRegionInsert (
  IN OUT NVDA_MEMORY_REGION  *Regions,
  IN OUT UINTN               *RegionsCount,
  IN NVDA_MEMORY_REGION      *NewRegion,
  IN UINTN                   RegionCountMax,
  IN COMPARE_FUNC            CompareFunc
  )
{
  UINTN           InsertIndex;
  UINTN           ShiftCount;
  UINTN           Low, High;
  COMPARE_RESULT  CompareResult;

  NV_ASSERT_RETURN (Regions != NULL, return , "%a: Regions is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (RegionsCount != NULL, return , "%a: RegionsCount is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (NewRegion != NULL, return , "%a: NewRegion is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CompareFunc != NULL, return , "%a: CompareFunc is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (
    ((NewRegion + 1) <= Regions) ||
    (&Regions[RegionCountMax] <= NewRegion),
    return ,
    "%a: NewRegion overlaps with Regions, which isn't allowed\n",
    __FUNCTION__
    );
  DEBUG ((DEBUG_VERBOSE, "%a: Called with RegionsCount = %lu, NewRegion->Base = 0x%lx, NewRegion->Size = 0x%lx, RegionCountMax = %lu\n", __FUNCTION__, *RegionsCount, NewRegion->MemoryBaseAddress, NewRegion->MemoryLength, RegionCountMax));

  // Determine insertion point for NewRegion
  if (*RegionsCount == 0) {
    InsertIndex = 0;
  } else if (*RegionsCount == 1) {
    if (CompareFunc (&Regions[0], NewRegion) == ORDER_IS_SWAPPED) {
      InsertIndex = 0;
    } else {
      InsertIndex = 1;
    }
  } else {
    // Binary Search to get near the insertion point quickly
    Low  = 0;
    High = *RegionsCount;
    while (High > Low) {
      InsertIndex = Low + (High - Low + 1)/2;
      DEBUG ((DEBUG_VERBOSE, "%a: Binary Searching, Low/Insert/High = %lu, %lu, %lu\n", __FUNCTION__, Low, InsertIndex, High));
      CompareResult = CompareFunc (&Regions[InsertIndex], NewRegion);
      if (CompareResult == ORDER_IS_SWAPPED) {
        High = InsertIndex - 1;
      } else if (CompareResult == ORDER_IS_EQUAL) {
        break;
      } else {
        Low = InsertIndex + 1;
      }
    }

    // Narrow it down to the exact location, preferring the end if equal
    DEBUG ((DEBUG_VERBOSE, "%a: Binary Search done, Low/Insert/High = %lu, %lu, %lu, CompareResult = %d\n", __FUNCTION__, Low, InsertIndex, High, CompareResult));
    while (((InsertIndex) < *RegionsCount) && (CompareFunc (&Regions[InsertIndex], NewRegion) != ORDER_IS_SWAPPED)) {
      InsertIndex++;
      DEBUG ((DEBUG_VERBOSE, "%a: Incremented InsertIndex to %lu because NOT ORDER_IS_SWAPPED\n", __FUNCTION__, InsertIndex));
    }

    while ((InsertIndex > 0) && (CompareFunc (&Regions[InsertIndex-1], NewRegion) == ORDER_IS_SWAPPED)) {
      InsertIndex--;
      DEBUG ((DEBUG_VERBOSE, "%a: Decremented InsertIndex to %lu because ORDER_IS_SWAPPED\n", __FUNCTION__, InsertIndex));
    }

    // Sanity check result
    DEBUG ((DEBUG_VERBOSE, "%a: InsertIndex found to be %lu\n", __FUNCTION__, InsertIndex));
    if (InsertIndex > 0) {
      ASSERT (CompareFunc (&Regions[InsertIndex-1], NewRegion) != ORDER_IS_SWAPPED);
    }

    if (InsertIndex < *RegionsCount) {
      ASSERT (CompareFunc (&Regions[InsertIndex], NewRegion) == ORDER_IS_SWAPPED);
    }
  }

  // Insert the object if it fits
  if (InsertIndex < RegionCountMax) {
    // Shift to make space if needed
    ShiftCount = (*RegionsCount - InsertIndex);
    if (*RegionsCount == RegionCountMax) {
      ShiftCount--;
    } else {
      *RegionsCount = *RegionsCount + 1;
    }

    if (ShiftCount > 0) {
      CopyMem (&Regions[InsertIndex + 1], &Regions[InsertIndex], sizeof (*NewRegion) * ShiftCount);
    }

    // Copy the new element in
    CopyMem (&Regions[InsertIndex], NewRegion, sizeof (*NewRegion));
    DEBUG ((DEBUG_VERBOSE, "%a: Added entry at index %lu\n", __FUNCTION__, InsertIndex));
  }
}

/**
  Simple in place sort to sort regions entries in ascending order.

  @param Regions [IN, OUT]    Array of regions to sort
  @param RegionsCount [IN]    Number of regions in array
  @param CompareFunc [IN]     Function to use to sort
**/
STATIC
VOID
MemoryRegionSort (
  IN OUT NVDA_MEMORY_REGION  *Regions,
  IN UINTN                   RegionsCount,
  IN COMPARE_FUNC            CompareFunc
  )
{
  NVDA_MEMORY_REGION  CurrentRegion;
  UINTN               SortedSize = 1;

  NV_ASSERT_RETURN (CompareFunc != NULL, return , "%a: Found NULL CompareFunc\n", __FUNCTION__);

  while (SortedSize < RegionsCount) {
    CopyMem (&CurrentRegion, &Regions[SortedSize], sizeof (CurrentRegion));
    MemoryRegionInsert (Regions, &SortedSize, &CurrentRegion, SortedSize + 1, CompareFunc);
  }
}

/**
  Finds a memory hob that contains the specified address

  @param[in] MemoryAddress      Address to look for in a HOB.

  @return Hob that contains the address, NULL if not found
**/
STATIC
EFI_HOB_RESOURCE_DESCRIPTOR *
FindMemoryHob (
  IN EFI_PHYSICAL_ADDRESS  MemoryAddress
  )
{
  EFI_PEI_HOB_POINTERS  HobPtr;

  HobPtr.Raw = GetHobList ();
  while ((HobPtr.Raw = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, HobPtr.Raw)) != NULL) {
    if ((HobPtr.ResourceDescriptor->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) &&
        (MemoryAddress >= HobPtr.ResourceDescriptor->PhysicalStart) &&
        (MemoryAddress <= (HobPtr.ResourceDescriptor->PhysicalStart + HobPtr.ResourceDescriptor->ResourceLength - 1)))
    {
      return HobPtr.ResourceDescriptor;
    }

    HobPtr.Raw = GET_NEXT_HOB (HobPtr);
  }

  return NULL;
}

/**
  Mark memory regions that are in use as tested.
**/
STATIC
VOID
MarkUsedMemoryTested (
  VOID
  )
{
  EFI_PHYSICAL_ADDRESS         Address;
  EFI_HOB_RESOURCE_DESCRIPTOR  *ResourceHob;
  EFI_PEI_HOB_POINTERS         HobPtr;

  HobPtr.Raw  = GetHobList ();
  Address     = (EFI_PHYSICAL_ADDRESS)HobPtr.Raw;
  ResourceHob = FindMemoryHob (Address);
  if (ResourceHob != NULL) {
    ResourceHob->ResourceAttribute |= EFI_RESOURCE_ATTRIBUTE_TESTED;
  }

  // Find all memory allocations
  while ((HobPtr.Raw = GetNextHob (EFI_HOB_TYPE_MEMORY_ALLOCATION, HobPtr.Raw)) != NULL) {
    ResourceHob = FindMemoryHob (HobPtr.MemoryAllocation->AllocDescriptor.MemoryBaseAddress);
    if (ResourceHob != NULL) {
      ResourceHob->ResourceAttribute |= EFI_RESOURCE_ATTRIBUTE_TESTED;
    }

    HobPtr.Raw = GET_NEXT_HOB (HobPtr);
  }
}

/**
  Installs DRAM resources to the HOB list

  This function install the specified DRAM regions into memory while removing
  the carveout regions.
  This function is called by the platform memory initialization library.

  @param  InputDramRegions            Unsorted list of available DRAM regions
  @param  DramRegionsCount            Number of regions in DramRegions.
  @param  UefiDramRegionIndex         Index of UEFI usable regions in DramRegions.
  @param  CarveoutRegions             Unsorted list of carveout regions that will be
                                      removed from DramRegions.
  @param  CarveoutRegionsCount        Number of regions in CarveoutRegions.
  @param  UsableCarveoutRegions       Unsorted list of usable carveout regions that will be
                                      added to DramRegions.
  @param  UsableCarveoutRegionsCount  Number of regions in UsableCarveoutRegions.
  @param  FinalRegionsCount           Number of regions installed into HOB list.
  @param  MaxRegionStart              Base address of largest region in DRAM usable by UEFI
  @param  MaxRegionSize               Size of largest region region in DRAM usable by UEFI

  @retval EFI_SUCCESS                 Resources have been installed
  @retval EFI_DEVICE_ERROR            Error setting up memory

**/
EFI_STATUS
InstallDramWithCarveouts (
  IN  CONST NVDA_MEMORY_REGION  *InputDramRegions,
  IN  UINTN                     DramRegionsCount,
  IN  UINTN                     UefiDramRegionIndex,
  IN  NVDA_MEMORY_REGION        *CarveoutRegions,
  IN  UINTN                     CarveoutRegionsCount,
  IN  NVDA_MEMORY_REGION        *UsableCarveoutRegions,
  IN  UINTN                     UsableCarveoutRegionsCount,
  OUT UINTN                     *FinalRegionsCount,
  OUT EFI_PHYSICAL_ADDRESS      *MaxRegionStart,
  OUT UINTN                     *MaxRegionSize
  )
{
  NVDA_MEMORY_REGION           *DramRegions    = NULL;
  NVDA_MEMORY_REGION           *LargestRegions = NULL;
  NVDA_MEMORY_REGION           Region;
  NVDA_MEMORY_REGION           LargestUefiRegion;
  UINTN                        DramIndex           = 0;
  UINTN                        CarveoutIndex       = 0;
  UINTN                        UsableCarveoutIndex = 0;
  UINTN                        InstalledRegions    = 0;
  EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttributes;
  EFI_PHYSICAL_ADDRESS         CarveoutStart;
  EFI_PHYSICAL_ADDRESS         CarveoutEnd;
  EFI_PHYSICAL_ADDRESS         DramEnd;
  CONST UINTN                  MaxGeneralRegions = (MAX_MEMORY_REGIONS - UsableCarveoutRegionsCount - 1);
  EFI_PHYSICAL_ADDRESS         UefiMemoryBase    = InputDramRegions[UefiDramRegionIndex].MemoryBaseAddress;
  EFI_PHYSICAL_ADDRESS         UefiMemoryEnd     = UefiMemoryBase + InputDramRegions[UefiDramRegionIndex].MemoryLength;
  UINTN                        ListIndex;

  // InputDramRegions is CONST, so we need a sortable copy
  DramRegions = AllocatePool (sizeof (NVDA_MEMORY_REGION) * DramRegionsCount);
  NV_ASSERT_RETURN (DramRegions != NULL, return EFI_DEVICE_ERROR, "%a: Unable to allocate space for %lu DRAM regions\n", __FUNCTION__, DramRegionsCount);
  CopyMem (DramRegions, InputDramRegions, sizeof (NVDA_MEMORY_REGION) * DramRegionsCount);

  LargestRegions = AllocatePool (sizeof (NVDA_MEMORY_REGION) * MAX_MEMORY_REGIONS);
  NV_ASSERT_RETURN (LargestRegions != NULL, return EFI_DEVICE_ERROR, "%a: Unable to allocate space for the %d largest regions\n", __FUNCTION__, MAX_MEMORY_REGIONS);

  MemoryRegionSort (DramRegions, DramRegionsCount, CompareRegionAddressLowToHigh);
  for (DramIndex = 0; DramIndex < DramRegionsCount; DramIndex++) {
    DEBUG ((
      DEBUG_VERBOSE,
      "%a() Dram Region: Base: 0x%016lx, Size: 0x%016lx\n",
      __FUNCTION__,
      DramRegions[DramIndex].MemoryBaseAddress,
      DramRegions[DramIndex].MemoryLength
      ));
  }

  MemoryRegionSort (CarveoutRegions, CarveoutRegionsCount, CompareRegionAddressLowToHigh);
  for (CarveoutIndex = 0; CarveoutIndex < CarveoutRegionsCount; CarveoutIndex++) {
    DEBUG ((
      DEBUG_VERBOSE,
      "%a() Carveout Region: Base: 0x%016lx, Size: 0x%016lx\n",
      __FUNCTION__,
      CarveoutRegions[CarveoutIndex].MemoryBaseAddress,
      CarveoutRegions[CarveoutIndex].MemoryLength
      ));
  }

  MemoryRegionSort (UsableCarveoutRegions, UsableCarveoutRegionsCount, CompareRegionAddressLowToHigh);
  for (UsableCarveoutIndex = 0; UsableCarveoutIndex < UsableCarveoutRegionsCount; UsableCarveoutIndex++) {
    DEBUG ((
      DEBUG_VERBOSE,
      "%a() Usable Carveout Region: Base: 0x%016lx, Size: 0x%016lx\n",
      __FUNCTION__,
      UsableCarveoutRegions[UsableCarveoutIndex].MemoryBaseAddress,
      UsableCarveoutRegions[UsableCarveoutIndex].MemoryLength
      ));
  }

  DramIndex                           = 0;
  CarveoutIndex                       = 0;
  UsableCarveoutIndex                 = 0;
  LargestUefiRegion.MemoryBaseAddress = 0;
  LargestUefiRegion.MemoryLength      = 0;

  while (DramIndex < DramRegionsCount) {
    if (CarveoutIndex < CarveoutRegionsCount) {
      CarveoutStart = CarveoutRegions[CarveoutIndex].MemoryBaseAddress;
      CarveoutEnd   = CarveoutRegions[CarveoutIndex].MemoryBaseAddress + CarveoutRegions[CarveoutIndex].MemoryLength;
    } else {
      CarveoutStart = MAX_UINT64;
      CarveoutEnd   = MAX_UINT64;
    }

    DramEnd = DramRegions[DramIndex].MemoryBaseAddress + DramRegions[DramIndex].MemoryLength;
    // Check if region starts before the carveout, will install that region
    // Installs the space between the Dram Start and the Carveout Start as a DRAM region
    if (DramRegions[DramIndex].MemoryBaseAddress < CarveoutStart) {
      Region.MemoryBaseAddress = DramRegions[DramIndex].MemoryBaseAddress;
      if (CarveoutStart < DramEnd) {
        Region.MemoryLength = CarveoutStart - Region.MemoryBaseAddress;
      } else {
        Region.MemoryLength = DramRegions[DramIndex].MemoryLength;
      }

      // Either save this region as the largest UEFI region, or add it to the list
      if ((Region.MemoryBaseAddress >= UefiMemoryBase) &&
          (Region.MemoryBaseAddress <  UefiMemoryEnd) &&
          (LargestUefiRegion.MemoryLength < Region.MemoryLength))
      {
        // Add the previous largest to the list before overwriting it
        if (LargestUefiRegion.MemoryLength > 0) {
          DEBUG ((DEBUG_VERBOSE, "DRAM Region: %016lx, %016lx\r\n", LargestUefiRegion.MemoryBaseAddress, LargestUefiRegion.MemoryLength));
          MemoryRegionInsert (LargestRegions, &InstalledRegions, &LargestUefiRegion, MaxGeneralRegions, CompareRegionSizeHighToLow);
        }

        // Save the new largest uefi region
        CopyMem (&LargestUefiRegion, &Region, sizeof (Region));
      } else {
        DEBUG ((DEBUG_VERBOSE, "DRAM Region: %016lx, %016lx\r\n", Region.MemoryBaseAddress, Region.MemoryLength));
        MemoryRegionInsert (LargestRegions, &InstalledRegions, &Region, MaxGeneralRegions, CompareRegionSizeHighToLow);
      }
    }

    // Update indexes and regions
    // Regions will be reduced to reflect processed info and indexes updated
    // if region is done in processing
    if (CarveoutStart >= DramEnd) {
      // Entire DRAM region was below the carveout, so we installed it above and need to move to the next one
      DramIndex++;
    } else if (CarveoutEnd > DramEnd) {
      // Carveout potentially carries over into the next DRAM region, so change carveout to be the leftover portion and check next DRAM region
      // Remove current dram region from carveout may be in other carveout
      CarveoutRegions[CarveoutIndex].MemoryBaseAddress = DramEnd;
      CarveoutRegions[CarveoutIndex].MemoryLength      = CarveoutEnd - DramEnd;
      DramIndex++;
    } else if (CarveoutEnd <= DramRegions[DramIndex].MemoryBaseAddress) {
      // Carveout is completely before DramRegion, so move on to next carveout
      CarveoutIndex++;
    } else if (CarveoutEnd < DramEnd) {
      // DRAM carries past carveout, so change dram region to have the excess, and check next Carveout
      // Reduce remaining size to what is after carveout
      DramRegions[DramIndex].MemoryBaseAddress = CarveoutEnd;
      DramRegions[DramIndex].MemoryLength      = DramEnd - CarveoutEnd;
      CarveoutIndex++;
    } else {
      // Both end at the same time, so move on to the next region for both
      // CarveOutEnd == DramEnd
      CarveoutIndex++;
      DramIndex++;
    }
  }

  // Add the largest UEFI region in the reserved space
  if (LargestUefiRegion.MemoryLength) {
    DEBUG ((DEBUG_VERBOSE, "DRAM Region [UEFI]: %016lx, %016lx\r\n", LargestUefiRegion.MemoryBaseAddress, LargestUefiRegion.MemoryLength));
    MemoryRegionInsert (LargestRegions, &InstalledRegions, &LargestUefiRegion, MAX_MEMORY_REGIONS, CompareRegionSizeHighToLow);
  }

  // Add the UsableCarveout regions in the reserved space
  for (UsableCarveoutIndex = 0; UsableCarveoutIndex < UsableCarveoutRegionsCount; UsableCarveoutIndex++) {
    DEBUG ((DEBUG_VERBOSE, "DRAM Region [Usable Carveout]: %016lx, %016lx\r\n", UsableCarveoutRegions[UsableCarveoutIndex].MemoryBaseAddress, UsableCarveoutRegions[UsableCarveoutIndex].MemoryLength));
    MemoryRegionInsert (LargestRegions, &InstalledRegions, &UsableCarveoutRegions[UsableCarveoutIndex], MAX_MEMORY_REGIONS, CompareRegionSizeHighToLow);
  }

  ResourceAttributes = (
                        EFI_RESOURCE_ATTRIBUTE_PRESENT |
                        EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
                        EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
                        EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
                        EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
                        EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
                        EFI_RESOURCE_ATTRIBUTE_READ_PROTECTABLE |
                        EFI_RESOURCE_ATTRIBUTE_WRITE_PROTECTABLE |
                        EFI_RESOURCE_ATTRIBUTE_EXECUTION_PROTECTABLE |
                        EFI_RESOURCE_ATTRIBUTE_READ_ONLY_PROTECTABLE
                        );

  if (PcdGet64 (PcdExpectedPeiMemoryUsage) == 0) {
    ResourceAttributes |= EFI_RESOURCE_ATTRIBUTE_TESTED;
  }

  // Now that we have the final list, install it in the HOB
  for (ListIndex = 0; ListIndex < InstalledRegions; ListIndex++) {
    BuildResourceDescriptorHob (
      EFI_RESOURCE_SYSTEM_MEMORY,
      ResourceAttributes,
      LargestRegions[ListIndex].MemoryBaseAddress,
      LargestRegions[ListIndex].MemoryLength
      );
  }

  MarkUsedMemoryTested ();

  // Specify the largest chunk of the UEFI DDR region that wasn't covered by carveouts
  *MaxRegionStart    = LargestUefiRegion.MemoryBaseAddress;
  *MaxRegionSize     = LargestUefiRegion.MemoryLength;
  *FinalRegionsCount = InstalledRegions;

  FreePool (DramRegions);
  FreePool (LargestRegions);
  return EFI_SUCCESS;
}
