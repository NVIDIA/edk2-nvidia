/** @file
*
*  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, Linaro, Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>

#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <libfdt.h>
#include "FloorSweepPrivate.h"

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraDeviceTreeOverlayLib.h>
#include <Library/OpteeLib.h>
#include <Library/BootChainInfoLib.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/Eeprom.h>
#include <IndustryStandard/ArmStdSmc.h>

#define TRUSTY_OS_UID0          0xf025ee40
#define TRUSTY_OS_UID1          0x4c30bca2
#define TRUSTY_OS_UID2          0x73a14c8c
#define TRUSTY_OS_UID3          0xf18a7dc5

typedef struct {
  CONST CHAR8 *Compatibility;
} QSPI_COMPATIBILITY;

EFI_EVENT FdtInstallEvent;
QSPI_COMPATIBILITY gQspiCompatibilityMap[] = {
  { "nvidia,tegra186-qspi" },
  { "nvidia,tegra194-qspi" },
  { "nvidia,tegra23x-qspi" },
  { NULL }
};

VOID
EFIAPI
AddBoardProperties (
  IN VOID *Dtb
  )
{
  EFI_STATUS                  Status;
  TEGRA_EEPROM_BOARD_INFO     *Eeprom;
  INTN                        NodeOffset;
  EFI_HANDLE                  *Handles;
  UINTN                       NoHandles;
  UINTN                       Count;

  Eeprom = NULL;
  Status = gBS->LocateProtocol (&gNVIDIACvmEepromProtocolGuid, NULL, (VOID **)&Eeprom);
  if (!EFI_ERROR (Status)) {
    fdt_setprop (Dtb, 0, "serial-number", &Eeprom->SerialNumber, sizeof (Eeprom->SerialNumber));
    NodeOffset = fdt_path_offset (Dtb, "/chosen");
    if (NodeOffset >= 0) {
      fdt_setprop (Dtb, NodeOffset, "nvidia,sku", &Eeprom->ProductId, sizeof (Eeprom->ProductId));
    }
  }

  Handles = NULL;
  NoHandles = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol, &gNVIDIAEepromProtocolGuid, NULL, &NoHandles, &Handles);
  if (!EFI_ERROR (Status)) {
    for (Count = 0; Count < NoHandles; Count++) {
      Status = gBS->HandleProtocol (Handles[Count], &gNVIDIAEepromProtocolGuid, (VOID **)&Eeprom);
      if (!EFI_ERROR (Status)) {
        NodeOffset = fdt_path_offset (Dtb, "/chosen");
        if (NodeOffset >= 0) {
          fdt_appendprop (Dtb, NodeOffset, "ids", Eeprom->BoardId, strlen (Eeprom->BoardId) + 1);
          fdt_appendprop (Dtb, NodeOffset, "ids", " ", 1);
        }
      }
    }
  }

  NodeOffset = fdt_path_offset (Dtb, "/chosen");
  if (NodeOffset >= 0) {
    fdt_appendprop (Dtb, NodeOffset, "ids", "\n", 1);
  }
}

STATIC
BOOLEAN
IsTrustyPresent (
  VOID
)
{
  ARM_SMC_ARGS ArmSmcArgs;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));
  // Send a Trusted OS Calls UID command
  ArmSmcArgs.Arg0 = ARM_SMC_ID_TOS_UID;
  ArmCallSmc (&ArmSmcArgs);

  if ((ArmSmcArgs.Arg0 == TRUSTY_OS_UID0) &&
      (ArmSmcArgs.Arg1 == TRUSTY_OS_UID1) &&
      (ArmSmcArgs.Arg2 == TRUSTY_OS_UID2) &&
      (ArmSmcArgs.Arg3 == TRUSTY_OS_UID3)) {
    return TRUE;
  } else {
    return FALSE;
  }
}

STATIC
VOID
EnableTrustyNode (
  IN VOID *Dtb
 )
{
  INT32 TrustyNodeOffset;
  INT32 Ret;

  TrustyNodeOffset = fdt_path_offset (Dtb, "/trusty");
  if (TrustyNodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Trusty Node not found %d\n", __FUNCTION__,
              TrustyNodeOffset));
    return;
  }

  Ret = fdt_setprop (Dtb, TrustyNodeOffset, "status", "okay", sizeof("okay"));
  if (Ret != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to add status Property %d\n",
              __FUNCTION__, Ret));
    return;
  }
}


STATIC
VOID
EnableOpteeNode (
  IN VOID *Dtb
 )
{
  INT32 OpteeNodeOffset;
  INT32 Ret;

  OpteeNodeOffset = fdt_path_offset (Dtb, "/firmware/optee");
  if (OpteeNodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Optee Node not found %d\n", __FUNCTION__,
              OpteeNodeOffset));
    return;
  }

  Ret = fdt_setprop (Dtb, OpteeNodeOffset, "status", "okay", sizeof("okay"));
  if (Ret != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to add status Property %d\n",
              __FUNCTION__, Ret));
    return;
  }
}

VOID
EFIAPI
RemoveQspiNodes (
  IN VOID *Dtb
  )
{
  QSPI_COMPATIBILITY *Map;
  INT32              NodeOffset;

  if (GetBootType () == TegrablBootRcm) {
    return;
  }

  Map = gQspiCompatibilityMap;

  while (Map->Compatibility != NULL) {
    NodeOffset = fdt_node_offset_by_compatible(Dtb, 0, Map->Compatibility);
    while (NodeOffset >= 0) {
      if ((fdt_subnode_offset (Dtb, NodeOffset, "flash@0") >= 0) ||
          (fdt_subnode_offset (Dtb, NodeOffset, "spiflash@0") >= 0)) {
        fdt_del_node (Dtb, NodeOffset);
      }
      NodeOffset = fdt_node_offset_by_compatible(Dtb, NodeOffset, Map->Compatibility);
    }
    Map++;
  }
}

VOID
EFIAPI
FdtInstalled (
    IN EFI_EVENT Event,
    IN VOID      *Context
    )
{
  EFI_STATUS Status;
  VOID       *Dtb;
  VOID       *CpublDtb;
  VOID       *OverlayDtb;
  INT32      NodeOffset;
  CHAR8      SWModule[] = "kernel";

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &Dtb);
  if (EFI_ERROR (Status)) {
    return;
  }

  //Apply kernel-dtb overlays
  CpublDtb = (VOID *)(UINTN)GetDTBBaseAddress ();
  OverlayDtb = (VOID *)ALIGN_VALUE ((UINTN)CpublDtb + fdt_totalsize (CpublDtb), SIZE_4KB);
  if (fdt_check_header(OverlayDtb) == 0) {
    Status = ApplyTegraDeviceTreeOverlay(Dtb, OverlayDtb, SWModule);
    if (EFI_ERROR (Status)) {
      return;
    }
  }

  //Remove plugin-manager node for device trees.
  NodeOffset = fdt_path_offset (Dtb, "/plugin-manager");
  if (NodeOffset >= 0) {
    fdt_del_node ((VOID *)Dtb, NodeOffset);
  }

  //Remove grid of semaphores as we do not set up memory for this
  NodeOffset = fdt_path_offset (Dtb, "/reserved-memory/grid-of-semaphores");
  if (NodeOffset > 0) {
    fdt_del_node (Dtb, NodeOffset);
  }

  UpdateCpuFloorsweepingConfig (Dtb);
  RemoveQspiNodes (Dtb);
  AddBoardProperties (Dtb);
  if (IsOpteePresent ()) {
    EnableOpteeNode (Dtb);
  } else if (IsTrustyPresent ()) {
    EnableTrustyNode (Dtb);
  }
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
  CHAR16                      PartitionName[MAX_PARTITION_NAME_LEN];
  UINTN                       NumOfHandles;
  EFI_HANDLE                  *HandleBuffer = NULL;
  UINT64                      Size;
  EFI_PARTITION_INFO_PROTOCOL *PartitionInfo = NULL;
  BOOLEAN                     ValidFlash;
  BOOLEAN                     DtbLocAdjusted;
  UINTN                       Index;
  EFI_BLOCK_IO_PROTOCOL       *BlockIo;
  VOID                        *DtbAllocated;
  VOID                        *DtbCopy;

  ValidFlash = FALSE;
  DtbLocAdjusted = FALSE;
  *Dtb = NULL;
  DtbAllocated = NULL;
  DtbCopy = NULL;

  Status = GetActivePartitionName (PcdGetPtr(PcdKernelDtbPartitionName), PartitionName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

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
                          PartitionName,
                          StrnLenS(PartitionName, sizeof(PartitionInfo->Info.Gpt.PartitionName)))) {
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
                                  (VOID **)&DtbAllocated);

      if (EFI_ERROR(Status) || (DtbAllocated == NULL)) {
        ValidFlash = FALSE;
        break;
      }
      *Dtb = DtbAllocated;
      Status = BlockIo->ReadBlocks (BlockIo,
                                    BlockIo->Media->MediaId,
                                    0,
                                    Size,
                                    *Dtb);
      if (EFI_ERROR(Status)) {
        ValidFlash = FALSE;
        break;
      }

      if(fdt_check_header (*Dtb) != 0) {
        *Dtb += PcdGet32 (PcdSignedImageHeaderSize);
        if(fdt_check_header (*Dtb) != 0) {
          DEBUG ((DEBUG_ERROR, "%a: DTB on partition was corrupted, attempt use to UEFI DTB\r\n", __FUNCTION__));
          ValidFlash = FALSE;
          break;
        }
      }
    } while (FALSE);
  }

  if (!ValidFlash) {
    DEBUG ((DEBUG_ERROR, "%a: Using UEFI DTB\r\n", __FUNCTION__));
    *Dtb = (VOID *)(UINTN)GetDTBBaseAddress ();
    if(fdt_check_header (*Dtb) != 0) {
      DEBUG ((DEBUG_ERROR, "%a: UEFI DTB corrupted\r\n", __FUNCTION__));
      Status = EFI_NOT_FOUND;
      goto Exit;
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Using Kernel DTB\r\n", __FUNCTION__));
  }

  //Double the size taken by DTB to have enough buffer to accommodate
  //any runtime additions made to it.
  DtbCopy = AllocatePages (EFI_SIZE_TO_PAGES (2 * fdt_totalsize (*Dtb)));
  if (fdt_open_into (*Dtb, DtbCopy, 2 * fdt_totalsize (*Dtb)) != 0) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  *Dtb = DtbCopy;
  *DtbSize = fdt_totalsize (*Dtb);

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                               TPL_CALLBACK,
                               FdtInstalled,
                               NULL,
                               &gFdtTableGuid,
                               &FdtInstallEvent);

Exit:
  if (DtbAllocated != NULL) {
    gBS->FreePool (DtbAllocated);
    DtbAllocated = NULL;
  }

  if (HandleBuffer != NULL) {
    gBS->FreePool (HandleBuffer);
    HandleBuffer = NULL;
  }

  if (EFI_ERROR (Status)) {
    if (DtbCopy != NULL) {
      gBS->FreePool (DtbCopy);
      DtbCopy = NULL;
    }
    *Dtb = NULL;
    *DtbSize = 0;
  }

  return Status;
}
