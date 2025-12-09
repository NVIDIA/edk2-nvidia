/**
  Implementation for AndroidBcbLib library class interfaces.

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/AndroidBcbLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformResourceLib.h>

#define COMPARE_MSG_COMMAND(Msg, Target)  (AsciiStrCmp (Msg.command, Target))
#define NV_OFFSETOF(type, member)         ((UINT32)(UINT64)&(((type *)0)->member))

#define MSG_COMMAND_BOOT_RECOVERY             "boot-recovery"
#define MSG_COMMAND_BOOT_QUIESCENT            "boot-quiescent"
#define MSG_COMMAND_BOOT_FASTBOOT_USERSPACE   "boot-fastboot"
#define MSG_COMMAND_BOOT_FASTBOOT_BOOTLOADER  "bootonce-bootloader"

CONST UINT32  kDefaultPriority     = 15;
CONST UINT32  kDefaultBootAttempts = 3;

static MiscCmdType  CacheCmdType = MISC_CMD_TYPE_MAX;

/**
  Dump the BootCtrl Metadata for debug purpose.

  @param[in]    BootCtrl    Start address of BootCtrl
**/
STATIC
VOID
BcbDumpSlotInfo (
  IN BootloaderControl  *BootCtrl
  )
{
  UINT32  Idx;

  DEBUG ((
    DEBUG_ERROR,
    "%a: magic:0x%x, version: %d, num_slots: %d\r\n",
    __FUNCTION__,
    BootCtrl->Magic,
    BootCtrl->Version,
    BootCtrl->NbSlot
    ));
  for (Idx = 0; Idx < BootCtrl->NbSlot; Idx++) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: slot: %u, pri: %u, retry: %u, boot_successful: %u\r\n",
      __FUNCTION__,
      Idx,
      BootCtrl->SlotInfo[Idx].Priority,
      BootCtrl->SlotInfo[Idx].TriesRemaining,
      BootCtrl->SlotInfo[Idx].SuccessfulBoot
      ));
  }
}

/**
  Get the BlockIo & DiskIo for accessing Misc.

  @param[in]  Handle      Image Handle to access block device.
  @param[out] MscBlockIo  BlockIo to access Misc.
  @param[out] MscDiskIo   DiskIo to access Misc.

  @retval EFI_SUCCESS      Operation successful.
  @retval others           Error occurred.
**/
STATIC
EFI_STATUS
GetMiscIoProtocolFromHandle (
  IN  EFI_HANDLE             Handle,
  OUT EFI_BLOCK_IO_PROTOCOL  **MscBlockIo,
  OUT EFI_DISK_IO_PROTOCOL   **MscDiskIo
  )
{
  EFI_STATUS                   Status = EFI_SUCCESS;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  EFI_HANDLE                   PartitionHandle;
  UINTN                        Index;
  UINTN                        NumOfHandles;
  EFI_HANDLE                   *HandleBuffer = NULL;
  EFI_BLOCK_IO_PROTOCOL        *BlockIo;
  EFI_DISK_IO_PROTOCOL         *DiskIo;

  if (Handle == NULL) {
    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEfiPartitionInfoProtocolGuid,
                    NULL,
                    &NumOfHandles,
                    &HandleBuffer
                    );

    if (EFI_ERROR (Status)) {
      Status = EFI_UNSUPPORTED;
      goto Exit;
    }

    for (Index = 0; Index < NumOfHandles; Index++) {
      // Get partition info protcol from handle and validate
      Status = gBS->HandleProtocol (
                      HandleBuffer[Index],
                      &gEfiPartitionInfoProtocolGuid,
                      (VOID **)&PartitionInfo
                      );

      if (EFI_ERROR (Status) || (PartitionInfo == NULL)) {
        Status = EFI_NOT_FOUND;
        goto Exit;
      }

      // Found MISC_PARTITION
      if (0 == StrCmp (
                 PartitionInfo->Info.Gpt.PartitionName,
                 MISC_PARTITION_BASE_NAME
                 )
          )
      {
        break;
      }
    }

    if (Index >= NumOfHandles) {
      Status = EFI_NOT_FOUND;
      DEBUG ((DEBUG_INFO, "%a: Unable to locate MSC partition\r\n", __FUNCTION__));
      goto Exit;
    }

    PartitionHandle = HandleBuffer[Index];
  } else {
    PartitionHandle = Handle;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to locate block io protocol on partition\r\n", __FUNCTION__));
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to locate disk io protocol on partition\r\n", __FUNCTION__));
    goto Exit;
  }

  *MscBlockIo = BlockIo;
  *MscDiskIo  = DiskIo;

Exit:
  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
ClearCmdFromMiscPartition (
  EFI_BLOCK_IO_PROTOCOL  *MscBlockIo,
  EFI_DISK_IO_PROTOCOL   *MscDiskIo
  )
{
  EFI_STATUS         Status = EFI_SUCCESS;
  BootloaderMessage  Message;

  SetMem (Message.command, BOOTLOADER_MESSAGE_COMMAND_BYTES, 0x00);
  Status = MscDiskIo->WriteDisk (
                        MscDiskIo,
                        MscBlockIo->Media->MediaId,
                        0,
                        sizeof (BootloaderMessage),
                        (VOID *)&Message
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r tring to clean BCB %a\r\n", __FUNCTION__, Status, MSG_COMMAND_BOOT_FASTBOOT_BOOTLOADER));
  }

  return Status;
}

EFI_STATUS
EFIAPI
GetCmdFromMiscPartition (
  EFI_HANDLE   Handle,
  MiscCmdType  *Type,
  BOOLEAN      CleanBootOnceCmd
  )
{
  EFI_STATUS             Status = EFI_SUCCESS;
  EFI_BLOCK_IO_PROTOCOL  *MscBlockIo;
  EFI_DISK_IO_PROTOCOL   *MscDiskIo;
  BootloaderMessage      Message;

  if (Type == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  if (CacheCmdType != MISC_CMD_TYPE_MAX) {
    *Type = CacheCmdType;
    goto Exit;
  }

  Status = GetMiscIoProtocolFromHandle (Handle, &MscBlockIo, &MscDiskIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to fetch IO protocols\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Status = MscDiskIo->ReadDisk (
                        MscDiskIo,
                        MscBlockIo->Media->MediaId,
                        0,
                        sizeof (BootloaderMessage),
                        (VOID *)&Message
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to read disk\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  if (COMPARE_MSG_COMMAND (Message, MSG_COMMAND_BOOT_RECOVERY) == 0) {
    *Type = MISC_CMD_TYPE_RECOVERY;
  } else if (COMPARE_MSG_COMMAND (Message, MSG_COMMAND_BOOT_FASTBOOT_USERSPACE) == 0) {
    *Type = MISC_CMD_TYPE_FASTBOOT_USERSPACE;
  } else if (COMPARE_MSG_COMMAND (Message, MSG_COMMAND_BOOT_QUIESCENT) == 0) {
    *Type = MISC_CMD_TYPE_BOOT_QUIESCENT;
    // boot-quiescent, clean the field to avoid boot into quiescent mode again
    if (CleanBootOnceCmd == TRUE) {
      Status = ClearCmdFromMiscPartition (MscBlockIo, MscDiskIo);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }
    }
  } else if (COMPARE_MSG_COMMAND (Message, MSG_COMMAND_BOOT_FASTBOOT_BOOTLOADER) == 0) {
    *Type = MISC_CMD_TYPE_FASTBOOT_BOOTLOADER;
    // bootonce-bootloader, clean the field to avoid boot into fastboot again
    if (CleanBootOnceCmd == TRUE) {
      Status = ClearCmdFromMiscPartition (MscBlockIo, MscDiskIo);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }
    }
  } else {
    *Type = MISC_CMD_TYPE_INVALID;
  }

  CacheCmdType = *Type;

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
SetCmdToMiscPartition (
  EFI_HANDLE   Handle,
  MiscCmdType  Type
  )
{
  EFI_STATUS             Status = EFI_SUCCESS;
  EFI_BLOCK_IO_PROTOCOL  *MscBlockIo;
  EFI_DISK_IO_PROTOCOL   *MscDiskIo;
  BootloaderMessage      Message = { 0 };

  Status = GetMiscIoProtocolFromHandle (Handle, &MscBlockIo, &MscDiskIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to fetch IO protocols\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Status = MscDiskIo->ReadDisk (
                        MscDiskIo,
                        MscBlockIo->Media->MediaId,
                        0,
                        sizeof (BootloaderMessage),
                        (VOID *)&Message
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to read BCB\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  // bootloader only support boot-fastboot & boot-recovery
  SetMem (Message.command, BOOTLOADER_MESSAGE_COMMAND_BYTES, 0x00);
  if (Type == MISC_CMD_TYPE_RECOVERY) {
    AsciiStrCpyS (Message.command, BOOTLOADER_MESSAGE_COMMAND_BYTES, MSG_COMMAND_BOOT_RECOVERY);
  } else if (Type == MISC_CMD_TYPE_FASTBOOT_USERSPACE) {
    AsciiStrCpyS (Message.command, BOOTLOADER_MESSAGE_COMMAND_BYTES, MSG_COMMAND_BOOT_FASTBOOT_USERSPACE);
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Invalid cmd type \r\n", __FUNCTION__, Type));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  Status = MscDiskIo->WriteDisk (
                        MscDiskIo,
                        MscBlockIo->Media->MediaId,
                        0,
                        sizeof (BootloaderMessage),
                        (VOID *)&Message
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to write BCB\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  CacheCmdType = Type;

Exit:
  return Status;
}

/**
  Calculate CRC32 sum of BootCtrl struct

  @param[in]    BootCtrl    Start address of BootCtrl

  @retval                   CRC32 result
**/
STATIC
UINT32
BootloaderControlLeCrc (
  BootloaderControl  *BootCtrl
  )
{
  return CalculateCrc32 ((UINT8 *)BootCtrl, NV_OFFSETOF (BootloaderControl, Crc32Le));
}

/**
  Get the Active boot slot from Bcb

  @param[in]    BootCtrl    Start address of BootCtrl

  @retval                   Bcb active boot slot id
**/
STATIC
UINT32
BcbGetActiveBootSlot (
  BootloaderControl  *BootCtrl
  )
{
  UINT32  ActiveBootSlot = 0;
  UINT32  Idx;
  UINT32  MaxPriority = BootCtrl->SlotInfo[0].Priority;

  // Find the slot with the highest priority.
  for (Idx = 0; Idx < BootCtrl->NbSlot; ++Idx) {
    if (BootCtrl->SlotInfo[Idx].Priority > MaxPriority) {
      MaxPriority    = BootCtrl->SlotInfo[Idx].Priority;
      ActiveBootSlot = Idx;
    }
  }

  return ActiveBootSlot;
}

UINT32
EFIAPI
BcbGetActiveFwBootChain (
  VOID
  )
{
  VOID  *Hob;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  NV_ASSERT_RETURN (
    ((Hob != NULL) && (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO))),
    return BOOT_CHAIN_A,
    "%a: Error getting boot chain\n",
    __FUNCTION__
    );

  return ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ActiveBootChain;
}

EFI_STATUS
EFIAPI
AndroidBcbLockChain (
  EFI_HANDLE  Handle
  )
{
  EFI_STATUS             Status = EFI_SUCCESS;
  EFI_BLOCK_IO_PROTOCOL  *MscBlockIo;
  EFI_DISK_IO_PROTOCOL   *MscDiskIo;
  BootloaderControl      BootCtrl;
  UINT32                 CurrentSlotIndex;
  UINT32                 MscActiveSlotIndex;
  UINT32                 BootCtrlOffset;
  UINT32                 ComputedCrc;

  Status = GetMiscIoProtocolFromHandle (Handle, &MscBlockIo, &MscDiskIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to fetch IO protocols\r\n", __FUNCTION__, Status));
    return Status;
  }

  BootCtrlOffset = NV_OFFSETOF (BootloaderMessageAb, BootCtrl);

  Status = MscDiskIo->ReadDisk (
                        MscDiskIo,
                        MscBlockIo->Media->MediaId,
                        BootCtrlOffset,
                        sizeof (BootloaderControl),
                        (VOID *)&BootCtrl
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to read bootcontrol from Misc\r\n", __FUNCTION__, Status));
    return Status;
  }

  ComputedCrc = BootloaderControlLeCrc (&BootCtrl);
  if (ComputedCrc != BootCtrl.Crc32Le) {
    // Skip as this is the first boot after factory flash
    // Just boot current chain
    DEBUG ((DEBUG_ERROR, "%a: BootCtrl Crc mismatch, considering first boot and boot current chain\r\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  MscActiveSlotIndex = BcbGetActiveBootSlot (&BootCtrl);
  CurrentSlotIndex   = BcbGetActiveFwBootChain ();

  // Lock BCB active chain as Current boot chain if not same
  // 1. if SlotInfo[ActiveSlot].TriesRemaining != 0
  //    device failed boot and revert to old chain from non-android components like DU, other GOS
  // 2. if SlotInfo[ActiveSlot].TriesRemaining == 0
  //    android run out of TriesRemaining, chain got reverted due to DU wdt timeout.
  if (CurrentSlotIndex != MscActiveSlotIndex) {
    DEBUG ((DEBUG_ERROR, "%a: BootCtrl chain %u not match current boot chain %u, force BootCtrl chain to %u\r\n", __FUNCTION__, MscActiveSlotIndex, CurrentSlotIndex, CurrentSlotIndex));
    BootCtrl.SlotInfo[CurrentSlotIndex].Priority        = kDefaultPriority;
    BootCtrl.SlotInfo[MscActiveSlotIndex].Priority      = kDefaultPriority - 1;
    BootCtrl.SlotInfo[CurrentSlotIndex].TriesRemaining  = kDefaultBootAttempts;
    BootCtrl.SlotInfo[CurrentSlotIndex].VerityCorrupted = 0;
    BootCtrl.Crc32Le                                    = BootloaderControlLeCrc (&BootCtrl);
    Status                                              = MscDiskIo->WriteDisk (
                                                                       MscDiskIo,
                                                                       MscBlockIo->Media->MediaId,
                                                                       BootCtrlOffset,
                                                                       sizeof (BootloaderControl),
                                                                       (VOID *)&BootCtrl
                                                                       );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to flush bootcontrol to Misc\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_ERROR, "%a: Dump Bcb BootCtrl:\r\n", __FUNCTION__));
  BcbDumpSlotInfo (&BootCtrl);
  DEBUG_CODE_END ();

  return Status;
}

EFI_STATUS
EFIAPI
AndroidBcbCheckAndUpdateRetryCount (
  EFI_HANDLE  Handle
  )
{
  EFI_STATUS             Status = EFI_SUCCESS;
  EFI_BLOCK_IO_PROTOCOL  *MscBlockIo;
  EFI_DISK_IO_PROTOCOL   *MscDiskIo;
  BootloaderControl      BootCtrl;
  UINT32                 MscActiveSlotIndex;
  UINT32                 BootCtrlOffset;
  UINT32                 ComputedCrc;

  Status = GetMiscIoProtocolFromHandle (Handle, &MscBlockIo, &MscDiskIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to fetch IO protocols\r\n", __FUNCTION__, Status));
    return Status;
  }

  BootCtrlOffset = NV_OFFSETOF (BootloaderMessageAb, BootCtrl);

  Status = MscDiskIo->ReadDisk (
                        MscDiskIo,
                        MscBlockIo->Media->MediaId,
                        BootCtrlOffset,
                        sizeof (BootloaderControl),
                        (VOID *)&BootCtrl
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to read bootcontrol from Misc\r\n", __FUNCTION__, Status));
    return Status;
  }

  ComputedCrc = BootloaderControlLeCrc (&BootCtrl);
  if (ComputedCrc != BootCtrl.Crc32Le) {
    // Skip as this is the first boot after factory flash
    // Just boot current chain
    DEBUG ((DEBUG_ERROR, "%a: BootCtrl Crc mismatch, considering first boot and skip update retry count\r\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  MscActiveSlotIndex = BcbGetActiveBootSlot (&BootCtrl);

  // On vUEFI:
  // if no more TriesRemaining left, vUEFI has no ability to change DOS chain, hence just fail boot and wait for DU timeout
  // On native UEFI
  // TODO: if no more TriesRemaining left, issue a bct chain switch
  if (BootCtrl.SlotInfo[MscActiveSlotIndex].TriesRemaining == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Run out of TriesRemaining, always fail boot and wait for DOS to revert chain\r\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  // TriesRemaining-- if current boot chain is not boot_successful yet
  if ((BootCtrl.SlotInfo[MscActiveSlotIndex].SuccessfulBoot == 0) &&
      (BootCtrl.SlotInfo[MscActiveSlotIndex].TriesRemaining != 0))
  {
    BootCtrl.SlotInfo[MscActiveSlotIndex].TriesRemaining--;
    BootCtrl.Crc32Le = BootloaderControlLeCrc (&BootCtrl);
    Status           = MscDiskIo->WriteDisk (
                                    MscDiskIo,
                                    MscBlockIo->Media->MediaId,
                                    BootCtrlOffset,
                                    sizeof (BootloaderControl),
                                    (VOID *)&BootCtrl
                                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to flush bootcontrol to Misc\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_ERROR, "%a: Dump Bcb BootCtrl:\r\n", __FUNCTION__));
  BcbDumpSlotInfo (&BootCtrl);
  DEBUG_CODE_END ();

  return Status;
}
