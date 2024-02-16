/** @file
  Google Test mocks for Standalone MM MmVarLib

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <GoogleTest/Library/MockMmVarLib.h>

MOCK_INTERFACE_DEFINITION(MockMmVarLib);

MOCK_FUNCTION_DEFINITION(MockMmVarLib, DoesVariableExist, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION(MockMmVarLib, MmGetVariable3, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION(MockMmVarLib, MmGetVariable2, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION(MockMmVarLib, MmGetVariable, 4, EFIAPI);
