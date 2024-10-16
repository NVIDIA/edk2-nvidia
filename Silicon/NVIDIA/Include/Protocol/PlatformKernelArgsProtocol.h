/** @file
  Platform kernel args protocol

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PLATFORM_KERNEL_ARGS_PROTOCOL_H__
#define __PLATFORM_KERNEL_ARGS_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define NVIDIA_PLATFORM_KERNEL_ARGS_PROTOCOL_GUID \
  {0x41330bd9, 0xcac1, 0x4dd4, {0x93, 0x64, 0x31, 0x61, 0x39, 0xd0, 0xec, 0xf4}}

typedef struct _NVIDIA_PLATFORM_KERNEL_ARGS_PROTOCOL NVIDIA_PLATFORM_KERNEL_ARGS_PROTOCOL;

/**
  Append platform kernel args

  @param[in]  Args              Pointer to kernel args string
  @param[in]  Size              Total bytes available in Args array

  @retval EFI_SUCCESS              Operation successful
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *PLATFORM_KERNEL_ARGS_APPEND)(
  IN CHAR16  *Args,
  IN UINTN   Size
  );

// protocol structure
struct _NVIDIA_PLATFORM_KERNEL_ARGS_PROTOCOL {
  PLATFORM_KERNEL_ARGS_APPEND    Append;
};

extern EFI_GUID  gNVIDIAPlatformKernelArgsProtocolGuid;

#endif
