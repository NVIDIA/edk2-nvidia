/** @file

  A driver that sends SMBIOS tables to an OpenBMC receiver

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Guid/SmBios.h>

#include <IndustryStandard/SmBios.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Protocol/IpmiBlobTransfer.h>

#define SMBIOS_TRANSFER_DEBUG  0

/**
  This function will send all installed SMBIOS tables to the BMC

  @param  Event    The event of notify protocol.
  @param  Context  Notify event context.
**/
VOID
EFIAPI
SmbiosBmcTransferSendTables (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                    Status;
  SMBIOS_TABLE_3_0_ENTRY_POINT  *Smbios30Table;
  SMBIOS_TABLE_3_0_ENTRY_POINT  *Smbios30TableModified;
  IPMI_BLOB_TRANSFER_PROTOCOL   *IpmiBlobTransfer;
  UINT16                        Index;
  UINT16                        SessionId;
  UINT8                         *SendData;
  UINT32                        SendDataSize;
  UINT32                        RemainingDataSize;

  gBS->CloseEvent (Event);

  Status = gBS->LocateProtocol (&gNVIDIAIpmiBlobTransferProtocolGuid, NULL, (VOID **)&IpmiBlobTransfer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: No IpmiBlobTransferProtocol available. Exiting\n", __FUNCTION__));
    return;
  }

  Smbios30Table = NULL;
  Status        = EfiGetSystemConfigurationTable (&gEfiSmbios3TableGuid, (VOID **)&Smbios30Table);
  if (EFI_ERROR (Status) || (Smbios30Table == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: No SMBIOS Table found: %r\n", __FUNCTION__, Status));
    return;
  }

  //
  // BMC expects the Smbios Entry Point to point to the address within the binary data sent
  // The value is initially pointing to the location in memory where the table lives
  // So we will save off that value and then modify the entry point to make the BMC happy
  Smbios30TableModified = AllocateZeroPool (sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT));
  CopyMem (Smbios30TableModified, Smbios30Table, sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT));
  Smbios30TableModified->TableAddress = sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT);
  //
  // Fixup checksums in the Entry Point Structure
  //
  Smbios30TableModified->EntryPointStructureChecksum = 0;
  Smbios30TableModified->EntryPointStructureChecksum =
    CalculateCheckSum8 ((UINT8 *)Smbios30TableModified, Smbios30TableModified->EntryPointLength);

  SendData = AllocateZeroPool (sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT) + Smbios30Table->TableMaximumSize);
  CopyMem (SendData, Smbios30TableModified, sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT));
  CopyMem (SendData + sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT), (UINT8 *)Smbios30Table->TableAddress, Smbios30Table->TableMaximumSize);
  SendDataSize      = sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT) + Smbios30Table->TableMaximumSize;
  RemainingDataSize = SendDataSize;

 #if SMBIOS_TRANSFER_DEBUG
  DEBUG ((DEBUG_INFO, "%a: SMBIOS BINARY DATA OUTPUT\n", __FUNCTION__));
  DEBUG ((DEBUG_INFO, "%a: Table Address: %x\n", __FUNCTION__, Smbios30Table->TableAddress));
  DEBUG ((DEBUG_INFO, "%a: Table Length: %x\n", __FUNCTION__, Smbios30Table->TableMaximumSize));
  for (Index = 0; Index < SendDataSize; Index++) {
    if ((Index % IPMI_OEM_BLOB_MAX_DATA_PER_PACKET) == 0) {
      DEBUG ((DEBUG_INFO, "\nIndex:%x ", Index));
    }

    DEBUG ((DEBUG_INFO, "%02x ", *(SendData + Index)));
  }

 #endif

  Status = IpmiBlobTransfer->BlobOpen ((CHAR8 *)PcdGetPtr (PcdBmcSmbiosBlobTransferId), BLOB_TRANSFER_STAT_OPEN_W, &SessionId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to open Blob with Id %a: %r\n", __FUNCTION__, PcdGetPtr (PcdBmcSmbiosBlobTransferId), Status));
    return;
  }

  for (Index = 0; Index < (SendDataSize / IPMI_OEM_BLOB_MAX_DATA_PER_PACKET); Index++) {
    Status = IpmiBlobTransfer->BlobWrite (SessionId, Index * IPMI_OEM_BLOB_MAX_DATA_PER_PACKET, SendData, IPMI_OEM_BLOB_MAX_DATA_PER_PACKET);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failure writing to blob: %r\n", __FUNCTION__, Status));
      return;
    }

    SendData           = (UINT8 *)SendData + IPMI_OEM_BLOB_MAX_DATA_PER_PACKET;
    RemainingDataSize -= IPMI_OEM_BLOB_MAX_DATA_PER_PACKET;
  }

  ASSERT (RemainingDataSize < IPMI_OEM_BLOB_MAX_DATA_PER_PACKET);
  if (RemainingDataSize) {
    Status = IpmiBlobTransfer->BlobWrite (SessionId, Index * IPMI_OEM_BLOB_MAX_DATA_PER_PACKET, SendData, RemainingDataSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failure writing final block to blob: %r\n", __FUNCTION__, Status));
      return;
    }
  }

  Status = IpmiBlobTransfer->BlobCommit (SessionId, 0, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failure sending commit to blob: %r\n", __FUNCTION__, Status));
    return;
  }

  Status = IpmiBlobTransfer->BlobClose (SessionId);
  DEBUG ((DEBUG_ERROR, "%a: Sent SMBIOS Tables to BMC: %r\n", __FUNCTION__, Status));
  return;
}

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
  EFI_STATUS  Status;
  EFI_EVENT   ReadyToBootEvent;

  //
  // Register ReadyToBoot event to send the SMBIOS tables once they have all been installed
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  SmbiosBmcTransferSendTables,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &ReadyToBootEvent
                  );

  ASSERT_EFI_ERROR (Status);
  return Status;
}
