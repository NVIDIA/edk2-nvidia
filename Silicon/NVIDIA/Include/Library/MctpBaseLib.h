/** @file

  MCTP base protocol definitions and helper function library

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_BASE_LIB_H__
#define __MCTP_BASE_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/MctpProtocol.h>

#define MCTP_BASELINE_TRANSMISSION_UNIT_SIZE  64
#define MCTP_CONTROL_MAX_BYTES                MCTP_BASELINE_TRANSMISSION_UNIT_SIZE

// MCTP completion codes
#define MCTP_SUCCESS             0x00
#define MCTP_ERROR               0x01
#define MCTP_INVALID_DATA        0x02
#define MCTP_INVALID_LENGTH      0x03
#define MCTP_NOT_READY           0x04
#define MCTP_UNSUPPORTED_CMD     0x05
#define MCTP_CMD_SPECIFIC_START  0x80

// MCTP transport header
#define MCTP_TRANSPORT_SOM                    0x80
#define MCTP_TRANSPORT_EOM                    0x40
#define MCTP_TRANSPORT_PACKET_SEQUENCE        0x30
#define MCTP_TRANSPORT_PACKET_SEQUENCE_MASK   0x3
#define MCTP_TRANSPORT_PACKET_SEQUENCE_SHIFT  4
#define MCTP_TRANSPORT_TO                     0x08
#define MCTP_TRANSPORT_MESSAGE_TAG_MASK       0x7

// MCTP Type field bits
#define MCTP_INTEGRITY_CHECK_NONE  0x0
#define MCTP_INTEGRITY_CHECK       0x80
#define MCTP_TYPE_MASK             0x7f
#define MCTP_TYPE_CONTROL          0x00
#define MCTP_TYPE_PLDM             0x01
#define MCTP_TYPE_VENDOR_IANA      0x7f

// MCTP InstanceID field bits
#define MCTP_RQ                0x80
#define MCTP_DATAGRAM          0x40
#define MCTP_ASYNC             0xc0
#define MCTP_INSTANCE_ID_MASK  0x3f

// MCTP control commands
#define MCTP_CONTROL_SET_ENDPOINT_ID  0x01

// Set Endpoint definitions
#define MCTP_SET_ENDPOINT_OPERATION_SET_EID         0x00
#define MCTP_SET_ENDPOINT_OPERATION_FORCE_EID       0x01
#define MCTP_SET_ENDPOINT_OPERATION_RESET_EID       0x02
#define MCTP_SET_ENDPOINT_OPERATION_SET_DISCOVERED  0x03

#pragma pack(1)

typedef struct {
  UINT8    HdrVer;
  UINT8    DstEID;
  UINT8    SrcEID;
  UINT8    Control;
} MCTP_TRANSPORT_HEADER;

typedef struct {
  UINT8    Id[4];
} MCTP_VDM_IANA_VENDOR_ID;

typedef struct {
  UINT8    Type;
  UINT8    InstanceId;
  UINT8    Command;
} MCTP_CONTROL_COMMON;

typedef struct {
  MCTP_CONTROL_COMMON    Common;
  UINT8                  Data[1];
} MCTP_CONTROL_REQUEST;

typedef struct {
  MCTP_CONTROL_COMMON    Common;
  UINT8                  CompletionCode;
  UINT8                  Data[1];
} MCTP_CONTROL_RESPONSE;

typedef struct {
  UINT8    Type;
  UINT8    Data[1];
} MCTP_PLDM_MESSAGE;

typedef struct {
  MCTP_CONTROL_COMMON    Common;
  UINT8                  Operation;
  UINT8                  EndpointId;
} MCTP_SET_ENDPOINT_REQUEST;

typedef struct {
  MCTP_CONTROL_COMMON    Common;
  UINT8                  CompletionCode;
  UINT8                  Status;
  UINT8                  EndpointId;
  UINT8                  EIDPoolSize;
} MCTP_SET_ENDPOINT_RESPONSE;

#pragma pack()

/**
  Fill common fields in an MCTP control request payload

  @param[in]  Common        Pointer to MCTP control header structure.
  @param[in]  Command       Command code for this request.

  @retval None

**/
VOID
EFIAPI
MctpControlReqFillCommon (
  IN  MCTP_CONTROL_COMMON  *Common,
  IN  UINT8                Command
  );

/**
  Return 32-bit value from big endian buffer.

  @param[in]  Buffer        Pointer to buffer containing 32-bit big endian value.

  @retval UINT32            Value extracted from buffer.

**/
UINT32
EFIAPI
MctpBEBufferToUint32 (
  IN CONST UINT8  *Buffer
  );

/**
  Return 16-bit value from big endian buffer.

  @param[in]  Buffer        Pointer to buffer containing 16-bit big endian value.

  @retval UINT32            Value extracted from buffer.

**/
UINT16
EFIAPI
MctpBEBufferToUint16 (
  IN CONST UINT8  *Buffer
  );

/**
  Put 32-bit value into big endian buffer.

  @param[in]  Buffer        Pointer to big endian buffer.
  @param[in]  Value         Value to put into buffer.

  @retval None

**/
VOID
EFIAPI
MctpUint32ToBEBuffer (
  OUT UINT8   *Buffer,
  IN  UINT32  Value
  );

/**
  Put 16-bit value into big endian buffer.

  @param[in]  Buffer        Pointer to big endian buffer.
  @param[in]  Value         Value to put into buffer.

  @retval None

**/
VOID
EFIAPI
MctpUint16ToBEBuffer (
  UINT8   *Buffer,
  UINT16  Value
  );

#endif
