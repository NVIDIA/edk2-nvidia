/** @file

  CXL Driver

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CxlDxe.h"

#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Cxl.h>

#include <Library/BaseMemoryLib.h>
#include <Library/CxlLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

/**
 * A list of all the CXL type parsers that have been registered.
 **/
CXL_PARSER_INFO  *mCxlTypeParsers    = NULL;
UINT32           mCxlTypeParserCount = 0;

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
CxlDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  UINTN                     Segment;
  UINTN                     Bus;
  UINTN                     Device;
  UINTN                     Function;
  UINTN                     ParserIndex;
  UINT32                    ExtCapOffset;

  if (mCxlTypeParserCount == 0) {
    DEBUG ((DEBUG_WARN, "%a: No CXL type parsers registered\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  // Add RemainingDevicePath path logic here

  //
  // Open the EFI Device Path protocol needed to perform the supported test
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status == EFI_ALREADY_STARTED) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Close protocol, don't use device path protocol in the Support() function
  //
  gBS->CloseProtocol (
         Controller,
         &gEfiDevicePathProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  //
  // Attempt to Open PCI I/O Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status == EFI_ALREADY_STARTED) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Read Segment, Bus, Device and Function of the CXL device
  //
  Status = PciIo->GetLocation (
                    PciIo,
                    &Segment,
                    &Bus,
                    &Device,
                    &Function
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetLocation failed with error %r\n", __FUNCTION__, Status));
    goto Done;
  }

  // Look for any CXL DVSEC Capability
  ExtCapOffset = 0;
  Status       = CxlLibLocateCxlDvsec (PciIo, &ExtCapOffset, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: Segment: %02x\t Bus: 0x%02x\t Device: 0x%02x\t Function: 0x%02x - No CXL DVSEC found\n",
      __FUNCTION__,
      Segment,
      Bus,
      Device,
      Function
      ));
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

  // Attempt to find a parser that knows how to parse this CXL entity
  Status = EFI_UNSUPPORTED;
  for (ParserIndex = 0; ParserIndex < mCxlTypeParserCount; ParserIndex++) {
    if (EFI_ERROR (mCxlTypeParsers[ParserIndex].Parser (Controller, PciIo))) {
      continue;
    }

    DEBUG ((DEBUG_ERROR, "%a: %a device found\n", __FUNCTION__, mCxlTypeParsers[ParserIndex].ParserName));
    DEBUG ((
      DEBUG_ERROR,
      "%a: Segment: %02x\t Bus: 0x%02x\t Device: 0x%02x\t Function: 0x%02x\n",
      __FUNCTION__,
      Segment,
      Bus,
      Device,
      Function
      ));
    Status = EFI_SUCCESS;
    break;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Segment: %02x\t Bus: 0x%02x\t Device: 0x%02x\t Function: 0x%02x - No suitable CXL parser found, it will be ignored\n",
      __FUNCTION__,
      Segment,
      Bus,
      Device,
      Function
      ));
    goto Done;
  }

Done:
  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  return Status;
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
CxlDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  return EFI_SUCCESS;
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
CxlDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
{
  return EFI_SUCCESS;
}

EFI_DRIVER_BINDING_PROTOCOL  gCxlDriverBinding = {
  CxlDriverBindingSupported,
  CxlDriverBindingStart,
  CxlDriverBindingStop,
  0x1,
  NULL,
  NULL
};

/**
  The user Entry Point for module CxlDxe. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some errors occur when executing this entry point.
**/
EFI_STATUS
EFIAPI
InitializeCxlDxe (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return EfiLibInstallDriverBinding (
           ImageHandle,
           SystemTable,
           &gCxlDriverBinding,
           ImageHandle
           );
}

/**
  Function to register a parser for use by the CXL Driver.

  @param  [in]  Parser            The CXL parser library to register.
  @param  [in]  ParserSkipString  The DTB property to check to see if
                                  the parser should be skipped.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
CxlDxeRegisterTypeParser (
  CXL_PARSER_INFO  *Parser,
  CHAR8            *ParserSkipString
  )
{
  EFI_STATUS       Status;
  INT32            NodeOffset;
  CXL_PARSER_INFO  *NewParserList;

  DEBUG ((DEBUG_VERBOSE, "%a: Called, mCxlTypeParserCount = %u\n", __FUNCTION__, mCxlTypeParserCount));

  if (Parser == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ParserSkipString != NULL) {
    Status = DeviceTreeGetNodeByPath ("/firmware/uefi", &NodeOffset);
    if (Status == EFI_NOT_FOUND) {
      DEBUG ((DEBUG_WARN, "%a: Can't find /firmware/uefi to check for %a\n", __FUNCTION__, ParserSkipString));
    } else if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to check for %a\n", __FUNCTION__, Status, ParserSkipString));
      return Status;
    } else if (NodeOffset >= 0) {
      Status = DeviceTreeGetNodeProperty (NodeOffset, ParserSkipString, NULL, NULL);
      if (Status == EFI_SUCCESS) {
        DEBUG ((DEBUG_ERROR, "%a: Skipping %a due to seeing %a\n", __FUNCTION__, Parser->ParserName, ParserSkipString));
        return EFI_SUCCESS;
      }
    }
  }

  NewParserList = ReallocatePool ((mCxlTypeParserCount) * sizeof (CXL_PARSER_INFO), (mCxlTypeParserCount+1) * sizeof (CXL_PARSER_INFO), mCxlTypeParsers);
  if (NewParserList == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: No resources to add parser for %a to the parser list\n", __FUNCTION__, Parser->ParserName));
    return EFI_OUT_OF_RESOURCES;
  }

  mCxlTypeParsers = NewParserList;

  CopyMem (&mCxlTypeParsers[mCxlTypeParserCount], Parser, sizeof (CXL_PARSER_INFO));
  mCxlTypeParserCount++;

  return EFI_SUCCESS;
}
