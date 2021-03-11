/** @file
*
*  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
*  Copyright (c) 2017, Linaro, Ltd. All rights reserved.
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

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <libfdt.h>
#include "FloorSweepPrivate.h"

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>

EFI_EVENT FdtInstallEvent;

VOID
EFIAPI
FdtInstalled (
    IN EFI_EVENT Event,
    IN VOID      *Context
    )
{
  EFI_STATUS Status;
  VOID       *Dtb;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &Dtb);
  if (EFI_ERROR (Status)) {
    return;
  }

  UpdateCpuFloorsweepingConfig (Dtb);
}

/**
  Return a pool allocated copy of the DTB image that is appropriate for
  booting the current platform via DT.

  @param[out]   Dtb                   Pointer to the DTB copy
  @param[out]   DtbSize               Size of the DTB copy

  @retval       EFI_SUCCESS           Operation completed successfully
  @retval       EFI_NOT_FOUND         No suitable DTB image could be located
  @retval       EFI_OUT_OF_RESOURCES  No pool memory available

**/
EFI_STATUS
EFIAPI
DtPlatformLoadDtb (
  OUT   VOID        **Dtb,
  OUT   UINTN       *DtbSize
  )
{
  EFI_STATUS                  Status;
  UINTN                       NumOfHandles;
  EFI_HANDLE                  *HandleBuffer = NULL;
  UINT64                      Size;
  EFI_PARTITION_INFO_PROTOCOL *PartitionInfo = NULL;
  BOOLEAN                     ValidFlash;
  BOOLEAN                     DtbLocAdjusted;
  UINTN                       Index;
  VOID                        *Hob = NULL;
  EFI_BLOCK_IO_PROTOCOL       *BlockIo;
  INT32                       NodeOffset;
  INT32                       DtStatus;
  VOID                        *DtbCopy;

  ValidFlash = FALSE;
  DtbLocAdjusted = FALSE;
  *Dtb = NULL;

  if (!PcdGetBool(PcdEmuVariableNvModeEnable)) {
    do {
      Status = gBS->LocateHandleBuffer (ByProtocol,
                                        &gEfiPartitionInfoProtocolGuid,
                                        NULL,
                                        &NumOfHandles,
                                        &HandleBuffer);

      if (EFI_ERROR(Status)) {
        Status = EFI_UNSUPPORTED;
        break;
      }

      for (Index = 0; Index < NumOfHandles; Index++) {
        Status = gBS->HandleProtocol (HandleBuffer[Index],
                                    &gEfiPartitionInfoProtocolGuid,
                                    (VOID **)&PartitionInfo);

        if (EFI_ERROR(Status) || (PartitionInfo == NULL)) {
          continue;
        }

        if (PartitionInfo->Info.Gpt.StartingLBA > PartitionInfo->Info.Gpt.EndingLBA) {
          continue;
        }

        if (PartitionInfo->Type != PARTITION_TYPE_GPT) {
          continue;
        }

        if (0 == StrnCmp (PartitionInfo->Info.Gpt.PartitionName,
                          PcdGetPtr(PcdKernelDtbPartitionName),
                          StrnLenS(PcdGetPtr(PcdKernelDtbPartitionName), sizeof(PartitionInfo->Info.Gpt.PartitionName)))) {
          break;
        }
      }

      if (Index == NumOfHandles) {
        break;
      }

      ValidFlash = TRUE;

      Status = gBS->HandleProtocol (HandleBuffer[Index],
                                  &gEfiBlockIoProtocolGuid,
                                  (VOID **)&BlockIo);

      if (EFI_ERROR(Status) || (BlockIo == NULL)) {
        ValidFlash = FALSE;
        break;
      }

      Size = MultU64x32 (BlockIo->Media->LastBlock+1, BlockIo->Media->BlockSize);

      *Dtb = NULL;
      Status = gBS->AllocatePool (EfiBootServicesData,
                                  Size,
                                  (VOID **)Dtb);

      if (EFI_ERROR(Status) || (*Dtb == NULL)) {
        ValidFlash = FALSE;
        break;
      }
      Status = BlockIo->ReadBlocks (BlockIo,
                                    BlockIo->Media->MediaId,
                                    0,
                                    Size,
                                    *Dtb);
      if (EFI_ERROR(Status)) {
        ValidFlash = FALSE;
        break;
      }
    } while (FALSE);
  }

  if (HandleBuffer != NULL) {
    gBS->FreePool (HandleBuffer);
    HandleBuffer = NULL;
  }

  if (!ValidFlash) {
    if (*Dtb != NULL) {
      gBS->FreePool (*Dtb);
      *Dtb = NULL;
    }
    Hob = GetFirstGuidHob (&gFdtHobGuid);
    if (Hob == NULL || GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64)) {
      return EFI_NOT_FOUND;
    }
    *Dtb = (VOID *)(UINTN)*(UINT64 *)GET_GUID_HOB_DATA (Hob);
    //Double the size taken by DTB to have enough buffer to accommodate
    //any runtime additions made to it.
    DtbCopy = AllocatePages (EFI_SIZE_TO_PAGES (2 * fdt_totalsize (*Dtb)));
    if (fdt_open_into (*Dtb, DtbCopy, 2 * fdt_totalsize (*Dtb)) != 0) {
      return EFI_NOT_FOUND;
    }
    *Dtb = DtbCopy;
  }

  if (fdt_check_header (*Dtb) != 0) {
    if (ValidFlash) {
      *Dtb += PcdGet32 (PcdBootImgSigningHeaderSize);
      DtbLocAdjusted = TRUE;
    }
    if (fdt_check_header (*Dtb) != 0) {
      DEBUG ((DEBUG_ERROR, "%a: No DTB found @ 0x%p\n", __FUNCTION__,
        *Dtb));
      return EFI_NOT_FOUND;
    }
  }

  if (ValidFlash) {
    //Double the size taken by DTB to have enough buffer to accommodate
    //any runtime additions made to it.
    DtbCopy = AllocatePages (EFI_SIZE_TO_PAGES (2 * fdt_totalsize (*Dtb)));
    if (fdt_open_into (*Dtb, DtbCopy, 2 * fdt_totalsize (*Dtb)) != 0) {
      return EFI_NOT_FOUND;
    }
    if (*Dtb != NULL) {
      gBS->FreePool (DtbLocAdjusted ? *Dtb - PcdGet32 (PcdBootImgSigningHeaderSize) : *Dtb);
      *Dtb = DtbCopy;
    }
  }

  //Check floorsweeping here in addition to callback to allow for users of this API who
  //do not install the Dtb into the system table to get correct data.
  UpdateCpuFloorsweepingConfig (*Dtb);

  //Disable grid of semaphores as we do not set up memory for this
  NodeOffset = fdt_path_offset (*Dtb, "/reserved-memory/grid-of-semaphores");
  if (NodeOffset > 0) {
    DtStatus = fdt_setprop (*Dtb, NodeOffset, "status", "disabled", sizeof("disabled"));
    if (DtStatus != 0) {
      DEBUG ((DEBUG_ERROR, "Failed to disable grid-of-semaphores %d\r\n", DtStatus));
    }
  }

  *DtbSize = fdt_totalsize (*Dtb);

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                               TPL_NOTIFY,
                               FdtInstalled,
                               NULL,
                               &gFdtTableGuid,
                               &FdtInstallEvent);

  return EFI_SUCCESS;
}
