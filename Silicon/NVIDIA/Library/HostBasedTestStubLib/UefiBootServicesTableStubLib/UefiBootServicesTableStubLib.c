/** @file

  UEFI Boot Services Table Lib stubs for host based tests

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <HostBasedTestStubLib/UefiBootServicesTableStubLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

typedef struct {
  LIST_ENTRY            List;
  EFI_EVENT_NOTIFY      NotifyFunction;
  CONST VOID            *NotifyContext;
  CONST EFI_GUID        *EventGroup;
} UEFI_EVENT;

EFI_HANDLE         gImageHandle         = NULL;
EFI_SYSTEM_TABLE   *gST                 = NULL;
EFI_BOOT_SERVICES  *gBS                 = NULL;
LIST_ENTRY          mUefiEventListHead  = {0};

// EFI_CREATE_EVENT_EX stub
EFI_STATUS
EFIAPI
UefiCreateEventEx (
  IN       UINT32                 Type,
  IN       EFI_TPL                NotifyTpl,
  IN       EFI_EVENT_NOTIFY       NotifyFunction OPTIONAL,
  IN CONST VOID                   *NotifyContext OPTIONAL,
  IN CONST EFI_GUID               *EventGroup    OPTIONAL,
  OUT      EFI_EVENT              *Event
  )
{
  EFI_STATUS    ReturnStatus;
  EFI_EVENT     *EventSavePtr;

  check_expected (EventGroup);

  EventSavePtr = (EFI_EVENT *) mock ();
  ReturnStatus = (EFI_STATUS) mock ();

  if (!EFI_ERROR (ReturnStatus)) {
    UEFI_EVENT *MockEvent;

    MockEvent = AllocateZeroPool (sizeof (*MockEvent));

    InsertTailList (&mUefiEventListHead, &MockEvent->List);
    MockEvent->NotifyFunction = NotifyFunction;
    MockEvent->NotifyContext = NotifyContext;
    MockEvent->EventGroup = EventGroup;

    *Event = (EFI_EVENT *) MockEvent;
    *EventSavePtr = (EFI_EVENT *) MockEvent;
  }

  return ReturnStatus;
}

VOID
MockUefiCreateEventEx (
  IN  EFI_GUID      *EventGroup,
  IN  EFI_EVENT     *EventSavePtr,
  IN  EFI_STATUS    ReturnStatus
  )
{
  expect_value (UefiCreateEventEx, EventGroup, EventGroup);

  will_return (UefiCreateEventEx, EventSavePtr);
  will_return (UefiCreateEventEx, ReturnStatus);
}

// EFI_CLOSE_EVENT stub
EFI_STATUS
EFIAPI
UefiCloseEvent (
  IN      EFI_EVENT     Event
  )
{
  UEFI_EVENT *MockEvent = (UEFI_EVENT *)Event;
  LIST_ENTRY *Head;

  ASSERT (MockEvent != NULL);
  ASSERT (!IsListEmpty (&mUefiEventListHead));

  Head = RemoveEntryList (&MockEvent->List);
  ASSERT (Head == &mUefiEventListHead);
  ASSERT (IsListEmpty (&mUefiEventListHead));
  DEBUG ((DEBUG_ERROR, "%a: freeing MockEvent=0x%p\n", __FUNCTION__, MockEvent));

  FreePool (MockEvent);

  return EFI_SUCCESS;
}

// EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES stub
EFI_STATUS
EFIAPI
UefiInstallMultipleProtocolInterfaces (
  IN OUT EFI_HANDLE           *Handle,
  ...
  )
{
  EFI_GUID      *ProtocolGuid;
  VOID          *Interface;
  VOID          **InterfaceSavePtr;
  VA_LIST       Args;
  EFI_STATUS    ReturnStatus;

  ReturnStatus = (EFI_STATUS) mock ();

  VA_START (Args, Handle);

  while (TRUE) {
    ProtocolGuid = (EFI_GUID *) VA_ARG (Args, void *);
    if (ProtocolGuid == NULL) {
      break;
    }
    check_expected (ProtocolGuid);

    Interface = VA_ARG (Args, void *);
    InterfaceSavePtr = (VOID **) mock ();
    if (!EFI_ERROR (ReturnStatus)) {
      *InterfaceSavePtr = Interface;
    }
  }

  VA_END (Args);

  return ReturnStatus;
}

VOID
EFIAPI
MockInstallMultipleProtocolInterfaces (
  IN  EFI_STATUS    ReturnStatus,
  ...
  )
{
  EFI_GUID      *ProtocolGuid;
  VOID          **InterfaceSavePtr;
  VA_LIST       Args;

  will_return (UefiInstallMultipleProtocolInterfaces, ReturnStatus);

  VA_START (Args, ReturnStatus);

  while (TRUE) {
    ProtocolGuid = (EFI_GUID *) VA_ARG (Args, void *);
    if (ProtocolGuid == NULL) {
      break;
    }
    InterfaceSavePtr = (VOID **) VA_ARG (Args, void *);

    expect_value (UefiInstallMultipleProtocolInterfaces, ProtocolGuid, ProtocolGuid);
    will_return (UefiInstallMultipleProtocolInterfaces, InterfaceSavePtr);
  }

  VA_END (Args);
}

// EFI_LOCATE_PROTOCOL stub
EFI_STATUS
EFIAPI
UefiLocateProtocol(
  IN  EFI_GUID  *Protocol,
  IN  VOID      *Registration, OPTIONAL
  OUT VOID      **Interface
  )
{
  EFI_STATUS    Status;
  VOID          *InterfacePtr;

  check_expected (Protocol);

  Status = (EFI_STATUS) mock ();
  InterfacePtr = (VOID *) mock ();
  if (!EFI_ERROR (Status)) {
    *Interface = InterfacePtr;
  }

  return Status;
}

VOID
MockUefiLocateProtocol (
  IN  EFI_GUID      *ProtocolGuid,
  IN  VOID          *ReturnProtocolInterface,
  IN  EFI_STATUS    ReturnStatus
  )
{
  expect_value (UefiLocateProtocol, Protocol, ProtocolGuid);

  will_return (UefiLocateProtocol, ReturnStatus);
  will_return (UefiLocateProtocol, ReturnProtocolInterface);
}

// EFI_SIGNAL_EVENT stub
EFI_STATUS
EFIAPI
UefiSignalEvent (
  IN  EFI_EVENT                Event
  )
{
  UEFI_EVENT *UefiEvent;

  UefiEvent = (UEFI_EVENT *) Event;

  UefiEvent->NotifyFunction (Event, (VOID *)UefiEvent->NotifyContext);

  return EFI_SUCCESS;
}

VOID
UefiBootServicesTableInit (
  VOID
  )
{
  ASSERT (gBS == NULL);
  gBS = AllocateZeroPool (sizeof (*gBS));

  InitializeListHead (&mUefiEventListHead);

  gBS->CloseEvent                       = UefiCloseEvent;
  gBS->CreateEventEx                    = UefiCreateEventEx;
  gBS->InstallMultipleProtocolInterfaces= UefiInstallMultipleProtocolInterfaces;
  gBS->LocateProtocol                   = UefiLocateProtocol;
  gBS->SignalEvent                      = UefiSignalEvent;
}

VOID
UefiBootServicesTableDeinit (
  VOID
  )
{
  LIST_ENTRY    *Entry = NULL;
  LIST_ENTRY    *Next = NULL;

  ASSERT (gBS != NULL);

  BASE_LIST_FOR_EACH_SAFE (Entry, Next, &mUefiEventListHead) {
    RemoveEntryList (Entry);
    FreePool (Entry);
  }
  ASSERT (IsListEmpty (&mUefiEventListHead));

  FreePool (gBS);
  gBS           = NULL;
  gImageHandle  = NULL;
  gST           = NULL;
}
