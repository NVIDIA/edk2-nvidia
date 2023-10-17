/** @file
  Google Test mocks for SmmVariable Protocol

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <GoogleTest/Library/MockNvNorFlashProto.h>

MOCK_INTERFACE_DEFINITION(MockNvNorFlashProto);

MOCK_FUNCTION_DEFINITION(MockNvNorFlashProto, NvNorFlashProto_Read, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION(MockNvNorFlashProto, NvNorFlashProto_Write, 4, EFIAPI);
MOCK_FUNCTION_DEFINITION(MockNvNorFlashProto, NvNorFlashProto_Erase, 3, EFIAPI);

static NVIDIA_NOR_FLASH_PROTOCOL localNvNorFlashProto = {
  0,                      // FvbAttributes;
  NULL,                   // GetAttributes;
  NvNorFlashProto_Read,   // Read
  NvNorFlashProto_Write,  // Write
  NvNorFlashProto_Erase,  // Erase
};

extern "C" {
  NVIDIA_NOR_FLASH_PROTOCOL* MockNvNorFlash = &localNvNorFlashProto;
}
