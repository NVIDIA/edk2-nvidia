/**
  Implementation for AndroidBcbLib library class interfaces.

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/AndroidBcbLib.h>

#define COMPARE_MSG_COMMAND(Msg, Target)  (AsciiStrCmp (Msg.command, Target))

#define MSG_COMMAND_BOOT_RECOVERY             "boot-recovery"
#define MSG_COMMAND_BOOT_FASTBOOT_USERSPACE   "boot-fastboot"
#define MSG_COMMAND_BOOT_FASTBOOT_BOOTLOADER  "bootonce-bootloader"

static MiscCmdType  CacheCmdType = MISC_CMD_TYPE_MAX;

EFI_STATUS
EFIAPI
GetCmdFromMiscPartition (
  EFI_HANDLE   Handle,
  MiscCmdType  *Type,
  BOOLEAN      CleanBootOnceCmd
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
  BootloaderMessage            Message;

  if (Type == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  if (CacheCmdType != MISC_CMD_TYPE_MAX) {
    *Type = CacheCmdType;
    goto Exit;
  }

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

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     0,
                     sizeof (BootloaderMessage),
                     (VOID *)&Message
                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to read disk\r\n"));
    goto Exit;
  }

  if (COMPARE_MSG_COMMAND (Message, MSG_COMMAND_BOOT_RECOVERY) == 0) {
    *Type = MISC_CMD_TYPE_RECOVERY;
  } else if (COMPARE_MSG_COMMAND (Message, MSG_COMMAND_BOOT_FASTBOOT_USERSPACE) == 0) {
    *Type = MISC_CMD_TYPE_FASTBOOT_USERSPACE;
  } else if (COMPARE_MSG_COMMAND (Message, MSG_COMMAND_BOOT_FASTBOOT_BOOTLOADER) == 0) {
    *Type = MISC_CMD_TYPE_FASTBOOT_BOOTLOADER;
    // bootonce-bootloader, clean the field to avoid boot into fastboot again
    if (CleanBootOnceCmd == TRUE) {
      SetMem (Message.command, BOOTLOADER_MESSAGE_COMMAND_BYTES, 0x00);
      Status = DiskIo->WriteDisk (
                         DiskIo,
                         BlockIo->Media->MediaId,
                         0,
                         sizeof (BootloaderMessage),
                         (VOID *)&Message
                         );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r tring to clean BCB %a\r\n", __FUNCTION__, Status, MSG_COMMAND_BOOT_FASTBOOT_BOOTLOADER));
        goto Exit;
      }
    }
  } else {
    *Type = MISC_CMD_TYPE_INVALID;
  }

  CacheCmdType = *Type;

Exit:
  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  return Status;
}
