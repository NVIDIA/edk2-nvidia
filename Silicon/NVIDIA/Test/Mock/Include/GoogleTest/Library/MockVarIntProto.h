/** @file
  Google Test mocks SmmVariable Prototcol

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_SMM_VAR_PROTO_LIB_H_
#define MOCK_SMM_VAR_PROTO_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <Uefi.h>
  #include <Protocol/SmmVariable.h>
}

struct MockSmmVarProto {
  MOCK_INTERFACE_DECLARATION (MockSmmVarProto);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    SmmVarProto_SmmGetVariable,
    (IN      CHAR16    *VariableName,
     IN      EFI_GUID  *VendorGuid,
     OUT     UINT32    *Attributes OPTIONAL,
     IN OUT  UINTN     *DataSize,
     OUT     VOID      *Data)
    );
};

#endif
