/** @file

  OEM Status code handler to log addtional data as string

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __OEM_DESC_STATUS_CODE_DXE_H__
#define __OEM_DESC_STATUS_CODE_DXE_H__

#define IPMI_NETFN_OEM                 0x3C
#define IPMI_CMD_OEM_SEND_DESCRIPTION  0xD1

#define IPMI_OEM_DESC_MAX_LEN  256

#define VARIABLE_LEN  1

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

#endif // __OEM_DESC_STATUS_CODE_DXE_H__
