/** @file

  EEPROM Driver

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2019-2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2020 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <PiDxe.h>

#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/Crc8Lib.h>

#include <Protocol/DriverBinding.h>
#include <Protocol/I2cIo.h>
#include <Protocol/Eeprom.h>
#include <Protocol/Rng.h>

#define EEPROM_DATA_SIZE 256

EFI_STATUS
EFIAPI
ValidateEepromData (
  IN UINT8 *EepromData
)
{
  UINTN             ChipID;
  T194_EEPROM_DATA  *T194EepromData;
  T234_EEPROM_DATA  *T234EepromData;
  UINT8             Checksum;

  ChipID = TegraGetChipID();

  if (ChipID == T194_CHIP_ID) {
    T194EepromData = (T194_EEPROM_DATA *)EepromData;
    if ((T194EepromData->Version != T194_EEPROM_VERSION) ||
        (T194EepromData->Size <= ((UINTN)&T194EepromData->Reserved2 - (UINTN)T194EepromData))) {
      DEBUG ((DEBUG_ERROR, "%a: Invalid size/version in eeprom %x %x\r\n",
              __FUNCTION__, T194EepromData->Version, T194EepromData->Size));
      return EFI_DEVICE_ERROR;
    }

    Checksum = CalculateCrc8 (EepromData, EEPROM_DATA_SIZE - 1, 0, TYPE_CRC8_MAXIM);
    if (Checksum != T194EepromData->Checksum) {
      DEBUG ((DEBUG_ERROR, "%a: CRC mismatch, expected %02x got %02x\r\n", __FUNCTION__, Checksum, T194EepromData->Checksum));
      return EFI_DEVICE_ERROR;
    }
  } else if (ChipID == T234_CHIP_ID) {
    T234EepromData = (T234_EEPROM_DATA *)EepromData;
    if ((T234EepromData->Version != T234_EEPROM_VERSION) ||
        (T234EepromData->Size <= ((UINTN)&T234EepromData->Reserved2 - (UINTN)T234EepromData))) {
      DEBUG ((DEBUG_ERROR, "%a: Invalid size/version in eeprom %x %x\r\n",
              __FUNCTION__, T234EepromData->Version, T234EepromData->Size));
      return EFI_DEVICE_ERROR;
    }

    Checksum = CalculateCrc8 (EepromData, EEPROM_DATA_SIZE - 1, 0, TYPE_CRC8_MAXIM);
    if (Checksum != T234EepromData->Checksum) {
      DEBUG ((DEBUG_ERROR, "%a: CRC mismatch, expected %02x got %02x\r\n", __FUNCTION__, Checksum, T234EepromData->Checksum));
      return EFI_DEVICE_ERROR;
    }
  } else {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
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
EepromDxeDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL   *This,
  IN EFI_HANDLE                    Controller,
  IN EFI_DEVICE_PATH_PROTOCOL      *RemainingDevicePath
)
{
  EFI_STATUS                       Status;
  EFI_I2C_IO_PROTOCOL              *I2cIo;
  EFI_RNG_PROTOCOL                 *RngProtocol;
  BOOLEAN                          SupportedCvmDevice;
  BOOLEAN                          SupportedIdDevice;
  TEGRA_PLATFORM_TYPE              PlatformType;

  PlatformType = TegraGetPlatform();
  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    // Check whether driver has already been started.
    Status = gBS->OpenProtocol (Controller,
                                &gEfiI2cIoProtocolGuid,
                                (VOID**) &I2cIo,
                                This->DriverBindingHandle,
                                Controller,
                                EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    SupportedCvmDevice = CompareGuid (&gNVIDIACvmEeprom, I2cIo->DeviceGuid);
    SupportedIdDevice = CompareGuid (&gNVIDIAIdEeprom, I2cIo->DeviceGuid);

    Status = gBS->CloseProtocol (Controller,
                                &gEfiI2cIoProtocolGuid,
                                This->DriverBindingHandle,
                                Controller);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (SupportedCvmDevice || SupportedIdDevice) {
      Status = EFI_SUCCESS;
    } else {
      Status = EFI_UNSUPPORTED;
    }
  } else {
    Status = gBS->OpenProtocol (Controller,
                                &gEfiRngProtocolGuid,
                                (VOID**) &RngProtocol,
                                This->DriverBindingHandle,
                                Controller,
                                EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = gBS->CloseProtocol (Controller,
                                &gEfiI2cIoProtocolGuid,
                                This->DriverBindingHandle,
                                Controller);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

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
EepromDxeDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL   *This,
  IN EFI_HANDLE                    Controller,
  IN EFI_DEVICE_PATH_PROTOCOL      *RemainingDevicePath
)
{
  EFI_STATUS              Status;
  EFI_I2C_IO_PROTOCOL     *I2cIo = NULL;
  EFI_RNG_PROTOCOL        *RngProtocol = NULL;
  EFI_I2C_REQUEST_PACKET  *Request = NULL;
  UINT8                   Address = 0;
  UINT8                   *RawData;
  TEGRA_PLATFORM_TYPE     PlatformType;
  BOOLEAN                 CvmEeprom;

  RawData = NULL;
  CvmEeprom = FALSE;

  PlatformType = TegraGetPlatform();
  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    // Open i2cio Controller Protocol
    Status = gBS->OpenProtocol (Controller,
                                &gEfiI2cIoProtocolGuid,
                                (VOID**) &I2cIo,
                                This->DriverBindingHandle,
                                Controller,
                                EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to open I2cIo Protocol\n", __FUNCTION__));
      goto ErrorExit;
    }

    // Allocate EEPROM Data
    RawData = (UINT8 *)AllocateZeroPool (EEPROM_DATA_SIZE);
    if (RawData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Request = (EFI_I2C_REQUEST_PACKET *)AllocateZeroPool (sizeof (EFI_I2C_REQUEST_PACKET) + sizeof (EFI_I2C_OPERATION));
    if (Request == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }
    Request->OperationCount = 2;
    Request->Operation[0].Flags = 0;
    Request->Operation[0].LengthInBytes = sizeof (Address);
    Request->Operation[0].Buffer = &Address;
    Request->Operation[1].Flags = I2C_FLAG_READ;
    Request->Operation[1].LengthInBytes = EEPROM_DATA_SIZE;
    Request->Operation[1].Buffer = RawData;
    Status = I2cIo->QueueRequest (I2cIo, 0, NULL, Request, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to read eeprom (%r)\r\n", Status));
      goto ErrorExit;
    }
    FreePool (Request);
    Request = NULL;

    Status = ValidateEepromData (RawData);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Eeprom data validation failed(%r)\r\n", Status));
      goto ErrorExit;
    }

    if (CompareGuid (&gNVIDIACvmEeprom, I2cIo->DeviceGuid)) {
      CvmEeprom = TRUE;
    }
  } else {
    CvmEeprom = TRUE;
    // Use RNG to generate a random MAC address instead
    Status = gBS->OpenProtocol (Controller,
                                &gEfiRngProtocolGuid,
                                (VOID**) &RngProtocol,
                                This->DriverBindingHandle,
                                Controller,
                                EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }

    // Allocate EEPROM Data
    RawData = (UINT8 *)AllocateZeroPool (EEPROM_DATA_SIZE);
    if (RawData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    // Get random EEPROM data
    Status = RngProtocol->GetRNG (RngProtocol, NULL, EEPROM_DATA_SIZE, RawData);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get RNG for EEPROM\r\n", __FUNCTION__));
      goto ErrorExit;
    }
  }

  Status = gBS->InstallMultipleProtocolInterfaces (&Controller,
                                                   CvmEeprom ?
                                                     &gNVIDIACvmEepromProtocolGuid :
                                                     &gNVIDIAIdEepromProtocolGuid,
                                                   RawData,
                                                   NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install EEPROM protocols\n", __FUNCTION__));
    goto ErrorExit;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    gBS->UninstallMultipleProtocolInterfaces (Controller,
                                              &gNVIDIACvmEepromProtocolGuid,
                                              RawData,
                                              NULL);
    gBS->UninstallMultipleProtocolInterfaces (Controller,
                                              &gNVIDIAIdEepromProtocolGuid,
                                              RawData,
                                              NULL);
    if (RawData != NULL) {
      FreePool (RawData);
    }
    if (Request != NULL) {
      FreePool (Request);
    }
    if (I2cIo != NULL) {
      gBS->CloseProtocol (Controller,
                          &gEfiI2cIoProtocolGuid,
                          This->DriverBindingHandle,
                          Controller);
    }
    if (RngProtocol != NULL) {
      gBS->CloseProtocol (Controller,
                          &gEfiRngProtocolGuid,
                          This->DriverBindingHandle,
                          Controller);
    }
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
EepromDxeDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN  EFI_HANDLE                      Controller,
  IN  UINTN                           NumberOfChildren,
  IN  EFI_HANDLE                      *ChildHandleBuffer
)
{
  EFI_STATUS            Status;
  UINT8                 *EepromData;
  TEGRA_PLATFORM_TYPE   PlatformType;

  EepromData = NULL;
  Status = gBS->HandleProtocol (Controller, &gNVIDIACvmEepromProtocolGuid, (VOID **)&EepromData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get cvm eeprom protocol (%r)\r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (Controller,
                                                     &gNVIDIACvmEepromProtocolGuid,
                                                     EepromData,
                                                     NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to uninstall cvm eeprom protocol (%r)\r\n", __FUNCTION__, Status));
    return Status;
  }
  FreePool (EepromData);

  EepromData = NULL;
  Status = gBS->HandleProtocol (Controller, &gNVIDIAIdEepromProtocolGuid, (VOID **)&EepromData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get id eeprom protocol (%r)\r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (Controller,
                                                     &gNVIDIAIdEepromProtocolGuid,
                                                     EepromData,
                                                     NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to uninstall id eeprom protocol (%r)\r\n", __FUNCTION__, Status));
    return Status;
  }
  FreePool (EepromData);

  PlatformType = TegraGetPlatform();
  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    Status = gBS->CloseProtocol (Controller,
                                 &gEfiI2cIoProtocolGuid,
                                 This->DriverBindingHandle,
                                 Controller);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to close i2cio protocol (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }
  } else {
    Status = gBS->CloseProtocol (Controller,
                                 &gEfiRngProtocolGuid,
                                 This->DriverBindingHandle,
                                 Controller);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to close rng protocol (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  return EFI_SUCCESS;
}


EFI_DRIVER_BINDING_PROTOCOL gEepromDxeDriverBinding = {
  EepromDxeDriverBindingSupported,
  EepromDxeDriverBindingStart,
  EepromDxeDriverBindingStop,
  0x1,
  NULL,
  NULL
};


/**
  The user Entry Point for module NorFlashDxe. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some errors occur when executing this entry point.
**/
EFI_STATUS
EFIAPI
InitializeEepromDxe (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
)
{
  UINTN       ChipID;
  EFI_HANDLE  Handle;
  UINT32      DataSize;
  UINT8       *EepromData;
  EFI_STATUS  Status;

  ChipID = TegraGetChipID();

  if (ChipID == T234_CHIP_ID) {
    DataSize = GetCvmEepromData (&EepromData);
    if (DataSize == 0) {
      DEBUG ((DEBUG_ERROR, "%a: Eeprom data size received is 0\r\n", __FUNCTION__));
      return EFI_DEVICE_ERROR;
    }

    Status = ValidateEepromData (EepromData);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Eeprom data validation failed(%r)\r\n", Status));
      return Status;
    }

    Handle = NULL;
    Status = gBS->InstallMultipleProtocolInterfaces (&Handle,
                                                     &gNVIDIACvmEepromProtocolGuid,
                                                     EepromData,
                                                     NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to install EEPROM protocols\n", __FUNCTION__));
      return Status;
    }
  }

  // TODO: Add component name support.
  return EfiLibInstallDriverBinding (ImageHandle,
                                     SystemTable,
                                     &gEepromDxeDriverBinding,
                                     ImageHandle);

  return Status;
}
