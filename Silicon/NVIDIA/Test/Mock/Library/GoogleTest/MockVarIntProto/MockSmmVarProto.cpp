/** @file
  Google Test mocks for SmmVariable Protocol

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <GoogleTest/Library/MockSmmVarProto.h>

MOCK_INTERFACE_DEFINITION (MockSmmVarProto);

MOCK_FUNCTION_DEFINITION (MockSmmVarProto, SmmVarProto_SmmGetVariable, 5, EFIAPI);

static EFI_SMM_VARIABLE_PROTOCOL  localSmmVar = {
  SmmVarProto_SmmGetVariable, // EFI_GET_VARIABLE
  NULL,                       // EFI_GET_NEXT_VARIABLE_NAME
  NULL,                       // EFI_SET_VARIABLE
  NULL,                       // EFI_QUERY_VARIABLE_INFO
};

extern "C" {
  EFI_SMM_VARIABLE_PROTOCOL  *MockSmmVar = &localSmmVar;
}
