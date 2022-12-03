/** @file

  BmpIpc private structures

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BPMP_IPC_PRIVATE_H__
#define __BPMP_IPC_PRIVATE_H__

#include <Protocol/BpmpIpc.h>
#include "HspDoorbellPrivate.h"

typedef struct {
  UINT32    WriteCount;
  UINT32    State;
  UINT32    WriteReserved[14];

  UINT32    ReadCount;
  UINT32    ReadReserved[15];

  UINT32    MessageRequest;
  UINT32    Flags;
  UINT8     Data[0];
} IVC_CHANNEL;

typedef enum {
  IvcStateEstablished,
  IvcStateSync,
  IvcStateAck,
  IvcStateMax
} IVC_STATE;

#define IVC_DATA_SIZE_BYTES      120
#define IVC_FLAGS_DO_ACK         BIT0
#define IVC_FLAGS_RING_DOORBELL  BIT1
//
// Transaction linked list
//
#define BPMP_PENDING_TRANSACTION_SIGNATURE  SIGNATURE_32('B','P','M','T')

typedef struct {
  //
  // Signature used to indentify data
  //
  UINT32                   Signature;

  //
  // List Entry
  //
  LIST_ENTRY               Link;

  //
  // Transaction data
  //
  NVIDIA_BPMP_IPC_TOKEN    *Token;
  UINT32                   MessageRequest;
  VOID                     *TxData;
  UINTN                    TxDataSize;
  VOID                     *RxData;
  UINTN                    RxDataSize;
  BOOLEAN                  Blocking;
  INT32                    *MessageError;
} BPMP_PENDING_TRANSACTION;

#define BPMP_PENDING_TRANSACTION_FROM_LINK(a)  CR(a, BPMP_PENDING_TRANSACTION, Link, BPMP_PENDING_TRANSACTION_SIGNATURE)

#define BPMP_IPC_SIGNATURE  SIGNATURE_32('B','P','M','P')

//
// Private data structure for channel and doorbell info.
//
typedef struct {
  volatile IVC_CHANNEL    *RxChannel;

  volatile IVC_CHANNEL    *TxChannel;

  UINT32                  BpmpPhandle;

  UINT32                  HspPhandle;

  EFI_PHYSICAL_ADDRESS    HspDoorbellLocation[HspDoorbellMax];
} NVIDIA_BPMP_MRQ_CHANNEL;

typedef struct {
  //
  // Standard signature used to identify BpmpIpc private data
  //
  UINT32                      Signature;

  //
  // Protocol instance of NVIDIA_BPMP_IPC_PROTOCOL produced by this driver
  //
  NVIDIA_BPMP_IPC_PROTOCOL    BpmpIpcProtocol;

  //
  // Indicates if the BpmpIpcProtocol is installed
  //
  BOOLEAN                     ProtocolInstalled;

  //
  // Controller handler
  //
  EFI_HANDLE                  Controller;

  //
  // Driver binding handle
  //
  EFI_HANDLE                  DriverBindingHandle;

  //
  // Number of BPMP Nodes
  //
  UINT32                      DeviceCount;

  //
  // MRQ Channels
  //
  NVIDIA_BPMP_MRQ_CHANNEL     *Channels;

  //
  // Current/Active Channel Index
  //
  UINT32                      ActiveChannel;

  //
  // Pending Transaction Linked List
  //
  LIST_ENTRY                  TransactionList;

  //
  // Timer event
  //
  EFI_EVENT                   TimerEvent;
} NVIDIA_BPMP_IPC_PRIVATE_DATA;

#define BPMP_IPC_PRIVATE_DATA_FROM_THIS(a)  CR(a, NVIDIA_BPMP_IPC_PRIVATE_DATA, BpmpIpcProtocol, BPMP_IPC_SIGNATURE)

#endif
