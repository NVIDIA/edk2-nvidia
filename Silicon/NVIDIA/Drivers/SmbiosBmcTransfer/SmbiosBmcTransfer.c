/** @file

  A driver that sends SMBIOS tables to an OpenBMC receiver

  SPDX-FileCopyrightText: copyright (c) 2022-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Guid/SmBios.h>

#include <IndustryStandard/SmBios.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Guid/NVIDIAPublicVariableGuid.h>

#include <Protocol/IpmiBlobTransfer.h>
#include <Library/BaseCryptLib.h>

#include <NVIDIAStatusCodes.h>
#include <OemStatusCodes.h>

#define SMBIOS_TRANSFER_DEBUG  0

/**
  This function will calculate smbios hash, compares it with stored
  hash, updates UEFI variable if smbios data changed and return status.

  @param[in]    The smbios data to detect if changed.
  @param[in]    The smbios data size.
  @retval TRUE  If smbios data changed, otherwise FALSE.
**/
BOOLEAN
DetectSmbiosChange (
  UINT8   *SmbiosData,
  UINT32  SmbiosDataSize
  )
{
  BOOLEAN     UpdateRequired = TRUE;
  BOOLEAN     DeleteVar      = FALSE;
  BOOLEAN     Response;
  UINT8       ComputedHashValue[SHA256_DIGEST_SIZE];
  UINT8       StoredHashValue[SHA256_DIGEST_SIZE];
  UINTN       StoredHashValueSize;
  EFI_STATUS  Status;

  Response = Sha256HashAll ((VOID *)SmbiosData, SmbiosDataSize, ComputedHashValue);

  if (Response == FALSE) {
    // Delete the SmbiosHash variable and continue with smbios transfer to BMC
    Status = gRT->SetVariable (
                    L"SmbiosHash",
                    &gNVIDIAPublicVariableGuid,
                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                    0,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to delete UEFI Variable SmbiosHash %r\n", __FUNCTION__, Status));
    }

    DeleteVar = TRUE;
  } else {
    StoredHashValueSize = SHA256_DIGEST_SIZE;
    // Get the stored smbios data hash
    Status = gRT->GetVariable (
                    L"SmbiosHash",
                    &gNVIDIAPublicVariableGuid,
                    NULL,
                    &StoredHashValueSize,
                    (VOID *)StoredHashValue
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get UEFI Variable SmbiosHash %r\n", __FUNCTION__, Status));
    } else {
      // Compare StoredHash with currently ComputedHash
      if (StoredHashValueSize != SHA256_DIGEST_SIZE) {
        DEBUG ((DEBUG_ERROR, "%a: Invalid Hash Size %u\n", __FUNCTION__, StoredHashValueSize));
      }

      if (CompareMem (StoredHashValue, ComputedHashValue, SHA256_DIGEST_SIZE) == 0) {
        DEBUG ((DEBUG_INFO, "%a: Same Keys , Hash values match\n", __FUNCTION__));
        UpdateRequired = FALSE;
      }
    }
  }

  if (UpdateRequired == FALSE) {
    return UpdateRequired;
  }

  if (DeleteVar == FALSE) {
    // ComputedHashValue is valid
    // store the computed hash and transfer smbios tables if smbios hash variable not found or hash mismatched.
    Status = gRT->SetVariable (
                    L"SmbiosHash",
                    &gNVIDIAPublicVariableGuid,
                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                    SHA256_DIGEST_SIZE,
                    (VOID *)ComputedHashValue
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set UEFI Variable SmbiosHash %r\n", __FUNCTION__, Status));
    }
  }

  return UpdateRequired;
}

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
  BOOLEAN                       SmbiosTransferRequired;

  gBS->CloseEvent (Event);

  Status = gBS->LocateProtocol (&gNVIDIAIpmiBlobTransferProtocolGuid, NULL, (VOID **)&IpmiBlobTransfer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: No IpmiBlobTransferProtocol available. Exiting\n", __FUNCTION__));
    goto ErrorExit;
  }

  Smbios30Table = NULL;
  Status        = EfiGetSystemConfigurationTable (&gEfiSmbios3TableGuid, (VOID **)&Smbios30Table);
  if (EFI_ERROR (Status) || (Smbios30Table == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: No SMBIOS Table found: %r\n", __FUNCTION__, Status));
    REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
      EFI_ERROR_CODE | EFI_ERROR_MAJOR,
      EFI_CLASS_NV_FIRMWARE | EFI_NV_FW_UEFI_EC_NO_SMBIOS_TABLE,
      OEM_EC_DESC_NO_SMBIOS_TABLE,
      sizeof (OEM_EC_DESC_NO_SMBIOS_TABLE)
      );
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

  SmbiosTransferRequired = DetectSmbiosChange (SendData, SendDataSize);

  if (SmbiosTransferRequired == FALSE) {
    DEBUG ((DEBUG_INFO, "%a: Smbios tables are not changed, skipping transfer to BMC\n", __FUNCTION__));
    return;
  }

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
    if (Status == EFI_UNSUPPORTED) {
      return;
    }

    DEBUG ((DEBUG_ERROR, "%a: Unable to open Blob with Id %a: %r\n", __FUNCTION__, PcdGetPtr (PcdBmcSmbiosBlobTransferId), Status));
    goto ErrorExit;
  }

  for (Index = 0; Index < (SendDataSize / IPMI_OEM_BLOB_MAX_DATA_PER_PACKET); Index++) {
    Status = IpmiBlobTransfer->BlobWrite (SessionId, Index * IPMI_OEM_BLOB_MAX_DATA_PER_PACKET, SendData, IPMI_OEM_BLOB_MAX_DATA_PER_PACKET);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failure writing to blob: %r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }

    SendData           = (UINT8 *)SendData + IPMI_OEM_BLOB_MAX_DATA_PER_PACKET;
    RemainingDataSize -= IPMI_OEM_BLOB_MAX_DATA_PER_PACKET;
  }

  ASSERT (RemainingDataSize < IPMI_OEM_BLOB_MAX_DATA_PER_PACKET);
  if (RemainingDataSize) {
    Status = IpmiBlobTransfer->BlobWrite (SessionId, Index * IPMI_OEM_BLOB_MAX_DATA_PER_PACKET, SendData, RemainingDataSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failure writing final block to blob: %r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  }

  Status = IpmiBlobTransfer->BlobCommit (SessionId, 0, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failure sending commit to blob: %r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  Status = IpmiBlobTransfer->BlobClose (SessionId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Sent SMBIOS Tables to BMC: %r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  return;

ErrorExit:
  REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
    EFI_ERROR_CODE | EFI_ERROR_MAJOR,
    EFI_CLASS_NV_FIRMWARE | EFI_NV_FW_UEFI_EC_SMBIOS_TRANSFER_FAILED,
    OEM_EC_DESC_SMBIOS_TRANSFER_FAILED,
    sizeof (OEM_EC_DESC_SMBIOS_TRANSFER_FAILED)
    );
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
