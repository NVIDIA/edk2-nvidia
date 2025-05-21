/** @file

  BootConfig Protocol library

  SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BOOTCONFIG_PROTOCOL_LIB_H_
#define __BOOTCONFIG_PROTOCOL_LIB_H_

#include <Protocol/BootConfigUpdateProtocol.h>

EFI_STATUS
EFIAPI
GetBootConfigUpdateProtocol (
  OUT NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  **BootConfigProtocol
  );

EFI_STATUS
EFIAPI
BootConfigAddSerialNumber (
  CONST CHAR8  *NewValue OPTIONAL,
  CHAR8        *OutStrSn,
  UINT32       OutStrSnLen
  );

#endif /* __BOOTCONFIG_PROTOCOL_LIB_H_ */
