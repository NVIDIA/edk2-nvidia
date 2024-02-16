/** @file
  Implement the driver binding protocol for USB RNDIS driver

  Copyright (c) 2011, Intel Corporation. All rights reserved.
  Copyright (c) 2020, ARM Limited. All rights reserved
  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UsbRndisDxe.h"
#include "ComponentName.h"
#include "Rndis.h"
#include "Snp.h"
#include "Debug.h"

/**
  Initial RNDIS SNP service

  @param[in]      Controller      Controller handle

  @retval USB_RNDIS_PRIVATE_DATA  Pointer to newly created private data
  @retval NULL                    Error occurs

**/
USB_RNDIS_PRIVATE_DATA *
NewUsbRndisPrivate (
  IN  EFI_HANDLE  Controller
  )
{
  USB_RNDIS_PRIVATE_DATA  *NewBuf;
  EFI_STATUS              Status;

  NewBuf = AllocateZeroPool (sizeof (USB_RNDIS_PRIVATE_DATA));
  if (NewBuf == NULL) {
    DEBUG ((DEBUG_ERROR, "%a, out of resource\n", __FUNCTION__));
    return NULL;
  }

  NewBuf->Signature  = USB_RNDIS_PRIVATE_DATA_SIGNATURE;
  NewBuf->Controller = Controller;
  USB_RESET_REQUEST_ID (NewBuf->UsbData.RequestId);
  InitializeListHead (&NewBuf->UsbData.ReceiveQueue);

  //
  // receiver control timer.
  //
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL | EVT_TIMER,
                  TPL_NOTIFY,
                  RndisReceiveControlTimer,
                  NewBuf,
                  &NewBuf->ReceiverControlTimer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, failed to create event: %r\n", __FUNCTION__, Status));
  }

  return NewBuf;
}

/**
  Release private data and stop corresponding timer event.

  @param[in]      Private       Pointer to private data
  @param[in]      DriverBinding Driver binding handle

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
ReleaseUsbRndisPrivate (
  IN USB_RNDIS_PRIVATE_DATA       *Private,
  IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding
  )
{
  if ((Private == NULL) || (DriverBinding == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Private->ReceiverControlTimer != NULL) {
    gBS->SetTimer (Private->ReceiverControlTimer, TimerCancel, 0);
    gBS->CloseEvent (Private->ReceiverControlTimer);
    Private->ReceiverControlTimer = NULL;
  }

  if (Private->UsbIoProtocol != NULL) {
    gBS->CloseProtocol (
           Private->Controller,
           &gEfiUsbIoProtocolGuid,
           DriverBinding->DriverBindingHandle,
           Private->Controller
           );
    Private->UsbIoProtocol = NULL;
  }

  if (Private->UsbIoDataProtocol != NULL) {
    gBS->CloseProtocol (
           Private->ControllerData,
           &gEfiUsbIoProtocolGuid,
           DriverBinding->DriverBindingHandle,
           Private->ControllerData
           );
    gBS->CloseProtocol (
           Private->ControllerData,
           &gEfiUsbIoProtocolGuid,
           DriverBinding->DriverBindingHandle,
           Private->Handle
           );
    Private->UsbIoDataProtocol = NULL;
  }

  if (Private->DevicePathProtocol != NULL) {
    gBS->CloseProtocol (
           Private->Controller,
           &gEfiDevicePathProtocolGuid,
           DriverBinding->DriverBindingHandle,
           Private->Controller
           );
  }

  FREE_NON_NULL (Private->DevicePathProtocol);
  FreePool (Private);

  return EFI_SUCCESS;
}

/**
  Find private data from early populated handle

  @param[in]      Controller    Controller handle

  @retval USB_RNDIS_PRIVATE_DATA  Pointer to private data
  @retval NULL                    Error occurs

**/
USB_RNDIS_PRIVATE_DATA *
GetRndisPrivateData (
  IN  EFI_HANDLE  Controller
  )
{
  EFI_STATUS              Status;
  UINTN                   HandleCount;
  EFI_HANDLE              *Handles;
  UINTN                   Index;
  UINT32                  *Id;
  USB_RNDIS_PRIVATE_DATA  *PrivateData;

  if (Controller == NULL) {
    return NULL;
  }

  //
  // Retrieve the array of handles that support gEfiDevicePathProtocolGuid
  //
  HandleCount = 0;
  Handles     = NULL;
  Status      = gBS->LocateHandleBuffer (
                       ByProtocol,
                       &gEfiDevicePathProtocolGuid,
                       NULL,
                       &HandleCount,
                       &Handles
                       );
  if (EFI_ERROR (Status)) {
    if ((Status != EFI_BUFFER_TOO_SMALL) || (HandleCount == 0)) {
      return NULL;
    }

    Handles = AllocatePool (sizeof (EFI_HANDLE) * HandleCount);
    if (Handles == NULL) {
      return NULL;
    }

    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEfiDevicePathProtocolGuid,
                    NULL,
                    &HandleCount,
                    &Handles
                    );
    if (EFI_ERROR (Status)) {
      return NULL;
    }
  }

  for (Index = 0; Index < HandleCount; Index++) {
    //
    // Find private data from caller ID
    //
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiCallerIdGuid,
                    (VOID **)&Id
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    PrivateData = USB_RNDIS_PRIVATE_DATA_FROM_ID (Id);
    FreePool (Handles);
    return PrivateData;
  }

  FreePool (Handles);
  return NULL;
}

/**
  Verify the controller type

  @param [in] This                Protocol instance pointer.
  @param [in] Controller           Handle of device to test.
  @param [in] pRemainingDevicePath Not used.

  @retval EFI_SUCCESS          This driver supports this device.
  @retval other                This driver does not support this device.

**/
EFI_STATUS
EFIAPI
UsbRndisDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_USB_IO_PROTOCOL  *UsbIo;
  EFI_STATUS           Status;

  //
  //  Connect to the USB stack
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (!EFI_ERROR (Status)) {
    if (!(IsRndisInterface (UsbIo) || IsRndisDataInterface (UsbIo))) {
      Status = EFI_UNSUPPORTED;
    } else {
      if (IsRndisDataInterface (UsbIo) && (GetRndisPrivateData (Controller) == NULL)) {
        DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a, wait for control interface to be started first\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
      }
    }

    //
    //  Done with the USB stack
    //
    gBS->CloseProtocol (
           Controller,
           &gEfiUsbIoProtocolGuid,
           This->DriverBindingHandle,
           Controller
           );
  }

  //
  //  Return the device supported status
  //
  return Status;
}

/**
  Start this driver on Controller by opening UsbIo and DevicePath protocols.
  Initialize PXE structures, create a copy of the Controller Device Path with the
  NIC's MAC address apEnded to it, install the NetworkInterfaceIdentifier protocol
  on the newly created Device Path.

  @param [in] This                Protocol instance pointer.
  @param [in] Controller           Handle of device to work with.
  @param [in] pRemainingDevicePath Not used, always produce all possible children.

  @retval EFI_SUCCESS          This driver is added to Controller.
  @retval other                This driver does not support this device.

**/
EFI_STATUS
EFIAPI
UsbRndisDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                Status;
  USB_RNDIS_PRIVATE_DATA    *Private;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  MAC_ADDR_DEVICE_PATH      MacDeviceNode;
  EFI_USB_IO_PROTOCOL       *UsbIo;

  Private          = NULL;
  ParentDevicePath = NULL;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto OnError;
  }

  //
  // If this is data interface, attach it to original one.
  //
  if (IsRndisDataInterface (UsbIo)) {
    DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a, Controller Data: 0x%p\n", __FUNCTION__, Controller));
    Private = GetRndisPrivateData (Controller);
    if (Private == NULL) {
      return EFI_UNSUPPORTED;
    }

    Private->ControllerData    = Controller;
    Private->UsbIoDataProtocol = UsbIo;
    RndisConfigureUsbDevice (Private->UsbIoDataProtocol, &Private->UsbData.EndPoint);
    DEBUG ((USB_DEBUG_RNDIS, "%a Bulk-in: %x, Bulk-out: %x Interrupt: %x\n", __FUNCTION__, Private->UsbData.EndPoint.BulkIn, Private->UsbData.EndPoint.BulkOut, Private->UsbData.EndPoint.Interrupt));

    //
    // Everything is ready. Now expose SNP
    //
    ASSERT (Private->Handle != NULL);
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Private->Handle,
                    &gEfiSimpleNetworkProtocolGuid,
                    &Private->SnpProtocol,
                    &gNVIDIAUsbNicInfoProtocolGuid,
                    &Private->UsbNicInfoProtocol,
                    NULL
                    );
    if (!EFI_ERROR (Status)) {
      //
      // Open For Child Device
      //
      Status = gBS->OpenProtocol (
                      Controller,
                      &gEfiUsbIoProtocolGuid,
                      (VOID **)&UsbIo,
                      This->DriverBindingHandle,
                      Private->Handle,
                      EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: open protocol by child controller failed: %r\n", __FUNCTION__, Status));
      }
    } else {
      DEBUG ((DEBUG_ERROR, "%a: install SNP and corresponding protocols failed: %r\n", __FUNCTION__, Status));
    }

    DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a: Controller Data: 0x%p done, new handle: 0x%p\n", __FUNCTION__, Controller, Private->Handle));

    return Status;
  }

  DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a, Controller: 0x%p\n", __FUNCTION__, Controller));
  Private = NewUsbRndisPrivate (Controller);
  if (Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->DeviceLost    = FALSE;
  Private->UsbIoProtocol = UsbIo;

  //
  //  Initialize the simple network protocol
  //
  Status = UsbRndisInitialSnpService (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: UsbRndisInitialSnpService: %r\n", __FUNCTION__, Status));
    goto OnError;
  }

  //
  // Initialize USB NIC info protocol
  //
  Status = UsbRndisInitialUsbNicInfo (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: UsbRndisInitialUsbNicInfo: %r\n", __FUNCTION__, Status));
    goto OnError;
  }

  //
  // Set Device Path
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto OnError;
  }

  ZeroMem (&MacDeviceNode, sizeof (MAC_ADDR_DEVICE_PATH));
  MacDeviceNode.Header.Type    = MESSAGING_DEVICE_PATH;
  MacDeviceNode.Header.SubType = MSG_MAC_ADDR_DP;

  SetDevicePathNodeLength (&MacDeviceNode.Header, sizeof (MAC_ADDR_DEVICE_PATH));

  CopyMem (
    &MacDeviceNode.MacAddress,
    &Private->SnpModeData.CurrentAddress,
    NET_ETHER_ADDR_LEN
    );

  MacDeviceNode.IfType = Private->SnpModeData.IfType;

  Private->DevicePathProtocol = AppendDevicePathNode (
                                  ParentDevicePath,
                                  (EFI_DEVICE_PATH_PROTOCOL *)&MacDeviceNode
                                  );
  DEBUG_CODE_BEGIN ();
  CHAR16  *DevicePathStr;

  DevicePathStr = ConvertDevicePathToText (Private->DevicePathProtocol, TRUE, TRUE);
  if (DevicePathStr != NULL) {
    DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a, device path: %s\n", __FUNCTION__, DevicePathStr));
    FreePool (DevicePathStr);
  }

  DEBUG_CODE_END ();

  //
  //  Install both the caller id and device path protocols.
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gEfiCallerIdGuid,
                  &Private->Id,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, install caller id failed: %r\n", __FUNCTION__, Status));
    goto OnError;
  }

  DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a, install caller ID: %g\n", __FUNCTION__, &gEfiCallerIdGuid));

  Private->Handle = NULL;
  Status          = gBS->InstallMultipleProtocolInterfaces (
                           &Private->Handle,
                           &gEfiDevicePathProtocolGuid,
                           Private->DevicePathProtocol,
                           NULL
                           );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, install device path protocol failed: %r\n", __FUNCTION__, Status));
    gBS->UninstallProtocolInterface (
           Controller,
           &gEfiCallerIdGuid,
           &Private->Id
           );
    goto OnError;
  }

  DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a, Controller: 0x%p done\n", __FUNCTION__, Controller));

  return EFI_SUCCESS;

OnError:

  if (Private != NULL) {
    ReleaseUsbRndisPrivate (Private, This);
  }

  return Status;
}

/**
  Stop this driver on Controller by removing NetworkInterfaceIdentifier protocol and
  closing the DevicePath and PciIo protocols on Controller.

  @param [in] This                Protocol instance pointer.
  @param [in] Controller           Handle of device to stop driver on.
  @param [in] NumberOfChildren     How many children need to be stopped.
  @param [in] pChildHandleBuffer   Not used.

  @retval EFI_SUCCESS          This driver is removed Controller.
  @retval EFI_DEVICE_ERROR     The device could not be stopped due to a device error.
  @retval other                This driver was not removed from this device.

**/
EFI_STATUS
EFIAPI
UsbRndisDriverStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;
  EFI_STATUS              Status;
  UINT32                  *Id;

  ASSERT (NumberOfChildren == 0);

  DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a, Controller: 0x%p\n", __FUNCTION__, Controller));

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiCallerIdGuid,
                  (VOID **)&Id,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, no caller id found: %r\n", __FUNCTION__, Status));
    return EFI_SUCCESS;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_ID (Id);

  //
  // SNP protocol may not be uninstalled because MNP is using it.
  // Set this flag because we no long can communicate with USB NIC.
  //
  Private->DeviceLost = TRUE;
  DEBUG ((DEBUG_INFO, "%a, USB NIC lost!!\n", __FUNCTION__));

  //
  // Uninstall caller id.
  //
  DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a, uninstall caller id: %g\n", __FUNCTION__, &gEfiCallerIdGuid));
  Status = gBS->UninstallProtocolInterface (
                  Controller,
                  &gEfiCallerIdGuid,
                  &Private->Id
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, uninstall caller ID failed: %r\n", __FUNCTION__, Status));
  }

  //
  // Uninstall protocols
  //
  DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a, uninstall protocols\n", __FUNCTION__));
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Private->Handle,
                  &gEfiDevicePathProtocolGuid,
                  Private->DevicePathProtocol,
                  &gEfiSimpleNetworkProtocolGuid,
                  &Private->SnpProtocol,
                  &gNVIDIAUsbNicInfoProtocolGuid,
                  &Private->UsbNicInfoProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, uninstall protocols failed, MNP may still consume SNP: %r\n", __FUNCTION__, Status));
    return Status;
  }

  ReleaseUsbRndisPrivate (Private, This);

  DEBUG ((USB_DEBUG_DRIVER_BINDING, "%a, Controller: 0x%p done\n", __FUNCTION__, Controller));

  return EFI_SUCCESS;
}

/**
  Driver binding protocol declaration
**/
EFI_DRIVER_BINDING_PROTOCOL  gDriverBinding = {
  UsbRndisDriverSupported,
  UsbRndisDriverStart,
  UsbRndisDriverStop,
  USB_RNDIS_VERSION,
  NULL,
  NULL
};

/**
USB RNDIS driver entry point.

@param [in] ImageHandle       Handle for the image.
@param [in] SystemTable       Address of the system table.

@retval EFI_SUCCESS           Image successfully loaded.

**/
EFI_STATUS
EFIAPI
UsbRndisEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  //  Add the driver to the list of drivers
  //
  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gDriverBinding,
             ImageHandle,
             &gComponentName,
             &gComponentName2
             );

  return Status;
}
