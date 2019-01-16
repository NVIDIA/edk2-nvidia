/** @file
  BpmpIpc protocol implementation for BPMP IPC driver.

  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "BpmpIpcDxePrivate.h"
#include "BpmpIpcPrivate.h"
#include <Library/ArmLib.h>

#define BOTH_ALIGNED(a, b, align) ((((UINTN)(a) | (UINTN)(b)) & ((align) - 1)) == 0)

/**
  Copy Length bytes from Source to Destination, using aligned accesses only.
  Note that this implementation uses memcpy() semantics rather then memmove()
  semantics, i.e., SourceBuffer and DestinationBuffer should not overlap.

  @param  DestinationBuffer The target of the copy request.
  @param  SourceBuffer      The place to copy from.
  @param  Length            The number of bytes to copy.

  @return Destination

**/
STATIC
VOID *
AlignedCopyMem (
  OUT     VOID                      *DestinationBuffer,
  IN      CONST VOID                *SourceBuffer,
  IN      UINTN                     Length
  )
{
  UINT8             *Destination8;
  CONST UINT8       *Source8;
  UINT32            *Destination32;
  CONST UINT32      *Source32;
  UINT64            *Destination64;
  CONST UINT64      *Source64;

  if (BOTH_ALIGNED(DestinationBuffer, SourceBuffer, 8) && Length >= 8) {
    Destination64 = DestinationBuffer;
    Source64 = SourceBuffer;
    while (Length >= 8) {
      *Destination64++ = *Source64++;
      Length -= 8;
    }

    Destination8 = (UINT8 *)Destination64;
    Source8 = (CONST UINT8 *)Source64;
  } else if (BOTH_ALIGNED(DestinationBuffer, SourceBuffer, 4) && Length >= 4) {
    Destination32 = DestinationBuffer;
    Source32 = SourceBuffer;
    while (Length >= 4) {
      *Destination32++ = *Source32++;
      Length -= 4;
    }

    Destination8 = (UINT8 *)Destination32;
    Source8 = (CONST UINT8 *)Source32;
  } else {
    Destination8 = DestinationBuffer;
    Source8 = SourceBuffer;
  }
  while (Length-- != 0) {
    *Destination8++ = *Source8++;
  }
  return DestinationBuffer;
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
  IN volatile IVC_CHANNEL *Channel
  )
{
  UINT32 TransferCount = Channel->WriteCount - Channel->ReadCount;

  //If excess writes are seen then treat as free
  return (TransferCount != 1);
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
  EFI_TPL                  OldTpl;
  LIST_ENTRY               *List;
  BPMP_PENDING_TRANSACTION *Transaction;
  UINT64                   TimerTick;
  EFI_STATUS               Status;
  BOOLEAN                  ListEmpty;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  List = GetFirstNode (&PrivateData->TransactionList);
  gBS->RestoreTPL (OldTpl);

  //List is empty
  if (List == &PrivateData->TransactionList) {
    return;
  }

  Transaction = BPMP_PENDING_TRANSACTION_FROM_LINK (List);
  if (Transaction == NULL) {
    return;
  }

  //Validate channels are empty
  if (!ChannelFree (PrivateData->RxChannel) || !ChannelFree (PrivateData->TxChannel)) {
    DEBUG ((EFI_D_ERROR, "%a: Channel not idle\r\n", __FUNCTION__));
    ASSERT (FALSE);

    OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
    RemoveEntryList (List);
    ListEmpty = IsListEmpty (&PrivateData->TransactionList);
    gBS->RestoreTPL (OldTpl);

    Transaction->Token->TransactionStatus = EFI_DEVICE_ERROR;
    gBS->SignalEvent (Transaction->Token->Event);
    if (!ListEmpty) {
      ProcessTransaction (PrivateData);
    }
    return;
  }

  //Copy to Tx channel
  PrivateData->TxChannel->MessageRequest = Transaction->MessageRequest;
  PrivateData->TxChannel->Flags = IVC_FLAGS_DO_ACK;
  AlignedCopyMem ((VOID *)PrivateData->TxChannel->Data, Transaction->TxData, Transaction->TxDataSize);

  PrivateData->TxChannel->WriteCount++;
  ArmDataMemoryBarrier ();

  PrivateData->DoorbellProtocol->RingDoorbell (
                                   PrivateData->DoorbellProtocol,
                                   HspDoorbellBpmp
                                   );

  //Wait for done
  if (Transaction->Blocking) {
    TimerTick = 0;
  } else {
    TimerTick = BPMP_POLL_INTERVAL;
  }

  Status = gBS->SetTimer (
                  PrivateData->TimerEvent,
                  TimerPeriodic,
                  TimerTick
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to set timer:%r\r\n", __FUNCTION__, Status));

    OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
    RemoveEntryList (List);
    ListEmpty = IsListEmpty (&PrivateData->TransactionList);
    gBS->RestoreTPL (OldTpl);

    Transaction->Token->TransactionStatus = EFI_DEVICE_ERROR;
    gBS->SignalEvent (Transaction->Token->Event);
    if (!ListEmpty) {
      ProcessTransaction (PrivateData);
    }
    return;
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
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  NVIDIA_BPMP_IPC_PRIVATE_DATA  *PrivateData = (NVIDIA_BPMP_IPC_PRIVATE_DATA *)Context;
  EFI_TPL                       OldTpl;
  LIST_ENTRY                    *List;
  BPMP_PENDING_TRANSACTION      *Transaction;
  BOOLEAN                       ListEmpty;

  if (NULL == PrivateData) {
    return;
  }

  if (ChannelFree (PrivateData->RxChannel)) {
    return;
  }

  ArmDataMemoryBarrier ();

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  List = GetFirstNode (&PrivateData->TransactionList);
  gBS->RestoreTPL (OldTpl);

  //List is empty
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
    *Transaction->MessageError = PrivateData->RxChannel->MessageRequest;
  }
  if (PrivateData->RxChannel->MessageRequest != 0) {
    Transaction->Token->TransactionStatus = EFI_PROTOCOL_ERROR;
  } else {
    Transaction->Token->TransactionStatus = EFI_SUCCESS;
  }
  AlignedCopyMem (Transaction->RxData, (VOID *)PrivateData->RxChannel->Data, Transaction->RxDataSize);

  PrivateData->RxChannel->ReadCount++;

  ArmDataMemoryBarrier ();

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  RemoveEntryList (List);
  ListEmpty = IsListEmpty (&PrivateData->TransactionList);
  gBS->RestoreTPL (OldTpl);

  gBS->SignalEvent (Transaction->Token->Event);

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
  IN  NVIDIA_BPMP_IPC_PROTOCOL   *This,
  IN  OUT NVIDIA_BPMP_IPC_TOKEN  *Token, OPTIONAL
  IN  UINT32                     MessageRequest,
  IN  VOID                       *TxData,
  IN  UINTN                      TxDataSize,
  OUT VOID                       *RxData,
  IN  UINTN                      RxDataSize,
  IN  INT32                      *MessageError OPTIONAL
  )
{
  NVIDIA_BPMP_IPC_TOKEN        LocalToken;
  BOOLEAN                      Blocking = FALSE;
  EFI_STATUS                   Status;
  EFI_TPL                      OldTpl;
  NVIDIA_BPMP_IPC_PRIVATE_DATA *PrivateData        = NULL;
  BPMP_PENDING_TRANSACTION     *PendingTransaction = NULL;
  BOOLEAN                      NeedQueue = FALSE;

  if (NULL == This) {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData = BPMP_IPC_PRIVATE_DATA_FROM_THIS(This);

  if ((Token != NULL) && (Token->Event == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((TxData == NULL) ||
      (TxDataSize == 0) ||
      (TxDataSize > IVC_DATA_SIZE_BYTES) ||
      ((RxData != NULL) && (RxDataSize == 0)) ||
      ((RxData == NULL) && (RxDataSize != 0)) ||
      (RxDataSize > IVC_DATA_SIZE_BYTES)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Token == NULL) {
    Blocking = TRUE;
    Token = &LocalToken;
    Status = gBS->CreateEvent (
                    0,
                    TPL_CALLBACK,
                    NULL,
                    NULL,
                    &Token->Event
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  PendingTransaction = (BPMP_PENDING_TRANSACTION *) AllocatePool(sizeof (BPMP_PENDING_TRANSACTION));
  if (NULL == PendingTransaction) {
    return EFI_OUT_OF_RESOURCES;
  }

  PendingTransaction->Signature = BPMP_PENDING_TRANSACTION_SIGNATURE;
  PendingTransaction->Token = Token;
  PendingTransaction->MessageRequest = MessageRequest;
  PendingTransaction->TxData = TxData;
  PendingTransaction->TxDataSize = TxDataSize;
  PendingTransaction->RxData = RxData;
  PendingTransaction->RxDataSize = RxDataSize;
  PendingTransaction->Blocking = Blocking;
  PendingTransaction->MessageError = MessageError;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  NeedQueue = IsListEmpty (&PrivateData->TransactionList);
  InsertTailList (&PrivateData->TransactionList, &PendingTransaction->Link);
  gBS->RestoreTPL (OldTpl);

  if (NeedQueue) {
    ProcessTransaction (PrivateData);
  }

  if (Blocking) {
    Status = EFI_NOT_READY;
    while (Status == EFI_NOT_READY) {
      Status = gBS->CheckEvent (Token->Event);
      if (Status != EFI_NOT_READY) {
        break;
      }
      gBS->Stall (TIMEOUT_STALL_US);
    }
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
  IN NVIDIA_BPMP_IPC_PRIVATE_DATA  *PrivateData,
  IN IVC_STATE                     State
  )
{
  EFI_STATUS Status;

  ArmDataMemoryBarrier ();

  PrivateData->TxChannel->State = State;
  Status = PrivateData->DoorbellProtocol->RingDoorbell (
                                            PrivateData->DoorbellProtocol,
                                            HspDoorbellBpmp
                                            );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to ring doorbell: %r\r\n", __FUNCTION__, Status));
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
  IN NVIDIA_BPMP_IPC_PRIVATE_DATA  *PrivateData
  )
{
  UINT32     RemoteState;
  EFI_STATUS Status;
  UINTN      Timeout = PcdGet32 (PcdBpmpResponseTimeout) / TIMEOUT_STALL_US;

  Status = MoveTxChannelState (PrivateData, IvcStateSync);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  while (PrivateData->TxChannel->State != IvcStateEstablished) {
    gBS->Stall (TIMEOUT_STALL_US);
    Timeout--;
    if (Timeout == 0) {
      return EFI_TIMEOUT;
    }

    RemoteState = PrivateData->RxChannel->State;

    if ((RemoteState == IvcStateSync) ||
        ((RemoteState == IvcStateAck) &&
         (PrivateData->TxChannel->State == IvcStateSync))) {

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
  This routine is called to notify system that HspDoorbell protocol is availible.

  @param Event                      Event that was notified
  @param Context                    Pointer to private data.

**/
VOID
EFIAPI
HspProtocolNotify (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_STATUS                       Status;
  NVIDIA_BPMP_IPC_PRIVATE_DATA     *PrivateData = (NVIDIA_BPMP_IPC_PRIVATE_DATA *)Context;
  UINTN                            NumberOfHandles = 0;
  EFI_HANDLE                       *HandleBuffer = NULL;

  if (NULL == PrivateData) {
    return;
  }

  Status = gBS->LocateHandleBuffer (
                  ByRegisterNotify,
                  &gNVIDIAHspDoorbellProtocolGuid,
                  PrivateData->ProtocolNotifyToken,
                  &NumberOfHandles,
                  &HandleBuffer
                  );

  if (EFI_ERROR (Status) || NumberOfHandles == 0) {
    DEBUG ((EFI_D_ERROR, "%a: Locate Handle:%r\r\n", __FUNCTION__, Status));
    return;
  }

  PrivateData->DoorbellHandle = HandleBuffer[0];
  FreePool (HandleBuffer);

  Status = gBS->OpenProtocol (
                  PrivateData->DoorbellHandle,
                  &gNVIDIAHspDoorbellProtocolGuid,
                  (VOID **) &PrivateData->DoorbellProtocol,
                  PrivateData->DriverBindingHandle,
                  PrivateData->DoorbellHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Open Protocol %r\r\n", __FUNCTION__, Status));
    PrivateData->DoorbellHandle = NULL;
    return;
  }

  if (NULL != PrivateData->RegisterNotifyEvent) {
    gBS->CloseEvent (PrivateData->RegisterNotifyEvent);
    PrivateData->RegisterNotifyEvent = NULL;
  }

  Status = PrivateData->DoorbellProtocol->EnableChannel (
                                            PrivateData->DoorbellProtocol,
                                            HspDoorbellBpmp
                                            );

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to enable doorbell channel: %r", __FUNCTION__, Status));
    return;
  }

  Status = InitializeIvcChannel (PrivateData);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to initialize channel: %r", __FUNCTION__, Status));
    return;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &PrivateData->Controller,
                  &gNVIDIABpmpIpcProtocolGuid,
                  &PrivateData->BpmpIpcProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to install protocol: %r", __FUNCTION__, Status));
    return;
  }

  PrivateData->ProtocolInstalled = TRUE;
}

/**
  This routine is called right after the .Supported() called and
  Starts the HspDoorbell protocol on the device.

  @param This                     Protocol instance pointer.
  @param Controller               Handle of device to bind driver to.
  @param NonDiscoverableProtocol  A pointer to the NonDiscoverableProtocol.

  @retval EFI_SUCCESS             This driver is added to this device.
  @retval EFI_ALREADY_STARTED     This driver is already running on this device.
  @retval other                   Some error occurs when binding this driver to this device.

**/
EFI_STATUS
EFIAPI
BpmpIpcProtocolStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN NON_DISCOVERABLE_DEVICE        *NonDiscoverableProtocol
  )
{
  EFI_STATUS                          Status;
  NVIDIA_BPMP_IPC_PRIVATE_DATA        *PrivateData = NULL;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR   *Desc;
  UINTN                               ResourceCount = 0;

  PrivateData = AllocateZeroPool (sizeof (NVIDIA_BPMP_IPC_PRIVATE_DATA));
  if (NULL == PrivateData) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  PrivateData->Signature = BPMP_IPC_SIGNATURE;
  PrivateData->ProtocolInstalled = FALSE;
  PrivateData->DriverBindingHandle = This->DriverBindingHandle;
  PrivateData->DoorbellProtocol = NULL;
  PrivateData->DoorbellHandle = NULL;
  PrivateData->TxChannel = NULL;
  PrivateData->RxChannel = NULL;
  PrivateData->BpmpIpcProtocol.Communicate = BpmpIpcCommunicate;
  PrivateData->Controller = Controller;

  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  BpmpIpcTimerNotify,
                  PrivateData,
                  &PrivateData->TimerEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to create timer event: %r\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  InitializeListHead (&PrivateData->TransactionList);

  //
  // We only support MMIO devices, so iterate over the resources to ensure
  // that they only describe things that we can handle
  //
  for (Desc = NonDiscoverableProtocol->Resources; Desc->Desc != ACPI_END_TAG_DESCRIPTOR;
       Desc = (VOID *)((UINT8 *)Desc + Desc->Len + 3)) {
    if (Desc->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR ||
        Desc->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM) {
      Status = EFI_UNSUPPORTED;
      goto ErrorExit;
    }
    //Last two resources are tx and rx, some device trees have 3 nodes and some have 2.
    if (PrivateData->TxChannel == NULL) {
      PrivateData->TxChannel = (IVC_CHANNEL *)(VOID *)Desc->AddrRangeMin;
    } else if (PrivateData->RxChannel == NULL) {
      PrivateData->RxChannel = (IVC_CHANNEL *)(VOID *)Desc->AddrRangeMin;
    } else {
      PrivateData->TxChannel = PrivateData->RxChannel;
      PrivateData->RxChannel = (IVC_CHANNEL *)(VOID *)Desc->AddrRangeMin;
    }
    ResourceCount++;
  }

  if ((NULL == PrivateData->TxChannel) ||
      (NULL == PrivateData->RxChannel)) {
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  PrivateData->RegisterNotifyEvent = EfiCreateProtocolNotifyEvent (
                 &gNVIDIAHspDoorbellProtocolGuid,
                 TPL_CALLBACK,
                 HspProtocolNotify,
                 (VOID *) PrivateData,
                 &PrivateData->ProtocolNotifyToken
                 );
  if (NULL == PrivateData->RegisterNotifyEvent) {
    DEBUG ((EFI_D_ERROR, "%a: Enable Channel\r\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto ErrorExit;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gEfiCallerIdGuid,
                  PrivateData,
                  NULL);
ErrorExit:
  if (EFI_ERROR (Status)) {
    if (NULL != PrivateData) {
      if (NULL != PrivateData->RegisterNotifyEvent) {
        gBS->CloseEvent (PrivateData->RegisterNotifyEvent);
        PrivateData->RegisterNotifyEvent = NULL;
      }
      if (NULL != PrivateData->DoorbellProtocol) {
        gBS->CloseProtocol (
                        PrivateData->DoorbellHandle,
                        &gNVIDIAHspDoorbellProtocolGuid,
                        This->DriverBindingHandle,
                        PrivateData->DoorbellHandle
                        );
      }

      FreePool (PrivateData);
    }
  }
  return Status;
}

/**
  This function disconnects the HspDoorbell protocol from the specified controller.

  @param This                     Protocol instance pointer.
  @param Controller               Handle of device to disconnect driver from.
  @param NonDiscoverableProtocol  A pointer to the NonDiscoverableProtocol.

  @retval EFI_SUCCESS   This driver is removed from this device.
  @retval other         Some error occurs when removing this driver from this device.

**/
EFI_STATUS
EFIAPI
BpmpIpcProtocolStop (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN NON_DISCOVERABLE_DEVICE        *NonDiscoverableProtocol
  )
{
  EFI_STATUS                       Status;
  NVIDIA_BPMP_IPC_PRIVATE_DATA     *PrivateData = NULL;

  //
  // Open the produced protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiCallerIdGuid,
                  (VOID **) &PrivateData,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if (PrivateData->ProtocolInstalled) {
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    Controller,
                    &gNVIDIABpmpIpcProtocolGuid,
                    &PrivateData->BpmpIpcProtocol,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
  }

  if (NULL != PrivateData->RegisterNotifyEvent) {
    gBS->CloseEvent (PrivateData->RegisterNotifyEvent);
    PrivateData->RegisterNotifyEvent = NULL;
  }

  if (NULL != PrivateData->DoorbellProtocol) {
    gBS->CloseProtocol (
                    PrivateData->DoorbellHandle,
                    &gNVIDIAHspDoorbellProtocolGuid,
                    This->DriverBindingHandle,
                    PrivateData->DoorbellHandle
                    );
  }

  FreePool (PrivateData);

  return EFI_SUCCESS;
}
