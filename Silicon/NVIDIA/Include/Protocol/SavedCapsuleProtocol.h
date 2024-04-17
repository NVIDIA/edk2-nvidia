/** @file

  NVIDIA saved capsule protocol

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_SAVED_CAPSULE_PROTOCOL_H__
#define __NVIDIA_SAVED_CAPSULE_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define NVIDIA_SAVED_CAPSULE_PROTOCOL_GUID    \
  {0xddac6297, 0x19fa, 0x46de, {0x8b, 0xb2, 0xcc, 0xdd, 0x94, 0x12, 0x25, 0xbe}}

typedef struct _NVIDIA_SAVED_CAPSULE_PROTOCOL NVIDIA_SAVED_CAPSULE_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *SAVED_CAPSULE_GET_CAPSULE)(
  IN  NVIDIA_SAVED_CAPSULE_PROTOCOL *This,
  OUT EFI_CAPSULE_HEADER            **CapsuleHeader
  );

// Protocol structure
struct _NVIDIA_SAVED_CAPSULE_PROTOCOL {
  SAVED_CAPSULE_GET_CAPSULE    GetCapsule;
};

extern EFI_GUID  gNVIDIASavedCapsuleProtocolGuid;

#endif
