/** @file
  NVIDIA SMMMU V3 Controller Protocol

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_SMMU_V3_PROTOCOL_H__
#define __NVIDIA_SMMU_V3_PROTOCOL_H__

#define NVIDIA_SMMUV3_CONTROLLER_PROTOCOL_GUID \
  { \
  0xF6C64F84, 0x702C, 0x4BE7, { 0xA4, 0x1B, 0x64, 0xD5, 0xB5, 0x5F, 0x10, 0x1C } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_SMMUV3_CONTROLLER_PROTOCOL NVIDIA_SMMUV3_CONTROLLER_PROTOCOL;

/// NVIDIA_SMMUV3_CONTROLLER_PROTOCOL protocol structure.
struct _NVIDIA_SMMUV3_CONTROLLER_PROTOCOL {
  UINT32    PHandle;
};

extern EFI_GUID  gNVIDIASmmuV3ProtocolGuid;

#endif
