/** @file
  Re-define types within AvbLib

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef AVBLIB_UEFI_PORT_H
#define AVBLIB_UEFI_PORT_H

#include <Library/BaseLib.h>

// #ifndef uint64_t
#define uint64_t  UINT64
// #endif
// #ifndef uint32_t
#define uint32_t  UINT32
// #endif
// #ifndef uint16_t
#define uint16_t  UINT16
// #endif
// #ifndef uint8_t
#define uint8_t  UINT8
// #endif
// #ifndef int64_t
#define int64_t  INT64
// #endif
// #ifndef int32_t
#define int32_t  INT32
// #endif
// #ifndef uintptr_t
#define uintptr_t  UINTN
// #endif
// #ifndef size_t
#define size_t  UINTN
// #endif
// #ifndef bool
#define bool  BOOLEAN
// #endif
// #ifndef true
#define true  TRUE
// #endif
// #ifndef false
#define false  FALSE
// #endif

#endif
