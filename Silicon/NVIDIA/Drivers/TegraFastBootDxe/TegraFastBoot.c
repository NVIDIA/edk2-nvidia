/** @file

  SPDX-FileCopyrightText: Copyright (c) 2022-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/*
  Implementation of the Android Fastboot Platform protocol, to be used by the
  Fastboot UEFI application.
*/

#include <Protocol/AndroidFastbootPlatform.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define GPT_PARTITION_NAME_LENGTH  36

//
// Erase tunables. The Tegra eMMC/SD bdev does not expose a hardware
// CMD_ERASE through EFI_BLOCK_IO_PROTOCOL, so erase is implemented
// as a streaming write of 0xFF.
//
// A 4 MiB page-aligned scratch buffer lets WriteBlocks issue a single
// CMD25 burst per call. The buffer is allocated lazily and kept for
// the lifetime of the driver.
//
// Small partitions (<= FB_FULL_ERASE_THRESHOLD) are wiped fully.
// Larger partitions only have their first FB_LARGE_HEAD_BYTES wiped,
// which is enough to clobber ext4 primary + sparse-super backup
// superblocks (groups 0/1/3/5/7 with 32 MiB groups, plus the inode
// tables) and the f2fs superblock layout, so fs_mgr will reformat on
// next mount.
//
#define FB_ERASE_BUF_SIZE        (SIZE_4MB)
#define FB_LARGE_HEAD_BYTES      ((UINT64)256 * SIZE_1MB)
#define FB_FULL_ERASE_THRESHOLD  ((UINT64)512 * SIZE_1MB)

STATIC UINT8  *mEraseBuffer = NULL;

/**
  Locate a GPT partition by ASCII name. Returns the BlockIo (and
  optionally DiskIo) of the matching partition.
**/
STATIC
EFI_STATUS
FindGptPartition (
  IN  CONST CHAR8            *PartitionName,
  OUT EFI_BLOCK_IO_PROTOCOL  **BlockIoOut,
  OUT EFI_DISK_IO_PROTOCOL   **DiskIoOut OPTIONAL
  )
{
  EFI_STATUS                   Status;
  UINTN                        NumOfHandles;
  EFI_HANDLE                   *HandleBuffer;
  UINTN                        Index;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  CHAR16                       NameUnicode[GPT_PARTITION_NAME_LENGTH];
  EFI_BLOCK_IO_PROTOCOL        *BlockIo;
  EFI_DISK_IO_PROTOCOL         *DiskIo;

  if ((PartitionName == NULL) || (BlockIoOut == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *BlockIoOut = NULL;
  if (DiskIoOut != NULL) {
    *DiskIoOut = NULL;
  }

  AsciiStrToUnicodeStrS (PartitionName, NameUnicode, ARRAY_SIZE (NameUnicode));

  NumOfHandles = 0;
  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gEfiPartitionInfoProtocolGuid,
                        NULL,
                        &NumOfHandles,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status) || (NumOfHandles == 0) || (HandleBuffer == NULL)) {
    return EFI_UNSUPPORTED;
  }

  Status = EFI_NOT_FOUND;
  for (Index = 0; Index < NumOfHandles; Index++) {
    PartitionInfo = NULL;
    if (  EFI_ERROR (gBS->HandleProtocol (HandleBuffer[Index], &gEfiPartitionInfoProtocolGuid, (VOID **)&PartitionInfo))
       || (PartitionInfo == NULL))
    {
      continue;
    }

    if (PartitionInfo->Type != PARTITION_TYPE_GPT) {
      continue;
    }

    if (StrnCmp (
          PartitionInfo->Info.Gpt.PartitionName,
          NameUnicode,
          StrnLenS (NameUnicode, sizeof (PartitionInfo->Info.Gpt.PartitionName))
          ) != 0)
    {
      continue;
    }

    BlockIo = NULL;
    if (  EFI_ERROR (gBS->HandleProtocol (HandleBuffer[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo))
       || (BlockIo == NULL))
    {
      continue;
    }

    *BlockIoOut = BlockIo;

    if (DiskIoOut != NULL) {
      DiskIo = NULL;
      if (!EFI_ERROR (gBS->HandleProtocol (HandleBuffer[Index], &gEfiDiskIoProtocolGuid, (VOID **)&DiskIo))) {
        *DiskIoOut = DiskIo;
      }
    }

    Status = EFI_SUCCESS;
    break;
  }

  if (HandleBuffer != NULL) {
    gBS->FreePool (HandleBuffer);
  }

  return Status;
}

/**
  Get-or-allocate a 4 MiB page-aligned scratch buffer prefilled with
  0xFF, used as the source for partition wiping.
**/
STATIC
EFI_STATUS
GetEraseBuffer (
  OUT UINT8  **BufOut
  )
{
  if (mEraseBuffer == NULL) {
    mEraseBuffer = AllocatePages (EFI_SIZE_TO_PAGES (FB_ERASE_BUF_SIZE));
    if (mEraseBuffer == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    SetMem (mEraseBuffer, FB_ERASE_BUF_SIZE, 0xFF);
  }

  *BufOut = mEraseBuffer;
  return EFI_SUCCESS;
}

/*
  Do any initialisation that needs to be done in order to be able to respond to
  commands.

  @retval EFI_SUCCESS   Initialised successfully.
  @retval !EFI_SUCCESS  Error in initialisation.
*/
STATIC
EFI_STATUS
TegraFastbootPlatformInit (
  VOID
  )
{
  return EFI_SUCCESS;
}

/*
  To be called when Fastboot is finished and we aren't rebooting or booting an
  image. Undo initialisation, free resrouces.
*/
STATIC
VOID
TegraFastbootPlatformUnInit (
  VOID
  )
{
  if (mEraseBuffer != NULL) {
    FreePages (mEraseBuffer, EFI_SIZE_TO_PAGES (FB_ERASE_BUF_SIZE));
    mEraseBuffer = NULL;
  }

  return;
}

/*
  Flash the partition named (according to a platform-specific scheme)
  PartitionName, with the image pointed to by Buffer, whose size is BufferSize.

  @param[in] PartitionName  Null-terminated name of partition to write.
  @param[in] BufferSize     Size of Buffer in byets.
  @param[in] Buffer         Data to write to partition.

  @retval EFI_NOT_FOUND     No such partition.
  @retval EFI_DEVICE_ERROR  Flashing failed.
*/
STATIC
EFI_STATUS
TegraFastbootPlatformFlashPartition (
  IN CHAR8  *PartitionName,
  IN UINTN  Size,
  IN VOID   *Image
  )
{
  EFI_STATUS                   Status;
  UINTN                        NumOfHandles;
  EFI_HANDLE                   *HandleBuffer;
  UINTN                        Index;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  CHAR16                       PartitionNameUnicode[GPT_PARTITION_NAME_LENGTH];
  EFI_BLOCK_IO_PROTOCOL        *BlockIo;
  UINTN                        PartitionSize;
  EFI_DISK_IO_PROTOCOL         *DiskIo;
  UINT32                       MediaId;

  NumOfHandles = 0;
  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gEfiPartitionInfoProtocolGuid,
                        NULL,
                        &NumOfHandles,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status) || (NumOfHandles == 0) || (HandleBuffer == NULL)) {
    return EFI_UNSUPPORTED;
  }

  for (Index = 0; Index < NumOfHandles; Index++) {
    PartitionInfo = NULL;
    Status        = gBS->HandleProtocol (
                           HandleBuffer[Index],
                           &gEfiPartitionInfoProtocolGuid,
                           (VOID **)&PartitionInfo
                           );

    if (EFI_ERROR (Status) || (PartitionInfo == NULL)) {
      Status = EFI_NOT_FOUND;
      goto NoFlashExit;
    }

    if (PartitionInfo->Info.Gpt.StartingLBA > PartitionInfo->Info.Gpt.EndingLBA) {
      Status = EFI_PROTOCOL_ERROR;
      goto NoFlashExit;
    }

    if (PartitionInfo->Type != PARTITION_TYPE_GPT) {
      continue;
    }

    AsciiStrToUnicodeStrS (PartitionName, PartitionNameUnicode, ARRAY_SIZE (PartitionNameUnicode));

    if (0 == StrnCmp (
               PartitionInfo->Info.Gpt.PartitionName,
               PartitionNameUnicode,
               StrnLenS (PartitionNameUnicode, sizeof (PartitionInfo->Info.Gpt.PartitionName))
               ))
    {
      break;
    }
  }

  if (Index >= NumOfHandles) {
    Status = EFI_NOT_FOUND;
    goto NoFlashExit;
  }

  BlockIo = NULL;
  Status  = gBS->HandleProtocol (
                   HandleBuffer[Index],
                   &gEfiBlockIoProtocolGuid,
                   (VOID **)&BlockIo
                   );
  if (EFI_ERROR (Status) || (BlockIo == NULL)) {
    Status = EFI_NOT_FOUND;
    goto NoFlashExit;
  }

  PartitionSize = (BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  if (PartitionSize < Size) {
    Status =  EFI_VOLUME_FULL;
    goto NoFlashExit;
  }

  DiskIo = NULL;
  Status = gBS->HandleProtocol (
                  HandleBuffer[Index],
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo
                  );
  if (EFI_ERROR (Status) || (DiskIo == NULL)) {
    Status = EFI_NOT_FOUND;
    goto NoFlashExit;
  }

  MediaId = BlockIo->Media->MediaId;

  Status = DiskIo->WriteDisk (DiskIo, MediaId, 0, Size, Image);
  if (EFI_ERROR (Status)) {
    goto NoFlashExit;
  }

  BlockIo->FlushBlocks (BlockIo);

NoFlashExit:
  if (HandleBuffer != NULL) {
    gBS->FreePool (HandleBuffer);
  }

  return Status;
}

/*
  Erase the partition named PartitionName by streaming 0xFF via
  BlockIo->WriteBlocks (the Tegra eMMC/SD bdev does not expose
  EFI_ERASE_BLOCK_PROTOCOL).

  Partitions <= 512 MiB are wiped fully; larger partitions only have
  their first 256 MiB wiped, which is enough to clobber ext4 / f2fs
  primary + backup superblocks and the inode tables so fs_mgr will
  reformat on next mount.

  @param[in] PartitionName  Null-terminated name of partition to erase.

  @retval EFI_NOT_FOUND     No such partition.
  @retval EFI_DEVICE_ERROR  Erasing failed.
*/
STATIC
EFI_STATUS
TegraFastbootPlatformErasePartition (
  IN CHAR8  *PartitionName
  )
{
  EFI_STATUS             Status;
  EFI_BLOCK_IO_PROTOCOL  *BlockIo;
  UINT8                  *Buf;
  UINT32                 MediaId;
  UINT32                 BlockSize;
  UINT64                 PartitionBytes;
  UINT64                 ToEraseBytes;
  UINT64                 TotalBlocks;
  UINT64                 BlocksLeft;
  UINT64                 BlocksDone;
  UINT64                 ChunkBlocks;
  UINT64                 MaxChunkBlocks;

  if (PartitionName == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = FindGptPartition (PartitionName, &BlockIo, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: not found\n", __FUNCTION__, PartitionName));
    return Status;
  }

  if ((BlockIo->Media == NULL) || (BlockIo->Media->BlockSize == 0)) {
    return EFI_DEVICE_ERROR;
  }

  Status = GetEraseBuffer (&Buf);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MediaId        = BlockIo->Media->MediaId;
  BlockSize      = BlockIo->Media->BlockSize;
  PartitionBytes = (BlockIo->Media->LastBlock + 1) * BlockSize;

  ToEraseBytes = (PartitionBytes <= FB_FULL_ERASE_THRESHOLD)
                 ? PartitionBytes
                 : FB_LARGE_HEAD_BYTES;

  TotalBlocks    = (ToEraseBytes + BlockSize - 1) / BlockSize;
  MaxChunkBlocks = FB_ERASE_BUF_SIZE / BlockSize;

  DEBUG ((
    DEBUG_ERROR,
    "%a: %a: erasing %llu MiB of %llu MiB partition (BlockSize=%u)\n",
    __FUNCTION__,
    PartitionName,
    (UINT64)(TotalBlocks * BlockSize) / SIZE_1MB,
    PartitionBytes / SIZE_1MB,
    BlockSize
    ));

  BlocksLeft = TotalBlocks;
  BlocksDone = 0;
  while (BlocksLeft > 0) {
    ChunkBlocks = (BlocksLeft > MaxChunkBlocks) ? MaxChunkBlocks : BlocksLeft;

    Status = BlockIo->WriteBlocks (
                        BlockIo,
                        MediaId,
                        BlocksDone,
                        (UINTN)(ChunkBlocks * BlockSize),
                        Buf
                        );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: %a: WriteBlocks LBA=%llu count=%llu: %r\n",
        __FUNCTION__,
        PartitionName,
        BlocksDone,
        ChunkBlocks,
        Status
        ));
      return EFI_DEVICE_ERROR;
    }

    BlocksDone += ChunkBlocks;
    BlocksLeft -= ChunkBlocks;
  }

  BlockIo->FlushBlocks (BlockIo);
  return EFI_SUCCESS;
}

/*
  If the variable referred to by Name exists, copy it (as a null-terminated
  string) into Value. If it doesn't exist, put the Empty string in Value.

  Variable names and values may not be larger than 60 bytes, excluding the
  terminal null character. This is a limitation of the Fastboot protocol.

  The Fastboot application will handle platform-nonspecific variables
  (Currently "version" is the only one of these.)

  @param[in]  Name   Null-terminated name of Fastboot variable to retrieve.
  @param[out] Value  Caller-allocated buffer for null-terminated value of
                     variable.

  @retval EFI_SUCCESS       The variable was retrieved, or it doesn't exist.
  @retval EFI_DEVICE_ERROR  There was an error looking up the variable. This
                            does _not_ include the variable not existing.
*/
STATIC
EFI_STATUS
TegraFastbootPlatformGetVar (
  IN  CHAR8  *Name,
  OUT CHAR8  *Value
  )
{
  *Value = '\0';

  return EFI_SUCCESS;
}

/*
  React to an OEM-specific command.

  Future versions of this function might want to allow the platform to do some
  extra communication with the host. A way to do this would be to add a function
  to the FASTBOOT_TRANSPORT_PROTOCOL that allows the implementation of
  DoOemCommand to replace the ReceiveEvent with its own, and to restore the old
  one when it's finished.

  However at the moment although the specification allows it, the AOSP fastboot
  host application doesn't handle receiving any data from the client, and it
  doesn't support a data phase for OEM commands.

  @param[in] Command    Null-terminated command string.

  @retval EFI_SUCCESS       The command executed successfully.
  @retval EFI_NOT_FOUND     The command wasn't recognised.
  @retval EFI_DEVICE_ERROR  There was an error executing the command.
*/
STATIC
EFI_STATUS
TegraFastbootPlatformOemCommand (
  IN  CHAR8  *Command
  )
{
  return EFI_NOT_FOUND;
}

STATIC FASTBOOT_PLATFORM_PROTOCOL  mPlatformProtocol = {
  TegraFastbootPlatformInit,
  TegraFastbootPlatformUnInit,
  TegraFastbootPlatformFlashPartition,
  TegraFastbootPlatformErasePartition,
  TegraFastbootPlatformGetVar,
  TegraFastbootPlatformOemCommand
};

EFI_STATUS
EFIAPI
TegraAndroidFastbootPlatformEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return gBS->InstallProtocolInterface (
                &ImageHandle,
                &gAndroidFastbootPlatformProtocolGuid,
                EFI_NATIVE_INTERFACE,
                &mPlatformProtocol
                );
}
