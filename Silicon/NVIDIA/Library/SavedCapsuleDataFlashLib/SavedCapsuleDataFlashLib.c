/** @file

  Saved capsule data flash library

  SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Guid/SystemResourceTable.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Protocol/FwPartitionProtocol.h>

#define FMP_WRITE_LOOP_SIZE  (64 * 1024)

STATIC   NVIDIA_FW_PARTITION_PROTOCOL  *mFwPartitionProtocol = NULL;
STATIC   UINT64                        mPartitionSize        = 0;
STATIC   BOOLEAN                       mInitialized          = FALSE;
STATIC   EFI_EVENT                     mAddressChangeEvent   = NULL;

STATIC
VOID
EFIAPI
AddressChangeNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EfiConvertPointer (0x0, (VOID **)&mFwPartitionProtocol);
}

/**
  Store the capsule for access after reset

  @param[in]  CapsuleData       Pointer to capsule data
  @param[in]  Size              Size of capsule data

  @retval EFI_SUCCESS           Operation completed successfully
  @retval Others                An error occurred

**/
EFI_STATUS
EFIAPI
CapsuleStore (
  IN VOID   *CapsuleData,
  IN UINTN  Size
  )
{
  EFI_STATUS  Status;
  VOID        *Data;
  UINTN       WriteOffset;
  UINTN       WriteBytes;
  UINTN       BytesPerLoop;

  if (!mInitialized) {
    DEBUG ((DEBUG_ERROR, "%a: lib not initialized\n", __FUNCTION__));
    return EFI_NOT_READY;
  }

  if (Size > mPartitionSize) {
    DEBUG ((DEBUG_ERROR, "%a: Capsule size %u is larger than partition size %llu\n", __FUNCTION__, Size, mPartitionSize));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Store capsule image to SPI
  //
  BytesPerLoop = FMP_WRITE_LOOP_SIZE;
  WriteOffset  = 0x00;
  WriteBytes   = Size;
  Data         = (UINT8 *)CapsuleData;
  while (WriteBytes > 0) {
    UINTN  WriteSize;

    WriteSize = (WriteBytes > BytesPerLoop) ? BytesPerLoop : WriteBytes;
    Status    = mFwPartitionProtocol->Write (
                                        mFwPartitionProtocol,
                                        WriteOffset,
                                        WriteSize,
                                        Data
                                        );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    WriteOffset += WriteSize;
    WriteBytes  -= WriteSize;
    Data        += WriteSize;
  }

  return EFI_SUCCESS;
}

/**
  Load a saved capsule into buffer

  @param[in]  Buffer            Pointer to buffer to load capsule
  @param[in]  Size              Size of capsule buffer

  @retval EFI_SUCCESS           Operation completed successfully
  @retval Others                An error occurred

**/
EFI_STATUS
EFIAPI
CapsuleLoad (
  IN VOID   *Buffer,
  IN UINTN  Size
  )
{
  EFI_STATUS  Status;
  VOID        *Data;
  UINTN       ReadOffset;
  UINTN       ReadBytes;

  if (!mInitialized) {
    DEBUG ((DEBUG_ERROR, "%a: lib not initialized\n", __FUNCTION__));
    return EFI_NOT_READY;
  }

  if (Size > mPartitionSize) {
    DEBUG ((DEBUG_ERROR, "%a: request size %u is larger than partition size %llu\n", __FUNCTION__, Size, mPartitionSize));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Load capsule image to buffer
  //
  Data       = (UINT8 *)Buffer;
  ReadOffset = 0x00;
  ReadBytes  = Size;

  while (ReadBytes > 0) {
    UINTN  ReadSize;

    ReadSize   = (ReadBytes > FMP_WRITE_LOOP_SIZE) ? FMP_WRITE_LOOP_SIZE : ReadBytes;
    ReadBytes -= ReadSize;

    Status = mFwPartitionProtocol->Read (
                                     mFwPartitionProtocol,
                                     ReadOffset,
                                     ReadSize,
                                     Data
                                     );
    DEBUG ((DEBUG_VERBOSE, "%a: 0x%x size 0x%x to Data 0x%p %r. ReadBytes = 0x%x\n", __FUNCTION__, ReadOffset, ReadSize, Data, Status, ReadBytes));
    if (EFI_ERROR (Status)) {
      return EFI_ABORTED;
    }

    ReadOffset += ReadSize;
    Data       += ReadSize;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SavedCapsuleLibInitialize (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    *Handles = NULL;
  UINTN                         HandleCount;
  NVIDIA_FW_PARTITION_PROTOCOL  *FwPartitionProtocol;
  BOOLEAN                       IsProtocolFound;
  FW_PARTITION_ATTRIBUTES       Attributes;

  //
  // Get MM-NorFlash FwPartitionProtocol
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAFwPartitionProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get FW Partition protocol\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  do {
    HandleCount--;
    Status = gBS->HandleProtocol (
                    Handles[HandleCount],
                    &gNVIDIAFwPartitionProtocolGuid,
                    (VOID **)&FwPartitionProtocol
                    );
    if (!EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: PartitionName = %s\n", __FUNCTION__, FwPartitionProtocol->PartitionName));
      if (StrCmp (FwPartitionProtocol->PartitionName, L"MM-Capsule") == 0) {
        Status = FwPartitionProtocol->GetAttributes (FwPartitionProtocol, &Attributes);
        if (!EFI_ERROR (Status) && (Attributes.Bytes != 0)) {
          mPartitionSize  = Attributes.Bytes;
          IsProtocolFound = TRUE;
        }

        break;
      }
    }
  } while (HandleCount > 0);

  if (!IsProtocolFound) {
    DEBUG ((DEBUG_ERROR, "%a: Cannot find FW Partition.\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  AddressChangeNotify,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mAddressChangeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error creating address change event: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  mFwPartitionProtocol = FwPartitionProtocol;
  mInitialized         = TRUE;

  Status = EFI_SUCCESS;
CleanupAndReturn:
  if (Handles != NULL) {
    FreePool (Handles);
  }

  return Status;
}
