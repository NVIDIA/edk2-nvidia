/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2013-2015, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/ArmPlatformLib.h>
#include <Library/NVIDIADebugLib.h>
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
#include <Guid/MemoryTypeInformation.h>

// Initial memory needed for GCD
#define MINIMUM_INITIAL_MEMORY_SIZE  SIZE_64KB

/**
  Calculate total memory bin size neeeded.

  @return The total memory bin size neeeded.

**/
UINT64
CalculateTotalMemoryBinSizeNeeded (
  VOID
  )
{
  UINTN                        Index;
  UINT64                       TotalSize;
  EFI_HOB_GUID_TYPE            *GuidHob;
  EFI_MEMORY_TYPE_INFORMATION  *EfiMemoryTypeInformation;
  UINTN                        DataSize;

  TotalSize = 0;
  //
  // Loop through each memory type in the order specified by the gMemoryTypeInformation[] array
  //

  GuidHob = GetFirstGuidHob (&gEfiMemoryTypeInformationGuid);
  if (GuidHob != NULL) {
    EfiMemoryTypeInformation = GET_GUID_HOB_DATA (GuidHob);
    DataSize                 = GET_GUID_HOB_DATA_SIZE (GuidHob);
    if ((EfiMemoryTypeInformation != NULL) && (DataSize > 0) && (DataSize <= (EfiMaxMemoryType + 1) * sizeof (EFI_MEMORY_TYPE_INFORMATION))) {
      TotalSize = 0;
      for (Index = 0; EfiMemoryTypeInformation[Index].Type != EfiMaxMemoryType; Index++) {
        TotalSize += EFI_PAGES_TO_SIZE (EfiMemoryTypeInformation[Index].NumberOfPages);
      }
    }
  }

  return TotalSize;
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
  EFI_HOB_HANDOFF_INFO_TABLE   *OldHob;
  EFI_PHYSICAL_ADDRESS         OldHobAddress;
  EFI_HOB_HANDOFF_INFO_TABLE   *NewHob;
  EFI_HOB_RESOURCE_DESCRIPTOR  *ResourceHob;
  UINTN                        MemorySizeNeeded;

  OldHob        = (EFI_HOB_HANDOFF_INFO_TABLE *)PrePeiGetHobList ();
  OldHobAddress = (EFI_PHYSICAL_ADDRESS)OldHob;

  // These are corrupt checks if these fail boot can't happen
  ASSERT (OldHob->EfiFreeMemoryBottom > OldHobAddress);
  ASSERT (OldHob->EfiFreeMemoryTop >= OldHob->EfiFreeMemoryBottom);
  ASSERT (OldHob->EfiEndOfHobList > OldHobAddress);

  MemorySizeNeeded = PcdGet64 (PcdExpectedPeiMemoryUsage);
  if (MemorySizeNeeded != 0) {
    MemorySizeNeeded += CalculateTotalMemoryBinSizeNeeded () + MINIMUM_INITIAL_MEMORY_SIZE;
    MemorySizeNeeded  = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (MemorySizeNeeded));

    if ((OldHob->EfiMemoryTop - OldHob->EfiMemoryBottom) >= MemorySizeNeeded) {
      return EFI_SUCCESS;
    }

    // Split the region so only specified size is marked as tested
    ResourceHob = FindMemoryHob (RegionStart);
    // This should never happen as we create this hob in same module
    ASSERT ((ResourceHob != NULL) && (ResourceHob->PhysicalStart == RegionStart));

    if (RegionSize > MemorySizeNeeded) {
      ResourceHob->ResourceLength = MemorySizeNeeded;
      BuildResourceDescriptorHob (
        EFI_RESOURCE_SYSTEM_MEMORY,
        ResourceHob->ResourceAttribute,
        RegionStart + MemorySizeNeeded,
        RegionSize - MemorySizeNeeded
        );
      RegionSize = MemorySizeNeeded;
    } else if (RegionSize > MemorySizeNeeded) {
      DEBUG ((DEBUG_WARN, "Memory needed 0x%llx is more than region size 0x%llx\r\n", MemorySizeNeeded, RegionSize));
    }

    ResourceHob->ResourceAttribute |= EFI_RESOURCE_ATTRIBUTE_TESTED;
  }

  if (RegionSize <= (OldHob->EfiFreeMemoryTop - OldHobAddress)) {
    // Free area is not larger then current, do not move Hob list.
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

  // Mark old HOB list as allocated to protect existing AllocatePool entries
  BuildMemoryAllocationHob (
    OldHob->EfiMemoryBottom,
    ALIGN_VALUE ((OldHob->EfiEndOfHobList - OldHobAddress), EFI_PAGE_SIZE),
    EfiBootServicesData
    );

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

  ArmSetMemoryAttributes (FixedPcdGet64 (PcdMiscRegBaseAddress), SIZE_4KB, EFI_MEMORY_UC, 0);
  ChipID = TegraGetChipID ();
  ArmSetMemoryAttributes ((TegraGetBLInfoLocationAddress (ChipID) & ~EFI_PAGE_MASK), SIZE_4KB, EFI_MEMORY_UC, 0);
  ArmSetMemoryAttributes (GetCPUBLBaseAddress (), SIZE_64KB, EFI_MEMORY_WB, 0);
  ArmSetMemoryAttributes (GetDTBBaseAddress (), SIZE_64KB, EFI_MEMORY_WB, 0);
 #if FixedPcdGet64 (PcdSerialRegisterBase) != 0
  ArmSetMemoryAttributes (FixedPcdGet64 (PcdSerialRegisterBase), SIZE_4KB, EFI_MEMORY_UC, 0);
 #endif
 #if FixedPcdGet64 (PcdTegraCombinedUartRxMailbox) != 0
  ArmSetMemoryAttributes (FixedPcdGet64 (PcdTegraCombinedUartRxMailbox), SIZE_4KB, EFI_MEMORY_UC, 0);
 #endif
 #if FixedPcdGet64 (PcdTegraCombinedUartTxMailbox) != 0
  ArmSetMemoryAttributes (FixedPcdGet64 (PcdTegraCombinedUartTxMailbox), SIZE_4KB, EFI_MEMORY_UC, 0);
 #endif
 #if FixedPcdGet64 (PcdTegraUtcUartAddress) != 0
  ArmSetMemoryAttributes (FixedPcdGet64 (PcdTegraUtcUartAddress), SIZE_64KB, EFI_MEMORY_UC, 0);
 #endif

  DramPageBlacklistInfo = GetDramPageBlacklistInfoAddress ();
  if (DramPageBlacklistInfo != NULL) {
    while (DramPageBlacklistInfo->MemoryBaseAddress != 0 &&
           DramPageBlacklistInfo->MemoryLength != 0)
    {
      ArmSetMemoryAttributes (DramPageBlacklistInfo->MemoryBaseAddress, DramPageBlacklistInfo->MemoryLength, EFI_MEMORY_WB, 0);
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
  NV_ASSERT_EFI_ERROR_RETURN (Status, return );

  ASSERT (ResourcesCount != 0);

  // Walk HOB list for resources
  HobList = GetHobList ();
  NV_ASSERT_RETURN (HobList != NULL, return , "Missing HobList\n");

  HobList = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, HobList);
  while (NULL != HobList) {
    EFI_HOB_RESOURCE_DESCRIPTOR  *Resource = (EFI_HOB_RESOURCE_DESCRIPTOR *)HobList;
    DEBUG ((
      DEBUG_VERBOSE,
      "ArmPlatformGetVirtualMemoryMap() Resource: Base: 0x%016lx, Size: 0x%016lx, Type: 0x%x\n",
      Resource->PhysicalStart,
      Resource->ResourceLength,
      Resource->ResourceType
      ));

    if (Resource->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) {
      ArmSetMemoryAttributes (Resource->PhysicalStart, Resource->ResourceLength, EFI_MEMORY_WB, 0);
    } else {
      ArmSetMemoryAttributes (Resource->PhysicalStart, Resource->ResourceLength, EFI_MEMORY_UC, 0);
    }

    Index++;
    HobList = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, GET_NEXT_HOB (HobList));
  }

  ASSERT (Index == ResourcesCount);

  MigrateHobList (MaxRegionStart, MaxRegionSize);
}
