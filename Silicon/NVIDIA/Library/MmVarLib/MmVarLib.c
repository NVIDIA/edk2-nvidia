/** @file
  The StMM Library provides functions to get EFI Variables. This is meant for use from
  StMM Drivers and Libraries.

  SPDX-FileCopyrightText: Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Guid/ImageAuthentication.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/MmServicesTableLib.h>
#include <Protocol/SmmVariable.h>
#include <Library/MmVarLib.h>

STATIC EFI_SMM_VARIABLE_PROTOCOL  *mSmmVar = NULL;

/*
 * GetSmmVarProto
 * Get the SmmVariable Protocol.
 *
 * @return EFI_SUCCESS Found the SmmVariable Protocol.
 *         other       Failed to get the SmmVariable Proto.
 */
STATIC
EFI_STATUS
GetSmmVarProto (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;
  if (mSmmVar == NULL) {
    Status = gMmst->MmLocateProtocol (
                      &gEfiSmmVariableProtocolGuid,
                      NULL,
                      (VOID **)&mSmmVar
                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: gEfiSmmVariableProtocolGuid: NOT LOCATED!\n",
        __FUNCTION__
        ));
      mSmmVar = NULL;
    }
  }

  return Status;
}

/*
 * DoesVarExist
 * Is the variable in the VarStore.
 *
 * @param[in] VarName  Name of Variable.
 * @param[in] VarGuid  Guid of Variable.
 *
 * @return TRUE  Variable exists.
 *         FALSE Variable doesn't exist.
 */
EFIAPI
BOOLEAN
DoesVariableExist (
  IN  CHAR16    *Name,
  IN  EFI_GUID  *Guid,
  OUT UINTN     *Size OPTIONAL,
  OUT UINT32    *Attr    OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINTN       VarSz;
  UINT32      VarAttr;
  BOOLEAN     VarExists;

  Status = GetSmmVarProto ();
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  VarSz  = 0;
  Status = mSmmVar->SmmGetVariable (
                      Name,
                      Guid,
                      &VarAttr,
                      &VarSz,
                      NULL
                      );
  if ((Status == EFI_BUFFER_TOO_SMALL)) {
    VarExists = TRUE;
    if (Size != NULL) {
      *Size = VarSz;
    }

    if (Attr != NULL) {
      *Attr = VarAttr;
    }
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "Var %s Doesn't exist %r\n",
      Name,
      Status
      ));
    VarExists = FALSE;
  }

  return VarExists;
}

/**
  Returns the status whether get the variable success. The function retrieves
  variable  through the UEFI Runtime Service GetVariable().  The
  returned buffer is allocated using AllocatePool().  The caller is responsible
  for freeing this buffer with FreePool().

  If Name  is NULL, then ASSERT().
  If Guid  is NULL, then ASSERT().
  If Value is NULL, then ASSERT().

  @param[in]  Name  The pointer to a Null-terminated Unicode string.
  @param[in]  Guid  The pointer to an EFI_GUID structure
  @param[out] Value The buffer point saved the variable info.
  @param[out] Size  The buffer size of the variable.

  @return EFI_OUT_OF_RESOURCES      Allocate buffer failed.
  @return EFI_SUCCESS               Find the specified variable.
  @return Others Errors             Return errors from call to gRT->GetVariable.

**/
EFI_STATUS
EFIAPI
MmGetVariable2 (
  IN CONST CHAR16    *Name,
  IN CONST EFI_GUID  *Guid,
  OUT VOID           **Value,
  OUT UINTN          *Size OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;

  if ((Name == NULL) ||
      (Guid == NULL) ||
      (Value == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetSmmVarProto ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Try to get the variable size.
  //
  BufferSize = 0;
  *Value     = NULL;
  if (Size != NULL) {
    *Size = 0;
  }

  Status = mSmmVar->SmmGetVariable (
                      (CHAR16 *)Name,
                      (EFI_GUID *)Guid,
                      NULL,
                      &BufferSize,
                      NULL
                      );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  //
  // Allocate buffer to get the variable.
  //
  *Value = AllocatePool (BufferSize);
  ASSERT (*Value != NULL);
  if (*Value == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Get the variable data.
  //
  Status = mSmmVar->SmmGetVariable (
                      (CHAR16 *)Name,
                      (EFI_GUID *)Guid,
                      NULL,
                      &BufferSize,
                      *Value
                      );
  if (EFI_ERROR (Status)) {
    FreePool (*Value);
    *Value = NULL;
  }

  if (Size != NULL) {
    *Size = BufferSize;
  }

  return Status;
}

/** Return the attributes of the variable.

  Returns the status whether get the variable success. The function retrieves
  variable  through the UEFI Runtime Service GetVariable().  The
  returned buffer is allocated using AllocatePool().  The caller is responsible
  for freeing this buffer with FreePool().  The attributes are returned if
  the caller provides a valid Attribute parameter.

  If Name  is NULL OR If Guid  is NULL If Value is NULL, then return EFI_INVALID_PARAMETER.

  @param[in]  Name  The pointer to a Null-terminated Unicode string.
  @param[in]  Guid  The pointer to an EFI_GUID structure
  @param[out] Value The buffer point saved the variable info.
  @param[out] Size  The buffer size of the variable.
  @param[out] Attr  The pointer to the variable attributes as found in var store

  @retval EFI_OUT_OF_RESOURCES      Allocate buffer failed.
  @retval EFI_SUCCESS               Find the specified variable.
  @retval Others Errors             Return errors from call to gRT->GetVariable.

**/
EFI_STATUS
EFIAPI
MmGetVariable3 (
  IN CONST CHAR16    *Name,
  IN CONST EFI_GUID  *Guid,
  OUT VOID           **Value,
  OUT UINTN          *Size OPTIONAL,
  OUT UINT32         *Attr OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;
  UINT32      VarAttr;

  if ((Name == NULL) ||
      (Guid == NULL) ||
      (Value == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetSmmVarProto ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  //
  // Try to get the variable size.
  //
  BufferSize = 0;
  *Value     = NULL;
  if (Size != NULL) {
    *Size = 0;
  }

  if (Attr != NULL) {
    *Attr = 0;
  }

  DEBUG ((DEBUG_ERROR, "%a:%d\n", __FUNCTION__, __LINE__));
  Status = mSmmVar->SmmGetVariable (
                      (CHAR16 *)Name,
                      (EFI_GUID *)Guid,
                      NULL,
                      &BufferSize,
                      NULL
                      );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  DEBUG ((DEBUG_ERROR, "%a:%d\n", __FUNCTION__, __LINE__));

  //
  // Allocate buffer to get the variable.
  //
  *Value = AllocatePool (BufferSize);
  ASSERT (*Value != NULL);
  if (*Value == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((DEBUG_ERROR, "%a:%d\n", __FUNCTION__, __LINE__));
  //
  // Get the variable data.
  //
  Status = mSmmVar->SmmGetVariable (
                      (CHAR16 *)Name,
                      (EFI_GUID *)Guid,
                      &VarAttr,
                      &BufferSize,
                      *Value
                      );
  if (EFI_ERROR (Status)) {
    FreePool (*Value);
    *Value = NULL;
  }

  if (Size != NULL) {
    *Size = BufferSize;
  }

  if (Attr != NULL) {
    *Attr = VarAttr;
  }

  return Status;
}

/** MmGetVariable.

  Return a Variable via SmmVariable Protocol. The caller passes in an allocated
  buffer and the expected size. If the Variable size doesn't match the expected
  size return EFI_INVALID_PARAMETER.
  Returns the status of getvariable.

  If Name  is NULL, or.
  If Guid  is NULL, or.
  If Value is NULL, or then return EFI_INVALID_PARAMETER.

  @param[in]  Name  The pointer to a Null-terminated Unicode string.
  @param[in]  Guid  The pointer to an EFI_GUID structure
  @param[out] Value The buffer point saved the variable info.
  @param[in]  Size  The buffer size of the variable.

  @retval EFI_SUCCESS               Find the specified variable.
  @retval Others Errors             Return errors from call to gRT->GetVariable.

**/
EFI_STATUS
EFIAPI
MmGetVariable (
  IN  CONST CHAR16    *Name,
  IN  CONST EFI_GUID  *Guid,
  OUT VOID            *Value,
  IN  UINTN           Size
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;

  if ((Name == NULL) ||
      (Guid == NULL) ||
      (Value == NULL) || (Size == 0))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetSmmVarProto ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Try to get the variable size.
  //
  BufferSize = 0;
  DEBUG ((DEBUG_ERROR, "%a:%d\n", __FUNCTION__, __LINE__));

  Status = mSmmVar->SmmGetVariable (
                      (CHAR16 *)Name,
                      (EFI_GUID *)Guid,
                      NULL,
                      &BufferSize,
                      NULL
                      );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get Var %r\n", __FUNCTION__, Status));
    return Status;
  }

  //
  // Check that the buffer size matches the expected var size
  //
  if (BufferSize != Size) {
    DEBUG ((
      DEBUG_ERROR,
      "Expected VarSize %u but got %u\n",
      BufferSize,
      Size
      ));
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Value, Size);

  //
  // Get the variable data.
  //
  Status = mSmmVar->SmmGetVariable ((CHAR16 *)Name, (EFI_GUID *)Guid, NULL, &BufferSize, Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get Var %s %r\n", Name, Status));
  }

  return Status;
}

/** TestHookMmVarLibClearPtr
    To be called from the Unit Tests to clear the SmmVar between tests.
 */
EFIAPI
VOID
TestHookMmVarLibClearPtr (
  VOID
  )
{
  mSmmVar = NULL;
}
