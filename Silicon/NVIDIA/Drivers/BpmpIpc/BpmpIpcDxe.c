/** @file

  BPMP IPC Driver

  Copyright (c) 2018-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <libfdt.h>

#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include <Protocol/BpmpIpc.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include "BpmpIpcDxePrivate.h"

/**
  This is function is caused to allow the system to check if this implementation supports
  the device tree node. If EFI_SUCCESS is returned then handle will be created and driver binding
  will occur.

  @param[in]  This                   The instance of the NVIDIA_DEVICE_TREE_BINDING_PROTOCOL.
  @param[in]  Node                   The pointer to the requested node info structure.
  @param[out] DeviceType             Pointer to allow the return of the device type
  @param[out] PciIoInitialize        Pointer to allow return of function that will be called
                                       when the PciIo subsystem connects to this device.
                                       Note that this will may not be called if the device
                                       is not in the boot path.

  @return EFI_SUCCESS               The node is supported by this instance
  @return EFI_UNSUPPORTED           The node is not supported by this instance
**/
EFI_STATUS
DeviceTreeIsSupported (
  IN NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL   *This,
  IN CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL      *Node,
  OUT EFI_GUID                                   **DeviceType,
  OUT NON_DISCOVERABLE_DEVICE_INIT               *PciIoInitialize
  )
{
  if ((Node == NULL) ||
      (DeviceType == NULL) ||
      (PciIoInitialize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *PciIoInitialize = NULL;

  if ((0 == fdt_node_check_compatible (Node->DeviceTreeBase, Node->NodeOffset, "nvidia,tegra186-hsp")) ||
      (0 == fdt_node_check_compatible (Node->DeviceTreeBase, Node->NodeOffset, "nvidia,tegra194-hsp")) ||
      (0 == fdt_node_check_compatible (Node->DeviceTreeBase, Node->NodeOffset, "nvidia,tegra234-hsp"))){
    //Only support hsp with doorbell
    CONST CHAR8 *InterruptNames;
    INT32       NamesLength;

    InterruptNames = (CONST CHAR8*)fdt_getprop (Node->DeviceTreeBase, Node->NodeOffset, "interrupt-names", &NamesLength);
    if ((InterruptNames == NULL) || (NamesLength == 0)) {
      return EFI_UNSUPPORTED;
    }

    while (NamesLength > 0) {
      INT32 Size = AsciiStrSize (InterruptNames);
      if ((Size <= 0) || (Size > NamesLength)) {
        return EFI_UNSUPPORTED;
      }

      if (0 == AsciiStrnCmp (InterruptNames, "doorbell", Size)) {
        *DeviceType = &gNVIDIANonDiscoverableHspTopDeviceGuid;
        return EFI_SUCCESS;
      }
      NamesLength -= Size;
      InterruptNames += Size;
    }
    return EFI_UNSUPPORTED;
  } else if (0 == fdt_node_check_compatible (Node->DeviceTreeBase, Node->NodeOffset, "nvidia,tegra186-bpmp")) {
    *DeviceType = &gNVIDIANonDiscoverableBpmpDeviceGuid;
    return EFI_SUCCESS;
  } else {
    return EFI_UNSUPPORTED;
  }
}

NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL gDeviceTreeCompatibilty = {
    DeviceTreeIsSupported
};

/**
  Supported function of Driver Binding protocol for this driver.
  Test to see if this driver supports ControllerHandle.

  @param This                   Protocol instance pointer.
  @param Controller             Handle of device to test.
  @param RemainingDevicePath    A pointer to the device path.
                                it should be ignored by device driver.

  @retval EFI_SUCCESS           This driver supports this device.
  @retval EFI_ALREADY_STARTED   This driver is already running on this device.
  @retval other                 This driver does not support this device.

**/
EFI_STATUS
EFIAPI
BpmpIpcSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS            Status;
  NON_DISCOVERABLE_DEVICE *NonDiscoverableProtocol = NULL;

  //
  // Attempt to open NonDiscoverable Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **) &NonDiscoverableProtocol,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableBpmpDeviceGuid)) ||
      (CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableHspTopDeviceGuid))) {
    Status = EFI_SUCCESS;
  } else {
    Status = EFI_UNSUPPORTED;
  }

  gBS->CloseProtocol (
         Controller,
         &gNVIDIANonDiscoverableDeviceProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  return Status;

}

/**
  This routine is called right after the .Supported() called and
  Start this driver on ControllerHandle.

  @param This                   Protocol instance pointer.
  @param Controller             Handle of device to bind driver to.
  @param RemainingDevicePath    A pointer to the device path.
                                it should be ignored by device driver.

  @retval EFI_SUCCESS           This driver is added to this device.
  @retval EFI_ALREADY_STARTED   This driver is already running on this device.
  @retval other                 Some error occurs when binding this driver to this device.

**/
EFI_STATUS
EFIAPI
BpmpIpcStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS                        Status;
  NON_DISCOVERABLE_DEVICE           *NonDiscoverableProtocol = NULL;

  DEBUG ((EFI_D_INFO, "%a START\n", __FUNCTION__));

  //
  // Attempt to open NonDiscoverable Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **) &NonDiscoverableProtocol,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableBpmpDeviceGuid)) {
    Status = BpmpIpcProtocolStart (This, Controller, NonDiscoverableProtocol);
  } else if (CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableHspTopDeviceGuid)) {
    Status = HspDoorbellProtocolStart (This, Controller, NonDiscoverableProtocol);
  } else {
    Status = EFI_UNSUPPORTED;
  }

  if (EFI_ERROR (Status)) {

    gBS->CloseProtocol (
          Controller,
          &gNVIDIANonDiscoverableDeviceProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );
  }

  DEBUG ((EFI_D_INFO, "%a END status = %r\n", __FUNCTION__, Status));

  return Status;
}

/**
  Stop this driver on ControllerHandle.

  @param This               Protocol instance pointer.
  @param Controller         Handle of device to stop driver on.
  @param NumberOfChildren   Not used.
  @param ChildHandleBuffer  Not used.

  @retval EFI_SUCCESS   This driver is removed from this device.
  @retval other         Some error occurs when removing this driver from this device.

**/
EFI_STATUS
EFIAPI
BpmpIpcStop (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN UINTN                          NumberOfChildren,
  IN EFI_HANDLE                     *ChildHandleBuffer
  )
{
  EFI_STATUS                        Status;
  NON_DISCOVERABLE_DEVICE           *NonDiscoverableProtocol = NULL;

  //
  // Attempt to open NonDiscoverable Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **) &NonDiscoverableProtocol,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if (CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableBpmpDeviceGuid)) {
    Status = BpmpIpcProtocolStop (This, Controller, NonDiscoverableProtocol);
  } else if (CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableHspTopDeviceGuid)) {
    Status = HspDoorbellProtocolStop (This, Controller, NonDiscoverableProtocol);
  } else {
    Status = EFI_UNSUPPORTED;
  }

  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  //
  // Close protocols opened by BpmpIpc Controller driver
  //
  return gBS->CloseProtocol (
                Controller,
                &gNVIDIANonDiscoverableDeviceProtocolGuid,
                This->DriverBindingHandle,
                Controller
                );
}

///
/// EFI_DRIVER_BINDING_PROTOCOL instance
///
EFI_DRIVER_BINDING_PROTOCOL gBpmpIpcDriverBinding = {
  BpmpIpcSupported,
  BpmpIpcStart,
  BpmpIpcStop,
  0x0,
  NULL,
  NULL
};

/**
  This function allows for a remote IPC to the BPMP firmware to be executed.
  This is a dummy version that is used if BPMP is not present.

  @param[in]     This                The instance of the NVIDIA_BPMP_IPC_PROTOCOL.
  @param[in,out] Token               Optional pointer to a token structure, if this is NULL
                                     this API will process IPC in a blocking manner.
  @param[in]     MessageRequest      Id of the message to send
  @param[in]     TxData              Pointer to the payload data to send
  @param[in]     TxDataSize          Size of the TxData buffer
  @param[out]    RxData              Pointer to the payload data to receive
  @param[in]     RxDataSize          Size of the RxData buffer
  @param[out]    MessageError        If not NULL, will contain the BPMP error code on return

  @return EFI_SUCCESS               If Token is not NULL IPC has been queued.
  @return EFI_SUCCESS               If Token is NULL IPC has been completed.
  @return EFI_INVALID_PARAMETER     Token is not NULL but Token->Event is NULL
  @return EFI_INVALID_PARAMETER     TxData or RxData are NULL
  @return EFI_DEVICE_ERROR          Failed to send IPC
**/
EFI_STATUS
BpmpIpcDummyCommunicate (
  IN  NVIDIA_BPMP_IPC_PROTOCOL   *This,
  IN  OUT NVIDIA_BPMP_IPC_TOKEN  *Token, OPTIONAL
  IN  UINT32                     MessageRequest,
  IN  VOID                       *TxData,
  IN  UINTN                      TxDataSize,
  OUT VOID                       *RxData,
  IN  UINTN                      RxDataSize,
  IN  INT32                      *MessageError OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

CONST NVIDIA_BPMP_IPC_PROTOCOL mBpmpDummyProtocol = {
    BpmpIpcDummyCommunicate
};
/**
  Initialize the Bpmp Ipc Protocol Driver

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
BpmpIpcInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;
  VOID        *DeviceTreeBase;
  UINTN       DeviceTreeSize;

  //If BPMP is disabled on target return dummy ipc protocol
  Status = DtPlatformLoadDtb (&DeviceTreeBase, &DeviceTreeSize);
  if (!EFI_ERROR (Status)) {
    BOOLEAN BpmpPresent = FALSE;
    INT32 NodeOffset;
    NodeOffset = fdt_node_offset_by_compatible (DeviceTreeBase, -1, "nvidia,tegra186-bpmp");
    if (NodeOffset >= 0) {
      CONST VOID *Property = NULL;
      INT32      PropertySize = 0;
      Property = fdt_getprop (DeviceTreeBase,
                              NodeOffset,
                              "status",
                              &PropertySize);
      if ((Property == NULL) || (AsciiStrCmp (Property, "okay") == 0)) {
          BpmpPresent = TRUE;
      }
    }
    if (!BpmpPresent) {
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ImageHandle,
                      &gNVIDIABpmpIpcProtocolGuid,
                      &mBpmpDummyProtocol,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a, Failed to install protocol: %r", __FUNCTION__, Status));
      }
      return Status;
    }
  }

  //
  // Install driver model protocol(s).
  //
  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gBpmpIpcDriverBinding,
             ImageHandle,
             &gBpmpIpcComponentName,
             &gBpmpIpcComponentName2
             );

  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gNVIDIADeviceTreeCompatibilityProtocolGuid,
                  &gDeviceTreeCompatibilty,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    EfiLibUninstallDriverBindingComponentName2 (
      &gBpmpIpcDriverBinding,
      &gBpmpIpcComponentName,
      &gBpmpIpcComponentName2
      );
  }

  return Status;
}


