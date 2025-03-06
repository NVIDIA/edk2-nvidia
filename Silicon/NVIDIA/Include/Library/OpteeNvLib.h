/** @file
  OP-TEE specific header file.

  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef OPTEENV_LIB_H_
#define OPTEENV_LIB_H_

#define ARM_SMC_ID_TOS_CAPABILITIES  0xb2000009

/*
 * The 'Trusted OS Call UID' is supposed to return the following UUID for
 * OP-TEE OS. This is a 128-bit value.
 */
#define OPTEE_OS_UID0  0x384fb3e0
#define OPTEE_OS_UID1  0xe7f811e3
#define OPTEE_OS_UID2  0xaf630002
#define OPTEE_OS_UID3  0xa5d5c51b

#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_NONE           0x0
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT    0x1
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_OUTPUT   0x2
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INOUT    0x3
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INPUT   0x5
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_OUTPUT  0x6
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT   0x7
#define OPTEE_MESSAGE_ATTR_NONCONTIG                BIT9
#define OPTEE_MESSAGE_ATTR_TYPE_TMEM_INPUT          0x9
#define OPTEE_MESSAGE_ATTR_TYPE_TMEM_OUTPUT         0xa
#define OPTEE_MESSAGE_ATTR_TYPE_TMEM_INOUT          0xb

#define OPTEE_MESSAGE_COMMAND_OPEN_SESSION     0
#define OPTEE_MESSAGE_COMMAND_INVOKE_FUNCTION  1
#define OPTEE_MESSAGE_COMMAND_CLOSE_SESSION    2
#define OPTEE_MESSAGE_COMMAND_REGISTER_SHM     4
#define OPTEE_MESSAGE_COMMAND_UNREGISTER_SHM   5

/*
 * Values sent/obtained as part of the exchange capabilities SMC ID.
 */

#define OPTEE_SMC_NSEC_CAP_UNIPROCESSOR      BIT0
#define OPTEE_SMC_SEC_CAP_HAVE_RESERVED_SHM  BIT0
#define OPTEE_SMC_SEC_CAP_UNREGISTERED_SHM   BIT1
#define OPTEE_SMC_SEC_CAP_DYNAMIC_SHM        BIT2

#define OPTEE_MESSAGE_FUNCTION_STMM_COMMUNICATE  0

#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_MASK  0xff

#define OPTEE_SUCCESS               0x00000000
#define OPTEE_ORIGIN_COMMUNICATION  0x00000002
#define OPTEE_ERROR_COMMUNICATION   0xFFFF000E
#define OPTEE_ERROR_BAD_PARAMS      0xFFFF0006
#define OPTEE_ERROR_OUT_OF_MEMORY   0xFFFF000C

#define OPTEE_MSG_PAGE_SIZE  0x1000
#define MAX_PAGELIST_ENTRIES \
 ((OPTEE_MSG_PAGE_SIZE / sizeof(UINT64)) - 1)

typedef struct {
  UINT64    BufferAddress;
  UINT64    Size;
  UINT64    SharedMemoryReference;
} OPTEE_MESSAGE_PARAM_MEMORY;

typedef struct {
  UINT64    Offset;
  UINT64    Size;
  UINT64    SharedMemoryReference;
} OPTEE_MESSAGE_PARAM_RMEMORY;

typedef struct {
  UINT64    A;
  UINT64    B;
  UINT64    C;
} OPTEE_MESSAGE_PARAM_VALUE;

typedef union {
  OPTEE_MESSAGE_PARAM_MEMORY     Memory;
  OPTEE_MESSAGE_PARAM_RMEMORY    RMemory;
  OPTEE_MESSAGE_PARAM_VALUE      Value;
} OPTEE_MESSAGE_PARAM_UNION;

typedef struct {
  UINT64                       Attribute;
  OPTEE_MESSAGE_PARAM_UNION    Union;
} OPTEE_MESSAGE_PARAM;

#define OPTEE_MAX_CALL_PARAMS  4

typedef struct {
  UINT32                 Command;
  UINT32                 Function;
  UINT32                 Session;
  UINT32                 CancelId;
  UINT32                 Pad;
  UINT32                 Return;
  UINT32                 ReturnOrigin;
  UINT32                 NumParams;

  // NumParams tells the actual number of element in Params
  OPTEE_MESSAGE_PARAM    Params[OPTEE_MAX_CALL_PARAMS];
} OPTEE_MESSAGE_ARG;

typedef struct {
  EFI_GUID    Uuid;         // [in] GUID/UUID of the Trusted Application
  UINT32      Session;      // [out] Session id
  UINT32      Return;       // [out] Return value
  UINT32      ReturnOrigin; // [out] Origin of the return value
} OPTEE_OPEN_SESSION_ARG;

typedef struct {
  UINT32                 Function;                      // [in] Trusted Application function, specific to the TA
  UINT32                 Session;                       // [in] Session id
  UINT32                 Return;                        // [out] Return value
  UINT32                 ReturnOrigin;                  // [out] Origin of the return value
  OPTEE_MESSAGE_PARAM    Params[OPTEE_MAX_CALL_PARAMS]; // Params for function to be invoked
} OPTEE_INVOKE_FUNCTION_ARG;

typedef struct {
  UINT32    Size;
  VOID      *Addr;
} OPTEE_SHM_COOKIE;

typedef  struct  {
  UINT64    PagesArray[MAX_PAGELIST_ENTRIES];
  UINT64    NextPage;
} OPTEE_SHM_PAGE_LIST;

BOOLEAN
EFIAPI
IsOpteePresent (
  VOID
  );

EFI_STATUS
EFIAPI
OpteeInit (
  VOID
  );

EFI_STATUS
EFIAPI
OpteeOpenSession (
  IN OUT OPTEE_OPEN_SESSION_ARG  *OpenSessionArg
  );

EFI_STATUS
EFIAPI
OpteeCloseSession (
  IN UINT32  Session
  );

EFI_STATUS
EFIAPI
OpteeInvokeFunction (
  IN OUT OPTEE_INVOKE_FUNCTION_ARG  *InvokeFunctionArg
  );

EFI_STATUS
EFIAPI
OpteeRegisterShm (
  VOID                 *Buf,
  UINT64               SharedMemCookie,
  UINTN                Size,
  OPTEE_SHM_PAGE_LIST  *Shm
  );

BOOLEAN
EFIAPI
OpteeExchangeCapabilities (
  UINT64  *Cap
  );

UINT32
EFIAPI
OpteeCallWithArg (
  IN UINT64  PhysicalArg
  );

EFI_STATUS
EFIAPI
OpteeSetProperties (
  UINT64  PBuf,
  UINT64  VBuf,
  UINT64  Size
  );

EFI_STATUS
EFIAPI
OpteeSetShmCookie (
  UINT64  Cookie
  );

EFI_STATUS
EFIAPI
OpteeUnRegisterShm (
  UINT64  SharedMemCookie
  );

VOID
EFIAPI
HandleCmdRpmb (
  OPTEE_MESSAGE_ARG  *Msg
  );

VOID
EFIAPI
OpteeLibNotifyRuntime (
  BOOLEAN  Runtime
  );

#endif // OPTEENV_LIB_H
