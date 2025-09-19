/** @file
  BpmpIpc protocol implementation for BPMP IPC driver.

  SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BpmpIpcDxePrivate.h"
#include "BpmpIpcPrivate.h"
#include <Library/ArmLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>

#define BOTH_ALIGNED(a, b, align)  ((((UINTN)(a) | (UINTN)(b)) & ((align) - 1)) == 0)
#define PLATFORM_MAX_SOCKETS       (PcdGet32 (PcdTegraMaxSockets))
#define BPMP_IPC_COMM_BUFFER_SIZE  SIZE_4KB

/**
  Copy Length bytes from Source to Destination, using mmio accesses for specified direction.

  @param   DestinationBuffer The target of the copy request.
  @param   SourceBuffer      The place to copy from.
  @param   Length            The number of bytes to copy.
  @param   ReadFromMmio      TRUE if SourceBuffer is Mmio bugger

**/
STATIC
VOID
MmioCopyMem (
  OUT     VOID        *DestinationBuffer,
  IN      CONST VOID  *SourceBuffer,
  IN      UINTN       Length,
  IN      BOOLEAN     ReadFromMmio
  )
{
  UINTN  AlignedLength;

  if (BOTH_ALIGNED (DestinationBuffer, SourceBuffer, 8) && (Length >= 8)) {
    AlignedLength = Length & ~0x7;
    if (ReadFromMmio) {
      MmioReadBuffer64 ((UINTN)SourceBuffer, AlignedLength, DestinationBuffer);
    } else {
      MmioWriteBuffer64 ((UINTN)DestinationBuffer, AlignedLength, SourceBuffer);
    }

    Length            -= AlignedLength;
    DestinationBuffer += AlignedLength;
    SourceBuffer      += AlignedLength;
  }

  if (BOTH_ALIGNED (DestinationBuffer, SourceBuffer, 4) && (Length >= 4)) {
    AlignedLength = Length & ~0x3;
    if (ReadFromMmio) {
      MmioReadBuffer32 ((UINTN)SourceBuffer, AlignedLength, DestinationBuffer);
    } else {
      MmioWriteBuffer32 ((UINTN)DestinationBuffer, AlignedLength, SourceBuffer);
    }

    Length            -= AlignedLength;
    DestinationBuffer += AlignedLength;
    SourceBuffer      += AlignedLength;
  }

  if (BOTH_ALIGNED (DestinationBuffer, SourceBuffer, 2) && (Length >= 2)) {
    AlignedLength = Length & ~0x1;
    if (ReadFromMmio) {
      MmioReadBuffer16 ((UINTN)SourceBuffer, AlignedLength, DestinationBuffer);
    } else {
      MmioWriteBuffer16 ((UINTN)DestinationBuffer, AlignedLength, SourceBuffer);
    }

    Length            -= AlignedLength;
    DestinationBuffer += AlignedLength;
    SourceBuffer      += AlignedLength;
  }

  if (Length != 0) {
    if (ReadFromMmio) {
      MmioReadBuffer8 ((UINTN)SourceBuffer, Length, DestinationBuffer);
    } else {
      MmioWriteBuffer8 ((UINTN)DestinationBuffer, Length, SourceBuffer);
    }
  }

  return;
}

/**
  This returns true if the channel is free

  @param Channel                    Pointer to ivc channel.

  @return TRUE                      Channel is free
  @return FALSE                     Channel is in use
**/
BOOLEAN
EFIAPI
ChannelFree (
  IN volatile IVC_CHANNEL  *Channel
  )
{
  UINT32  TransferCount = Channel->WriteCount - Channel->ReadCount;

  // If excess writes are seen then treat as free
  return (TransferCount != 1);
}

/**
  Free up the transaction memory

  @param Transaction                   Pointer to Transaction.

**/
VOID
EFIAPI
TransactionFree (
  IN BPMP_PENDING_TRANSACTION  *Transaction
  )
{
  if (Transaction == NULL) {
    return;
  }

  // Blocking call uses local pending transaction structure off of
  // the stack. No need to free that.
  if (!Transaction->Blocking) {
    FreePool (Transaction);
  }
}

/**
  This processes the next entry in the list

  @param PrivateData                    Pointer to private data.

**/
VOID
EFIAPI
ProcessTransaction (
  IN NVIDIA_BPMP_IPC_PRIVATE_DATA  *PrivateData
  )
{
  EFI_TPL                   OldTpl;
  LIST_ENTRY                *List;
  BPMP_PENDING_TRANSACTION  *Transaction;
  UINT64                    TimerTick;
  EFI_STATUS                Status;
  BOOLEAN                   ListEmpty;
  UINT32                    CIndex;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  List   = GetFirstNode (&PrivateData->TransactionList);
  gBS->RestoreTPL (OldTpl);

  // List is empty
  if (List == &PrivateData->TransactionList) {
    return;
  }

  Transaction = BPMP_PENDING_TRANSACTION_FROM_LINK (List);
  if (Transaction == NULL) {
    return;
  }

  CIndex = PrivateData->ActiveChannel;
  // Validate channels are empty
  if (!ChannelFree (PrivateData->Channels[CIndex].RxChannel) || !ChannelFree (PrivateData->Channels[CIndex].TxChannel)) {
    DEBUG ((DEBUG_ERROR, "%a: Channel not idle\r\n", __FUNCTION__));
    ASSERT (FALSE);

    OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
    RemoveEntryList (List);
    ListEmpty = IsListEmpty (&PrivateData->TransactionList);
    gBS->RestoreTPL (OldTpl);

    gBS->SignalEvent (Transaction->Token->Event);
    TransactionFree (Transaction);

    if (!ListEmpty) {
      ProcessTransaction (PrivateData);
    }

    return;
  }

  // Copy to Tx channel
  PrivateData->Channels[CIndex].TxChannel->MessageRequest = Transaction->MessageRequest;
  PrivateData->Channels[CIndex].TxChannel->Flags          = IVC_FLAGS_DO_ACK;
  MmioCopyMem ((VOID *)PrivateData->Channels[CIndex].TxChannel->Data, Transaction->TxData, Transaction->TxDataSize, FALSE);

  PrivateData->Channels[CIndex].TxChannel->WriteCount++;
  ArmDataMemoryBarrier ();

  Status = HspDoorbellRingDoorbell (PrivateData->Channels[CIndex].HspDoorbellLocation);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to ring doorbell: %r\r\n", __FUNCTION__, Status));
  }

  // Wait for done
  if (!Transaction->Blocking) {
    TimerTick = BPMP_POLL_INTERVAL;
    Status    = gBS->SetTimer (
                       PrivateData->TimerEvent,
                       TimerPeriodic,
                       TimerTick
                       );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set timer:%r\r\n", __FUNCTION__, Status));

      OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
      RemoveEntryList (List);
      ListEmpty = IsListEmpty (&PrivateData->TransactionList);
      gBS->RestoreTPL (OldTpl);

      gBS->SignalEvent (Transaction->Token->Event);
      TransactionFree (Transaction);

      if (!ListEmpty) {
        ProcessTransaction (PrivateData);
      }

      return;
    }
  }
}

/**
  This routine is called to notify system that HspDoorbell protocol is availible.

  @param Event                      Event that was notified
  @param Context                    Pointer to private data.

**/
VOID
EFIAPI
BpmpIpcTimerNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  NVIDIA_BPMP_IPC_PRIVATE_DATA  *PrivateData = (NVIDIA_BPMP_IPC_PRIVATE_DATA *)Context;
  EFI_TPL                       OldTpl;
  LIST_ENTRY                    *List;
  BPMP_PENDING_TRANSACTION      *Transaction;
  BOOLEAN                       ListEmpty;
  UINT32                        CIndex;

  if (NULL == PrivateData) {
    return;
  }

  CIndex = PrivateData->ActiveChannel;
  if (ChannelFree (PrivateData->Channels[CIndex].RxChannel)) {
    return;
  }

  ArmDataMemoryBarrier ();

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  List   = GetFirstNode (&PrivateData->TransactionList);
  gBS->RestoreTPL (OldTpl);

  // List is empty
  if (List == &PrivateData->TransactionList) {
    return;
  }

  Transaction = BPMP_PENDING_TRANSACTION_FROM_LINK (List);
  if (Transaction == NULL) {
    return;
  }

  gBS->SetTimer (
         PrivateData->TimerEvent,
         TimerCancel,
         0
         );

  if (NULL != Transaction->MessageError) {
    *Transaction->MessageError = PrivateData->Channels[CIndex].RxChannel->MessageRequest;
  }

  if (PrivateData->Channels[CIndex].RxChannel->MessageRequest != 0) {
    Transaction->Token->TransactionStatus = EFI_PROTOCOL_ERROR;
  } else {
    Transaction->Token->TransactionStatus = EFI_SUCCESS;
  }

  MmioCopyMem (Transaction->RxData, (VOID *)PrivateData->Channels[CIndex].RxChannel->Data, Transaction->RxDataSize, TRUE);

  PrivateData->Channels[CIndex].RxChannel->ReadCount++;

  ArmDataMemoryBarrier ();

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  RemoveEntryList (List);
  ListEmpty = IsListEmpty (&PrivateData->TransactionList);
  gBS->RestoreTPL (OldTpl);

  gBS->SignalEvent (Transaction->Token->Event);
  TransactionFree (Transaction);

  if (!ListEmpty) {
    ProcessTransaction (PrivateData);
  }
}

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
**/
EFI_STATUS
BpmpIpcCommunicate (
  IN  NVIDIA_BPMP_IPC_PROTOCOL *This,
  IN  OUT NVIDIA_BPMP_IPC_TOKEN *Token, OPTIONAL
  IN  UINT32                     BpmpPhandle,
  IN  UINT32                     MessageRequest,
  IN  VOID                       *TxData,
  IN  UINTN                      TxDataSize,
  OUT VOID                       *RxData,
  IN  UINTN                      RxDataSize,
  IN  INT32                      *MessageError OPTIONAL
  )
{
  NVIDIA_BPMP_IPC_TOKEN         LocalToken;
  BPMP_PENDING_TRANSACTION      LocalPendingTransaction;
  BOOLEAN                       Blocking = FALSE;
  EFI_STATUS                    Status;
  EFI_TPL                       OldTpl;
  EFI_TPL                       EntryTpl;
  NVIDIA_BPMP_IPC_PRIVATE_DATA  *PrivateData        = NULL;
  BPMP_PENDING_TRANSACTION      *PendingTransaction = NULL;
  BOOLEAN                       NeedQueue           = FALSE;
  UINT32                        ChannelNo           = 0;

  if (NULL == This) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&LocalToken, sizeof (NVIDIA_BPMP_IPC_TOKEN));

  PrivateData = BPMP_IPC_PRIVATE_DATA_FROM_THIS (This);

  if ((Token != NULL) && (Token->Event == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  while (ChannelNo < PrivateData->DeviceCount) {
    if (BpmpPhandle == PrivateData->Channels[ChannelNo].BpmpPhandle) {
      break;
    }

    ChannelNo++;
  }

  if (ChannelNo >= PrivateData->DeviceCount) {
    if (PLATFORM_MAX_SOCKETS == 1) {
      ChannelNo = 0;
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Invalid Bpmp device phandle: %u\n", __FUNCTION__, BpmpPhandle));
      return EFI_INVALID_PARAMETER;
    }
  }

  PrivateData->ActiveChannel = ChannelNo;

  if (((TxData != NULL) && (TxDataSize == 0)) ||
      ((TxData == NULL) && (TxDataSize != 0)) ||
      (TxDataSize > IVC_DATA_SIZE_BYTES) ||
      ((RxData != NULL) && (RxDataSize == 0)) ||
      ((RxData == NULL) && (RxDataSize != 0)) ||
      (RxDataSize > IVC_DATA_SIZE_BYTES))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (Token == NULL) {
    Blocking           = TRUE;
    PendingTransaction = &LocalPendingTransaction;
    Token              = &LocalToken;
    Status             = gBS->CreateEvent (
                                0,
                                TPL_NOTIFY,
                                NULL,
                                NULL,
                                &Token->Event
                                );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  } else {
    PendingTransaction = (BPMP_PENDING_TRANSACTION *)AllocateZeroPool (sizeof (BPMP_PENDING_TRANSACTION));

    if (NULL == PendingTransaction) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  PendingTransaction->Signature      = BPMP_PENDING_TRANSACTION_SIGNATURE;
  PendingTransaction->Token          = Token;
  PendingTransaction->MessageRequest = MessageRequest;
  PendingTransaction->TxData         = TxData;
  PendingTransaction->TxDataSize     = TxDataSize;
  PendingTransaction->RxData         = RxData;
  PendingTransaction->RxDataSize     = RxDataSize;
  PendingTransaction->Blocking       = Blocking;
  PendingTransaction->MessageError   = MessageError;

  if (Blocking) {
    // prevent threaded device discovery callbacks until this blocking call completes
    EntryTpl = gBS->RaiseTPL (TPL_NOTIFY);
  }

  OldTpl    = gBS->RaiseTPL (TPL_NOTIFY);
  NeedQueue = IsListEmpty (&PrivateData->TransactionList);
  InsertTailList (&PrivateData->TransactionList, &PendingTransaction->Link);
  gBS->RestoreTPL (OldTpl);

  if (NeedQueue) {
    ProcessTransaction (PrivateData);
  }

  if (Blocking) {
    gBS->SetTimer (
           PrivateData->TimerEvent,
           TimerCancel,
           0
           );
    Status = EFI_NOT_READY;
    while (Status == EFI_NOT_READY) {
      BpmpIpcTimerNotify (NULL, PrivateData);
      Status = gBS->CheckEvent (Token->Event);
      if (Status != EFI_NOT_READY) {
        break;
      }

      gBS->Stall (TIMEOUT_STALL_US);
    }

    gBS->RestoreTPL (EntryTpl);

    gBS->CloseEvent (Token->Event);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    return Token->TransactionStatus;
  } else {
    return EFI_SUCCESS;
  }
}

/**
  This routine moves the tx state and rings the doorbell

  @param PrivateData                    Pointer to private data.
  @param State                          NewTxState

  @return EFI_SUCCESS                    State updated
  @return others                         Error ringing doorbell

**/
EFI_STATUS
EFIAPI
MoveTxChannelState (
  IN NVIDIA_BPMP_MRQ_CHANNEL  *PrivateData,
  IN IVC_STATE                State
  )
{
  EFI_STATUS  Status;

  ArmDataMemoryBarrier ();

  PrivateData->TxChannel->State = State;
  Status                        = HspDoorbellRingDoorbell (PrivateData->HspDoorbellLocation);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to ring doorbell: %r\r\n", __FUNCTION__, Status));
  }

  return Status;
}

/**
  This routine processes the state machine for the ivc channel initialization..

  @param PrivateData                    Pointer to private data.

  @return EFI_SUCCESS                   State has moved to established
  @return EFI_TIMEOUT                   Timeout occurred
  @return others                        Error occured
**/
EFI_STATUS
EFIAPI
InitializeIvcChannel (
  IN NVIDIA_BPMP_MRQ_CHANNEL  *PrivateData
  )
{
  UINT32      RemoteState;
  EFI_STATUS  Status;
  UINTN       Timeout = PcdGet32 (PcdBpmpResponseTimeout) / TIMEOUT_STALL_US;

  Status = MoveTxChannelState (PrivateData, IvcStateSync);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  while (PrivateData->TxChannel->State != IvcStateEstablished) {
    gBS->Stall (TIMEOUT_STALL_US);
    if (Timeout != 0) {
      Timeout--;
      if (Timeout == 0) {
        return EFI_TIMEOUT;
      }
    }

    RemoteState = PrivateData->RxChannel->State;

    if ((RemoteState == IvcStateSync) ||
        ((RemoteState == IvcStateAck) &&
         (PrivateData->TxChannel->State == IvcStateSync)))
    {
      ArmDataMemoryBarrier ();

      PrivateData->TxChannel->WriteCount = 0;
      PrivateData->RxChannel->ReadCount  = 0;

      if (RemoteState == IvcStateSync) {
        Status = MoveTxChannelState (PrivateData, IvcStateAck);
        if (EFI_ERROR (Status)) {
          return Status;
        }
      } else {
        Status = MoveTxChannelState (PrivateData, IvcStateEstablished);
        if (EFI_ERROR (Status)) {
          return Status;
        }
      }
    } else if (PrivateData->TxChannel->State == IvcStateAck) {
      Status = MoveTxChannelState (PrivateData, IvcStateEstablished);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  This routine starts the BmpIpc protocol on the device.

  @param BpmpNodeInfo             A pointer to BPMP device tree node info.
  @param BpmpDevice               A pointer to Non Discoverable Device.
  @param BpmpDeviceCount          Count of BPMP nodes enabled.
  @param HspNodeInfo              A pointer to HSP device tree node info.
  @param HspDevice                A pointer to Non Discoverable Device.
  @param HspDeviceCount           Count of HSP nodes enabled.

  @retval EFI_SUCCESS           This driver is added to this device.
  @retval EFI_ALREADY_STARTED   This driver is already running on this device.
  @retval other                 Some error occurs when binding this driver to this device.

**/
EFI_STATUS
EFIAPI
BpmpIpcProtocolInit (
  IN NVIDIA_DT_NODE_INFO      *BpmpNodeInfo,
  IN NON_DISCOVERABLE_DEVICE  *BpmpDevice,
  IN UINT32                   BpmpDeviceCount,
  IN NVIDIA_DT_NODE_INFO      *HspNodeInfo,
  IN NON_DISCOVERABLE_DEVICE  *HspDevice,
  IN UINT32                   HspDeviceCount
  )
{
  EFI_STATUS                         Status;
  EFI_HANDLE                         DeviceHandle = NULL;
  NVIDIA_BPMP_IPC_PRIVATE_DATA       *PrivateData = NULL;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Desc;
  UINT32                             Index;
  INT32                              HspIndex;
  CONST VOID                         *MboxesProperty = NULL;
  INT32                              PropertySize    = 0;

  PrivateData = AllocateZeroPool (sizeof (NVIDIA_BPMP_IPC_PRIVATE_DATA));
  if (NULL == PrivateData) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  PrivateData->Signature                   = BPMP_IPC_SIGNATURE;
  PrivateData->ProtocolInstalled           = TRUE; // TODO: check usage
  PrivateData->DriverBindingHandle         = NULL;
  PrivateData->BpmpIpcProtocol.Communicate = BpmpIpcCommunicate;
  PrivateData->Controller                  = DeviceHandle; // TODO: Move to the end.
  PrivateData->DeviceCount                 = BpmpDeviceCount;

  PrivateData->Channels = AllocateZeroPool (sizeof (NVIDIA_BPMP_MRQ_CHANNEL) * BpmpDeviceCount);
  if (NULL == PrivateData->Channels) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  BpmpIpcTimerNotify,
                  PrivateData,
                  &PrivateData->TimerEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create timer event: %r\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  InitializeListHead (&PrivateData->TransactionList);

  for (Index = 0; Index < BpmpDeviceCount; Index++) {
    PrivateData->Channels[Index].BpmpPhandle = BpmpNodeInfo[Index].Phandle;
    MboxesProperty                           = fdt_getprop (BpmpNodeInfo[Index].DeviceTreeBase, BpmpNodeInfo[Index].NodeOffset, "mboxes", &PropertySize);
    if (NULL == MboxesProperty) {
      Status = EFI_UNSUPPORTED;
      goto ErrorExit;
    }

    ASSERT (PropertySize % sizeof (UINT32) == 0);
    PrivateData->Channels[Index].HspPhandle = SwapBytes32 (*(UINT32 *)MboxesProperty);
    //
    // We only support MMIO devices, so iterate over the resources to ensure
    // that they only describe things that we can handle
    //
    for (Desc = BpmpDevice[Index].Resources; Desc->Desc != ACPI_END_TAG_DESCRIPTOR;
         Desc = (VOID *)((UINT8 *)Desc + Desc->Len + 3))
    {
      if ((Desc->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR) ||
          (Desc->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM))
      {
        Status = EFI_UNSUPPORTED;
        goto ErrorExit;
      }

      // Last two resources are tx and rx, some device trees have 3 nodes and some have 2.
      if (PrivateData->Channels[Index].TxChannel == NULL) {
        PrivateData->Channels[Index].TxChannel = (IVC_CHANNEL *)(VOID *)Desc->AddrRangeMin;
      } else if (PrivateData->Channels[Index].RxChannel == NULL) {
        PrivateData->Channels[Index].RxChannel = (IVC_CHANNEL *)(VOID *)Desc->AddrRangeMin;
      } else {
        PrivateData->Channels[Index].TxChannel = PrivateData->Channels[Index].RxChannel;
        PrivateData->Channels[Index].RxChannel = (IVC_CHANNEL *)(VOID *)Desc->AddrRangeMin;
      }
    }

    // In some cases, we may have a single resource. In that case, set rx buffer at fixed offset above tx buffer.
    if ((NULL != PrivateData->Channels[Index].TxChannel) &&
        (NULL == PrivateData->Channels[Index].RxChannel))
    {
      if (BpmpDevice[Index].Resources->AddrLen < (2 * BPMP_IPC_COMM_BUFFER_SIZE)) {
        DEBUG ((EFI_D_ERROR, "%a: Bpmp buffer too small: %llu\r\n", __FUNCTION__, BpmpDevice[Index].Resources->AddrLen));
        Status = EFI_UNSUPPORTED;
        goto ErrorExit;
      }

      PrivateData->Channels[Index].RxChannel = (IVC_CHANNEL *)((UINT8 *)PrivateData->Channels[Index].TxChannel + BPMP_IPC_COMM_BUFFER_SIZE);
    }

    if ((NULL == PrivateData->Channels[Index].TxChannel) ||
        (NULL == PrivateData->Channels[Index].RxChannel))
    {
      Status = EFI_UNSUPPORTED;
      goto ErrorExit;
    }

    HspIndex = 0;
    while (HspIndex <  HspDeviceCount) {
      if (PrivateData->Channels[Index].HspPhandle == HspNodeInfo[HspIndex].Phandle) {
        break;
      }

      HspIndex++;
    }

    if (HspIndex >= HspDeviceCount) {
      DEBUG ((DEBUG_ERROR, "%a, HSP device with phandle %u not found.\n", __FUNCTION__, PrivateData->Channels[Index].HspPhandle));
      Status = EFI_UNSUPPORTED;
      goto ErrorExit;
    }

    Status = HspDoorbellInit (&HspNodeInfo[HspIndex], &HspDevice[HspIndex], &PrivateData->Channels[Index].HspDoorbellLocation);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, Failed to initialize Hsp Doorbell: %r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }

    Status = HspDoorbellEnableChannel (PrivateData->Channels[Index].HspDoorbellLocation);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, Failed to enable Hsp Doorbell channel: %r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }

    Status = InitializeIvcChannel (&PrivateData->Channels[Index]);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, Failed to initialize channel: %r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &PrivateData->Controller,
                  &gNVIDIABpmpIpcProtocolGuid,
                  &PrivateData->BpmpIpcProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, Failed to install protocol: %r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  PrivateData->ProtocolInstalled = TRUE;

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (NULL != PrivateData) {
      if (NULL != PrivateData->TimerEvent) {
        gBS->CloseEvent (PrivateData->TimerEvent);
      }

      if (NULL != PrivateData->Channels) {
        FreePool (PrivateData->Channels);
      }

      FreePool (PrivateData);
    }
  }

  return Status;
}
