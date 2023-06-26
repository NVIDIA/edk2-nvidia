/** @file
  NVIDIA Memory Test Config Protocol

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef MEMORY_TEST_CONFIG_H__
#define MEMORY_TEST_CONFIG_H__

#include <Uefi/UefiSpec.h>
#include <Library/MemoryVerificationLib.h>

#define NVIDIA_MEMORY_TEST_CONFIG_PROTOCOL_GUID \
  { \
  0x78bb2dfe, 0xe17b, 0x4ca1, { 0x9b, 0x82, 0xa3, 0x1e, 0x33, 0xf1, 0x1f, 0xac } \
  }

/// NVIDIA_BPMP_IPC_PROTOCOL protocol structure.
typedef struct {
  MEMORY_TEST_MODE    TestMode;
  UINT64              Parameter1;
  UINT64              Parameter2;
} NVIDIA_MEMORY_TEST_CONFIG_PROTOCOL;

extern EFI_GUID  gNVIDIAMemoryTestConfig;

#endif
