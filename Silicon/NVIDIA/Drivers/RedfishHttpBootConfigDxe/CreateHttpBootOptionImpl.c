/** @file
  Implementation of CreateHttpBootOption function.
  Separated from main driver to enable unit testing.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "RedfishHttpBootConfigUtils.h"

/**
  Create HTTP boot option with Redfish signature.

  @param[in]  MacAddr      MAC address of NIC (all zeros = use first NIC with link up)
  @param[in]  Uri          HTTP/HTTPS URI
  @param[out] OptionNum    Created boot option number

  When MacAddr is all zeros, selects the first NIC with link up (MediaPresent).
  If no NICs have link up, falls back to the first NIC in enumeration order.

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
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE                NicHandle;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  CHAR16                    Description[128];
  UINT8                     *LoadOptionPtr;
  UINTN                     LoadOptionSize;
  UINTN                     DescriptionSize;
  UINTN                     DevicePathSize;
  BOOLEAN                   IsAllNics;

  // Check if MAC is all zeros (use first available NIC)
  IsAllNics = IsMacAllZeros (MacAddr);

  if (IsAllNics) {
    // For unspecified MAC, use first NIC with link up
    UINTN                        HandleCount;
    EFI_HANDLE                   *Handles;
    EFI_SIMPLE_NETWORK_PROTOCOL  *Snp;
    UINTN                        Index;
    BOOLEAN                      FoundNicWithLink;

    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEfiSimpleNetworkProtocolGuid,
                    NULL,
                    &HandleCount,
                    &Handles
                    );
    if (EFI_ERROR (Status) || (HandleCount == 0)) {
      DEBUG ((DEBUG_ERROR, "%a: No NICs found\n", __func__));
      return EFI_NOT_FOUND;
    }

    //
    // Find first NIC with link up
    //
    FoundNicWithLink = FALSE;
    NicHandle        = NULL;

    for (Index = 0; Index < HandleCount; Index++) {
      Status = gBS->HandleProtocol (
                      Handles[Index],
                      &gEfiSimpleNetworkProtocolGuid,
                      (VOID **)&Snp
                      );
      if (EFI_ERROR (Status)) {
        continue;
      }

      //
      // Check if media is present (link up)
      //
      if (Snp->Mode->MediaPresent) {
        NicHandle        = Handles[Index];
        FoundNicWithLink = TRUE;

        DEBUG ((
          DEBUG_INFO,
          "%a: Found NIC with link up: %02X:%02X:%02X:%02X:%02X:%02X\n",
          __func__,
          Snp->Mode->CurrentAddress.Addr[0],
          Snp->Mode->CurrentAddress.Addr[1],
          Snp->Mode->CurrentAddress.Addr[2],
          Snp->Mode->CurrentAddress.Addr[3],
          Snp->Mode->CurrentAddress.Addr[4],
          Snp->Mode->CurrentAddress.Addr[5]
          ));
        break;
      }
    }

    //
    // If no NICs have link up, fall back to first NIC
    //
    if (!FoundNicWithLink) {
      DEBUG ((DEBUG_WARN, "%a: No NICs with link up found, using first NIC\n", __func__));
      NicHandle = Handles[0];

      Status = gBS->HandleProtocol (
                      NicHandle,
                      &gEfiSimpleNetworkProtocolGuid,
                      (VOID **)&Snp
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to get SNP from first NIC: %r\n", __func__, Status));
        FreePool (Handles);
        return Status;
      }

      DEBUG ((
        DEBUG_INFO,
        "%a: Using first NIC (no link): %02X:%02X:%02X:%02X:%02X:%02X\n",
        __func__,
        Snp->Mode->CurrentAddress.Addr[0],
        Snp->Mode->CurrentAddress.Addr[1],
        Snp->Mode->CurrentAddress.Addr[2],
        Snp->Mode->CurrentAddress.Addr[3],
        Snp->Mode->CurrentAddress.Addr[4],
        Snp->Mode->CurrentAddress.Addr[5]
        ));
    }

    FreePool (Handles);

    if (NicHandle == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to select NIC\n", __func__));
      return EFI_NOT_FOUND;
    }
  } else {
    // Find specific NIC by MAC
    Status = FindNicByMac (MacAddr, &NicHandle);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: NIC not found for MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
        __func__,
        MacAddr->Addr[0],
        MacAddr->Addr[1],
        MacAddr->Addr[2],
        MacAddr->Addr[3],
        MacAddr->Addr[4],
        MacAddr->Addr[5]
        ));
      return Status;
    }
  }

  // Build device path
  Status = BuildHttpBootDevicePath (NicHandle, Uri, &DevicePath);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to build device path: %r\n", __func__, Status));
    return Status;
  }

  // Create boot option description
  if (IsAllNics) {
    UnicodeSPrint (
      Description,
      sizeof (Description),
      L"Redfish HTTP Boot (First NIC): %s",
      Uri
      );
  } else {
    UnicodeSPrint (
      Description,
      sizeof (Description),
      L"Redfish HTTP Boot: %s",
      Uri
      );
  }

  //
  // Construct EFI_LOAD_OPTION
  //
  DescriptionSize = (StrLen (Description) + 1) * sizeof (CHAR16);
  DevicePathSize  = GetDevicePathSize (DevicePath);
  LoadOptionSize  = sizeof (UINT32) + sizeof (UINT16) + DescriptionSize + DevicePathSize;

  LoadOptionPtr = AllocateZeroPool (LoadOptionSize);
  if (LoadOptionPtr == NULL) {
    FreePool (DevicePath);
    return EFI_OUT_OF_RESOURCES;
  }

  // Write Attributes
  *(UINT32 *)LoadOptionPtr = LOAD_OPTION_ACTIVE;

  // Write FilePathListLength
  *(UINT16 *)(LoadOptionPtr + sizeof (UINT32)) = (UINT16)DevicePathSize;

  // Write Description
  CopyMem (
    LoadOptionPtr + sizeof (UINT32) + sizeof (UINT16),
    Description,
    DescriptionSize
    );

  // Write FilePath
  CopyMem (
    LoadOptionPtr + sizeof (UINT32) + sizeof (UINT16) + DescriptionSize,
    DevicePath,
    DevicePathSize
    );

  //
  // Use a fixed boot option number for Redfish HTTP boot
  // Number is derived from NVIDIA_HTTP_BOOT_CONFIG_GUID to ensure uniqueness
  //
  UINT16  RedfishBootNum = REDFISH_HTTP_BOOT_OPTION_NUM;
  CHAR16  BootVarName[16];

  UnicodeSPrint (BootVarName, sizeof (BootVarName), L"Boot%04x", RedfishBootNum);

  //
  // Create or overwrite the boot option
  //
  Status = gRT->SetVariable (
                  BootVarName,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  LoadOptionSize,
                  LoadOptionPtr
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create %s: %r\n", __func__, BootVarName, Status));
    FreePool (LoadOptionPtr);
    FreePool (DevicePath);
    return Status;
  }

  *OptionNum = RedfishBootNum;
  DEBUG ((DEBUG_INFO, "%a: Created %s with description: %s\n", __func__, BootVarName, Description));

  FreePool (LoadOptionPtr);
  FreePool (DevicePath);

  //
  // Set BootNext to our boot option
  // This makes the firmware boot from this option on the next boot only
  // BootNext is automatically cleared after being used
  //
  Status = gRT->SetVariable (
                  L"BootNext",
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  sizeof (UINT16),
                  &RedfishBootNum
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set BootNext: %r\n", __func__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Set BootNext to Boot%04x\n", __func__, RedfishBootNum));

  return EFI_SUCCESS;
}
