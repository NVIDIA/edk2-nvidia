/** @file

  Pcd Lib stubs for host based tests

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <HostBasedTestStubLib/PcdStubLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

typedef struct {
  LIST_ENTRY    List;
  UINTN         TokenNumber;
  union {
    BOOLEAN    Boolean;
    UINT64     UInt64;
  } Value;
} UEFI_PCD;

LIST_ENTRY  mUefiPcdListHead = { 0 };

/**
  Find a PCD in the linked list

  @param[In]  TokenNumber        PCD number

  @retval non-NULL              Pointer to requested PCD control structure
  @retval NULL                  Variable not found

**/
STATIC
UEFI_PCD *
EFIAPI
UefiFindPcd (
  IN  UINTN  TokenNumber
  )
{
  UEFI_PCD    *ReturnPcd;
  LIST_ENTRY  *Entry;

  ReturnPcd = NULL;
  BASE_LIST_FOR_EACH (Entry, &mUefiPcdListHead) {
    UEFI_PCD  *Pcd = (UEFI_PCD *)Entry;

    if (Pcd->TokenNumber == TokenNumber) {
      ReturnPcd = Pcd;
      break;
    }
  }

  return ReturnPcd;
}

/**
  Return a PCD in the linked list, adding it if necessary.

  @param[In]  TokenNumber        PCD number

  @retval Pointer to requested PCD control structure

**/
STATIC
UEFI_PCD *
EFIAPI
UefiFindOrAddPcd (
  IN  UINTN  TokenNumber
  )
{
  UEFI_PCD  *ReturnPcd;

  ReturnPcd = UefiFindPcd (TokenNumber);

  if (ReturnPcd == NULL) {
    ReturnPcd              = (UEFI_PCD *)AllocateZeroPool (sizeof (UEFI_PCD));
    ReturnPcd->TokenNumber = TokenNumber;
    InsertTailList (&mUefiPcdListHead, &ReturnPcd->List);
  }

  return ReturnPcd;
}

/**
  Initialize Uefi PCD stub support.

  This should be called once before running tests.

  @retval None

**/
VOID
UefiPcdInit (
  VOID
  )
{
  InitializeListHead (&mUefiPcdListHead);
}

/**
  Clear Uefi PCD list

  This should be called at the start of a test, before adding PCD values.

  @retval None

**/
VOID
UefiPcdClear (
  VOID
  )
{
  LIST_ENTRY  *Entry = NULL;
  LIST_ENTRY  *Next  = NULL;

  // Clear any entries we might have left over from a previous test.
  BASE_LIST_FOR_EACH_SAFE (Entry, Next, &mUefiPcdListHead) {
    RemoveEntryList (Entry);
    FreePool (Entry);
  }
}

/**
  Stubbed implementation of LibPcdGetBool().

  Returns values set by MockLibPcdGetBool ().
**/
BOOLEAN
EFIAPI
LibPcdGetBool (
  IN UINTN  TokenNumber
  )
{
  UEFI_PCD  *Pcd;

  Pcd = UefiFindPcd (TokenNumber);
  if (Pcd == NULL) {
    DEBUG ((DEBUG_ERROR, "Missing mocked value for PCD %lx\n", TokenNumber));
    ASSERT (Pcd != NULL);
    return FALSE;
  }

  return Pcd->Value.Boolean;
}

/**
  Set the return of LibPcdGetBool () for a PCD TokenNumber.

  @param[In]  TokenNumber   PCD request
  @param[In]  ReturnValue   Value to return when requested

  @retval None
 **/
VOID
MockLibPcdGetBool (
  IN UINTN    TokenNumber,
  IN BOOLEAN  ReturnValue
  )
{
  UEFI_PCD  *Pcd;

  Pcd                = UefiFindOrAddPcd (TokenNumber);
  Pcd->Value.Boolean = ReturnValue;
}

/**
  Mocked version of LibPcdGet64().

  Returns values set by MockLibPcdGet64 ().
**/
UINT64
EFIAPI
LibPcdGet64 (
  IN UINTN  TokenNumber
  )
{
  UEFI_PCD  *Pcd;

  Pcd = UefiFindPcd (TokenNumber);
  if (Pcd == NULL) {
    DEBUG ((DEBUG_ERROR, "Missing mocked value for PCD %lx\n", TokenNumber));
    ASSERT (Pcd != NULL);
    return FALSE;
  }

  return Pcd->Value.UInt64;
}

/**
  Set the return of LibPcdGet64 () for a PCD TokenNumber.

  @param[In]  TokenNumber   PCD request
  @param[In]  ReturnValue   Value to return when requested

  @retval None
 **/
VOID
MockLibPcdGet64 (
  IN UINTN   TokenNumber,
  IN UINT64  ReturnValue
  )
{
  UEFI_PCD  *Pcd;

  Pcd               = UefiFindOrAddPcd (TokenNumber);
  Pcd->Value.UInt64 = ReturnValue;
}
