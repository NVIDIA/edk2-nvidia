/** @file

  Standalone MM driver Fvb Driver

  SPDX-FileCopyrightText: Copyright (c) 2018 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/MmServicesTableLib.h>
#include <FvbPrivate.h>
#include <Library/PlatformResourceLib.h>
#include <Library/IoLib.h>

/* FVB transactions will only be made to socket 0. */
#define FVB_DEVICE_SOCKET    0
#define VARINT_CHECK_FAILED  L"VarIntCheckFailed"
#define UEFI_VARS_SOCKET     0

STATIC BOOLEAN                  CheckVarStoreIntegrity = FALSE;
STATIC NVIDIA_VAR_INT_PROTOCOL  *VarInt                = NULL;
STATIC UINT64                   VariableOffset;
STATIC UINT64                   VariableSize;
STATIC UINT64                   ReservedPartitionOffset;
STATIC UINT64                   ReservedPartitionSize;
STATIC NOR_FLASH_ATTRIBUTES     NorFlashAttributes;

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
FvbGetAttributes (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  OUT       EFI_FVB_ATTRIBUTES_2                 *Attributes
  )
{
  NVIDIA_FVB_PRIVATE_DATA  *Private;

  if ((This == NULL) || (Attributes == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = BASE_CR (This, NVIDIA_FVB_PRIVATE_DATA, FvbProtocol);
  if (Private->PartitionData != NULL) {
    *Attributes = ((EFI_FIRMWARE_VOLUME_HEADER *)Private->PartitionData)->Attributes;
  } else {
    *Attributes = (EFI_FVB_ATTRIBUTES_2)(
                                         EFI_FVB2_READ_ENABLED_CAP   | // Reads may be enabled
                                         EFI_FVB2_READ_STATUS        | // Reads are currently enabled
                                         EFI_FVB2_STICKY_WRITE       | // A block erase is required to flip bits into EFI_FVB2_ERASE_POLARITY
                                         EFI_FVB2_ERASE_POLARITY     | // After erasure all bits take this value (i.e. '1')
                                         EFI_FVB2_WRITE_STATUS       | // Writes are currently enabled
                                         EFI_FVB2_WRITE_ENABLED_CAP    // Writes may be enabled
                                         );
  }

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
FvbSetAttributes (
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
  NVIDIA_FVB_PRIVATE_DATA  *Private;

  if ((This == NULL) || (Address == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private  = BASE_CR (This, NVIDIA_FVB_PRIVATE_DATA, FvbProtocol);
  *Address = Private->PartitionAddress;
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
  NVIDIA_FVB_PRIVATE_DATA  *Private;

  if ((This == NULL) ||
      (BlockSize == NULL) ||
      (NumberOfBlocks == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private         = BASE_CR (This, NVIDIA_FVB_PRIVATE_DATA, FvbProtocol);
  *BlockSize      = Private->FlashAttributes.BlockSize;
  *NumberOfBlocks = Private->PartitionSize / Private->FlashAttributes.BlockSize;

  return EFI_SUCCESS;
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
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  IN        EFI_LBA                              Lba,
  IN        UINTN                                Offset,
  IN OUT    UINTN                                *NumBytes,
  IN OUT    UINT8                                *Buffer
  )
{
  EFI_STATUS               Status;
  UINT32                   BlockSize;
  UINT64                   FvbOffset;
  EFI_LBA                  LastBlock;
  BOOLEAN                  LbaBoundaryCrossed;
  NVIDIA_FVB_PRIVATE_DATA  *Private;

  if ((This == NULL) ||
      (NumBytes == NULL) ||
      (Buffer == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  if ((Offset > (MAX_UINT64 - *NumBytes)) ||
      (*NumBytes > (MAX_UINT64 - Offset)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private   = BASE_CR (This, NVIDIA_FVB_PRIVATE_DATA, FvbProtocol);
  BlockSize = Private->FlashAttributes.BlockSize;
  LastBlock = (Private->PartitionSize / Private->FlashAttributes.BlockSize) - 1;

  // The read must not span FV boundaries.
  if (Lba > LastBlock) {
    *NumBytes = 0;
    return EFI_BAD_BUFFER_SIZE;
  }

  // We must have some bytes to read
  if (*NumBytes == 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // The read must not span block boundaries.
  // We need to check each variable individually because adding two large values together overflows.
  if (Offset >= BlockSize) {
    *NumBytes = 0;
    return EFI_BAD_BUFFER_SIZE;
  }

  LbaBoundaryCrossed = FALSE;
  if ((Offset + *NumBytes) >  BlockSize) {
    *NumBytes          = BlockSize - Offset;
    LbaBoundaryCrossed = TRUE;
  }

  FvbOffset = MultU64x32 (Lba, BlockSize) + Offset;

  if (Private->PartitionData != NULL) {
    CopyMem (Buffer, Private->PartitionData + FvbOffset, *NumBytes);
    Status = EFI_SUCCESS;
  } else {
    // Update storage
    Status = Private->NorFlashProtocol->Read (
                                          Private->NorFlashProtocol,
                                          FvbOffset + Private->PartitionOffset,
                                          *NumBytes,
                                          Buffer
                                          );
  }

  return LbaBoundaryCrossed ? EFI_BAD_BUFFER_SIZE : Status;
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
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  IN        EFI_LBA                              Lba,
  IN        UINTN                                Offset,
  IN OUT    UINTN                                *NumBytes,
  IN        UINT8                                *Buffer
  )
{
  EFI_STATUS               Status;
  UINT32                   BlockSize;
  UINT64                   FvbOffset;
  EFI_LBA                  LastBlock;
  BOOLEAN                  LbaBoundaryCrossed;
  NVIDIA_FVB_PRIVATE_DATA  *Private;

  if ((This == NULL) ||
      (NumBytes == NULL) ||
      (Buffer == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  if ((Offset > (MAX_UINT64 - *NumBytes)) ||
      (*NumBytes > (MAX_UINT64 - Offset)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private   = BASE_CR (This, NVIDIA_FVB_PRIVATE_DATA, FvbProtocol);
  BlockSize = Private->FlashAttributes.BlockSize;
  LastBlock = (Private->PartitionSize / Private->FlashAttributes.BlockSize) - 1;
  if (CheckVarStoreIntegrity == TRUE) {
    if (VarInt->CurMeasurement[0] == FVB_ERASED_BYTE) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Writing Measurement 0x%x\n",
        __FUNCTION__,
        VarInt->CurMeasurement[0]
        ));
      VarInt->WriteNewMeasurement (VarInt);
    }
  }

  // The write must not span FV boundaries.
  if (Lba > LastBlock) {
    *NumBytes = 0;
    return EFI_BAD_BUFFER_SIZE;
  }

  // We must have some bytes to write
  if (*NumBytes == 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // The write must not span block boundaries.
  // We need to check each variable individually because adding two large values together overflows.
  if (Offset >= BlockSize) {
    *NumBytes = 0;
    return EFI_BAD_BUFFER_SIZE;
  }

  LbaBoundaryCrossed = FALSE;
  if ((Offset + *NumBytes) >  BlockSize) {
    *NumBytes          = BlockSize - Offset;
    LbaBoundaryCrossed = TRUE;
  }

  // Modify FVB
  FvbOffset = MultU64x32 (Lba, BlockSize) + Offset;
  if (Private->PartitionData != NULL) {
    CopyMem (Private->PartitionData + FvbOffset, Buffer, *NumBytes);
  }

  // Update storage
  Status = Private->NorFlashProtocol->Write (
                                        Private->NorFlashProtocol,
                                        FvbOffset + Private->PartitionOffset,
                                        *NumBytes,
                                        Buffer
                                        );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FVB write failed. Recovered FVB could be corrupt.\n", __FUNCTION__));
    ASSERT (FALSE);
    if (Private->PartitionData != NULL) {
      Private->NorFlashProtocol->Read (
                                   Private->NorFlashProtocol,
                                   FvbOffset + Private->PartitionOffset,
                                   *NumBytes,
                                   Private->PartitionData + FvbOffset
                                   );
    }

    Status = EFI_DEVICE_ERROR;
  }

  return (!EFI_ERROR (Status) && LbaBoundaryCrossed) ? EFI_BAD_BUFFER_SIZE : Status;
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
  IN CONST EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  ...
  )
{
  EFI_STATUS               Status;
  VA_LIST                  Args;
  EFI_LBA                  StartingLba;
  UINTN                    NumOfLba;
  UINT32                   BlockSize;
  EFI_LBA                  LastBlock;
  UINT64                   FvbOffset;
  UINT64                   FvbBufferSize;
  NVIDIA_FVB_PRIVATE_DATA  *Private;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = BASE_CR (This, NVIDIA_FVB_PRIVATE_DATA, FvbProtocol);

  // If no blocks are passed in return should be invalid parameter.
  Status = EFI_INVALID_PARAMETER;

  BlockSize = Private->FlashAttributes.BlockSize;
  LastBlock = (Private->PartitionSize / Private->FlashAttributes.BlockSize) - 1;

  // Before erasing, check the entire list of parameters to ensure all specified blocks are valid
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

    // All blocks must be within range
    if ((NumOfLba == 0) ||
        (StartingLba > (MAX_UINT64 - NumOfLba)) ||
        (NumOfLba > (MAX_UINT64 - StartingLba)) ||
        ((StartingLba + NumOfLba - 1) > LastBlock))
    {
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

    // Modify FVB
    FvbOffset     = MultU64x32 (StartingLba, BlockSize);
    FvbBufferSize = MultU64x32 (NumOfLba, BlockSize);
    if (Private->PartitionData != NULL) {
      SetMem (Private->PartitionData + FvbOffset, FvbBufferSize, FVB_ERASED_BYTE);
    }

    Status = Private->NorFlashProtocol->Erase (
                                          Private->NorFlashProtocol,
                                          (FvbOffset + Private->PartitionOffset) / BlockSize,
                                          NumOfLba
                                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: FVB write failed. Recovered FVB could be corrupt.\n", __FUNCTION__));
      ASSERT (FALSE);
      if (Private->PartitionData != NULL) {
        Private->NorFlashProtocol->Read (
                                     Private->NorFlashProtocol,
                                     FvbOffset + Private->PartitionOffset,
                                     FvbBufferSize,
                                     Private->PartitionData + FvbOffset
                                     );
      }

      Status = EFI_DEVICE_ERROR;
      break;
    }
  } while (!EFI_ERROR (Status));

  VA_END (Args);

  return Status;
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
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  NVIDIA_FVB_PRIVATE_DATA  *Private;

  Private = (NVIDIA_FVB_PRIVATE_DATA *)Context;
  EfiConvertPointer (0x0, (VOID **)&Private->NorFlashProtocol->Erase);
  EfiConvertPointer (0x0, (VOID **)&Private->NorFlashProtocol->GetAttributes);
  EfiConvertPointer (0x0, (VOID **)&Private->NorFlashProtocol->Read);
  EfiConvertPointer (0x0, (VOID **)&Private->NorFlashProtocol->Write);
  EfiConvertPointer (0x0, (VOID **)&Private->NorFlashProtocol);
  if (Private->PartitionData != NULL) {
    EfiConvertPointer (0x0, (VOID **)&Private->PartitionData);
    EfiConvertPointer (0x0, (VOID **)&Private->PartitionAddress);
  }

  EfiConvertPointer (0x0, (VOID **)&Private);
  return;
}

/**

  Check whether a flash buffer is erased.

  @param Buffer          Buffer to check
  @param BufferSize      Size of the buffer

  @return A BOOLEAN value indicating erased or not.

**/
BOOLEAN
IsErasedFlashBuffer (
  IN UINT8  *Buffer,
  IN UINTN  BufferSize
  )
{
  BOOLEAN  IsEmpty;
  UINT8    *Ptr;
  UINTN    Index;

  Ptr     = Buffer;
  IsEmpty = TRUE;
  for (Index = 0; Index < BufferSize; Index += 1) {
    if (*Ptr++ != FVB_ERASED_BYTE) {
      IsEmpty = FALSE;
      break;
    }
  }

  return IsEmpty;
}

/**
  Initializes the FV Header and Variable Store Header
  to support variable operations.

  @param FirmwareVolumeHeader - A pointer to a firmware volume header
  @param PartitionOffset      - Offset of the variable partition
  @param PartitionSize        - Size of the partition
  @param CheckVariableStore   - TRUE if the variable data should be checked
  @param NorFlashProtocol     - Pointer to nor flash protocol
  @param FlashAttributes      - Pointer to flash attributes for the nor flash partition is on

**/
EFI_STATUS
InitializeFvAndVariableStoreHeaders (
  IN EFI_FIRMWARE_VOLUME_HEADER  *FirmwareVolumeHeader,
  IN UINT64                      PartitionOffset,
  IN UINT64                      PartitionSize,
  IN BOOLEAN                     CheckVariableStore,
  IN NVIDIA_NOR_FLASH_PROTOCOL   *NorFlashProtocol,
  IN NOR_FLASH_ATTRIBUTES        *FlashAttributes
  )
{
  EFI_STATUS             Status;
  VARIABLE_STORE_HEADER  *VariableStoreHeader;

  DEBUG ((DEBUG_ERROR, "%a Address 0x%x\n", __FUNCTION__, (UINTN)FirmwareVolumeHeader));
  if (FirmwareVolumeHeader == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Check if the size of the area is at least one block size
  if (PartitionSize <= 0) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (!IsErasedFlashBuffer ((UINT8 *)FirmwareVolumeHeader, PartitionSize)) {
    Status = NorFlashProtocol->Erase (
                                 NorFlashProtocol,
                                 PartitionOffset / FlashAttributes->BlockSize,
                                 PartitionSize / FlashAttributes->BlockSize
                                 );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to Erase Partition\r\n", __FUNCTION__));
      return Status;
    }

    NorFlashProtocol->Read (
                        NorFlashProtocol,
                        PartitionOffset,
                        PartitionSize,
                        FirmwareVolumeHeader
                        );
    ASSERT (IsErasedFlashBuffer ((UINT8 *)FirmwareVolumeHeader, PartitionSize));
  }

  //
  // EFI_FIRMWARE_VOLUME_HEADER
  //
  ZeroMem (
    FirmwareVolumeHeader,
    sizeof (EFI_FIRMWARE_VOLUME_HEADER) + sizeof (EFI_FV_BLOCK_MAP_ENTRY)
    );
  CopyGuid (&FirmwareVolumeHeader->FileSystemGuid, &gEfiSystemNvDataFvGuid);
  FirmwareVolumeHeader->FvLength   = PartitionSize;
  FirmwareVolumeHeader->Signature  = EFI_FVH_SIGNATURE;
  FirmwareVolumeHeader->Attributes = (EFI_FVB_ATTRIBUTES_2)(
                                                            EFI_FVB2_READ_ENABLED_CAP   | // Reads may be enabled
                                                            EFI_FVB2_READ_STATUS        | // Reads are currently enabled
                                                            EFI_FVB2_STICKY_WRITE       | // A block erase is required to flip bits into EFI_FVB2_ERASE_POLARITY
                                                            EFI_FVB2_MEMORY_MAPPED      | // It is memory mapped
                                                            EFI_FVB2_ERASE_POLARITY     | // After erasure all bits take this value (i.e. '1')
                                                            EFI_FVB2_WRITE_STATUS       | // Writes are currently enabled
                                                            EFI_FVB2_WRITE_ENABLED_CAP    // Writes may be enabled
                                                            );
  FirmwareVolumeHeader->HeaderLength          = sizeof (EFI_FIRMWARE_VOLUME_HEADER) + sizeof (EFI_FV_BLOCK_MAP_ENTRY);
  FirmwareVolumeHeader->Revision              = EFI_FVH_REVISION;
  FirmwareVolumeHeader->BlockMap[0].NumBlocks = PartitionSize / FlashAttributes->BlockSize;
  FirmwareVolumeHeader->BlockMap[0].Length    = FlashAttributes->BlockSize;
  FirmwareVolumeHeader->BlockMap[1].NumBlocks = 0;
  FirmwareVolumeHeader->BlockMap[1].Length    = 0;
  FirmwareVolumeHeader->Checksum              = CalculateCheckSum16 ((UINT16 *)FirmwareVolumeHeader, FirmwareVolumeHeader->HeaderLength);

  Status = NorFlashProtocol->Write (
                               NorFlashProtocol,
                               PartitionOffset,
                               FirmwareVolumeHeader->HeaderLength,
                               FirmwareVolumeHeader
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to Write Partition header\r\n", __FUNCTION__));
    return Status;
  }

  if (CheckVariableStore) {
    //
    // VARIABLE_STORE_HEADER
    //
    VariableStoreHeader = (VARIABLE_STORE_HEADER *)((UINTN)FirmwareVolumeHeader + FirmwareVolumeHeader->HeaderLength);
    ZeroMem (
      VariableStoreHeader,
      sizeof (VARIABLE_STORE_HEADER)
      );
    CopyGuid (&VariableStoreHeader->Signature, &gEfiAuthenticatedVariableGuid);
    VariableStoreHeader->Size   = PcdGet32 (PcdFlashNvStorageVariableSize) - FirmwareVolumeHeader->HeaderLength;
    VariableStoreHeader->Format = VARIABLE_STORE_FORMATTED;
    VariableStoreHeader->State  = VARIABLE_STORE_HEALTHY;

    // Write the combined super-header in the flash
    Status = NorFlashProtocol->Write (
                                 NorFlashProtocol,
                                 PartitionOffset + FirmwareVolumeHeader->HeaderLength,
                                 sizeof (VARIABLE_STORE_HEADER),
                                 VariableStoreHeader
                                 );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to Write variable header\r\n", __FUNCTION__));
    }
  }

  return Status;
}

/**
 * EraseMeasurementPartition
 * Erase the "Reserved" Partition that contain the measurements.
 *
 * @param[in] NorFlasProto    Pointer to the NOR Flash Protocol for the socket.
 * @param[in] PartitionOffset Byte offset of the Measurement partition.
 * @param[in] PartitionSize   Size of the Measurement Partition.
 *
 * @return EFI_SUCCESS  Successfully erased the Measurement partition.
 *         Other        Erase Failed.
 *
 **/
STATIC
EFI_STATUS
EraseMeasurementPartition (
  IN NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProto,
  IN UINT64                     PartitionOffset,
  IN UINT64                     PartitionSize
  )
{
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlash;
  EFI_STATUS                 Status;

  NorFlash = NorFlashProto;
  if (NorFlashProto == NULL) {
    NorFlash = GetSocketNorFlashProtocol (UEFI_VARS_SOCKET);
    if (NorFlash == NULL) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get NorFlashProtocol for Socket 0\n",
        __FUNCTION__
        ));
      Status = EFI_UNSUPPORTED;
      return Status;
    }
  }

  Status = NorFlash->Erase (
                       NorFlash,
                       PartitionOffset / NorFlashAttributes.BlockSize,
                       PartitionSize / NorFlashAttributes.BlockSize
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Re-init Measurement Partition %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * MmFvbSmmVarReady
 * Callback function when the SmmVariable protocol is installed.
 * This notify function is only installed if the VarStore Integrity feature
 * is enabled, so when called check the validity of of the stored measurement.
 *
 * @param Protocol   Protocol for which the notify is installed
 * @param Interface  Interface that is passed down when the callback is instaleld
 * @param Handle     Handle on which this notify is called.
 *
 * @return EFI_SUCCESS Succesful validation of the VarStore Measurement.
 *         Other       Assert as this means the VarStore could be tampered.
 *
 */
STATIC
EFI_STATUS
EFIAPI
MmFvbSmmVarReady (
  IN CONST EFI_GUID  *Protocol,
  IN VOID            *Interface,
  IN EFI_HANDLE      Handle
  )
{
  EFI_STATUS                 Status;
  EFI_SMM_VARIABLE_PROTOCOL  *SmmVariable;
  UINT32                     VarIntCheckFail;

  Status = gMmst->MmLocateProtocol (
                    &gNVIDIAVarIntGuid,
                    NULL,
                    (VOID **)&VarInt
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get VarInt Proto%r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status          = VarInt->Validate (VarInt);
  VarIntCheckFail = 0;
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Var Store Validation failed %r",
      __FUNCTION__,
      Status
      ));

    VarIntCheckFail = 1;

    /* We're here, which means there is a non-erased Variable Integrity space
     * that isn't matching our expected measurement.
     * The default behavior if we fail to validate the integrity of the Variable Store
     * is to assert.
     * But Users can choose to not assert (via the PCD item), if they do, then the driver
     * will corrupt the FV header and set a volatile variable that will signal the NS UEFI
     * to reboot the system. The subsequent reboot should re-initialize the Variable Store.
     */
    if (FeaturePcdGet (PcdAssertOnVarStoreIntegrityCheckFail) == TRUE) {
      ASSERT (FALSE);
    } else {
      Status = gMmst->MmLocateProtocol (&gEfiSmmVariableProtocolGuid, NULL, (VOID **)&SmmVariable);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: gEfiSmmVariableProtocolGuid: NOT LOCATED!\n", __FUNCTION__));
      }

      ASSERT_EFI_ERROR (Status);

      // Set a Volatile Variable that NS side will check in UEFI.
      Status = SmmVariable->SmmSetVariable (
                              VARINT_CHECK_FAILED,
                              &gEfiGlobalVariableGuid,
                              EFI_VARIABLE_BOOTSERVICE_ACCESS,
                              sizeof (UINT32),
                              &VarIntCheckFail
                              );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to Set %s Variable %r\n",
          __FUNCTION__,
          VARINT_CHECK_FAILED,
          Status
          ));
      }

      ASSERT_EFI_ERROR (Status);

      // Corrupt the FV header which forces a re-init of the Variable store during the next reboot.
      DEBUG ((DEBUG_ERROR, "%a: Corrupting FV Header\n", __FUNCTION__));
      Status = CorruptFvHeader (VariableOffset, VariableSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to Corrupt FV Header %r\n",
          __FUNCTION__,
          Status
          ));
        return Status;
      }

      // Erase the Measeurement Partition.
      Status = EraseMeasurementPartition (NULL, ReservedPartitionOffset, ReservedPartitionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to Erase Partition %r\n",
          __FUNCTION__,
          Status
          ));
        return Status;
      }
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a: VarStore validation Succesful\n", __FUNCTION__));
  }

  return EFI_SUCCESS;
}

/**
  Check the integrity of firmware volume header.

  @param PartitionData      - A pointer to the partition data
  @param PartitionOffset      - Offset of the variable partition
  @param PartitionSize        - Size of the partition
  @param CheckVariableStore   - TRUE if the variable data should be checked
  @param NorFlashProtocol     - Pointer to nor flash protocol
  @param FlashAttributes      - Pointer to flash attributes for the nor flash partition is on

**/
EFI_STATUS
ValidateFvHeader (
  IN VOID                       *PartitionData,
  IN UINT64                     PartitionOffset,
  IN UINT64                     PartitionSize,
  IN BOOLEAN                    CheckVariableStore,
  IN NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  IN NOR_FLASH_ATTRIBUTES       *FlashAttributes,
  IN UINT64                     MeasurementOffset,
  IN UINT64                     MeasurementPartitionSize
  )
{
  EFI_STATUS                  Status;
  EFI_FIRMWARE_VOLUME_HEADER  *FwVolHeader;
  UINT16                      Checksum;
  VARIABLE_STORE_HEADER       *VariableStoreHeader;
  UINTN                       VariableStoreLength;
  UINT64                      OriginalLength;

  FwVolHeader = (EFI_FIRMWARE_VOLUME_HEADER *)PartitionData;

  //
  // Verify the header revision, header signature, length
  // Length of FvBlock cannot be 2**64-1
  // HeaderLength cannot be an odd number
  //
  if ((FwVolHeader->Revision  != EFI_FVH_REVISION)  ||
      (FwVolHeader->Signature != EFI_FVH_SIGNATURE) ||
      (FwVolHeader->FvLength  > PartitionSize))
  {
    DEBUG ((DEBUG_INFO, "%a: No Firmware Volume header present\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  // Check the Firmware Volume Guid
  if ( CompareGuid (&FwVolHeader->FileSystemGuid, &gEfiSystemNvDataFvGuid) == FALSE ) {
    DEBUG ((DEBUG_INFO, "%a: Firmware Volume Guid non-compatible\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  // Verify the header checksum
  Checksum = CalculateSum16 ((UINT16 *)FwVolHeader, FwVolHeader->HeaderLength);
  if (Checksum != 0) {
    DEBUG ((DEBUG_INFO, "%a: FV checksum is invalid (Checksum:0x%X)\n", __FUNCTION__, Checksum));
    return EFI_NOT_FOUND;
  }

  if (CheckVariableStore) {
    VariableStoreHeader = (VARIABLE_STORE_HEADER *)((UINTN)FwVolHeader + FwVolHeader->HeaderLength);

    // Check the Variable Store Guid
    if (!CompareGuid (&VariableStoreHeader->Signature, &gEfiVariableGuid) &&
        !CompareGuid (&VariableStoreHeader->Signature, &gEfiAuthenticatedVariableGuid))
    {
      DEBUG ((DEBUG_INFO, "%a: Variable Store Guid non-compatible\n", __FUNCTION__));
      return EFI_NOT_FOUND;
    }

    VariableStoreLength = FwVolHeader->FvLength - FwVolHeader->HeaderLength;

    if (VariableStoreHeader->Size != VariableStoreLength) {
      DEBUG ((DEBUG_INFO, "%a: Variable Store Length does not match\n", __FUNCTION__));
      return EFI_NOT_FOUND;
    }

    // The FV header of the variable store is validated. If we've enabled VarStoreIntegrity feature
    // then check if the partition is erased. If it is erased, then re-initialize the varstore, as it
    // could be a possible tamper.
    if (CheckVarStoreIntegrity == TRUE) {
      if (IsMeasurementPartitionErasedOrZero (NorFlashProtocol, MeasurementOffset, MeasurementPartitionSize) == TRUE) {
        DEBUG ((DEBUG_ERROR, "%a: No Valid Measurements found. Re-initializing the Variable Store\n", __FUNCTION__));
        Status = EraseMeasurementPartition (NorFlashProtocol, MeasurementOffset, MeasurementPartitionSize);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Failed to Erase Partition %r\n",
            __FUNCTION__,
            Status
            ));
          return Status;
        }
      }
    }
  }

  // Resize if everything looks good except the size
  if ((FwVolHeader->FvLength != PartitionSize) ||
      (FwVolHeader->BlockMap[0].Length != FlashAttributes->BlockSize))
  {
    OriginalLength        = FwVolHeader->FvLength;
    FwVolHeader->FvLength = PartitionSize;
    if (CheckVariableStore) {
      VariableStoreHeader->Size = FwVolHeader->FvLength - FwVolHeader->HeaderLength;
    }

    FwVolHeader->BlockMap[0].NumBlocks = PartitionSize / FlashAttributes->BlockSize;
    FwVolHeader->BlockMap[0].Length    = FlashAttributes->BlockSize;
    FwVolHeader->BlockMap[1].NumBlocks = 0;
    FwVolHeader->BlockMap[1].Length    = 0;

    FwVolHeader->Checksum = 0;
    FwVolHeader->Checksum = CalculateCheckSum16 ((UINT16 *)FwVolHeader, FwVolHeader->HeaderLength);

    Status = NorFlashProtocol->Erase (
                                 NorFlashProtocol,
                                 PartitionOffset / FlashAttributes->BlockSize,
                                 PartitionSize / FlashAttributes->BlockSize
                                 );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to Erase Partition\r\n", __FUNCTION__));
      return Status;
    }

    NorFlashProtocol->Write (
                        NorFlashProtocol,
                        PartitionOffset,
                        OriginalLength,
                        PartitionData
                        );
  }

  return EFI_SUCCESS;
}

/**
  Initialize a work space header.

  Since Signature and WriteQueueSize have been known, Crc can be calculated out,
  then the work space header will be fixed.
**/
VOID
InitializeWorkSpaceHeader (
  IN UINT64                     PartitionOffset,
  IN UINT64                     PartitionSize,
  IN NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  IN NOR_FLASH_ATTRIBUTES       *FlashAttributes
  )
{
  EFI_STATUS                               Status;
  EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER  WorkingBlockHeader;

  Status = NorFlashProtocol->Read (
                               NorFlashProtocol,
                               PartitionOffset,
                               sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER),
                               &WorkingBlockHeader
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read the working area\r\n", __FUNCTION__));
    return;
  }

  //
  // Check signature with gEdkiiWorkingBlockSignatureGuid.
  //
  if (CompareGuid (&gEdkiiWorkingBlockSignatureGuid, &WorkingBlockHeader.Signature)) {
    //
    // The work space header has been initialized.
    //
    return;
  }

  if (!IsErasedFlashBuffer ((UINT8 *)&WorkingBlockHeader, sizeof (WorkingBlockHeader))) {
    Status = NorFlashProtocol->Erase (
                                 NorFlashProtocol,
                                 PartitionOffset / FlashAttributes->BlockSize,
                                 PartitionSize / FlashAttributes->BlockSize
                                 );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to erase working block\r\n", __FUNCTION__));
    }
  }

  SetMem (
    &WorkingBlockHeader,
    sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER),
    FVB_ERASED_BYTE
    );

  //
  // Here using gEdkiiWorkingBlockSignatureid as the signature.
  //
  CopyMem (
    &WorkingBlockHeader.Signature,
    &gEdkiiWorkingBlockSignatureGuid,
    sizeof (EFI_GUID)
    );
  WorkingBlockHeader.WriteQueueSize = PartitionSize - sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER);

  //
  // Crc is calculated with all the fields except Crc and STATE, so leave them as FTW_ERASED_BYTE.
  //

  //
  // Calculate the Crc of working block header
  //
  WorkingBlockHeader.Crc                 = CalculateCrc32 (&WorkingBlockHeader, sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER));
  WorkingBlockHeader.WorkingBlockValid   = FTW_VALID_STATE;
  WorkingBlockHeader.WorkingBlockInvalid = FTW_INVALID_STATE;

  Status = NorFlashProtocol->Write (
                               NorFlashProtocol,
                               PartitionOffset,
                               sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER),
                               &WorkingBlockHeader
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to write the working area\r\n", __FUNCTION__));
    return;
  }
}

/**
 * Validate that the Variable and FTW partition sizes are valid.
 **/
STATIC
EFI_STATUS
ValidatePartitionInfo (
  IN NOR_FLASH_ATTRIBUTES  *NorFlashAttributes,
  IN UINT64                VariableOffset,
  IN UINT64                VariableSize,
  IN UINT64                FtwOffset,
  IN UINT64                FtwSize
  )
{
  if ((VariableSize == 0) ||
      ((VariableSize % NorFlashAttributes->BlockSize) != 0) ||
      (FtwSize == 0) ||
      ((FtwSize % NorFlashAttributes->BlockSize) != 0))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Invalid Partition Size.Var(%lu) FTW(%lu) Block %u\n",
      __FUNCTION__,
      VariableSize,
      FtwSize,
      NorFlashAttributes->BlockSize
      ));
    return EFI_DEVICE_ERROR;
  }

  if ((VariableOffset == 0) && (FtwOffset == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid Variable and FTW Offset (0)\n",
      __FUNCTION__
      ));
    return EFI_DEVICE_ERROR;
  }

  if (FtwSize < (VariableSize + (VariableSize >> 1))) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:FTW(%lu) should be atleast 1.5x Variable(%lu)\n",
      __FUNCTION__,
      FtwSize,
      VariableSize
      ));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Get the Variable and FTW partition offset and size from the BootLoader Params in
  configs where GPT table isn't available.
**/
STATIC
EFI_STATUS
FvbInitializeNoGpt (
  IN  NOR_FLASH_ATTRIBUTES  *NorFlashAttributes,
  OUT UINT64                *VariableOffset,
  OUT UINT64                *VariableSize,
  OUT UINT64                *FtwOffset,
  OUT UINT64                *FtwSize
  )
{
  EFI_STATUS            Status;
  UINT16                DeviceInstance;
  UINT64                VOffset;
  UINT64                VSize;
  UINT64                FOffset;
  UINT64                FSize;
  EFI_PHYSICAL_ADDRESS  CpuBlParamsAddr;

  *VariableOffset = 0;
  *VariableSize   = 0;
  *FtwOffset      = 0;
  *FtwSize        = 0;

  Status = GetCpuBlParamsAddrStMm (&CpuBlParamsAddr);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get CPU BL Addr %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status = GetPartitionInfoStMm (
             (UINTN)CpuBlParamsAddr,
             TEGRABL_VARIABLE_IMAGE_INDEX,
             &DeviceInstance,
             &VOffset,
             &VSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Variable partition Info %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  DeviceInstance = 0;
  Status         = GetPartitionInfoStMm (
                     (UINTN)CpuBlParamsAddr,
                     TEGRABL_FTW_IMAGE_INDEX,
                     &DeviceInstance,
                     &FOffset,
                     &FSize
                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get FTW partition Info %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  *VariableOffset = VOffset;
  *VariableSize   = VSize;
  *FtwOffset      = FOffset;
  *FtwSize        = FSize;

  Status = ValidatePartitionInfo (
             NorFlashAttributes,
             *VariableOffset,
             *VariableSize,
             *FtwOffset,
             *FtwSize
             );
  return Status;
}

/**
  Get the Variable and FTW Partition offset and sizes from the GPT table. This
  should be the typical call flow in most platforms.
**/
STATIC
EFI_STATUS
FvbInitializeGpt (
  IN  NOR_FLASH_ATTRIBUTES       *NorFlashAttributes,
  IN  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  OUT UINT64                     *VariableOffset,
  OUT UINT64                     *VariableSize,
  OUT UINT64                     *FtwOffset,
  OUT UINT64                     *FtwSize,
  OUT UINT64                     *ResvdPartitionOffset,
  OUT UINT64                     *ResvdPartitionSize
  )
{
  EFI_STATUS                  Status;
  EFI_PARTITION_TABLE_HEADER  PartitionHeader;
  VOID                        *PartitionEntryArray;
  CONST EFI_PARTITION_ENTRY   *PartitionEntry;
  UINTN                       GptHeaderOffset;

  GptHeaderOffset = GptGetHeaderOffset (
                      StmmGetBootChainForGpt (),
                      NorFlashAttributes->MemoryDensity,
                      NorFlashAttributes->BlockSize
                      );

  Status = NorFlashProtocol->Read (
                               NorFlashProtocol,
                               GptHeaderOffset,
                               sizeof (PartitionHeader),
                               &PartitionHeader
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read GPT partition table (%r)\r\n", __FUNCTION__, Status));
    return Status;
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

  Status = NorFlashProtocol->Read (
                               NorFlashProtocol,
                               PartitionHeader.PartitionEntryLBA * GPT_PARTITION_BLOCK_SIZE,
                               GptPartitionTableSizeInBytes (&PartitionHeader),
                               PartitionEntryArray
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read GPT partition array (%r)\r\n", __FUNCTION__, Status));
    goto ExitInitializeGpt;
  }

  Status = GptValidatePartitionTable (&PartitionHeader, PartitionEntryArray);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Invalid PartitionEntryArray\r\n"));
    goto ExitInitializeGpt;
  }

  *VariableOffset = 0;
  *VariableSize   = 0;
  *FtwOffset      = 0;
  *FtwSize        = 0;
  // Find variable and FTW partitions
  PartitionEntry = GptFindPartitionByName (
                     &PartitionHeader,
                     PartitionEntryArray,
                     UEFI_VARIABLE_PARTITION_NAME
                     );
  if (PartitionEntry != NULL) {
    *VariableOffset = PartitionEntry->StartingLBA * GPT_PARTITION_BLOCK_SIZE;
    *VariableSize   = GptPartitionSizeInBlocks (PartitionEntry) * GPT_PARTITION_BLOCK_SIZE;
  }

  PartitionEntry = GptFindPartitionByName (
                     &PartitionHeader,
                     PartitionEntryArray,
                     FTW_PARTITION_NAME
                     );
  if (PartitionEntry != NULL) {
    *FtwOffset = PartitionEntry->StartingLBA * GPT_PARTITION_BLOCK_SIZE;
    *FtwSize   = GptPartitionSizeInBlocks (PartitionEntry) * GPT_PARTITION_BLOCK_SIZE;
  }

  if (FeaturePcdGet (PcdVarStoreIntegritySupported) == TRUE) {
    *ResvdPartitionOffset = 0;
    *ResvdPartitionSize   = 0;
    PartitionEntry        = GptFindPartitionByName (
                              &PartitionHeader,
                              PartitionEntryArray,
                              RESERVED_PARTITION_NAME
                              );
    if (PartitionEntry != NULL) {
      *ResvdPartitionOffset = PartitionEntry->StartingLBA * GPT_PARTITION_BLOCK_SIZE;
      *ResvdPartitionSize   = GptPartitionSizeInBlocks (PartitionEntry) * GPT_PARTITION_BLOCK_SIZE;
    }
  }

  Status = ValidatePartitionInfo (
             NorFlashAttributes,
             *VariableOffset,
             *VariableSize,
             *FtwOffset,
             *FtwSize
             );
ExitInitializeGpt:
  FreePool (PartitionEntryArray);
  return Status;
}

/**
  Initialize the FVB Driver

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

**/
EFI_STATUS
EFIAPI
FVBNORInitialize (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS                 Status;
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;
  UINTN                      Index;
  UINT64                     FtwOffset;
  UINT64                     FtwSize;
  NVIDIA_FVB_PRIVATE_DATA    *FvpData;
  VOID                       *VarStoreBuffer;
  VOID                       *FtwSpareBuffer;
  VOID                       *FtwWorkingBuffer;
  VOID                       *MmFvbRegistration;

  if (PcdGetBool (PcdEmuVariableNvModeEnable)) {
    return EFI_SUCCESS;
  }

  if (!IsQspi0Present (NULL)) {
    return EFI_SUCCESS;
  }

  NorFlashProtocol = GetSocketNorFlashProtocol (FVB_DEVICE_SOCKET);
  if (NorFlashProtocol == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:%d Failed to get Socket0 NORFlash proto\n",
      __FUNCTION__,
      __LINE__
      ));
    return EFI_SUCCESS;
  }

  Status = NorFlashProtocol->GetAttributes (NorFlashProtocol, &NorFlashAttributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get NOR Flash attributes (%r)\r\n", __FUNCTION__, Status));
    return EFI_SUCCESS;
  }

  /* Assume that the Variable store part has a GPT */
  Status = FvbInitializeGpt (
             &NorFlashAttributes,
             NorFlashProtocol,
             &VariableOffset,
             &VariableSize,
             &FtwOffset,
             &FtwSize,
             &ReservedPartitionOffset,
             &ReservedPartitionSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Couldn't get Partition info from Gpt(%r), Trying CPUBL\r\n",
      __FUNCTION__,
      Status
      ));
    /* Try to look up the CPU BL Params for the partition Info. */
    Status = FvbInitializeNoGpt (
               &NorFlashAttributes,
               &VariableOffset,
               &VariableSize,
               &FtwOffset,
               &FtwSize
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a:Failed to get Partition info from CPUBL(%r) \n",
        __FUNCTION__,
        Status
        ));

      /*
       * If we fail here, then return a success to the MM dispatcher to
       * a. Keep the dispatcher from crashing. (known issue with failed drivers).
       * b. There could be cases where we dynamically fallback to emulated store;
       *    this is unique to MM since we don't do dynamic PCDs or DTB overlays.
       **/
      return EFI_SUCCESS;
    }
  }

  CheckVarStoreIntegrity = FeaturePcdGet (PcdVarStoreIntegritySupported);

  /* If Variable Store Integrity is enabled, use the reserved partition
   * to store the measurements. If the reserved partition is not found or does
   * not meet the alignment requirements, return an error.
   */
  if (CheckVarStoreIntegrity == TRUE) {
    if ((ReservedPartitionOffset != 0) && (ReservedPartitionSize != 0) &&
        ((ReservedPartitionOffset % NorFlashAttributes.BlockSize) == 0) &&
        ((ReservedPartitionSize % NorFlashAttributes.BlockSize) == 0))
    {
      DEBUG ((
        DEBUG_ERROR,
        "%a:Using Reserved Partition %lu %lu for VarStore Integrity\n",
        __FUNCTION__,
        ReservedPartitionOffset,
        ReservedPartitionSize
        ));
    } else {
      DEBUG ((
        DEBUG_ERROR,
        "Cannot store Variable measurements %lu %lu %lu\n",
        ReservedPartitionOffset,
        ReservedPartitionSize,
        NorFlashAttributes.BlockSize
        ));
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }
  }

  // Build FVB instances
  FvpData = NULL;
  FvpData = AllocateRuntimeZeroPool (sizeof (NVIDIA_FVB_PRIVATE_DATA) * FVB_TO_CREATE);
  if (FvpData == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to create FvpData\r\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  VarStoreBuffer = NULL;
  VarStoreBuffer = AllocateRuntimePages (EFI_SIZE_TO_PAGES (VariableSize));
  if (VarStoreBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to create VarStoreBuffer\r\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: VariableOffset %lu VariableSize %lu"
    "FTW Offset %lu FTW Size %lu",
    __FUNCTION__,
    VariableOffset,
    VariableSize,
    FtwOffset,
    FtwSize
    ));
  FvpData[FVB_VARIABLE_INDEX].PartitionOffset  = VariableOffset;
  FvpData[FVB_VARIABLE_INDEX].PartitionSize    = VariableSize;
  FvpData[FVB_VARIABLE_INDEX].PartitionData    = VarStoreBuffer;
  FvpData[FVB_VARIABLE_INDEX].PartitionAddress = (UINTN)FvpData[FVB_VARIABLE_INDEX].PartitionData;
  PatchPcdSet64 (PcdFlashNvStorageVariableBase64, FvpData[FVB_VARIABLE_INDEX].PartitionAddress);
  PatchPcdSet32 (PcdFlashNvStorageVariableSize, FvpData[FVB_VARIABLE_INDEX].PartitionSize);

  FtwSpareBuffer = NULL;
  FtwSpareBuffer = AllocateAlignedRuntimePages (EFI_SIZE_TO_PAGES (VariableSize), NorFlashAttributes.BlockSize);
  if (FtwSpareBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to create FtwSpareBuffer\r\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  FvpData[FVB_FTW_SPARE_INDEX].PartitionOffset  = FtwOffset;
  FvpData[FVB_FTW_SPARE_INDEX].PartitionSize    = VariableSize;
  FvpData[FVB_FTW_SPARE_INDEX].PartitionData    = NULL;
  FvpData[FVB_FTW_SPARE_INDEX].PartitionAddress = (UINTN)FtwSpareBuffer;

  PatchPcdSet64 (PcdFlashNvStorageFtwSpareBase64, FvpData[FVB_FTW_SPARE_INDEX].PartitionAddress);
  PatchPcdSet32 (PcdFlashNvStorageFtwSpareSize, FvpData[FVB_FTW_SPARE_INDEX].PartitionSize);

  FtwWorkingBuffer = NULL;
  FtwWorkingBuffer = AllocateAlignedRuntimePages (EFI_SIZE_TO_PAGES (FtwSize - VariableSize), NorFlashAttributes.BlockSize);
  if (FtwWorkingBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to create FtwWorkingBuffer\r\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  FvpData[FVB_FTW_WORK_INDEX].PartitionOffset  = FtwOffset + PcdGet32 (PcdFlashNvStorageFtwSpareSize);
  FvpData[FVB_FTW_WORK_INDEX].PartitionSize    = FtwSize - VariableSize;
  FvpData[FVB_FTW_WORK_INDEX].PartitionData    = NULL;
  FvpData[FVB_FTW_WORK_INDEX].PartitionAddress = (UINTN)FtwWorkingBuffer;
  PatchPcdSet64 (PcdFlashNvStorageFtwWorkingBase64, FvpData[FVB_FTW_WORK_INDEX].PartitionAddress);
  PatchPcdSet32 (PcdFlashNvStorageFtwWorkingSize, FvpData[FVB_FTW_WORK_INDEX].PartitionSize);

  // Spare needs to hold at least the full variable partition
  ASSERT (FvpData[FVB_FTW_SPARE_INDEX].PartitionSize >= VariableSize);

  // Verify partitions are big enough
  if ((FvpData[FVB_FTW_WORK_INDEX].PartitionSize + FvpData[FVB_FTW_SPARE_INDEX].PartitionSize) > FtwSize) {
    DEBUG ((DEBUG_ERROR, "%a: FTW partition not large enough to fit working and spare\r\n", __FUNCTION__));
    ASSERT (FALSE);
    goto Exit;
  }

  /* If VarStore Integrity feature is enabled, initialize and install the protocol */
  if (CheckVarStoreIntegrity == TRUE) {
    Status = VarIntInit (
               ReservedPartitionOffset,
               ReservedPartitionSize,
               NorFlashProtocol,
               &NorFlashAttributes
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a:%d Failed to init Variable Integrity %r\n",
        __FUNCTION__,
        __LINE__,
        Status
        ));
      // If enabled VarStore Integrity feature is a must have.
      ASSERT (FALSE);
      goto Exit;
    }

    /* Register a callback for when the SmmVariable Protocol is installed
     * to validate the measurements.
     */
    Status = gMmst->MmRegisterProtocolNotify (
                      &gEfiSmmVariableProtocolGuid,
                      MmFvbSmmVarReady,
                      &MmFvbRegistration
                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to register callback %r\n",
        __FUNCTION__,
        Status
        ));
      ASSERT_EFI_ERROR (Status);
    }
  }

  for (Index = 0; Index < FVB_TO_CREATE; Index++) {
    if (Index == FVB_FTW_WORK_INDEX) {
      FvpData[Index].Signature = NVIDIA_FWB_SIGNATURE;
    } else if (Index == FVB_FTW_SPARE_INDEX) {
      FvpData[Index].Signature = NVIDIA_FSB_SIGNATURE;
    } else {
      FvpData[Index].Signature = NVIDIA_FVB_SIGNATURE;
    }

    FvpData[Index].NorFlashProtocol = NorFlashProtocol;
    CopyMem (&FvpData[Index].FlashAttributes, &NorFlashAttributes, sizeof (NorFlashAttributes));

    if (FvpData[Index].PartitionData != NULL) {
      Status = NorFlashProtocol->Read (
                                   NorFlashProtocol,
                                   FvpData[Index].PartitionOffset,
                                   FvpData[Index].PartitionSize,
                                   FvpData[Index].PartitionData
                                   );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to read partition data (%r)\r\n", __FUNCTION__, Status));
        goto Exit;
      }
    }

    FvpData[Index].FvbProtocol.GetAttributes      = FvbGetAttributes;
    FvpData[Index].FvbProtocol.SetAttributes      = FvbSetAttributes;
    FvpData[Index].FvbProtocol.GetPhysicalAddress = FvbGetPhysicalAddress;
    FvpData[Index].FvbProtocol.GetBlockSize       = FvbGetBlockSize;
    FvpData[Index].FvbProtocol.Read               = FvbRead;
    FvpData[Index].FvbProtocol.Write              = FvbWrite;
    FvpData[Index].FvbProtocol.EraseBlocks        = FvbEraseBlocks;
    FvpData[Index].FvbProtocol.ParentHandle       = NULL;

    // Validate and initialize content
    if (Index == FVB_VARIABLE_INDEX) {
      Status = ValidateFvHeader (
                 FvpData[Index].PartitionData,
                 FvpData[Index].PartitionOffset,
                 FvpData[Index].PartitionSize,
                 (Index == FVB_VARIABLE_INDEX),
                 NorFlashProtocol,
                 &NorFlashAttributes,
                 ReservedPartitionOffset,
                 ReservedPartitionSize
                 );
      if (EFI_ERROR (Status)) {
        // Re-init partition
        Status = InitializeFvAndVariableStoreHeaders (
                   (EFI_FIRMWARE_VOLUME_HEADER *)FvpData[Index].PartitionData,
                   FvpData[Index].PartitionOffset,
                   FvpData[Index].PartitionSize,
                   (Index == FVB_VARIABLE_INDEX),
                   NorFlashProtocol,
                   &NorFlashAttributes
                   );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to init FVB %u\r\n", __FUNCTION__, Index));
          goto Exit;
        }
      }
    } else if (Index == FVB_FTW_WORK_INDEX) {
      // Init work partition if needed
      InitializeWorkSpaceHeader (
        FvpData[Index].PartitionOffset,
        FvpData[Index].PartitionSize,
        NorFlashProtocol,
        &NorFlashAttributes
        );
    }

    Status = gMmst->MmInstallProtocolInterface (
                      &FvpData[Index].Handle,
                      &gEfiSmmFirmwareVolumeBlockProtocolGuid,
                      EFI_NATIVE_INTERFACE,
                      &FvpData[Index].FvbProtocol
                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to install FVP protocol Index %u %p Status %r\r\n",
        __FUNCTION__,
        Index,
        &FvpData[Index].Handle,
        Status
        ));
      goto Exit;
    }
  }

  Status = gMmst->MmInstallProtocolInterface (
                    &ImageHandle,
                    &gEdkiiNvVarStoreFormattedGuid,
                    EFI_NATIVE_INTERFACE,
                    NULL
                    );

Exit:

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ERROR!!!!", __FUNCTION__));
    if (FvpData != NULL) {
      FreePool (FvpData);
    }

    if (VarStoreBuffer != NULL) {
      FreePages (VarStoreBuffer, EFI_SIZE_TO_PAGES (VariableSize));
    }

    if (FtwSpareBuffer != NULL) {
      FreePages (FtwSpareBuffer, EFI_SIZE_TO_PAGES (VariableSize));
    }

    if (FtwWorkingBuffer != NULL) {
      FreePages (FtwWorkingBuffer, EFI_SIZE_TO_PAGES (FtwSize - VariableSize));
    }
  }

  return EFI_SUCCESS;
}
