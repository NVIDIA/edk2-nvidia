/** @file
*
*  BootConfig Protocol definition.
*
*  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __BOOTCONFIG_PROTOCOL_H__
#define __BOOTCONFIG_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>

#define BOOTCONFIG_MAX_LEN  1024

//
// Define for forward reference.
//
typedef struct _NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL;

/**
  Copy the new BootConfig into the BootConfig Protocol

  @param[in] This     A pointer to the NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL.
  @param[in] NewArgs  A pointer to the new bootconfig string argument.
  @param[in] NewValue A pointer to the new value string for the argument.

  @retval EFI_SUCCESS            The operation completed successfully.
  @retval EFI_INVALID_PARAMETER  "This" is NULL.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate space for the new args.

**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_BOOTCONFIG_UPDATE)(
  IN NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *This,
  IN CONST CHAR8                        *NewArgs,
  IN CONST CHAR8                        *NewValue
  );

// NVIDIA_BOOTCONFIG_PROTOCOL protocol structure.
struct _NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL {
  CHAR8                       *BootConfigs;
  NVIDIA_BOOTCONFIG_UPDATE    UpdateBootConfigs;
};

extern EFI_GUID  gNVIDIABootConfigUpdateProtocolGuid;

#endif /* __BOOTCONFIG_PROTOCOL_H__ */
