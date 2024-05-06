/** @file

  MCTP protocol standalone MM

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MmServicesTableLib.h>
#include <Protocol/MctpProtocol.h>
#include "MctpMmCommMsgs.h"

#define MCTP_MM_MAX_DEVICES  4

NVIDIA_MCTP_PROTOCOL  *mProtocols[MCTP_MM_MAX_DEVICES];
UINTN                 mNumDevices;
EFI_HANDLE            mHandlerHandle;

/**
  Initialize MCTP protocol interfaces.

  @param[out] NumDevices            Pointer to return number of MCTP devices found.
  @retval EFI_SUCCESS              Operation successful.
  @retval Others                   Error occurred.

**/
STATIC
EFI_STATUS
EFIAPI
MctpMmInitProtocols  (
  OUT UINTN  *NumDevices
  )
{
  EFI_STATUS  Status;
  UINTN       Index;
  UINTN       NumHandles;
  UINTN       HandleBufferSize;
  EFI_HANDLE  HandleBuffer[MCTP_MM_MAX_DEVICES];

  mNumDevices      = 0;
  HandleBufferSize = sizeof (HandleBuffer);
  Status           = gMmst->MmLocateHandle (
                              ByProtocol,
                              &gNVIDIAMctpProtocolGuid,
                              NULL,
                              &HandleBufferSize,
                              HandleBuffer
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error locating MCTP handles: %r\n", __FUNCTION__, Status));
    return EFI_NOT_FOUND;
  }

  NumHandles = HandleBufferSize / sizeof (EFI_HANDLE);
  for (Index = 0; Index < NumHandles; Index++) {
    Status = gMmst->MmHandleProtocol (
                      HandleBuffer[Index],
                      &gNVIDIAMctpProtocolGuid,
                      (VOID **)&mProtocols[Index]
                      );
    if (EFI_ERROR (Status) || (mProtocols[Index] == NULL)) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to get MCTP protocol for handle index %u: %r\n",
        Index,
        Status
        ));
      continue;
    }

    mNumDevices++;
  }

  *NumDevices = mNumDevices;
  DEBUG ((DEBUG_INFO, "%a: Found %u devices\n", __FUNCTION__, mNumDevices));

  return EFI_SUCCESS;
}

// EFI_MM_HANDLER_ENTRY_POINT
EFI_STATUS
EFIAPI
MctpMmHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  MCTP_COMM_HEADER        *MctpCommHeader;
  UINTN                   PayloadSize;
  EFI_STATUS              Status;
  NVIDIA_MCTP_PROTOCOL    *Protocol;
  UINTN                   Index;
  MCTP_MM_DEVICE_INFO     *DeviceInfo;
  MCTP_DEVICE_ATTRIBUTES  Attributes;

  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_INVALID_PARAMETER));
    return EFI_SUCCESS;
  }

  if (*CommBufferSize < sizeof (MCTP_COMM_HEADER)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_BUFFER_TOO_SMALL));
    return EFI_SUCCESS;
  }

  MctpCommHeader = (MCTP_COMM_HEADER *)CommBuffer;
  DEBUG ((DEBUG_INFO, "%a: Func=%u\n", __FUNCTION__, MctpCommHeader->Function));

  PayloadSize                  = *CommBufferSize - MCTP_COMM_HEADER_SIZE;
  MctpCommHeader->ReturnStatus = EFI_DEVICE_ERROR;
  switch (MctpCommHeader->Function) {
    case MCTP_COMM_FUNCTION_INITIALIZE:
    {
      MCTP_COMM_INITIALIZE  *Payload = (MCTP_COMM_INITIALIZE *)MctpCommHeader->Data;
      if (PayloadSize < sizeof (MCTP_COMM_INITIALIZE)) {
        DEBUG ((DEBUG_ERROR, "%a: Command [%d], payload buffer invalid!\n", __FUNCTION__, MctpCommHeader->Function));
        MctpCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      Status                       = MctpMmInitProtocols (&Payload->NumDevices);
      MctpCommHeader->ReturnStatus = Status;
      break;
    }

    case MCTP_COMM_FUNCTION_GET_DEVICES:
    {
      MCTP_COMM_GET_DEVICES  *Payload = (MCTP_COMM_GET_DEVICES *)MctpCommHeader->Data;
      if ((PayloadSize != OFFSET_OF (MCTP_COMM_GET_DEVICES, Devices) +
           (Payload->MaxCount * sizeof (Payload->Devices))) ||
          (mNumDevices > Payload->MaxCount))
      {
        DEBUG ((DEBUG_ERROR, "%a: Command [%d], payload buffer invalid!\n", __FUNCTION__, MctpCommHeader->Function));
        MctpCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      DeviceInfo = Payload->Devices;
      for (Index = 0; Index < mNumDevices; Index++, DeviceInfo++) {
        Protocol = mProtocols[Index];
        Protocol->GetDeviceAttributes (Protocol, &Attributes);

        DeviceInfo->MmIndex = Index;
        DeviceInfo->Type    = Attributes.DeviceType;
        DeviceInfo->Socket  = Attributes.Socket;
        StrCpyS (DeviceInfo->Name, sizeof (DeviceInfo->Name), Attributes.DeviceName);
      }

      Payload->Count = mNumDevices;

      MctpCommHeader->ReturnStatus = EFI_SUCCESS;
      break;
    }

    case MCTP_COMM_FUNCTION_SEND:
    {
      MCTP_COMM_SEND  *Payload = (MCTP_COMM_SEND *)MctpCommHeader->Data;
      if ((PayloadSize != OFFSET_OF (MCTP_COMM_SEND, Data) + Payload->Length) ||
          (Payload->MmIndex >= mNumDevices))
      {
        DEBUG ((DEBUG_ERROR, "%a: Command [%d], payload buffer invalid!\n", __FUNCTION__, MctpCommHeader->Function));
        MctpCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      Protocol = mProtocols[Payload->MmIndex];
      Protocol->GetDeviceAttributes (Protocol, &Attributes);

      Payload->ReqMsgTag = Payload->RspMsgTag;

      DEBUG ((
        DEBUG_INFO,
        "%a: send %s IsReq=%u Length=%u\n",
        __FUNCTION__,
        Attributes.DeviceName,
        Payload->IsRequest,
        Payload->Length
        ));

      Status = Protocol->Send (
                           Protocol,
                           Payload->IsRequest,
                           Payload->Data,
                           Payload->Length,
                           &Payload->ReqMsgTag
                           );

      MctpCommHeader->ReturnStatus = Status;
      break;
    }

    case MCTP_COMM_FUNCTION_RECV:
    {
      MCTP_COMM_RECV  *Payload = (MCTP_COMM_RECV *)MctpCommHeader->Data;
      if ((PayloadSize != OFFSET_OF (MCTP_COMM_RECV, Data) + Payload->MaxLength) ||
          (Payload->MmIndex >= mNumDevices))
      {
        DEBUG ((DEBUG_ERROR, "%a: Command [%d], payload buffer invalid!\n", __FUNCTION__, MctpCommHeader->Function));
        MctpCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      Protocol = mProtocols[Payload->MmIndex];
      Protocol->GetDeviceAttributes (Protocol, &Attributes);

      Payload->Length = Payload->MaxLength;

      DEBUG ((
        DEBUG_INFO,
        "%a: recv %s TO=%ums MaxLength=%u\n",
        __FUNCTION__,
        Attributes.DeviceName,
        Payload->TimeoutMs,
        Payload->MaxLength
        ));

      Status = Protocol->Recv (
                           Protocol,
                           Payload->TimeoutMs,
                           Payload->Data,
                           &Payload->Length,
                           &Payload->MsgTag
                           );

      MctpCommHeader->ReturnStatus = Status;
      break;
    }

    case MCTP_COMM_FUNCTION_DO_REQUEST:
    {
      MCTP_COMM_DO_REQUEST  *Payload = (MCTP_COMM_DO_REQUEST *)MctpCommHeader->Data;
      if ((PayloadSize != OFFSET_OF (MCTP_COMM_DO_REQUEST, Data) + MAX (Payload->RequestLength, Payload->ResponseBufferLength)) ||
          (Payload->MmIndex >= mNumDevices))
      {
        DEBUG ((DEBUG_ERROR, "%a: Command [%d], payload buffer invalid!\n", __FUNCTION__, MctpCommHeader->Function));
        MctpCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      Protocol = mProtocols[Payload->MmIndex];
      Protocol->GetDeviceAttributes (Protocol, &Attributes);

      DEBUG ((
        DEBUG_INFO,
        "%a: dorequest %s ReqLen=%u RspBufLen=%u\n",
        __FUNCTION__,
        Attributes.DeviceName,
        Payload->RequestLength,
        Payload->ResponseBufferLength
        ));

      Status = Protocol->DoRequest (
                           Protocol,
                           Payload->Data,
                           Payload->RequestLength,
                           Payload->Data,
                           Payload->ResponseBufferLength,
                           &Payload->ResponseLength
                           );

      MctpCommHeader->ReturnStatus = Status;
      break;
    }

    default:
      MctpCommHeader->ReturnStatus = EFI_UNSUPPORTED;
      break;
  }

  DEBUG ((DEBUG_INFO, "%a: Func=%u ReturnStatus=%u\n", __FUNCTION__, MctpCommHeader->Function, MctpCommHeader->ReturnStatus));

  return EFI_SUCCESS;
}

/**
  Initialize the MCTP standalone MM driver

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

**/
EFI_STATUS
EFIAPI
MctpStandaloneMmInitialize (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS  Status;

  mHandlerHandle = NULL;
  Status         = gMmst->MmiHandlerRegister (
                            MctpMmHandler,
                            &gNVIDIAMctpProtocolGuid,
                            &mHandlerHandle
                            );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
