/** @file
  FW Partition Protocol BlockIo Dxe

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/BrBctUpdateDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FwPartitionDeviceLib.h>
#include <Library/GptLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Protocol/BlockIo.h>

#define FW_PARTITION_BLOCK_IO_MAX_DEVICES     3
#define FW_PARTITION_USER_PARTITION           0
#define FW_PARTITION_BOOT_PARTITION_ZERO      1
#define FW_PARTITION_BOOT_PARTITION_ONE       2
#define FW_PARTITION_BLOCK_IO_INFO_SIGNATURE  SIGNATURE_32 ('F','W','B','I')
#define FW_PARTITION_LOCAL_BUFFER_BLOCKS      8

// private BlockIo device data structure
typedef struct {
  UINT32                      Signature;
  UINT64                      Bytes;
  EFI_BLOCK_IO_PROTOCOL       *BlockIo;
  FW_PARTITION_DEVICE_INFO    DeviceInfo;
} FW_PARTITION_BLOCK_IO_INFO;

STATIC FW_PARTITION_BLOCK_IO_INFO  *mBlockIoInfo       = NULL;
STATIC UINTN                       mNumDevices         = 0;
STATIC EFI_EVENT                   mAddressChangeEvent = NULL;

/**
  Read data from device.

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset to read from
  @param[in]  Bytes             Number of bytes to read
  @param[out] Buffer            Address to read data into

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPBlockIoRead (
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  OUT VOID                      *Buffer
  )
{
  FW_PARTITION_BLOCK_IO_INFO  *BlockIoInfo;
  EFI_BLOCK_IO_PROTOCOL       *BlockIo;
  EFI_STATUS                  Status;

  if (EfiAtRuntime ()) {
    return EFI_UNSUPPORTED;
  }

  BlockIoInfo = CR (
                  DeviceInfo,
                  FW_PARTITION_BLOCK_IO_INFO,
                  DeviceInfo,
                  FW_PARTITION_BLOCK_IO_INFO_SIGNATURE
                  );
  BlockIo = BlockIoInfo->BlockIo;

  if (((Offset % BlockIo->Media->BlockSize) != 0) ||
      (ALIGN_POINTER (Buffer, BlockIo->Media->IoAlign) != Buffer) ||
      ((Bytes % BlockIo->Media->BlockSize) != 0))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = FwPartitionCheckOffsetAndBytes (BlockIoInfo->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: read offset=%llu, bytes=%u error: %r\n",
      __FUNCTION__,
      Offset,
      Bytes,
      Status
      ));
    return Status;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: read offset=%llu, bytes=%u\n",
    __FUNCTION__,
    Offset,
    Bytes
    ));

  return BlockIo->ReadBlocks (
                    BlockIo,
                    BlockIo->Media->MediaId,
                    Offset / BlockIo->Media->BlockSize,
                    Bytes,
                    Buffer
                    );
}

/**
  Write data to device.  Supports unaligned buffers and partial last block
  writes using copies through a local buffer, but Offset must be on a
  block boundary.

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset to write
  @param[in]  Bytes             Number of bytes to write
  @param[in]  Buffer            Address of write data

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPBlockIoWrite (
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  IN  CONST VOID                *Buffer
  )
{
  FW_PARTITION_BLOCK_IO_INFO  *BlockIoInfo;
  EFI_BLOCK_IO_PROTOCOL       *BlockIo;
  EFI_BLOCK_IO_MEDIA          *Media;
  EFI_STATUS                  Status;
  UINTN                       BlockSize;
  VOID                        *LocalBuffer;
  UINTN                       LocalBufferSize;
  EFI_LBA                     Lba;

  if (EfiAtRuntime ()) {
    return EFI_UNSUPPORTED;
  }

  BlockIoInfo = CR (
                  DeviceInfo,
                  FW_PARTITION_BLOCK_IO_INFO,
                  DeviceInfo,
                  FW_PARTITION_BLOCK_IO_INFO_SIGNATURE
                  );
  BlockIo   = BlockIoInfo->BlockIo;
  Media     = BlockIo->Media;
  BlockSize = Media->BlockSize;
  Lba       = Offset / BlockSize;

  if ((Offset % BlockSize) != 0) {
    return EFI_INVALID_PARAMETER;
  }

  Status = FwPartitionCheckOffsetAndBytes (BlockIoInfo->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: write offset=%llu, bytes=%u error: %r\n",
      __FUNCTION__,
      Offset,
      Bytes,
      Status
      ));
    return Status;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: write offset=%llu, bytes=%u\n",
    __FUNCTION__,
    Offset,
    Bytes
    ));

  // handle unaligned buffer and/or partial block write using a local buffer
  LocalBuffer = NULL;
  if ((ALIGN_POINTER (Buffer, Media->IoAlign) != Buffer) ||
      ((Bytes % BlockSize) != 0))
  {
    DEBUG ((DEBUG_VERBOSE, "Using local buffer for unaligned/partial write\n"));

    LocalBufferSize = FW_PARTITION_LOCAL_BUFFER_BLOCKS * BlockSize;
    LocalBuffer     = AllocateZeroPool (LocalBufferSize);
    if (LocalBuffer == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    ASSERT (LocalBuffer == ALIGN_POINTER (LocalBuffer, Media->IoAlign));
  }

  while (Bytes > 0) {
    UINTN  BytesToWrite;
    VOID   *WriteBuffer;

    if (LocalBuffer == NULL) {
      BytesToWrite = Bytes;
      WriteBuffer  = (VOID *)((UINTN)Buffer);     // remove CONST
    } else {
      WriteBuffer = LocalBuffer;
      if (Bytes >= LocalBufferSize) {
        BytesToWrite = LocalBufferSize;
      } else {
        BytesToWrite = Bytes;
        if (ALIGN_VALUE (BytesToWrite, BlockSize) != BytesToWrite) {
          BytesToWrite = ALIGN_VALUE (BytesToWrite, BlockSize);
          SetMem (
            (UINT8 *)LocalBuffer + Bytes,
            BytesToWrite - Bytes,
            0
            );
        }
      }

      CopyMem (LocalBuffer, Buffer, MIN (Bytes, BytesToWrite));
    }

    Status = BlockIo->WriteBlocks (
                        BlockIo,
                        Media->MediaId,
                        Lba,
                        BytesToWrite,
                        WriteBuffer
                        );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Error writing Lba=%llu, Bytes=%u: %r\n",
        __FUNCTION__,
        Lba,
        BytesToWrite,
        Status
        ));
      break;
    }

    Bytes -= MIN (Bytes, BytesToWrite);
    Buffer = (CONST UINT8 *)Buffer + BytesToWrite;
    Lba   += (BytesToWrite / BlockSize);
  }

  if (LocalBuffer != NULL) {
    FreePool (LocalBuffer);
  }

  return Status;
}

/**
  Read data from device boot partitions

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset to read from
  @param[in]  Bytes             Number of bytes to read
  @param[out] Buffer            Address to read data into

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPBlockIoReadBootPartition (
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  OUT VOID                      *Buffer
  )
{
  FW_PARTITION_BLOCK_IO_INFO  *BlockIoInfo;

  BlockIoInfo = &mBlockIoInfo[FW_PARTITION_BOOT_PARTITION_ZERO];

  if (Offset >= BlockIoInfo->Bytes) {
    Offset -= BlockIoInfo->Bytes;
    BlockIoInfo++;
  }

  return FPBlockIoRead (
           &BlockIoInfo->DeviceInfo,
           Offset,
           Bytes,
           Buffer
           );
}

/**
  Write data to device boot partitions.

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset to write
  @param[in]  Bytes             Number of bytes to write
  @param[in]  Buffer            Address of write data

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPBlockIoWriteBootPartition (
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  IN  CONST VOID                *Buffer
  )
{
  FW_PARTITION_BLOCK_IO_INFO  *BlockIoInfo;

  BlockIoInfo = &mBlockIoInfo[FW_PARTITION_BOOT_PARTITION_ZERO];

  if (Offset >= BlockIoInfo->Bytes) {
    Offset -= BlockIoInfo->Bytes;
    BlockIoInfo++;
  }

  return FPBlockIoWrite (
           &BlockIoInfo->DeviceInfo,
           Offset,
           Bytes,
           Buffer
           );
}

/**
  Check if device path is a supported BlockIo device:
     eMMC: Type == MESSAGING_DEVICE_PATH (3),  SubType == MSG_EMMC_DP (0x1D)
     SD:   Type == MESSAGING_DEVICE_PATH (3),  SubType == MSG_SD_DP   (0x1A)


  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset to begin erase
  @param[in]  Bytes             Number of bytes to erase

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPBlockIoIsSupportedDevicePath (
  IN  CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  BOOLEAN  ValidFlash;

  ValidFlash = FALSE;

  // Check for device with eMMC/Ctrl or SD
  while (IsDevicePathEnd (DevicePath) == FALSE) {
    if (DevicePath->Type == MESSAGING_DEVICE_PATH) {
      if (DevicePath->SubType == MSG_EMMC_DP) {
        DevicePath = NextDevicePathNode (DevicePath);
        if ((DevicePath->Type == HARDWARE_DEVICE_PATH) &&
            (DevicePath->SubType == HW_CONTROLLER_DP))
        {
          ValidFlash = TRUE;
          break;
        }
      }

      if (DevicePath->SubType == MSG_SD_DP) {
        ValidFlash = TRUE;
        break;
      }
    }

    DevicePath = NextDevicePathNode (DevicePath);
  }

  // if valid so far, check that path has no HD node
  if (ValidFlash) {
    DevicePath = NextDevicePathNode (DevicePath);
    if (IsDevicePathEnd (DevicePath)) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Find BlockIo devices and initialize private data structures.

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FPBlockIoInitDevices (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       NumHandles;
  EFI_HANDLE  *HandleBuffer;
  UINTN       Index;
  CHAR16      *DeviceName;

  DEBUG ((DEBUG_INFO, "%a: Entry\n", __FUNCTION__));

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiBlockIoProtocolGuid,
                  NULL,
                  &NumHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Error locating BlockIo handles: %r\n", Status));
    return Status;
  }

  for (Index = 0; Index < NumHandles; Index++) {
    EFI_HANDLE                  Handle = HandleBuffer[Index];
    EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
    EFI_BLOCK_IO_PROTOCOL       *BlockIo;
    FW_PARTITION_BLOCK_IO_INFO  *BlockIoInfo;
    FW_PARTITION_DEVICE_INFO    *DeviceInfo;

    Status = gBS->HandleProtocol (
                    Handle,
                    &gEfiBlockIoProtocolGuid,
                    (VOID **)&BlockIo
                    );
    if (EFI_ERROR (Status) || (BlockIo == NULL)) {
      DEBUG ((
        DEBUG_INFO,
        "Failed to get BlockIo for handle index %u: %r\n",
        Index,
        Status
        ));
      continue;
    }

    Status = gBS->HandleProtocol (
                    Handle,
                    &gEfiDevicePathProtocolGuid,
                    (VOID **)&DevicePath
                    );
    if (EFI_ERROR (Status) || (DevicePath == NULL)) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to get DevicePath for handle index %u: %r\n",
        Index,
        Status
        ));
      continue;
    }

    if (BlockIo->Media->LogicalPartition) {
      continue;
    }

    if (!FPBlockIoIsSupportedDevicePath (DevicePath)) {
      DEBUG ((
        DEBUG_INFO,
        "Handle index=%u is not a supported flash DevicePath\n",
        Index
        ));
      continue;
    }

    DeviceName = ConvertDevicePathToText (DevicePath, TRUE, TRUE);
    DEBUG ((
      DEBUG_INFO,
      "Found device=%s, BlockSize=%u, LastBlock=%llu\n",
      DeviceName,
      BlockIo->Media->BlockSize,
      BlockIo->Media->LastBlock
      ));

    if (mNumDevices >= FW_PARTITION_BLOCK_IO_MAX_DEVICES) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Max devices=%d exceeded\n",
        __FUNCTION__,
        FW_PARTITION_BLOCK_IO_MAX_DEVICES
        ));
      break;
    }

    BlockIoInfo            = &mBlockIoInfo[mNumDevices];
    BlockIoInfo->Signature = FW_PARTITION_BLOCK_IO_INFO_SIGNATURE;
    BlockIoInfo->Bytes     = ((BlockIo->Media->LastBlock + 1) *
                              BlockIo->Media->BlockSize);
    BlockIoInfo->BlockIo = BlockIo;

    DeviceInfo             = &BlockIoInfo->DeviceInfo;
    DeviceInfo->DeviceName = DeviceName;
    DeviceInfo->BlockSize  = BlockIo->Media->BlockSize;

    if (mNumDevices == FW_PARTITION_USER_PARTITION) {
      DeviceInfo->DeviceRead  = FPBlockIoRead;
      DeviceInfo->DeviceWrite = FPBlockIoWrite;
    } else {
      DeviceInfo->DeviceRead  = FPBlockIoReadBootPartition;
      DeviceInfo->DeviceWrite = FPBlockIoWriteBootPartition;
    }

    mNumDevices++;
  }

  FreePool (HandleBuffer);

  return EFI_SUCCESS;
}

/**
  Convert given pointer to support runtime execution.

  @param[in]  Pointer       Address of pointer to convert

  @retval None

**/
STATIC
VOID
EFIAPI
FPBlockIoAddressConvert (
  IN  VOID  **Pointer
  )
{
  EfiConvertPointer (0x0, Pointer);
}

/**
  Handle address change notification to support runtime execution.
  Note: BlockIo requests are rejected at runtime with EFI_UNSUPPORTED.

  @param[in]  Event         Event being handled
  @param[in]  Context       Event context

  @retval None

**/
STATIC
VOID
EFIAPI
FPBlockIoAddressChangeNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINTN                       Index;
  FW_PARTITION_BLOCK_IO_INFO  *BlockIoInfo;

  BlockIoInfo = mBlockIoInfo;
  for (Index = 0; Index < mNumDevices; Index++, BlockIoInfo++) {
    FW_PARTITION_DEVICE_INFO  *DeviceInfo;

    EfiConvertPointer (0x0, (VOID **)&BlockIoInfo->BlockIo);

    DeviceInfo = &BlockIoInfo->DeviceInfo;
    EfiConvertPointer (0x0, (VOID **)&DeviceInfo->DeviceName);
    EfiConvertPointer (0x0, (VOID **)&DeviceInfo->DeviceRead);
    EfiConvertPointer (0x0, (VOID **)&DeviceInfo->DeviceWrite);
  }

  EfiConvertPointer (0x0, (VOID **)&mBlockIoInfo);

  BrBctUpdateAddressChangeHandler (FPBlockIoAddressConvert);
  FwPartitionAddressChangeHandler (FPBlockIoAddressConvert);
}

/**
  Fw Partition Block IO Driver initialization entry point.

  @param[in]  ImageHandle       Image handle
  @param[in]  SystemTable       Pointer to system table

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FwPartitionBlockIoDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                  Status;
  UINTN                       Index;
  FW_PARTITION_BLOCK_IO_INFO  *BlockIoInfo;
  FW_PARTITION_DEVICE_INFO    *DeviceInfo;
  UINT32                      ActiveBootChain;
  BR_BCT_UPDATE_PRIVATE_DATA  *BrBctUpdatePrivate;
  FW_PARTITION_PRIVATE_DATA   *FwPartitionPrivate;
  VOID                        *Hob;
  BOOLEAN                     PcdOverwriteActiveFwPartition;

  PcdOverwriteActiveFwPartition = PcdGetBool (PcdOverwriteActiveFwPartition);
  BrBctUpdatePrivate            = NULL;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    ActiveBootChain = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ActiveBootChain;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting active boot chain\n",
      __FUNCTION__
      ));
    return EFI_UNSUPPORTED;
  }

  Status = FwPartitionDeviceLibInit (ActiveBootChain, MAX_FW_PARTITIONS, PcdOverwriteActiveFwPartition, TegraGetChipID (), GetBootChainForGpt ());
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FwPartition lib init failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  mBlockIoInfo = (FW_PARTITION_BLOCK_IO_INFO *)AllocateRuntimeZeroPool (
                                                 FW_PARTITION_BLOCK_IO_MAX_DEVICES * sizeof (FW_PARTITION_BLOCK_IO_INFO)
                                                 );
  if (mBlockIoInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "mBlockIoInfo allocation failed\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = FPBlockIoInitDevices ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a: Error initializing BlockIo devices: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  // add FwPartition structs from GPT on device user partition
  if (mNumDevices > FW_PARTITION_USER_PARTITION) {
    BlockIoInfo = &mBlockIoInfo[FW_PARTITION_USER_PARTITION];
    DeviceInfo  = &BlockIoInfo->DeviceInfo;
    Status      = FwPartitionAddFromDeviceGpt (DeviceInfo, BlockIoInfo->Bytes);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_INFO,
        "%a: Error adding partitions from FW device=%s: %r\n",
        __FUNCTION__,
        DeviceInfo->DeviceName,
        Status
        ));
    }
  }

  // add FwPartition structs from GPT on device 2nd boot partition
  if (mNumDevices > FW_PARTITION_BOOT_PARTITION_ONE) {
    BlockIoInfo = &mBlockIoInfo[FW_PARTITION_BOOT_PARTITION_ONE];
    DeviceInfo  = &BlockIoInfo->DeviceInfo;
    Status      = FwPartitionAddFromDeviceGpt (DeviceInfo, 2 * BlockIoInfo->Bytes);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_INFO,
        "%a: Error adding partitions from FW device=%s: %r\n",
        __FUNCTION__,
        DeviceInfo->DeviceName,
        Status
        ));
    }
  }

  // install FwPartition protocols for all partitions
  FwPartitionPrivate = FwPartitionGetPrivateArray ();
  for (Index = 0; Index < FwPartitionGetCount (); Index++, FwPartitionPrivate++) {
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &FwPartitionPrivate->Handle,
                    &gNVIDIAFwPartitionProtocolGuid,
                    &FwPartitionPrivate->Protocol,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Couldn't install protocol for Index=%u, partition=%s: %r\n",
        __FUNCTION__,
        Index,
        FwPartitionPrivate->PartitionInfo.Name,
        Status
        ));
      goto Done;
    }
  }

  Status = BrBctUpdateDeviceLibInit (
             ActiveBootChain,
             1
             );
  if (Status == EFI_SUCCESS) {
    BrBctUpdatePrivate = BrBctUpdateGetPrivate ();
    Status             = gBS->InstallMultipleProtocolInterfaces (
                                &BrBctUpdatePrivate->Handle,
                                &gNVIDIABrBctUpdateProtocolGuid,
                                &BrBctUpdatePrivate->Protocol,
                                NULL
                                );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Couldn't install BR-BCT update protocol: %r\n",
        __FUNCTION__,
        Status
        ));
      goto Done;
    }
  } else {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Error initializing BrBct lib: %r\n",
        __FUNCTION__,
        Status
        ));
      goto Done;
    }
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  FPBlockIoAddressChangeNotify,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mAddressChangeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error creating address change event Status = %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

Done:
  if (EFI_ERROR (Status)) {
    if (mAddressChangeEvent != NULL) {
      gBS->CloseEvent (mAddressChangeEvent);
      mAddressChangeEvent = NULL;
    }

    if ((BrBctUpdatePrivate != NULL) && (BrBctUpdatePrivate->Handle != NULL)) {
      gBS->UninstallMultipleProtocolInterfaces (
             BrBctUpdatePrivate->Handle,
             &gNVIDIABrBctUpdateProtocolGuid,
             &BrBctUpdatePrivate->Protocol,
             NULL
             );
      BrBctUpdatePrivate->Handle = NULL;
    }

    FwPartitionPrivate = FwPartitionGetPrivateArray ();
    for (Index = 0; Index < FwPartitionGetCount (); Index++, FwPartitionPrivate++) {
      if (FwPartitionPrivate->Handle != NULL) {
        EFI_STATUS  LocalStatus;

        LocalStatus = gBS->UninstallMultipleProtocolInterfaces (
                             FwPartitionPrivate->Handle,
                             &gNVIDIAFwPartitionProtocolGuid,
                             &FwPartitionPrivate->Protocol,
                             NULL
                             );
        if (EFI_ERROR (LocalStatus)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Error uninstalling protocol for partition=%s: %r\n",
            __FUNCTION__,
            FwPartitionPrivate->PartitionInfo.Name,
            LocalStatus
            ));
        }

        FwPartitionPrivate->Handle = NULL;
      }
    }

    BrBctUpdateDeviceLibDeinit ();
    FwPartitionDeviceLibDeinit ();

    if (mBlockIoInfo != NULL) {
      FreePool (mBlockIoInfo);
      mBlockIoInfo = NULL;
    }

    mNumDevices = 0;
  }

  return Status;
}
