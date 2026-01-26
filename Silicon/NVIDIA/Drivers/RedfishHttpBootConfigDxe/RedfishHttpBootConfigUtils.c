/** @file
  Utility functions for Redfish HTTP Boot Configuration.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "RedfishHttpBootConfigUtils.h"
#include <Library/NetLib.h>

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
  )
{
  EFI_STATUS                   Status;
  UINTN                        HandleCount;
  EFI_HANDLE                   *Handles;
  UINTN                        Index;
  EFI_SIMPLE_NETWORK_PROTOCOL  *Snp;

  if ((MacAddr == NULL) || (NicHandle == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleNetworkProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiSimpleNetworkProtocolGuid,
                    (VOID **)&Snp
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (CompareMem (MacAddr, &Snp->Mode->CurrentAddress, NET_ETHER_ADDR_LEN) == 0) {
      *NicHandle = Handles[Index];
      FreePool (Handles);
      return EFI_SUCCESS;
    }
  }

  FreePool (Handles);
  return EFI_NOT_FOUND;
}

/**
  Check if MAC address is all zeros.

  @param[in]  MacAddr      MAC address to check

  @retval TRUE   MAC address is all zeros
  @retval FALSE  MAC address has at least one non-zero byte
**/
BOOLEAN
IsMacAllZeros (
  IN CONST EFI_MAC_ADDRESS  *MacAddr
  )
{
  UINTN  Index;

  if (MacAddr == NULL) {
    return FALSE;
  }

  for (Index = 0; Index < 6; Index++) {
    if (MacAddr->Addr[Index] != 0) {
      return FALSE;
    }
  }

  return TRUE;
}

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
  )
{
  if (AsciiUri == NULL) {
    return FALSE;
  }

  return (AsciiStrStr (AsciiUri, "[") != NULL);
}

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
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *NicDevicePath;
  CHAR8                     *AsciiUri;
  UINTN                     UriLength;
  UINTN                     UriNodeSize;
  URI_DEVICE_PATH           *UriNode;
  IPv4_DEVICE_PATH          Ipv4Node;
  IPv6_DEVICE_PATH          Ipv6Node;
  EFI_DEVICE_PATH_PROTOCOL  *TempPath;
  EFI_DEVICE_PATH_PROTOCOL  *FinalPath;
  BOOLEAN                   UseIPv6;

  // Get base device path from NIC
  NicDevicePath = DevicePathFromHandle (NicHandle);
  if (NicDevicePath == NULL) {
    return EFI_NOT_FOUND;
  }

  // Convert URI to ASCII
  UriLength = StrLen (Uri);
  AsciiUri  = AllocateZeroPool (UriLength + 1);
  if (AsciiUri == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = UnicodeStrToAsciiStrS (Uri, AsciiUri, UriLength + 1);
  if (EFI_ERROR (Status)) {
    FreePool (AsciiUri);
    return EFI_INVALID_PARAMETER;
  }

  // Determine IPv4 vs IPv6 from URI
  UseIPv6 = IsUriIPv6 (AsciiUri);

  // Build IP node
  if (UseIPv6) {
    ZeroMem (&Ipv6Node, sizeof (Ipv6Node));
    Ipv6Node.Header.Type    = MESSAGING_DEVICE_PATH;
    Ipv6Node.Header.SubType = MSG_IPv6_DP;
    SetDevicePathNodeLength (&Ipv6Node, sizeof (Ipv6Node));
    Ipv6Node.IpAddressOrigin = 0x02;  // Use DHCP
    Ipv6Node.LocalPort       = 0;
    Ipv6Node.RemotePort      = 0;
    Ipv6Node.Protocol        = EFI_IP_PROTO_TCP;
    TempPath                 = AppendDevicePathNode (NicDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&Ipv6Node);
  } else {
    ZeroMem (&Ipv4Node, sizeof (Ipv4Node));
    Ipv4Node.Header.Type    = MESSAGING_DEVICE_PATH;
    Ipv4Node.Header.SubType = MSG_IPv4_DP;
    SetDevicePathNodeLength (&Ipv4Node, sizeof (Ipv4Node));
    Ipv4Node.StaticIpAddress = FALSE;  // Use DHCP
    Ipv4Node.LocalPort       = 0;
    Ipv4Node.RemotePort      = 0;
    Ipv4Node.Protocol        = EFI_IP_PROTO_TCP;
    TempPath                 = AppendDevicePathNode (NicDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&Ipv4Node);
  }

  if (TempPath == NULL) {
    FreePool (AsciiUri);
    return EFI_OUT_OF_RESOURCES;
  }

  // Build URI node
  UriNodeSize = sizeof (EFI_DEVICE_PATH_PROTOCOL) +
                AsciiStrLen (AsciiUri) + 1;
  UriNode = AllocateZeroPool (UriNodeSize);
  if (UriNode == NULL) {
    FreePool (AsciiUri);
    FreePool (TempPath);
    return EFI_OUT_OF_RESOURCES;
  }

  UriNode->Header.Type    = MESSAGING_DEVICE_PATH;
  UriNode->Header.SubType = MSG_URI_DP;
  SetDevicePathNodeLength (&UriNode->Header, (UINT16)UriNodeSize);
  AsciiStrCpyS (
    (CHAR8 *)UriNode + sizeof (EFI_DEVICE_PATH_PROTOCOL),
    UriNodeSize - sizeof (EFI_DEVICE_PATH_PROTOCOL),
    AsciiUri
    );

  // Append URI node
  FinalPath = AppendDevicePathNode (TempPath, (EFI_DEVICE_PATH_PROTOCOL *)UriNode);

  FreePool (AsciiUri);
  FreePool (UriNode);
  FreePool (TempPath);

  if (FinalPath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *DevicePath = FinalPath;
  return EFI_SUCCESS;
}

/**
  Check if character is a valid hexadecimal digit.

  @param[in]  Char  Character to check

  @retval TRUE   Character is hex digit (0-9, A-F, a-f)
  @retval FALSE  Character is not hex digit

**/
STATIC
BOOLEAN
IsHexDigit (
  IN CHAR16  Char
  )
{
  return (((Char >= L'0') && (Char <= L'9')) ||
          ((Char >= L'A') && (Char <= L'F')) ||
          ((Char >= L'a') && (Char <= L'f')));
}

/**
  Parse HTTP_BOOT_URI variable format.

  Supports five formats:
  1. MAC||URI - Specific MAC, persistent
  2. MAC|once|URI - Specific MAC, one-time
  3. URI - First NIC with link up, persistent
  4. |once|URI - First NIC with link up, one-time
  5. ||URI - Same as "URI" format.

  MAC format: XX:XX:XX:XX:XX:XX (colon-separated hex)

  A MAC of all-zeros will use the first NIC with link up.  Omitted MAC implies
  all-zeros.

  @param[in]  UriString    Input string to parse
  @param[out] MacAddr      Parsed MAC address (all zeros if not specified = use first NIC with link up)
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
  )
{
  CHAR16  *Ptr;
  CHAR16  *NextDelim;
  UINTN   Index;
  CHAR16  ByteStr[3];

  if ((UriString == NULL) || (MacAddr == NULL) || (Once == NULL) || (Uri == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // Initialize outputs
  ZeroMem (MacAddr, sizeof (EFI_MAC_ADDRESS));
  *Once = FALSE;
  *Uri  = NULL;

  Ptr = UriString;

  //
  // Check if string starts with '|' (implied all-zeros MAC)
  // Format: |once|URI or ||URI
  //
  if (Ptr[0] == L'|') {
    //
    // MAC is implied as all zeros (first NIC with link up)
    // MacAddr is already initialized to all zeros
    //
    DEBUG ((DEBUG_INFO, "%a: Implied all-zeros MAC (first NIC)\n", __func__));
    Ptr++;  // Skip leading pipe

    //
    // Parse middle field: |once|URI or ||URI
    //
    if (Ptr[0] == L'|') {
      // Double pipe (||URI) - no middle field, skip second pipe
      Ptr++;  // Skip second '|'
    } else {
      // Single pipe (|once|URI) - check for "once" keyword and look for second pipe
      NextDelim = StrStr (Ptr, L"|");
      if (NextDelim != NULL) {
        // There's a second pipe - check if middle field is "once"
        if (StrnCmp (Ptr, L"once", 4) == 0) {
          *Once = TRUE;
          DEBUG ((DEBUG_INFO, "%a: One-time boot option requested\n", __func__));
        }

        // Skip to after second pipe (regardless of middle field content)
        Ptr = NextDelim + 1;
      } else {
        // No second pipe - treat everything after first pipe as URI
        // Ptr already points to start of URI
      }
    }
  } else if (StrStr (Ptr, L"|") != NULL) {
    //
    // Parse MAC address (format: XX:XX:XX:XX:XX:XX)
    // Validate format strictly to prevent silent misconfigurations
    //
    ByteStr[2] = L'\0';
    for (Index = 0; Index < 6; Index++) {
      if ((Ptr[0] == L'\0') || (Ptr[1] == L'\0')) {
        DEBUG ((DEBUG_ERROR, "%a: MAC address too short\n", __func__));
        return EFI_INVALID_PARAMETER;
      }

      //
      // Validate both characters are hex digits
      //
      if (!IsHexDigit (Ptr[0]) || !IsHexDigit (Ptr[1])) {
        DEBUG ((DEBUG_ERROR, "%a: Invalid hex digit in MAC address at byte %u\n", __func__, Index));
        return EFI_INVALID_PARAMETER;
      }

      ByteStr[0]           = Ptr[0];
      ByteStr[1]           = Ptr[1];
      MacAddr->Addr[Index] = (UINT8)StrHexToUintn (ByteStr);

      Ptr += 2;

      //
      // Enforce colon separator (except after last byte)
      //
      if (Index < 5) {
        if (Ptr[0] != L':') {
          DEBUG ((DEBUG_ERROR, "%a: Missing colon separator in MAC address at position %u\n", __func__, Index));
          return EFI_INVALID_PARAMETER;
        }

        Ptr++;  // Skip colon
      }
    }

    DEBUG ((
      DEBUG_ERROR,
      "%a: Parsed MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
      __func__,
      MacAddr->Addr[0],
      MacAddr->Addr[1],
      MacAddr->Addr[2],
      MacAddr->Addr[3],
      MacAddr->Addr[4],
      MacAddr->Addr[5]
      ));

    // Expect '|' after MAC
    if (Ptr[0] != L'|') {
      DEBUG ((DEBUG_ERROR, "%a: Expected '|' after MAC address\n", __func__));
      return EFI_INVALID_PARAMETER;
    }

    Ptr++;  // Skip first '|'

    //
    // Check for double pipe (MAC||URI) or middle field (MAC|once|URI)
    //
    if (Ptr[0] == L'|') {
      // Double pipe - no middle field, skip second pipe
      Ptr++;  // Skip second '|'
    } else {
      // Single pipe - check for "once" keyword and look for second pipe
      NextDelim = StrStr (Ptr, L"|");
      if (NextDelim != NULL) {
        // There's a second pipe - check if middle field is "once"
        if (StrnCmp (Ptr, L"once", 4) == 0) {
          *Once = TRUE;
          DEBUG ((DEBUG_ERROR, "%a: One-time boot option requested\n", __func__));
        }

        // Skip to after second pipe (regardless of middle field content)
        Ptr = NextDelim + 1;
      }

      // else: Single pipe with no second pipe (MAC|URI format)
      // Ptr already points to start of URI
    }
  }

  // Rest of string is the URI
  *Uri = Ptr;

  if (StrLen (*Uri) == 0) {
    DEBUG ((DEBUG_ERROR, "%a: URI is empty\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_ERROR, "%a: Parsed URI: %s\n", __func__, *Uri));

  return EFI_SUCCESS;
}
