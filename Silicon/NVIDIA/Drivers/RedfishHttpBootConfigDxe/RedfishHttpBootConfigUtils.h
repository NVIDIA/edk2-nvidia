/** @file
  Utility functions for Redfish HTTP Boot Configuration.
  These functions are shared between the driver and unit tests.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef REDFISH_HTTP_BOOT_CONFIG_UTILS_H_
#define REDFISH_HTTP_BOOT_CONFIG_UTILS_H_

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/DevicePath.h>
#include <Guid/GlobalVariable.h>

#include "RedfishHttpBootConfigVfrDefs.h"

extern EFI_GUID  gNvidiaHttpBootConfigGuid;

#ifdef __cplusplus
extern "C" {
#endif

/**
  Find NIC handle by MAC address.

  @param[in]  MacAddr     Target MAC address
  @param[out] NicHandle   Handle of matching NIC

  @retval EFI_SUCCESS     NIC found
  @retval EFI_NOT_FOUND   No matching NIC
  @retval Others          Error from LocateHandleBuffer
**/
EFI_STATUS
FindNicByMac (
  IN  EFI_MAC_ADDRESS  *MacAddr,
  OUT EFI_HANDLE       *NicHandle
  );

/**
  Check if MAC address is all zeros.

  @param[in]  MacAddr      MAC address to check

  @retval TRUE   MAC address is all zeros
  @retval FALSE  MAC address has at least one non-zero byte
**/
BOOLEAN
IsMacAllZeros (
  IN CONST EFI_MAC_ADDRESS  *MacAddr
  );

/**
  Build HTTP boot device path.

  @param[in]  NicHandle   NIC handle
  @param[in]  Uri         HTTP/HTTPS URI
  @param[out] DevicePath  Constructed device path

  @retval EFI_SUCCESS     Device path built
  @retval EFI_NOT_FOUND   NIC device path not found
  @retval EFI_OUT_OF_RESOURCES Memory allocation failed
  @retval EFI_INVALID_PARAMETER Invalid URI format
**/
EFI_STATUS
BuildHttpBootDevicePath (
  IN  EFI_HANDLE                NicHandle,
  IN  CONST CHAR16              *Uri,
  OUT EFI_DEVICE_PATH_PROTOCOL  **DevicePath
  );

/**
  Check if a URI contains an IPv6 address.

  IPv6 addresses in URIs are enclosed in square brackets per RFC 3986.
  Examples:
    - http://[2001:db8::1]/file -> TRUE
    - https://[::1]:8080/path -> TRUE
    - http://192.168.1.1/file -> FALSE
    - http://example.com/file -> FALSE

  @param[in]  AsciiUri     ASCII URI string to check

  @retval TRUE   URI contains IPv6 address (has '[' character)
  @retval FALSE  URI does not contain IPv6 address
**/
BOOLEAN
IsUriIPv6 (
  IN CONST CHAR8  *AsciiUri
  );

/**
  Parse HTTP_BOOT_URI variable format.

  Supports three formats:
  1. MAC||URI - Specific MAC, persistent
  2. MAC|once|URI - Specific MAC, one-time
  3. URI - First NIC, persistent

  MAC format: XX:XX:XX:XX:XX:XX (colon-separated hex)

  @param[in]  UriString    Input string to parse
  @param[out] MacAddr      Parsed MAC address (all zeros if not specified = use first NIC)
  @param[out] Once         TRUE if "once" flag present
  @param[out] Uri          Pointer to URI portion (points into UriString)

  @retval EFI_SUCCESS      Parsing successful
  @retval EFI_INVALID_PARAMETER Invalid format
**/
EFI_STATUS
ParseHttpBootUri (
  IN  CHAR16           *UriString,
  OUT EFI_MAC_ADDRESS  *MacAddr,
  OUT BOOLEAN          *Once,
  OUT CHAR16           **Uri
  );

//
// Redfish HTTP Boot option number
// Derived from NVIDIA_HTTP_BOOT_CONFIG_GUID first word (0x8c7d9a1e)
// This provides uniqueness and avoids collisions with other boot options
//
#define REDFISH_HTTP_BOOT_OPTION_NUM  0x8C7D

/**
  Create HTTP boot option with Redfish signature.

  @param[in]  MacAddr      MAC address of NIC (all zeros = use first NIC)
  @param[in]  Uri          HTTP/HTTPS URI
  @param[out] OptionNum    Created boot option number

  @retval EFI_SUCCESS      Boot option created
  @retval EFI_NOT_FOUND    NIC not found
  @retval EFI_OUT_OF_RESOURCES Memory allocation failed
  @retval EFI_UNSUPPORTED  Protocol not supported
  @retval Others           Error from SetVariable
**/
EFI_STATUS
CreateHttpBootOption (
  IN  EFI_MAC_ADDRESS  *MacAddr,
  IN  CONST CHAR16     *Uri,
  OUT UINT16           *OptionNum
  );

/**
  Compare HttpBootUri variable with Boot8C7D and sync as needed.

  Multi-boot cleanup logic:
  - Boot N (creation): HII RouteConfig creates boot option and sets BootNext
  - Boot N+1 (wait): BdsDxe reads BootNext and boots from Redfish boot option
  - Boot N+2 (cleanup): This function deletes one-time boot option and variable

  Handles three cases:
  1. Variable doesn't exist → delete boot option if present
  2. Variable exists but boot option doesn't → create boot option
  3. Variable exists with "once" flag → delete after HTTP boot completes

  @retval EFI_SUCCESS      Sync completed successfully
  @retval EFI_INVALID_PARAMETER Invalid URI format
  @retval Others           Error from variable services
**/
EFI_STATUS
CompareAndSyncBootOptions (
  VOID
  );

#ifdef __cplusplus
}
#endif

#endif // REDFISH_HTTP_BOOT_CONFIG_UTILS_H_
