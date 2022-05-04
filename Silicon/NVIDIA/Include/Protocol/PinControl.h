/** @file
  Pin Control Protocol

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PIN_CONTROL_PROTOCOL_H
#define PIN_CONTROL_PROTOCOL_H

#include <Uefi/UefiSpec.h>

#define NVIDIA_PIN_CONTROL_PROTOCOL_GUID \
  { \
  0x1f4054ed, 0xfb5b, 0x43a3, { 0xae, 0xe6, 0x10, 0xea, 0x43, 0x39, 0xb6, 0x2a } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_PIN_CONTROL_PROTOCOL NVIDIA_PIN_CONTROL_PROTOCOL;

/**
  This function enables the pin control id on the system.

  @param[in]     This                The instance of the NVIDIA_PIN_CONTROL_PROTOCOL.
  @param[in]     PinControlId        Id of the pin control configuration

  @return EFI_SUCCESS                Config is enabled
  @return EFI_NOT_FOUND              Config is not supported.
  @return others                     Failed to enable if the config is supported
**/
typedef
EFI_STATUS
(EFIAPI *PIN_CONTROL_ENABLE) (
  IN  NVIDIA_PIN_CONTROL_PROTOCOL *This,
  IN  UINT32                      PinControlId
  );

/// NVIDIA_CLOCK_PARENT_PROTOCOL protocol structure.
struct _NVIDIA_PIN_CONTROL_PROTOCOL {

  PIN_CONTROL_ENABLE   Enable;
};

extern EFI_GUID gNVIDIAPinControlProtocolGuid;

#endif
