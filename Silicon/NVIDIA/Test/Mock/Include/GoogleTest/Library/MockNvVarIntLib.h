/** @file
  Google Test mocks for NvVarIntLib

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_NV_VAR_INT_LIB_H_
#define MOCK_NV_VAR_INT_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
#include <Uefi.h>
#include <Library/NvVarIntLib.h>
}

struct MockNvVarIntLib {
  MOCK_INTERFACE_DECLARATION (MockNvVarIntLib);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    ComputeVarMeasurement,
    (IN  CHAR16                   *VarName   OPTIONAL,
     IN  EFI_GUID                 *VarGuid   OPTIONAL,
     IN  UINT32                   Attributes OPTIONAL,
     IN  VOID                     *Data      OPTIONAL,
     IN  UINTN                    DataSize   OPTIONAL,
     OUT UINT8                    *Meas)
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
