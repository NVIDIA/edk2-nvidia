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
#include <Guid/MemoryTypeInformation.h>

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

  @param  InputDramRegions            Sorted list of available DRAM regions
  @param  DramRegionsCount            Number of regions in DramRegions.
  @param  UefiDramRegionIndex         Index of uefi usable regions in DramRegions.
  @param  CarveoutRegions             Sorted list of carveout regions that will be
                                      removed from DramRegions.
  @param  CarveoutRegionsCount        Number of regions in CarveoutRegions.
  @param  UsableCarveoutRegions       Sorted list of usable carveout regions that will be
                                      added to DramRegions.
  @param  UsableCarveoutRegionsCount  Number of regions in UsableCarveoutRegions.
  @param  FinalRegionsCount           Number of regions installed into HOB list.
  @param  MaxRegionStart              Base address of largest region in dram
  @param  MaxRegionSize               Size of largest region

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
  NVDA_MEMORY_REGION           *DramRegions        = NULL;
  UINTN                        DramIndex           = 0;
  UINTN                        CarveoutIndex       = 0;
  UINTN                        UsableCarveoutIndex = 0;
  UINTN                        InstalledRegions    = 0;
  EFI_PHYSICAL_ADDRESS         LargestRegionStart  = 0;
  UINTN                        MaxSize             = 0;
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
      DEBUG_VERBOSE,
      "InstallDramWithCarveouts() Dram Region: Base: 0x%016lx, Size: 0x%016lx\n",
      DramRegions[DramIndex].MemoryBaseAddress,
      DramRegions[DramIndex].MemoryLength
      ));
  }

  DramIndex = 0;

  MemoryRegionSort (CarveoutRegions, CarveoutRegionsCount);
  for (CarveoutIndex = 0; CarveoutIndex < CarveoutRegionsCount; CarveoutIndex++) {
    DEBUG ((
      DEBUG_VERBOSE,
      "InstallDramWithCarveouts() Carveout Region: Base: 0x%016lx, Size: 0x%016lx\n",
      CarveoutRegions[CarveoutIndex].MemoryBaseAddress,
      CarveoutRegions[CarveoutIndex].MemoryLength
      ));
  }

  CarveoutIndex = 0;

  MemoryRegionSort (UsableCarveoutRegions, UsableCarveoutRegionsCount);
  for (UsableCarveoutIndex = 0; UsableCarveoutIndex < UsableCarveoutRegionsCount; UsableCarveoutIndex++) {
    DEBUG ((
      DEBUG_VERBOSE,
      "InstallDramWithCarveouts() Usable Carveout Region: Base: 0x%016lx, Size: 0x%016lx\n",
      UsableCarveoutRegions[UsableCarveoutIndex].MemoryBaseAddress,
      UsableCarveoutRegions[UsableCarveoutIndex].MemoryLength
      ));
  }

  UsableCarveoutIndex = 0;

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

  for (UsableCarveoutIndex = 0; UsableCarveoutIndex < UsableCarveoutRegionsCount; UsableCarveoutIndex++) {
    BuildResourceDescriptorHob (
      EFI_RESOURCE_SYSTEM_MEMORY,
      ResourceAttributes,
      UsableCarveoutRegions[UsableCarveoutIndex].MemoryBaseAddress,
      UsableCarveoutRegions[UsableCarveoutIndex].MemoryLength
      );

    InstalledRegions++;
  }

  MarkUsedMemoryTested ();

  *MaxRegionStart    = LargestRegionStart;
  *MaxRegionSize     = MaxSize;
  *FinalRegionsCount = InstalledRegions;
  return EFI_SUCCESS;
}
