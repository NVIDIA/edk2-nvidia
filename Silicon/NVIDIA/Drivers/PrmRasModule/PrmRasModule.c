/** @file

  PRM Module for CPER error dump

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PrmModule.h>

#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/PlatformResourceLib.h>

#include <Protocol/FwPartitionProtocol.h>
#include "PrmRasModule.h"

STATIC   NVIDIA_FW_PARTITION_PROTOCOL  *mFwPartitionProtocol = NULL;
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
  A Platform Runtime Mechanism (PRM) handler.

  @param[in]  ParameterBuffer     A pointer to the PRM handler parameter buffer
  @param[in]  ContextBUffer       A pointer to the PRM handler context buffer

  @retval EFI_STATUS              The PRM handler executed successfully.
  @retval Others                  An error occurred in the PRM handler.

**/
PRM_HANDLER_EXPORT (RasPrmHandler) {
  EFI_STATUS                                 Status;
  PRM_RAS_MODULE_STATIC_DATA_CONTEXT_BUFFER  *RasDataBuffer;
  UINTN                                      ReadOffset;
  UINTN                                      ReadSize;
  UINT8                                      *ReadData;
  FW_PARTITION_ATTRIBUTES                    Attributes;

  if (ContextBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ContextBuffer->StaticDataBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Verify PRM data buffer signature is valid
  //
  if (
      (ContextBuffer->Signature != PRM_CONTEXT_BUFFER_SIGNATURE) ||
      (ContextBuffer->StaticDataBuffer->Header.Signature != PRM_DATA_BUFFER_HEADER_SIGNATURE))
  {
    return EFI_NOT_FOUND;
  }

  if (mFwPartitionProtocol == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: FW Partition protocol not found\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  Status = mFwPartitionProtocol->GetAttributes (mFwPartitionProtocol, &Attributes);
  if (EFI_ERROR (Status) || (Attributes.Bytes == 0)) {
    return EFI_UNSUPPORTED;
  }

  RasDataBuffer = (PRM_RAS_MODULE_STATIC_DATA_CONTEXT_BUFFER *)&ContextBuffer->StaticDataBuffer->Data[0];

  // 64K alignment
  RasDataBuffer->PartitionOffset &= 0xFFFFFFFFFFFF0000;

  if (RasDataBuffer->PartitionOffset + PRM_SPI_ACCESS_DATA_SIZE > Attributes.Bytes) {
    DEBUG ((DEBUG_ERROR, "%a: Wrong partition offset input\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  RasDataBuffer->PartitionSize = Attributes.Bytes;
  RasDataBuffer->DataSize      = PRM_SPI_ACCESS_DATA_SIZE;

  // Read SPI data
  ReadOffset = RasDataBuffer->PartitionOffset;
  ReadSize   = RasDataBuffer->DataSize;
  ReadData   = RasDataBuffer->CperData;

  // Use NVIDIA_FW_PARTITION_PROTOCOL.PrmRead to read SPI via the new mailbox
  Status = mFwPartitionProtocol->PrmRead (
                                   mFwPartitionProtocol,
                                   ReadOffset,
                                   ReadSize,
                                   ReadData
                                   );
  if (EFI_ERROR (Status)) {
    return EFI_ABORTED;
  }

  return EFI_SUCCESS;
}

//
// Register the PRM export information for this PRM Module
//
PRM_MODULE_EXPORT (
  PRM_HANDLER_EXPORT_ENTRY (NVIDIA_RAS_PRM_HANDLER_GUID, RasPrmHandler)
  );

/**
  Module entry point.

  @param[in]   ImageHandle     The image handle.
  @param[in]   SystemTable     A pointer to the system table.

  @retval  EFI_SUCCESS         This function always returns success.

**/
EFI_STATUS
EFIAPI
PrmRasModuleInit (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    *Handles;
  UINTN                         HandleCount;
  NVIDIA_FW_PARTITION_PROTOCOL  *FwPartitionProtocol;
  BOOLEAN                       IsProtocolFound;

  IsProtocolFound = FALSE;
  HandleCount     = 0;

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
      DEBUG ((DEBUG_ERROR, "%a: PartitionName = %s\n", __FUNCTION__, FwPartitionProtocol->PartitionName));
      if (StrCmp (FwPartitionProtocol->PartitionName, L"MM-RAS") == 0) {
        IsProtocolFound = TRUE;
        break;
      }
    }
  } while (HandleCount > 0);

  if (!IsProtocolFound) {
    DEBUG ((DEBUG_ERROR, "%a: Cannot find FW Partition.\n", __FUNCTION__));
    return EFI_NOT_FOUND;
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
    return Status;
  }

  mFwPartitionProtocol = FwPartitionProtocol;

  return EFI_SUCCESS;
}
