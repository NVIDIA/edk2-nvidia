/** @file

  BR-BCT Update Device Library

  Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/BrBctUpdateDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Uefi/UefiBaseType.h>

#define BR_BCT_SLOT_MAX                         4
#define BR_BCT_BACKUP_PARTITION_CHAIN_OFFSET    (16 * 1024)

STATIC BR_BCT_UPDATE_PRIVATE_DATA   mPrivate                            = {0};
STATIC UINT32                       mActiveBootChain                    = MAX_UINT32;
STATIC VOID                         *mVerifyBuffer                      = NULL;
STATIC BOOLEAN                      mPcdBrBctVerifyUpdateBeforeWrite    = FALSE;
STATIC BOOLEAN                      mPcdFwImageEnableBPartitions        = FALSE;
STATIC BOOLEAN                      mPcdOverwriteActiveFwPartition      = FALSE;

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
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  UINTN                             Slot
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
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  FW_PARTITION_PRIVATE_DATA         *Partition,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  OUT VOID                              *Buffer
  )
{
  NVIDIA_FW_PARTITION_PROTOCOL      *Protocol;
  EFI_STATUS                        Status;

  DEBUG ((DEBUG_INFO, "%a: Reading %s offset %u\n",
          __FUNCTION__, Partition->PartitionInfo.Name, Offset));

  Protocol          = &Partition->Protocol;

  Status = Protocol->Read (Protocol, Offset, Bytes, Buffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error reading %s offset %u: %r\n",
            __FUNCTION__, Partition->PartitionInfo.Name, Offset, Status));
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
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  FW_PARTITION_PRIVATE_DATA         *Partition,
  IN  UINTN                             Slot,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer
  )
{
  EFI_STATUS                        Status;
  UINT32                            BlockSize;
  UINTN                             BytesToRead;

  DEBUG ((DEBUG_INFO, "%a: Verifying slot %u\n", __FUNCTION__, Slot));

  BlockSize         = Partition->DeviceInfo->BlockSize;
  BytesToRead       = ALIGN_VALUE (Bytes, BlockSize);

  if (BytesToRead > Private->BrBctDataSize) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BrBctReadSlot (Private,
                          Partition,
                          BrBctGetSlotOffset (Private, Slot),
                          BytesToRead,
                          mVerifyBuffer);
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
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  FW_PARTITION_PRIVATE_DATA         *Partition,
  IN  UINTN                             Slot,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer
  )
{
  NVIDIA_FW_PARTITION_PROTOCOL      *Protocol;
  UINT64                            Offset;
  EFI_STATUS                        Status;

  Protocol          = &Partition->Protocol;
  Offset            = BrBctGetSlotOffset (Private, Slot);

  DEBUG ((DEBUG_INFO, "%a: Writing %s slot %u offset 0x%x\n",
          __FUNCTION__, Protocol->PartitionName, Slot, Offset));

  Status = Protocol->Write (Protocol, Offset, Bytes, Buffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error writing slot=%u: %r\n",
          __FUNCTION__, Slot, Status));
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
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  FW_PARTITION_PRIVATE_DATA         *Partition,
  IN  UINTN                             Slot,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer
  )
{
  EFI_STATUS                            Status;

  if (mPcdBrBctVerifyUpdateBeforeWrite) {
    Status = BrBctVerifySlot (Private, Partition, Slot, Bytes, Buffer);
    if (Status == EFI_SUCCESS) {
      DEBUG ((DEBUG_INFO, "%a: Slot=%u Bytes=%u no update needed\n",
              __FUNCTION__, Slot, Bytes));
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

/**
  Update all BCT partition slots

  @param[in]  Private           Pointer to private data structure
  @param[in]  Bytes             Bytes to write
  @param[out] Buffer            Address of data to write

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
BrBctUpdateBctSlots (
  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  UINTN                         Bytes,
  IN  CONST VOID                    *Buffer
  )
{
  UINTN                             Index;
  EFI_STATUS                        Status;

  for (Index = 0; Index < Private->BctPartitionSlots; Index++) {
    Status = BrBctWriteAndVerifySlot (Private,
                                      Private->BrBctPartition,
                                      Private->BctPartitionSlots - Index - 1,
                                      Bytes,
                                      Buffer);
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
  IN  CONST NVIDIA_BR_BCT_UPDATE_PROTOCOL   *This,
  IN  UINTN                                 NewFwChain
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA        *Private;
  EFI_STATUS                        Status;
  VOID                              *Buffer;
  UINT32                            DataSize;
  UINT64                            BackupOffset;

  DEBUG ((DEBUG_INFO, "%a: ActiveChain=%u, NewFwChain=%u\n",
          __FUNCTION__, mActiveBootChain, NewFwChain));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private   = CR (This,
                  BR_BCT_UPDATE_PRIVATE_DATA,
                  Protocol,
                  BR_BCT_UPDATE_PRIVATE_DATA_SIGNATURE);
  DataSize  = Private->BrBctDataSize;
  Buffer    = AllocateZeroPool (DataSize);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  BackupOffset = NewFwChain * BR_BCT_BACKUP_PARTITION_CHAIN_OFFSET;

  Status = BrBctReadSlot (Private,
                          Private->BrBctBackupPartition,
                          BackupOffset,
                          DataSize,
                          Buffer);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = BrBctUpdateBctSlots (Private, DataSize, Buffer);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  SetNextBootChain (NewFwChain);

Done:
  FreePool (Buffer);
  return Status;
}

VOID
EFIAPI
BrBctUpdateAddressChangeHandler (
  IN  FW_PARTITION_ADDRESS_CONVERT ConvertFunction
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA    *Private;

  Private = &mPrivate;
  if (Private != NULL ) {
    ConvertFunction ((VOID **) &Private->BrBctPartition);
    ConvertFunction ((VOID **) &Private->BrBctBackupPartition);
    ConvertFunction ((VOID **) &Private->DeviceErase);
    ConvertFunction ((VOID **) &Private->Protocol.UpdateFwChain);
  }

  if (mVerifyBuffer != NULL) {
    ConvertFunction ((VOID **) &mVerifyBuffer);
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
  mActiveBootChain = MAX_UINT32;
  SetMem (&mPrivate, sizeof (mPrivate), 0);
}

EFI_STATUS
EFIAPI
BrBctUpdateDeviceLibInit (
  IN  UINT32                        ActiveBootChain,
  IN  BR_BCT_UPDATE_DEVICE_ERASE    DeviceErase,
  IN  UINT32                        EraseBlockSize
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA        *Private;

  mActiveBootChain = ActiveBootChain;

  // Initialize our private data for the protocol
  Private                           = &mPrivate;
  Private->Signature                = BR_BCT_UPDATE_PRIVATE_DATA_SIGNATURE;
  Private->BrBctDataSize            = PcdGet32 (PcdBrBctDataSize);
  Private->SlotSize                 = MAX (EraseBlockSize,
                                           PcdGet32 (PcdBrBctLogicalSlotSize));
  Private->DeviceErase              = DeviceErase;
  Private->Protocol.UpdateFwChain   = BrBctUpdateFwChain;

  // Find the BCT and backup partitions
  Private->BrBctPartition = FwPartitionFindByName (L"BCT");
  Private->BrBctBackupPartition = FwPartitionFindByName (L"BCT-boot-chain_backup");
  if ((Private->BrBctPartition == NULL) ||
      (Private->BrBctBackupPartition == NULL)) {
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
      DEBUG ((DEBUG_ERROR, "%a: BCT partition size=%u too small\n",
              __FUNCTION__, Private->BrBctPartition->PartitionInfo.Bytes));

      return EFI_UNSUPPORTED;
    }
  } else if (Private->BctPartitionSlots > BR_BCT_SLOT_MAX) {
    Private->BctPartitionSlots = BR_BCT_SLOT_MAX;
  }

  DEBUG ((DEBUG_INFO, "%a: BCT partition slots=%u size=0x%x\n",
          __FUNCTION__, Private->BctPartitionSlots, Private->SlotSize));

  // Pre-allocate a slot verify buffer to support runtime update of BCT data
  mVerifyBuffer = AllocateRuntimeZeroPool (Private->BrBctDataSize);
  if (mVerifyBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: mVerifyBuffer alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  mPcdBrBctVerifyUpdateBeforeWrite = PcdGetBool (PcdBrBctVerifyUpdateBeforeWrite);
  mPcdFwImageEnableBPartitions = PcdGetBool (PcdFwImageEnableBPartitions);
  mPcdOverwriteActiveFwPartition = PcdGetBool (PcdOverwriteActiveFwPartition);

  return EFI_SUCCESS;
}
