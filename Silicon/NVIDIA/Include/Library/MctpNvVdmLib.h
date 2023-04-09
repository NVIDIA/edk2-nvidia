/** @file

  MCTP NVIDIA Vendor-defined message library

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_NV_VDM_LIB_H__
#define __MCTP_NV_VDM_LIB_H__

#include <Library/MctpBaseLib.h>

#define MCTP_NV_VDM_MAX_BYTES   64
#define MCTP_NV_NVIDIA_IANA_ID  0x1647
#define MCTP_NV_TYPE_EROT       0x1

#define MCTP_NV_CMD_BOOT_COMPLETE  0x2
#define MCTP_NV_VER_BOOT_COMPLETE  0x2

#define MCTP_NV_BOOT_COMPLETE_SLOT_VALID    0x4
#define MCTP_NV_BOOT_COMPLETE_SLOT_INVALID  0x3

#pragma pack(1)

typedef struct {
  UINT8                      Type;
  MCTP_VDM_IANA_VENDOR_ID    Vendor;
  UINT8                      InstanceId;
  UINT8                      NvType;
  UINT8                      Command;
  UINT8                      Version;
} MCTP_NV_VDM_COMMON;

typedef struct {
  MCTP_NV_VDM_COMMON    Common;
} MCTP_NV_VDM_REQUEST_HEADER;

typedef struct {
  MCTP_NV_VDM_COMMON    Common;
  UINT8                 CompletionCode;
} MCTP_NV_VDM_RESPONSE_HEADER;

typedef struct {
  MCTP_NV_VDM_COMMON    Common;
  UINT8                 Data[1];
} MCTP_NV_VDM_REQUEST;

typedef struct {
  MCTP_NV_VDM_COMMON    Common;
  UINT8                 CompletionCode;
  UINT8                 Data[1];
} MCTP_NV_VDM_RESPONSE;

typedef struct {
  MCTP_NV_VDM_COMMON    Common;
  UINT8                 BootSlot;
  UINT8                 Reserved[2];
} MCTP_NV_BOOT_COMPLETE_REQUEST;

typedef MCTP_NV_VDM_RESPONSE_HEADER MCTP_NV_BOOT_COMPLETE_RESPONSE;

#pragma pack()

/**
  Fill common fields in an MCTP control request payload

  @param[in]  Vendor        Pointer to Vendor field

  @retval None

**/
VOID
EFIAPI
MctpNvFillVendorId (
  IN  MCTP_VDM_IANA_VENDOR_ID  *Vendor
  );

/**
  Fill common fields in an NVIDIA VDM MCTP request header

  @param[in]  Common        Pointer to VDM control header structure.
  @param[in]  Command       Command code for this request.
  @param[in]  Version       NVIDIA command version.

  @retval None

**/
VOID
EFIAPI
MctpNvReqFillCommon (
  IN  MCTP_NV_VDM_COMMON  *Common,
  IN  UINT8               Command,
  IN  UINT8               Version
  );

/**
  Fill fields in an NVIDIA BootComplete request message

  @param[out] Request       Pointer to the request message.
  @param[in]  BootSlot      Boot slot value for request.

  @retval None

**/
VOID
EFIAPI
MctpNvBootCompleteFillReq (
  OUT MCTP_NV_BOOT_COMPLETE_REQUEST  *Request,
  IN UINTN                           BootSlot
  );

#endif
