/** @file
*
*  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>
#include <Library/DramCarveoutLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Pi/PiHob.h>
#include <Library/SortLib.h>


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
      BuildResourceDescriptorHob (
        EFI_RESOURCE_SYSTEM_MEMORY,
        ResourceAttributes,
        DramRegions[DramIndex].MemoryBaseAddress,
        DramRegions[DramIndex].MemoryLength
      );

      DramIndex++;
      InstalledRegions++;
    } else {
      UINTN CarveoutEnd = CarveoutRegions[CarveoutIndex].MemoryBaseAddress + CarveoutRegions[CarveoutIndex].MemoryLength;
      UINTN DramEnd = DramRegions[DramIndex].MemoryBaseAddress + DramRegions[DramIndex].MemoryLength;
      if (DramRegions[DramIndex].MemoryBaseAddress < CarveoutRegions[CarveoutIndex].MemoryBaseAddress) {
        BuildResourceDescriptorHob (
                EFI_RESOURCE_SYSTEM_MEMORY,
                ResourceAttributes,
                DramRegions[DramIndex].MemoryBaseAddress,
                CarveoutRegions[CarveoutIndex].MemoryBaseAddress - DramRegions[DramIndex].MemoryBaseAddress
              );
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

  *FinalRegionsCount = InstalledRegions;
  return EFI_SUCCESS;
}
