/** @file
  This driver will remove SMBIOS type 42 record if "Redfish Host Interface" setup
  menu is set to "Disabled".

  This driver listen to Redfish after-provision event and remove SMBIOS type 42
  record so tht OS can not use it to talk to Redfish service.

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "RedfishHostInterfaceControlDxe.h"

EFI_EVENT  mEvent = NULL;

/**
  Callback function executed when the after-provisioning event group is signaled.
  @param[in]   Event    Event whose notification function is being invoked.
  @param[out]  Context  Pointer to the Context buffer
**/
VOID
EFIAPI
RedfishAfterProvisioning (
  IN  EFI_EVENT  Event,
  OUT VOID       *Context
  )
{
  EFI_STATUS               Status;
  EFI_SMBIOS_PROTOCOL      *Smbios;
  EFI_SMBIOS_HANDLE        SmbiosHandle;
  EFI_SMBIOS_TABLE_HEADER  *Record;

  Status = gBS->LocateProtocol (
                  &gEfiSmbiosProtocolGuid,
                  NULL,
                  (VOID **)&Smbios
                  );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  //
  // Look for type 42 record in SMBIOS table
  //
  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status       = Smbios->GetNext (Smbios, &SmbiosHandle, NULL, &Record, NULL);
  while (!EFI_ERROR (Status) && SmbiosHandle != SMBIOS_HANDLE_PI_RESERVED) {
    if (Record->Type == SMBIOS_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE) {
      break;
    }

    Status = Smbios->GetNext (Smbios, &SmbiosHandle, NULL, &Record, NULL);
  }

  if (SmbiosHandle == SMBIOS_HANDLE_PI_RESERVED) {
    DEBUG ((DEBUG_WARN, "%a: no SMBIOS type 42 record is found\n", __FUNCTION__));
    goto ON_EXIT;
  }

  //
  // Remove the type 42 record
  //
  Status = Smbios->Remove (Smbios, SmbiosHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to remove SMBIOS type 42 record\n", __FUNCTION__));
    goto ON_EXIT;
  }

  DEBUG ((DEBUG_INFO, "%a: SMBIOS type 42 record is removed\n", __FUNCTION__));

ON_EXIT:

  //
  // Close event
  //
  gBS->CloseEvent (Event);
  mEvent = NULL;
}

/**
  Unloads an image.

  @param  ImageHandle           Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
  @retval EFI_INVALID_PARAMETER ImageHandle is not a valid image handle.

**/
EFI_STATUS
EFIAPI
RedfishHostInterfaceControlUnload (
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
  The entry point for platform redfish BIOS driver which installs the Redfish Resource
  Addendum protocol on its ImageHandle.

  @param[in]  ImageHandle        The image handle of the driver.
  @param[in]  SystemTable        The system table.

  @retval EFI_SUCCESS            Protocol install successfully.
  @retval Others                 Failed to install the protocol.

**/
EFI_STATUS
EFIAPI
RedfishHostInterfaceControlEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  if (PcdGet8 (PcdRedfishHostInterface) == REDFISH_HOST_INTERFACE_DISABLE) {
    DEBUG ((DEBUG_INFO, "%a: Redfish Host Interface is set to disabled. Remove SMBIOS type 42 record\n", __FUNCTION__));
    //
    // Register after-provisioning event
    //
    Status = CreateAfterProvisioningEvent (
               RedfishAfterProvisioning,
               NULL,
               &mEvent
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: failed to register after-provisioning event: %r\n", __FUNCTION__, Status));
    }
  }

  return EFI_SUCCESS;
}
