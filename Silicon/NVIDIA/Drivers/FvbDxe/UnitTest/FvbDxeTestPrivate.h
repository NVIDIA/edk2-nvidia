/** @file
  Unit test definitions for the Fvb driver.

  SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _FVB_DXE_TEST_PRIVATE_H_
#define _FVB_DXE_TEST_PRIVATE_H_

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>

#include <HostBasedTestStubLib/FlashStubLib.h>

// So that we can reference FvbPrivate declarations
#include "../FvbPrivate.h"

// Test context for FvbRead and FvbWrite tests
typedef struct {
  EFI_LBA       Lba;              // Input Lba
  UINTN         Offset;           // Input Offset
  UINTN         NumBytes;         // Input NumBytes
  EFI_STATUS    ExpectedStatus;   // Expected Status returned
  UINTN         ExpectedNumBytes; // Expected NumBytes returned
} RW_TEST_CONTEXT;

// Fvb Functions we want to test

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

/**
  Initializes the FV Header and Variable Store Header
  to support variable operations.

  @param FirmwareVolumeHeader - A pointer to a firmware volume header

**/
EFI_STATUS
InitializeFvAndVariableStoreHeaders (
  IN EFI_FIRMWARE_VOLUME_HEADER  *FirmwareVolumeHeader
  );

/**
  Check the integrity of firmware volume header.

  @param FwVolHeader - A pointer to a firmware volume header

**/
EFI_STATUS
ValidateFvHeader (
  IN EFI_FIRMWARE_VOLUME_HEADER  *FwVolHeader
  );

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
FtwGetMaxBlockSize (
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL  *This,
  OUT UINTN                             *BlockSize
  );

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
FtwWrite (
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL  *This,
  IN EFI_LBA                            Lba,
  IN UINTN                              Offset,
  IN UINTN                              Length,
  IN VOID                               *PrivateData,
  IN EFI_HANDLE                         FvbHandle,
  IN VOID                               *Buffer
  );

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
FtwAllocate (
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL  *This,
  IN EFI_GUID                           *CallerId,
  IN UINTN                              PrivateDataSize,
  IN UINTN                              NumberOfWrites
  );

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
FtwRestart (
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL  *This,
  IN EFI_HANDLE                         FvbHandle
  );

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
FtwGetLastWrite (
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL  *This,
  OUT EFI_GUID                          *CallerId,
  OUT EFI_LBA                           *Lba,
  OUT UINTN                             *Offset,
  OUT UINTN                             *Length,
  IN OUT UINTN                          *PrivateDataSize,
  OUT VOID                              *PrivateData,
  OUT BOOLEAN                           *Complete
  );

/**
  Aborts all previously allocated writes.

  @param  This                 The calling context.

  @retval EFI_SUCCESS          The function completed successfully.
  @retval EFI_ABORTED          The function could not complete successfully.
  @retval EFI_NOT_FOUND        No allocated writes exist.

**/
EFI_STATUS
EFIAPI
FtwAbort (
  IN EFI_FAULT_TOLERANT_WRITE_PROTOCOL  *This
  );

#endif
