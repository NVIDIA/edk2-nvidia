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
#include <Library/FloorSweepingLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraDeviceTreeOverlayLib.h>
#include <Library/OpteeLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/Eeprom.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <NVIDIAConfiguration.h>
#include <libfdt.h>

#define TRUSTY_OS_UID0  0xf025ee40
#define TRUSTY_OS_UID1  0x4c30bca2
#define TRUSTY_OS_UID2  0x73a14c8c
#define TRUSTY_OS_UID3  0xf18a7dc5

typedef struct {
  CONST CHAR8    *Compatibility;
} QSPI_COMPATIBILITY;

EFI_EVENT  FdtInstallEvent;
EFI_EVENT  ReadyToBootEvent;

QSPI_COMPATIBILITY  gQspiCompatibilityMap[] = {
  { "nvidia,tegra186-qspi" },
  { "nvidia,tegra194-qspi" },
  { "nvidia,tegra23x-qspi" },
  { NULL                   }
};

VOID
EFIAPI
AddBoardProperties (
  IN VOID  *Dtb
  )
{
  EFI_STATUS               Status;
  TEGRA_EEPROM_BOARD_INFO  *Eeprom;
  INTN                     NodeOffset;
  EFI_HANDLE               *Handles;
  UINTN                    NoHandles;
  UINTN                    Count;
  CHAR8                    *CameraId;

  Eeprom = NULL;
  Status = gBS->LocateProtocol (&gNVIDIACvmEepromProtocolGuid, NULL, (VOID **)&Eeprom);
  if (!EFI_ERROR (Status)) {
    fdt_setprop (Dtb, 0, "serial-number", &Eeprom->SerialNumber, sizeof (Eeprom->SerialNumber));
    NodeOffset = fdt_path_offset (Dtb, "/chosen");
    if (NodeOffset >= 0) {
      fdt_setprop (Dtb, NodeOffset, "nvidia,sku", &Eeprom->ProductId, sizeof (Eeprom->ProductId));
    }
  }

  Handles   = NULL;
  NoHandles = 0;
  Status    = gBS->LocateHandleBuffer (ByProtocol, &gNVIDIAEepromProtocolGuid, NULL, &NoHandles, &Handles);
  if (!EFI_ERROR (Status)) {
    for (Count = 0; Count < NoHandles; Count++) {
      Status = gBS->HandleProtocol (Handles[Count], &gNVIDIAEepromProtocolGuid, (VOID **)&Eeprom);
      if (!EFI_ERROR (Status)) {
        NodeOffset = fdt_path_offset (Dtb, "/chosen");
        if (NodeOffset >= 0) {
          CameraId = AsciiStrStr (Eeprom->ProductId, CAMERA_EEPROM_PART_NAME);
          if (CameraId == NULL) {
            fdt_appendprop (Dtb, NodeOffset, "ids", Eeprom->BoardId, strlen (Eeprom->BoardId) + 1);
          } else {
            fdt_appendprop (Dtb, NodeOffset, "ids", CameraId, strlen (CameraId) + 1);
          }

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
  ARM_SMC_ARGS  ArmSmcArgs;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));
  // Send a Trusted OS Calls UID command
  ArmSmcArgs.Arg0 = ARM_SMC_ID_TOS_UID;
  ArmCallSmc (&ArmSmcArgs);

  if ((ArmSmcArgs.Arg0 == TRUSTY_OS_UID0) &&
      (ArmSmcArgs.Arg1 == TRUSTY_OS_UID1) &&
      (ArmSmcArgs.Arg2 == TRUSTY_OS_UID2) &&
      (ArmSmcArgs.Arg3 == TRUSTY_OS_UID3))
  {
    return TRUE;
  } else {
    return FALSE;
  }
}

STATIC
VOID
EnableTrustyNode (
  IN VOID  *Dtb
  )
{
  INT32  TrustyNodeOffset;
  INT32  Ret;

  TrustyNodeOffset = fdt_path_offset (Dtb, "/trusty");
  if (TrustyNodeOffset < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Trusty Node not found %d\n",
      __FUNCTION__,
      TrustyNodeOffset
      ));
    return;
  }

  Ret = fdt_setprop (Dtb, TrustyNodeOffset, "status", "okay", sizeof ("okay"));
  if (Ret != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to add status Property %d\n",
      __FUNCTION__,
      Ret
      ));
    return;
  }
}

STATIC
VOID
EnableOpteeNode (
  IN VOID  *Dtb
  )
{
  INT32  OpteeNodeOffset;
  INT32  Ret;

  OpteeNodeOffset = fdt_path_offset (Dtb, "/firmware/optee");
  if (OpteeNodeOffset < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Optee Node not found %d\n",
      __FUNCTION__,
      OpteeNodeOffset
      ));
    return;
  }

  Ret = fdt_setprop (Dtb, OpteeNodeOffset, "status", "okay", sizeof ("okay"));
  if (Ret != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to add status Property %d\n",
      __FUNCTION__,
      Ret
      ));
    return;
  }
}

VOID
EFIAPI
RemoveQspiNodes (
  IN VOID  *Dtb
  )
{
  QSPI_COMPATIBILITY  *Map;
  INT32               NodeOffset;

  if (GetBootType () == TegrablBootRcm) {
    return;
  }

  Map = gQspiCompatibilityMap;

  while (Map->Compatibility != NULL) {
    NodeOffset = fdt_node_offset_by_compatible (Dtb, 0, Map->Compatibility);
    while (NodeOffset >= 0) {
      if ((fdt_subnode_offset (Dtb, NodeOffset, "flash@0") >= 0) ||
          (fdt_subnode_offset (Dtb, NodeOffset, "spiflash@0") >= 0))
      {
        fdt_del_node (Dtb, NodeOffset);
      }

      NodeOffset = fdt_node_offset_by_compatible (Dtb, NodeOffset, Map->Compatibility);
    }

    Map++;
  }
}

VOID
EFIAPI
UpdateRamOopsMemory (
  IN VOID  *Dtb
  )
{
  EFI_STATUS            Status;
  VOID                  *Hob;
  EFI_PHYSICAL_ADDRESS  RamOopsBase;
  UINT64                RamOopsSize;
  INT32                 NodeOffset;
  INT32                 AddressCells;
  INT32                 SizeCells;
  UINT8                 *Data;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    RamOopsBase = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ResourceInfo->RamOopsRegion.MemoryBaseAddress;
    RamOopsSize = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ResourceInfo->RamOopsRegion.MemoryLength;
    DEBUG ((DEBUG_ERROR, "%a: RamOopsBase: 0x%lx, RamOopsSize: 0x%lx\r\n", __FUNCTION__, RamOopsBase, RamOopsSize));
  } else {
    DEBUG ((DEBUG_ERROR, "%a: RamOopsBase Unsupported\r\n", __FUNCTION__));
    return;
  }

  if ((RamOopsBase != 0) && (RamOopsSize != 0)) {
    NodeOffset   = fdt_node_offset_by_compatible (Dtb, 0, "ramoops");
    AddressCells = fdt_address_cells (Dtb, fdt_parent_offset (Dtb, NodeOffset));
    SizeCells    = fdt_size_cells (Dtb, fdt_parent_offset (Dtb, NodeOffset));
    if ((AddressCells > 2) ||
        (AddressCells == 0) ||
        (SizeCells > 2) ||
        (SizeCells == 0))
    {
      DEBUG ((DEBUG_ERROR, "%a: Bad cell values, %d, %d\r\n", __FUNCTION__, AddressCells, SizeCells));
      return;
    }

    Data   = NULL;
    Status = gBS->AllocatePool (
                    EfiBootServicesData,
                    (AddressCells + SizeCells) * sizeof (UINT32),
                    (VOID **)&Data
                    );
    if (EFI_ERROR (Status)) {
      return;
    }

    if (AddressCells == 2) {
      *(UINT64 *)Data = SwapBytes64 (RamOopsBase);
    } else {
      *(UINT32 *)Data = SwapBytes32 (RamOopsBase);
    }

    if (SizeCells == 2) {
      *(UINT64 *)&Data[AddressCells * sizeof (UINT32)] = SwapBytes64 (RamOopsSize);
    } else {
      *(UINT32 *)&Data[AddressCells * sizeof (UINT32)] = SwapBytes32 (RamOopsSize);
    }

    fdt_setprop (Dtb, NodeOffset, "reg", Data, (AddressCells + SizeCells) * sizeof (UINT32));
    fdt_setprop (Dtb, NodeOffset, "status", "okay", sizeof ("okay"));

    gBS->FreePool (Data);
  }
}

VOID
EFIAPI
UpdateFdt (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;
  VOID        *AcpiBase;
  VOID        *Dtb;
  VOID        *CpublDtb;
  VOID        *OverlayDtb;
  INT32       NodeOffset;
  CHAR8       SWModule[] = "kernel";

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    return;
  }

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &Dtb);
  if (EFI_ERROR (Status)) {
    return;
  }

  // Apply kernel-dtb overlays
  CpublDtb   = (VOID *)(UINTN)GetDTBBaseAddress ();
  OverlayDtb = (VOID *)ALIGN_VALUE ((UINTN)CpublDtb + fdt_totalsize (CpublDtb), SIZE_4KB);
  if (fdt_check_header (OverlayDtb) == 0) {
    Status = ApplyTegraDeviceTreeOverlay (Dtb, OverlayDtb, SWModule);
    if (EFI_ERROR (Status)) {
      return;
    }
  }

  // Remove plugin-manager node for device trees.
  NodeOffset = fdt_path_offset (Dtb, "/plugin-manager");
  if (NodeOffset >= 0) {
    fdt_del_node ((VOID *)Dtb, NodeOffset);
  }

  // Remove grid of semaphores as we do not set up memory for this
  NodeOffset = fdt_path_offset (Dtb, "/reserved-memory/grid-of-semaphores");
  if (NodeOffset > 0) {
    fdt_del_node (Dtb, NodeOffset);
  }

  FloorSweepDtb (Dtb);
  RemoveQspiNodes (Dtb);
  AddBoardProperties (Dtb);
  UpdateRamOopsMemory (Dtb);
  if (IsOpteePresent ()) {
    EnableOpteeNode (Dtb);
  } else if (IsTrustyPresent ()) {
    EnableTrustyNode (Dtb);
  }
}

VOID
EFIAPI
InstallFdt (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                   Status;
  VOID                         *AcpiBase;
  VOID                         *CurrentDtb;
  CHAR16                       PartitionName[MAX_PARTITION_NAME_LEN];
  UINTN                        NumOfHandles;
  EFI_HANDLE                   *HandleBuffer;
  UINT64                       Size;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  BOOLEAN                      DtbLocAdjusted;
  UINTN                        Index;
  EFI_BLOCK_IO_PROTOCOL        *BlockIo;
  VOID                         *KernelDtb;
  VOID                         *Dtb;
  VOID                         *DtbCopy;
  UINTN                        DataSize;
  UINT32                       BootMode;

  HandleBuffer   = NULL;
  PartitionInfo  = NULL;
  DtbLocAdjusted = FALSE;
  KernelDtb      = NULL;
  Dtb            = NULL;
  DtbCopy        = NULL;

  gBS->CloseEvent (Event);

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    return;
  }

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &CurrentDtb);
  if (EFI_ERROR (Status)) {
    return;
  }

  DataSize = sizeof (BootMode);
  Status   = gRT->GetVariable (L4T_BOOTMODE_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, NULL, &DataSize, &BootMode);
  if (!EFI_ERROR (Status) && (BootMode == NVIDIA_L4T_BOOTMODE_RECOVERY)) {
    StrCpyS (PartitionName, MAX_PARTITION_NAME_LEN, PcdGetPtr (PcdRecoveryKernelDtbPartitionName));
  } else {
    Status = GetActivePartitionName (PcdGetPtr (PcdKernelDtbPartitionName), PartitionName);
    if (EFI_ERROR (Status)) {
      return;
    }
  }

  do {
    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEfiPartitionInfoProtocolGuid,
                    NULL,
                    &NumOfHandles,
                    &HandleBuffer
                    );

    if (EFI_ERROR (Status)) {
      return;
    }

    for (Index = 0; Index < NumOfHandles; Index++) {
      Status = gBS->HandleProtocol (
                      HandleBuffer[Index],
                      &gEfiPartitionInfoProtocolGuid,
                      (VOID **)&PartitionInfo
                      );

      if (EFI_ERROR (Status) || (PartitionInfo == NULL)) {
        continue;
      }

      if (PartitionInfo->Info.Gpt.StartingLBA > PartitionInfo->Info.Gpt.EndingLBA) {
        continue;
      }

      if (PartitionInfo->Type != PARTITION_TYPE_GPT) {
        continue;
      }

      if (0 == StrCmp (
                 PartitionInfo->Info.Gpt.PartitionName,
                 PartitionName
                 ))
      {
        break;
      }
    }

    if (Index == NumOfHandles) {
      break;
    }

    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiBlockIoProtocolGuid,
                    (VOID **)&BlockIo
                    );
    if (EFI_ERROR (Status) || (BlockIo == NULL)) {
      break;
    }

    Size = MultU64x32 (BlockIo->Media->LastBlock+1, BlockIo->Media->BlockSize);

    KernelDtb = NULL;
    Status    = gBS->AllocatePool (
                       EfiBootServicesData,
                       Size,
                       &KernelDtb
                       );

    if (EFI_ERROR (Status) || (KernelDtb == NULL)) {
      break;
    }

    Status = BlockIo->ReadBlocks (
                        BlockIo,
                        BlockIo->Media->MediaId,
                        0,
                        Size,
                        KernelDtb
                        );
    if (EFI_ERROR (Status)) {
      break;
    }

    Dtb = KernelDtb;
    if (fdt_check_header (Dtb) != 0) {
      Dtb += PcdGet32 (PcdSignedImageHeaderSize);
      if (fdt_check_header (Dtb) != 0) {
        DEBUG ((DEBUG_ERROR, "%a: DTB on partition was corrupted, attempt use to UEFI DTB\r\n", __FUNCTION__));
        break;
      }
    }

    DtbCopy = NULL;
    DtbCopy = AllocatePages (EFI_SIZE_TO_PAGES (2 * fdt_totalsize (Dtb)));
    if (fdt_open_into (Dtb, DtbCopy, 2 * fdt_totalsize (Dtb)) != 0) {
      goto Exit;
    }
  } while (FALSE);

  DEBUG ((DEBUG_ERROR, "%a: Installing Kernel DTB\r\n", __FUNCTION__));
  Status = gBS->InstallConfigurationTable (&gFdtTableGuid, DtbCopy);
  if (EFI_ERROR (Status)) {
    if (DtbCopy != NULL) {
      gBS->FreePages ((EFI_PHYSICAL_ADDRESS)DtbCopy, EFI_SIZE_TO_PAGES (fdt_totalsize (DtbCopy)));
      DtbCopy = NULL;
    }
  } else {
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)CurrentDtb, EFI_SIZE_TO_PAGES (fdt_totalsize (CurrentDtb)));
  }

Exit:
  if (KernelDtb != NULL) {
    gBS->FreePool (KernelDtb);
    KernelDtb = NULL;
  }

  if (HandleBuffer != NULL) {
    gBS->FreePool (HandleBuffer);
    HandleBuffer = NULL;
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
  OUT   VOID   **Dtb,
  OUT   UINTN  *DtbSize
  )
{
  EFI_STATUS           Status;
  VOID                 *UefiDtb;
  VOID                 *DtbCopy;
  TEGRA_PLATFORM_TYPE  PlatformType;

  UefiDtb = (VOID *)(UINTN)GetDTBBaseAddress ();
  if (fdt_check_header (UefiDtb) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: UEFI DTB corrupted\r\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  // Double the size taken by DTB to have enough buffer to accommodate
  // any runtime additions made to it.
  DtbCopy = NULL;
  DtbCopy = AllocatePages (EFI_SIZE_TO_PAGES (2 * fdt_totalsize (UefiDtb)));
  if (fdt_open_into (UefiDtb, DtbCopy, 2 * fdt_totalsize (UefiDtb)) != 0) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  DEBUG ((DEBUG_ERROR, "%a: Defaulting to UEFI DTB\r\n", __FUNCTION__));
  *Dtb     = DtbCopy;
  *DtbSize = fdt_totalsize (*Dtb);

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  UpdateFdt,
                  NULL,
                  &gFdtTableGuid,
                  &FdtInstallEvent
                  );

  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  PlatformType = TegraGetPlatform ();

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  PlatformType == TEGRA_PLATFORM_SILICON ?
                  InstallFdt :
                  UpdateFdt,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &ReadyToBootEvent
                  );

Exit:
  if (EFI_ERROR (Status)) {
    if (DtbCopy != NULL) {
      gBS->FreePages ((EFI_PHYSICAL_ADDRESS)DtbCopy, EFI_SIZE_TO_PAGES (fdt_totalsize (DtbCopy)));
      DtbCopy = NULL;
    }

    *Dtb     = NULL;
    *DtbSize = 0;
  }

  return Status;
}
