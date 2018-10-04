/** @file
*
*  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
*  Copyright (c) 2013-2015, ARM Limited. All rights reserved.
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

#include <Library/ArmPlatformLib.h>
#include <Library/DebugLib.h>
#include <Pi/PiHob.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SystemResourceLib.h>

// DDR attributes
#define DDR_ATTRIBUTES_CACHED           ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK
#define DDR_ATTRIBUTES_UNCACHED         ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED

/**
  Return the Virtual Memory Map of your platform

  This Virtual Memory Map is used by MemoryInitPei Module to initialize the MMU on your platform.

  @param[out]   VirtualMemoryMap    Array of ARM_MEMORY_REGION_DESCRIPTOR describing a Physical-to-
                                    Virtual Memory mapping. This array must be ended by a zero-filled
                                    entry

**/
VOID
ArmPlatformGetVirtualMemoryMap (
  IN ARM_MEMORY_REGION_DESCRIPTOR** VirtualMemoryMap
  )
{
  UINTN                         Index = 0;
  ARM_MEMORY_REGION_DESCRIPTOR  *VirtualMemoryTable = NULL;
  UINTN                         ResourcesCount = 0;
  VOID                          *HobList = NULL;
  EFI_STATUS                    Status;

  ASSERT (VirtualMemoryMap != NULL);

  Status = InstallSystemResources (&ResourcesCount);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return;
  }

  ASSERT (ResourcesCount != 0);

  VirtualMemoryTable = (ARM_MEMORY_REGION_DESCRIPTOR*)AllocatePages(EFI_SIZE_TO_PAGES (sizeof(ARM_MEMORY_REGION_DESCRIPTOR) * (ResourcesCount + 1)));
  if (VirtualMemoryTable == NULL) {
    ASSERT (FALSE);
    return;
  }

  //Walk HOB list for resources
  HobList = GetHobList ();
  if (NULL == HobList) {
    ASSERT (FALSE);
    return;
  }

  HobList = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, HobList);
  while (NULL != HobList) {
    EFI_HOB_RESOURCE_DESCRIPTOR *Resource = (EFI_HOB_RESOURCE_DESCRIPTOR *)HobList;
    DEBUG ((EFI_D_VERBOSE,
        "ArmPlatformGetVirtualMemoryMap() Resource: Base: 0x%016lx, Size: 0x%016lx, Type: 0x%x\n",
        Resource->PhysicalStart,
        Resource->ResourceLength,
        Resource->ResourceType
      ));

    VirtualMemoryTable[Index].PhysicalBase    = Resource->PhysicalStart;
    VirtualMemoryTable[Index].VirtualBase     = Resource->PhysicalStart;
    VirtualMemoryTable[Index].Length          = Resource->ResourceLength;
    if (Resource->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) {
      VirtualMemoryTable[Index].Attributes      = DDR_ATTRIBUTES_CACHED;
    } else {
      VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
    }
    Index++;
    HobList = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, GET_NEXT_HOB (HobList));
  };

  // End of Table
  VirtualMemoryTable[Index].PhysicalBase    = 0;
  VirtualMemoryTable[Index].VirtualBase     = 0;
  VirtualMemoryTable[Index].Length          = 0;
  VirtualMemoryTable[Index].Attributes      = (ARM_MEMORY_REGION_ATTRIBUTES)0;

  ASSERT(Index == ResourcesCount);

  *VirtualMemoryMap = VirtualMemoryTable;
}
