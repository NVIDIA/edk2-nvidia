/** @file

  Copyright (c) 2020-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiDxe.h>

#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UsbFirmwareLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/NorFlash.h>
#include <Uefi/UefiGpt.h>
#include <Library/DebugLib.h>
#include <Library/GptLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DevicePathLib.h>

#include <Protocol/UsbFwProtocol.h>

NVIDIA_USBFW_PROTOCOL mUsbFwData;

#define GPT_PARTITION_BLOCK_SIZE 512

/**
  Read the data from the disk

  @param[in]  Handle   Handle with storage protocol
  @param[in]  Offset   Offset in the disk
  @param[out] Buffer
  @param[out] Size

**/
STATIC
EFI_STATUS
EFIAPI
ReadStorageData(
  IN  EFI_HANDLE  Handle,
  IN  UINT64      Offset,
  OUT VOID        *Buffer,
  IN  UINT64      Size
  )
{
  EFI_STATUS                  Status;
  NVIDIA_NOR_FLASH_PROTOCOL   *NorFlashProtocol;
  EFI_BLOCK_IO_PROTOCOL       *BlockIoProtocol;
  VOID                        *TempBuffer;
  UINT32                      BlockSize;
  UINT64                      CopySize;

  Status = gBS->HandleProtocol (Handle, &gNVIDIANorFlashProtocolGuid, (VOID **)&NorFlashProtocol);
  if (!EFI_ERROR (Status)) {
    Status = NorFlashProtocol->Read (NorFlashProtocol,
                                     Offset,
                                     Size,
                                     Buffer);
    return Status;
  } else {
    Status = gBS->HandleProtocol (Handle, &gEfiBlockIoProtocolGuid, (VOID **)&BlockIoProtocol);
    if (!EFI_ERROR (Status)) {
      BlockSize = BlockIoProtocol->Media->BlockSize;
      if ((Offset % BlockSize) != 0) {
        CopySize = MIN (Size, BlockSize - (Offset % BlockSize));
        TempBuffer = AllocatePool (BlockSize);
        if (TempBuffer == NULL) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to allocate temp buffer\r\n", __FUNCTION__));
          return EFI_OUT_OF_RESOURCES;
        }
        Status = BlockIoProtocol->ReadBlocks (BlockIoProtocol,
                                              BlockIoProtocol->Media->MediaId,
                                              Offset / BlockSize,
                                              BlockSize,
                                              TempBuffer);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to read data\r\n", __FUNCTION__));
          FreePool (TempBuffer);
          return Status;
        }
        CopyMem (Buffer, TempBuffer + (Offset % BlockSize) , CopySize);
        FreePool (TempBuffer);
        Buffer += CopySize;
        Offset += CopySize;
        Size -= CopySize;
      }

      CopySize = Size - (Size % BlockSize);
      if (CopySize != 0) {
        Status = BlockIoProtocol->ReadBlocks (BlockIoProtocol,
                                              BlockIoProtocol->Media->MediaId,
                                              Offset / BlockSize,
                                              CopySize,
                                              Buffer);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to read data\r\n", __FUNCTION__));
          FreePool (TempBuffer);
          return Status;
        }
        Buffer += CopySize;
        Offset += CopySize;
        Size -= CopySize;
      }

      if (Size != 0) {
        CopySize = Size;
        TempBuffer = AllocatePool (BlockSize);
        if (TempBuffer == NULL) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to allocate temp buffer\r\n", __FUNCTION__));
          return EFI_OUT_OF_RESOURCES;
        }
        Status = BlockIoProtocol->ReadBlocks (BlockIoProtocol,
                                              BlockIoProtocol->Media->MediaId,
                                              Offset / BlockSize,
                                              BlockSize,
                                              TempBuffer);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to read data\r\n", __FUNCTION__));
          FreePool (TempBuffer);
          return Status;
        }
        CopyMem (Buffer, TempBuffer, CopySize);
        FreePool (TempBuffer);
        Buffer += CopySize;
        Offset += CopySize;
        Size -= CopySize;
      }

      return EFI_SUCCESS;
    }
  }

  DEBUG ((DEBUG_ERROR, "%a: Unable to read data\r\n", __FUNCTION__));
  ASSERT (FALSE);
  return EFI_DEVICE_ERROR;
}

/**
  Read the last 512 bytes from the disk

  @param[in]  Handle   Handle with storage protocol
  @param[out] Partition

**/
STATIC
EFI_STATUS
EFIAPI
ReadBackupGpt(
  IN  EFI_HANDLE                   Handle,
  OUT EFI_PARTITION_TABLE_HEADER  *PartitionHeader
  )
{
  EFI_STATUS                  Status;
  UINT64                      StorageSize;
  NVIDIA_NOR_FLASH_PROTOCOL   *NorFlashProtocol;
  NOR_FLASH_ATTRIBUTES        NorFlashAttributes;
  EFI_BLOCK_IO_PROTOCOL       *BlockIoProtocol;

  StorageSize = 0;
  Status = gBS->HandleProtocol (Handle, &gNVIDIANorFlashProtocolGuid, (VOID **)&NorFlashProtocol);
  if (!EFI_ERROR (Status)) {
    Status = NorFlashProtocol->GetAttributes (NorFlashProtocol, &NorFlashAttributes);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get NOR Flash attributes (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }
    StorageSize = NorFlashAttributes.MemoryDensity;
  } else {
    Status = gBS->HandleProtocol (Handle, &gEfiBlockIoProtocolGuid, (VOID **)&BlockIoProtocol);
    if (!EFI_ERROR (Status)) {
      StorageSize = BlockIoProtocol->Media->BlockSize * (BlockIoProtocol->Media->LastBlock + 1);
    }
  }

  if (StorageSize == 0) {
    DEBUG ((DEBUG_ERROR, "%a: No storage detected\r\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }
  return ReadStorageData (Handle, StorageSize - GPT_PARTITION_BLOCK_SIZE, PartitionHeader, sizeof (EFI_PARTITION_TABLE_HEADER));
}

/**
  Check partition media to be valid

  @param[in]  Handle   Handle with partition protocol

**/
STATIC
EFI_STATUS
EFIAPI
CheckPartitionFlash(
  IN EFI_HANDLE         Handle
  )
{
  EFI_STATUS               Status;
  EFI_DEVICE_PATH_PROTOCOL *PartitionDevicePath;
  EFI_DEVICE_PATH_PROTOCOL *CurrentDevicePath;
  BOOLEAN                  ValidFlash;

  PartitionDevicePath = NULL;
  CurrentDevicePath = NULL;

  // Query for Device Path on the handle
  Status = gBS->HandleProtocol (Handle,
                                &gEfiDevicePathProtocolGuid,
                                (VOID **)&PartitionDevicePath);

  if (EFI_ERROR(Status) || (PartitionDevicePath == NULL) || IsDevicePathEnd(PartitionDevicePath)) {
    Status = EFI_UNSUPPORTED;
    goto Exit;
  }

  // Check if any node on device path is of EMMC type
  ValidFlash = FALSE;
  CurrentDevicePath = PartitionDevicePath;
  while (IsDevicePathEnd (CurrentDevicePath) == FALSE) {
    if ((CurrentDevicePath->Type == MESSAGING_DEVICE_PATH) &&
        (CurrentDevicePath->SubType == MSG_EMMC_DP)) {
      CurrentDevicePath = NextDevicePathNode (CurrentDevicePath);
      if ((CurrentDevicePath->Type == HARDWARE_DEVICE_PATH) &&
          (CurrentDevicePath->SubType == HW_CONTROLLER_DP)) {
        CONTROLLER_DEVICE_PATH  *ControlNode = (CONTROLLER_DEVICE_PATH *)CurrentDevicePath;
        if (ControlNode->ControllerNumber == 0) {
          CurrentDevicePath = NextDevicePathNode (CurrentDevicePath);
          if (IsDevicePathEnd (CurrentDevicePath)) {
            ValidFlash = TRUE;
          }
        }
      }
      break;
    }
    CurrentDevicePath = NextDevicePathNode (CurrentDevicePath);
  }

  if (ValidFlash != TRUE) {
    Status = EFI_UNSUPPORTED;
    goto Exit;
  }

Exit:
  return Status;
}

/**
  Entrypoint of USB Firmware Dxe.

  @param  ImageHandle
  @param  SystemTable

  @return EFI_SUCCESS
  @return EFI_LOAD_ERROR
  @return EFI_OUT_OF_RESOURCES

**/
EFI_STATUS
EFIAPI
UsbFirmwareDxeInitialize (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE  * SystemTable
  )
{
  UINTN                       ChipID;
  EFI_STATUS                  Status;
  UINTN                       NumOfHandles;
  EFI_HANDLE                  *HandleBuffer = NULL;
  EFI_HANDLE                  StorageHandle;
  BOOLEAN                     StorageFound;
  UINTN                       Index;
  EFI_PARTITION_TABLE_HEADER  PartitionHeader;
  VOID                        *PartitionEntryArray;
  CONST EFI_PARTITION_ENTRY   *PartitionEntry;
  CHAR8                       *UsbFwBuffer;

  ChipID = TegraGetChipID();
  if (ChipID != T194_CHIP_ID) {
    return EFI_SUCCESS;
  }

  StorageFound = FALSE;
  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gNVIDIANorFlashProtocolGuid,
                                    NULL,
                                    &NumOfHandles,
                                    &HandleBuffer);
  if (!EFI_ERROR (Status)) {
    StorageHandle = HandleBuffer[0];
    StorageFound = TRUE;
    FreePool (HandleBuffer);
  } else {
    Status = gBS->LocateHandleBuffer (ByProtocol,
                                      &gEfiBlockIoProtocolGuid,
                                      NULL,
                                      &NumOfHandles,
                                      &HandleBuffer);
    if (!EFI_ERROR (Status)) {
      for (Index = 0; Index < NumOfHandles; Index++) {
        Status = CheckPartitionFlash(HandleBuffer[Index]);
        if (!EFI_ERROR (Status)) {
          StorageHandle = HandleBuffer[Index];
          StorageFound = TRUE;
          break;
        }
      }
      FreePool (HandleBuffer);
    }
  }

  if (!StorageFound) {
    DEBUG ((DEBUG_ERROR, "%a: No storage partition\r\n", __FUNCTION__));
    ASSERT(FALSE);
    return EFI_NOT_FOUND;
  }

  Status = ReadBackupGpt (StorageHandle, &PartitionHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Error Reading GPT Header\n"));
    return EFI_DEVICE_ERROR;
  }

  Status = GptValidateHeader (&PartitionHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Invalid efi partition table header\n"));
    return EFI_DEVICE_ERROR;
  }

  //
  // Read the partition Entries;
  //
  PartitionEntryArray = AllocateZeroPool (GptPartitionTableSizeInBytes (&PartitionHeader));
  if (PartitionEntryArray == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = ReadStorageData (StorageHandle,
                            PartitionHeader.PartitionEntryLBA * GPT_PARTITION_BLOCK_SIZE,
                            PartitionEntryArray,
                            GptPartitionTableSizeInBytes (&PartitionHeader));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read GPT partition array (%r)\r\n", __FUNCTION__, Status));
    FreePool (PartitionEntryArray);
    return Status;
  }

  Status = GptValidatePartitionTable (&PartitionHeader, PartitionEntryArray);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Invalid PartitionEntryArray\r\n"));
    FreePool (PartitionEntryArray);
    return Status;
  }

  // Find variable and FTW partitions
  PartitionEntry = GptFindPartitionByName (&PartitionHeader,
                                           PartitionEntryArray,
                                           L"xusb-fw");
  if (PartitionEntry != NULL) {
    mUsbFwData.UsbFwSize = GptPartitionSizeInBlocks (PartitionEntry) * GPT_PARTITION_BLOCK_SIZE;
    UsbFwBuffer = AllocateZeroPool (mUsbFwData.UsbFwSize);
    mUsbFwData.UsbFwBase = UsbFwBuffer;
    Status = ReadStorageData (StorageHandle,
                              PartitionEntry->StartingLBA * GPT_PARTITION_BLOCK_SIZE,
                              mUsbFwData.UsbFwBase,
                              mUsbFwData.UsbFwSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to read Partition\r\n"));
    } else {
      if (0 == AsciiStrnCmp ((CONST CHAR8 *) mUsbFwData.UsbFwBase,
                             (CONST CHAR8 *) PcdGetPtr (PcdSignedImageHeaderSignature),
                             sizeof (UINT32))) {
        mUsbFwData.UsbFwSize -= PcdGet32 (PcdSignedImageHeaderSize);
        mUsbFwData.UsbFwBase = UsbFwBuffer + PcdGet32 (PcdSignedImageHeaderSize);
      }
      Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
                                                       &gNVIDIAUsbFwProtocolGuid,
                                                       (VOID*)&mUsbFwData,
                                                       NULL);
      if (EFI_ERROR(Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to install USB firmware protocol - %r\r\n", __FUNCTION__, Status));
      }
    }
  }

  FreePool (PartitionEntryArray);
  return Status;
}
