/** @file

  TPM2 Driver

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <libfdt.h>

#include <Protocol/DriverBinding.h>
#include <Protocol/DeviceTreeNode.h>
#include <Protocol/QspiController.h>
#include <Protocol/Tpm2.h>

#include "Tpm2Dxe.h"

CONST CHAR8  *TpmCompatibilityMap[] = {
  "tcg,tpm_tis-spi",
  "infineon,slb9670"
};

VENDOR_DEVICE_PATH  VendorDevicePath = {
  {
    HARDWARE_DEVICE_PATH,
    HW_VENDOR_DP,
    {
      (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
      (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
    }
  },
  NVIDIA_TPM2_PROTOCOL_GUID
};

/**
  Performs read/write data transfer to/from TPM over QSPI

  @param  This         pointer to NVIDIA_TPM2_PROTOCOL
  @param  ReadAccess   TRUE:  for read request; FALSE: for write request
  @param  Addr         TPM register address
  @param  Data         pointer to the data buffer
  @param  DataSize     data size in bytes

  @retval EFI_SUCCESS           The transfer completes successfully.
  @retval EFI_INVALID_PARAMETER Data size is out of range.
  @retval Others                Data transmission failed.
**/
EFI_STATUS
Tpm2Transfer (
  IN     NVIDIA_TPM2_PROTOCOL  *This,
  IN     BOOLEAN               ReadAccess,
  IN     UINT16                Addr,
  IN OUT UINT8                 *Data,
  IN     UINT16                DataSize
  )
{
  EFI_STATUS                       Status;
  QSPI_TRANSACTION_PACKET          Packet;
  UINT8                            TxBuf[TPM_SPI_CMD_SIZE + TPM_MAX_TRANSFER_SIZE];
  TPM2_PRIVATE_DATA                *Private;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiInstance;

  if (DataSize > TPM_MAX_TRANSFER_SIZE) {
    ASSERT (FALSE);
    return EFI_INVALID_PARAMETER;
  }

  Private      = TPM2_PRIVATE_DATA (This);
  QspiInstance = Private->QspiController;

  TxBuf[0] = (ReadAccess ? 0x80 : 0x00) | (DataSize - 1);
  TxBuf[1] = TPM_SPI_ADDR_PREFIX;
  TxBuf[2] = (UINT8)(Addr >> 8);
  TxBuf[3] = (UINT8)Addr;

  if (ReadAccess) {
    Packet.TxBuf      = TxBuf;
    Packet.TxLen      = TPM_SPI_CMD_SIZE;
    Packet.RxBuf      = Data;
    Packet.RxLen      = DataSize;
    Packet.WaitCycles = 0;
    Packet.ChipSelect = Private->ChipSelect;
    Packet.Control    = 0;
  } else {
    CopyMem (&TxBuf[TPM_SPI_CMD_SIZE], Data, DataSize);
    Packet.TxBuf      = TxBuf;
    Packet.TxLen      = TPM_SPI_CMD_SIZE + DataSize;
    Packet.RxBuf      = NULL;
    Packet.RxLen      = 0;
    Packet.WaitCycles = 0;
    Packet.ChipSelect = Private->ChipSelect;
    Packet.Control    = 0;
  }

  Status = QspiInstance->PerformTransaction (QspiInstance, &Packet);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to %a %04x. %r\n", __FUNCTION__, ReadAccess ? "read" : "write", Addr, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Check for TPM in device tree.

  Looks through all subnodes of the QSPI node to see if any of them has
  TPM subnode.

  @param[in]   Controller       The handle of the controller to test. This handle
                                must support a protocol interface that supplies
                                an I/O abstraction to the driver.

  @retval EFI_SUCCESS       Operation successful.
  @retval others            Error occurred
**/
EFI_STATUS
CheckTpmCompatibility (
  IN EFI_HANDLE  Controller
  )
{
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTree = NULL;
  INT32                             Index       = 0;
  INT32                             Node        = 0;

  // Check whether device tree node protocol is available.
  Status = gBS->HandleProtocol (
                  Controller,
                  &gNVIDIADeviceTreeNodeProtocolGuid,
                  (VOID **)&DeviceTree
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Check compatibility
  fdt_for_each_subnode (Node, DeviceTree->DeviceTreeBase, DeviceTree->NodeOffset) {
    for (Index = 0; Index < ARRAY_SIZE (TpmCompatibilityMap); Index++) {
      if (0 == fdt_node_check_compatible (DeviceTree->DeviceTreeBase, Node, TpmCompatibilityMap[Index])) {
        DEBUG ((DEBUG_INFO, "%a: TPM device found.\n", __FUNCTION__));
        return EFI_SUCCESS;
      }
    }
  }

  return EFI_UNSUPPORTED;
}

/**
  Get TPM properties from device tree.

  @param  Private      pointer to TPM2_PRIVATE_DATA
  @param  Controller   handle of the QSPI controller

  @retval EFI_SUCCESS       Operation successful.
  @retval others            Error occurred
**/
EFI_STATUS
GetTpmProperties (
  IN TPM2_PRIVATE_DATA  *Private,
  IN EFI_HANDLE         Controller
  )
{
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTree  = NULL;
  INT32                             Index        = 0;
  INT32                             Node         = 0;
  CONST VOID                        *Property    = NULL;
  INT32                             PropertySize = 0;
  UINT8                             NumChipSelects;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL   *QspiInstance;

  // Check whether device tree node protocol is available.
  Status = gBS->HandleProtocol (
                  Controller,
                  &gNVIDIADeviceTreeNodeProtocolGuid,
                  (VOID **)&DeviceTree
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  QspiInstance = Private->QspiController;
  Status       = QspiInstance->GetNumChipSelects (QspiInstance, &NumChipSelects);
  ASSERT_EFI_ERROR (Status);

  // Look for TPM device node
  fdt_for_each_subnode (Node, DeviceTree->DeviceTreeBase, DeviceTree->NodeOffset) {
    for (Index = 0; Index < ARRAY_SIZE (TpmCompatibilityMap); Index++) {
      if (0 == fdt_node_check_compatible (DeviceTree->DeviceTreeBase, Node, TpmCompatibilityMap[Index])) {
        Property = fdt_getprop (
                     DeviceTree->DeviceTreeBase,
                     Node,
                     "reg",
                     &PropertySize
                     );
        if (Property != NULL) {
          Private->ChipSelect = SwapBytes32 (*(UINT32 *)Property);
          if (Private->ChipSelect >= NumChipSelects) {
            ASSERT (Private->ChipSelect < NumChipSelects);
            return EFI_UNSUPPORTED;
          }
        }

        return EFI_SUCCESS;
      }
    }
  }

  return EFI_UNSUPPORTED;
}

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  This function checks to see if the driver specified by This supports the device specified by
  ControllerHandle. Drivers will typically use the device path attached to
  ControllerHandle and/or the services from the bus I/O abstraction attached to
  ControllerHandle to determine if the driver supports ControllerHandle. This function
  may be called many times during platform initialization. In order to reduce boot times, the tests
  performed by this function must be very small, and take as little time as possible to execute. This
  function must not change the state of any hardware devices, and this function must be aware that the
  device specified by ControllerHandle may already be managed by the same driver or a
  different driver. This function must match its calls to AllocatePages() with FreePages(),
  AllocatePool() with FreePool(), and OpenProtocol() with CloseProtocol().
  Since ControllerHandle may have been previously started by the same driver, if a protocol is
  already in the opened state, then it must not be closed with CloseProtocol(). This is required
  to guarantee the state of ControllerHandle is not modified by this function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
Tpm2DxeDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                       Status;
  EFI_STATUS                       CompatibilityStatus;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiInstance = NULL;

  // Check whether driver has already been started.
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIAQspiControllerProtocolGuid,
                  (VOID **)&QspiInstance,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CompatibilityStatus = CheckTpmCompatibility (Controller);

  Status = gBS->CloseProtocol (
                  Controller,
                  &gNVIDIAQspiControllerProtocolGuid,
                  This->DriverBindingHandle,
                  Controller
                  );
  ASSERT_EFI_ERROR (Status);

  return CompatibilityStatus;
}

/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service ConnectController().
  As a result, much of the error checking on the parameters to Start() has been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a naturally aligned
     EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver specified by This must
     have been called with the same calling parameters, and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For a bus driver, if this parameter is NULL, then handles
                                   for all the children of Controller are created by this driver.
                                   If this parameter is not NULL and the first Device Path Node is
                                   not the End of Device Path Node, then only the handle for the
                                   child device specified by the first Device Path Node of
                                   RemainingDevicePath is created by this driver.
                                   If the first Device Path Node of RemainingDevicePath is
                                   the End of Device Path Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.
**/
EFI_STATUS
EFIAPI
Tpm2DxeDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                       Status;
  TPM2_PRIVATE_DATA                *Private;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiInstance = NULL;
  EFI_DEVICE_PATH_PROTOCOL         *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL         *TpmDevicePath;
  VOID                             *Interface;

  // Open Qspi Controller Protocol
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIAQspiControllerProtocolGuid,
                  (VOID **)&QspiInstance,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to open QSPI Protocol\n", __FUNCTION__));
    goto ErrorExit;
  }

  // Allocate Private Data
  Private = AllocateRuntimeZeroPool (sizeof (TPM2_PRIVATE_DATA));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Private->Signature      = TPM2_SIGNATURE;
  Private->QspiController = QspiInstance;

  Status = GetTpmProperties (Private, Controller);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = QspiInstance->DeviceSpecificInit (QspiInstance, QspiDevFeatWaitState);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  // Get Parent's device path.
  Status = gBS->HandleProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&ParentDevicePath
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to get parent's device path\n", __FUNCTION__));
    goto ErrorExit;
  }

  // Append Vendor device path to parent device path.
  TpmDevicePath = AppendDevicePathNode (
                    ParentDevicePath,
                    (EFI_DEVICE_PATH_PROTOCOL *)&VendorDevicePath
                    );
  if (TpmDevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Private->Tpm2Handle            = NULL;
  Private->TpmDevicePath         = TpmDevicePath;
  Private->Tpm2Protocol.Transfer = Tpm2Transfer;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->Tpm2Handle,
                  &gNVIDIATpm2ProtocolGuid,
                  &Private->Tpm2Protocol,
                  &gEfiDevicePathProtocolGuid,
                  Private->TpmDevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install Tpm2 protocols\n", __FUNCTION__));
    goto ErrorExit;
  }

  Private->ProtocolsInstalled = TRUE;

  // Open caller ID protocol for child
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gEfiCallerIdGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install callerid protocol\n", __FUNCTION__));
    goto ErrorExit;
  }

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiCallerIdGuid,
                  (VOID **)&Interface,
                  This->DriverBindingHandle,
                  Private->Tpm2Handle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to open caller ID protocol\n", __FUNCTION__));
    goto ErrorExit;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      gBS->CloseProtocol (
             Controller,
             &gEfiCallerIdGuid,
             This->DriverBindingHandle,
             Private->Tpm2Handle
             );
      gBS->UninstallMultipleProtocolInterfaces (
             Controller,
             &gEfiCallerIdGuid,
             NULL,
             NULL
             );
      if (Private->ProtocolsInstalled) {
        gBS->UninstallMultipleProtocolInterfaces (
               Private->Tpm2Handle,
               &gNVIDIATpm2ProtocolGuid,
               &Private->Tpm2Protocol,
               &gEfiDevicePathProtocolGuid,
               Private->TpmDevicePath,
               NULL
               );
      }

      if (Private->TpmDevicePath != NULL) {
        FreePool (Private->TpmDevicePath);
      }

      FreePool (Private);
    }

    gBS->CloseProtocol (
           Controller,
           &gNVIDIAQspiControllerProtocolGuid,
           This->DriverBindingHandle,
           Controller
           );
  }

  return Status;
}

/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been moved
  into this common boot service. It is legal to call Stop() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous call to this
     same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in this driver's
     Start() function, and the Start() function must have called OpenProtocol() on
     ControllerHandle with an Attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.
**/
EFI_STATUS
EFIAPI
Tpm2DxeDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS            Status;
  NVIDIA_TPM2_PROTOCOL  *Tpm2Protocol;
  TPM2_PRIVATE_DATA     *Private;
  UINT32                Index;

  if (NumberOfChildren == 0) {
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < NumberOfChildren; Index++) {
    Status = gBS->OpenProtocol (
                    ChildHandleBuffer[Index],
                    &gNVIDIATpm2ProtocolGuid,
                    (VOID **)&Tpm2Protocol,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      // Not handled by this driver
      continue;
    }

    Private = TPM2_PRIVATE_DATA (Tpm2Protocol);

    Status = gBS->CloseProtocol (
                    Controller,
                    &gEfiCallerIdGuid,
                    This->DriverBindingHandle,
                    ChildHandleBuffer[Index]
                    );
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

    if (Private->ProtocolsInstalled) {
      Status = gBS->UninstallMultipleProtocolInterfaces (
                      ChildHandleBuffer[Index],
                      &gNVIDIATpm2ProtocolGuid,
                      &Private->Tpm2Protocol,
                      &gEfiDevicePathProtocolGuid,
                      Private->TpmDevicePath,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }
    }

    if (Private->TpmDevicePath != NULL) {
      FreePool (Private->TpmDevicePath);
    }

    FreePool (Private);
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Controller,
                  &gEfiCallerIdGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = gBS->CloseProtocol (
                  Controller,
                  &gNVIDIAQspiControllerProtocolGuid,
                  This->DriverBindingHandle,
                  Controller
                  );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_DRIVER_BINDING_PROTOCOL  gTpm2DxeDriverBinding = {
  Tpm2DxeDriverBindingSupported,
  Tpm2DxeDriverBindingStart,
  Tpm2DxeDriverBindingStop,
  0x1,
  NULL,
  NULL
};

/**
  The user Entry Point for module Tpm2Dxe. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some errors occur when executing this entry point.
**/
EFI_STATUS
EFIAPI
InitializeTpm2Dxe (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  // TODO: Add component name support.
  return EfiLibInstallDriverBinding (
           ImageHandle,
           SystemTable,
           &gTpm2DxeDriverBinding,
           ImageHandle
           );
}
