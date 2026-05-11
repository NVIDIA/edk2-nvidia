/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2011-2017, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiPei.h>
#include <Uefi.h>

#include <Library/DebugAgentLib.h>
#include <Library/PrePiLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/TimerLib.h>
#include <Library/PerformanceLib.h>
#include <Library/ArmMmuLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/GoldenRegisterLib.h>
#include <Library/SystemResourceLib.h>
#include <Library/TegraSerialPortLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/CpuExceptionHandlerLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/StatusRegLib.h>
#include <Library/TegraPlatformInfoLib.h>

#include <Ppi/GuidedSectionExtraction.h>
#include <Ppi/SecPerformance.h>
#include <Pi/PiFirmwareVolume.h>
#include <Library/FdtLib.h>

#include "PrePi.h"

typedef struct {
  EFI_FIRMWARE_VOLUME_HEADER    *FvHeader;
  UINT64                        FvSize;
  UINT64                        DtbBase;
  UINT64                        DtbSize;
  UINT64                        StartTimeStamp;
  EFI_PHYSICAL_ADDRESS          NewStackBase;
  UINT64                        NewStackLength;
  UINTN                         NewStackSize;
} PREPI_STACK_SWITCH_CONTEXT;

STATIC
VOID
InitMmu (
  IN ARM_MEMORY_REGION_DESCRIPTOR  *MemoryTable
  )
{
  VOID        *TranslationTableBase;
  UINTN       TranslationTableSize;
  EFI_STATUS  Status;

  // Note: Because we called PeiServicesInstallPeiMemory() before to call InitMmu() the MMU Page Table resides in
  //      DRAM (even at the top of DRAM as it is the first permanent memory allocation)
  Status = ArmConfigureMmu (MemoryTable, &TranslationTableBase, &TranslationTableSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Error: Failed to enable MMU\n"));
  }
}

STATIC
VOID *
ReserveHobTopPages (
  IN UINTN  Pages
  )
{
  EFI_PEI_HOB_POINTERS  Hob;
  EFI_PHYSICAL_ADDRESS  NewTop;
  UINTN                 AllocationSize;

  if ((Pages == 0) || (Pages > (MAX_UINTN / EFI_PAGE_SIZE))) {
    return NULL;
  }

  AllocationSize = Pages * EFI_PAGE_SIZE;
  Hob.Raw        = GetHobList ();
  NewTop         = Hob.HandoffInformationTable->EfiFreeMemoryTop &
                   ~(EFI_PHYSICAL_ADDRESS)EFI_PAGE_MASK;
  if (NewTop < AllocationSize) {
    return NULL;
  }

  NewTop -= AllocationSize;

  if (NewTop < (Hob.HandoffInformationTable->EfiFreeMemoryBottom +
                sizeof (EFI_HOB_MEMORY_ALLOCATION)))
  {
    return NULL;
  }

  // Reserve space from the active HOB heap without creating an allocation HOB.
  // Some callers need to describe the final type with a special HOB later.
  Hob.HandoffInformationTable->EfiFreeMemoryTop = NewTop;

  return (VOID *)(UINTN)NewTop;
}

STATIC
VOID *
AllocateBootServicesCodePages (
  IN UINTN  Pages
  )
{
  VOID   *Allocation;
  UINTN  AllocationSize;

  Allocation = ReserveHobTopPages (Pages);
  if (Allocation == NULL) {
    return NULL;
  }

  AllocationSize = Pages * EFI_PAGE_SIZE;
  BuildMemoryAllocationHob (
    (EFI_PHYSICAL_ADDRESS)(UINTN)Allocation,
    AllocationSize,
    EfiBootServicesCode
    );

  return Allocation;
}

STATIC
VOID *
CopyMmuLiveTranslationEntryHelper (
  VOID
  )
{
  extern UINT32  ArmReplaceLiveTranslationEntrySize;

  VOID   *HelperCopy;
  UINTN  HelperPages;
  UINTN  HelperSize;

  HelperSize  = ArmReplaceLiveTranslationEntrySize;
  HelperPages = EFI_SIZE_TO_PAGES (HelperSize);
  HelperCopy  = AllocateBootServicesCodePages (HelperPages);
  NV_ASSERT_RETURN (
    HelperCopy != NULL,
    return NULL,
    "%a: failed to allocate %lu pages for MMU live-entry helper\n",
    __FUNCTION__,
    HelperPages
    );

  // CpuDxe calls this helper while changing live page tables, after reserved
  // carveouts may have been made non-executable by DXE memory protection.
  CopyMem (
    HelperCopy,
    (VOID *)(UINTN)ArmReplaceLiveTranslationEntry,
    HelperSize
    );
  WriteBackDataCacheRange (HelperCopy, HelperSize);
  InvalidateInstructionCacheRange (HelperCopy, HelperSize);
  DEBUG ((
    DEBUG_ERROR,
    "%a: copied MMU live-entry helper: 0x%p -> 0x%p, size 0x%lx\n",
    __FUNCTION__,
    (VOID *)(UINTN)ArmReplaceLiveTranslationEntry,
    HelperCopy,
    HelperSize
    ));

  return HelperCopy;
}

STATIC
BOOLEAN
IsRangeInReservedResource (
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                Length
  )
{
  EFI_PEI_HOB_POINTERS         Hob;
  EFI_HOB_RESOURCE_DESCRIPTOR  *Resource;
  EFI_PHYSICAL_ADDRESS         EndAddress;
  EFI_PHYSICAL_ADDRESS         ResourceEnd;

  if ((Length == 0) || (BaseAddress > MAX_UINT64 - Length)) {
    return FALSE;
  }

  EndAddress = BaseAddress + Length;
  Hob.Raw    = GetHobList ();
  Hob.Raw    = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, Hob.Raw);
  while (Hob.Raw != NULL) {
    Resource = Hob.ResourceDescriptor;
    if ((Resource->ResourceType != EFI_RESOURCE_MEMORY_RESERVED) ||
        (Resource->ResourceLength == 0) ||
        (Resource->PhysicalStart > MAX_UINT64 - Resource->ResourceLength))
    {
      Hob.Raw = GET_NEXT_HOB (Hob);
      Hob.Raw = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, Hob.Raw);
      continue;
    }

    ResourceEnd = Resource->PhysicalStart + Resource->ResourceLength;
    if ((BaseAddress >= Resource->PhysicalStart) &&
        (EndAddress <= ResourceEnd))
    {
      return TRUE;
    }

    Hob.Raw = GET_NEXT_HOB (Hob);
    Hob.Raw = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, Hob.Raw);
  }

  return FALSE;
}

STATIC
VOID
ReserveBootAllocationsInReservedResources (
  VOID
  )
{
  EFI_PEI_HOB_POINTERS              Hob;
  EFI_HOB_MEMORY_ALLOCATION_HEADER  *Allocation;

  // Allocation HOB memory types feed the DXE memory map. Boot-service
  // allocations already placed in reserved carveouts must therefore be
  // reserved for the OS even though firmware keeps using the memory.
  Hob.Raw = GetHobList ();
  Hob.Raw = GetNextHob (EFI_HOB_TYPE_MEMORY_ALLOCATION, Hob.Raw);
  while (Hob.Raw != NULL) {
    Allocation = &Hob.MemoryAllocation->AllocDescriptor;
    if ((Allocation->MemoryType != EfiBootServicesCode) &&
        (Allocation->MemoryType != EfiBootServicesData))
    {
      Hob.Raw = GET_NEXT_HOB (Hob);
      Hob.Raw = GetNextHob (EFI_HOB_TYPE_MEMORY_ALLOCATION, Hob.Raw);
      continue;
    }

    if (IsRangeInReservedResource (
          Allocation->MemoryBaseAddress,
          Allocation->MemoryLength
          ))
    {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Reserved carveout allocation: Base: 0x%016lx, "
        "Size: 0x%016lx, Type: %u -> %u\n",
        __FUNCTION__,
        Allocation->MemoryBaseAddress,
        Allocation->MemoryLength,
        (UINT32)Allocation->MemoryType,
        (UINT32)EfiReservedMemoryType
        ));
      Allocation->MemoryType = EfiReservedMemoryType;
    }

    Hob.Raw = GET_NEXT_HOB (Hob);
    Hob.Raw = GetNextHob (EFI_HOB_TYPE_MEMORY_ALLOCATION, Hob.Raw);
  }
}

/*++

Routine Description:
  Registers the primary firmware volume
  1. Creates Fv HOB entry
  2. Split FV into its own resource
  3. Marks region as allocated


Arguments:

  FvBase  - Base address of firmware volume.
  FvSize  - Size of firmware volume.

Returns:

  Status -  EFI_SUCCESS if the firmware volume registerd

--*/
EFI_STATUS
EFIAPI
RegisterFirmwareVolume (
  IN EFI_PHYSICAL_ADDRESS  FvBase,
  IN UINT64                FvSize
  )
{
  EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttributes;
  EFI_RESOURCE_TYPE            ResourceType;
  EFI_MEMORY_TYPE              MemoryType;
  UINT64                       ResourceLength;
  EFI_PEI_HOB_POINTERS         NextHob;
  EFI_PHYSICAL_ADDRESS         FvTop;
  EFI_PHYSICAL_ADDRESS         ResourceTop;
  BOOLEAN                      Found;

  FvTop = FvBase + FvSize;
  // EDK2 does not have the concept of boot firmware copied into DRAM. To avoid the DXE
  // core to overwrite this area we must create a memory allocation HOB for the region,
  // but this only works if we split off the underlying resource descriptor as well.
  Found = FALSE;

  // Search for a resource HOB that contains the firmware.
  NextHob.Raw = GetHobList ();
  while ((NextHob.Raw = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, NextHob.Raw)) != NULL) {
    ResourceType = NextHob.ResourceDescriptor->ResourceType;
    if (((ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) ||
         (ResourceType == EFI_RESOURCE_MEMORY_RESERVED)) &&
        (FvBase >= NextHob.ResourceDescriptor->PhysicalStart) &&
        (FvTop <= NextHob.ResourceDescriptor->PhysicalStart + NextHob.ResourceDescriptor->ResourceLength))
    {
      ResourceAttributes = NextHob.ResourceDescriptor->ResourceAttribute;
      ResourceLength     = NextHob.ResourceDescriptor->ResourceLength;
      ResourceTop        = NextHob.ResourceDescriptor->PhysicalStart + ResourceLength;
      MemoryType         = (ResourceType == EFI_RESOURCE_MEMORY_RESERVED) ?
                           EfiReservedMemoryType :
                           EfiBootServicesCode;

      if (FvBase == NextHob.ResourceDescriptor->PhysicalStart) {
        if (ResourceTop != FvTop) {
          // Split off the resource HOB for the firmware.
          BuildResourceDescriptorHob (
            ResourceType,
            ResourceAttributes,
            FvBase,
            FvSize
            );

          // Top of the FD keeps the original resource type.
          NextHob.ResourceDescriptor->PhysicalStart  += FvSize;
          NextHob.ResourceDescriptor->ResourceLength -= FvSize;
        }
      } else {
        // Split off the resource HOB for the firmware.
        BuildResourceDescriptorHob (
          ResourceType,
          ResourceAttributes,
          FvBase,
          FvSize
          );

        // Update the HOB
        NextHob.ResourceDescriptor->ResourceLength = FvBase - NextHob.ResourceDescriptor->PhysicalStart;

        // If there is memory above the FD then create a matching resource HOB.
        if (FvTop < NextHob.ResourceDescriptor->PhysicalStart + ResourceLength) {
          // Create a HOB for the remaining resource above the FD.
          BuildResourceDescriptorHob (
            ResourceType,
            ResourceAttributes,
            FvTop,
            ResourceTop - FvTop
            );
        }
      }

      // Match the allocation type to the resource type so carveout-hosted
      // firmware remains reserved in the OS-facing memory map.
      BuildMemoryAllocationHob (FvBase, FvSize, MemoryType);

      Found = TRUE;
      break;
    }

    NextHob.Raw = GET_NEXT_HOB (NextHob);
  }

  ASSERT (Found);

  return EFI_SUCCESS;
}

/*++

Outputs the system resource that contains the Hob List.
This is used for debug purposes

--*/
VOID
EFIAPI
DisplayHobResource (
  VOID
  )
{
  EFI_PEI_HOB_POINTERS  NextHob;
  VOID                  *HobBase;

  // Search for System Memory Hob that contains the hob list
  HobBase     = GetHobList ();
  NextHob.Raw = HobBase;
  while ((NextHob.Raw = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, NextHob.Raw)) != NULL) {
    if ((NextHob.ResourceDescriptor->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) &&
        ((UINTN)HobBase >= NextHob.ResourceDescriptor->PhysicalStart) &&
        ((UINTN)HobBase < NextHob.ResourceDescriptor->PhysicalStart + NextHob.ResourceDescriptor->ResourceLength))
    {
      DEBUG ((
        DEBUG_INIT,
        "Main memory region: (0x%016llx, 0x%016llx)\r\n",
        NextHob.ResourceDescriptor->PhysicalStart,
        NextHob.ResourceDescriptor->ResourceLength
        ));
      return;
    }

    NextHob.Raw = GET_NEXT_HOB (NextHob);
  }
}

VOID
PrintModel (
  VOID
  );

STATIC
VOID
EFIAPI
CEntryPointOnPermanentStack (
  IN VOID  *Context1,
  IN VOID  *Context2
  )
{
  EFI_STATUS                  Status;
  FIRMWARE_SEC_PERFORMANCE    Performance;
  VOID                        *MmuFuncPtr;
  VOID                        *MmuFuncHob;
  PREPI_STACK_SWITCH_CONTEXT  *Context;
  EFI_PHYSICAL_ADDRESS        StackGuardBase;

  (VOID)Context2;

  Context = (PREPI_STACK_SWITCH_CONTEXT *)Context1;

  // The bootloader starts PrePi on the UEFI carveout stack. Keep the carveout
  // reserved for the OS, but run DXE and OS loaders from a normal DRAM stack.
  // OS loaders inherit the current firmware SP, and some reject a stack that
  // sits in a GetMemoryMap() descriptor reported as EfiReservedMemoryType.
  StackGuardBase = Context->NewStackBase + Context->NewStackSize;
  Status         = ArmSetMemoryAttributes (
                     StackGuardBase,
                     SIZE_4KB,
                     EFI_MEMORY_RO,
                     EFI_MEMORY_RO
                     );
  ASSERT_EFI_ERROR (Status);
  DEBUG ((
    DEBUG_ERROR,
    "%a: running on migrated stack: Base: 0x%016lx, Size: 0x%016lx\n",
    __FUNCTION__,
    Context->NewStackBase,
    Context->NewStackLength
    ));

  // Register UEFI DTB
  RegisterDeviceTree (Context->DtbBase);

  // Get info from platform
  Status = UpdatePlatformResourceInformation ();
  NV_ASSERT_RETURN (
    !EFI_ERROR (Status),
    CpuDeadLoop (),
    "Failed to UpdatePlatformResourceInformation - %r!\r\n",
    Status
    );

  // Print Chip ID
  DEBUG ((DEBUG_ERROR, "ChipID: 0x%x\n", TegraGetChipID ()));

  // Print Platform ID
  DEBUG ((DEBUG_ERROR, "Platform ID: %d\n", TegraGetPlatform ()));

  // Print platform model info from UEFI DTB
  PrintModel ();

  // Create DTB memory allocation HOB
  BuildMemoryAllocationHob (Context->DtbBase, Context->DtbSize, EfiBootServicesData);

  // Publish only the migrated stack as the active UEFI stack. The old
  // bootloader stack remains inside the reserved UEFI carveout and must not
  // be exposed as boot-service memory in the OS-facing map.
  BuildStackHob (Context->NewStackBase, Context->NewStackLength);

  // Save a normal-DRAM copy of ArmReplaceLiveTranslationEntry() in a HOB so
  // CpuDxe can replace live translation entries after UEFI carveouts become
  // reserved and non-executable under DXE memory protection.
  MmuFuncPtr = CopyMmuLiveTranslationEntryHelper ();
  NV_ASSERT_RETURN (
    MmuFuncPtr != NULL,
    CpuDeadLoop (),
    "Missing MMU live-entry helper copy!\r\n"
    );
  MmuFuncHob = BuildGuidDataHob (
                 &gArmMmuReplaceLiveTranslationEntryFuncGuid,
                 &MmuFuncPtr,
                 sizeof (MmuFuncPtr)
                 );
  ASSERT (MmuFuncHob != NULL);

  // TODO: Call CpuPei as a library
  BuildCpuHob (ArmGetPhysicalAddressBits (), ArmGetPhysicalAddressBits ());

  // Store timer value logged at the beginning of firmware image execution
  Performance.ResetEnd = GetTimeInNanoSecond (Context->StartTimeStamp);

  // Build SEC Performance Data Hob
  BuildGuidDataHob (&gEfiFirmwarePerformanceGuid, &Performance, sizeof (Performance));

  // Set the Boot Mode
  SetBootMode (BOOT_WITH_FULL_CONFIGURATION);

  // Register firmware volume
  Status = RegisterFirmwareVolume ((EFI_PHYSICAL_ADDRESS)Context->FvHeader, Context->FvSize);
  ASSERT_EFI_ERROR (Status);

  // Boot-service allocations inside carveouts must stay reserved for the OS.
  ReserveBootAllocationsInReservedResources ();

  // Now, the HOB List has been initialized, we can register performance information
  PERF_START (NULL, "PEI", NULL, Context->StartTimeStamp);

  // SEC phase needs to run library constructors by hand.
  ProcessLibraryConstructorList ();

  DisplayHobResource ();

  // Assume the FV that contains the SEC (our code) also contains a compressed FV.
  Status = DecompressFirstFv ();
  ASSERT_EFI_ERROR (Status);

  // Load the DXE Core and transfer control to it
  Status = LoadDxeCoreFromFv (NULL, 0);
  ASSERT_EFI_ERROR (Status);

  // DXE Core should always load and never return
  ASSERT (FALSE);
}

STATIC
VOID
SwitchToPermanentStack (
  IN EFI_FIRMWARE_VOLUME_HEADER  *FvHeader,
  IN UINT64                      FvSize,
  IN UINT64                      DtbBase,
  IN UINT64                      DtbSize,
  IN UINT64                      StartTimeStamp,
  IN UINTN                       OldStackBase,
  IN UINTN                       OldStackSize
  )
{
  PREPI_STACK_SWITCH_CONTEXT  *Context;
  EFI_PHYSICAL_ADDRESS        OldStackEnd;
  EFI_PHYSICAL_ADDRESS        NewStackTop;
  UINTN                       NewStackLength;
  UINTN                       NewStackPages;

  if (OldStackSize > MAX_UINTN - SIZE_4KB) {
    CpuDeadLoop ();
  }

  NewStackLength = ALIGN_VALUE (OldStackSize + SIZE_4KB, EFI_PAGE_SIZE);
  NewStackPages  = EFI_SIZE_TO_PAGES (NewStackLength);
  Context        = AllocatePages (EFI_SIZE_TO_PAGES (sizeof (*Context)));
  NV_ASSERT_RETURN (
    Context != NULL,
    CpuDeadLoop (),
    "%a: failed to allocate stack switch context\n",
    __FUNCTION__
    );
  if (Context == NULL) {
    return;
  }

  ZeroMem (Context, sizeof (*Context));

  // Delay the allocation HOB until after SwitchStack() so the HOB can use the
  // stack allocation GUID instead of a generic boot-service data type.
  Context->NewStackBase = (EFI_PHYSICAL_ADDRESS)(UINTN)ReserveHobTopPages (
                                                         NewStackPages
                                                         );
  NV_ASSERT_RETURN (
    Context->NewStackBase != 0,
    CpuDeadLoop (),
    "%a: failed to allocate %lu pages for migrated stack\n",
    __FUNCTION__,
    NewStackPages
    );

  Context->FvHeader       = FvHeader;
  Context->FvSize         = FvSize;
  Context->DtbBase        = DtbBase;
  Context->DtbSize        = DtbSize;
  Context->StartTimeStamp = StartTimeStamp;
  Context->NewStackLength = NewStackLength;
  Context->NewStackSize   = OldStackSize;
  NewStackTop             = Context->NewStackBase + Context->NewStackSize;
  OldStackEnd             = (EFI_PHYSICAL_ADDRESS)OldStackBase + NewStackLength;

  DEBUG ((
    DEBUG_ERROR,
    "%a: switching stack out of UEFI carveout: "
    "0x%016lx-0x%016lx -> 0x%016lx-0x%016lx\n",
    __FUNCTION__,
    OldStackBase,
    OldStackEnd,
    Context->NewStackBase,
    Context->NewStackBase + Context->NewStackLength
    ));

  SwitchStack (
    CEntryPointOnPermanentStack,
    Context,
    NULL,
    (VOID *)(UINTN)NewStackTop
    );
  ASSERT (FALSE);
}

VOID
PrintModel (
  VOID
  )
{
  EFI_STATUS  Status;
  VOID        *Dtb;
  UINTN       DtbSize;
  INT32       NumProperty;
  UINT32      Count;
  CHAR8       *Data;
  INT32       Length;

  Status = DtPlatformLoadDtb (&Dtb, &DtbSize);
  if (EFI_ERROR (Status)) {
    return;
  }

  NumProperty = FdtStringListCount (Dtb, 0, "model");
  if (NumProperty <= 0) {
    return;
  }

  for (Count = 0; Count < NumProperty; Count++) {
    Data = (CHAR8 *)FdtStringListGet (Dtb, 0, "model", Count, &Length);
    if (Length <= 0) {
      return;
    }

    DEBUG ((DEBUG_ERROR, "Model: %a\n", Data));
  }
}

VOID
CEntryPoint (
  IN  UINTN  MemoryBase,
  IN  UINTN  MemorySize,
  IN  UINTN  StackBase,
  IN  UINTN  StackSize
  )
{
  EFI_HOB_HANDOFF_INFO_TABLE    *HobList;
  EFI_STATUS                    Status;
  CHAR8                         Buffer[150];
  UINTN                         CharCount;
  UINT64                        StartTimeStamp;
  EFI_FIRMWARE_VOLUME_HEADER    *FvHeader;
  UINT64                        FvSize;
  UINT64                        FvOffset = 0;
  UINT64                        HobBase;
  UINT64                        HobSize;
  UINT64                        HobFree;
  UINT64                        DtbBase;
  UINT64                        DtbSize;
  UINT64                        DtbOffset;
  UINT64                        DtbNext;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  ARM_MEMORY_REGION_DESCRIPTOR  InitialMemory[2];
  SERIAL_MAPPING                *Mapping;

  Mapping = NULL;

  if (PerformanceMeasurementEnabled ()) {
    // We cannot call yet the PerformanceLib because the HOB List has not been initialized
    StartTimeStamp = GetPerformanceCounter ();
  } else {
    StartTimeStamp = 0;
  }

  FvHeader = NULL;

  while (FvOffset < MemorySize) {
    FvHeader = (EFI_FIRMWARE_VOLUME_HEADER *)(VOID *)(MemoryBase + FvOffset);
    if (FvHeader->Signature == EFI_FVH_SIGNATURE) {
      break;
    }

    FvOffset += SIZE_64KB;
  }

  ASSERT (FvOffset < MemorySize);
  ASSERT (FvHeader != NULL);
  FvSize = FvHeader->FvLength;
  // Make UEFI FV size aligned to 64KB.
  FvSize = ALIGN_VALUE (FvSize, SIZE_64KB);

  if ((GetGRBlobBaseAddress () != 0) &&
      (ValidateGrBlobHeader (GetGRBlobBaseAddress ()) == EFI_SUCCESS))
  {
    FvSize += GrBlobBinarySize (GetGRBlobBaseAddress ());
  }

  // Share Fv location with Arm libraries
  PatchPcdSet64 (PcdFvBaseAddress, (UINT64)FvHeader);

  DtbBase = GetDTBBaseAddress ();
  ASSERT ((VOID *)DtbBase != NULL);
  DtbSize = FdtTotalSize ((VOID *)DtbBase);

  // Find the end of overlay DTB region.
  // Overlay DTBs are aligned to 4KB
  DtbNext = ALIGN_VALUE (DtbBase + DtbSize, SIZE_4KB);
  while (DtbNext < MemoryBase + MemorySize) {
    if (FdtCheckHeader ((VOID *)DtbNext) != 0) {
      break;
    }

    DtbNext += FdtTotalSize ((VOID *)DtbNext);
    DtbNext  = ALIGN_VALUE (DtbNext, SIZE_4KB);
  }

  DtbSize = DtbNext - DtbBase;

  // DTB Base may not be aligned to page boundary. Add overlay to size.
  DtbSize += (DtbBase & EFI_PAGE_MASK);
  DtbSize  = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (DtbSize));
  // Align DTB Base to page boundary.
  DtbBase  &= ~(EFI_PAGE_MASK);
  DtbOffset = DtbBase - MemoryBase;

  if ((DtbBase >= MemoryBase) && (DtbBase < (MemoryBase + MemorySize))) {
    // Find out where HOB region should be depending on the biggest available
    // memory chunk in memory. Memory has stack at the very end and FV and DTB
    // somewhere in the middle. FV and DTB could be present in any order.
    DtbOffset = DtbBase - MemoryBase;
    if (DtbOffset > FvOffset) {
      // If DTB is loaded after FV
      if (FvOffset > (DtbOffset - FvOffset - FvSize)) {
        // Available space between Memory Base and FV is bigger than
        // available space between FV and DTB
        if (FvOffset > (MemorySize - DtbOffset - DtbSize - StackSize)) {
          // Available space between Memory Base and FV is bigger than
          // available space after DTB
          HobBase = MemoryBase;
          HobSize = FvOffset;
        } else {
          // Available space between Memory Base and FV is smaller than
          // available space after DTB
          HobBase = DtbBase + DtbSize;
          HobSize = MemorySize - DtbOffset - DtbSize - StackSize;
        }
      } else {
        // Available space between Memory Base and FV is smaller than
        // available space between FV and DTB
        if ((DtbOffset - FvOffset - FvSize) > (MemorySize - DtbOffset - DtbSize - StackSize)) {
          // Available space between FV and DTB is bigger than
          // available space after DTB
          HobBase = MemoryBase + FvOffset + FvSize;
          HobSize = DtbOffset - FvOffset - FvSize;
        } else {
          // Available space between FV and DTB is smaller than
          // available space after DTB
          HobBase = DtbBase + DtbSize;
          HobSize = MemorySize - DtbOffset - DtbSize - StackSize;
        }
      }
    } else {
      // If DTB is loaded before FV
      if (DtbOffset > (FvOffset - DtbOffset - DtbSize)) {
        // Available space between Memory Base and DTB is bigger than
        // available space between DTB and FV
        if (DtbOffset > (MemorySize - FvOffset - FvSize - StackSize)) {
          // Available space between Memory Base and DTB is bigger than
          // available space after FV
          HobBase = MemoryBase;
          HobSize = DtbOffset;
        } else {
          // Available space between Memory Base and DTB is smaller than
          // available space after FV
          HobBase = MemoryBase + FvOffset + FvSize;
          HobSize = MemorySize - FvOffset - FvSize - StackSize;
        }
      } else {
        // Available space between Memory Base and DTB is smaller than
        // available space between DTB and FV
        if ((FvOffset - DtbOffset - DtbSize) > (MemorySize - FvOffset - FvSize - StackSize)) {
          // Available space between DTB and FV is bigger than
          // available space after FV
          HobBase = MemoryBase + DtbOffset + DtbSize;
          HobSize = FvOffset - DtbOffset - DtbSize;
        } else {
          // Available space between DTB and FV is smaller than
          // available space after FV
          HobBase = MemoryBase + FvOffset + FvSize;
          HobSize = MemorySize - FvOffset - FvSize - StackSize;
        }
      }
    }
  } else {
    // Default to hob after the FV
    HobBase = MemoryBase + FvOffset + FvSize;
    HobSize = MemorySize - FvSize - FvOffset - StackSize;
    // Unless area before FV is larger
    if (FvOffset > HobSize) {
      HobBase = MemoryBase;
      HobSize = FvOffset;
    }
  }

  // Data Cache enabled on Primary core when MMU is enabled.
  ArmDisableDataCache ();
  // Invalidate instruction cache
  ArmInvalidateInstructionCache ();
  // Enable Instruction Caches on all cores.
  ArmEnableInstructionCache ();

  // Initialize the architecture specific bits
  ArchInitialize ();

  // Declare the PI/UEFI memory region
  HobFree = HobBase + HobSize;
  HobList = HobConstructor (
              (VOID *)HobBase,
              HobSize,
              (VOID *)HobBase,
              (VOID *)HobFree
              );
  PrePeiSetHobList (HobList);

  InitialMemory[0].PhysicalBase = MemoryBase;
  InitialMemory[0].VirtualBase  = MemoryBase;
  InitialMemory[0].Length       = MemorySize;
  InitialMemory[0].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;
  InitialMemory[1].PhysicalBase = 0;
  InitialMemory[1].VirtualBase  = 0;
  InitialMemory[1].Length       = 0;
  InitialMemory[1].Attributes   = (ARM_MEMORY_REGION_ATTRIBUTES)0;
  InitMmu (InitialMemory);
  MapCorePlatformMemory ();
  StatusRegSetPhase (STATUS_REG_PHASE_PREPI, STATUS_REG_PREPI_STARTED);

  // Initialize the Serial Port
  SerialPortInitialize ();
  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "%s UEFI firmware (version %s built on %s)\n\r",
                (CHAR16 *)PcdGetPtr (PcdPlatformFamilyName),
                (CHAR16 *)PcdGetPtr (PcdUefiVersionString),
                (CHAR16 *)PcdGetPtr (PcdUefiDateTimeBuiltString)
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);

  DEBUG_CODE_BEGIN ();
  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "Memory: 0x%lx-0x%lx (0x%lx)\n\r",
                MemoryBase,
                MemoryBase + MemorySize,
                MemorySize
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);
  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "Stack : 0x%lx-0x%lx (0x%lx)\n\r",
                StackBase,
                StackBase + StackSize,
                StackSize
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);
  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "FV    : 0x%lx-0x%lx (0x%lx)\n\r",
                (EFI_PHYSICAL_ADDRESS)FvHeader,
                (EFI_PHYSICAL_ADDRESS)FvHeader + FvSize,
                FvSize
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);
  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "DTB   : 0x%lx-0x%lx (0x%lx)\n\r",
                DtbBase,
                DtbBase + DtbSize,
                DtbSize
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);
  DEBUG_CODE_END ();

  // Enable exception handlers, now that we have a serial port to write to.
  Status = InitializeCpuExceptionHandlers (NULL);
  ASSERT_EFI_ERROR (Status);

  // Initialize the Debug Agent for Source Level Debugging
  // InitializeDebugAgent (DEBUG_AGENT_INIT_POSTMEM_SEC, NULL, NULL);
  SaveAndSetDebugTimerInterrupt (TRUE);

  // Register Firmware volume
  BuildFvHob ((EFI_PHYSICAL_ADDRESS)FvHeader, FvSize);

  // Build Platform Resource Data HOB
  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)BuildGuidHob (&gNVIDIAPlatformResourceDataGuid, sizeof (TEGRA_PLATFORM_RESOURCE_INFO));
  NV_ASSERT_RETURN (PlatformResourceInfo != NULL, CpuDeadLoop (), "Failed to allocate platform resource!\r\n");
  ZeroMem (PlatformResourceInfo, sizeof (TEGRA_PLATFORM_RESOURCE_INFO));
  Status = GetPlatformResourceInformation (PlatformResourceInfo);
  NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "Failed to GetPlatformResourceInformation - %r!\r\n", Status);

  if (FeaturePcdGet (PcdPrePiProduceMemoryTypeInformationHob)) {
    // Optional feature that helps prevent EFI memory map fragmentation.
    BuildMemoryTypeInformationHob ();
  }

  // Add DRAM and reserved carveout resources, then migrate the HOB list into
  // normal DRAM before leaving the bootloader-provided UEFI carveout stack.
  UpdateMemoryMap ();

  SwitchToPermanentStack (
    FvHeader,
    FvSize,
    DtbBase,
    DtbSize,
    StartTimeStamp,
    StackBase,
    StackSize
    );

  ASSERT (FALSE);
}
