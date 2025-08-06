/** @file
  This driver is tracking IPv4 and IPv6 config variables. If the USB NIC MAC is changing, this driver
  removes stale IPv4 and IPv6 config variables.

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UsbLanJanitorDxe.h"

EFI_EVENT  mEvent = NULL;

/**
  Get the MAC address of NIC.

  @param[out] MacAddress      Pointer to retrieve MAC address

  @retval   EFI_SUCCESS      MAC address is returned in MacAddress

**/
EFI_STATUS
GetUsbLanMacAddress (
  OUT EFI_MAC_ADDRESS  *MacAddress
  )
{
  EFI_STATUS                    Status;
  NVIDIA_USB_NIC_INFO_PROTOCOL  *UsbNicInfo;

  if (MacAddress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateProtocol (
                  &gNVIDIAUsbNicInfoProtocolGuid,
                  NULL,
                  (VOID **)&UsbNicInfo
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UsbNicInfo->GetMacAddress (
                         UsbNicInfo,
                         MacAddress
                         );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, failed to get MAC address: %r\n", __func__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Save the MAC address to variable for next boot.

  @param[in] MacAddressString     MAC address string to save.

  @retval   EFI_SUCCESS      MAC address is saved in variable.
  @retval   Others           Error occurs.

**/
EFI_STATUS
SaveUsbLanMacAddress (
  IN CHAR16  *MacAddressString
  )
{
  EFI_STATUS  Status;
  UINTN       DataSize;

  if (MacAddressString == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DataSize = StrSize (MacAddressString);
  if (DataSize == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gRT->SetVariable (
                  USB_LAN_JANITOR_VARIABLE,
                  &gNVIDIATokenSpaceGuid,
                  USB_LAN_JANITOR_VAR_ATTR,
                  DataSize,
                  (VOID *)MacAddressString
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to save variable %s: %r\n", __func__, USB_LAN_JANITOR_VARIABLE, Status));
    return Status;
  }

  DEBUG ((USB_LAN_JANITOR_DEBUG, "%a: save %s to %s\n", __func__, MacAddressString, USB_LAN_JANITOR_VARIABLE));

  return EFI_SUCCESS;
}

/**
  Delete the UEFI variable. This driver checks to see if variable
  exists or not before deleting it.

  @param[in] VarName         Variable Name.
  @param[in] VarGuid         Variable GUID.

  @retval   EFI_SUCCESS      Variable is deleted.
  @retval   Others           Error occurs.

**/
EFI_STATUS
DeleteVariable (
  IN CHAR16    *VarName,
  IN EFI_GUID  *VarGuid
  )
{
  EFI_STATUS  Status;
  UINT32      Attribute;
  UINTN       DataSize;

  if ((VarName == NULL) || (VarGuid == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DataSize  = 0;
  Attribute = 0;
  Status    = gRT->GetVariable (
                     VarName,
                     VarGuid,
                     &Attribute,
                     &DataSize,
                     NULL
                     );
  if (Status == EFI_BUFFER_TOO_SMALL) {
    Status = gRT->SetVariable (
                    VarName,
                    VarGuid,
                    Attribute,
                    0,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: cannot remove variable: %g %s: %r\n", __func__, VarGuid, VarName, Status));
    }
  }

  return EFI_SUCCESS;
}

/**
  Remove IPv4 and IPv6 config variables.

  @param[in] MacAddressString   MAC address string.

  @retval   EFI_SUCCESS      Variables are deleted.
  @retval   Others           Error occurs.

**/
EFI_STATUS
RemoveStaleIpConfigVariables (
  IN CHAR16  *MacAddressString
  )
{
  EFI_STATUS  Status;

  if (MacAddressString == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Delete IPv4 config variable
  //
  Status = DeleteVariable (MacAddressString, &gEfiIp4Config2ProtocolGuid);
  if (!EFI_ERROR (Status)) {
    DEBUG ((USB_LAN_JANITOR_DEBUG, "%a: IP4 config2 variable %s is deleted\n", __func__, MacAddressString));
  }

  //
  // Delete IPv6 config variable
  //
  Status = DeleteVariable (MacAddressString, &gEfiIp6ConfigProtocolGuid);
  if (!EFI_ERROR (Status)) {
    DEBUG ((USB_LAN_JANITOR_DEBUG, "%a: IP6 config variable %s is deleted\n", __func__, MacAddressString));
  }

  return EFI_SUCCESS;
}

/**
  Callback function when gNVIDIAUsbNicInfoProtocolGuid is installed.

  @param[in] Event    Event whose notification function is being invoked.
  @param[in] Context  Pointer to the notification function's context.
**/
VOID
EFIAPI
BmcUsbNicProtocolIsReady (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS       Status;
  EFI_MAC_ADDRESS  UsbLanMacAddress;
  CHAR16           MacAddrString[MAX_ADDR_STR_LEN];
  CHAR16           *LastMacAddrString;

  LastMacAddrString = NULL;
  ZeroMem (&UsbLanMacAddress, sizeof (EFI_MAC_ADDRESS));

  //
  // Find current MAC address
  //
  Status = GetUsbLanMacAddress (&UsbLanMacAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((USB_LAN_JANITOR_DEBUG, "%a: cannot get USB LAN MAC: %r\n", __func__, Status));
    return;
  }

  gBS->CloseEvent (mEvent);
  mEvent = NULL;

  UnicodeSPrint (
    MacAddrString,
    MAX_ADDR_STR_SIZE,
    L"%02X%02X%02X%02X%02X%02X",
    UsbLanMacAddress.Addr[0],
    UsbLanMacAddress.Addr[1],
    UsbLanMacAddress.Addr[2],
    UsbLanMacAddress.Addr[3],
    UsbLanMacAddress.Addr[4],
    UsbLanMacAddress.Addr[5]
    );

  DEBUG ((USB_LAN_JANITOR_DEBUG, "%a: USB LAN MAC: %s\n", __func__, MacAddrString));

  //
  // Look for MAC address from last boot
  //
  Status = GetVariable2 (
             USB_LAN_JANITOR_VARIABLE,
             &gNVIDIATokenSpaceGuid,
             (VOID **)&LastMacAddrString,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((USB_LAN_JANITOR_DEBUG, "%a: cannot find MAC address from last boot: %r\n", __func__, Status));
    SaveUsbLanMacAddress (MacAddrString);
    return;
  }

  //
  // Check to see if MAC address is changed or not
  //
  if (StrCmp (LastMacAddrString, MacAddrString) == 0) {
    DEBUG ((USB_LAN_JANITOR_DEBUG, "%a: MAC address is not changed\n", __func__));
    goto ON_RELEASE;
  }

  //
  // Delete IPv4 config and IPv6 config variables when MAC is changed.
  //
  DEBUG ((USB_LAN_JANITOR_DEBUG, "%a: MAC address is changed. Old: %s Current: %s\n", __func__, LastMacAddrString, MacAddrString));
  Status = RemoveStaleIpConfigVariables (LastMacAddrString);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot remove stale IP config variables: %r\n", __func__, Status));
  }

  //
  // Keep MAC address for next boot.
  //
  Status = SaveUsbLanMacAddress (MacAddrString);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot keep USB LAN MAC address: %r\n", __func__, Status));
  }

ON_RELEASE:

  if (LastMacAddrString != NULL) {
    FreePool (LastMacAddrString);
  }
}

/**
  Unloads an image.

  @param  ImageHandle           Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
  @retval EFI_INVALID_PARAMETER ImageHandle is not a valid image handle.

**/
EFI_STATUS
EFIAPI
UsbLanJanitorUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  if (mEvent != NULL) {
    gBS->CloseEvent (mEvent);
    mEvent = NULL;
  }

  return EFI_SUCCESS;
}

/**
  The entry point for USB LAN janitor driver.

  @param[in]  ImageHandle        The image handle of the driver.
  @param[in]  SystemTable        The system table.

  @retval EFI_SUCCESS            Protocol install successfully.
  @retval Others                 Failed to install the protocol.

**/
EFI_STATUS
EFIAPI
UsbLanJanitorEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  VOID  *Registration;

  mEvent =   EfiCreateProtocolNotifyEvent (
               &gNVIDIAUsbNicInfoProtocolGuid,
               TPL_CALLBACK,
               BmcUsbNicProtocolIsReady,
               NULL,
               &Registration
               );
  if (mEvent == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: cannot create protocol notify event\n", __func__));
  }

  return EFI_SUCCESS;
}
