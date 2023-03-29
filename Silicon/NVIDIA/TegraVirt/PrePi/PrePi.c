/** @file
*
*  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2011-2014, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/PrePiLib.h>
#include <Library/PrintLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/TimerLib.h>
#include <Library/PerformanceLib.h>
#include <Library/CacheMaintenanceLib.h>

#include <libfdt.h>

#include "PrePi.h"

VOID
PrePiMain (
  IN  UINTN   UefiMemoryBase,
  IN  UINTN   StacksBase,
  IN  UINT64  StartTimeStamp
  )
{
  EFI_HOB_HANDOFF_INFO_TABLE  *HobList;
  EFI_STATUS                  Status;
  CHAR8                       Buffer[120];
  UINTN                       CharCount;
  UINTN                       StacksSize;
  UINTN                       UefiMemorySize;
  UINTN                       HobBase;
  UINTN                       HobSize;
  UINTN                       DtbBase;
  UINTN                       DtbSize;
  EFI_FIRMWARE_VOLUME_HEADER  *FvHeader;
  UINT64                      FvSize;
  UINT64                      FvOffset = 0;

  // Initialize the architecture specific bits
  ArchInitialize ();

  /////////////////////////////
  // Serial port
  /////////////////////////////

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

  /////////////////////////////
  // Memory
  /////////////////////////////

  UefiMemorySize = PcdGet64 (PcdSystemMemorySize);

  DEBUG_CODE_BEGIN ();
  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "Memory: 0x%lx-0x%lx (0x%lx)\n\r",
                UefiMemoryBase,
                UefiMemoryBase + UefiMemorySize,
                UefiMemorySize
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);
  DEBUG_CODE_END ();

  /////////////////////////////
  // Stack
  /////////////////////////////

  StacksSize = FixedPcdGet32 (PcdCPUCorePrimaryStackSize);

  DEBUG_CODE_BEGIN ();
  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "Stack : 0x%lx-0x%lx (0x%lx)\n\r",
                StacksBase,
                StacksBase + StacksSize,
                StacksSize
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);
  DEBUG_CODE_END ();

  /////////////////////////////
  // FV
  /////////////////////////////

  FvSize = FixedPcdGet32 (PcdFvSize);

  // Find the FV header.  We expect it on a 64KB boundary.
  FvHeader = NULL;
  while (FvOffset < UefiMemorySize) {
    FvHeader = (EFI_FIRMWARE_VOLUME_HEADER *)(VOID *)(UefiMemoryBase + FvOffset);
    if (FvHeader->Signature == EFI_FVH_SIGNATURE) {
      break;
    }

    FvOffset += SIZE_64KB;
  }

  ASSERT (FvOffset < UefiMemorySize);
  ASSERT (FvHeader != NULL);

  // Share Fv location with Arm libraries
  PatchPcdSet64 (PcdFvBaseAddress, (UINT64)FvHeader);

  DEBUG_CODE_BEGIN ();
  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "FV    : 0x%lx-0x%lx (0x%lx)\n\r",
                (EFI_PHYSICAL_ADDRESS)FvHeader,
                (EFI_PHYSICAL_ADDRESS)FvHeader + FvSize,
                FvSize
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);
  DEBUG_CODE_END ();

  /////////////////////////////
  // DTB
  /////////////////////////////

  DtbBase = PcdGet64 (PcdDeviceTreeInitialBaseAddress);
  ASSERT ((VOID *)DtbBase != NULL);
  DtbSize = fdt_totalsize ((VOID *)DtbBase);
  DtbSize = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (DtbSize));

  DEBUG_CODE_BEGIN ();
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

  /////////////////////////////
  // HOB
  /////////////////////////////

  // Use the memory region between the DTB and the Stack for the HOB.
  HobBase = DtbBase + DtbSize;
  HobSize = StacksBase - HobBase;

  DEBUG_CODE_BEGIN ();
  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "Hob   : 0x%lx-0x%lx (0x%lx)\n\r",
                HobBase,
                HobBase + HobSize,
                HobSize
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);
  DEBUG_CODE_END ();

  /////////////////////////////
  // HOB init
  /////////////////////////////

  // Create the HOB and declare the PI/UEFI memory region
  HobList = HobConstructor (
              (VOID *)HobBase,
              HobSize,
              (VOID *)HobBase,
              (VOID *)HobBase + HobSize
              );
  PrePeiSetHobList (HobList);

  // Create the Stack HOB (reserve the memory for all stacks)
  BuildStackHob (StacksBase, StacksSize);

  // Create DTB memory allocation HOB
  BuildMemoryAllocationHob (DtbBase, DtbSize, EfiBootServicesData);

  // TODO: Call CpuPei as a library
  BuildCpuHob (ArmGetPhysicalAddressBits (), PcdGet8 (PcdPrePiCpuIoSize));

  /////////////////////////////
  // MMU
  /////////////////////////////

  // Ensure that the loaded image is invalidated in the caches, so that any
  // modifications we made with the caches and MMU off (such as the applied
  // relocations) don't become invisible once we turn them on.
  InvalidateDataCacheRange ((VOID *)FvHeader, FvSize);

  // Initialize MMU and Memory HOBs (Resource Descriptor HOBs)
  Status = MemoryPeim (UefiMemoryBase, UefiMemorySize);
  ASSERT_EFI_ERROR (Status);

  // Set the Boot Mode
  SetBootMode (BOOT_WITH_FULL_CONFIGURATION);

  // Initialize Platform HOBs (CpuHob and FvHob)
  Status = PlatformPeim ();
  ASSERT_EFI_ERROR (Status);

  // Now, the HOB List has been initialized, we can register performance information
  PERF_START (NULL, "PEI", NULL, StartTimeStamp);

  // SEC phase needs to run library constructors by hand.
  ProcessLibraryConstructorList ();

  /////////////////////////////
  // Launch DXE
  /////////////////////////////

  // Assume the FV that contains the SEC (our code) also contains a compressed FV.
  Status = DecompressFirstFv ();
  ASSERT_EFI_ERROR (Status);

  // Load the DXE Core and transfer control to it
  Status = LoadDxeCoreFromFv (NULL, 0);
  ASSERT_EFI_ERROR (Status);
}

VOID
CEntryPoint (
  IN  UINTN  MpId,
  IN  UINTN  UefiMemoryBase,
  IN  UINTN  StacksBase
  )
{
  UINT64  StartTimeStamp;

  if (PerformanceMeasurementEnabled ()) {
    // Initialize the Timer Library to setup the Timer HW controller
    TimerConstructor ();
    // We cannot call yet the PerformanceLib because the HOB List has not been initialized
    StartTimeStamp = GetPerformanceCounter ();
  } else {
    StartTimeStamp = 0;
  }

  // Data Cache enabled on Primary core when MMU is enabled.
  ArmDisableDataCache ();
  // Invalidate instruction cache
  ArmInvalidateInstructionCache ();
  // Enable Instruction Caches on all cores.
  ArmEnableInstructionCache ();

  PrePiMain (UefiMemoryBase, StacksBase, StartTimeStamp);

  // DXE Core should always load and never return
  ASSERT (FALSE);
}
