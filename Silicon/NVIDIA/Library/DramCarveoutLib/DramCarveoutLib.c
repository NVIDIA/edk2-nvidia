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
#include <Library/SortLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Guid/MemoryTypeInformation.h>

/**
  Prototype for comparison function for any two element types.

  @param[in] Buffer1                  The pointer to first buffer.
  @param[in] Buffer2                  The pointer to second buffer.

  @retval 0                           Buffer1 equal to Buffer2.
  @return <0                          Buffer1 is less than Buffer2.
  @return >0                          Buffer1 is greater than Buffer2.
**/
STATIC
INTN
MemoryRegionCompare (
  IN CONST VOID  *Buffer1,
  IN CONST VOID  *Buffer2
  )
{
  NVDA_MEMORY_REGION  *Region1 = (NVDA_MEMORY_REGION *)Buffer1;
  NVDA_MEMORY_REGION  *Region2 = (NVDA_MEMORY_REGION *)Buffer2;

  if (Region1->MemoryBaseAddress == Region2->MemoryBaseAddress) {
    return 0;
  } else if (Region1->MemoryBaseAddress < Region2->MemoryBaseAddress) {
    return -1;
  } else {
    return 1;
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
  IN  UINTN                     UefiDramRegionsCount,
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

  DramRegions = AllocatePool (sizeof (NVDA_MEMORY_REGION) * DramRegionsCount);
  ASSERT (DramRegions != NULL);
  if (DramRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  CopyMem (DramRegions, InputDramRegions, sizeof (NVDA_MEMORY_REGION) * DramRegionsCount);

  PerformQuickSort (
    (VOID *)DramRegions,
    DramRegionsCount,
    sizeof (NVDA_MEMORY_REGION),
    MemoryRegionCompare
    );
  for (DramIndex = 0; DramIndex < DramRegionsCount; DramIndex++) {
    DEBUG ((
      EFI_D_ERROR,
      "InstallDramWithCarveouts() Dram Region: Base: 0x%016lx, Size: 0x%016lx\n",
      DramRegions[DramIndex].MemoryBaseAddress,
      DramRegions[DramIndex].MemoryLength
      ));
  }

  DramIndex = 0;

  PerformQuickSort (
    (VOID *)CarveoutRegions,
    CarveoutRegionsCount,
    sizeof (NVDA_MEMORY_REGION),
    MemoryRegionCompare
    );
  for (CarveoutIndex = 0; CarveoutIndex < CarveoutRegionsCount; CarveoutIndex++) {
    DEBUG ((
      EFI_D_ERROR,
      "InstallDramWithCarveouts() Carveout Region: Base: 0x%016lx, Size: 0x%016lx\n",
      CarveoutRegions[CarveoutIndex].MemoryBaseAddress,
      CarveoutRegions[CarveoutIndex].MemoryLength
      ));
  }

  CarveoutIndex = 0;

  PerformQuickSort (
    (VOID *)UsableCarveoutRegions,
    UsableCarveoutRegionsCount,
    sizeof (NVDA_MEMORY_REGION),
    MemoryRegionCompare
    );
  for (UsableCarveoutIndex = 0; UsableCarveoutIndex < UsableCarveoutRegionsCount; UsableCarveoutIndex++) {
    DEBUG ((
      EFI_D_ERROR,
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

  while (DramIndex < UefiDramRegionsCount) {
    // No more carveouts or carveout is after dram region
    if ((CarveoutRegionsCount == CarveoutIndex) ||
        ((DramRegions[DramIndex].MemoryBaseAddress + DramRegions[DramIndex].MemoryLength) <= CarveoutRegions[CarveoutIndex].MemoryBaseAddress))
    {
      if (DramRegions[DramIndex].MemoryLength > MaxSize) {
        if (LargestRegionStart != 0) {
          DEBUG ((DEBUG_ERROR, "DRAM Region: %016lx, %016lx\r\n", LargestRegionStart, MaxSize));
          BuildResourceDescriptorHob (
            EFI_RESOURCE_SYSTEM_MEMORY,
            ResourceAttributes,
            LargestRegionStart,
            MaxSize
            );
        }

        LargestRegionStart = DramRegions[DramIndex].MemoryBaseAddress;
        MaxSize            = DramRegions[DramIndex].MemoryLength;
      } else {
        DEBUG ((DEBUG_ERROR, "DRAM Region: %016lx, %016lx\r\n", DramRegions[DramIndex].MemoryBaseAddress, DramRegions[DramIndex].MemoryLength));
        BuildResourceDescriptorHob (
          EFI_RESOURCE_SYSTEM_MEMORY,
          ResourceAttributes,
          DramRegions[DramIndex].MemoryBaseAddress,
          DramRegions[DramIndex].MemoryLength
          );
      }

      DramIndex++;
      InstalledRegions++;
    } else {
      EFI_PHYSICAL_ADDRESS  CarveoutEnd = CarveoutRegions[CarveoutIndex].MemoryBaseAddress + CarveoutRegions[CarveoutIndex].MemoryLength;
      EFI_PHYSICAL_ADDRESS  DramEnd     = DramRegions[DramIndex].MemoryBaseAddress + DramRegions[DramIndex].MemoryLength;
      if (DramRegions[DramIndex].MemoryBaseAddress < CarveoutRegions[CarveoutIndex].MemoryBaseAddress) {
        if ((CarveoutRegions[CarveoutIndex].MemoryBaseAddress - DramRegions[DramIndex].MemoryBaseAddress) > MaxSize) {
          if (LargestRegionStart != 0) {
            DEBUG ((DEBUG_ERROR, "DRAM Region: %016lx, %016lx\r\n", LargestRegionStart, MaxSize));
            BuildResourceDescriptorHob (
              EFI_RESOURCE_SYSTEM_MEMORY,
              ResourceAttributes,
              LargestRegionStart,
              MaxSize
              );
          }

          LargestRegionStart = DramRegions[DramIndex].MemoryBaseAddress;
          MaxSize            = CarveoutRegions[CarveoutIndex].MemoryBaseAddress - DramRegions[DramIndex].MemoryBaseAddress;
        } else {
          DEBUG ((DEBUG_ERROR, "DRAM Region: %016lx, %016lx\r\n", DramRegions[DramIndex].MemoryBaseAddress, CarveoutRegions[CarveoutIndex].MemoryBaseAddress - DramRegions[DramIndex].MemoryBaseAddress));
          BuildResourceDescriptorHob (
            EFI_RESOURCE_SYSTEM_MEMORY,
            ResourceAttributes,
            DramRegions[DramIndex].MemoryBaseAddress,
            CarveoutRegions[CarveoutIndex].MemoryBaseAddress - DramRegions[DramIndex].MemoryBaseAddress
            );
        }

        InstalledRegions++;
      }

      if (CarveoutEnd > DramEnd) {
        // Move carveout end
        CarveoutRegions[CarveoutIndex].MemoryBaseAddress = DramEnd;
        CarveoutRegions[CarveoutIndex].MemoryLength      = CarveoutEnd - DramEnd;
        DramIndex++;
      } else if (CarveoutEnd <= DramRegions[DramIndex].MemoryBaseAddress) {
        CarveoutIndex++;
      } else if (CarveoutEnd < DramEnd) {
        DramRegions[DramIndex].MemoryBaseAddress = CarveoutEnd;
        DramRegions[DramIndex].MemoryLength      = DramEnd - CarveoutEnd;
        CarveoutIndex++;
      } else {
        CarveoutIndex++;
        DramIndex++;
      }
    }
  }

  while (DramIndex < DramRegionsCount) {
    BuildResourceDescriptorHob (
      EFI_RESOURCE_SYSTEM_MEMORY,
      ResourceAttributes,
      DramRegions[DramIndex].MemoryBaseAddress,
      DramRegions[DramIndex].MemoryLength
      );
    InstalledRegions++;
    DramIndex++;
  }

  BuildResourceDescriptorHob (
    EFI_RESOURCE_SYSTEM_MEMORY,
    ResourceAttributes,
    LargestRegionStart,
    MaxSize
    );

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
