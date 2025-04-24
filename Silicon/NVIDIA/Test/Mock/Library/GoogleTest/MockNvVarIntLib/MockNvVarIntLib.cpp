/** @file
  Google Test mocks for UefiLib

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <GoogleTest/Library/MockNvVarIntLib.h>

MOCK_INTERFACE_DEFINITION (MockNvVarIntLib);

MOCK_FUNCTION_DEFINITION (MockNvVarIntLib, ComputeVarMeasurement, 6, EFIAPI);
