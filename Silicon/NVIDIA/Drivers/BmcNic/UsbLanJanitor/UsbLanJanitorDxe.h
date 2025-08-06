/** @file
  The header file of USB LAN Janitor driver

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef USB_LAN_JANITOR_DXE_H_
#define USB_LAN_JANITOR_DXE_H_

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Protocol/UsbNicInfoProtocol.h>

#define MAX_ADDR_STR_LEN          32
#define MAX_ADDR_STR_SIZE         (sizeof (CHAR16) * MAX_ADDR_STR_LEN)
#define USB_LAN_JANITOR_DEBUG     DEBUG_VERBOSE
#define USB_LAN_JANITOR_VAR_ATTR  (EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE)
#define USB_LAN_JANITOR_VARIABLE  L"BmcUsbLanMacLast"

#endif
