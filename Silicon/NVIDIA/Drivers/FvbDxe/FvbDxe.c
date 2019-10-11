/** @file

  Fvb Driver

  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Ltd. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <FvbPrivate.h>

NVIDIA_FVB_PRIVATE_DATA        *Private;
EFI_EVENT                      FvbVirtualAddrChangeEvent;

/**
  The GetAttributes() function retrieves the attributes and
  current settings of the block.

  @param This       Indicates the EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL instance.

  @param Attributes Pointer to EFI_FVB_ATTRIBUTES_2 in which the
                    attributes and current settings are
                    returned. Type EFI_FVB_ATTRIBUTES_2 is defined
                    in EFI_FIRMWARE_VOLUME_HEADER.

  @retval EFI_SUCCESS The firmware volume attributes were
                      returned.

**/
EFI_STATUS
EFIAPI
FvbGetAttributes(
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL    *This,
  OUT       EFI_FVB_ATTRIBUTES_2                   *Attributes
  )
{
  if (Attributes == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Attributes = ((EFI_FIRMWARE_VOLUME_HEADER *)Private->VariablePartition)->Attributes;

  return EFI_SUCCESS;
}

/**
  The SetAttributes() function sets configurable firmware volume
  attributes and returns the new settings of the firmware volume.

  @param This         Indicates the EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL instance.

  @param Attributes   On input, Attributes is a pointer to
                      EFI_FVB_ATTRIBUTES_2 that contains the
                      desired firmware volume settings. On
                      successful return, it contains the new
                      settings of the firmware volume. Type
                      EFI_FVB_ATTRIBUTES_2 is defined in
                      EFI_FIRMWARE_VOLUME_HEADER.

  @retval EFI_SUCCESS           The firmware volume attributes were returned.

  @retval EFI_INVALID_PARAMETER The attributes requested are in
                                conflict with the capabilities
                                as declared in the firmware
                                volume header.

**/
EFI_STATUS
EFIAPI
FvbSetAttributes(
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  IN OUT    EFI_FVB_ATTRIBUTES_2                 *Attributes
  )
{
  return EFI_UNSUPPORTED;
}

/**
  The GetPhysicalAddress() function retrieves the base address of
  a memory-mapped firmware volume. This function should be called
  only for memory-mapped firmware volumes.

  @param This     Indicates the EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL instance.

  @param Address  Pointer to a caller-allocated
                  EFI_PHYSICAL_ADDRESS that, on successful
                  return from GetPhysicalAddress(), contains the
                  base address of the firmware volume.

  @retval EFI_SUCCESS       The firmware volume base address was returned.

  @retval EFI_UNSUPPORTED   The firmware volume is not memory mapped.

**/
EFI_STATUS
EFIAPI
FvbGetPhysicalAddress (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  OUT       EFI_PHYSICAL_ADDRESS                 *Address
  )
{
  if (Address == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Address = (EFI_PHYSICAL_ADDRESS)Private->VariablePartition;
  return EFI_SUCCESS;
}

/**
  The GetBlockSize() function retrieves the size of the requested
  block. It also returns the number of additional blocks with
  the identical size. The GetBlockSize() function is used to
  retrieve the block map (see EFI_FIRMWARE_VOLUME_HEADER).


  @param This           Indicates the EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL instance.

  @param Lba            Indicates the block for which to return the size.

  @param BlockSize      Pointer to a caller-allocated UINTN in which
                        the size of the block is returned.

  @param NumberOfBlocks Pointer to a caller-allocated UINTN in
                        which the number of consecutive blocks,
                        starting with Lba, is returned. All
                        blocks in this range have a size of
                        BlockSize.


  @retval EFI_SUCCESS             The firmware volume base address was returned.

  @retval EFI_INVALID_PARAMETER   The requested LBA is out of range.

**/
EFI_STATUS
EFIAPI
FvbGetBlockSize (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  IN        EFI_LBA                              Lba,
  OUT       UINTN                                *BlockSize,
  OUT       UINTN                                *NumberOfBlocks
  )
{
  EFI_STATUS Status;
  EFI_LBA    LastBlock;

  if ((BlockSize == NULL) ||
      (NumberOfBlocks == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  LastBlock = Private->NumBlocks - 1;

  if (Lba > LastBlock) {
    Status = EFI_INVALID_PARAMETER;
  } else {
    *BlockSize = Private->BlockIo->Media->BlockSize;
    *NumberOfBlocks = LastBlock - Lba + 1;

    Status = EFI_SUCCESS;
  }

  return Status;
}

/**
  Reads the specified number of bytes into a buffer from the specified block.

  The Read() function reads the requested number of bytes from the
  requested block and stores them in the provided buffer.
  Implementations should be mindful that the firmware volume
  might be in the ReadDisabled state. If it is in this state,
  the Read() function must return the status code
  EFI_ACCESS_DENIED without modifying the contents of the
  buffer. The Read() function must also prevent spanning block
  boundaries. If a read is requested that would span a block
  boundary, the read must read up to the boundary but not
  beyond. The output parameter NumBytes must be set to correctly
  indicate the number of bytes actually read. The caller must be
  aware that a read may be partially completed.

  @param This     Indicates the EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL instance.

  @param Lba      The starting logical block index
                  from which to read.

  @param Offset   Offset into the block at which to begin reading.

  @param NumBytes Pointer to a UINTN. At entry, *NumBytes
                  contains the total size of the buffer. At
                  exit, *NumBytes contains the total number of
                  bytes read.

  @param Buffer   Pointer to a caller-allocated buffer that will
                  be used to hold the data that is read.

  @retval EFI_SUCCESS         The firmware volume was read successfully,
                              and contents are in Buffer.

  @retval EFI_BAD_BUFFER_SIZE Read attempted across an LBA
                              boundary. On output, NumBytes
                              contains the total number of bytes
                              returned in Buffer.

  @retval EFI_ACCESS_DENIED   The firmware volume is in the
                              ReadDisabled state.

  @retval EFI_DEVICE_ERROR    The block device is not
                              functioning correctly and could
                              not be read.

**/
EFI_STATUS
EFIAPI
FvbRead (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL   *This,
  IN        EFI_LBA                               Lba,
  IN        UINTN                                 Offset,
  IN OUT    UINTN                                 *NumBytes,
  IN OUT    UINT8                                 *Buffer
  )
{
  UINT32        BlockSize;
  UINT64        FvbOffset;
  EFI_LBA       LastBlock;

  if ((NumBytes == NULL) ||
      (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Offset > (MAX_UINT64 - *NumBytes)) ||
      (*NumBytes > (MAX_UINT64 - Offset))) {
    return EFI_INVALID_PARAMETER;
  }

  BlockSize = Private->BlockIo->Media->BlockSize;
  LastBlock = Private->NumBlocks - 1;

  // The read must not span FV boundaries.
  if (Lba > LastBlock) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // We must have some bytes to read
  if (*NumBytes == 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // The read must not span block boundaries.
  // We need to check each variable individually because adding two large values together overflows.
  if (Offset >= BlockSize) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if ((Offset + *NumBytes) >  BlockSize) {
    *NumBytes = BlockSize - Offset;
  }

  FvbOffset = MultU64x32 (Lba, BlockSize) + Offset;

  CopyMem(Buffer, Private->VariablePartition + FvbOffset, *NumBytes);

  return EFI_SUCCESS;
}

/**
  Writes the specified number of bytes from the input buffer to the block.

  The Write() function writes the specified number of bytes from
  the provided buffer to the specified block and offset. If the
  firmware volume is sticky write, the caller must ensure that
  all the bits of the specified range to write are in the
  EFI_FVB_ERASE_POLARITY state before calling the Write()
  function, or else the result will be unpredictable. This
  unpredictability arises because, for a sticky-write firmware
  volume, a write may negate a bit in the EFI_FVB_ERASE_POLARITY
  state but cannot flip it back again.  Before calling the
  Write() function,  it is recommended for the caller to first call
  the EraseBlocks() function to erase the specified block to
  write. A block erase cycle will transition bits from the
  (NOT)EFI_FVB_ERASE_POLARITY state back to the
  EFI_FVB_ERASE_POLARITY state. Implementations should be
  mindful that the firmware volume might be in the WriteDisabled
  state. If it is in this state, the Write() function must
  return the status code EFI_ACCESS_DENIED without modifying the
  contents of the firmware volume. The Write() function must
  also prevent spanning block boundaries. If a write is
  requested that spans a block boundary, the write must store up
  to the boundary but not beyond. The output parameter NumBytes
  must be set to correctly indicate the number of bytes actually
  written. The caller must be aware that a write may be
  partially completed. All writes, partial or otherwise, must be
  fully flushed to the hardware before the Write() service
  returns.

  @param This     Indicates the EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL instance.

  @param Lba      The starting logical block index to write to.

  @param Offset   Offset into the block at which to begin writing.

  @param NumBytes The pointer to a UINTN. At entry, *NumBytes
                  contains the total size of the buffer. At
                  exit, *NumBytes contains the total number of
                  bytes actually written.

  @param Buffer   The pointer to a caller-allocated buffer that
                  contains the source for the write.

  @retval EFI_SUCCESS         The firmware volume was written successfully.

  @retval EFI_BAD_BUFFER_SIZE The write was attempted across an
                              LBA boundary. On output, NumBytes
                              contains the total number of bytes
                              actually written.

  @retval EFI_ACCESS_DENIED   The firmware volume is in the
                              WriteDisabled state.

  @retval EFI_DEVICE_ERROR    The block device is malfunctioning
                              and could not be written.


**/
EFI_STATUS
EFIAPI
FvbWrite (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL   *This,
  IN        EFI_LBA                               Lba,
  IN        UINTN                                 Offset,
  IN OUT    UINTN                                 *NumBytes,
  IN        UINT8                                 *Buffer
  )
{
  EFI_STATUS    Status;
  UINT32        BlockSize;
  UINT64        FvbOffset;
  EFI_LBA       LastBlock;

  if (EfiAtRuntime()) {
    return EFI_UNSUPPORTED;
  }

  if ((NumBytes == NULL) ||
      (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Offset > (MAX_UINT64 - *NumBytes)) ||
      (*NumBytes > (MAX_UINT64 - Offset))) {
    return EFI_INVALID_PARAMETER;
  }

  BlockSize = Private->BlockIo->Media->BlockSize;
  LastBlock = Private->NumBlocks - 1;

  // The write must not span FV boundaries.
  if (Lba > LastBlock) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // We must have some bytes to write
  if (*NumBytes == 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // The write must not span block boundaries.
  // We need to check each variable individually because adding two large values together overflows.
  if (Offset >= BlockSize) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if ((Offset + *NumBytes) >  BlockSize) {
    *NumBytes = BlockSize - Offset;
  }

  //Modify FVB
  FvbOffset = MultU64x32 (Lba, BlockSize) + Offset;
  CopyMem(Private->VariablePartition + FvbOffset, Buffer, *NumBytes);

  // Update storage block
  FvbOffset = MultU64x32 (Lba, BlockSize);
  Status = Private->BlockIo->WriteBlocks (Private->BlockIo,
                                          Private->BlockIo->Media->MediaId,
                                          Private->PartitionStartingLBA + Lba,
                                          BlockSize,
                                          Private->VariablePartition + FvbOffset);

  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: FVB write failed. Recovered FVB could be corrupt.\n", __FUNCTION__));
    ASSERT_EFI_ERROR(Private->BlockIo->ReadBlocks (Private->BlockIo,
                                                Private->BlockIo->Media->MediaId,
                                                Private->PartitionStartingLBA + Lba,
                                                BlockSize,
                                                Private->VariablePartition + FvbOffset));
  }

  return Status;
}

/**
  Erases and initializes a firmware volume block.

  The EraseBlocks() function erases one or more blocks as denoted
  by the variable argument list. The entire parameter list of
  blocks must be verified before erasing any blocks. If a block is
  requested that does not exist within the associated firmware
  volume (it has a larger index than the last block of the
  firmware volume), the EraseBlocks() function must return the
  status code EFI_INVALID_PARAMETER without modifying the contents
  of the firmware volume. Implementations should be mindful that
  the firmware volume might be in the WriteDisabled state. If it
  is in this state, the EraseBlocks() function must return the
  status code EFI_ACCESS_DENIED without modifying the contents of
  the firmware volume. All calls to EraseBlocks() must be fully
  flushed to the hardware before the EraseBlocks() service
  returns.

  @param This   Indicates the EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL
                instance.

  @param ...    The variable argument list is a list of tuples.
                Each tuple describes a range of LBAs to erase
                and consists of the following:
                - An EFI_LBA that indicates the starting LBA
                - A UINTN that indicates the number of blocks to
                  erase.

                The list is terminated with an
                EFI_LBA_LIST_TERMINATOR. For example, the
                following indicates that two ranges of blocks
                (5-7 and 10-11) are to be erased: EraseBlocks
                (This, 5, 3, 10, 2, EFI_LBA_LIST_TERMINATOR);

  @retval EFI_SUCCESS The erase request successfully
                      completed.

  @retval EFI_ACCESS_DENIED   The firmware volume is in the
                              WriteDisabled state.
  @retval EFI_DEVICE_ERROR  The block device is not functioning
                            correctly and could not be written.
                            The firmware device may have been
                            partially erased.
  @retval EFI_INVALID_PARAMETER One or more of the LBAs listed
                                in the variable argument list do
                                not exist in the firmware volume.

**/
EFI_STATUS
EFIAPI
FvbEraseBlocks (
  IN CONST EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL *This,
  ...
  )
{
  EFI_STATUS  Status;
  VA_LIST     Args;
  EFI_LBA     StartingLba;
  UINTN       NumOfLba;
  UINT32      BlockSize;
  EFI_LBA     LastBlock;
  UINT64      FvbOffset;
  UINT64      FvbBufferSize;

  if (EfiAtRuntime()) {
    return EFI_UNSUPPORTED;
  }

  //If no blocks are passed in return should be invalid parameter.
  Status = EFI_INVALID_PARAMETER;

  BlockSize = Private->BlockIo->Media->BlockSize;
  LastBlock = Private->NumBlocks - 1;

  // Before erasing, check the entire list of parameters to ensure all specified blocks are valid
  VA_START (Args, This);
  do {
    // Get the Lba from which we start erasing
    StartingLba = VA_ARG (Args, EFI_LBA);

    // Have we reached the end of the list?
    if (StartingLba == EFI_LBA_LIST_TERMINATOR) {
      //Exit the while loop
      break;
    }

    // How many Lba blocks are we requested to erase?
    NumOfLba = VA_ARG (Args, UINTN);

    // All blocks must be within range
    if ((NumOfLba == 0) ||
        (StartingLba > (MAX_UINT64 - NumOfLba)) ||
        (NumOfLba > (MAX_UINT64 - StartingLba)) ||
        ((StartingLba + NumOfLba - 1) > LastBlock)) {
      VA_END (Args);
      return EFI_INVALID_PARAMETER;
    }
  } while (TRUE);
  VA_END (Args);

  //
  // To get here, all must be ok, so start erasing
  //
  VA_START (Args, This);
  do {
    // Get the Lba from which we start erasing
    StartingLba = VA_ARG (Args, EFI_LBA);

    // Have we reached the end of the list?
    if (StartingLba == EFI_LBA_LIST_TERMINATOR) {
      // Exit the while loop
      break;
    }

    // How many Lba blocks are we requested to erase?
    NumOfLba = VA_ARG (Args, UINTN);

    //Modify FVB
    FvbOffset = MultU64x32 (StartingLba, BlockSize);
    FvbBufferSize = MultU64x32 (NumOfLba, BlockSize);
    SetMem(Private->VariablePartition + FvbOffset, FvbBufferSize, 0xFF);

    Status = Private->BlockIo->WriteBlocks(Private->BlockIo,
                                           Private->BlockIo->Media->MediaId,
                                           Private->PartitionStartingLBA + StartingLba,
                                           FvbBufferSize,
                                           Private->VariablePartition + FvbOffset);
    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_ERROR, "%a: FVB write failed. Recovered FVB could be corrupt.\n", __FUNCTION__));
      ASSERT_EFI_ERROR(Private->BlockIo->ReadBlocks(Private->BlockIo,
                                                 Private->BlockIo->Media->MediaId,
                                                 Private->PartitionStartingLBA + StartingLba,
                                                 FvbBufferSize,
                                                 Private->VariablePartition + FvbOffset));
      break;
    }

  } while (TRUE);
  VA_END (Args);

  return Status;
}

/**
  Initializes the FV Header and Variable Store Header
  to support variable operations.

  @param FirmwareVolumeHeader - A pointer to a firmware volume header

**/
EFI_STATUS
InitializeFvAndVariableStoreHeaders (
  IN EFI_FIRMWARE_VOLUME_HEADER *FirmwareVolumeHeader
  )
{
  VARIABLE_STORE_HEADER               *VariableStoreHeader;

  if (FirmwareVolumeHeader == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Check if the size of the area is at least one block size
  if ((PcdGet32(PcdFlashNvStorageVariableSize) <= 0) ||
      ((PcdGet32(PcdFlashNvStorageVariableSize) / Private->BlockIo->Media->BlockSize) <= 0)) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // EFI_FIRMWARE_VOLUME_HEADER
  //
  ZeroMem (FirmwareVolumeHeader,
           sizeof(EFI_FIRMWARE_VOLUME_HEADER) + sizeof(EFI_FV_BLOCK_MAP_ENTRY));
  CopyGuid (&FirmwareVolumeHeader->FileSystemGuid, &gEfiSystemNvDataFvGuid);
  FirmwareVolumeHeader->FvLength = PcdGet32(PcdFlashNvStorageVariableSize);
  FirmwareVolumeHeader->Signature = EFI_FVH_SIGNATURE;
  FirmwareVolumeHeader->Attributes = (EFI_FVB_ATTRIBUTES_2) (
                                          EFI_FVB2_READ_ENABLED_CAP   | // Reads may be enabled
                                          EFI_FVB2_READ_STATUS        | // Reads are currently enabled
                                          EFI_FVB2_STICKY_WRITE       | // A block erase is required to flip bits into EFI_FVB2_ERASE_POLARITY
                                          EFI_FVB2_MEMORY_MAPPED      | // It is memory mapped
                                          EFI_FVB2_ERASE_POLARITY     | // After erasure all bits take this value (i.e. '1')
                                          EFI_FVB2_WRITE_STATUS       | // Writes are currently enabled
                                          EFI_FVB2_WRITE_ENABLED_CAP    // Writes may be enabled
                                      );
  FirmwareVolumeHeader->HeaderLength = sizeof(EFI_FIRMWARE_VOLUME_HEADER) + sizeof(EFI_FV_BLOCK_MAP_ENTRY);
  FirmwareVolumeHeader->Revision = EFI_FVH_REVISION;
  FirmwareVolumeHeader->BlockMap[0].NumBlocks = Private->NumBlocks;
  FirmwareVolumeHeader->BlockMap[0].Length = Private->BlockIo->Media->BlockSize;
  FirmwareVolumeHeader->BlockMap[1].NumBlocks = 0;
  FirmwareVolumeHeader->BlockMap[1].Length = 0;
  FirmwareVolumeHeader->Checksum = CalculateCheckSum16 ((UINT16*)FirmwareVolumeHeader,FirmwareVolumeHeader->HeaderLength);

  //
  // VARIABLE_STORE_HEADER
  //
  VariableStoreHeader = (VARIABLE_STORE_HEADER*)((UINTN)FirmwareVolumeHeader + FirmwareVolumeHeader->HeaderLength);
  ZeroMem (VariableStoreHeader,
           sizeof(VARIABLE_STORE_HEADER));
  CopyGuid (&VariableStoreHeader->Signature, &gEfiAuthenticatedVariableGuid);
  VariableStoreHeader->Size = PcdGet32(PcdFlashNvStorageVariableSize) - FirmwareVolumeHeader->HeaderLength;
  VariableStoreHeader->Format = VARIABLE_STORE_FORMATTED;
  VariableStoreHeader->State  = VARIABLE_STORE_HEALTHY;

  // Write the combined super-header in the flash
  return Private->BlockIo->WriteBlocks (Private->BlockIo,
                                        Private->BlockIo->Media->MediaId,
                                        Private->PartitionStartingLBA,
                                        Private->BlockIo->Media->BlockSize,
                                        FirmwareVolumeHeader);
}

/**
  Check the integrity of firmware volume header.

  @param FwVolHeader - A pointer to a firmware volume header

**/
EFI_STATUS
ValidateFvHeader (
  IN EFI_FIRMWARE_VOLUME_HEADER *FwVolHeader
  )
{
  UINT16                      Checksum;
  VARIABLE_STORE_HEADER       *VariableStoreHeader;
  UINTN                       VariableStoreLength;
  UINTN                       FvLength;

  FvLength = PcdGet32(PcdFlashNvStorageVariableSize);

  //
  // Verify the header revision, header signature, length
  // Length of FvBlock cannot be 2**64-1
  // HeaderLength cannot be an odd number
  //
  if ((FwVolHeader->Revision  != EFI_FVH_REVISION)  ||
      (FwVolHeader->Signature != EFI_FVH_SIGNATURE) ||
      (FwVolHeader->FvLength  != FvLength)) {
    DEBUG ((EFI_D_INFO, "%a: No Firmware Volume header present\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  // Check the Firmware Volume Guid
  if( CompareGuid (&FwVolHeader->FileSystemGuid, &gEfiSystemNvDataFvGuid) == FALSE ) {
    DEBUG ((EFI_D_INFO, "%a: Firmware Volume Guid non-compatible\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  // Verify the header checksum
  Checksum = CalculateSum16((UINT16*)FwVolHeader, FwVolHeader->HeaderLength);
  if (Checksum != 0) {
    DEBUG ((EFI_D_INFO, "%a: FV checksum is invalid (Checksum:0x%X)\n", __FUNCTION__, Checksum));
    return EFI_NOT_FOUND;
  }

  VariableStoreHeader = (VARIABLE_STORE_HEADER*)((UINTN)FwVolHeader + FwVolHeader->HeaderLength);

  // Check the Variable Store Guid
  if (!CompareGuid (&VariableStoreHeader->Signature, &gEfiVariableGuid) &&
      !CompareGuid (&VariableStoreHeader->Signature, &gEfiAuthenticatedVariableGuid)) {
    DEBUG ((EFI_D_INFO, "%a: Variable Store Guid non-compatible\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  VariableStoreLength = PcdGet32 (PcdFlashNvStorageVariableSize) - FwVolHeader->HeaderLength;

  if (VariableStoreHeader->Size != VariableStoreLength) {
    DEBUG ((EFI_D_INFO, "%a: Variable Store Length does not match\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  Fixup internal data so that EFI can be call in virtual mode.
  Call the passed in Child Notify event and convert any pointers in
  lib to virtual mode.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
FVBVirtualNotifyEvent (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  EfiConvertPointer (0x0, (VOID**)&Private->BlockIo);
  EfiConvertPointer (0x0, (VOID**)&Private->VariablePartition);
  EfiConvertPointer (0x0, (VOID**)&Private);
  return;
}

/**
  Get the size of the largest block that can be updated in a fault-tolerant manner.

  @param  This                 Indicates a pointer to the calling context.
  @param  BlockSize            A pointer to a caller-allocated UINTN that is
                               updated to indicate the size of the largest block
                               that can be updated.

  @retval EFI_SUCCESS          The function completed successfully.
  @retval EFI_ABORTED          The function could not complete successfully.

**/
EFI_STATUS
EFIAPI
FtwGetMaxBlockSize(
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL    * This,
  OUT UINTN                               *BlockSize
  )
{
  *BlockSize = PcdGet32 (PcdFlashNvStorageVariableSize);

  return EFI_SUCCESS;
}

/**
  Allocates space for the protocol to maintain information about writes.
  Since writes must be completed in a fault-tolerant manner and multiple
  writes require more resources to be successful, this function
  enables the protocol to ensure that enough space exists to track
  information about upcoming writes.

  @param  This                 A pointer to the calling context.
  @param  CallerId             The GUID identifying the write.
  @param  PrivateDataSize      The size of the caller's private data  that must be
                               recorded for each write.
  @param  NumberOfWrites       The number of fault tolerant block writes that will
                               need to occur.

  @retval EFI_SUCCESS          The function completed successfully
  @retval EFI_ABORTED          The function could not complete successfully.
  @retval EFI_ACCESS_DENIED    Not all allocated writes have been completed.  All
                               writes must be completed or aborted before another
                               fault tolerant write can occur.

**/
EFI_STATUS
EFIAPI
FtwAllocate(
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL    * This,
  IN EFI_GUID                             * CallerId,
  IN UINTN                                PrivateDataSize,
  IN UINTN                                NumberOfWrites
  )
{
  return EFI_SUCCESS;
}

/**
  Starts a target block update. This records information about the write
  in fault tolerant storage, and will complete the write in a recoverable
  manner, ensuring at all times that either the original contents or
  the modified contents are available.

  @param  This                 The calling context.
  @param  Lba                  The logical block address of the target block.
  @param  Offset               The offset within the target block to place the
                               data.
  @param  Length               The number of bytes to write to the target block.
  @param  PrivateData          A pointer to private data that the caller requires
                               to complete any pending writes in the event of a
                               fault.
  @param  FvBlockHandle        The handle of FVB protocol that provides services
                               for reading, writing, and erasing the target block.
  @param  Buffer               The data to write.

  @retval EFI_SUCCESS          The function completed successfully.
  @retval EFI_ABORTED          The function could not complete successfully.
  @retval EFI_BAD_BUFFER_SIZE  The write would span a block boundary, which is not
                               a valid action.
  @retval EFI_ACCESS_DENIED    No writes have been allocated.
  @retval EFI_NOT_READY        The last write has not been completed. Restart()
                               must be called to complete it.

**/
EFI_STATUS
EFIAPI
FtwWrite(
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL     * This,
  IN EFI_LBA                               Lba,
  IN UINTN                                 Offset,
  IN UINTN                                 Length,
  IN VOID                                  *PrivateData,
  IN EFI_HANDLE                            FvbHandle,
  IN VOID                                  *Buffer
  )
{
  return Private->FvbInstance.Write(&Private->FvbInstance,
                                    Lba,
                                    Offset,
                                    &Length,
                                    Buffer);
}

/**
  Restarts a previously interrupted write. The caller must provide the
  block protocol needed to complete the interrupted write.

  @param  This                 The calling context.
  @param  FvBlockProtocol      The handle of FVB protocol that provides services.
                               for reading, writing, and erasing the target block.

  @retval EFI_SUCCESS          The function completed successfully.
  @retval EFI_ABORTED          The function could not complete successfully.
  @retval EFI_ACCESS_DENIED    No pending writes exist.

**/
EFI_STATUS
EFIAPI
FtwRestart(
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL     * This,
  IN EFI_HANDLE                            FvbHandle
  )
{
  return EFI_SUCCESS;
}

/**
  Aborts all previously allocated writes.

  @param  This                 The calling context.

  @retval EFI_SUCCESS          The function completed successfully.
  @retval EFI_ABORTED          The function could not complete successfully.
  @retval EFI_NOT_FOUND        No allocated writes exist.

**/
EFI_STATUS
EFIAPI
FtwAbort(
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL     * This
  )
{
  return EFI_SUCCESS;
}

/**
  Starts a target block update. This function records information about the write
  in fault-tolerant storage and completes the write in a recoverable
  manner, ensuring at all times that either the original contents or
  the modified contents are available.

  @param  This                 Indicates a pointer to the calling context.
  @param  CallerId             The GUID identifying the last write.
  @param  Lba                  The logical block address of the last write.
  @param  Offset               The offset within the block of the last write.
  @param  Length               The length of the last write.
  @param  PrivateDataSize      On input, the size of the PrivateData buffer. On
                               output, the size of the private data stored for
                               this write.
  @param  PrivateData          A pointer to a buffer. The function will copy
                               PrivateDataSize bytes from the private data stored
                               for this write.
  @param  Complete             A Boolean value with TRUE indicating that the write
                               was completed.

  @retval EFI_SUCCESS          The function completed successfully.
  @retval EFI_ABORTED          The function could not complete successfully.
  @retval EFI_NOT_FOUND        No allocated writes exist.

**/
EFI_STATUS
EFIAPI
FtwGetLastWrite(
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL     * This,
  OUT EFI_GUID                             * CallerId,
  OUT EFI_LBA                              *Lba,
  OUT UINTN                                *Offset,
  OUT UINTN                                *Length,
  IN OUT UINTN                             *PrivateDataSize,
  OUT VOID                                 *PrivateData,
  OUT BOOLEAN                              *Complete
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Initialize the FVB Driver

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

**/
EFI_STATUS
FVBInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS                  Status;
  UINTN                       NumOfHandles;
  EFI_HANDLE                  *HandleBuffer;
  UINT64                      Size;
  UINT32                      BlockSize;
  EFI_DEVICE_PATH_PROTOCOL    *PartitionDevicePath = NULL;
  EFI_DEVICE_PATH_PROTOCOL    *FlashDevicePath = NULL;
  EFI_DEVICE_PATH_PROTOCOL    *CurrentDevicePath = NULL;
  EFI_DEVICE_PATH_PROTOCOL    *NextDevicePath = NULL;
  EFI_PARTITION_INFO_PROTOCOL *PartitionInfo = NULL;
  EFI_HANDLE                  FlashHandle;
  BOOLEAN                     ValidFlash;
  UINTN                       Index;

  Private = NULL;
  Status = gBS->AllocatePool(EfiRuntimeServicesData,
                             sizeof(NVIDIA_FVB_PRIVATE_DATA),
                             (VOID**)&Private);

  if (EFI_ERROR(Status) || (Private == NULL)) {
    return EFI_OUT_OF_RESOURCES;
  }

  gBS->SetMem (Private, sizeof (NVIDIA_FVB_PRIVATE_DATA), 0);

  if (!PcdGetBool(PcdEmuVariableNvModeEnable)) {
    Status = gBS->LocateHandleBuffer (ByProtocol,
                                      &gEfiPartitionInfoProtocolGuid,
                                      NULL,
                                      &NumOfHandles,
                                      &HandleBuffer);

    if (EFI_ERROR(Status)) {
      Status = EFI_UNSUPPORTED;
      goto NoFlashExit;
    }

    for (Index = 0; Index < NumOfHandles; Index++) {
      Status = gBS->OpenProtocol (HandleBuffer[Index],
                                  &gEfiPartitionInfoProtocolGuid,
                                  (VOID **)&PartitionInfo,
                                  ImageHandle,
                                  NULL,
                                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

      if (EFI_ERROR(Status) || (PartitionInfo == NULL)) {
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

      if (0 == StrnCmp (PartitionInfo->Info.Gpt.PartitionName,
                        PcdGetPtr(PcdUEFIVariablesPartitionName),
                        StrnLenS(PcdGetPtr(PcdUEFIVariablesPartitionName), sizeof(PartitionInfo->Info.Gpt.PartitionName)))) {
        break;
      }
    }

    if (Index == NumOfHandles) {
      Status = EFI_NOT_FOUND;
      goto NoFlashExit;
    }

    Private->PartitionStartingLBA = PartitionInfo->Info.Gpt.StartingLBA;
    Private->NumBlocks = PartitionInfo->Info.Gpt.EndingLBA -
                           PartitionInfo->Info.Gpt.StartingLBA + 1;

    Status = gBS->HandleProtocol (HandleBuffer[Index],
                                  &gEfiDevicePathProtocolGuid,
                                  (VOID **)&PartitionDevicePath);

    if (EFI_ERROR(Status) || (PartitionDevicePath == NULL) || IsDevicePathEnd(PartitionDevicePath)) {
      Status = EFI_NOT_FOUND;
      goto NoFlashExit;
    }

    ValidFlash = FALSE;
    CurrentDevicePath = PartitionDevicePath;
    while (IsDevicePathEnd (CurrentDevicePath) == FALSE) {
      if (CurrentDevicePath->SubType == MSG_EMMC_DP) {
        ValidFlash = TRUE;
        break;
      }
      CurrentDevicePath = NextDevicePathNode (CurrentDevicePath);
    }

    if (ValidFlash != TRUE) {
      Status = EFI_UNSUPPORTED;
      goto NoFlashExit;
    }

    FlashDevicePath = DuplicateDevicePath (PartitionDevicePath);

    if (FlashDevicePath == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto NoFlashExit;
    }

    CurrentDevicePath = FlashDevicePath;
    NextDevicePath = NextDevicePathNode (CurrentDevicePath);

    while (IsDevicePathEnd (NextDevicePath) == FALSE) {
      CurrentDevicePath = NextDevicePath;
      NextDevicePath = NextDevicePathNode (NextDevicePath);
    }

    SetDevicePathEndNode(CurrentDevicePath);

    Status = gBS->LocateDevicePath (&gEfiBlockIoProtocolGuid,
                                    &FlashDevicePath,
                                    &FlashHandle);

    if (EFI_ERROR(Status) || (FlashHandle == NULL)) {
      Status = EFI_NOT_FOUND;
      goto NoFlashExit;
    }

    Status = gBS->OpenProtocol (FlashHandle,
                                &gEfiBlockIoProtocolGuid,
                                (VOID **)&Private->BlockIo,
                                ImageHandle,
                                NULL,
                                EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    if (EFI_ERROR(Status) || (Private->BlockIo == NULL)) {
      Status = EFI_NOT_FOUND;
      goto NoFlashExit;
    }

    BlockSize = Private->BlockIo->Media->BlockSize;
    Size = MultU64x32 (Private->NumBlocks, BlockSize);

    if (Size != PcdGet32(PcdFlashNvStorageVariableSize)) {
      DEBUG ((EFI_D_ERROR, "UEFI variables not supported.\n"));
      Status = EFI_UNSUPPORTED;
      goto NoFlashExit;
    }

    Private->VariablePartition = NULL;
    Status = gBS->AllocatePool (EfiRuntimeServicesData,
                                Size,
                                (VOID **)&Private->VariablePartition);

    if (EFI_ERROR(Status) || (Private->VariablePartition == NULL)) {
      Status = EFI_OUT_OF_RESOURCES;
      goto NoFlashExit;
    }

    PcdSet64(PcdFlashNvStorageVariableBase64, (UINT64)Private->VariablePartition);

    Status = Private->BlockIo->ReadBlocks (Private->BlockIo,
                                           Private->BlockIo->Media->MediaId,
                                           Private->PartitionStartingLBA,
                                           Size,
                                           Private->VariablePartition);
    if (EFI_ERROR(Status)) {
      goto NoFlashExit;
    }

    Status = ValidateFvHeader((EFI_FIRMWARE_VOLUME_HEADER *)Private->VariablePartition);

    if (EFI_ERROR(Status)) {
      DEBUG ((EFI_D_INFO, "%a: The FVB Header is not valid.\n", __FUNCTION__));
      DEBUG ((EFI_D_INFO, "%a: Installing a correct one for this volume.\n", __FUNCTION__));

      gBS->SetMem (Private->VariablePartition, Size, 0xFF);

      Status = Private->BlockIo->WriteBlocks (Private->BlockIo,
                                              Private->BlockIo->Media->MediaId,
                                              Private->PartitionStartingLBA,
                                              Size,
                                              Private->VariablePartition);
      if (EFI_ERROR(Status)) {
        goto NoFlashExit;
      }

      // Install all appropriate headers
      Status = InitializeFvAndVariableStoreHeaders ((EFI_FIRMWARE_VOLUME_HEADER *)Private->VariablePartition);
      if (EFI_ERROR(Status)) {
        goto NoFlashExit;
      }
    }
  }

NoFlashExit:
  //
  // If reached here because of an error and not using emukated variables,
  // switch to use emulated variables.
  //
  if (EFI_ERROR(Status) && !PcdGetBool(PcdEmuVariableNvModeEnable)) {
    DEBUG ((EFI_D_ERROR, "%a: FVB Initialization Failed. Switching to Emulated Variables.\n", __FUNCTION__));
    PcdSetBool(PcdEmuVariableNvModeEnable, TRUE);
  }

  //
  // The driver implementing the variable read service can now be dispatched;
  // the varstore headers are in place.
  //
  Status = gBS->InstallProtocolInterface (&gImageHandle,
                                          &gEdkiiNvVarStoreFormattedGuid,
                                          EFI_NATIVE_INTERFACE,
                                          NULL);

  if (!EFI_ERROR(Status) && PcdGetBool(PcdEmuVariableNvModeEnable)) {
    Status = EFI_UNSUPPORTED;
  }

  if (!EFI_ERROR(Status)) {
    //
    // Register for the virtual address change event
    //
    FvbVirtualAddrChangeEvent = (EFI_EVENT)NULL;
    Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                                 TPL_NOTIFY,
                                 FVBVirtualNotifyEvent,
                                 NULL,
                                 &gEfiEventVirtualAddressChangeGuid,
                                 &FvbVirtualAddrChangeEvent);

    if (!EFI_ERROR(Status) && FvbVirtualAddrChangeEvent != NULL) {
      Private->FvbInstance.GetAttributes = FvbGetAttributes;
      Private->FvbInstance.SetAttributes = FvbSetAttributes;
      Private->FvbInstance.GetPhysicalAddress = FvbGetPhysicalAddress;
      Private->FvbInstance.GetBlockSize = FvbGetBlockSize;
      Private->FvbInstance.Read = FvbRead;
      Private->FvbInstance.Write = FvbWrite;
      Private->FvbInstance.EraseBlocks = FvbEraseBlocks;
      Private->FvbInstance.ParentHandle = NULL;

      Private->FtwInstance.GetMaxBlockSize = FtwGetMaxBlockSize;
      Private->FtwInstance.Allocate = FtwAllocate;
      Private->FtwInstance.Write = FtwWrite;
      Private->FtwInstance.Restart = FtwRestart;
      Private->FtwInstance.Abort = FtwAbort;
      Private->FtwInstance.GetLastWrite = FtwGetLastWrite;

      Status = gBS->InstallMultipleProtocolInterfaces(&ImageHandle,
                                                      &gEfiFirmwareVolumeBlockProtocolGuid,
                                                      &Private->FvbInstance,
                                                      &gEfiFaultTolerantWriteProtocolGuid,
                                                      &Private->FtwInstance,
                                                      NULL);
    } else {
      Status = EFI_OUT_OF_RESOURCES;
    }
  }

  if (EFI_ERROR(Status)) {
    if (Private != NULL) {
      if (Private->VariablePartition != NULL) {
        gBS->FreePool(Private->VariablePartition);
      }
      gBS->FreePool(Private);
    }
    if (FlashDevicePath != NULL) {
      gBS->FreePool(FlashDevicePath);
    }
    if (FvbVirtualAddrChangeEvent != NULL) {
      gBS->CloseEvent(FvbVirtualAddrChangeEvent);
    }
  }

  return Status;
}
