/** @file

  BR-BCT Update Device Library

  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BrBctUpdateDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Uefi/UefiBaseType.h>

typedef enum {
  BR_BCT_SLOT_A0    = 0,
  BR_BCT_SLOT_B0    = 1,
  BR_BCT_SLOT_A1    = 2,
  BR_BCT_SLOT_B1    = 3,
} BR_BCT_UPDATE_SLOT;

STATIC BR_BCT_UPDATE_PRIVATE_DATA   mPrivate                            = {0};
STATIC UINT32                       mActiveBootChain                    = MAX_UINT32;
STATIC VOID                         *mVerifyBuffer                      = NULL;
STATIC BOOLEAN                      mPcdBrBctVerifyUpdateBeforeWrite    = FALSE;

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
  IN  BR_BCT_UPDATE_SLOT                Slot
  )
{
  return Private->BrBctPartition->PartitionInfo.Offset +
    (Slot * Private->SlotSize);
}

/**
  Erase given slot from BR-BCT partition.

  @param[in]  Private           Pointer to private data structure
  @param[in]  Slot              Slot number

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
BrBctEraseSlot (
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  BR_BCT_UPDATE_SLOT                Slot
  )
{
  FW_PARTITION_PRIVATE_DATA         *BrBctPartition;
  FW_PARTITION_DEVICE_INFO          *DeviceInfo;
  UINT32                            SlotSize;
  UINT64                            Offset;
  EFI_STATUS                        Status;

  DEBUG ((DEBUG_INFO, "%a: Erasing slot %u\n", __FUNCTION__, Slot));

  if (Slot != BR_BCT_SLOT_A0) {
    DEBUG ((DEBUG_ERROR, "%a: Slot=%u, can only erase A0\n",
            __FUNCTION__, Slot));
    return EFI_INVALID_PARAMETER;
  }

  SlotSize          = Private->SlotSize;
  BrBctPartition    = Private->BrBctPartition;
  DeviceInfo        = BrBctPartition->DeviceInfo;
  Offset            = BrBctGetSlotOffset (Private, Slot);

  Status = FwPartitionCheckOffsetAndBytes (BrBctPartition->PartitionInfo.Bytes,
                                           Offset,
                                           SlotSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: erase offset=%llu, bytes=%u error: %r\n",
            __FUNCTION__, Offset, SlotSize, Status));
    return Status;
  }

  return Private->DeviceErase (DeviceInfo, Offset, SlotSize);
}

/**
  Read slot's data from BR-BCT partition.

  @param[in]  Private           Pointer to private data structure
  @param[in]  Slot              Slot number
  @param[out] Buffer            Address to read data into

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
BrBctReadSlot (
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  BR_BCT_UPDATE_SLOT                Slot,
  OUT VOID                              *Buffer
  )
{
  FW_PARTITION_PRIVATE_DATA         *BrBctPartition;
  FW_PARTITION_DEVICE_INFO          *DeviceInfo;
  UINT32                            SlotSize;
  UINT64                            Offset;
  EFI_STATUS                        Status;

  DEBUG ((DEBUG_INFO, "%a: Reading slot %u\n", __FUNCTION__, Slot));

  SlotSize          = Private->SlotSize;
  BrBctPartition    = Private->BrBctPartition;
  DeviceInfo        = BrBctPartition->DeviceInfo;
  Offset            = BrBctGetSlotOffset (Private, Slot);

  Status = FwPartitionCheckOffsetAndBytes (BrBctPartition->PartitionInfo.Bytes,
                                           Offset,
                                           SlotSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: read offset=%llu, bytes=%u error: %r\n",
            __FUNCTION__, Offset, SlotSize, Status));
    return Status;
  }

  Status = DeviceInfo->DeviceRead (DeviceInfo, Offset, SlotSize, Buffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error reading slot=%u: %r\n",
          __FUNCTION__, Slot, Status));
  }

  return Status;
}

/**
  Verify slot's data in BR-BCT partition.

  @param[in]  Private           Pointer to private data structure
  @param[in]  Slot              Slot number
  @param[out] Buffer            Address of data for comparison

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
BrBctVerifySlot (
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  BR_BCT_UPDATE_SLOT                Slot,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer
  )
{
  EFI_STATUS                        Status;

  DEBUG ((DEBUG_INFO, "%a: Verifying slot %u\n", __FUNCTION__, Slot));

  Status = BrBctReadSlot (Private, Slot, mVerifyBuffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (CompareMem (mVerifyBuffer, Buffer, Bytes) != 0) {
    return EFI_VOLUME_CORRUPTED;
  }

  return EFI_SUCCESS;
}

/**
  Write data into slot in BR-BCT partition.

  @param[in]  Private           Pointer to private data structure
  @param[in]  Slot              Slot number
  @param[out] Buffer            Address of data to write

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
BrBctWriteSlot (
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  BR_BCT_UPDATE_SLOT                Slot,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer
  )
{
  FW_PARTITION_PRIVATE_DATA         *BrBctPartition;
  FW_PARTITION_DEVICE_INFO          *DeviceInfo;
  UINT32                            SlotSize;
  UINT64                            Offset;
  EFI_STATUS                        Status;

  DEBUG ((DEBUG_INFO, "%a: Writing slot %u\n", __FUNCTION__, Slot));

  SlotSize          = Private->SlotSize;
  BrBctPartition    = Private->BrBctPartition;
  DeviceInfo        = BrBctPartition->DeviceInfo;
  Offset            = BrBctGetSlotOffset (Private, Slot);

  Status = FwPartitionCheckOffsetAndBytes (BrBctPartition->PartitionInfo.Bytes,
                                           Offset,
                                           SlotSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: write offset=%llu, bytes=%u error: %r\n",
            __FUNCTION__, Offset, SlotSize, Status));
    return Status;
  }

  Status = DeviceInfo->DeviceWrite (DeviceInfo, Offset, Bytes, Buffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error writing slot=%u: %r\n",
          __FUNCTION__, Slot, Status));
  }

  return Status;
}

/**
  Write and verify a slot's data in the BR-BCT partition.
  If PcdBrBctVerifyUpdateBeforeWrite is TRUE, slot data is verified to need
  updating before performing a new write/verify.

  @param[in]  Private           Pointer to private data structure
  @param[in]  Slot              Slot number
  @param[out] Buffer            Address of data to write

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
BrBctWriteAndVerifySlot (
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  BR_BCT_UPDATE_SLOT                Slot,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer
  )
{
  EFI_STATUS                            Status;

  if (mPcdBrBctVerifyUpdateBeforeWrite) {
    Status = BrBctVerifySlot (Private, Slot, Bytes, Buffer);
    if (Status == EFI_SUCCESS) {
      DEBUG ((DEBUG_INFO, "%a: Slot=%u Bytes=%u no update needed\n",
              __FUNCTION__, Slot, Bytes));
      return Status;
    }
  }

  Status = BrBctWriteSlot (Private, Slot, Bytes, Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = BrBctVerifySlot (Private, Slot, Bytes, Buffer);

  return Status;
}

/**
  Copy data from one slot to another in the BR-BCT partition.

  @param[in]  Private           Pointer to private data structure
  @param[in]  OutputSlot        Slot number to write data into
  @param[in]  InputSlot         Slot number to read data from

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
BrBctCopySlot (
  IN  BR_BCT_UPDATE_PRIVATE_DATA        *Private,
  IN  BR_BCT_UPDATE_SLOT                OutputSlot,
  IN  BR_BCT_UPDATE_SLOT                InputSlot
  )
{
  FW_PARTITION_PRIVATE_DATA         *BrBctPartition;
  FW_PARTITION_DEVICE_INFO          *DeviceInfo;
  VOID                              *Buffer;
  EFI_STATUS                        Status;
  UINT32                            SlotSize;

  DEBUG ((DEBUG_INFO, "%a: Copying slot %u to slot %u\n",
          __FUNCTION__, InputSlot, OutputSlot));

  if (((OutputSlot ^ InputSlot) & 0x1) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: InputSlot=%u, OutputSlot=%u, are not both odd or even\n",
          __FUNCTION__, InputSlot, OutputSlot));
    return EFI_INVALID_PARAMETER;
  }

  BrBctPartition    = Private->BrBctPartition;
  DeviceInfo        = BrBctPartition->DeviceInfo;
  SlotSize          = Private->SlotSize;

  Buffer = AllocateZeroPool (SlotSize);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = BrBctReadSlot (Private, InputSlot, Buffer);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = BrBctWriteAndVerifySlot (Private, OutputSlot, SlotSize, Buffer);

Done:
  FreePool (Buffer);
  return Status;
}

// NVIDIA_BR_BCT_UPDATE_PROTOCOL.UpdateBct()
EFI_STATUS
BrBctUpdateBct (
  IN  CONST NVIDIA_BR_BCT_UPDATE_PROTOCOL   *This,
  IN  UINTN                                 Bytes,
  IN  CONST VOID                            *Buffer
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA        *Private;
  EFI_STATUS                        Status;
  UINT32                            SlotSize;

  DEBUG ((DEBUG_INFO, "%a: Bytes=%u\n", __FUNCTION__, Bytes));

  if ((This == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (This,
                BR_BCT_UPDATE_PRIVATE_DATA,
                Protocol,
                BR_BCT_UPDATE_PRIVATE_DATA_SIGNATURE);
  SlotSize = Private->SlotSize;

  if ((Bytes > Private->BrBctPartition->PartitionInfo.Bytes) ||
      (Bytes > SlotSize)) {
    DEBUG ((DEBUG_ERROR, "%a: invalid Bytes=%u\n", __FUNCTION__, Bytes));
    return EFI_INVALID_PARAMETER;
  }

  if ((mActiveBootChain == 0) && PcdGetBool (PcdOverwriteActiveFwPartition)) {
    Status = BrBctWriteAndVerifySlot (Private, BR_BCT_SLOT_A0, Bytes, Buffer);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  } else {
    Status = EFI_UNSUPPORTED;
  }

  return Status;
}

// NVIDIA_BR_BCT_UPDATE_PROTOCOL.UpdateFwChain()
EFI_STATUS
BrBctUpdateFwChain (
  IN  CONST NVIDIA_BR_BCT_UPDATE_PROTOCOL   *This,
  IN  UINTN                                 NewFwChain
  )
{
  BR_BCT_UPDATE_PRIVATE_DATA        *Private;
  EFI_STATUS                        Status;

  DEBUG ((DEBUG_INFO, "%a: ActiveChain=%u, NewFwChain=%u\n",
          __FUNCTION__, mActiveBootChain, NewFwChain));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (NewFwChain == mActiveBootChain) {
    DEBUG ((DEBUG_ERROR, "%a: NewFwChain=%u same as current\n",
            __FUNCTION__, NewFwChain));
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (This,
                BR_BCT_UPDATE_PRIVATE_DATA,
                Protocol,
                BR_BCT_UPDATE_PRIVATE_DATA_SIGNATURE);

  switch (NewFwChain) {
    case 0:
      // switching to A: copy A1 to A0
      Status = BrBctCopySlot (Private, BR_BCT_SLOT_A0, BR_BCT_SLOT_A1);
      break;
    case 1:
      // switching to B: erase A0, but first copy A0 to A1,
      //                 since we probably used A0 to boot
      Status = BrBctCopySlot (Private, BR_BCT_SLOT_A1, BR_BCT_SLOT_A0);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error copying slot A0 to A1 before deleting A0: %r\n",
                __FUNCTION__, Status));
        break;
      }
      Status = BrBctEraseSlot (Private, BR_BCT_SLOT_A0);
      break;
    default:
      Status = EFI_INVALID_PARAMETER;
      break;
  }

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
    ConvertFunction ((VOID **) &Private->DeviceErase);
    ConvertFunction ((VOID **) &Private->Protocol.UpdateBct);
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
  FW_PARTITION_PRIVATE_DATA         *BrBctPartition;
  BR_BCT_UPDATE_PRIVATE_DATA        *Private;

  mActiveBootChain = ActiveBootChain;

  // Find the BCT partition
  BrBctPartition = FwPartitionFindByName (L"BCT");
  if (BrBctPartition == NULL) {
    return EFI_NOT_FOUND;
  }

  // Initialize our private data for the protocol
  Private                           = &mPrivate;
  Private->Signature                = BR_BCT_UPDATE_PRIVATE_DATA_SIGNATURE;
  Private->BrBctPartition           = BrBctPartition;
  Private->SlotSize                 = MAX (EraseBlockSize,
                                           PcdGet32 (PcdBrBctLogicalSlotSize));
  Private->DeviceErase              = DeviceErase;
  Private->Protocol.UpdateBct       = BrBctUpdateBct;
  Private->Protocol.UpdateFwChain   = BrBctUpdateFwChain;

  // Pre-allocate a slot verify buffer to support runtime update of BCT data
  mVerifyBuffer = AllocateRuntimeZeroPool (Private->SlotSize);
  if (mVerifyBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  mPcdBrBctVerifyUpdateBeforeWrite = PcdGetBool (PcdBrBctVerifyUpdateBeforeWrite);
  return EFI_SUCCESS;
}
