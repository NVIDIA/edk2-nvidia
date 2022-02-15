/** @file

Stub implementation of a flash device that reports device errors.

Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Protocol/BlockIo.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <HostBasedTestStubLib/FlashStubLib.h>

#include "FlashStubLibPrivate.h"

/**
  Reset the Block Device.

  @param  This                 Indicates a pointer to the calling context.
  @param  ExtendedVerification Driver may perform diagnostics on reset.

  @retval EFI_SUCCESS          The device was reset.
  @retval EFI_DEVICE_ERROR     The device is not functioning properly and could
                               not be reset.

**/
EFI_STATUS
EFIAPI
FaultyReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
) {
  return EFI_DEVICE_ERROR;
}

/**
  Read BufferSize bytes from Lba into Buffer.

  @param  This       Indicates a pointer to the calling context.
  @param  MediaId    Id of the media, changes every time the media is replaced.
  @param  Lba        The starting Logical Block Address to read from
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  Buffer     A pointer to the destination buffer for the data.
                     The caller is responsible for either having implicit
                     or explicit ownership of the buffer.

  @retval EFI_SUCCESS           The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR      The device reported an error
                                while performing the read.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHANGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the
                                block size of the device.
  @retval EFI_INVALID_PARAMETER The read request contains LBAs that are not
                                valid, or the buffer is not on proper alignment.

**/
EFI_STATUS
EFIAPI
FaultyReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  OUT VOID                  *Buffer
) {
  return EFI_DEVICE_ERROR;
}

/**
  Write BufferSize bytes from Lba into Buffer.

  @param  This       Indicates a pointer to the calling context.
  @param  MediaId    The media ID that the write request is for.
  @param  Lba        The starting logical block address to be written. Caller
                     is responsible for writing to only legitimate locations.
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  Buffer     A pointer to the source buffer for the data.

  @retval EFI_SUCCESS           The data was written correctly to the device.
  @retval EFI_WRITE_PROTECTED   The device can not be written to.
  @retval EFI_DEVICE_ERROR      The device reported an error
                                while performing the write.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHNAGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of
                                the block size of the device.
  @retval EFI_INVALID_PARAMETER The write request contains LBAs that are not
                                valid, or the buffer is not on proper alignment.

**/
EFI_STATUS
EFIAPI
FaultyWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
) {
  return EFI_DEVICE_ERROR;
}

/**
  Flush the Block Device.

  @param  This              Indicates a pointer to the calling context.

  @retval EFI_SUCCESS       All outstanding data was written to the device
  @retval EFI_DEVICE_ERROR  The device reported an error
                            while writting back the data
  @retval EFI_NO_MEDIA      There is no media in the device.

**/
EFI_STATUS
EFIAPI
FaultyFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
) {
  return EFI_DEVICE_ERROR;
}

FLASH_TEST_PRIVATE mFaultyFlashTestPrivate = {
  FLASH_TEST_PRIVATE_SIGNATURE,
  {
    EFI_BLOCK_IO_PROTOCOL_REVISION,
    &mFaultyFlashTestPrivate.Media,
    FaultyReset,
    FaultyReadBlocks,
    FaultyWriteBlocks,
    FaultyFlushBlocks,
  },
  {
    0,     // MediaId;
    FALSE, // RemovableMedia;
    TRUE,  // MediaPresent;
    FALSE, // LogicalPartition;
    FALSE, // ReadOnly;
    FALSE, // WriteCaching;
    0,     // BlockSize;
    1,     // IoAlign;
    0,     // LastBlock;
    0, // LowestAlignedLba;
    0, // LogicalBlocksPerPhysicalBlock;
    0, // OptimalTransferLengthGranularity;
  },
  0, // StartingAddr
  0, // Size
};

/**
  Initialize the Flash Stub.

  @param  Buffer                Pointer to the starting address for the flash
                                stub's memory.
  @param  BufferSize            BufferSize of the flash stub.
  @param  BlockSize             BlockSize of the flash stub.
  @param  IoAlign               IoAlign value for the BlockIo interface's media.
  @param  BlockIo               Pointer to the initialized BlockIo interface
                                returned back to the user.

  @retval EFI_SUCCESS           Initialization succeeded.
  @retval EFI_OUT_OF_RESOURCES  Internal memory allocation failed.
  @retval EFI_BAD_BUFFER_SIZE   BufferSize or BlockSize was 0,
                                or if BufferSize was not a multiple of BlockSize
  @retval EFI_INVALID_PARAMETER Buffer or BlockIo was NULL
**/
EFI_STATUS
EFIAPI
FaultyFlashStubInitialize (
  IN  VOID                   *Buffer,
  IN  UINTN                  BufferSize,
  IN  UINT32                 BlockSize,
  IN  UINT32                 IoAlign,
  OUT EFI_BLOCK_IO_PROTOCOL  **BlockIo
) {
  FLASH_TEST_PRIVATE *FaultyFlashTestPrivate;

  if (Buffer == NULL || BlockIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == 0 || BlockSize == 0 || BufferSize % BlockSize != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  FaultyFlashTestPrivate = AllocatePool(sizeof(FLASH_TEST_PRIVATE));
  if (FaultyFlashTestPrivate == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem(
    FaultyFlashTestPrivate,
    &mFaultyFlashTestPrivate,
    sizeof(FLASH_TEST_PRIVATE)
  );

  FaultyFlashTestPrivate->BlockIo.Media = &FaultyFlashTestPrivate->Media;

  FaultyFlashTestPrivate->StartingAddr = (UINTN)Buffer;
  FaultyFlashTestPrivate->Size         = BufferSize;

  FaultyFlashTestPrivate->Media.IoAlign   = IoAlign;
  FaultyFlashTestPrivate->Media.BlockSize = BlockSize;
  FaultyFlashTestPrivate->Media.LastBlock = (BufferSize + BlockSize - 1)
                                                / BlockSize - 1;

  *BlockIo = &FaultyFlashTestPrivate->BlockIo;

  return EFI_SUCCESS;
}

/**
  Clean up the space used by the flash stub if necessary.

  @param  BlockIo     BlockIo protocol of the flash stub.

  @retval EFI_SUCCESS Clean up was successful.
**/
EFI_STATUS
EFIAPI
FaultyFlashStubDestroy (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo
) {

  FLASH_TEST_PRIVATE *FaultyFlashTestPrivate;

  FaultyFlashTestPrivate = FLASH_TEST_PRIVATE_FROM_BLOCK_IO(BlockIo);

  if (FaultyFlashTestPrivate != NULL) {
    FreePool(FaultyFlashTestPrivate);
    FaultyFlashTestPrivate = NULL;
  }

  return EFI_SUCCESS;
}
