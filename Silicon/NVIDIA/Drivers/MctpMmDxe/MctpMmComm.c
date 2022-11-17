/** @file

  MM MCTP protocol communication

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2010 - 2019, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "MctpMmDxe.h"

/**
  Initialize comm buffer for MM communicate transaction.

  @param[out] DataPtr           Pointer to comm buffer client payload. OPTIONAL
  @param[in]  DataSize          Size of client data.
  @param[in]  Function          Function code to execute in MM.

  @retval EFI_SUCCESS           Operation successful.
  @retval Others                Error occurred.

**/
STATIC
EFI_STATUS
EFIAPI
MctpMmInitCommBuffer (
  OUT     VOID   **DataPtr OPTIONAL,
  IN      UINTN  DataSize,
  IN      UINTN  Function
  )
{
  EFI_MM_COMMUNICATE_HEADER  *MmCommHeader;
  MCTP_COMM_HEADER           *MctpCommHeader;

  if (DataSize + OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) +
      MCTP_COMM_HEADER_SIZE > MCTP_COMM_BUFFER_SIZE)
  {
    DEBUG ((DEBUG_ERROR, "%a: size=%u error\n", __FUNCTION__, DataSize));
    return EFI_INVALID_PARAMETER;
  }

  MmCommHeader = (EFI_MM_COMMUNICATE_HEADER *)mMctpMmCommBuffer;
  CopyGuid (&MmCommHeader->HeaderGuid, &gNVIDIAMctpProtocolGuid);
  MmCommHeader->MessageLength = DataSize + MCTP_COMM_HEADER_SIZE;

  MctpCommHeader               = (MCTP_COMM_HEADER *)MmCommHeader->Data;
  MctpCommHeader->Function     = Function;
  MctpCommHeader->ReturnStatus = EFI_PROTOCOL_ERROR;
  if (DataPtr != NULL) {
    *DataPtr = MctpCommHeader->Data;
  }

  return EFI_SUCCESS;
}

/**
  Send comm buffer to MM for processing

  @param[in]  DataSize          Size of client data.

  @retval EFI_SUCCESS           Operation successful.
  @retval Others                Error occurred.

**/
STATIC
EFI_STATUS
EFIAPI
MctpMmSendCommBuffer (
  IN      UINTN  DataSize
  )
{
  EFI_STATUS                 Status;
  UINTN                      CommSize;
  EFI_MM_COMMUNICATE_HEADER  *CommHeader;
  MCTP_COMM_HEADER           *MctpCommHeader;

  CommSize = DataSize + OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) +
             MCTP_COMM_HEADER_SIZE;

  DEBUG ((DEBUG_INFO, "%a: doing communicate\n", __FUNCTION__));
  Status = mMctpMmCommProtocol->Communicate (
                                  mMctpMmCommProtocol,
                                  mMctpMmCommBufferPhysical,
                                  mMctpMmCommBuffer,
                                  &CommSize
                                  );
  DEBUG ((
    DEBUG_INFO,
    "%a: communicate returned: %r\n",
    __FUNCTION__,
    Status
    ));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CommHeader     = (EFI_MM_COMMUNICATE_HEADER *)mMctpMmCommBuffer;
  MctpCommHeader = (MCTP_COMM_HEADER *)CommHeader->Data;
  return MctpCommHeader->ReturnStatus;
}

/**
  Send MCTP initialize command.

  @param[out] NumDevices        Pointer to return number of devices.

  @retval EFI_SUCCESS           Operation successful.
  @retval Others                Error occurred.

**/
EFI_STATUS
EFIAPI
MctpMmSendInitialize  (
  OUT UINTN  *NumDevices
  )
{
  MCTP_COMM_INITIALIZE  *Payload;
  UINTN                 PayloadSize;
  EFI_STATUS            Status;

  PayloadSize = sizeof (MCTP_COMM_INITIALIZE);
  Status      = MctpMmInitCommBuffer (
                  (VOID **)&Payload,
                  PayloadSize,
                  MCTP_COMM_FUNCTION_INITIALIZE
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (Payload != NULL);
  ZeroMem (Payload, sizeof (*Payload));

  Status = MctpMmSendCommBuffer (PayloadSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error initializing MM: %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  // reply fields
  *NumDevices = Payload->NumDevices;

  return Status;
}

/**
  Send MCTP get devices command.

  @param[in]  MaxCount          Maximum count of devices to return.
  @param[out] Count             Pointer to return device count.
  @param[out] DeviceInfo        Pointer to return device info structures.

  @retval EFI_SUCCESS           Operation successful.
  @retval Others                Error occurred.

**/
EFI_STATUS
EFIAPI
MctpMmSendGetDevices  (
  IN  UINTN                MaxCount,
  OUT UINTN                *Count,
  OUT MCTP_MM_DEVICE_INFO  *DeviceInfo
  )
{
  MCTP_COMM_GET_DEVICES  *Payload;
  UINTN                  PayloadSize;
  EFI_STATUS             Status;

  PayloadSize = OFFSET_OF (MCTP_COMM_GET_DEVICES, Devices) +
                (MaxCount * sizeof (Payload->Devices));

  Status = MctpMmInitCommBuffer (
             (VOID **)&Payload,
             PayloadSize,
             MCTP_COMM_FUNCTION_GET_DEVICES
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (Payload != NULL);
  ZeroMem (Payload, sizeof (*Payload));

  // request fields
  Payload->MaxCount = MaxCount;

  Status = MctpMmSendCommBuffer (PayloadSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting MM devices: %r\n", __FUNCTION__, Status));
    return Status;
  }

  ASSERT (Payload->Count <= MaxCount);

  // reply fields
  *Count =   Payload->Count;
  CopyMem (DeviceInfo, Payload->Devices, Payload->Count * sizeof (*DeviceInfo));

  return Status;
}

/**
  Send MCTP receive operation.

  @param[in]     MmIndex    Index of Mctp device from MCTP_COMM_FUNCTION_GET_DEVICES.
  @param[in]     TimeoutMs  Timeout in ms to wait for device to send message.
  @param[out]    Message    Pointer to MCTP message buffer.
  @param[in out] Length     Pointer to message length in bytes.  As input
                            parameter, is length of Message buffer.  As output
                            parameter, is length of message received.
  @param[out]    MsgTag     Pointer to store message tag from received message.

  @retval EFI_SUCCESS           Operation successful.
  @retval Others                Error occurred.

**/
EFI_STATUS
EFIAPI
MctpMmSendRecv (
  IN UINT8      MmIndex,
  IN UINTN      TimeoutMs,
  OUT VOID      *Message,
  IN OUT UINTN  *Length,
  OUT UINT8     *MsgTag
  )
{
  EFI_STATUS      Status;
  MCTP_COMM_RECV  *Payload;
  UINTN           PayloadSize;

  PayloadSize = OFFSET_OF (MCTP_COMM_RECV, Data) + *Length;
  Status      = MctpMmInitCommBuffer (
                  (VOID **)&Payload,
                  PayloadSize,
                  MCTP_COMM_FUNCTION_RECV
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (Payload != NULL);
  ZeroMem (Payload, sizeof (*Payload));

  // request fields
  Payload->MmIndex   = MmIndex;
  Payload->TimeoutMs = TimeoutMs;
  Payload->MaxLength = *Length;

  Status = MctpMmSendCommBuffer (PayloadSize);
  if (EFI_ERROR (Status)) {
    if (Status != EFI_TIMEOUT) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: recv Index=%u Tag=%u Length=%u failed: %r\n",
        __FUNCTION__,
        MmIndex,
        *MsgTag,
        *Length,
        Status
        ));
    }

    return Status;
  }

  // reply fields
  *Length = Payload->Length;
  *MsgTag = Payload->MsgTag;
  CopyMem (Message, Payload->Data, Payload->Length);

  return EFI_SUCCESS;
}

/**
  Send MCTP send operation.

  @param[in]     MmIndex    Index of Mctp device from MCTP_COMM_FUNCTION_GET_DEVICES.
  @param[in]     IsRequest  Flag TRUE if this is an MCTP request.
  @param[in]     Message    Pointer to MCTP message buffer.
  @param[in]     Length     Message length in bytes.
  @param[in out] MsgTag     Pointer to message tag (input parameter if !IsRequest,
                            output parameter if IsRequest).

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
EFI_STATUS
EFIAPI
MctpMmSendSend (
  IN UINT8       MmIndex,
  IN BOOLEAN     IsRequest,
  IN CONST VOID  *Message,
  IN UINTN       Length,
  IN OUT UINT8   *MsgTag
  )
{
  EFI_STATUS      Status;
  MCTP_COMM_SEND  *Payload;
  UINTN           PayloadSize;

  PayloadSize = OFFSET_OF (MCTP_COMM_SEND, Data) + Length;
  Status      = MctpMmInitCommBuffer (
                  (VOID **)&Payload,
                  PayloadSize,
                  MCTP_COMM_FUNCTION_SEND
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (Payload != NULL);
  ZeroMem (Payload, sizeof (*Payload));

  // request fields
  Payload->MmIndex   = MmIndex;
  Payload->IsRequest = IsRequest;
  Payload->RspMsgTag = *MsgTag;
  Payload->Length    = Length;
  CopyMem (Payload->Data, Message, Length);

  Status = MctpMmSendCommBuffer (PayloadSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: send Index=%u IsReq=%u Tag=%u Length=%u failed: %r\n",
      __FUNCTION__,
      MmIndex,
      IsRequest,
      *MsgTag,
      Length,
      Status
      ));
    return Status;
  }

  // reply fields
  *MsgTag = Payload->ReqMsgTag;

  return Status;
}

/**
  Send MCTP do request operation.

  @param[in]     MmIndex           Index of Mctp device from MCTP_COMM_FUNCTION_GET_DEVICES.
  @param[in]  Request              Pointer to request message.
  @param[in]  RequestLength        Length of request message.
  @param[out] ResponseBuffer       Pointer to response buffer.
  @param[in]  ResponseBufferLength Length of response buffer.
  @param[out] ResponseLength       Pointer to return response length.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
EFI_STATUS
EFIAPI
MctpMmSendDoRequest (
  IN UINT8   MmIndex,
  IN  VOID   *Request,
  IN  UINTN  RequestLength,
  OUT VOID   *ResponseBuffer,
  IN  UINTN  ResponseBufferLength,
  OUT UINTN  *ResponseLength
  )
{
  EFI_STATUS            Status;
  MCTP_COMM_DO_REQUEST  *Payload;
  UINTN                 PayloadSize;

  PayloadSize = OFFSET_OF (MCTP_COMM_DO_REQUEST, Data) + MAX (RequestLength, ResponseBufferLength);
  Status      = MctpMmInitCommBuffer (
                  (VOID **)&Payload,
                  PayloadSize,
                  MCTP_COMM_FUNCTION_DO_REQUEST
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (Payload != NULL);
  ZeroMem (Payload, sizeof (*Payload));

  // request fields
  Payload->MmIndex              = MmIndex;
  Payload->RequestLength        = RequestLength;
  Payload->ResponseBufferLength = ResponseBufferLength;
  CopyMem (Payload->Data, Request, RequestLength);

  Status = MctpMmSendCommBuffer (PayloadSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: send Index=%u ReqLen=%u RspBuffLen=%u failed: %r\n",
      __FUNCTION__,
      MmIndex,
      RequestLength,
      ResponseBufferLength,
      Status
      ));
    return Status;
  }

  ASSERT (Payload->ResponseLength <= ResponseBufferLength);

  // reply fields
  *ResponseLength = Payload->ResponseLength;
  CopyMem (ResponseBuffer, Payload->Data, Payload->ResponseLength);

  return Status;
}
