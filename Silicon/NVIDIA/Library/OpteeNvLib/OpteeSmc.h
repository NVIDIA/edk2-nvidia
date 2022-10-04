/** @file
  OP-TEE SMC header file.

  Copyright (c) 2022, NVIDIA Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef OPTEE_SMC_H_
#define OPTEE_SMC_H_

/* Returned in Arg0 only from Trusted OS functions */
#define OPTEE_SMC_RETURN_OK  0x0

#define OPTEE_SMC_RETURN_FROM_RPC           0x32000003
#define OPTEE_SMC_CALL_WITH_ARG             0x32000004
#define OPTEE_SMC_GET_SHARED_MEMORY_CONFIG  0xb2000007

#define OPTEE_SMC_SHARED_MEMORY_CACHED  1

#define OPTEE_SMC_RETURN_UNKNOWN_FUNCTION       0xffffffff
#define OPTEE_SMC_RETURN_RPC_PREFIX_MASK        0xffff0000
#define OPTEE_SMC_RETURN_RPC_PREFIX             0xffff0000
#define OPTEE_SMC_RETURN_RPC_FUNC_ALLOC         0xffff0000
#define OPTEE_SMC_RETURN_RPC_FUNC_FREE          0xffff0002
#define OPTEE_SMC_RETURN_RPC_FOREIGN_INTERRUPT  0xffff0004
#define OPTEE_SMC_RETURN_RPC_FUNC_CMD           0xffff0005

#define OPTEE_MESSAGE_ATTRIBUTE_META  0x100

#define OPTEE_LOGIN_PUBLIC  0x0

#define OPTEE_MSG_RPC_CMD_SHM_ALLOC     6
#define OPTEE_MSG_RPC_CMD_SHM_FREE      7
#define OPTEE_MSG_RPC_CMD_RPMB          1
#define OPTEE_MSG_RPC_CMD_NOTIFICATION  4

#define RPMB_GET_DEV_INFO  1
#define RPMB_DATA_REQ      0

#define RPMB_CMD_GET_DEV_INFO_RET_OK     0x00
#define RPMB_CMD_GET_DEV_INFO_RET_ERROR  0x01
#define EMMC_TRANS_TIMEOUT               2500 * 1000
#define RPMB_ST_SIZE                     (196)
#define RPMB_MAC_SIZE                    (32)
#define RPMB_DATA_SIZE                   (256)
#define RPMB_NONCE_SIZE                  (16)
#define RPMB_FRAME_SIZE                  (512)

#define RPMB_MSG_TYPE_REQ_AUTH_KEY_PROGRAM         0x0001
#define RPMB_MSG_TYPE_REQ_WRITE_COUNTER_VAL_READ   0x0002
#define RPMB_MSG_TYPE_REQ_AUTH_DATA_WRITE          0x0003
#define RPMB_MSG_TYPE_REQ_AUTH_DATA_READ           0x0004
#define RPMB_MSG_TYPE_REQ_RESULT_READ              0x0005
#define RPMB_MSG_TYPE_RESP_AUTH_KEY_PROGRAM        0x0100
#define RPMB_MSG_TYPE_RESP_WRITE_COUNTER_VAL_READ  0x0200
#define RPMB_MSG_TYPE_RESP_AUTH_DATA_WRITE         0x0300
#define RPMB_MSG_TYPE_RESP_AUTH_DATA_READ          0x0400

#define NOTIFICATION_MSG_WAIT  (0)
#define NOTIFICATION_MSG_WAKE  (1)

typedef struct {
  UINT64    PBase;
  UINT64    VBase;
  UINTN     Size;
} OPTEE_SHARED_MEMORY_INFORMATION;

//
// UUID struct compliant with RFC4122 (network byte order).
//
typedef struct {
  UINT32    Data1;
  UINT16    Data2;
  UINT16    Data3;
  UINT8     Data4[8];
} RFC4122_UUID;

typedef struct {
  UINT16    Cmd;
  UINT16    DevId;
  UINT16    BlockCount;
  /* RPMB Frames */
} RPMB_REQUEST;

typedef struct {
  UINT8    Cid[16];
  UINT8    RpmbSizeMult;
  UINT8    RelWrSecCount;
  UINT8    RetCode;
} RPMB_DEV_INFO;

typedef struct {
  UINT8    St[RPMB_ST_SIZE];
  UINT8    Mac[RPMB_MAC_SIZE];
  UINT8    Data[RPMB_DATA_SIZE];
  UINT8    Nonce[RPMB_NONCE_SIZE];
  UINT8    WrCounter[4];
  UINT8    Address[2];
  UINT8    BlockCount[2];
  UINT8    Result[2];
  UINT8    Request[2];
} RPMB_FRAME;

#endif // OPTEE_SMC_H_
