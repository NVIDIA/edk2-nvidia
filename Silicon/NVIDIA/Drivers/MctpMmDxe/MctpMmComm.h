/** @file

  MM MCTP protocol communication

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2010 - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_MM_COMM_H__
#define __MCTP_MM_COMM_H__

#include "MctpMmCommMsgs.h"

#define MCTP_COMM_BUFFER_SIZE  (32 * 1024)

EFI_STATUS
EFIAPI
MctpMmSendInitialize  (
  OUT UINTN  *NumDevices
  );

EFI_STATUS
EFIAPI
MctpMmSendGetDevices  (
  IN  UINTN                MaxCount,
  OUT UINTN                *Count,
  OUT MCTP_MM_DEVICE_INFO  *DeviceInfo
  );

EFI_STATUS
EFIAPI
MctpMmSendRecv (
  IN UINT8      MmIndex,
  IN UINTN      TimeoutMs,
  OUT VOID      *Message,
  IN OUT UINTN  *Length,
  IN OUT UINT8  *MsgTag
  );

EFI_STATUS
EFIAPI
MctpMmSendSend (
  IN UINT8       MmIndex,
  IN BOOLEAN     IsRequest,
  IN CONST VOID  *Message,
  IN UINTN       Length,
  OUT UINT8      *MsgTag
  );

EFI_STATUS
EFIAPI
MctpMmSendDoRequest (
  IN UINT8   MmIndex,
  IN  VOID   *Request,
  IN  UINTN  RequestLength,
  OUT VOID   *ResponseBuffer,
  IN  UINTN  ResponseBufferLength,
  OUT UINTN  *ResponseLength
  );

extern EFI_MM_COMMUNICATION2_PROTOCOL  *mMctpMmCommProtocol;
extern VOID                            *mMctpMmCommBuffer;
extern VOID                            *mMctpMmCommBufferPhysical;

#endif
