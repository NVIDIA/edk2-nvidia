/** @file

  BR-BCT Update Device Library

  SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/BrBctUpdateDeviceLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Uefi/UefiBaseType.h>

#define BR_BCT_SLOT_MAX                       4
#define BR_BCT_SLOT_MARKER_BASED_MAX          3
#define BR_BCT_BACKUP_PARTITION_CHAIN_OFFSET  (16 * 1024)
#define BR_BCT_BACKUP_PARTITION_DATA_SIZE     (BR_BCT_BACKUP_PARTITION_CHAIN_OFFSET * BOOT_CHAIN_COUNT)

STATIC BR_BCT_UPDATE_PRIVATE_DATA  mPrivate                         = { 0 };
STATIC UINT32                      mActiveBootChain                 = MAX_UINT32;
STATIC VOID                        *mVerifyBuffer                   = NULL;
STATIC BOOLEAN                     mPcdBrBctVerifyUpdateBeforeWrite = FALSE;
STATIC BOOLEAN                     mPcdFwImageEnableBPartitions     = FALSE;
STATIC BOOLEAN                     mPcdOverwriteActiveFwPartition   = FALSE;
STATIC BOOLEAN                     mPcdBootChainIsMarkerBased       = FALSE;
STATIC VOID                        *mInvalidateBuffer               = NULL;
STATIC VOID                        *mBackupPartitionBuffer          = NULL;

/**
  Get device offset of given BR-BCT slot data.

  @param[in]  Private           Pointer to private data structure
  @param[in]  Slot              Slot number

  @retval UINT64                Offset in bytes of slot data in device

**/
STATIC
UINT64
EFIAPI
BrBctGetSlotOffset (
  IN  BR_BCT_UPDATE_PRIVATE_DATA  *Private,
  IN  UINTN                       Slot
  )
{
  return Slot * Private->SlotSize;
}

/**
  Read slot's data from a BR-BCT partition.

  @param[in]  Private           Pointer to private data structure
  @param[in]  Partition         Partition to read
  @param[in]  Slot              Slot number
  @param[in]  Bytes             Bytes to read
  @param[out] Buffer            Address to read data into

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
BrBctReadSlot (
  IN  BR_BCT_UPDATE_PRIVATE_DATA  *Private,
  IN  FW_PARTITION_PRIVATE_DATA   *Partition,
  IN  UINT64                      Offset,
  IN  UINTN                       Bytes,
  OUT VOID                        *Buffer
  )
{
  NVIDIA_FW_PARTITION_PROTOCOL  *Protocol;
  EFI_STATUS                    Status;

  DEBUG ((
    DEBUG_INFO,
    "%a: Reading %s offset %lu\n",
    __FUNCTION__,
    Partition->PartitionInfo.Name,
    Offset
    ));

  Protocol = &Partition->Protocol;

  Status = Protocol->Read (Protocol, Offset, Bytes, Buffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error reading %s offset %lu: %r\n",
      __FUNCTION__,
      Partition->PartitionInfo.Name,
      Offset,
      Status
      ));
  }

  return Status;
}

/**
  Verify slot's data in a BR-BCT partition.

  @param[in]  Private           Pointer to private data structure
  @param[in]  Partition         Partition to read
  @param[in]  Slot              Slot number
  @param[in]  Bytes             Bytes to read/verify
  @param[out] Buffer            Address of data for comparison

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
BrBctVerifySlot (
  IN  BR_BCT_UPDATE_PRIVATE_DATA  *Private,
  IN  FW_PARTITION_PRIVATE_DATA   *Partition,
  IN  UINTN                       Slot,
  IN  UINTN                       Bytes,
  IN  CONST VOID                  *Buffer
  )
{
  EFI_STATUS  Status;
  UINT32      BlockSize;
  UINTN       BytesToRead;

  DEBUG ((DEBUG_INFO, "%a: Verifying slot %u\n", __FUNCTION__, Slot));

  BlockSize   = Partition->DeviceInfo->BlockSize;
  BytesToRead = ALIGN_VALUE (Bytes, BlockSize);

  if (BytesToRead > Private->BrBctDataSize) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BrBctReadSlot (
             Private,
             Partition,
             BrBctGetSlotOffset (Private, Slot),
             BytesToRead,
             mVerifyBuffer
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (CompareMem (mVerifyBuffer, Buffer, Bytes) != 0) {
    return EFI_VOLUME_CORRUPTED;
  }

  return EFI_SUCCESS;
}

/**
  Write data into slot in a BR-BCT partition.

  @param[in]  Private           Pointer to private data structure
  @param[in]  Partition         Partition to write
  @param[in]  Slot              Slot number
  @param[in]  Bytes             Bytes to write
  @param[out] Buffer            Address of data to write

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
BrBctWriteSlot (
  IN  BR_BCT_UPDATE_PRIVATE_DATA  *Private,
  IN  FW_PARTITION_PRIVATE_DATA   *Partition,
  IN  UINTN                       Slot,
  IN  UINTN                       Bytes,
  IN  CONST VOID                  *Buffer
  )
{
  NVIDIA_FW_PARTITION_PROTOCOL  *Protocol;
  UINT64                        Offset;
  EFI_STATUS                    Status;

  Protocol = &Partition->Protocol;
  Offset   = BrBctGetSlotOffset (Private, Slot);

  DEBUG ((
    DEBUG_INFO,
    "%a: Writing %s slot %u offset 0x%lx\n",
    __FUNCTION__,
    Protocol->PartitionName,
    Slot,
    Offset
    ));

  Status = Protocol->Write (Protocol, Offset, Bytes, Buffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error writing slot=%u: %r\n",
      __FUNCTION__,
      Slot,
      Status
      ));
  }

  return Status;
}

/**
  Write and verify a slot's data in a BR-BCT partition.
  If PcdBrBctVerifyUpdateBeforeWrite is TRUE, slot data is verified to need
  updating before performing a new write/verify.

  @param[in]  Private           Pointer to private data structure
  @param[in]  Partition         Partition to write
  @param[in]  Slot              Slot number
  @param[in]  Bytes             Bytes to write
  @param[out] Buffer            Address of data to write

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
BrBctWriteAndVerifySlot (
  IN  BR_BCT_UPDATE_PRIVATE_DATA  *Private,
  IN  FW_PARTITION_PRIVATE_DATA   *Partition,
  IN  UINTN                       Slot,
  IN  UINTN                       Bytes,
  IN  CONST VOID                  *Buffer
  )
{
  EFI_STATUS  Status;

  if (mPcdBrBctVerifyUpdateBeforeWrite) {
    Status = BrBctVerifySlot (Private, Partition, Slot, Bytes, Buffer);
    if (Status == EFI_SUCCESS) {
      DEBUG ((
        DEBUG_INFO,
        "%a: Slot=%u Bytes=%u no update needed\n",
        __FUNCTION__,
        Slot,
        Bytes
        ));
      return Status;
    }
  }

  Status = BrBctWriteSlot (Private, Partition, Slot, Bytes, Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = BrBctVerifySlot (Private, Partition, Slot, Bytes, Buffer);

  return Status;
}

STATIC
BOOLEAN
EFIAPI
SlotShouldBeInvalidated (
  IN  UINTN  Slot,
  IN  UINTN  NewFwChain
  )
{
  if (mPcdBootChainIsMarkerBased) {
    if ((NewFwChain == BOOT_CHAIN_B) && (Slot == 0)) {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
BOOLEAN
EFIAPI
SlotShouldBeUpdated (
  IN  UINTN  Slot,
  IN  UINTN  NewFwChain
  )
{
  if (mPcdBootChainIsMarkerBased) {
    return (
            ((Slot & 0x1) == NewFwChain) ||
            SlotShouldBeInvalidated (Slot, NewFwChain)
            );
  }

  return TRUE;
}

/**
  Update all BCT partition slots

  @param[in]  Private           Pointer to private data structure
  @param[in]  Bytes             Bytes to write
  @param[out] Buffer            Address of data to write
  @param[in]  NewFwChain        New firmware chain to boot

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
BrBctUpdateBctSlots (
  BR_BCT_UPDATE_PRIVATE_DATA  *Private,
  IN  UINTN                   Bytes,
  IN  CONST VOID              *Buffer,
  IN  UINTN                   NewFwChain
  )
{
  UINTN       Index;
  UINTN       Slot;
  CONST VOID  *BufferToWrite = Buffer;
  EFI_STATUS  Status         = EFI_SUCCESS;

  for (Index = 0; Index < Private->BctPartitionSlots; Index++) {
    Slot = Private->BctPartitionSlots - Index - 1;
    if (!SlotShouldBeUpdated (Slot, NewFwChain)) {
      DEBUG ((DEBUG_INFO, "%a: Slot=%u not updated\n", __FUNCTION__, Slot));
      continue;
    }

    if (SlotShouldBeInvalidated (Slot, NewFwChain)) {
      DEBUG ((DEBUG_INFO, "%a: Slot=%u invalidated\n", __FUNCTION__, Slot));
      BufferToWrite = mInvalidateBuffer;
    }

    Status = BrBctWriteAndVerifySlot (
               Private,
               Private->BrBctPartition,
               Slot,
               Bytes,
               BufferToWrite
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return Status;
}

// NVIDIA_BR_BCT_UPDATE_PROTOCOL.UpdateFwChain()
EFI_STATUS
EFIAPI
BrBctUpdateFwChain (
  IN  CONST NVIDIA_BR_BCT_UPDATE_PROTOCOL  *This,
  IN  UINTN                                NewFwChain
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA  *Private;
  EFI_STATUS                  Status;
  VOID                        *Buffer;
  UINT32                      DataSize;
  UINT64                      BackupOffset;

  DEBUG ((
    DEBUG_INFO,
    "%a: ActiveChain=%u, NewFwChain=%u\n",
    __FUNCTION__,
    mActiveBootChain,
    NewFwChain
    ));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              BR_BCT_UPDATE_PRIVATE_DATA,
              Protocol,
              BR_BCT_UPDATE_PRIVATE_DATA_SIGNATURE
              );
  DataSize = Private->BrBctDataSize;
  Buffer   = AllocateZeroPool (DataSize);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  BackupOffset = NewFwChain * BR_BCT_BACKUP_PARTITION_CHAIN_OFFSET;

  Status = BrBctReadSlot (
             Private,
             Private->BrBctBackupPartition,
             BackupOffset,
             DataSize,
             Buffer
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = BrBctUpdateBctSlots (Private, DataSize, Buffer, NewFwChain);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  SetNextBootChain (NewFwChain);

Done:
  FreePool (Buffer);
  return Status;
}

// NVIDIA_BR_BCT_UPDATE_PROTOCOL.UpdateBackupPartition()
STATIC
EFI_STATUS
EFIAPI
BrBctUpdateBackupPartition (
  IN  CONST NVIDIA_BR_BCT_UPDATE_PROTOCOL  *This,
  IN  CONST VOID                           *Data
  )
{
  NVIDIA_FW_PARTITION_PROTOCOL  *PartitionProtocol;
  BR_BCT_UPDATE_PRIVATE_DATA    *Private;
  EFI_STATUS                    Status;
  UINT32                        PartitionDataSize;
  UINT64                        BackupOffset;
  UINTN                         UpdateFwChain;

  DEBUG ((DEBUG_INFO, "%a: ActiveChain=%u\n", __FUNCTION__, mActiveBootChain));

  if ((This == NULL) || (Data == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (mPcdFwImageEnableBPartitions) {
    UpdateFwChain = OTHER_BOOT_CHAIN (mActiveBootChain);
  } else if (mPcdOverwriteActiveFwPartition) {
    UpdateFwChain = mActiveBootChain;
  } else {
    return EFI_UNSUPPORTED;
  }

  Private = CR (
              This,
              BR_BCT_UPDATE_PRIVATE_DATA,
              Protocol,
              BR_BCT_UPDATE_PRIVATE_DATA_SIGNATURE
              );
  PartitionDataSize = BR_BCT_BACKUP_PARTITION_DATA_SIZE;
  PartitionProtocol = &Private->BrBctBackupPartition->Protocol;
  Status            = PartitionProtocol->Read (
                                           PartitionProtocol,
                                           0,
                                           PartitionDataSize,
                                           mBackupPartitionBuffer
                                           );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: read failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  BackupOffset = UpdateFwChain * BR_BCT_BACKUP_PARTITION_CHAIN_OFFSET;

  if (CompareMem (
        mBackupPartitionBuffer + BackupOffset,
        Data + BackupOffset,
        Private->BrBctDataSize
        ) == 0)
  {
    DEBUG ((DEBUG_INFO, "%a: no update needed at offset=0x%x\n", __FUNCTION__, BackupOffset));
    return EFI_SUCCESS;
  }

  CopyMem (
    mBackupPartitionBuffer + BackupOffset,
    Data + BackupOffset,
    Private->BrBctDataSize
    );

  DEBUG ((DEBUG_INFO, "%a: Updating partition at offset=0x%x bytes=%u\n", __FUNCTION__, BackupOffset, Private->BrBctDataSize));

  Status = PartitionProtocol->Write (
                                PartitionProtocol,
                                0,
                                PartitionDataSize,
                                mBackupPartitionBuffer
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: write failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = PartitionProtocol->Read (
                                PartitionProtocol,
                                0,
                                PartitionDataSize,
                                mVerifyBuffer
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: verify read failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (CompareMem (mVerifyBuffer, mBackupPartitionBuffer, PartitionDataSize) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: verify failed\n", __FUNCTION__));
    return EFI_VOLUME_CORRUPTED;
  }

  return Status;
}

VOID
EFIAPI
BrBctUpdateAddressChangeHandler (
  IN  FW_PARTITION_ADDRESS_CONVERT  ConvertFunction
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA  *Private;

  Private = &mPrivate;
  if (Private != NULL ) {
    ConvertFunction ((VOID **)&Private->BrBctPartition);
    ConvertFunction ((VOID **)&Private->BrBctBackupPartition);
    ConvertFunction ((VOID **)&Private->Protocol.UpdateFwChain);
    ConvertFunction ((VOID **)&Private->Protocol.UpdateBackupPartition);
  }

  if (mVerifyBuffer != NULL) {
    ConvertFunction ((VOID **)&mVerifyBuffer);
  }

  if (mBackupPartitionBuffer != NULL) {
    ConvertFunction ((VOID **)&mBackupPartitionBuffer);
  }
}

BR_BCT_UPDATE_PRIVATE_DATA *
EFIAPI
BrBctUpdateGetPrivate (
  VOID
  )
{
  return &mPrivate;
}

VOID
EFIAPI
BrBctUpdateDeviceLibDeinit (
  VOID
  )
{
  if (mVerifyBuffer != NULL) {
    FreePool (mVerifyBuffer);
    mVerifyBuffer = NULL;
  }

  if (mBackupPartitionBuffer != NULL) {
    FreePool (mBackupPartitionBuffer);
    mBackupPartitionBuffer = NULL;
  }

  mActiveBootChain = MAX_UINT32;
  SetMem (&mPrivate, sizeof (mPrivate), 0);
}

EFI_STATUS
EFIAPI
BrBctUpdateDeviceLibInit (
  IN  UINT32  ActiveBootChain,
  IN  UINT32  EraseBlockSize
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA  *Private;
  UINTN                       MaxBctSlotsSupported;

  mActiveBootChain                 = ActiveBootChain;
  mPcdBrBctVerifyUpdateBeforeWrite = PcdGetBool (PcdBrBctVerifyUpdateBeforeWrite);
  mPcdFwImageEnableBPartitions     = PcdGetBool (PcdFwImageEnableBPartitions);
  mPcdOverwriteActiveFwPartition   = PcdGetBool (PcdOverwriteActiveFwPartition);
  mPcdBootChainIsMarkerBased       = PcdGetBool (PcdBootChainIsMarkerBased);

  MaxBctSlotsSupported = (mPcdBootChainIsMarkerBased ? BR_BCT_SLOT_MARKER_BASED_MAX : BR_BCT_SLOT_MAX);

  // Initialize our private data for the protocol
  Private                = &mPrivate;
  Private->Signature     = BR_BCT_UPDATE_PRIVATE_DATA_SIGNATURE;
  Private->BrBctDataSize = PcdGet32 (PcdBrBctDataSize);
  Private->SlotSize      = MAX (
                             EraseBlockSize,
                             PcdGet32 (PcdBrBctLogicalSlotSize)
                             );
  Private->Protocol.UpdateFwChain         = BrBctUpdateFwChain;
  Private->Protocol.UpdateBackupPartition = BrBctUpdateBackupPartition;

  // Find the BCT and backup partitions
  Private->BrBctPartition       = FwPartitionFindByName (L"BCT");
  Private->BrBctBackupPartition = FwPartitionFindByName (BR_BCT_BACKUP_PARTITION_NAME);
  if ((Private->BrBctPartition == NULL) ||
      (Private->BrBctBackupPartition == NULL))
  {
    DEBUG ((DEBUG_INFO, "%a: Missing BCT partitions\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  // set number of BCT slots, min=1, max=BR_BCT_SLOT_MAX
  Private->BctPartitionSlots = (Private->BrBctPartition->PartitionInfo.Bytes /
                                Private->SlotSize);
  if (Private->BctPartitionSlots == 0) {
    if (Private->BrBctDataSize <= Private->BrBctPartition->PartitionInfo.Bytes) {
      Private->BctPartitionSlots = 1;
    } else {
      DEBUG ((
        DEBUG_ERROR,
        "%a: BCT partition size=%u too small\n",
        __FUNCTION__,
        Private->BrBctPartition->PartitionInfo.Bytes
        ));

      return EFI_UNSUPPORTED;
    }
  } else if (Private->BctPartitionSlots > MaxBctSlotsSupported) {
    Private->BctPartitionSlots = MaxBctSlotsSupported;
  }

  NV_ASSERT_RETURN (Private->BrBctDataSize <= BR_BCT_BACKUP_PARTITION_CHAIN_OFFSET, return EFI_UNSUPPORTED, "%a: data size %u > chain offset\n", __FUNCTION__, Private->BrBctDataSize);

  DEBUG ((
    DEBUG_INFO,
    "%a: BCT partition slots=%u size=0x%x\n",
    __FUNCTION__,
    Private->BctPartitionSlots,
    Private->SlotSize
    ));

  // Pre-allocate verify buffers to support runtime update of BCT data
  mVerifyBuffer = AllocateRuntimeZeroPool (BR_BCT_BACKUP_PARTITION_DATA_SIZE);
  if (mVerifyBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: mVerifyBuffer alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  mBackupPartitionBuffer = AllocateRuntimeZeroPool (BR_BCT_BACKUP_PARTITION_DATA_SIZE);
  if (mBackupPartitionBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: mBackupPartitionBuffer alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  if (mPcdBootChainIsMarkerBased) {
    mInvalidateBuffer = AllocateRuntimePool (Private->BrBctDataSize);
    if (mInvalidateBuffer == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: mInvalidateBuffer alloc failed\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    SetMem (mInvalidateBuffer, Private->BrBctDataSize, 0xff);
  }

  return EFI_SUCCESS;
}
