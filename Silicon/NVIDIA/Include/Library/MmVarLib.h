/** @file

  MmVarLib

  SPDX-FileCopyrightText: Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MM_VAR_LIB__
#define __MM_VAR_LIB__

#include <Guid/GlobalVariable.h>

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
  );

/** Return the attributes of the variable.

  Returns the status whether get the variable success. The function retrieves
  variable  through the UEFI Runtime Service GetVariable().  The
  returned buffer is allocated using AllocatePool().  The caller is responsible
  for freeing this buffer with FreePool().  The attributes are returned if
  the caller provides a valid Attribute parameter.


  @param[in]  Name  The pointer to a Null-terminated Unicode string.
  @param[in]  Guid  The pointer to an EFI_GUID structure
  @param[out] Value The buffer point saved the variable info.
  @param[out] Size  The buffer size of the variable.
  @param[out] Attr  The pointer to the variable attributes as found in var store

  @retval EFI_OUT_OF_RESOURCES      Allocate buffer failed.
  @retval EFI_INVALID_PARAMETER     If Name is NULL OR If Guid is NULL If Value is NULL.
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
  );

/**
  Returns the status whether get the variable success. The function retrieves
  variable  through the UEFI Runtime Service GetVariable().  The
  returned buffer is allocated using AllocatePool().  The caller is responsible
  for freeing this buffer with FreePool().

  @param[in]  Name  The pointer to a Null-terminated Unicode string.
  @param[in]  Guid  The pointer to an EFI_GUID structure
  @param[out] Value The buffer point saved the variable info.
  @param[out] Size  The buffer size of the variable.

  @return EFI_OUT_OF_RESOURCES      Allocate buffer failed.
  @retval EFI_INVALID_PARAMETER     If Name is NULL OR If Guid is NULL If Value is NULL.
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
  );

/** MmGetVariable.

  Return a Variable via SmmVariable Protocol. The caller passes in an allocated
  buffer and the expected size. If the Variable size doesn't match the expected
  size return EFI_INVALID_PARAMETER.
  Returns the status of getvariable.

  @param[in]  Name  The pointer to a Null-terminated Unicode string.
  @param[in]  Guid  The pointer to an EFI_GUID structure
  @param[out] Value The buffer point saved the variable info.
  @param[in]  Size  The buffer size of the variable.

  @retval EFI_SUCCESS               Find the specified variable.
  @retval EFI_INVALID_PARAMETER     If Name is NULL OR If Guid is NULL If Value is NULL.
  @retval Others Errors             Return errors from call to gRT->GetVariable.

**/
EFI_STATUS
EFIAPI
MmGetVariable (
  IN  CONST CHAR16    *Name,
  IN  CONST EFI_GUID  *Guid,
  OUT VOID            *Value,
  IN  UINTN           Size
  );

#endif // __MM_VAR_LIB__
