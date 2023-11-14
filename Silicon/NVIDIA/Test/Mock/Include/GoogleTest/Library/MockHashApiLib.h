/** @file
  Google Test mocks for HashApiLib

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_HASH_API_LIB_H_
#define MOCK_HASH_API_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
#include <Uefi.h>
#include <Library/HashApiLib.h>
}

struct MockHashApiLib {
  MOCK_INTERFACE_DECLARATION (MockHashApiLib);

  MOCK_FUNCTION_DECLARATION (
    UINTN,
    HashApiGetContextSize,
    ()
    );

  MOCK_FUNCTION_DECLARATION (
    BOOLEAN,
    HashApiInit,
    (OUT HASH_API_CONTEXT HashContext)
    );

  MOCK_FUNCTION_DECLARATION (
    BOOLEAN,
    HashApiUpdate,
    (IN HASH_API_CONTEXT  HashContext,
     IN VOID              *DataToHash,
     IN UINTN             DataToHashLen)
    );

  MOCK_FUNCTION_DECLARATION (
    BOOLEAN,
    HashApiFinal,
    (IN  HASH_API_CONTEXT  HashContext,
     OUT UINT8             *Digest)
    );
};

#endif
