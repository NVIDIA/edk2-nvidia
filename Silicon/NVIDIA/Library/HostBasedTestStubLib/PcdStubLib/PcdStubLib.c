/** @file

  Pcd Lib stubs for host based tests

  SPDX-FileCopyrightText: Copyright (c) 2023-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <HostBasedTestStubLib/PcdStubLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

// ---------------------------------------------------------------------------
// Per-token PCD store
// ---------------------------------------------------------------------------

typedef struct {
  LIST_ENTRY    List;
  UINTN         TokenNumber;
  union {
    BOOLEAN    Boolean;
    UINT64     Numeric;   // covers UINT8, UINT16, UINT32, UINT64
  } Value;
} UEFI_PCD;

// Self-referential initializer: valid empty list without requiring UefiPcdInit().
static LIST_ENTRY  mUefiPcdListHead = {
  (LIST_ENTRY *)&mUefiPcdListHead,
  (LIST_ENTRY *)&mUefiPcdListHead,
};

VOID
UefiPcdInit (
  VOID
  )
{
  // Kept for API compatibility; list is already valid at program start.
  InitializeListHead (&mUefiPcdListHead);
}

VOID
UefiPcdClear (
  VOID
  )
{
  LIST_ENTRY  *Entry;
  LIST_ENTRY  *Next;

  BASE_LIST_FOR_EACH_SAFE (Entry, Next, &mUefiPcdListHead) {
    RemoveEntryList (Entry);
    FreePool (Entry);
  }
}

STATIC
UEFI_PCD *
UefiFindPcd (
  IN UINTN  TokenNumber
  )
{
  LIST_ENTRY  *Entry;

  BASE_LIST_FOR_EACH (Entry, &mUefiPcdListHead) {
    UEFI_PCD  *Pcd = (UEFI_PCD *)Entry;

    if (Pcd->TokenNumber == TokenNumber) {
      return Pcd;
    }
  }

  return NULL;
}

STATIC
UEFI_PCD *
UefiFindOrAddPcd (
  IN UINTN  TokenNumber
  )
{
  UEFI_PCD  *Pcd;

  Pcd = UefiFindPcd (TokenNumber);
  if (Pcd == NULL) {
    Pcd              = AllocateZeroPool (sizeof (UEFI_PCD));
    Pcd->TokenNumber = TokenNumber;
    InsertTailList (&mUefiPcdListHead, &Pcd->List);
  }

  return Pcd;
}

// ---------------------------------------------------------------------------
// Mock helpers
// ---------------------------------------------------------------------------

VOID
MockLibPcdGet8 (
  IN UINTN  TokenNumber,
  IN UINT8  ReturnValue
  )
{
  UefiFindOrAddPcd (TokenNumber)->Value.Numeric = ReturnValue;
}

VOID
MockLibPcdGet16 (
  IN UINTN   TokenNumber,
  IN UINT16  ReturnValue
  )
{
  UefiFindOrAddPcd (TokenNumber)->Value.Numeric = ReturnValue;
}

VOID
MockLibPcdGet32 (
  IN UINTN   TokenNumber,
  IN UINT32  ReturnValue
  )
{
  UefiFindOrAddPcd (TokenNumber)->Value.Numeric = ReturnValue;
}

VOID
MockLibPcdGet64 (
  IN UINTN   TokenNumber,
  IN UINT64  ReturnValue
  )
{
  UefiFindOrAddPcd (TokenNumber)->Value.Numeric = ReturnValue;
}

VOID
MockLibPcdGetBool (
  IN UINTN    TokenNumber,
  IN BOOLEAN  ReturnValue
  )
{
  UefiFindOrAddPcd (TokenNumber)->Value.Boolean = ReturnValue;
}

// ---------------------------------------------------------------------------
// PcdLib interface: Get
// ---------------------------------------------------------------------------

STATIC
UINT64
GetNumeric (
  IN UINTN  TokenNumber
  )
{
  UEFI_PCD  *Pcd;

  Pcd = UefiFindPcd (TokenNumber);
  if (Pcd == NULL) {
    DEBUG ((DEBUG_VERBOSE, "PcdStubLib: unmocked numeric token 0x%lx, returning 0\n", (UINT64)TokenNumber));
    return 0;
  }

  return Pcd->Value.Numeric;
}

UINT8
EFIAPI
LibPcdGet8 (
  IN UINTN  TokenNumber
  )
{
  return (UINT8)GetNumeric (TokenNumber);
}

UINT16
EFIAPI
LibPcdGet16 (
  IN UINTN  TokenNumber
  )
{
  return (UINT16)GetNumeric (TokenNumber);
}

UINT32
EFIAPI
LibPcdGet32 (
  IN UINTN  TokenNumber
  )
{
  return (UINT32)GetNumeric (TokenNumber);
}

UINT64
EFIAPI
LibPcdGet64 (
  IN UINTN  TokenNumber
  )
{
  return GetNumeric (TokenNumber);
}

BOOLEAN
EFIAPI
LibPcdGetBool (
  IN UINTN  TokenNumber
  )
{
  UEFI_PCD  *Pcd;

  Pcd = UefiFindPcd (TokenNumber);
  if (Pcd == NULL) {
    DEBUG ((DEBUG_VERBOSE, "PcdStubLib: unmocked bool token 0x%lx, returning FALSE\n", (UINT64)TokenNumber));
    return FALSE;
  }

  return Pcd->Value.Boolean;
}

// ---------------------------------------------------------------------------
// PcdLib interface: Set (S-style)
// ---------------------------------------------------------------------------

STATIC
RETURN_STATUS
SetNumeric (
  IN UINTN   TokenNumber,
  IN UINT64  Value
  )
{
  UefiFindOrAddPcd (TokenNumber)->Value.Numeric = Value;
  return RETURN_SUCCESS;
}

RETURN_STATUS
EFIAPI
LibPcdSet8S (
  IN UINTN  TokenNumber,
  IN UINT8  Value
  )
{
  return SetNumeric (TokenNumber, Value);
}

RETURN_STATUS
EFIAPI
LibPcdSet16S (
  IN UINTN   TokenNumber,
  IN UINT16  Value
  )
{
  return SetNumeric (TokenNumber, Value);
}

RETURN_STATUS
EFIAPI
LibPcdSet32S (
  IN UINTN   TokenNumber,
  IN UINT32  Value
  )
{
  return SetNumeric (TokenNumber, Value);
}

RETURN_STATUS
EFIAPI
LibPcdSet64S (
  IN UINTN   TokenNumber,
  IN UINT64  Value
  )
{
  return SetNumeric (TokenNumber, Value);
}

RETURN_STATUS
EFIAPI
LibPcdSetBoolS (
  IN UINTN    TokenNumber,
  IN BOOLEAN  Value
  )
{
  UefiFindOrAddPcd (TokenNumber)->Value.Boolean = Value;
  return RETURN_SUCCESS;
}

// ---------------------------------------------------------------------------
// PcdLib interface: LibPatch* (binary-patch PCDs — real implementations)
// ---------------------------------------------------------------------------

VOID *
EFIAPI
LibPatchPcdSetPtr (
  OUT       VOID   *PatchVariable,
  IN        UINTN  MaximumDatumSize,
  IN OUT    UINTN  *SizeOfBuffer,
  IN CONST  VOID   *Buffer
  )
{
  ASSERT (PatchVariable != NULL);
  ASSERT (SizeOfBuffer  != NULL);
  if (*SizeOfBuffer > 0) {
    ASSERT (Buffer != NULL);
  }

  if ((*SizeOfBuffer > MaximumDatumSize) || (*SizeOfBuffer == MAX_ADDRESS)) {
    *SizeOfBuffer = MaximumDatumSize;
    return NULL;
  }

  CopyMem (PatchVariable, Buffer, *SizeOfBuffer);
  return (VOID *)Buffer;
}

RETURN_STATUS
EFIAPI
LibPatchPcdSetPtrS (
  OUT      VOID   *PatchVariable,
  IN       UINTN  MaximumDatumSize,
  IN OUT   UINTN  *SizeOfBuffer,
  IN CONST VOID   *Buffer
  )
{
  ASSERT (PatchVariable != NULL);
  ASSERT (SizeOfBuffer  != NULL);
  if (*SizeOfBuffer > 0) {
    ASSERT (Buffer != NULL);
  }

  if ((*SizeOfBuffer > MaximumDatumSize) || (*SizeOfBuffer == MAX_ADDRESS)) {
    *SizeOfBuffer = MaximumDatumSize;
    return RETURN_INVALID_PARAMETER;
  }

  CopyMem (PatchVariable, Buffer, *SizeOfBuffer);
  return RETURN_SUCCESS;
}

VOID *
EFIAPI
LibPatchPcdSetPtrAndSize (
  OUT       VOID   *PatchVariable,
  OUT       UINTN  *SizeOfPatchVariable,
  IN        UINTN  MaximumDatumSize,
  IN OUT    UINTN  *SizeOfBuffer,
  IN CONST  VOID   *Buffer
  )
{
  ASSERT (PatchVariable      != NULL);
  ASSERT (SizeOfPatchVariable != NULL);
  ASSERT (SizeOfBuffer       != NULL);
  if (*SizeOfBuffer > 0) {
    ASSERT (Buffer != NULL);
  }

  if ((*SizeOfBuffer > MaximumDatumSize) || (*SizeOfBuffer == MAX_ADDRESS)) {
    *SizeOfBuffer = MaximumDatumSize;
    return NULL;
  }

  CopyMem (PatchVariable, Buffer, *SizeOfBuffer);
  *SizeOfPatchVariable = *SizeOfBuffer;
  return (VOID *)Buffer;
}

RETURN_STATUS
EFIAPI
LibPatchPcdSetPtrAndSizeS (
  OUT      VOID   *PatchVariable,
  OUT      UINTN  *SizeOfPatchVariable,
  IN       UINTN  MaximumDatumSize,
  IN OUT   UINTN  *SizeOfBuffer,
  IN CONST VOID   *Buffer
  )
{
  ASSERT (PatchVariable      != NULL);
  ASSERT (SizeOfPatchVariable != NULL);
  ASSERT (SizeOfBuffer       != NULL);
  if (*SizeOfBuffer > 0) {
    ASSERT (Buffer != NULL);
  }

  if ((*SizeOfBuffer > MaximumDatumSize) || (*SizeOfBuffer == MAX_ADDRESS)) {
    *SizeOfBuffer = MaximumDatumSize;
    return RETURN_INVALID_PARAMETER;
  }

  CopyMem (PatchVariable, Buffer, *SizeOfBuffer);
  *SizeOfPatchVariable = *SizeOfBuffer;
  return RETURN_SUCCESS;
}

// ---------------------------------------------------------------------------
// PcdLib interface: deprecated Set (without S suffix)
// ---------------------------------------------------------------------------

#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

UINT8
EFIAPI
LibPcdSet8 (
  IN UINTN  TokenNumber,
  IN UINT8  Value
  )
{
  SetNumeric (TokenNumber, Value);
  return Value;
}

UINT16
EFIAPI
LibPcdSet16 (
  IN UINTN   TokenNumber,
  IN UINT16  Value
  )
{
  SetNumeric (TokenNumber, Value);
  return Value;
}

UINT32
EFIAPI
LibPcdSet32 (
  IN UINTN   TokenNumber,
  IN UINT32  Value
  )
{
  SetNumeric (TokenNumber, Value);
  return Value;
}

UINT64
EFIAPI
LibPcdSet64 (
  IN UINTN   TokenNumber,
  IN UINT64  Value
  )
{
  SetNumeric (TokenNumber, Value);
  return Value;
}

BOOLEAN
EFIAPI
LibPcdSetBool (
  IN UINTN    TokenNumber,
  IN BOOLEAN  Value
  )
{
  UefiFindOrAddPcd (TokenNumber)->Value.Boolean = Value;
  return Value;
}

VOID *
EFIAPI
LibPcdSetPtr (
  IN        UINTN  TokenNumber,
  IN OUT    UINTN  *SizeOfBuffer,
  IN CONST  VOID   *Buffer
  )
{
  ASSERT (FALSE);
  return NULL;
}

#endif // DISABLE_NEW_DEPRECATED_INTERFACES

// ---------------------------------------------------------------------------
// PcdLib interface: unsupported stubs
// ---------------------------------------------------------------------------

UINTN
EFIAPI
LibPcdSetSku (
  IN UINTN  SkuId
  )
{
  ASSERT (FALSE);
  return 0;
}

VOID *
EFIAPI
LibPcdGetPtr (
  IN UINTN  TokenNumber
  )
{
  ASSERT (FALSE);
  return NULL;
}

UINTN
EFIAPI
LibPcdGetSize (
  IN UINTN  TokenNumber
  )
{
  ASSERT (FALSE);
  return 0;
}

UINT8
EFIAPI
LibPcdGetEx8 (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber
  )
{
  ASSERT (FALSE);
  return 0;
}

UINT16
EFIAPI
LibPcdGetEx16 (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber
  )
{
  ASSERT (FALSE);
  return 0;
}

UINT32
EFIAPI
LibPcdGetEx32 (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber
  )
{
  ASSERT (FALSE);
  return 0;
}

UINT64
EFIAPI
LibPcdGetEx64 (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber
  )
{
  ASSERT (FALSE);
  return 0;
}

VOID *
EFIAPI
LibPcdGetExPtr (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber
  )
{
  ASSERT (FALSE);
  return NULL;
}

BOOLEAN
EFIAPI
LibPcdGetExBool (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber
  )
{
  ASSERT (FALSE);
  return FALSE;
}

UINTN
EFIAPI
LibPcdGetExSize (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber
  )
{
  ASSERT (FALSE);
  return 0;
}

RETURN_STATUS
EFIAPI
LibPcdSetPtrS (
  IN       UINTN  TokenNumber,
  IN OUT   UINTN  *SizeOfBuffer,
  IN CONST VOID   *Buffer
  )
{
  ASSERT (FALSE);
  return RETURN_INVALID_PARAMETER;
}

RETURN_STATUS
EFIAPI
LibPcdSetEx8S (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber,
  IN UINT8       Value
  )
{
  ASSERT (FALSE);
  return RETURN_INVALID_PARAMETER;
}

RETURN_STATUS
EFIAPI
LibPcdSetEx16S (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber,
  IN UINT16      Value
  )
{
  ASSERT (FALSE);
  return RETURN_INVALID_PARAMETER;
}

RETURN_STATUS
EFIAPI
LibPcdSetEx32S (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber,
  IN UINT32      Value
  )
{
  ASSERT (FALSE);
  return RETURN_INVALID_PARAMETER;
}

RETURN_STATUS
EFIAPI
LibPcdSetEx64S (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber,
  IN UINT64      Value
  )
{
  ASSERT (FALSE);
  return RETURN_INVALID_PARAMETER;
}

RETURN_STATUS
EFIAPI
LibPcdSetExPtrS (
  IN CONST GUID   *Guid,
  IN       UINTN  TokenNumber,
  IN OUT   UINTN  *SizeOfBuffer,
  IN       VOID   *Buffer
  )
{
  ASSERT (FALSE);
  return RETURN_INVALID_PARAMETER;
}

RETURN_STATUS
EFIAPI
LibPcdSetExBoolS (
  IN CONST GUID  *Guid,
  IN UINTN       TokenNumber,
  IN BOOLEAN     Value
  )
{
  ASSERT (FALSE);
  return RETURN_INVALID_PARAMETER;
}

VOID
EFIAPI
LibPcdCallbackOnSet (
  IN CONST GUID    *Guid  OPTIONAL,
  IN UINTN         TokenNumber,
  IN PCD_CALLBACK  NotificationFunction
  )
{
  ASSERT (FALSE);
}

VOID
EFIAPI
LibPcdCancelCallback (
  IN CONST GUID    *Guid  OPTIONAL,
  IN UINTN         TokenNumber,
  IN PCD_CALLBACK  NotificationFunction
  )
{
  ASSERT (FALSE);
}

UINTN
EFIAPI
LibPcdGetNextToken (
  IN CONST GUID  *Guid  OPTIONAL,
  IN UINTN       TokenNumber
  )
{
  ASSERT (FALSE);
  return 0;
}

GUID *
EFIAPI
LibPcdGetNextTokenSpace (
  IN CONST GUID  *TokenSpaceGuid
  )
{
  ASSERT (FALSE);
  return NULL;
}

VOID
EFIAPI
LibPcdGetInfo (
  IN        UINTN     TokenNumber,
  OUT       PCD_INFO  *PcdInfo
  )
{
  ASSERT (FALSE);
}

VOID
EFIAPI
LibPcdGetInfoEx (
  IN CONST  GUID      *Guid,
  IN        UINTN     TokenNumber,
  OUT       PCD_INFO  *PcdInfo
  )
{
  ASSERT (FALSE);
}

UINTN
EFIAPI
LibPcdGetSku (
  VOID
  )
{
  ASSERT (FALSE);
  return 0;
}
