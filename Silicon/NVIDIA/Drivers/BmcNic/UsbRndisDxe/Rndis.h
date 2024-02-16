/** @file
  Definitions for USB RNDIS interface.

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef RNDIS_H_
#define RNDIS_H_

#include "UsbRndisDxe.h"

#include <Uefi.h>

#include <Protocol/UsbIo.h>

#include <Library/UefiUsbLib.h>

//
// Ref: OpenBMC: linux/drivers/usb/gadget/function/f_rndis.c
//
#define USB_BASE_CLASS_COMMUNICATION        0x02
#define   USB_SUB_CLASS_ACM                 0x02
#define   USB_PROTOCOL_ACM_VENDOR_SPECIFIC  0xFF

//
// Bulk endpoints
//
#define USB_BASE_CLASS_CDC_DATA             0x0A
#define   USB_SUB_CLASS_CODE_CDC_DATA_NONE  0x00
#define   USB_PROTOCOL_CODE_CDC_DATA_NONE   0x00

#define RNDIS_USB_CONTROL_MESSAGE_MAX_POLL  100
#define RNDIS_USB_TRANSMIT_TIMEOUT          3000
#define RNDIS_USB_RECEIVE_TIMEOUT           1
#define RNDIS_RECEIVE_QUEUE_MAX             0x00FF

#define USB_SEND_ENCAPSULATED_CMD  0x00000000
#define USB_GET_ENCAPSULATED_RES   0x00000001

#define USB_IS_IN_ENDPOINT(EndPointAddr)      (((EndPointAddr) & BIT7) == BIT7)
#define USB_IS_OUT_ENDPOINT(EndPointAddr)     (((EndPointAddr) & BIT7) == 0)
#define USB_IS_BULK_ENDPOINT(Attribute)       (((Attribute) & (BIT0 | BIT1)) == USB_ENDPOINT_BULK)
#define USB_IS_INTERRUPT_ENDPOINT(Attribute)  (((Attribute) & (BIT0 | BIT1)) == USB_ENDPOINT_INTERRUPT)

#define USB_INCREASE_REQUEST_ID(a)  ((a)++)
#define USB_RESET_REQUEST_ID(a)     ((a) = 0x1)
#define USB_BACKGROUND_PULL_INTERVAL  (10 * TICKS_PER_MS) // slow down polling to 10 ms interval.
#define USB_LANGUAGE_ID_ENGLISH       0x0409              // English

//
// Per MS-RNDIS
//
#define RNDIS_MAJOR_VERSION        0x00000001
#define RNDIS_MINOR_VERSION        0x00000000
#define RNDIS_MAX_TRANSFER_SIZE    0x00004000
#define IEEE_802_3_WIRED_ETHERNET  0x000000000

//
// RNDIS Message types
//
#define RNDIS_PACKET_MSG           0x00000001
#define RNDIS_INITIALIZE_MSG       0x00000002
#define RNDIS_INITIALIZE_CMPLT     0x80000002
#define RNDIS_HLT_MSG              0x00000003
#define RNDIS_QUERY_MSG            0x00000004
#define RNDIS_QUERY_CMPLT          0x80000004
#define RNDIS_SET_MSG              0x00000005
#define RNDIS_SET_CMPLT            0x80000005
#define RNDIS_RESET_MSG            0x00000006
#define RNDIS_RESET_CMPLT          0x80000006
#define RNDIS_INDICATE_STATUS_MSG  0x00000007
#define RNDIS_KEEPALIVE_MSG        0x00000008
#define RNDIS_KEEPALIVE_CMPLT      0x80000008

//
// RNDIS return status
//
#define RNDIS_STATUS_SUCCESS           0x00000000
#define RNDIS_STATUS_FAILURE           0xC0000001
#define RNDIS_STATUS_INVALID_DATA      0xC0010015
#define RNDIS_STATUS_NOT_SUPPORTED     0xC00000BB
#define RNDIS_STATUS_MEDIA_CONNECT     0x4001000B
#define RNDIS_STATUS_MEDIA_DISCONNECT  0x4001000C

//
// NDIS OIDs
//
#define OID_GEN_SUPPORTED_LIST         0x00010101
#define OID_GEN_HARDWARE_STATUS        0x00010102
#define OID_GEN_MEDIA_SUPPORTED        0x00010103
#define OID_GEN_MEDIA_IN_USE           0x00010104
#define OID_GEN_MAXIMUM_LOOKAHEAD      0x00010105
#define OID_GEN_MAXIMUM_FRAME_SIZE     0x00010106
#define OID_GEN_LINK_SPEED             0x00010107
#define OID_GEN_TRANSMIT_BUFFER_SPACE  0x00010108
#define OID_GEN_RECEIVE_BUFFER_SPACE   0x00010109
#define OID_GEN_TRANSMIT_BLOCK_SIZE    0x0001010A
#define OID_GEN_RECEIVE_BLOCK_SIZE     0x0001010B
#define OID_GEN_VENDOR_ID              0x0001010C
#define OID_GEN_VENDOR_DESCRIPTION     0x0001010D
#define OID_GEN_CURRENT_PACKET_FILTER  0x0001010E
#define OID_GEN_CURRENT_LOOKAHEAD      0x0001010F
#define OID_GEN_DRIVER_VERSION         0x00010110
#define OID_GEN_MAXIMUM_TOTAL_SIZE     0x00010111
#define OID_GEN_PROTOCOL_OPTIONS       0x00010112
#define OID_GEN_MAC_OPTIONS            0x00010113
#define OID_GEN_MEDIA_CONNECT_STATUS   0x00010114
#define OID_GEN_MAXIMUM_SEND_PACKETS   0x00010115
#define OID_GEN_VENDOR_DRIVER_VERSION  0x00010116
#define OID_GEN_XMIT_OK                0x00020101
#define OID_GEN_RCV_OK                 0x00020102
#define OID_GEN_XMIT_ERROR             0x00020103
#define OID_GEN_RCV_ERROR              0x00020104
#define OID_GEN_RCV_NO_BUFFER          0x00020105
#define OID_GEN_DIRECTED_BYTES_XMIT    0x00020201
#define OID_GEN_DIRECTED_FRAMES_XMIT   0x00020202
#define OID_GEN_MULTICAST_BYTES_XMIT   0x00020203
#define OID_GEN_MULTICAST_FRAMES_XMIT  0x00020204
#define OID_GEN_BROADCAST_BYTES_XMIT   0x00020205
#define OID_GEN_BROADCAST_FRAMES_XMIT  0x00020206
#define OID_GEN_DIRECTED_BYTES_RCV     0x00020207
#define OID_GEN_DIRECTED_FRAMES_RCV    0x00020208
#define OID_GEN_MULTICAST_BYTES_RCV    0x00020209
#define OID_GEN_MULTICAST_FRAMES_RCV   0x0002020A
#define OID_GEN_BROADCAST_BYTES_RCV    0x0002020B
#define OID_GEN_BROADCAST_FRAMES_RCV   0x0002020C
#define OID_GEN_RCV_CRC_ERROR          0x0002020D
#define OID_GEN_TRANSMIT_QUEUE_LENGTH  0x0002020E

//
// NDIS Packet Filter Types (OID_GEN_CURRENT_PACKET_FILTER).
//
#define NDIS_PACKET_TYPE_DIRECTED        0x0001
#define NDIS_PACKET_TYPE_MULTICAST       0x0002
#define NDIS_PACKET_TYPE_ALL_MULTICAST   0x0004
#define NDIS_PACKET_TYPE_BROADCAST       0x0008
#define NDIS_PACKET_TYPE_SOURCE_ROUTING  0x0010
#define NDIS_PACKET_TYPE_PROMISCUOUS     0x0020
#define NDIS_PACKET_TYPE_SMT             0x0040
#define NDIS_PACKET_TYPE_ALL_LOCAL       0x0080
#define NDIS_PACKET_TYPE_MAC_FRAME       0x8000
#define NDIS_PACKET_TYPE_FUNCTIONAL      0x4000
#define NDIS_PACKET_TYPE_ALL_FUNCTIONAL  0x2000
#define NDIS_PACKET_TYPE_GROUP           0x1000

//
// Remote NDIS medium connection states (OID_GEN_MEDIA_CONNECT_STATUS).
//
#define RNDIS_MEDIA_STATE_CONNECTED     0x00000000
#define RNDIS_MEDIA_STATE_DISCONNECTED  0x00000001

//
// Ndis 802.3 object
//
#define RNDIS_OID_802_3_PERMANENT_ADDRESS  0x01010101
#define RNDIS_OID_802_3_CURRENT_ADDRESS    0x01010102

//
// RNDIS Message structure
//
#pragma pack(1)
typedef struct _RNDIS_MSG_HEADER {
  UINT32    MessageType;
  UINT32    MessageLength;
} RNDIS_MSG_HEADER;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    RequestId;
  UINT32    MajorVersion;
  UINT32    MinorVersion;
  UINT32    MaxTransferSize;
} RNDIS_INITIALIZE_MSG_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    RequestId;
} RNDIS_HALT_MSG_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    Reserved;
} RNDIS_RESET_MSG_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    RequestId;
  UINT32    Oid;
  UINT32    InformationBufferLength;
  UINT32    InformationBufferOffset;
  UINT32    Reserved;
} RNDIS_QUERY_MSG_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    RequestId;
  UINT32    Oid;
  UINT32    InformationBufferLength;
  UINT32    InformationBufferOffset;
  UINT32    Reserved;
} RNDIS_SET_MSG_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    RequestId;
} RNDIS_KEEPALIVE_MSG_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    Status;
  UINT32    StatusBufferLength;
  UINT32    StatusBufferOffset;
} RNDIS_INDICATE_STATUS_MSG_DATA;

typedef struct {
  UINT32    DiagStatus;
  UINT32    ErrorOffset;
} RNDIS_DIAGNOSTIC_INFO_DATA;

//
// Return data
//
typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    RequestId;
  UINT32    Status;
  UINT32    MajorVersion;
  UINT32    MinorVersion;
  UINT32    DeviceFlags;
  UINT32    Medium;
  UINT32    MaxPacketsPerTransfer;
  UINT32    MaxTransferSize;
  UINT32    PacketAlignmentFactor;
  UINT64    Reserved;
} RNDIS_INITIALIZE_CMPLT_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    RequestId;
  UINT32    Status;
  UINT32    InformationBufferLength;
  UINT32    InformationBufferOffset;
} RNDIS_QUERY_CMPLT_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    RequestId;
  UINT32    Status;
} RNDIS_SET_CMPLT_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    Status;
  UINT32    AddressingReset;
} RNDIS_RESET_CMPLT_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    RequestId;
  UINT32    Status;
} RNDIS_KEEPALIVE_CMPLT_DATA;

typedef struct {
  UINT32    MessageType;
  UINT32    MessageLength;
  UINT32    DataOffset;
  UINT32    DataLength;
  UINT32    OutOfBandDataOffset;
  UINT32    OutOfBandDataLength;
  UINT32    NumOutOfBandDataElements;
  UINT32    PerPacketInfoOffset;
  UINT32    PerPacketInfoLength;
  UINT32    Reserved1;
  UINT32    Reserved2;
} RNDIS_PACKET_MSG_DATA;
#pragma pack()

/**
  Check and see if this is RNDIS interface or not.

  @param[in]  UsbIo        USB IO protocol

  @retval TRUE      This is RNDIS interface
  @retval FALSE     This is not RNDIS interface

**/
BOOLEAN
IsRndisInterface (
  IN EFI_USB_IO_PROTOCOL  *UsbIo
  );

/**
  Check and see if this is CDC-DATA interface or not.

  @param[in]  UsbIo        USB IO protocol

  @retval TRUE      This is CDC-DATA interface
  @retval FALSE     This is not CDC-DATA interface

**/
BOOLEAN
IsRndisDataInterface (
  IN EFI_USB_IO_PROTOCOL  *UsbIo
  );

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
  );

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
  );

/**
  Shutdown RNDIS device with HALT command

  @param[in]      UsbIo         USB IO protocol

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
UsbRndisShutdownDevice (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo
  );

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
  );

/**
  Initial RNDIS device and query corresponding data for SNP use.

  @param[in]      Private       Pointer to private data

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
UsbRndisInitialRndisDevice (
  IN  USB_RNDIS_PRIVATE_DATA  *Private
  );

/**
  Send or RNDIS SET message.

  @param[in]      UsbIo             A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]      RequestId         RNDIS message request ID
  @param[in]      Oid               RNDIS OID
  @param[in]      Length            Buffer length in byte
  @param[in]      Buffer            Buffer to send

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
  );

/**
  Transmit RNDIS message to device.

  @param[in]      UsbIo             A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]      BulkOutEndpoint   bulk-out endpoint
  @param[in]      RndisMsg          RNDIS message to send.
  @param[in,out]  TransferLength    On input, it is message length in byte.
                                    On output, it is the length sent by USB.

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
RndisTransmitMessage (
  IN      EFI_USB_IO_PROTOCOL  *UsbIo,
  IN      UINT8                BulkOutEndpoint,
  IN      RNDIS_MSG_HEADER     *RndisMsg,
  IN OUT  UINTN                *TransferLength
  );

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
  );

/**
  Ask receive timer to receive data immediately.

  @param[in]      Private       Pointer to private data

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
VOID
RndisReceiveNow (
  IN USB_RNDIS_PRIVATE_DATA  *Private
  );

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
  );

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
  );

#endif //  RNDIS_H_
