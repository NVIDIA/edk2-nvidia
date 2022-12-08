/** @file

  MM MCTP protocol communication message definitions

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_MM_COMM_MSGS_H__
#define __MCTP_MM_COMM_MSGS_H__

#define MCTP_MM_DEVICE_NAME_LENGTH  16
#define MCTP_COMM_HEADER_SIZE       (OFFSET_OF (MCTP_COMM_HEADER, Data))

//
// MCTP protocol MM communications function codes
// Each function's payload structure type is the same label without _FUNCTION_
//
#define MCTP_COMM_FUNCTION_NOOP         0
#define MCTP_COMM_FUNCTION_INITIALIZE   1
#define MCTP_COMM_FUNCTION_GET_DEVICES  2
#define MCTP_COMM_FUNCTION_SEND         3
#define MCTP_COMM_FUNCTION_RECV         4
#define MCTP_COMM_FUNCTION_DO_REQUEST   5

typedef struct {
  UINTN         Function;
  EFI_STATUS    ReturnStatus;
  UINT8         Data[1];
} MCTP_COMM_HEADER;

typedef struct {
  UINT8     MmIndex;
  UINT8     Type;
  UINT8     Socket;
  CHAR16    Name[MCTP_MM_DEVICE_NAME_LENGTH];
} MCTP_MM_DEVICE_INFO;

typedef struct {
  // reply fields
  UINTN    NumDevices;
} MCTP_COMM_INITIALIZE;

typedef struct {
  // request fields
  UINTN                  MaxCount;
  // reply fields
  UINTN                  Count;
  MCTP_MM_DEVICE_INFO    Devices[1];
} MCTP_COMM_GET_DEVICES;

typedef struct {
  // request fields
  UINT8      MmIndex;
  BOOLEAN    IsRequest;
  UINT8      RspMsgTag;
  UINTN      Length;
  UINT8      ReqMsgTag; // reply field
  UINT8      Data[1];
} MCTP_COMM_SEND;

typedef struct {
  // request fields
  UINT8    MmIndex;
  UINTN    TimeoutMs;
  UINTN    MaxLength;
  // reply fields
  UINTN    Length;
  UINT8    MsgTag;
  UINT8    Data[1];
} MCTP_COMM_RECV;

typedef struct {
  // request fields
  UINT8    MmIndex;
  UINTN    RequestLength;
  UINTN    ResponseBufferLength;
  // reply fields
  UINTN    ResponseLength;
  UINT8    Data[1];     // reply/request field
} MCTP_COMM_DO_REQUEST;

#endif
