/** @file
  Platform Redfish boot order driver header file.

  (C) Copyright 2022 Hewlett Packard Enterprise Development LP<BR>
  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_REDFISH_BOOT_DXE_H_
#define PLATFORM_REDFISH_BOOT_DXE_H_

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/HiiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/RedfishEventLib.h>
#include <Library/JsonLib.h>
#include <Library/RedfishDebugLib.h>

#include <Protocol/HiiConfigAccess.h>
#include <Protocol/EdkIIRedfishResourceAddendumProtocol.h>

#include <Guid/VariableFormat.h>
#include <Guid/MdeModuleHii.h>
#include <Guid/GlobalVariable.h>

#include "PlatformRedfishBootData.h"

extern UINT8  PlatformRedfishBootVfrBin[];

//
// Define driver version Driver Binding Protocol
//
#define ADDENDUM_PROTOCOL_VERSION       0x01
#define COMPUTER_SYSTEM_SCHEMA_VERSION  "x-uefi-redfish-ComputerSystem.v1_17_0"
#define REDFISH_BOOT_OBJECT_NAME        "Boot"
#define REDFISH_BOOTORDER_OBJECT_NAME   "BootOrder"
#define REDFISH_BOOT_DEBUG_DUMP         DEBUG_INFO

#pragma pack(1)

///
/// HII specific Vendor Device Path definition.
///
typedef struct {
  VENDOR_DEVICE_PATH          VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    End;
} HII_VENDOR_DEVICE_PATH;

#pragma pack()

#endif
