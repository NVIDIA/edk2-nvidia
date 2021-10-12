/** @file
*
*  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
*  Copyright (c) 2011-2017, ARM Limited. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
*  Portions provided under the following terms:
*  Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#include <PiPei.h>

#include <Library/DebugAgentLib.h>
#include <Library/PrePiLib.h>
#include <Library/PrintLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/TimerLib.h>
#include <Library/PerformanceLib.h>
#include <Library/ArmMmuLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/GoldenRegisterLib.h>
#include <Library/SystemResourceLib.h>


#include <Ppi/GuidedSectionExtraction.h>
#include <Ppi/SecPerformance.h>
#include <Pi/PiFirmwareVolume.h>
#include <libfdt.h>

#include "PrePi.h"

STATIC
VOID
InitMmu (
  IN ARM_MEMORY_REGION_DESCRIPTOR  *MemoryTable
  )
{

  VOID                          *TranslationTableBase;
  UINTN                         TranslationTableSize;
  EFI_STATUS                    Status;

  //Note: Because we called PeiServicesInstallPeiMemory() before to call InitMmu() the MMU Page Table resides in
  //      DRAM (even at the top of DRAM as it is the first permanent memory allocation)
  Status = ArmConfigureMmu (MemoryTable, &TranslationTableBase, &TranslationTableSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Error: Failed to enable MMU\n"));
  }
}

/*++

Routine Description:
  Registers the primary firmware volume
  1. Creates Fv HOB entry
  2. Split FV into its own system resource
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
  IN EFI_PHYSICAL_ADDRESS               FvBase,
  IN UINT64                             FvSize
  )
{
  EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttributes;
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

  // Search for System Memory Hob that contains the firmware
  NextHob.Raw = GetHobList ();
  while ((NextHob.Raw = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, NextHob.Raw)) != NULL) {
    if ((NextHob.ResourceDescriptor->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) &&
        (FvBase >= NextHob.ResourceDescriptor->PhysicalStart) &&
        (FvTop <= NextHob.ResourceDescriptor->PhysicalStart + NextHob.ResourceDescriptor->ResourceLength))
    {
      ResourceAttributes = NextHob.ResourceDescriptor->ResourceAttribute;
      ResourceLength = NextHob.ResourceDescriptor->ResourceLength;
      ResourceTop = NextHob.ResourceDescriptor->PhysicalStart + ResourceLength;

      if (FvBase == NextHob.ResourceDescriptor->PhysicalStart) {
        if (ResourceTop != FvTop) {
          // Create the System Memory HOB for the firmware
          BuildResourceDescriptorHob (EFI_RESOURCE_SYSTEM_MEMORY,
                                      ResourceAttributes,
                                      FvBase,
                                      FvSize);

          // Top of the FD is system memory available for UEFI
          NextHob.ResourceDescriptor->PhysicalStart += FvSize;
          NextHob.ResourceDescriptor->ResourceLength -= FvSize;
        }
      } else {
        // Create the System Memory HOB for the firmware
        BuildResourceDescriptorHob (EFI_RESOURCE_SYSTEM_MEMORY,
                                    ResourceAttributes,
                                    FvBase,
                                    FvSize);

        // Update the HOB
        NextHob.ResourceDescriptor->ResourceLength = FvBase - NextHob.ResourceDescriptor->PhysicalStart;

        // If there is some memory available on the top of the FD then create a HOB
        if (FvTop < NextHob.ResourceDescriptor->PhysicalStart + ResourceLength) {
          // Create the System Memory HOB for the remaining region (top of the FD)
          BuildResourceDescriptorHob (EFI_RESOURCE_SYSTEM_MEMORY,
                                      ResourceAttributes,
                                      FvTop,
                                      ResourceTop - FvTop);
        }
      }

      // Mark the memory covering the Firmware Device as boot services data
      BuildMemoryAllocationHob (FvBase, FvSize, EfiBootServicesData);

      Found = TRUE;
      break;
    }
    NextHob.Raw = GET_NEXT_HOB (NextHob);
  }

  ASSERT(Found);

  return EFI_SUCCESS;
}

/*++

Outputs the system resource that contains the Hob List.
This is used for debug purposes

--*/
VOID
EFIAPI
DisplayHobResource ( VOID )
{
  EFI_PEI_HOB_POINTERS         NextHob;
  VOID                         *HobBase;

  // Search for System Memory Hob that contains the hob list
  HobBase = GetHobList ();
  NextHob.Raw = HobBase;
  while ((NextHob.Raw = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, NextHob.Raw)) != NULL) {
    if ((NextHob.ResourceDescriptor->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) &&
        ((UINTN)HobBase >= NextHob.ResourceDescriptor->PhysicalStart) &&
        ((UINTN)HobBase < NextHob.ResourceDescriptor->PhysicalStart + NextHob.ResourceDescriptor->ResourceLength)) {
      DEBUG ((EFI_D_INIT, "Main memory region: (0x%016llx, 0x%016llx)\r\n",
          NextHob.ResourceDescriptor->PhysicalStart,
          NextHob.ResourceDescriptor->ResourceLength));
      return;
    }
    NextHob.Raw = GET_NEXT_HOB (NextHob);
  }
}

VOID
CEntryPoint (
  IN  UINTN                     MemoryBase,
  IN  UINTN                     MemorySize,
  IN  UINTN                     StackBase,
  IN  UINTN                     StackSize
  )
{
  EFI_HOB_HANDOFF_INFO_TABLE*   HobList;
  EFI_STATUS                    Status;
  CHAR8                         Buffer[100];
  UINTN                         CharCount;
  FIRMWARE_SEC_PERFORMANCE      Performance;
  UINT64                        StartTimeStamp;
  ARM_MEMORY_REGION_DESCRIPTOR  *MemoryTable;
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

  while (FvOffset < MemorySize) {
    FvHeader = (EFI_FIRMWARE_VOLUME_HEADER *)(VOID *)(MemoryBase + FvOffset);
    if (FvHeader->Signature == EFI_FVH_SIGNATURE) {
      break;
    }
    FvOffset += SIZE_64KB;
  }
  ASSERT (FvOffset < MemorySize);
  FvSize = FvHeader->FvLength;
  // Make UEFI FV size aligned to 64KB.
  FvSize = ALIGN_VALUE (FvSize, SIZE_64KB);

  if (GetGRBlobBaseAddress () != 0 &&
      ValidateGrBlobHeader (GetGRBlobBaseAddress ()) == EFI_SUCCESS) {
    FvSize += GrBlobBinarySize (GetGRBlobBaseAddress ());
  }

  DtbBase = GetDTBBaseAddress ();
  ASSERT ((VOID *) DtbBase != NULL);
  DtbSize = fdt_totalsize ((VOID *)DtbBase);

  // Find the end of overlay DTB region.
  // Overlay DTBs are aligned to 4KB
  DtbNext = ALIGN_VALUE(DtbBase + DtbSize, SIZE_4KB);
  while (DtbNext < MemoryBase + MemorySize) {
    if (fdt_check_header((VOID *)DtbNext) != 0) {
      break;
    }
    DtbNext += fdt_totalsize((VOID *)DtbNext);
    DtbNext = ALIGN_VALUE(DtbNext, SIZE_4KB);
  }
  DtbSize = DtbNext - DtbBase;

  // DTB Base may not be aligned to page boundary. Add overlay to size.
  DtbSize += (DtbBase & EFI_PAGE_MASK);
  DtbSize = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (DtbSize));
  // Align DTB Base to page boundary.
  DtbBase &= ~(EFI_PAGE_MASK);
  DtbOffset = DtbBase - MemoryBase;

  if (DtbBase >= MemoryBase && DtbBase < (MemoryBase + MemorySize)) {
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
    //Default to hob after the FV
    HobBase = MemoryBase + FvOffset + FvSize;
    HobSize = MemorySize - FvSize - FvOffset - StackSize;
    //Unless area before FV is larger
    if (FvOffset > HobSize) {
      HobBase = MemoryBase;
      HobSize = FvOffset;
    }
  }

  if (PerformanceMeasurementEnabled ()) {
    // Initialize the Timer Library to setup the Timer HW controller
    if (!EFI_ERROR (TimerConstructor ())) {
      // We cannot call yet the PerformanceLib because the HOB List has not been initialized
      StartTimeStamp = GetPerformanceCounter ();
    } else {
      StartTimeStamp = 0;
    }
  } else {
    StartTimeStamp = 0;
  }

  // Data Cache enabled on Primary core when MMU is enabled.
  ArmDisableDataCache ();
  // Invalidate instruction cache
  ArmInvalidateInstructionCache ();
  // Enable Instruction Caches on all cores.
  ArmEnableInstructionCache ();

  // Initialize the architecture specific bits
  ArchInitialize ();

  // Initialize the Serial Port
  SerialPortInitialize ();
  CharCount = AsciiSPrint (Buffer,sizeof (Buffer),"%s UEFI firmware (version %s built on %s)\n\r",
    (CHAR16*)PcdGetPtr(PcdPlatformFamilyName),
    (CHAR16*)PcdGetPtr(PcdFirmwareVersionString),
    (CHAR16*)PcdGetPtr(PcdFirmwareDateTimeBuiltString));
  SerialPortWrite ((UINT8 *) Buffer, CharCount);

  // Initialize the Debug Agent for Source Level Debugging
  //InitializeDebugAgent (DEBUG_AGENT_INIT_POSTMEM_SEC, NULL, NULL);
  SaveAndSetDebugTimerInterrupt (TRUE);

  // Declare the PI/UEFI memory region
  HobFree = HobBase + HobSize;
  HobList = HobConstructor (
    (VOID*)HobBase,
    HobSize,
    (VOID*)HobBase,
    (VOID*)HobFree
    );
  PrePeiSetHobList (HobList);

  //Register Firmware volume
  BuildFvHob ((EFI_PHYSICAL_ADDRESS)FvHeader, FvSize);

  // Initialize MMU and Memory HOBs (Resource Descriptor HOBs)
  // Get Virtual Memory Map from the Platform Library
  GetVirtualMemoryMap (&MemoryTable);

  // Build Memory Allocation Hob
  InitMmu (MemoryTable);

  Status = ArmSetMemoryRegionReadOnly (StackBase + StackSize, SIZE_4KB);
  ASSERT_EFI_ERROR (Status);

  // Register UEFI DTB
  RegisterDeviceTree(DtbBase);

  if (FeaturePcdGet (PcdPrePiProduceMemoryTypeInformationHob)) {
    // Optional feature that helps prevent EFI memory map fragmentation.
    BuildMemoryTypeInformationHob ();
  }

  // Create DTB memory allocation HOB
  BuildMemoryAllocationHob (DtbBase, DtbSize, EfiBootServicesData);

  // Create the Stacks HOB (reserve the memory for all stacks)
  BuildStackHob (StackBase, StackSize);

  //TODO: Call CpuPei as a library
  BuildCpuHob (ArmGetPhysicalAddressBits (), ArmGetPhysicalAddressBits ());

  // Store timer value logged at the beginning of firmware image execution
  Performance.ResetEnd = GetTimeInNanoSecond (StartTimeStamp);

  // Build SEC Performance Data Hob
  BuildGuidDataHob (&gEfiFirmwarePerformanceGuid, &Performance, sizeof (Performance));

  // Set the Boot Mode
  SetBootMode (BOOT_WITH_FULL_CONFIGURATION);

  // Register firmware volume
  Status = RegisterFirmwareVolume ((EFI_PHYSICAL_ADDRESS)FvHeader, FvSize);
  ASSERT_EFI_ERROR (Status);

  // Now, the HOB List has been initialized, we can register performance information
  PERF_START (NULL, "PEI", NULL, StartTimeStamp);

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

