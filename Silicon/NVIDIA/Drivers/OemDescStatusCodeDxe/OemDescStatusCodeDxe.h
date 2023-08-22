/** @file

  OEM Status code handler to log addtional data as string

  SPDX-FileCopyrightText: Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __OEM_DESC_STATUS_CODE_DXE_H__
#define __OEM_DESC_STATUS_CODE_DXE_H__

#define IPMI_NETFN_OEM                 0x3C
#define IPMI_CMD_OEM_SEND_DESCRIPTION  0xD1

#define IPMI_OEM_DESC_MAX_LEN  256

#define VARIABLE_LEN  1

#define MAX_STAGED_OEM_DESC_ENTRIES  16

//
// IPMI OEM Send Description Request/Response structures
//
#pragma pack(1)
typedef struct {
  UINT32    EfiStatusCodeType;
  UINT32    EfiStatusCodeValue;
  CHAR8     Description[VARIABLE_LEN];
} IPMI_OEM_SEND_DESC_REQ_DATA;

typedef struct {
  UINT8    CompletionCode;
} IPMI_OEM_SEND_DESC_RSP_DATA;
#pragma pack()

typedef struct {
  IPMI_OEM_SEND_DESC_REQ_DATA    *RequestData;
  UINT32                         RequestDataSize;
} OEM_DESC_FIFO_ENTRY;

/**
  Helper function for sending OEM IPMI command

  @param[in]    RequestData     IPMI request data
  @param[in]    RequestDataSize IPMI request data size
  @param[in]    NumRetries      Number of retries if error
**/
EFI_STATUS
OemDescSend (
  IN  IPMI_OEM_SEND_DESC_REQ_DATA  *RequestData,
  IN  UINT32                       RequestDataSize,
  IN  INT32                        NumRetries
  );

#endif // __OEM_DESC_STATUS_CODE_DXE_H__
