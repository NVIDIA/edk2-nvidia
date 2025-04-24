/** @file
  Google Test mocks for HashApiLib

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <GoogleTest/Library/MockHashApiLib.h>

MOCK_INTERFACE_DEFINITION (MockHashApiLib);

MOCK_FUNCTION_DEFINITION (MockHashApiLib, HashApiGetContextSize, 0, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHashApiLib, HashApiInit, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHashApiLib, HashApiUpdate, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHashApiLib, HashApiFinal, 2, EFIAPI);
