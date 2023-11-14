/** @file
  Google Test mocks for Standalone MM MmVarLib

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_MM_VAR_LIB_H_
#define MOCK_MM_VAR_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
#include <Uefi.h>
#include <Library/MmVarLib.h>
}

struct MockMmVarLib {
  MOCK_INTERFACE_DECLARATION (MockMmVarLib);

  MOCK_FUNCTION_DECLARATION (
    BOOLEAN,
    DoesVariableExist,
    (IN CHAR16    *VarName,
     IN EFI_GUID  *VarGuid,
     OUT UINTN     *VarSize OPTIONAL,
     OUT UINT32    *Attr    OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    MmGetVariable3,
    (IN CONST CHAR16    *Name,
     IN CONST EFI_GUID  *Guid,
     OUT VOID           **Value,
     OUT UINTN          *Size OPTIONAL,
     OUT UINT32         *Attr OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    MmGetVariable2,
    (IN CONST CHAR16    *Name,
     IN CONST EFI_GUID  *Guid,
     OUT VOID           **Value,
     OUT UINTN          *Size OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    MmGetVariable,
    (IN  CONST CHAR16    *Name,
     IN  CONST EFI_GUID  *Guid,
     OUT VOID            *Value,
     IN  UINTN           Size OPTIONAL)
    );
};

#endif
