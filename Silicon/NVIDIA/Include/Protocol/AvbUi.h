/** @file
  AVB UI Protocol - Android Verified Boot User Interface.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __NVIDIA_AVB_UI_PROTOCOL_H__
#define __NVIDIA_AVB_UI_PROTOCOL_H__

#include <Uefi.h>

#define NVIDIA_AVB_UI_PROTOCOL_GUID \
  { 0x7a8f2e3d, 0x4c5b, 0x6a9e, { 0x8d, 0x7f, 0x1c, 0x2b, 0x3a, 0x4e, 0x5f, 0x60 } }

typedef struct _NVIDIA_AVB_UI_PROTOCOL NVIDIA_AVB_UI_PROTOCOL;

//
// AVB Boot States
//
typedef enum {
  AVB_UI_STATE_GREEN,     // Verified boot - no UI needed
  AVB_UI_STATE_YELLOW,    // Unverified signing key - show warning
  AVB_UI_STATE_ORANGE,    // Device unlocked - show warning
  AVB_UI_STATE_RED,       // Verification failed - show error and halt
  AVB_UI_STATE_RED_EIO    // I/O error - show error and halt
} AVB_UI_STATE;

/**
  Display AVB UI based on the boot state.

  @param[in] This       Pointer to this protocol instance.
  @param[in] State      The AVB boot state to display.

  @retval EFI_SUCCESS   UI displayed successfully.
  @retval Other         Error occurred.
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AVB_UI_SHOW)(
  IN NVIDIA_AVB_UI_PROTOCOL  *This,
  IN AVB_UI_STATE            State
  );

struct _NVIDIA_AVB_UI_PROTOCOL {
  NVIDIA_AVB_UI_SHOW    Show;
};

extern EFI_GUID  gNVIDIAAvbUiProtocolGuid;

#endif // __NVIDIA_AVB_UI_PROTOCOL_H__
