/** @file
  User Authentication Protocol

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_USER_AUTH_PROTOCOL_H__
#define __NVIDIA_USER_AUTH_PROTOCOL_H__

#define NVIDIA_USER_AUTH_PROTOCOL_GUID \
  { \
    0x9a46fc38, 0xc8ff, 0x11ed, { 0xb3, 0x9b, 0x57, 0x78, 0x4c, 0x66, 0x44, 0xfd } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_USER_AUTH_PROTOCOL NVIDIA_USER_AUTH_PROTOCOL;

/**
  Prompt user to enter password and check if it is valid.

  @param  This                     pointer to NVIDIA_USER_AUTH_PROTOCOL

  @retval EFI_SUCCESS              Valid password
  @retval EFI_SECURITY_VIOLATION   Invalid password
**/
typedef
EFI_STATUS
(EFIAPI *USER_AUTH_CHECK_PASSWORD)(
  IN     NVIDIA_USER_AUTH_PROTOCOL *This
  );

/// NVIDIA_USER_AUTH_PROTOCOL protocol structure.
struct _NVIDIA_USER_AUTH_PROTOCOL {
  USER_AUTH_CHECK_PASSWORD    CheckPassword;
};

#endif
