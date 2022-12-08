/** @file

  PLDM base protocol and helper functions

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PLDM_BASE_LIB_H__
#define __PLDM_BASE_LIB_H__

#include <Uefi/UefiBaseType.h>

// PLDM InstanceID field bits
#define PLDM_RQ                0x80
#define PLDM_DATAGRAM          0x40
#define PLDM_ASYNC             0xc0
#define PLDM_INSTANCE_ID_MASK  0x3f

// PLDM Type field bits
#define PLDM_HDR_VER_MASK  0xc0
#define PLDM_TYPE_MASK     0x3f
#define PLDM_HDR_VER       0x00

// PLDM types
#define PLDM_TYPE_CONTROL    0x00
#define PLDM_TYPE_SMBIOS     0x01
#define PLDM_TYPE_PLATFORM   0x02
#define PLDM_TYPE_BIOS       0x03
#define PLDM_TYPE_FRU        0x04
#define PLDM_TYPE_FW_UPDATE  0x05
#define PLDM_TYPE_OEM        0x3f

// PLDM timing
// Number of request retries
#define PLDM_PN1_RETRIES  2
// Request-to-response time
#define PLDM_PT1_MS_MAX  100
// Timeout waiting for a response
#define PLDM_PT2_MS_MIN  (PLDM_PT1_MS_MAX + (2 * PLDM_PT4_MS_MAX))
#define PLDM_PT2_MS_MAX  (PLDM_PT3_MS_MIN - (2 * PLDM_PT4_MS_MAX))
// Instance id expiration interval
#define PLDM_PT3_MS_MIN  (5 * 1000)
#define PLDM_PT3_MS_MAX  (6 * 1000)
// Transmission delay
#define PLDM_PT4_MS_MAX  100

// PLDM base error codes
#define PLDM_SUCCESS                     0x00
#define PLDM_ERROR                       0x01
#define PLDM_ERROR_INVALID_DATA          0x02
#define PLDM_ERROR_INVALID_LENGTH        0x03
#define PLDM_ERROR_NOT_READY             0x04
#define PLDM_ERROR_UNSUPPORTED_PLDM_CMD  0x05
#define PLDM_ERROR_INVALID_PLDM_TYPE     0x20

#pragma pack (1)

typedef struct {
  UINT8    Data[13];
} PLDM_TIMESTAMP104;

typedef struct {
  UINT8    MctpType;
  UINT8    InstanceId;
  UINT8    PldmType;
  UINT8    Command;
} MCTP_PLDM_COMMON;

typedef struct {
  MCTP_PLDM_COMMON    Common;
} MCTP_PLDM_REQUEST_HEADER;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
} MCTP_PLDM_RESPONSE_HEADER;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               Payload[1];
} MCTP_PLDM_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
  UINT8               Payload[1];
} MCTP_PLDM_RESPONSE;

#pragma pack ()

/**
  Fill common fields in PLDM request payload

  @param[in]  Common        Pointer to PLDM common header structure.
  @param[in]  IsRequest     TRUE to build request header.
  @param[in]  InstanceId    InstanceId for header.
  @param[in]  PldmType      PLDM type for header.
  @param[in]  Command       PLDM command code for this header.

  @retval None

**/
VOID
EFIAPI
PldmFillCommon (
  IN  MCTP_PLDM_COMMON  *Common,
  IN  BOOLEAN           IsRequest,
  IN  UINT8             InstanceId,
  IN  UINT8             PldmType,
  IN  UINT8             Command
  );

#endif
