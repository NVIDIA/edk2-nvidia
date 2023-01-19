/** @file
  BPMP IPC Protocol

  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BPMP_IPC_PROTOCOL_H__
#define __BPMP_IPC_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_BPMP_IPC_PROTOCOL_GUID \
  { \
  0x2b560a1f, 0x8e7b, 0x45a0, { 0x96, 0x8f, 0x7c, 0xa8, 0x2b, 0xd2, 0xb5, 0x99 } \
  }

/**
 * @ingroup MRQ_Codes
 * @name Legal MRQ codes
 * These are the legal values for mrq_request::mrq
 * @{
 */

#define MRQ_PING              0
#define MRQ_QUERY_TAG         1
#define MRQ_MODULE_LOAD       4
#define MRQ_MODULE_UNLOAD     5
#define MRQ_TRACE_MODIFY      7
#define MRQ_WRITE_TRACE       8
#define MRQ_THREADED_PING     9
#define MRQ_MODULE_MAIL       11
#define MRQ_DEBUGFS           19
#define MRQ_RESET             20
#define MRQ_I2C               21
#define MRQ_CLK               22
#define MRQ_QUERY_ABI         23
#define MRQ_PG_READ_STATE     25
#define MRQ_PG_UPDATE_STATE   26
#define MRQ_THERMAL           27
#define MRQ_CPU_VHINT         28
#define MRQ_ABI_RATCHET       29
#define MRQ_EMC_DVFS_LATENCY  31
#define MRQ_TRACE_ITER        64
#define MRQ_PG                66
#define MRQ_CPU_NDIV_LIMITS   67
#define MRQ_TELEMETRY         80
#define MRQ_C2C               85

#define   BPMP_ENOENT      (-2)   // No such file or directory.
#define   BPMP_ENOHANDLER  (-3)   // No MRQ handler.
#define   BPMP_EIO         (-5)   // I/O error.
#define   BPMP_EBADCMD     (-6)   // Bad sub-MRQ command.
#define   BPMP_ENOMEM      (-12)  // Not enough memory.
#define   BPMP_EACCES      (-13)  // Permission denied.
#define   BPMP_EFAULT      (-14)  // Bad address.
#define   BPMP_ENODEV      (-19)  // No such device.
#define   BPMP_EISDIR      (-21)  // Argument is a directory.
#define   BPMP_EINVAL      (-22)  // Invalid argument.
#define   BPMP_ETIMEDOUT   (-23)  // Timeout during operation.
#define   BPMP_ERANGE      (-34)  // Out of range.
#define   BPMP_ENOSYS      (-38)  // Function not implemented.
#define   BPMP_EBADSLT     (-57)  // Invalid slot.

//
// Define for forward reference.
//
typedef struct _NVIDIA_BPMP_IPC_PROTOCOL NVIDIA_BPMP_IPC_PROTOCOL;

typedef struct {
  ///
  /// Event that will be signaled when the IPC request is completed.
  ///
  EFI_EVENT     Event;

  ///
  /// Defines whether or not the signaled event encountered an error.
  ///
  EFI_STATUS    TransactionStatus;
} NVIDIA_BPMP_IPC_TOKEN;

/**
  This function allows for a remote IPC to the BPMP firmware to be executed.

  @param[in]     This                The instance of the NVIDIA_BPMP_IPC_PROTOCOL.
  @param[in,out] Token               Optional pointer to a token structure, if this is NULL
                                     this API will process IPC in a blocking manner.
  @param[in]     MessageRequest      Id of the message to send
  @param[in]     TxData              Pointer to the payload data to send
  @param[in]     TxDataSize          Size of the TxData buffer
  @param[out]    RxData              Pointer to the payload data to receive
  @param[in]     RxDataSize          Size of the RxData buffer
  @param[out]    MessageError        If not NULL, will contain the BPMP error code on return

  @return EFI_SUCCESS               If Token is not NULL IPC has been queued.
  @return EFI_SUCCESS               If Token is NULL IPC has been completed.
  @return EFI_INVALID_PARAMETER     Token is not NULL but Token->Event is NULL
  @return EFI_INVALID_PARAMETER     TxData or RxData are NULL
  @return EFI_DEVICE_ERROR          Failed to send IPC
  @return EFI_UNSUPPORTED           BPMP IPC is not supported on this system
**/
typedef
EFI_STATUS
(EFIAPI *BPMP_IPC_COMMUNICATE)(
  IN  NVIDIA_BPMP_IPC_PROTOCOL   *This,
  IN  OUT NVIDIA_BPMP_IPC_TOKEN  *Token, OPTIONAL
  IN  UINT32                     BpmpPhandle,
  IN  UINT32                     MessageRequest,
  IN  VOID                       *TxData,
  IN  UINTN                      TxDataSize,
  OUT VOID                       *RxData,
  IN  UINTN                      RxDataSize,
  IN  INT32                      *MessageError OPTIONAL
  );

/// NVIDIA_BPMP_IPC_PROTOCOL protocol structure.
struct _NVIDIA_BPMP_IPC_PROTOCOL {
  BPMP_IPC_COMMUNICATE    Communicate;
};

extern EFI_GUID  gNVIDIABpmpIpcProtocolGuid;

#endif
