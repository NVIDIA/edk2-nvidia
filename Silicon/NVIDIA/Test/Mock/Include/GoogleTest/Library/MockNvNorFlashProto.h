/** @file
  Google Test mocks SmmVariable Prototcol

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_NV_NOR_FLASH_PROTO_LIB_H_
#define MOCK_NV_NOR_FLASH_PROTO_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <Uefi.h>
  #include <Protocol/NorFlash.h>
}

struct MockNvNorFlashProto {
  MOCK_INTERFACE_DECLARATION (MockNvNorFlashProto);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    NvNorFlashProto_Read,
    (IN NVIDIA_NOR_FLASH_PROTOCOL *This,
     IN UINT32                    Offset,
     IN UINT32                    Size,
     IN VOID                      *Buffer)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    NvNorFlashProto_Write,
    (IN NVIDIA_NOR_FLASH_PROTOCOL *This,
     IN UINT32                    Offset,
     IN UINT32                    Size,
     IN VOID                      *Buffer)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    NvNorFlashProto_Erase,
    (IN NVIDIA_NOR_FLASH_PROTOCOL *This,
     IN UINT32                    Lba,
     IN UINT32                    NumLba)
    );
};

#endif
