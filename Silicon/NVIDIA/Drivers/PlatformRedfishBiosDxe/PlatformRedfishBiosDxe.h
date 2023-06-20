/** @file
  The header file of platform redfish BIOS driver

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_REDFISH_BIOS_DXE_H_
#define PLATFORM_REDFISH_BIOS_DXE_H_

#include <Uefi.h>

//
// Include Library Classes commonly used by UEFI Drivers
//
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/JsonLib.h>
#include <Library/RedfishPlatformConfigLib.h>

#include <Protocol/EdkIIRedfishResourceAddendumProtocol.h>

//
// Define driver version Driver Binding Protocol
//
#define ADDENDUM_PROTOCOL_VERSION  0x01

#define REDFISH_BIOS_ATTRIBUTES_NAME     "Attributes"
#define REDFISH_BIOS_CONFIG_LANG_PREFIX  L"/Bios/Attributes/"
#define REDFISH_BIOS_CONFIG_LANG_SIZE    64
#define REDFISH_TOP_MENU_PATH            "./Device Manager"
#define REDFISH_BIOS_DEBUG_DUMP          DEBUG_INFO

#endif
