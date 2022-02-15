/** @file
*
*  Copyright (c) 2018-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

  OldHob = (EFI_HOB_HANDOFF_INFO_TABLE *)PrePeiGetHobList ();
  OldHobAddress = (EFI_PHYSICAL_ADDRESS)OldHob;

  ASSERT (OldHob->EfiFreeMemoryBottom > OldHobAddress);
  ASSERT (OldHob->EfiFreeMemoryTop >= OldHob->EfiFreeMemoryBottom);
  ASSERT (OldHob->EfiEndOfHobList > OldHobAddress);

  if ((OldHobAddress >= RegionStart) &&
      (OldHobAddress < (RegionStart + RegionSize))) {
    //Hob list is already in the correct region, check if memory at end of region is larger
    if ((RegionStart + RegionSize - OldHob->EfiMemoryTop) > (OldHob->EfiFreeMemoryTop - OldHobAddress)) {
      RegionSize = RegionStart + RegionSize - OldHob->EfiMemoryTop;
      RegionStart = OldHob->EfiMemoryTop;
    } else {
      //Free area is smaller then current, do not move
      return EFI_SUCCESS;
    }
  }

  RegionEndAddress = RegionStart + RegionSize;
  //Filter out any prior allocations
  Allocation = (EFI_HOB_MEMORY_ALLOCATION *)(VOID *)OldHob;
  while ((Allocation = (EFI_HOB_MEMORY_ALLOCATION *)GetNextHob (EFI_HOB_TYPE_MEMORY_ALLOCATION, GET_NEXT_HOB (Allocation))) != NULL) {
    if ((Allocation->AllocDescriptor.MemoryBaseAddress >= RegionStart) &&
        (Allocation->AllocDescriptor.MemoryBaseAddress < (RegionStart + RegionSize))) {
      EFI_PHYSICAL_ADDRESS EndAddress = Allocation->AllocDescriptor.MemoryBaseAddress + Allocation->AllocDescriptor.MemoryLength;
      if ((Allocation->AllocDescriptor.MemoryBaseAddress - RegionStart) > (RegionEndAddress - EndAddress)) {
        RegionSize = Allocation->AllocDescriptor.MemoryBaseAddress - RegionStart;
        RegionEndAddress = RegionStart + RegionSize;
      } else {
        RegionStart = EndAddress;
        RegionSize = RegionEndAddress - EndAddress;
      }
    }
  }

  ASSERT (RegionSize != 0);

  if ((RegionStart + RegionSize - OldHob->EfiMemoryTop) <= (OldHob->EfiFreeMemoryTop - OldHobAddress)) {
    //Free area is smaller then current, do not move
    return EFI_SUCCESS;
  }

  //Move Hob list to take full region
  NewHob = (VOID *)RegionStart;
  CopyMem (NewHob, OldHob, OldHob->EfiFreeMemoryBottom - OldHobAddress);
  NewHob->EfiEndOfHobList = RegionStart + OldHob->EfiEndOfHobList - OldHobAddress;
  NewHob->EfiFreeMemoryBottom = RegionStart + OldHob->EfiFreeMemoryBottom - OldHobAddress;
  NewHob->EfiFreeMemoryTop = RegionStart + RegionSize;
  NewHob->EfiMemoryBottom = RegionStart;
  NewHob->EfiMemoryTop = RegionStart + RegionSize;

  PrePeiSetHobList ((VOID *)NewHob);
  return EFI_SUCCESS;
}


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
  IN CONST VOID                 *Buffer1,
  IN CONST VOID                 *Buffer2
)
{
  NVDA_MEMORY_REGION *Region1 = (NVDA_MEMORY_REGION *)Buffer1;
  NVDA_MEMORY_REGION *Region2 = (NVDA_MEMORY_REGION *)Buffer2;
  if (Region1->MemoryBaseAddress == Region2->MemoryBaseAddress) {
    return 0;
  } else if (Region1->MemoryBaseAddress < Region2->MemoryBaseAddress) {
    return -1;
  } else {
    return 1;
  }
}

/**
  Installs DRAM regions into the HOB list

  This function installs the specified DRAM regions into HOB list while
  removing the carveout regions.
  This function is called by the platform memory initialization library.

  @param  DramRegions              Sorted list of available DRAM regions
  @param  DramRegionsCount         Number of regions in DramRegions.
  @param  CarveoutRegions          Sorted list of carveout regions that will be
                                   removed from DramRegions.
  @param  CarveoutRegionsCount     Number of regions in CarveoutRegions.
  @param  FinalRegionsCount        Number of regions installed into HOB list.

  @retval EFI_SUCCESS              Resources have been installed
  @retval EFI_DEVICE_ERROR         Error setting up memory

**/
EFI_STATUS
InstallDramWithCarveouts (
  IN  NVDA_MEMORY_REGION *DramRegions,
  IN  UINTN              DramRegionsCount,
  IN  NVDA_MEMORY_REGION *CarveoutRegions,
  IN  UINTN              CarveoutRegionsCount,
  OUT UINTN              *FinalRegionsCount
)
{
  UINTN DramIndex = 0;
  UINTN CarveoutIndex = 0;
  UINTN InstalledRegions = 0;
  EFI_PHYSICAL_ADDRESS LargestRegionStart = 0;
  UINTN MaxSize = 0;
  EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttributes;

  PerformQuickSort (
    (VOID *)DramRegions,
    DramRegionsCount,
    sizeof (NVDA_MEMORY_REGION),
    MemoryRegionCompare
  );
  for (DramIndex = 0; DramIndex < DramRegionsCount; DramIndex++) {
    DEBUG ((EFI_D_VERBOSE,
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
    DEBUG ((EFI_D_VERBOSE,
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
    //No more carveouts or carveout is after dram region
    if ((CarveoutRegionsCount == CarveoutIndex) ||
        ((DramRegions[DramIndex].MemoryBaseAddress + DramRegions[DramIndex].MemoryLength) <= CarveoutRegions[CarveoutIndex].MemoryBaseAddress)) {
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
        MaxSize = DramRegions[DramIndex].MemoryLength;
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
      EFI_PHYSICAL_ADDRESS CarveoutEnd = CarveoutRegions[CarveoutIndex].MemoryBaseAddress + CarveoutRegions[CarveoutIndex].MemoryLength;
      EFI_PHYSICAL_ADDRESS DramEnd = DramRegions[DramIndex].MemoryBaseAddress + DramRegions[DramIndex].MemoryLength;
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
          MaxSize = CarveoutRegions[CarveoutIndex].MemoryBaseAddress - DramRegions[DramIndex].MemoryBaseAddress;
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
        //Move carveout end
        CarveoutRegions[CarveoutIndex].MemoryBaseAddress = DramEnd;
        CarveoutRegions[CarveoutIndex].MemoryLength = CarveoutEnd - DramEnd;
        DramIndex++;
      } else if (CarveoutEnd <= DramRegions[DramIndex].MemoryBaseAddress) {
        CarveoutIndex++;
      } else if (CarveoutEnd < DramEnd) {
        DramRegions[DramIndex].MemoryBaseAddress = CarveoutEnd;
        DramRegions[DramIndex].MemoryLength  = DramEnd - CarveoutEnd;
        CarveoutIndex++;
      } else {
        CarveoutIndex++;
        DramIndex++;
      }
    }
  }

  BuildResourceDescriptorHob (
    EFI_RESOURCE_SYSTEM_MEMORY,
    ResourceAttributes,
    LargestRegionStart,
    MaxSize
  );

  MigrateHobList (LargestRegionStart, MaxSize);
  *FinalRegionsCount = InstalledRegions;
  return EFI_SUCCESS;
}
