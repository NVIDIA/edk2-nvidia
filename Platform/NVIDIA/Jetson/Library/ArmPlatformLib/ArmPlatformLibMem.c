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
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>

#include "ArmPlatform.h"

// The total number of descriptors, including the final "end-of-table" descriptor.
#define MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS 16

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
  ARM_MEMORY_REGION_ATTRIBUTES  CacheAttributes;
  UINTN                         Index = 0;
  ARM_MEMORY_REGION_DESCRIPTOR  *VirtualMemoryTable;
  EFI_RESOURCE_ATTRIBUTE_TYPE   ResourceAttributes;

  ASSERT (VirtualMemoryMap != NULL);

  //
  // Declared the additional 6GB of memory
  //
  ResourceAttributes =
      EFI_RESOURCE_ATTRIBUTE_PRESENT |
      EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
      EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
      EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
      EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
      EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
      EFI_RESOURCE_ATTRIBUTE_TESTED;

// Disable additional memory resources until carveout support is added.

//  BuildResourceDescriptorHob (
//    EFI_RESOURCE_SYSTEM_MEMORY,
//    ResourceAttributes,
//    ARM_GALEN_EXTRA_SYSTEM_MEMORY_BASE,
//    ARM_GALEN_EXTRA_SYSTEM_MEMORY_SZ);

  VirtualMemoryTable = (ARM_MEMORY_REGION_DESCRIPTOR*)AllocatePages(EFI_SIZE_TO_PAGES (sizeof(ARM_MEMORY_REGION_DESCRIPTOR) * MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS));
  if (VirtualMemoryTable == NULL) {
      return;
  }

  CacheAttributes = DDR_ATTRIBUTES_CACHED;

  // Galen OnChip peripherals
  VirtualMemoryTable[Index].PhysicalBase    = ARM_GALEN_PERIPHERALS_BASE;
  VirtualMemoryTable[Index].VirtualBase     = ARM_GALEN_PERIPHERALS_BASE;
  VirtualMemoryTable[Index].Length          = ARM_GALEN_PERIPHERALS_SZ;
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Galen SOC peripherals
  VirtualMemoryTable[++Index].PhysicalBase  = ARM_GALEN_SOC_PERIPHERALS_BASE;
  VirtualMemoryTable[Index].VirtualBase     = ARM_GALEN_SOC_PERIPHERALS_BASE;
  VirtualMemoryTable[Index].Length          = ARM_GALEN_SOC_PERIPHERALS_SZ;
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // DDR - 2GB
  VirtualMemoryTable[++Index].PhysicalBase  = PcdGet64 (PcdSystemMemoryBase);
  VirtualMemoryTable[Index].VirtualBase     = PcdGet64 (PcdSystemMemoryBase);
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdSystemMemorySize);
  VirtualMemoryTable[Index].Attributes      = CacheAttributes;

// Disable additional memory resources until carveout support is added.

//  // DDR - 6GB
//  VirtualMemoryTable[++Index].PhysicalBase  = ARM_GALEN_EXTRA_SYSTEM_MEMORY_BASE;
//  VirtualMemoryTable[Index].VirtualBase     = ARM_GALEN_EXTRA_SYSTEM_MEMORY_BASE;
//  VirtualMemoryTable[Index].Length          = ARM_GALEN_EXTRA_SYSTEM_MEMORY_SZ;
//  VirtualMemoryTable[Index].Attributes      = CacheAttributes;

  // End of Table
  VirtualMemoryTable[++Index].PhysicalBase  = 0;
  VirtualMemoryTable[Index].VirtualBase     = 0;
  VirtualMemoryTable[Index].Length          = 0;
  VirtualMemoryTable[Index].Attributes      = (ARM_MEMORY_REGION_ATTRIBUTES)0;

  ASSERT((Index + 1) <= MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS);

  *VirtualMemoryMap = VirtualMemoryTable;
}
