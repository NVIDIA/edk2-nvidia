/** @file

  MCTP protocol

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_PROTOCOL_H__
#define __MCTP_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>

#define DEVICE_TYPE_UNKNOWN  0
#define DEVICE_TYPE_EROT     1

#define NVIDIA_MCTP_PROTOCOL_GUID \
  {0x22dfe80e, 0x712f, 0x4c6c, {0x91, 0xd7, 0xa6, 0x15, 0xd7, 0xce, 0xb4, 0x1d}}

typedef struct _NVIDIA_MCTP_PROTOCOL NVIDIA_MCTP_PROTOCOL;

typedef struct {
  CONST CHAR16    *DeviceName;
  UINT8           DeviceType;
  UINT8           Socket;
} MCTP_DEVICE_ATTRIBUTES;

/**
  Get attributes of MCTP device.

  @param[in]  This                 Instance of protocol for device.
  @param[out] Attributes           Pointer to return attributes structure.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
typedef
EFI_STATUS
(EFIAPI *MCTP_GET_DEVICE_ATTRIBUTES)(
  IN  NVIDIA_MCTP_PROTOCOL    *This,
  OUT MCTP_DEVICE_ATTRIBUTES  *Attributes
  );

/**
  Do MCTP request to device.

  @param[in]  This                 Instance of protocol for device.
  @param[in]  Request              Pointer to request message.
  @param[in]  RequestLength        Length of request message.
  @param[out] ResponseBuffer       Pointer to response buffer.
  @param[in]  ResponseBufferLength Length of response buffer.
  @param[out] ResponseLength       Pointer to return response length.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
typedef
EFI_STATUS
(EFIAPI *MCTP_DO_REQUEST)(
  IN  NVIDIA_MCTP_PROTOCOL  *This,
  IN  VOID                  *Request,
  IN  UINTN                 RequestLength,
  OUT VOID                  *ResponseBuffer,
  IN  UINTN                 ResponseBufferLength,
  OUT UINTN                 *ResponseLength
  );

/**
  Send MCTP message to device.

  @param[in]     This       Instance of protocol for device.
  @param[in]     IsRequest  Flag TRUE if this is an MCTP request.
  @param[in]     Message    Pointer to MCTP message buffer.
  @param[in]     Length     Message length in bytes.
  @param[in out] MsgTag     Pointer to message tag (input parameter if !IsRequest,
                            output parameter if IsRequest).

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
typedef
EFI_STATUS
(EFIAPI *MCTP_SEND)(
  IN  NVIDIA_MCTP_PROTOCOL      *This,
  IN  BOOLEAN                   IsRequest,
  IN  CONST VOID                *Message,
  IN  UINTN                     Length,
  IN OUT UINT8                  *MsgTag
  );

/**
  Receive MCTP message from device.

  @param[in]     This       Instance of protocol for device.
  @param[in]     TimeoutMs  Timeout in ms to wait for device to send message.
  @param[out]    Message    Pointer to MCTP message buffer.
  @param[in out] Length     Pointer to message length in bytes.  As input
                            parameter, is length of Message buffer.  As output
                            parameter, is length of message received.
  @param[out]    MsgTag     Pointer to store message tag from received message.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
typedef
EFI_STATUS
(EFIAPI *MCTP_RECV)(
  IN  NVIDIA_MCTP_PROTOCOL      *This,
  IN  UINTN                     TimeoutMs,
  OUT VOID                      *Message,
  IN OUT UINTN                  *Length,
  OUT UINT8                     *MsgTag
  );

// protocol interface
struct _NVIDIA_MCTP_PROTOCOL {
  MCTP_GET_DEVICE_ATTRIBUTES    GetDeviceAttributes;
  MCTP_DO_REQUEST               DoRequest;
  MCTP_SEND                     Send;
  MCTP_RECV                     Recv;
};

extern EFI_GUID  gNVIDIAMctpProtocolGuid;

#endif
