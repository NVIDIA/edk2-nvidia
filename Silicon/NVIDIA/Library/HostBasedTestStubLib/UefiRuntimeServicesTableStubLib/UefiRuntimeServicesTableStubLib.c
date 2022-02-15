/** @file

  UEFI Runtime Services Table Lib stubs for host based tests

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <HostBasedTestStubLib/UefiRuntimeServicesTableStubLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

typedef struct {
  LIST_ENTRY    List;
  CHAR16        *Name;
  EFI_GUID      Guid;
  UINT32        Attributes;
  UINTN         Size;
  VOID          *Data;
  EFI_STATUS    ForcedGetStatus;
  EFI_STATUS    ForcedSetStatus;
} UEFI_VARIABLE;

LIST_ENTRY              mUefiVariableListHead   = {0};
EFI_RUNTIME_SERVICES    *gRT                    = NULL;

/**
  Find a variable in the linked list

  @param[In]  Name                  Variable name
  @param[In]  Guid                  Variable guid

  @retval non-NULL              Pointer to requested variable control structure
  @retval NULL                  Variable not found

**/
STATIC
UEFI_VARIABLE *
EFIAPI
UefiFindVariable (
  IN  CHAR16    *Name,
  IN  EFI_GUID  *Guid
  )
{
  UEFI_VARIABLE *ReturnVariable;
  LIST_ENTRY    *Entry;

  ReturnVariable = NULL;
  BASE_LIST_FOR_EACH (Entry, &mUefiVariableListHead) {
    UEFI_VARIABLE *Var = (UEFI_VARIABLE *) Entry;
    if ((StrCmp (Var->Name, Name) == 0) &&
        (CompareMem (&Var->Guid, Guid, sizeof (*Guid)) == 0)) {
      ReturnVariable = Var;
      break;
    }
  }

  return ReturnVariable;
}

// EFI_GET_VARIABLE stub
EFI_STATUS
EFIAPI
UefiGetVariable (
  IN  CHAR16    *Name,
  IN  EFI_GUID  *Guid,
  IN  UINT32    *Attributes,
  IN  UINTN     *Size,
  IN  VOID      *Data
  )
{
  UEFI_VARIABLE *Var;

  Var = UefiFindVariable (Name, Guid);
  if ((Var == NULL) || ((Var->Data == NULL) && (!EFI_ERROR(Var->ForcedGetStatus)))) {
    return EFI_NOT_FOUND;
  }

  if (EFI_ERROR (Var->ForcedGetStatus)) {
    EFI_STATUS    ForcedStatus;

    ForcedStatus = Var->ForcedGetStatus;

    if (Var->Data != NULL) {
      Var->ForcedGetStatus = EFI_SUCCESS;
    } else {
      RemoveEntryList (&Var->List);
      FreePool (Var->Name);
      FreePool (Var);
    }

    return ForcedStatus;
  }

  if (Attributes != NULL) {
    *Attributes = Var->Attributes;
  }

  if (Var->Size > *Size) {
    *Size = Var->Size;
    return EFI_BAD_BUFFER_SIZE;
  }

  CopyMem (Data, Var->Data, Var->Size);

  return EFI_SUCCESS;
}

VOID
MockUefiGetVariable (
  IN  CHAR16        *Name,
  IN  EFI_GUID      *Guid,
  IN  EFI_STATUS    ReturnStatus
  )
{
  UEFI_VARIABLE *Var;

  Var = UefiFindVariable (Name, Guid);
  if (Var == NULL) {
    Var = (UEFI_VARIABLE *) AllocateZeroPool (sizeof (UEFI_VARIABLE));
    Var->Name = (CHAR16 *) AllocateZeroPool (StrSize (Name));
    CopyMem (Var->Name, Name, StrSize (Name));
    CopyMem (&Var->Guid, Guid, sizeof (*Guid));
    InsertTailList (&mUefiVariableListHead, &Var->List);
  }
  Var->ForcedGetStatus = ReturnStatus;
}

// EFI_SET_VARIABLE stub
EFI_STATUS
EFIAPI
UefiSetVariable (
  IN  CHAR16                       *Name,
  IN  EFI_GUID                     *Guid,
  IN  UINT32                       Attributes,
  IN  UINTN                        Size,
  IN  VOID                         *Data
  )
{
  UEFI_VARIABLE *Var;

  Var = UefiFindVariable (Name, Guid);
  if (Var == NULL) {
    if (Size == 0) {
      return EFI_SUCCESS;
    }
    Var = (UEFI_VARIABLE *) AllocateZeroPool (sizeof (UEFI_VARIABLE));
    Var->Name = (CHAR16 *) AllocateZeroPool (StrSize (Name));
    CopyMem (Var->Name, Name, StrSize (Name));
    CopyMem (&Var->Guid, Guid, sizeof (*Guid));
    InsertTailList (&mUefiVariableListHead, &Var->List);
  } else if (Size > Var->Size) {
    FreePool (Var->Data);
    Var->Data = NULL;
  }

  if (EFI_ERROR (Var->ForcedSetStatus)) {
    EFI_STATUS    ForcedStatus;

    ForcedStatus = Var->ForcedSetStatus;

    if (Var->Data != NULL) {
      Var->ForcedSetStatus = EFI_SUCCESS;
    } else {
      RemoveEntryList (&Var->List);
      FreePool (Var->Name);
      FreePool (Var);
    }

    return ForcedStatus;
  }

  if (Size == 0) {
    RemoveEntryList (&Var->List);
    FreePool (Var->Name);
    if (Var->Data != NULL) {
      FreePool (Var->Data);
    }
    FreePool (Var);

    return EFI_SUCCESS;
  }

  if (Var->Data == NULL) {
    Var->Data = AllocateZeroPool (Size);
  }

  Var->Guid = *Guid;
  Var->Size = Size;
  Var->Attributes = Attributes;
  CopyMem (Var->Data, Data, Size);

  return EFI_SUCCESS;
}

VOID
MockUefiSetVariable (
  IN  CHAR16        *Name,
  IN  EFI_GUID      *Guid,
  IN  EFI_STATUS    ReturnStatus
  )
{
  UEFI_VARIABLE *Var;

  Var = UefiFindVariable (Name, Guid);
  if (Var == NULL) {
    Var = (UEFI_VARIABLE *) AllocateZeroPool (sizeof (UEFI_VARIABLE));
    Var->Name = (CHAR16 *) AllocateZeroPool (StrSize (Name));
    CopyMem (Var->Name, Name, StrSize (Name));
    CopyMem (&Var->Guid, Guid, sizeof (*Guid));
    InsertTailList (&mUefiVariableListHead, &Var->List);
  }

  Var->ForcedSetStatus = ReturnStatus;
}

/**
  Initialize Uefi variable stub support

  @retval None

**/
STATIC
VOID
UefiVariableInit (
  VOID
  )
{
  InitializeListHead (&mUefiVariableListHead);

}

/**
  De-initialize Uefi variable stub support

  @retval None

**/
STATIC
VOID
UefiVariableDeinit (
  VOID
  )
{
  UEFI_VARIABLE *Var;
  LIST_ENTRY    *Entry = NULL;
  LIST_ENTRY    *Next = NULL;

  BASE_LIST_FOR_EACH_SAFE (Entry, Next, &mUefiVariableListHead) {
    EFI_STATUS Status;
    Var = (UEFI_VARIABLE *) Entry;
    Status = UefiSetVariable (Var->Name, &Var->Guid, Var->Attributes, 0, NULL);
    ASSERT (Status == EFI_SUCCESS);
  }

  ASSERT (IsListEmpty (&mUefiVariableListHead));
}

VOID
UefiRuntimeServicesTableInit (
  IN  BOOLEAN   PreserveVariables
  )
{
  ASSERT (gRT == NULL);
  gRT = AllocateZeroPool (sizeof (*gRT));

  if (!PreserveVariables) {
    UefiVariableInit ();
  }

  gRT->GetVariable = UefiGetVariable;
  gRT->SetVariable = UefiSetVariable;
}

VOID
UefiRuntimeServicesTableDeinit (
  IN  BOOLEAN   PreserveVariables
  )
{
  ASSERT (gRT != NULL);

  if (!PreserveVariables) {
    UefiVariableDeinit ();
  }

  FreePool (gRT);
  gRT = NULL;
}
