/** @file
*
*  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2013-2015, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/ArmPlatformLib.h>
#include <Library/DebugLib.h>
#include <Pi/PiHob.h>
#include <Uefi/UefiSpec.h>
#include <Library/ArmMmuLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SystemResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>

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
  Adds DTB and BLParams to MMU

**/
EFI_STATUS
EFIAPI
MapCorePlatformMemory (
  VOID
  )
{
  UINTN               ChipID;
  NVDA_MEMORY_REGION  *DramPageBlacklistInfo;

  ArmSetMemoryAttributes (FixedPcdGet64 (PcdMiscRegBaseAddress), SIZE_4KB, EFI_MEMORY_UC);
  ChipID = TegraGetChipID ();
  ArmSetMemoryAttributes ((TegraGetBLInfoLocationAddress (ChipID) & ~EFI_PAGE_MASK), SIZE_4KB, EFI_MEMORY_UC);
  ArmSetMemoryAttributes (GetCPUBLBaseAddress (), SIZE_64KB, EFI_MEMORY_WB);
  ArmSetMemoryAttributes (GetDTBBaseAddress (), SIZE_64KB, EFI_MEMORY_WB);
  ArmSetMemoryAttributes ((UINTN)FixedPcdGet64 (PcdTegraCombinedUartRxMailbox), SIZE_4KB, EFI_MEMORY_UC);
  DramPageBlacklistInfo = GetDramPageBlacklistInfoAddress ();
  if (DramPageBlacklistInfo != NULL) {
    while (DramPageBlacklistInfo->MemoryBaseAddress != 0 &&
           DramPageBlacklistInfo->MemoryLength != 0)
    {
      ArmSetMemoryAttributes (DramPageBlacklistInfo->MemoryBaseAddress, DramPageBlacklistInfo->MemoryLength, EFI_MEMORY_WB);
      DramPageBlacklistInfo++;
    }
  }

  return EFI_SUCCESS;
}

/**
  Updates MMU mapping and relocates HOB to largest region

**/
VOID
UpdateMemoryMap (
  VOID
  )
{
  UINTN                 Index          = 0;
  UINTN                 ResourcesCount = 0;
  VOID                  *HobList       = NULL;
  EFI_PHYSICAL_ADDRESS  MaxRegionStart;
  UINTN                 MaxRegionSize;
  EFI_STATUS            Status;

  Status = InstallSystemResources (&ResourcesCount, &MaxRegionStart, &MaxRegionSize);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return;
  }

  ASSERT (ResourcesCount != 0);

  // Walk HOB list for resources
  HobList = GetHobList ();
  if (NULL == HobList) {
    ASSERT (FALSE);
    return;
  }

  HobList = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, HobList);
  while (NULL != HobList) {
    EFI_HOB_RESOURCE_DESCRIPTOR  *Resource = (EFI_HOB_RESOURCE_DESCRIPTOR *)HobList;
    DEBUG ((
      EFI_D_VERBOSE,
      "ArmPlatformGetVirtualMemoryMap() Resource: Base: 0x%016lx, Size: 0x%016lx, Type: 0x%x\n",
      Resource->PhysicalStart,
      Resource->ResourceLength,
      Resource->ResourceType
      ));

    if (Resource->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) {
      ArmSetMemoryAttributes (Resource->PhysicalStart, Resource->ResourceLength, EFI_MEMORY_WB);
    } else {
      ArmSetMemoryAttributes (Resource->PhysicalStart, Resource->ResourceLength, EFI_MEMORY_UC);
    }

    Index++;
    HobList = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, GET_NEXT_HOB (HobList));
  }

  ASSERT (Index == ResourcesCount);

  MigrateHobList (MaxRegionStart, MaxRegionSize);
}
