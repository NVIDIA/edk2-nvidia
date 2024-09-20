/** @file
  Memory Allocation Library instance dedicated to running before the HOBs are setup and
  specifically meant during the early StMM boot to setup the MMU translations.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MmServicesTableLib.h>
#include <Library/StandaloneMmHafniumSlabMemAllocLib.h>

/*
 * Dynamic Memory is not enabled at this early point, so implement a simple slab-based allocator to replace
 * calls to "AllocatePages"

#define AllocatePages  AllocatePagesFromSlab
*/
STATIC UINT8  *AllocationSlab       = NULL;
STATIC UINT8  *LastAllocatedSlabPtr = NULL;
STATIC UINT8  AllocatedPages        = 0;
STATIC UINT8  MaxPages              = 0;

VOID
EFIAPI
SlabArmSetEntriesSlab (
  IN  UINT64  EntriesBase,
  IN  UINT64  EntriesPages
  )
{
  MaxPages       = EntriesPages;
  AllocationSlab = (UINT8 *)EntriesBase;
}

STATIC
VOID *
AllocatePagesFromSlab (
  IN UINTN  RequestedPages
  )
{
  VOID  *SlabPointer;

  if ((AllocatedPages >= MaxPages) || ((AllocatedPages + RequestedPages) >= MaxPages)) {
    DEBUG ((DEBUG_ERROR, "%a: Exhausted stage-1 entries memory Allocated %u Max %u\r\n", __FUNCTION__, AllocatedPages, MaxPages));
    ASSERT (0);
  }

  // SlabPointer     = ALIGN_POINTER ((VOID *)&AllocationSlab[(AllocatedPages * SIZE_4KB)], SIZE_4KB);
  if (LastAllocatedSlabPtr == NULL) {
    LastAllocatedSlabPtr = AllocationSlab;
  }

  SlabPointer          = ALIGN_POINTER ((VOID *)(LastAllocatedSlabPtr + (RequestedPages * SIZE_4KB)), SIZE_4KB);
  AllocatedPages      += RequestedPages;
  LastAllocatedSlabPtr = SlabPointer;

  DEBUG ((DEBUG_ERROR, "%a: Allocated %u Pages Max-Pages %u \n", __FUNCTION__, AllocatedPages, MaxPages));
  DEBUG ((DEBUG_ERROR, "%a: SlabPointer %p LastAllocated %p \n", __FUNCTION__, AllocationSlab, LastAllocatedSlabPtr));
  return SlabPointer;
}

VOID *
EFIAPI
AllocatePagesSlabMmSt (
  IN UINTN  Pages
  )
{
  EFI_PHYSICAL_ADDRESS  Memory;
  VOID                  *Buf;

  if (gMmst != NULL) {
    DEBUG ((DEBUG_ERROR, "%a: MmSt Allocate %u Pages \n", __FUNCTION__, Pages));
    gMmst->MmAllocatePages (AllocateAnyPages, EfiRuntimeServicesData, Pages, &Memory);
    Buf = (VOID *)Memory;
  } else {
    Buf = AllocatePagesFromSlab (Pages);
  }

  return Buf;
}

VOID
EFIAPI
FreePagesSlabMmst (
  IN VOID   *Buffer,
  IN UINTN  Pages
  )
{
  EFI_STATUS  Status;

  ASSERT (Pages != 0);
  if (gMmst != NULL) {
    Status = gMmst->MmFreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)Buffer, Pages);
    ASSERT_EFI_ERROR (Status);
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Free Slab not implemented", __FUNCTION__));
  }
}
