/** @file
  The header file of Redfish Host Interface Control driver

  SPDX-FileCopyrightText: Copyright (c) 2023-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef REDFISH_HOST_INTERFACE_CONTROL_DXE_H_
#define REDFISH_HOST_INTERFACE_CONTROL_DXE_H_

#include <Uefi.h>
#include <Pi/PiStatusCode.h>
#include <Library/UefiLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/RedfishEventLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Protocol/Smbios.h>
#include <Protocol/UsbNicInfoProtocol.h>

#define REDFISH_HOST_INTERFACE_DISABLE  0x00
#define REDFISH_HOST_INTERFACE_MISSING  "Redfish host interface is missing"

#endif
