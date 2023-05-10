/** @file
*
*  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/**
  Migrate Hob list to the new region.

  @param[in] RegionStart              The region to migrate the HOB list to.
  @param[in] RegionSize               Region size of the new region.

  @retval EFI_SUCCESS                 Hob list moved.
  @return others                      Other error.
**/
STATIC
EFI_STATUS
MigrateHobList (
  IN EFI_PHYSICAL_ADDRESS  RegionStart,
  IN UINTN                 RegionSize
  )
{
  EFI_HOB_HANDOFF_INFO_TABLE  *OldHob;
  EFI_PHYSICAL_ADDRESS        OldHobAddress;
  EFI_PHYSICAL_ADDRESS        RegionEndAddress;
  EFI_HOB_HANDOFF_INFO_TABLE  *NewHob;
  EFI_HOB_MEMORY_ALLOCATION   *Allocation;

  OldHob        = (EFI_HOB_HANDOFF_INFO_TABLE *)PrePeiGetHobList ();
  OldHobAddress = (EFI_PHYSICAL_ADDRESS)OldHob;

  ASSERT (OldHob->EfiFreeMemoryBottom > OldHobAddress);
  ASSERT (OldHob->EfiFreeMemoryTop >= OldHob->EfiFreeMemoryBottom);
  ASSERT (OldHob->EfiEndOfHobList > OldHobAddress);

  if ((OldHobAddress >= RegionStart) &&
      (OldHobAddress < (RegionStart + RegionSize)))
  {
    // Hob list is already in the correct region, check if memory at end of region is larger
    if ((RegionStart + RegionSize - OldHob->EfiMemoryTop) > (OldHob->EfiFreeMemoryTop - OldHobAddress)) {
      RegionSize  = RegionStart + RegionSize - OldHob->EfiMemoryTop;
      RegionStart = OldHob->EfiMemoryTop;
    } else {
      // Free area is smaller then current, do not move
      return EFI_SUCCESS;
    }
  }

  RegionEndAddress = RegionStart + RegionSize;
  // Filter out any prior allocations
  Allocation = (EFI_HOB_MEMORY_ALLOCATION *)(VOID *)OldHob;
  while ((Allocation = (EFI_HOB_MEMORY_ALLOCATION *)GetNextHob (EFI_HOB_TYPE_MEMORY_ALLOCATION, GET_NEXT_HOB (Allocation))) != NULL) {
    if ((Allocation->AllocDescriptor.MemoryBaseAddress >= RegionStart) &&
        (Allocation->AllocDescriptor.MemoryBaseAddress < (RegionStart + RegionSize)))
    {
      EFI_PHYSICAL_ADDRESS  EndAddress = Allocation->AllocDescriptor.MemoryBaseAddress + Allocation->AllocDescriptor.MemoryLength;
      if ((Allocation->AllocDescriptor.MemoryBaseAddress - RegionStart) > (RegionEndAddress - EndAddress)) {
        RegionSize       = Allocation->AllocDescriptor.MemoryBaseAddress - RegionStart;
        RegionEndAddress = RegionStart + RegionSize;
      } else {
        RegionStart = EndAddress;
        RegionSize  = RegionEndAddress - EndAddress;
      }
    }
  }

  ASSERT (RegionSize != 0);

  if ((RegionStart + RegionSize - OldHob->EfiMemoryTop) <= (OldHob->EfiFreeMemoryTop - OldHobAddress)) {
    // Free area is smaller then current, do not move
    return EFI_SUCCESS;
  }

  // Move Hob list to take full region
  NewHob = (VOID *)RegionStart;
  CopyMem (NewHob, OldHob, OldHob->EfiFreeMemoryBottom - OldHobAddress);
  NewHob->EfiEndOfHobList     = RegionStart + OldHob->EfiEndOfHobList - OldHobAddress;
  NewHob->EfiFreeMemoryBottom = RegionStart + OldHob->EfiFreeMemoryBottom - OldHobAddress;
  NewHob->EfiFreeMemoryTop    = RegionStart + RegionSize;
  NewHob->EfiMemoryBottom     = RegionStart;
  NewHob->EfiMemoryTop        = RegionStart + RegionSize;

  PrePeiSetHobList ((VOID *)NewHob);
  return EFI_SUCCESS;
}

/**
  Simple insertion sort to sort regions entries in ascending order.

  @param Regions [IN, OUT]    Array of regions to sort
  @param RegionsCount [IN]    Number of regions in array
**/
STATIC
VOID
MemoryRegionSort (
  IN OUT NVDA_MEMORY_REGION  *Regions,
  IN UINTN                   RegionsCount
  )
{
  EFI_PHYSICAL_ADDRESS  Address   = 0;
  UINTN                 Length    = 0;
  INT32                 PrevIndex = 0;
  INT32                 Index     = 0;

  for (Index = 1; Index < RegionsCount; Index++) {
    PrevIndex = Index - 1;
    Address   = Regions[Index].MemoryBaseAddress;
    Length    = Regions[Index].MemoryLength;

    while ((PrevIndex >= 0) && (Regions[PrevIndex].MemoryBaseAddress > Address)) {
      Regions[PrevIndex + 1].MemoryBaseAddress = Regions[PrevIndex].MemoryBaseAddress;
      Regions[PrevIndex + 1].MemoryLength      = Regions[PrevIndex].MemoryLength;
      PrevIndex                               -= 1;
    }

    Regions[PrevIndex + 1].MemoryBaseAddress = Address;
    Regions[PrevIndex + 1].MemoryLength      = Length;
  }
}

/**
  Installs DRAM regions into the HOB list

  This function installs the specified DRAM regions into HOB list while
  removing the carveout regions.
  This function is called by the platform memory initialization library.

  @param  InputDramRegions              Sorted list of available DRAM regions
  @param  DramRegionsCount         Number of regions in DramRegions.
  @param  UefiDramRegionIndex      Index of uefi usable regions in DramRegions.
  @param  CarveoutRegions          Sorted list of carveout regions that will be
                                   removed from DramRegions.
  @param  CarveoutRegionsCount     Number of regions in CarveoutRegions.
  @param  FinalRegionsCount        Number of regions installed into HOB list.

  @retval EFI_SUCCESS              Resources have been installed
  @retval EFI_DEVICE_ERROR         Error setting up memory

**/
EFI_STATUS
InstallDramWithCarveouts (
  IN  CONST NVDA_MEMORY_REGION  *InputDramRegions,
  IN  UINTN                     DramRegionsCount,
  IN  UINTN                     UefiDramRegionIndex,
  IN  NVDA_MEMORY_REGION        *CarveoutRegions,
  IN  UINTN                     CarveoutRegionsCount,
  OUT UINTN                     *FinalRegionsCount
  )
{
  NVDA_MEMORY_REGION           *DramRegions       = NULL;
  UINTN                        DramIndex          = 0;
  UINTN                        CarveoutIndex      = 0;
  UINTN                        InstalledRegions   = 0;
  EFI_PHYSICAL_ADDRESS         LargestRegionStart = 0;
  UINTN                        MaxSize            = 0;
  EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttributes;
  EFI_PHYSICAL_ADDRESS         CarveoutStart;
  EFI_PHYSICAL_ADDRESS         CarveoutEnd;
  EFI_PHYSICAL_ADDRESS         DramEnd;
  UINTN                        RegionSize;

  DramRegions = AllocatePool (sizeof (NVDA_MEMORY_REGION) * DramRegionsCount);
  ASSERT (DramRegions != NULL);
  if (DramRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  CopyMem (DramRegions, InputDramRegions, sizeof (NVDA_MEMORY_REGION) * DramRegionsCount);

  MemoryRegionSort (DramRegions, DramRegionsCount);
  for (DramIndex = 0; DramIndex < DramRegionsCount; DramIndex++) {
    DEBUG ((
      EFI_D_VERBOSE,
      "InstallDramWithCarveouts() Dram Region: Base: 0x%016lx, Size: 0x%016lx\n",
      DramRegions[DramIndex].MemoryBaseAddress,
      DramRegions[DramIndex].MemoryLength
      ));
  }

  DramIndex = 0;

  MemoryRegionSort (CarveoutRegions, CarveoutRegionsCount);
  for (CarveoutIndex = 0; CarveoutIndex < CarveoutRegionsCount; CarveoutIndex++) {
    DEBUG ((
      EFI_D_VERBOSE,
      "InstallDramWithCarveouts() Carveout Region: Base: 0x%016lx, Size: 0x%016lx\n",
      CarveoutRegions[CarveoutIndex].MemoryBaseAddress,
      CarveoutRegions[CarveoutIndex].MemoryLength
      ));
  }

  CarveoutIndex = 0;

  ResourceAttributes = (
                        EFI_RESOURCE_ATTRIBUTE_PRESENT |
                        EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
                        EFI_RESOURCE_ATTRIBUTE_TESTED |
                        EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
                        EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
                        EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
                        EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
                        EFI_RESOURCE_ATTRIBUTE_READ_PROTECTABLE |
                        EFI_RESOURCE_ATTRIBUTE_WRITE_PROTECTABLE |
                        EFI_RESOURCE_ATTRIBUTE_EXECUTION_PROTECTABLE |
                        EFI_RESOURCE_ATTRIBUTE_READ_ONLY_PROTECTABLE
                        );

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
    if (DramRegions[DramIndex].MemoryBaseAddress < CarveoutStart) {
      if (CarveoutStart < DramEnd) {
        RegionSize = CarveoutStart - DramRegions[DramIndex].MemoryBaseAddress;
      } else {
        RegionSize = DramRegions[DramIndex].MemoryLength;
      }

      DEBUG ((DEBUG_VERBOSE, "DRAM Region: %016lx, %016lx\r\n", DramRegions[DramIndex].MemoryBaseAddress, RegionSize));
      BuildResourceDescriptorHob (
        EFI_RESOURCE_SYSTEM_MEMORY,
        ResourceAttributes,
        DramRegions[DramIndex].MemoryBaseAddress,
        RegionSize
        );

      if ((DramIndex == UefiDramRegionIndex) &&
          (RegionSize > MaxSize))
      {
        LargestRegionStart = DramRegions[DramIndex].MemoryBaseAddress;
        MaxSize            = RegionSize;
      }

      InstalledRegions++;
    }

    // Update indexes and regions
    // Regions will be reduced to reflect processed info and indexes updated
    // if region is done in processing
    if (CarveoutStart >= DramEnd) {
      DramIndex++;
    } else if (CarveoutEnd > DramEnd) {
      // Remove current dram region from carveout may be in other carveout
      CarveoutRegions[CarveoutIndex].MemoryBaseAddress = DramEnd;
      CarveoutRegions[CarveoutIndex].MemoryLength      = CarveoutEnd - DramEnd;
      DramIndex++;
    } else if (CarveoutEnd <= DramRegions[DramIndex].MemoryBaseAddress) {
      // Carveout is completely before DramRegion
      CarveoutIndex++;
    } else if (CarveoutEnd < DramEnd) {
      // Reduce remaining size to what is after carveout
      DramRegions[DramIndex].MemoryBaseAddress = CarveoutEnd;
      DramRegions[DramIndex].MemoryLength      = DramEnd - CarveoutEnd;
      CarveoutIndex++;
    } else {
      // CarveOutEnd == DramEnd
      CarveoutIndex++;
      DramIndex++;
    }
  }

  MigrateHobList (LargestRegionStart, MaxSize);
  *FinalRegionsCount = InstalledRegions;
  return EFI_SUCCESS;
}
