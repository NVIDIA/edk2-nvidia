/** @file

  A driver that sends SMBIOS tables to an OpenBMC receiver

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Guid/SmBios.h>

#include <IndustryStandard/SmBios.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Protocol/IpmiBlobTransfer.h>

#define SMBIOS_TRANSFER_DEBUG  0

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  @param[in]  ImageHandle       The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.

**/
EFI_STATUS
EFIAPI
SmbiosBmcTransferEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                   Status;
  SMBIOS_TABLE_ENTRY_POINT     *SmbiosTable;
  IPMI_BLOB_TRANSFER_PROTOCOL  *IpmiBlobTransfer;
  UINT16                       Index;
  UINT16                       SessionId;
  UINT8                        *SendData;
  UINT32                       SendDataSize;
  UINT32                       RemainingDataSize;

  Status = gBS->LocateProtocol (&gNVIDIAIpmiBlobTransferProtocolGuid, NULL, (VOID **)&IpmiBlobTransfer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: No IpmiBlobTransferProtocol available. Exiting\n", __FUNCTION__));
    return Status;
  }

  SmbiosTable = NULL;
  Status      = EfiGetSystemConfigurationTable (&gEfiSmbiosTableGuid, (VOID **)&SmbiosTable);
  if (EFI_ERROR (Status) || (SmbiosTable == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: No SMBIOS Table found: %r\n", __FUNCTION__, Status));
    return EFI_NOT_FOUND;
  }

 #if SMBIOS_TRANSFER_DEBUG
  DEBUG ((DEBUG_INFO, "%a: SMBIOS BINARY DATA OUTPUT\n", __FUNCTION__));
  DEBUG ((DEBUG_INFO, "%a: Table Address: %x\n", __FUNCTION__, SmbiosTable->TableAddress));
  DEBUG ((DEBUG_INFO, "%a: Table Length: %x\n", __FUNCTION__, SmbiosTable->TableLength));
  DEBUG ((DEBUG_INFO, "------------------------------------------\nIndex:0"));
  for (Index = 0; Index < SmbiosTable->TableLength; Index++) {
    DEBUG ((DEBUG_INFO, "%02x ", *((UINT8 *)((UINTN)SmbiosTable->TableAddress + Index))));
    if ((Index % IPMI_OEM_BLOB_MAX_DATA_PER_PACKET) == 0) {
      DEBUG ((DEBUG_INFO, "\nIndex:%x ", Index));
    }
  }

 #endif

  Status = IpmiBlobTransfer->BlobOpen ((CHAR8 *)PcdGetPtr (PcdBmcSmbiosBlobTransferId), BLOB_TRANSFER_STAT_OPEN_W, &SessionId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to open Blob with Id %a: %r\n", __FUNCTION__, PcdGetPtr (PcdBmcSmbiosBlobTransferId), Status));
    return EFI_DEVICE_ERROR;
  }

  SendData          = (UINT8 *)(UINTN)SmbiosTable->TableAddress;
  SendDataSize      = SmbiosTable->TableLength;
  RemainingDataSize = SendDataSize;

  for (Index = 0; Index < (SendDataSize / IPMI_OEM_BLOB_MAX_DATA_PER_PACKET); Index++) {
    Status = IpmiBlobTransfer->BlobWrite (SessionId, Index * IPMI_OEM_BLOB_MAX_DATA_PER_PACKET, SendData, IPMI_OEM_BLOB_MAX_DATA_PER_PACKET);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failure writing to blob: %r\n", __FUNCTION__, Status));
      return EFI_ABORTED;
    }

    SendData           = (UINT8 *)SendData + IPMI_OEM_BLOB_MAX_DATA_PER_PACKET;
    RemainingDataSize -= IPMI_OEM_BLOB_MAX_DATA_PER_PACKET;
  }

  ASSERT (RemainingDataSize < IPMI_OEM_BLOB_MAX_DATA_PER_PACKET);
  if (RemainingDataSize) {
    Status = IpmiBlobTransfer->BlobWrite (SessionId, Index * IPMI_OEM_BLOB_MAX_DATA_PER_PACKET, SendData, RemainingDataSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failure writing to blob: %r\n", __FUNCTION__, Status));
      return EFI_ABORTED;
    }
  }

  Status = IpmiBlobTransfer->BlobCommit (SessionId, 0, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failure sending commit to blob: %r\n", __FUNCTION__, Status));
    return EFI_ABORTED;
  }

  Status = IpmiBlobTransfer->BlobClose (SessionId);
  DEBUG ((DEBUG_INFO, "%a: Exiting: %r\n", __FUNCTION__, Status));
  return Status;
}
