/** @file
  Implement the RNDIS interface.

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Rndis.h"
#include "Debug.h"

/**
  Check and see if this is RNDIS interface or not.

  @param[in]  UsbIo        USB IO protocol

  @retval TRUE      This is RNDIS interface
  @retval FALSE     This is not RNDIS interface

**/
BOOLEAN
IsRndisInterface (
  IN EFI_USB_IO_PROTOCOL  *UsbIo
  )
{
  EFI_STATUS                    Status;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;

  if (UsbIo == NULL) {
    return FALSE;
  }

  Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &InterfaceDescriptor);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a, InterfaceDescriptor class: 0x%02x subclass: 0x%02x protocol: 0x%02x\n", __FUNCTION__, InterfaceDescriptor.InterfaceClass, InterfaceDescriptor.InterfaceSubClass, InterfaceDescriptor.InterfaceProtocol));

  if ((InterfaceDescriptor.InterfaceClass == USB_BASE_CLASS_COMMUNICATION) &&
      (InterfaceDescriptor.InterfaceSubClass == USB_SUB_CLASS_ACM) &&
      (InterfaceDescriptor.InterfaceProtocol == USB_PROTOCOL_ACM_VENDOR_SPECIFIC))
  {
    return TRUE;
  }

  return FALSE;
}

/**
  Check and see if this is CDC-DATA interface or not.

  @param[in]  UsbIo        USB IO protocol

  @retval TRUE      This is CDC-DATA interface
  @retval FALSE     This is not CDC-DATA interface

**/
BOOLEAN
IsRndisDataInterface (
  IN EFI_USB_IO_PROTOCOL  *UsbIo
  )
{
  EFI_STATUS                    Status;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;

  if (UsbIo == NULL) {
    return FALSE;
  }

  Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &InterfaceDescriptor);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a, InterfaceDescriptor class: 0x%02x subclass: 0x%02x protocol: 0x%02x\n", __FUNCTION__, InterfaceDescriptor.InterfaceClass, InterfaceDescriptor.InterfaceSubClass, InterfaceDescriptor.InterfaceProtocol));

  if ((InterfaceDescriptor.InterfaceClass == USB_BSSE_CLASS_CDC_DATA) &&
      (InterfaceDescriptor.InterfaceSubClass == USB_SUB_CLASS_CODE_CDC_DATA_NONE) &&
      (InterfaceDescriptor.InterfaceProtocol == USB_SUB_CLASS_CODE_CDC_DATA_NONE))
  {
    return TRUE;
  }

  return FALSE;
}

/**
  Configure USB device

  @param[in]      UsbIo             A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[out]     UsbEndpoint       Usb endpoint data.

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
RndisConfigureUsbDevice (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  OUT USB_ENDPOINT_DATA    *UsbEndpoint
  )
{
  EFI_STATUS                    Status;
  UINT8                         Index;
  UINT32                        Result;
  EFI_USB_DEVICE_DESCRIPTOR     Device;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;
  EFI_USB_ENDPOINT_DESCRIPTOR   EndpointDescriptor;
  EFI_STRING                    ManufacturerString;
  EFI_STRING                    SerialNumber;

  if ((UsbIo == NULL) || (UsbEndpoint == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Find device id and vendor id
  //
  Status = UsbIo->UsbGetDeviceDescriptor (UsbIo, &Device);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((USB_DEBUG_RNDIS, "%a, vendor: 0x%x product: 0x%x\n", __FUNCTION__, Device.IdVendor, Device.IdProduct));

  //
  // Find manufactor string
  //
  ManufacturerString = NULL;
  Status             = UsbIo->UsbGetStringDescriptor (
                                UsbIo,
                                USB_LANGUAGE_ID_ENGLISH,
                                Device.StrManufacturer,
                                &ManufacturerString
                                );
  if (!EFI_ERROR (Status) && (ManufacturerString != NULL)) {
    DEBUG ((USB_DEBUG_RNDIS, "%a, Manufacturer: %s\n", __FUNCTION__, ManufacturerString));
    FreePool (ManufacturerString);
  }

  //
  // Find serial number string
  //
  SerialNumber = NULL;
  Status       = UsbIo->UsbGetStringDescriptor (
                          UsbIo,
                          USB_LANGUAGE_ID_ENGLISH,
                          Device.StrSerialNumber,
                          &SerialNumber
                          );
  if (!EFI_ERROR (Status) && (SerialNumber != NULL)) {
    DEBUG ((USB_DEBUG_RNDIS, "%a, Serial Number: %s\n", __FUNCTION__, SerialNumber));
    FreePool (SerialNumber);
  }

  Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &InterfaceDescriptor);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (InterfaceDescriptor.NumEndpoints == 0 ) {
    Status = UsbSetInterface (UsbIo, 1, 0, &Result);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &InterfaceDescriptor);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  //
  // Locate and save the first bulk-in and bulk-out endpoint
  //
  for (Index = 0; Index < InterfaceDescriptor.NumEndpoints; Index++) {
    Status = UsbIo->UsbGetEndpointDescriptor (UsbIo, Index, &EndpointDescriptor);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (USB_IS_BULK_ENDPOINT (EndpointDescriptor.Attributes)) {
      if (USB_IS_IN_ENDPOINT (EndpointDescriptor.EndpointAddress)) {
        UsbEndpoint->BulkIn = EndpointDescriptor.EndpointAddress;
      } else if (USB_IS_OUT_ENDPOINT (EndpointDescriptor.EndpointAddress)) {
        UsbEndpoint->BulkOut = EndpointDescriptor.EndpointAddress;
      }
    } else if (USB_IS_INTERRUPT_ENDPOINT (EndpointDescriptor.Attributes)) {
      UsbEndpoint->Interrupt = EndpointDescriptor.EndpointAddress;
    }
  }

  return EFI_SUCCESS;
}

/**
  Send or receive RNDIS control message.

  @param[in]      UsbIo             A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]      RndisMessage          RNDIS message
  @param[out]     RestRndisMsgRes   RNDIS message response.

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
RndisControlMessage (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  RNDIS_MSG_HEADER     *RndisMessage,
  OUT RNDIS_MSG_HEADER     *RestRndisMsgRes OPTIONAL
  )
{
  EFI_USB_DEVICE_REQUEST  DeviceRequest;
  UINT32                  UsbStatus;
  EFI_STATUS              Status;
  RNDIS_MSG_HEADER        CachedMsg;
  UINT32                  PullCount;

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a\n", __FUNCTION__));

  if ((UsbIo == NULL) || (RndisMessage == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&DeviceRequest, sizeof (EFI_USB_DEVICE_REQUEST));

  DeviceRequest.RequestType = USB_REQ_TYPE_CLASS | USB_TARGET_INTERFACE;
  DeviceRequest.Request     = USB_SEND_ENCAPSULATED_CMD;
  DeviceRequest.Value       = 0;
  DeviceRequest.Index       = 0;
  DeviceRequest.Length      = (UINT16)RndisMessage->MessageLength;

  DEBUG_CODE_BEGIN ();
  DumpRndisMessage (USB_DEBUG_RNDIS_CONTROL, __FUNCTION__, RndisMessage);
  DEBUG_CODE_END ();

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &DeviceRequest,
                    EfiUsbDataOut,
                    PcdGet32 (PcdUsbTransferTimeoutValue),
                    RndisMessage,
                    RndisMessage->MessageLength,
                    &UsbStatus
                    );
  if (EFI_ERROR (Status) || (UsbStatus == EFI_USB_ERR_NAK)) {
    DEBUG ((DEBUG_ERROR, "%a, UsbStatus : %x Status : %r\n", __FUNCTION__, UsbStatus, Status));
    return Status;
  }

  //
  // Do not need response
  //
  if (RestRndisMsgRes == NULL) {
    return EFI_SUCCESS;
  }

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a, UsbStatus : %x Status : %r\n", __FUNCTION__, UsbStatus, Status));

  //
  // Receive response
  //
  PullCount = 0;
  do {
    CopyMem (&CachedMsg, RestRndisMsgRes, sizeof (RNDIS_MSG_HEADER));

    ZeroMem (&DeviceRequest, sizeof (EFI_USB_DEVICE_REQUEST));
    DeviceRequest.RequestType = USB_ENDPOINT_DIR_IN | USB_REQ_TYPE_CLASS | USB_TARGET_INTERFACE;
    DeviceRequest.Request     = USB_GET_ENCAPSULATED_RES;
    DeviceRequest.Value       = 0;
    DeviceRequest.Index       = 0;
    DeviceRequest.Length      = (UINT16)RestRndisMsgRes->MessageLength;

    Status = UsbIo->UsbControlTransfer (
                      UsbIo,
                      &DeviceRequest,
                      EfiUsbDataIn,
                      PcdGet32 (PcdUsbTransferTimeoutValue),
                      RestRndisMsgRes,
                      RestRndisMsgRes->MessageLength,
                      &UsbStatus
                      );
    if (EFI_ERROR (Status) || (UsbStatus == EFI_USB_ERR_NAK)) {
      DEBUG ((DEBUG_ERROR, "%a, UsbStatus : %x Status : %r\n", __FUNCTION__, UsbStatus, Status));
      gBS->Stall (100 * TICKS_PER_MS);    // 100msec
      continue;
    }

    DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a, UsbStatus : %x Status : %r\n", __FUNCTION__, UsbStatus, Status));
    if (RestRndisMsgRes->MessageType == CachedMsg.MessageType) {
      DEBUG_CODE_BEGIN ();
      DumpRndisMessage (USB_DEBUG_RNDIS_CONTROL, __FUNCTION__, RestRndisMsgRes);
      DEBUG_CODE_END ();

      return Status;
    }

    DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a, Unexpected message type: 0x%x\n", __FUNCTION__, RestRndisMsgRes->MessageType));

    CopyMem (RestRndisMsgRes, &CachedMsg, sizeof (RNDIS_MSG_HEADER));
  } while (++PullCount < RNDIS_USB_CONTROL_MESSAGE_MAX_POLL);

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a, TimeOut\n", __FUNCTION__));

  return EFI_TIMEOUT;
}

/**
  Send or RNDIS SET message.

  @param[in]      UsbIo             A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]      RequestId         RNDIS message request ID
  @param[in]      Oid               RNDIS OID
  @param[in]      Length            Buffer length in byte
  @param[in]      Buffer            Bufer to send

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
RndisSetMessage (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT32               RequestId,
  IN  UINT32               Oid,
  IN  UINT32               Length,
  IN  UINT8                *Buffer
  )
{
  RNDIS_SET_MSG_DATA    *RndisSetMsg;
  RNDIS_SET_CMPLT_DATA  *RndisSetCmplMsg;
  EFI_STATUS            Status;

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a\n", __FUNCTION__));

  if ((UsbIo == NULL) || (Buffer == NULL) || (Length == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  RndisSetMsg     = NULL;
  RndisSetCmplMsg = NULL;

  RndisSetMsg = AllocateZeroPool (sizeof (RNDIS_SET_MSG_DATA) + Length);
  if (RndisSetMsg == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  RndisSetCmplMsg = AllocateZeroPool (sizeof (RNDIS_SET_CMPLT_DATA));
  if (RndisSetCmplMsg == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Release;
  }

  RndisSetMsg->MessageType             = RNDIS_SET_MSG;
  RndisSetMsg->MessageLength           = sizeof (RNDIS_SET_MSG_DATA) + Length;
  RndisSetMsg->RequestId               = RequestId;
  RndisSetMsg->Oid                     = Oid;
  RndisSetMsg->InformationBufferLength = Length;
  RndisSetMsg->InformationBufferOffset = sizeof (RNDIS_SET_MSG_DATA) - 8;

  RndisSetCmplMsg->MessageType   = RNDIS_SET_CMPLT;
  RndisSetCmplMsg->MessageLength = sizeof (RNDIS_SET_CMPLT_DATA);

  CopyMem (((UINT8 *)RndisSetMsg) + sizeof (RNDIS_SET_MSG_DATA), Buffer, Length);

  Status = RndisControlMessage (
             UsbIo,
             (RNDIS_MSG_HEADER *)RndisSetMsg,
             (RNDIS_MSG_HEADER *)RndisSetCmplMsg
             );
  if (EFI_ERROR (Status) || (RndisSetCmplMsg->Status != RNDIS_STATUS_SUCCESS)) {
    DEBUG ((DEBUG_ERROR, "%a, RNDIS_SET_MSG to OID: 0x%x failed: %r status: 0x%x\n", __FUNCTION__, Oid, Status, RndisSetCmplMsg->Status));
  }

Release:

  FREE_NON_NULL (RndisSetMsg);
  FREE_NON_NULL (RndisSetCmplMsg);

  return Status;
}

/**
  This function sends the RNDIS SET_MSG cmd

  @param[in]  UsbRndisDevice  A pointer to the USB_RNDIS_DEVICE instance.
  @param[in]  Oid             Value of the OID.
  @param[in]  Length          Length of the data buffer.
  @param[in]  Buf             A pointer to the data buffer.

  @retval EFI_SUCCESS         The request executed successfully.

**/
EFI_STATUS
RndisQueryMessage (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT32               RequestId,
  IN  UINT32               Oid,
  IN  UINT32               InputLength,
  IN  UINT8                *InputBuf,
  IN  UINT32               OutputLength,
  OUT UINT8                *OutputBuf
  )
{
  RNDIS_QUERY_MSG_DATA    *RndisQueryMessage;
  RNDIS_QUERY_CMPLT_DATA  *RndisQueryCmpleteMessage;
  EFI_STATUS              Status;

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a\n", __FUNCTION__));

  if (UsbIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((InputLength > 0) && (InputBuf == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((OutputLength > 0) && (OutputBuf == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  RndisQueryMessage        = NULL;
  RndisQueryCmpleteMessage = NULL;

  RndisQueryMessage = AllocateZeroPool (sizeof (RNDIS_QUERY_MSG_DATA) + InputLength);
  if (RndisQueryMessage == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  RndisQueryCmpleteMessage = AllocateZeroPool (sizeof (RNDIS_QUERY_CMPLT_DATA) + OutputLength);
  if (RndisQueryCmpleteMessage == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Release;
  }

  RndisQueryMessage->MessageType             = RNDIS_QUERY_MSG;
  RndisQueryMessage->MessageLength           = sizeof (RNDIS_QUERY_MSG_DATA) + InputLength;
  RndisQueryMessage->RequestId               = RequestId;
  RndisQueryMessage->Oid                     = Oid;
  RndisQueryMessage->InformationBufferLength = InputLength;
  RndisQueryMessage->InformationBufferOffset = sizeof (RNDIS_QUERY_MSG_DATA) - 8;

  CopyMem (((UINT8 *)RndisQueryMessage) + sizeof (RNDIS_QUERY_MSG_DATA), InputBuf, InputLength);

  RndisQueryCmpleteMessage->MessageType   = RNDIS_QUERY_CMPLT;
  RndisQueryCmpleteMessage->MessageLength = sizeof (RNDIS_QUERY_CMPLT_DATA) + OutputLength;

  Status = RndisControlMessage (
             UsbIo,
             (RNDIS_MSG_HEADER *)RndisQueryMessage,
             (RNDIS_MSG_HEADER *)RndisQueryCmpleteMessage
             );
  if (EFI_ERROR (Status) || (RndisQueryCmpleteMessage->Status != RNDIS_STATUS_SUCCESS)) {
    DEBUG ((DEBUG_ERROR, "%a, RNDIS_QUERY_MSG to OID: 0x%x failed: %r status: 0x%x\n", __FUNCTION__, Oid, Status, RndisQueryCmpleteMessage->Status));
  } else {
    CopyMem (OutputBuf, ((UINT8 *)RndisQueryCmpleteMessage) + sizeof (RNDIS_QUERY_CMPLT_DATA), OutputLength);
  }

Release:

  FREE_NON_NULL (RndisQueryMessage);
  FREE_NON_NULL (RndisQueryCmpleteMessage);

  return Status;
}

/**
  Transmit RNDIS message to device.

  @param[in]      UsbIo             A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]      BulkOutEndpoint   bulk-out endpoint
  @param[in]      RndisMessage          RNDIS message to send.
  @param[in,out]  TransferLength    On input, it is message length in byte.
                                    On output, it is the length sent by USB.

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
RndisTransmitMessage (
  IN      EFI_USB_IO_PROTOCOL  *UsbIo,
  IN      UINT8                BulkOutEndpoint,
  IN      RNDIS_MSG_HEADER     *RndisMessage,
  IN OUT  UINTN                *TransferLength
  )
{
  EFI_STATUS  Status;
  UINT32      UsbStatus;

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a\n", __FUNCTION__));

  if ((UsbIo == NULL) || (RndisMessage == NULL) || (TransferLength == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (BulkOutEndpoint == 0) {
    return EFI_NOT_READY;
  }

  DEBUG_CODE_BEGIN ();
  DumpRndisMessage (USB_DEBUG_RNDIS_TRANSFER, __FUNCTION__, RndisMessage);
  DEBUG_CODE_END ();

  Status = UsbIo->UsbBulkTransfer (
                    UsbIo,
                    BulkOutEndpoint,
                    RndisMessage,
                    TransferLength,
                    RNDIS_USB_TRANSMIT_TIMEOUT,
                    &UsbStatus
                    );
  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a, UsbStatus : %x Status : %r\n", __FUNCTION__, UsbStatus, Status));

  return Status;
}

/**
  Receive RNDIS message from device.

  @param[in]      UsbIo             A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]      BulkInEndpoint    bulk-in endpoint
  @param[in]      RndisMessage          RNDIS message to receive.
  @param[in,out]  TransferLength    On input, it is message length in byte.
                                    On output, it is the length received from USB.

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
RndisReceiveMessage (
  IN      EFI_USB_IO_PROTOCOL  *UsbIo,
  IN      UINT8                BulkInEndpoint,
  IN OUT  RNDIS_MSG_HEADER     *RndisMessage,
  IN OUT  UINTN                *TransferLength
  )
{
  EFI_STATUS  Status;
  UINT32      UsbStatus = 0;

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a\n", __FUNCTION__));

  if ((UsbIo == NULL) || (RndisMessage == NULL) || (TransferLength == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (BulkInEndpoint == 0) {
    return EFI_NOT_READY;
  }

  Status = UsbIo->UsbBulkTransfer (
                    UsbIo,
                    BulkInEndpoint,
                    RndisMessage,
                    TransferLength,
                    RNDIS_USB_RECEIVE_TIMEOUT,
                    &UsbStatus
                    );
  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a, UsbStatus : %x Status : %r\n", __FUNCTION__, UsbStatus, Status));

  DEBUG_CODE_BEGIN ();
  if (!EFI_ERROR (Status) && (*TransferLength > 0)) {
    DumpRndisMessage (USB_DEBUG_RNDIS_TRANSFER, __FUNCTION__, RndisMessage);
  }

  DEBUG_CODE_END ();

  return Status;
}

/**
  Add RNDIS message into queue for later use.

  @param[in]      Private           Pointer to private data.
  @param[in]      Buffer            Buffer to be put in queue
  @param[in]      BufferSize        The buffer size in byte.

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
RndisReceiveEnqueue (
  IN  USB_PRIVATE_DATA  *Private,
  IN  UINT8             *Buffer,
  IN  UINTN             BufferSize
  )
{
  USB_QUEUE_NODE  *NewNode;

  if ((Private == NULL) || (Buffer == NULL) || (BufferSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((USB_DEBUG_QUEUE, "%a, queue: (%d/%d)\n", __FUNCTION__, Private->QueueCount, RNDIS_RECEIVE_QUEUE_MAX));

  if (Private->QueueCount > RNDIS_RECEIVE_QUEUE_MAX) {
    ASSERT (FALSE);
    return EFI_OUT_OF_RESOURCES;
  }

  NewNode = AllocatePool (sizeof (USB_QUEUE_NODE));
  if (NewNode != NULL) {
    NewNode->Signature  = USB_QUEUE_NODE_SIGNATURE;
    NewNode->Buffer     = Buffer;
    NewNode->BufferSize = BufferSize;

    InsertTailList (&Private->ReceiveQueue, &NewNode->Link);
    Private->QueueCount += 1;

    return EFI_SUCCESS;
  }

  return EFI_OUT_OF_RESOURCES;
}

/**
  Get RNDIS message from queue.

  @param[in]      Private           Pointer to private data.
  @param[out]     Buffer            Buffer from queue
  @param[out]     BufferSize        The buffer size in byte.

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
RndisReceiveDequeue (
  IN  USB_PRIVATE_DATA  *Private,
  OUT UINT8             **Buffer,
  OUT UINTN             *BufferSize
  )
{
  LIST_ENTRY      *Link;
  USB_QUEUE_NODE  *Node;

  if ((Private == NULL) || (Buffer == NULL) || (BufferSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((USB_DEBUG_QUEUE, "%a, queue: (%d/%d)\n", __FUNCTION__, Private->QueueCount, RNDIS_RECEIVE_QUEUE_MAX));

  *BufferSize = 0;

  if (IsListEmpty (&Private->ReceiveQueue)) {
    return EFI_NOT_FOUND;
  }

  Link = GetFirstNode (&Private->ReceiveQueue);
  Node = USB_QUEUE_NODE_FROM_LINK (Link);

  *Buffer     = Node->Buffer;
  *BufferSize = Node->BufferSize;

  RemoveEntryList (&Node->Link);
  FreePool (Node);
  Private->QueueCount -= 1;

  return EFI_SUCCESS;
}

/**
  This function receives data from USB device and push it to queue for later use.

  @param[in]      Private       Pointer to private data.

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
UsbRndisReceive (
  IN  USB_RNDIS_PRIVATE_DATA  *Private
  )
{
  RNDIS_PACKET_MSG_DATA  *RndisPacketMessage;
  UINTN                  Length;
  UINT8                  *RndisBuffer;
  EFI_STATUS             Status;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Private->UsbIoProtocol == NULL) || (Private->UsbData.EndPoint.BulkIn == 0)) {
    return EFI_NOT_READY;
  }

  Length      = Private->UsbData.MaxTransferSize;
  RndisBuffer = AllocateZeroPool (Length);
  if (RndisBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Receive data on bulk-in endpoint.
  //
  Status = RndisReceiveMessage (
             Private->UsbIoDataProtocol,
             Private->UsbData.EndPoint.BulkIn,
             (RNDIS_MSG_HEADER *)RndisBuffer,
             &Length
             );
  if (EFI_ERROR (Status) || (Length == 0)) {
    DEBUG ((USB_DEBUG_SNP_TRACE, "%a, RndisReceiveMessage: %r Length: %u\n", __FUNCTION__, Status, Length));
    Status = EFI_NOT_READY;
    goto OnRelease;
  }

  RndisPacketMessage = (RNDIS_PACKET_MSG_DATA *)RndisBuffer;
  if ((RndisPacketMessage->MessageType != RNDIS_PACKET_MSG) || (RndisPacketMessage->DataOffset != (sizeof (RNDIS_PACKET_MSG_DATA) - 8))) {
    Status = EFI_DEVICE_ERROR;
    goto OnRelease;
  }

  //
  // Enqueue
  //
  Status = RndisReceiveEnqueue (&Private->UsbData, RndisBuffer, Private->UsbData.MaxTransferSize);
  if (EFI_ERROR (Status)) {
    goto OnRelease;
  }

  return EFI_SUCCESS;

OnRelease:

  FreePool (RndisBuffer);

  return Status;
}

/**
  Reset RNDIS device with REST command

  @param[in]      UsbIo         USB IO protocol
  @param[in]      RequestId     Request ID

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
UsbRndisResetDevice (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT32               RequestId
  )
{
  RNDIS_RESET_MSG_DATA    RndisResetMessage;
  RNDIS_RESET_CMPLT_DATA  RndisResetCompleteMessage;
  EFI_STATUS              Status;

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a\n", __FUNCTION__));

  if (UsbIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&RndisResetMessage, sizeof (RNDIS_RESET_MSG_DATA));
  ZeroMem (&RndisResetCompleteMessage, sizeof (RNDIS_RESET_CMPLT_DATA));

  RndisResetMessage.MessageType   = RNDIS_RESET_MSG;
  RndisResetMessage.MessageLength = sizeof (RNDIS_RESET_MSG_DATA);

  RndisResetCompleteMessage.MessageType   = RNDIS_RESET_CMPLT;
  RndisResetCompleteMessage.MessageLength = sizeof (RNDIS_RESET_CMPLT_DATA);

  Status = RndisControlMessage (UsbIo, (RNDIS_MSG_HEADER *)&RndisResetMessage, (RNDIS_MSG_HEADER *)&RndisResetCompleteMessage);
  if (EFI_ERROR (Status) || (RndisResetCompleteMessage.Status != RNDIS_STATUS_SUCCESS)) {
    DEBUG ((DEBUG_ERROR, "%a, RndisControlMessage: %r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Shutdown RNDIS device with HALT command

  @param[in]      UsbIo         USB IO protocol

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
UsbRndisShutdownDevice (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo
  )
{
  RNDIS_HALT_MSG_DATA  RndisHaltMessage;
  EFI_STATUS           Status;

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a\n", __FUNCTION__));

  if (UsbIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&RndisHaltMessage, sizeof (RNDIS_HALT_MSG_DATA));

  RndisHaltMessage.MessageType   = RNDIS_HLT_MSG;
  RndisHaltMessage.MessageLength = sizeof (RNDIS_HALT_MSG_DATA);

  Status = RndisControlMessage (UsbIo, (RNDIS_MSG_HEADER *)&RndisHaltMessage, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, RndisControlMessage: %r\n", __FUNCTION__, Status));
  }

  return EFI_SUCCESS;
}

/**
  Get RNDIS media status by using OID_GEN_MEDIA_CONNECT_STATUS.

  @param[in]      UsbIo         USB IO protocol
  @param[in]      RequestId     Request ID

  @retval UINT32       Media status. 0x0 is connected and 0x1 is disconnected.

**/
UINT32
UsbRndisMediaStatus (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT32               RequestId
  )
{
  EFI_STATUS  Status;
  UINT32      MediaStatus;

  if (UsbIo == NULL) {
    return FALSE;
  }

  MediaStatus = 0;

  //
  // OID_GEN_MEDIA_CONNECT_STATUS
  //
  Status = RndisQueryMessage (
             UsbIo,
             RequestId,
             OID_GEN_MEDIA_CONNECT_STATUS,
             0,
             NULL,
             sizeof (MediaStatus),
             (UINT8 *)&MediaStatus
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, OID_GEN_MEDIA_CONNECT_STATUS: %r\n", __FUNCTION__, Status));
  }

  return MediaStatus;
}

/**
  Initial RNDIS by using INITIAL command

  @param[in]      UsbIo         USB IO protocol
  @param[in]      RequestId     Request ID
  @param[out]     UsbData       USB device data

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
UsbRndisInitialDevice (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT32               RequestId,
  OUT USB_PRIVATE_DATA     *UsbData
  )
{
  RNDIS_INITIALIZE_MSG_DATA    RndisInitMsg;
  RNDIS_INITIALIZE_CMPLT_DATA  RndisInitMsgCmplt;
  EFI_STATUS                   Status;

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a\n", __FUNCTION__));

  if ((UsbIo == NULL) || (UsbData == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&RndisInitMsg, sizeof (RNDIS_INITIALIZE_MSG_DATA));
  ZeroMem (&RndisInitMsgCmplt, sizeof (RNDIS_INITIALIZE_CMPLT_DATA));

  RndisInitMsg.MessageType     = RNDIS_INITIALIZE_MSG;
  RndisInitMsg.MessageLength   = sizeof (RNDIS_INITIALIZE_MSG_DATA);
  RndisInitMsg.RequestId       = RequestId;
  RndisInitMsg.MajorVersion    = RNDIS_MAJOR_VERSION;
  RndisInitMsg.MinorVersion    = RNDIS_MINOR_VERSION;
  RndisInitMsg.MaxTransferSize = RNDIS_MAX_TRANSFER_SIZE;

  RndisInitMsgCmplt.MessageType   = RNDIS_INITIALIZE_CMPLT;
  RndisInitMsgCmplt.MessageLength = sizeof (RNDIS_INITIALIZE_CMPLT_DATA);

  Status = RndisControlMessage (UsbIo, (RNDIS_MSG_HEADER *)&RndisInitMsg, (RNDIS_MSG_HEADER *)&RndisInitMsgCmplt);
  if (EFI_ERROR (Status) || (RndisInitMsgCmplt.Status != RNDIS_STATUS_SUCCESS)) {
    DEBUG ((DEBUG_ERROR, "%a, RndisControlMessage: %r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  if (RndisInitMsgCmplt.Medium != IEEE_802_3_WIRED_ETHERNET) {
    return EFI_UNSUPPORTED;
  }

  UsbData->Medium                = RndisInitMsgCmplt.Medium;
  UsbData->MaxPacketsPerTransfer = RndisInitMsgCmplt.MaxPacketsPerTransfer;
  UsbData->MaxTransferSize       = RndisInitMsgCmplt.MaxTransferSize;
  UsbData->PacketAlignmentFactor = RndisInitMsgCmplt.PacketAlignmentFactor;

  DEBUG ((USB_DEBUG_RNDIS, "%a, Medium : %x \n", __FUNCTION__, UsbData->Medium));
  DEBUG ((USB_DEBUG_RNDIS, "%a, MaxPacketsPerTransfer : %x \n", __FUNCTION__, UsbData->MaxPacketsPerTransfer));
  DEBUG ((USB_DEBUG_RNDIS, "%a, MaxTransferSize : %x\n", __FUNCTION__, UsbData->MaxTransferSize));
  DEBUG ((USB_DEBUG_RNDIS, "%a, PacketAlignmentFactor : %x\n", __FUNCTION__, UsbData->PacketAlignmentFactor));

  return EFI_SUCCESS;
}

/**
  Initial RNDIS device and query corresponding data for SNP use.

  @param[in]      Private       Poniter to private data

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
UsbRndisInitialRndisDevice (
  IN  USB_RNDIS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a\n", __FUNCTION__));

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Private->UsbIoProtocol == NULL)) {
    return EFI_NOT_READY;
  }

  //
  // Get USB descriptor first
  //
  Status = RndisConfigureUsbDevice (Private->UsbIoProtocol, &Private->UsbData.EndPoint);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, RndisGetUsbEndpoint: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((USB_DEBUG_RNDIS, "%a Bulk-in: %x, Bulk-out: %x Interrupt: %x\n", __FUNCTION__, Private->UsbData.EndPoint.BulkIn, Private->UsbData.EndPoint.BulkOut, Private->UsbData.EndPoint.Interrupt));

  //
  // Reset device and initial it
  //
  Status = UsbRndisResetDevice (Private->UsbIoProtocol, USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, UsbRndisResetDevice: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = UsbRndisInitialDevice (Private->UsbIoProtocol, USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId), &Private->UsbData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, UsbRndisInitialDevice: %r\n", __FUNCTION__, Status));
    return Status;
  }

  //
  // OID_GEN_MEDIA_CONNECT_STATUS
  //
  Private->UsbData.MediaStatus = UsbRndisMediaStatus (Private->UsbIoProtocol, USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId));
  DEBUG ((USB_DEBUG_RNDIS, "%a, OID_GEN_MEDIA_CONNECT_STATUS 0x%x\n", __FUNCTION__, Private->UsbData.MediaStatus));

  //
  // OID_GEN_LINK_SPEED
  //
  Status = RndisQueryMessage (
             Private->UsbIoProtocol,
             USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId),
             OID_GEN_LINK_SPEED,
             0,
             NULL,
             sizeof (Private->UsbData.LinkSpeed),
             (UINT8 *)&Private->UsbData.LinkSpeed
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, OID_GEN_LINK_SPEED: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((USB_DEBUG_RNDIS, "%a, OID_GEN_LINK_SPEED 0x%x\n", __FUNCTION__, Private->UsbData.LinkSpeed));

  //
  // OID_GEN_MAXIMUM_FRAME_SIZE
  //
  Status = RndisQueryMessage (
             Private->UsbIoProtocol,
             USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId),
             OID_GEN_MAXIMUM_FRAME_SIZE,
             0,
             NULL,
             sizeof (Private->UsbData.MaxFrameSize),
             (UINT8 *)&Private->UsbData.MaxFrameSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, OID_GEN_MAXIMUM_FRAME_SIZE: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((USB_DEBUG_RNDIS, "%a, OID_GEN_MAXIMUM_FRAME_SIZE 0x%x\n", __FUNCTION__, Private->UsbData.MaxFrameSize));

  //
  // OID_GEN_CURRENT_PACKET_FILTER
  //
  Status = RndisQueryMessage (
             Private->UsbIoProtocol,
             USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId),
             OID_GEN_CURRENT_PACKET_FILTER,
             0,
             NULL,
             sizeof (Private->UsbData.Filter),
             (UINT8 *)&Private->UsbData.Filter
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, OID_GEN_CURRENT_PACKET_FILTER: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((USB_DEBUG_RNDIS, "%a, OID_GEN_CURRENT_PACKET_FILTER 0x%x\n", __FUNCTION__, Private->UsbData.Filter));

  //
  // RNDIS_OID_802_3_PERMANENT_ADDRESS
  //
  Status = RndisQueryMessage (
             Private->UsbIoProtocol,
             USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId),
             RNDIS_OID_802_3_PERMANENT_ADDRESS,
             0,
             NULL,
             NET_ETHER_ADDR_LEN,
             &Private->UsbData.PermanentAddress.Addr[0]
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, RNDIS_OID_802_3_PERMANENT_ADDRESS: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((
    USB_DEBUG_RNDIS,
    "%a, RNDIS_OID_802_3_PERMANENT_ADDRESS %02x:%02x:%02x:%02x:%02x:%02x\n",
    __FUNCTION__,
    Private->UsbData.PermanentAddress.Addr[0],
    Private->UsbData.PermanentAddress.Addr[1],
    Private->UsbData.PermanentAddress.Addr[2],
    Private->UsbData.PermanentAddress.Addr[3],
    Private->UsbData.PermanentAddress.Addr[4],
    Private->UsbData.PermanentAddress.Addr[5]
    ));

  //
  // RNDIS_OID_802_3_CURRENT_ADDRESS
  //
  Status = RndisQueryMessage (
             Private->UsbIoProtocol,
             USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId),
             RNDIS_OID_802_3_CURRENT_ADDRESS,
             0,
             NULL,
             NET_ETHER_ADDR_LEN,
             &Private->UsbData.CurrentAddress.Addr[0]
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, RNDIS_OID_802_3_CURRENT_ADDRESS: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((
    USB_DEBUG_RNDIS,
    "%a, RNDIS_OID_802_3_CURRENT_ADDRESS %02x:%02x:%02x:%02x:%02x:%02x\n",
    __FUNCTION__,
    Private->UsbData.CurrentAddress.Addr[0],
    Private->UsbData.CurrentAddress.Addr[1],
    Private->UsbData.CurrentAddress.Addr[2],
    Private->UsbData.CurrentAddress.Addr[3],
    Private->UsbData.CurrentAddress.Addr[4],
    Private->UsbData.CurrentAddress.Addr[5]
    ));

  return EFI_SUCCESS;
}

/**
  Ask receiver control to receive data immediately.

  @param[in]      Private       Poniter to private data

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
VOID
UndisReceiveNow (
  IN USB_RNDIS_PRIVATE_DATA  *Private
  )
{
  EFI_TPL  OldTpl;

  if ((Private == NULL) || (Private->ReceiverControlTimer == NULL)) {
    return;
  }

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  gBS->SetTimer (Private->ReceiverControlTimer, TimerCancel, 0);
  Private->ReceiverSlowWaitFlag = FALSE;

  gBS->RestoreTPL (OldTpl);
}

/**
  Ask receiver control to slow down receive ratio.

  @param[in]      Private       Poniter to private data

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
VOID
UndisReceiveSlowDown (
  IN USB_RNDIS_PRIVATE_DATA  *Private
  )
{
  EFI_TPL     OldTpl;
  EFI_STATUS  Status;

  if ((Private == NULL) || (Private->ReceiverControlTimer == NULL)) {
    return;
  }

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  //
  // Start receiver control timer
  //
  Private->ReceiverSlowWaitFlag = TRUE;
  Status                        = gBS->SetTimer (
                                         Private->ReceiverControlTimer,
                                         TimerRelative,
                                         USB_BACKGROUND_PULL_INTERVAL
                                         );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, Start timer failed: %r\n", __FUNCTION__, Status));
  } else {
    Private->ReceiverSlowWaitFlag = TRUE;
  }

  gBS->RestoreTPL (OldTpl);
}

/**
  This is the timer controlling the receive packet rate.

  @param[in]  Event        The event this notify function registered to.
  @param[in]  Context      Pointer to the context data registered to the event.

**/
VOID
EFIAPI
RndisReceiveControlTimer (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;

  Private = (USB_RNDIS_PRIVATE_DATA *)Context;
  if (Private == NULL) {
    return;
  }

  //
  // Turn off flag and exit. This is called at TPL_NOTIFY level.
  //
  Private->ReceiverSlowWaitFlag = FALSE;
}

/**
  This is worker to receive data from network and it returns until there is
  no network data available.

  Private       Pointer to private data.

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
RndisReceiveWorker (
  IN USB_RNDIS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  if (Private->ReceiverSlowWaitFlag) {
    return EFI_NOT_READY;
  }

  //
  // Receive data from USB device until failure happens.
  //
  do {
    Status = UsbRndisReceive (Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((USB_DEBUG_RNDIS_TRACE, "%a, receive failed: %r\n", __FUNCTION__, Status));
    }

    //
    // Leave loop when there is no data or when queue is full.
    //
  } while (!EFI_ERROR (Status) && Private->UsbData.QueueCount < RNDIS_RECEIVE_QUEUE_MAX);

  //
  // When receive error happens, slow down to USB_BACKGROUND_PULL_INTERVAL receive ratio.
  //
  UndisReceiveSlowDown (Private);

  return Status;
}
