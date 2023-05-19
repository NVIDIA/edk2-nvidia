/** @file

  EEPROM Driver

  Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/Crc8Lib.h>
#include <libfdt.h>

#include <Protocol/DriverBinding.h>
#include <Protocol/I2cIo.h>
#include <Protocol/Eeprom.h>
#include <Protocol/Rng.h>
#include <Protocol/KernelCmdLineUpdate.h>
#include <Protocol/TegraI2cSlaveDeviceTreeNode.h>

#define SERIAL_NUM_CMD_MAX_LEN  64
#define EEPROM_DATA_SIZE        256
#define EEPROM_DUMMY_BOARDID    "DummyId"
#define EEPROM_DUMMY_SERIALNUM  "DummySN"
#define EEPROM_DUMMY_PRODUCTID  "DummyProd"

EFI_STATUS
EFIAPI
PopulateEepromData (
  IN  UINT8  *EepromData,
  OUT VOID   *BoardInfo
  )
{
  UINTN                    ChipID;
  T194_EEPROM_DATA         *T194EepromData;
  T234_EEPROM_DATA         *T234EepromData;
  TEGRA_EEPROM_BOARD_INFO  *EepromBoardInfo;
  CONST CHAR8              *BoardId;

  ChipID = TegraGetChipID ();

  if (ChipID == T194_CHIP_ID) {
    T194EepromData  = (T194_EEPROM_DATA *)EepromData;
    EepromBoardInfo = (TEGRA_EEPROM_BOARD_INFO *)BoardInfo;
    BoardId         = TegraBoardIdFromPartNumber (&T194EepromData->PartNumber);
    CopyMem ((VOID *)EepromBoardInfo->BoardId, BoardId, TEGRA_BOARD_ID_LEN);
    CopyMem ((VOID *)EepromBoardInfo->ProductId, (VOID *)&T194EepromData->PartNumber, sizeof (T194EepromData->PartNumber));
    CopyMem ((VOID *)EepromBoardInfo->SerialNumber, (VOID *)&T194EepromData->SerialNumber, sizeof (T194EepromData->SerialNumber));
    if ((CompareMem (T194EepromData->CustomerBlockSignature, EEPROM_CUSTOMER_BLOCK_SIGNATURE, sizeof (T194EepromData->CustomerBlockSignature)) == 0) &&
        (CompareMem (T194EepromData->CustomerTypeSignature, EEPROM_CUSTOMER_TYPE_SIGNATURE, sizeof (T194EepromData->CustomerTypeSignature)) == 0))
    {
      CopyMem ((VOID *)EepromBoardInfo->MacAddr, (VOID *)T194EepromData->CustomerEthernetMacAddress, NET_ETHER_ADDR_LEN);
    } else {
      CopyMem ((VOID *)EepromBoardInfo->MacAddr, (VOID *)T194EepromData->EthernetMacAddress, NET_ETHER_ADDR_LEN);
    }
  } else if (ChipID == T234_CHIP_ID) {
    T234EepromData  = (T234_EEPROM_DATA *)EepromData;
    EepromBoardInfo = (TEGRA_EEPROM_BOARD_INFO *)BoardInfo;
    BoardId         = TegraBoardIdFromPartNumber (&T234EepromData->PartNumber);
    CopyMem ((VOID *)EepromBoardInfo->BoardId, BoardId, TEGRA_BOARD_ID_LEN);
    CopyMem ((VOID *)EepromBoardInfo->ProductId, (VOID *)&T234EepromData->PartNumber, sizeof (T234EepromData->PartNumber));
    CopyMem ((VOID *)EepromBoardInfo->SerialNumber, (VOID *)&T234EepromData->SerialNumber, sizeof (T234EepromData->SerialNumber));
    if ((CompareMem (T234EepromData->CustomerBlockSignature, EEPROM_CUSTOMER_BLOCK_SIGNATURE, sizeof (T234EepromData->CustomerBlockSignature)) == 0) &&
        (CompareMem (T234EepromData->CustomerTypeSignature, EEPROM_CUSTOMER_TYPE_SIGNATURE, sizeof (T234EepromData->CustomerTypeSignature)) == 0))
    {
      CopyMem ((VOID *)EepromBoardInfo->MacAddr, (VOID *)T234EepromData->CustomerEthernetMacAddress, NET_ETHER_ADDR_LEN);
      EepromBoardInfo->NumMacs = T234EepromData->CustomerNumEthernetMacs;
    } else {
      CopyMem ((VOID *)EepromBoardInfo->MacAddr, (VOID *)T234EepromData->EthernetMacAddress, NET_ETHER_ADDR_LEN);
      EepromBoardInfo->NumMacs = T234EepromData->NumEthernetMacs;
    }
  } else {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ValidateEepromData (
  IN UINT8    *EepromData,
  IN BOOLEAN  IgnoreVersionCheck,
  IN BOOLEAN  IgnoreCRCCheck
  )
{
  UINTN             ChipID;
  T194_EEPROM_DATA  *T194EepromData;
  T234_EEPROM_DATA  *T234EepromData;
  UINT8             Checksum;

  ChipID = TegraGetChipID ();

  if (ChipID == T194_CHIP_ID) {
    T194EepromData = (T194_EEPROM_DATA *)EepromData;
    if (!IgnoreVersionCheck &&
        (T194EepromData->Version != T194_EEPROM_VERSION))
    {
      DEBUG ((DEBUG_ERROR, "%a: Invalid version in eeprom %x\r\n", __FUNCTION__, T194EepromData->Version));
      return EFI_DEVICE_ERROR;
    }

    if ((T194EepromData->Size <= ((UINTN)&T194EepromData->Reserved2 - (UINTN)T194EepromData))) {
      DEBUG ((DEBUG_ERROR, "%a: Invalid size in eeprom %x\r\n", __FUNCTION__, T194EepromData->Size));
      return EFI_DEVICE_ERROR;
    }

    if (!IgnoreCRCCheck) {
      Checksum = CalculateCrc8 (EepromData, EEPROM_DATA_SIZE - 1, 0, TYPE_CRC8_MAXIM);
      if (Checksum != T194EepromData->Checksum) {
        DEBUG ((DEBUG_ERROR, "%a: CRC mismatch, expected %02x got %02x\r\n", __FUNCTION__, Checksum, T194EepromData->Checksum));
        return EFI_DEVICE_ERROR;
      }
    }
  } else if (ChipID == T234_CHIP_ID) {
    T234EepromData = (T234_EEPROM_DATA *)EepromData;
    if (!IgnoreVersionCheck &&
        (T234EepromData->Version != T234_EEPROM_VERSION))
    {
      DEBUG ((DEBUG_ERROR, "%a: Invalid version in eeprom %x\r\n", __FUNCTION__, T234EepromData->Version));
      return EFI_DEVICE_ERROR;
    }

    if ((T234EepromData->Size <= ((UINTN)&T234EepromData->Reserved2 - (UINTN)T234EepromData))) {
      DEBUG ((DEBUG_ERROR, "%a: Invalid size in eeprom %x\r\n", __FUNCTION__, T234EepromData->Size));
      return EFI_DEVICE_ERROR;
    }

    if (!IgnoreCRCCheck) {
      Checksum = CalculateCrc8 (EepromData, EEPROM_DATA_SIZE - 1, 0, TYPE_CRC8_MAXIM);
      if (Checksum != T234EepromData->Checksum) {
        DEBUG ((DEBUG_ERROR, "%a: CRC mismatch, expected %02x got %02x\r\n", __FUNCTION__, Checksum, T234EepromData->Checksum));
        return EFI_DEVICE_ERROR;
      }
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
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS           Status;
  EFI_I2C_IO_PROTOCOL  *I2cIo;
  EFI_RNG_PROTOCOL     *RngProtocol;
  TEGRA_PLATFORM_TYPE  PlatformType;

  PlatformType = TegraGetPlatform ();
  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    // Check whether driver has already been started.
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiI2cIoProtocolGuid,
                    (VOID **)&I2cIo,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_BY_DRIVER
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = gBS->CloseProtocol (
                    Controller,
                    &gEfiI2cIoProtocolGuid,
                    This->DriverBindingHandle,
                    Controller
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (CompareGuid (&gNVIDIAEeprom, I2cIo->DeviceGuid)) {
      Status = EFI_SUCCESS;
    } else {
      Status = EFI_UNSUPPORTED;
    }
  } else {
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiRngProtocolGuid,
                    (VOID **)&RngProtocol,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_BY_DRIVER
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = gBS->CloseProtocol (
                    Controller,
                    &gEfiI2cIoProtocolGuid,
                    This->DriverBindingHandle,
                    Controller
                    );
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
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS               Status;
  EFI_I2C_IO_PROTOCOL      *I2cIo       = NULL;
  EFI_RNG_PROTOCOL         *RngProtocol = NULL;
  EFI_I2C_REQUEST_PACKET   *Request     = NULL;
  UINT8                    Address      = 0;
  UINT8                    *RawData;
  TEGRA_PLATFORM_TYPE      PlatformType;
  BOOLEAN                  CvmEeprom;
  TEGRA_EEPROM_BOARD_INFO  *CvmBoardInfo;
  TEGRA_EEPROM_BOARD_INFO  *IdBoardInfo;
  BOOLEAN                  SkipEepromCRC;
  INT32                    DeviceTreePathLen;

  NVIDIA_TEGRA_I2C_SLAVE_DEVICE_TREE_NODE_PROTOCOL *I2cSlaveDeviceTreeNode = NULL;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL                 EepromDeviceTreeNode;


  RawData       = NULL;
  CvmBoardInfo  = NULL;
  IdBoardInfo   = NULL;
  CvmEeprom     = FALSE;
  SkipEepromCRC = FALSE;

  PlatformType = TegraGetPlatform ();
  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    // Open i2cio Controller Protocol
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiI2cIoProtocolGuid,
                    (VOID **)&I2cIo,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_BY_DRIVER
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to open I2cIo Protocol\n", __FUNCTION__));
      goto ErrorExit;
    }

    EFI_HANDLE  Handle[10]; // Usually only 8 Tegra I2C buses to choose from
    UINTN       HandleSize = sizeof(Handle);
    Status     = gBS->LocateHandle(ByProtocol,  &gNVIDIAI2cSlaveDeviceTreeNodeProtocolGuid, NULL,  &HandleSize, &Handle[0]);

    if ((EFI_ERROR (Status))) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to LocateHandle for I2cSlaveDeviceTreeNode Protocol (Status: %d)\n", __FUNCTION__, Status));
      return Status;
    }


    INT32 NumHandles = HandleSize/sizeof(Handle[0]);
    for (INT32 i = 0; i < NumHandles; i++) {
      Status = gBS->HandleProtocol (Handle[i], &gNVIDIAI2cSlaveDeviceTreeNodeProtocolGuid, (VOID **)&I2cSlaveDeviceTreeNode);
      if ((EFI_ERROR (Status))) {
	DEBUG ((DEBUG_ERROR, "%a: Unable to HandleProtocol for index %d I2cSlaveDeviceTreeNode Protocol (Status: %d)\n", __FUNCTION__, i, Status));
	return Status;
      }

      Status = I2cSlaveDeviceTreeNode->LookupNode(I2cSlaveDeviceTreeNode, I2cIo->DeviceGuid, I2cIo->DeviceIndex, &EepromDeviceTreeNode);
      if (!(EFI_ERROR (Status))) {
	// found
	break;
      }
    }

    if ((EFI_ERROR (Status))) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to LookupNode using any of the %d I2cSlaveDeviceTreeNode Protocols (device index %x, Status: %d)\n", __FUNCTION__, NumHandles, I2cIo->DeviceIndex, Status));
      return Status;
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

    Request->OperationCount             = 2;
    Request->Operation[0].Flags         = 0;
    Request->Operation[0].LengthInBytes = sizeof (Address);
    Request->Operation[0].Buffer        = &Address;
    Request->Operation[1].Flags         = I2C_FLAG_READ;
    Request->Operation[1].LengthInBytes = EEPROM_DATA_SIZE;
    Request->Operation[1].Buffer        = RawData;
    Status                              = I2cIo->QueueRequest (I2cIo, 0, NULL, Request, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "Failed to read eeprom (%r)\r\n", Status));
      goto ErrorExit;
    }

    FreePool (Request);
    Request = NULL;
    if (fdt_get_property(EepromDeviceTreeNode.DeviceTreeBase, EepromDeviceTreeNode.NodeOffset, "uefi-skip-crc", NULL) != NULL ||
	0 == AsciiStrnCmp (
               (CHAR8 *)&RawData[CAMERA_EEPROM_PART_OFFSET],
               CAMERA_EEPROM_PART_NAME,
               AsciiStrLen (CAMERA_EEPROM_PART_NAME)
               ))
    {
      SkipEepromCRC = TRUE;
    }

    Status = ValidateEepromData (RawData, TRUE, SkipEepromCRC);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Eeprom data validation failed(%r)\r\n", Status));
      goto ErrorExit;
    }

    IdBoardInfo = (TEGRA_EEPROM_BOARD_INFO *)AllocateZeroPool (sizeof (TEGRA_EEPROM_BOARD_INFO));
    if (IdBoardInfo == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    DeviceTreePathLen = fdt_get_path(EepromDeviceTreeNode.DeviceTreeBase, EepromDeviceTreeNode.NodeOffset, IdBoardInfo->EepromDeviceTreePath, MAX_I2C_DEVICE_DT_PATH);

    if (DeviceTreePathLen != 0) {
      DEBUG((DEBUG_ERROR, "%a: Failed to get device tree path length for I2c sub-device 0x%lx on I2c Bus 0x%lx (error: %d).\n", __FUNCTION__, I2cIo->DeviceIndex >> 16, I2cIo->DeviceIndex & 0xFFFF, DeviceTreePathLen));
    } else {
      DEBUG ((DEBUG_INFO, "%a: Starting (TEGRA_PLATFORM_SILICON) Bus %x Device %x %a\r\n", __FUNCTION__,   I2cIo->DeviceIndex >> 16, I2cIo->DeviceIndex & 0xFFFF, IdBoardInfo->EepromDeviceTreePath));
    }

    Status = PopulateEepromData (RawData, IdBoardInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Eeprom data population failed(%r)\r\n", Status));
      goto ErrorExit;
    }

    DEBUG ((DEBUG_ERROR, "Eeprom Product Id: %a\r\n", IdBoardInfo->ProductId));
  } else {
    CvmEeprom = TRUE;
    // Use RNG to generate a random MAC address instead
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiRngProtocolGuid,
                    (VOID **)&RngProtocol,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_BY_DRIVER
                    );
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }

    // Allocate EEPROM Data
    CvmBoardInfo = (TEGRA_EEPROM_BOARD_INFO *)AllocateZeroPool (sizeof (TEGRA_EEPROM_BOARD_INFO));
    if (CvmBoardInfo == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    // Get random EEPROM data
    Status = RngProtocol->GetRNG (RngProtocol, NULL, NET_ETHER_ADDR_LEN, CvmBoardInfo->MacAddr);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get RNG for EEPROM\r\n", __FUNCTION__));
      goto ErrorExit;
    }
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  CvmEeprom ?
                  &gNVIDIACvmEepromProtocolGuid :
                  &gNVIDIAEepromProtocolGuid,
                  CvmEeprom ?
                  (VOID *)CvmBoardInfo :
                  (VOID *)IdBoardInfo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install EEPROM protocols\n", __FUNCTION__));
    goto ErrorExit;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (CvmBoardInfo != NULL) {
      gBS->UninstallMultipleProtocolInterfaces (
             Controller,
             &gNVIDIACvmEepromProtocolGuid,
             CvmBoardInfo,
             NULL
             );
      FreePool (CvmBoardInfo);
    }

    if (IdBoardInfo != NULL) {
      gBS->UninstallMultipleProtocolInterfaces (
             Controller,
             &gNVIDIAEepromProtocolGuid,
             IdBoardInfo,
             NULL
             );
      FreePool (IdBoardInfo);
    }

    if (RawData != NULL) {
      FreePool (RawData);
    }

    if (Request != NULL) {
      FreePool (Request);
    }

    if (I2cIo != NULL) {
      gBS->CloseProtocol (
             Controller,
             &gEfiI2cIoProtocolGuid,
             This->DriverBindingHandle,
             Controller
             );
    }

    if (RngProtocol != NULL) {
      gBS->CloseProtocol (
             Controller,
             &gEfiRngProtocolGuid,
             This->DriverBindingHandle,
             Controller
             );
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
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS           Status;
  UINT8                *EepromData;
  TEGRA_PLATFORM_TYPE  PlatformType;

  EepromData = NULL;

  PlatformType = TegraGetPlatform ();
  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAEepromProtocolGuid, (VOID **)&EepromData);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get eeprom protocol (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }

    Status = gBS->UninstallMultipleProtocolInterfaces (
                    Controller,
                    &gNVIDIAEepromProtocolGuid,
                    EepromData,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to uninstall eeprom protocol (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }

    Status = gBS->CloseProtocol (
                    Controller,
                    &gEfiI2cIoProtocolGuid,
                    This->DriverBindingHandle,
                    Controller
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to close i2cio protocol (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }
  } else {
    Status = gBS->HandleProtocol (Controller, &gNVIDIACvmEepromProtocolGuid, (VOID **)&EepromData);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get cvm eeprom protocol (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }

    Status = gBS->UninstallMultipleProtocolInterfaces (
                    Controller,
                    &gNVIDIACvmEepromProtocolGuid,
                    EepromData,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to uninstall cvm eeprom protocol (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }

    Status = gBS->CloseProtocol (
                    Controller,
                    &gEfiRngProtocolGuid,
                    This->DriverBindingHandle,
                    Controller
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to close rng protocol (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  FreePool (EepromData);
  return EFI_SUCCESS;
}

EFI_DRIVER_BINDING_PROTOCOL  gEepromDxeDriverBinding = {
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
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_HANDLE                              Handle;
  TEGRABL_EEPROM_DATA                     *EepromData;
  TEGRA_EEPROM_BOARD_INFO                 *CvmBoardInfo;
  TEGRA_EEPROM_BOARD_INFO                 *CvbBoardInfo;
  EFI_STATUS                              Status;
  TEGRA_PLATFORM_TYPE                     PlatformType;
  BOOLEAN                                 ValidCvmEepromData;
  VOID                                    *Hob;
  NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL  *SerialNumberCmdLine;
  CHAR16                                  *SerialNumberCmdLineBuffer;

  PlatformType       = TegraGetPlatform ();
  ValidCvmEepromData = FALSE;
  EepromData         = NULL;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    EepromData = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->EepromData;
  }

  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    if ((EepromData == NULL) || (EepromData->CvmEepromDataSize == 0) ||
        EFI_ERROR (ValidateEepromData (EepromData->CvmEepromData, FALSE, FALSE)))
    {
      DEBUG ((DEBUG_ERROR, "Cvm Eeprom data validation failed\r\n"));
    } else {
      CvmBoardInfo = (TEGRA_EEPROM_BOARD_INFO *)AllocateZeroPool (sizeof (TEGRA_EEPROM_BOARD_INFO));
      if (CvmBoardInfo == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        return Status;
      }

      Status = PopulateEepromData (EepromData->CvmEepromData, CvmBoardInfo);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Cvm Eeprom data population failed(%r)\r\n", Status));
        return Status;
      }

      ValidCvmEepromData = TRUE;
      DEBUG ((DEBUG_ERROR, "Cvm Eeprom Product Id: %a\r\n", CvmBoardInfo->ProductId));
    }
  }

  if (ValidCvmEepromData == FALSE) {
    CvmBoardInfo = (TEGRA_EEPROM_BOARD_INFO *)AllocateZeroPool (sizeof (TEGRA_EEPROM_BOARD_INFO));
    if (CvmBoardInfo == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      return Status;
    }

    AsciiSPrint (
      CvmBoardInfo->BoardId,
      sizeof (CvmBoardInfo->BoardId),
      EEPROM_DUMMY_BOARDID
      );
    AsciiSPrint (
      CvmBoardInfo->ProductId,
      sizeof (CvmBoardInfo->ProductId),
      EEPROM_DUMMY_PRODUCTID
      );
    AsciiSPrint (
      CvmBoardInfo->SerialNumber,
      sizeof (CvmBoardInfo->SerialNumber),
      EEPROM_DUMMY_SERIALNUM
      );
    DEBUG ((DEBUG_ERROR, "Populated dummy Cvm Eeprom data\r\n"));
  }

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gNVIDIACvmEepromProtocolGuid,
                  CvmBoardInfo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install Cvm EEPROM protocols\n", __FUNCTION__));
    return Status;
  }

  SerialNumberCmdLine = (NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL *)AllocateZeroPool (sizeof (NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL));
  if (SerialNumberCmdLine == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  SerialNumberCmdLineBuffer = (CHAR16 *)AllocateZeroPool (sizeof (CHAR16) * SERIAL_NUM_CMD_MAX_LEN);
  if (SerialNumberCmdLineBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  SerialNumberCmdLine->ExistingCommandLineArgument = NULL;
  UnicodeSPrintAsciiFormat (SerialNumberCmdLineBuffer, sizeof (CHAR16) * SERIAL_NUM_CMD_MAX_LEN, "androidboot.serialno=%a", CvmBoardInfo->SerialNumber);
  SerialNumberCmdLine->NewCommandLineArgument = SerialNumberCmdLineBuffer;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gNVIDIAKernelCmdLineUpdateGuid,
                  SerialNumberCmdLine,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install serial number kernel command line update protocol\n", __FUNCTION__));
    return Status;
  }

  if ((EepromData == NULL) || (EepromData->CvbEepromDataSize == 0) ||
      EFI_ERROR (ValidateEepromData (EepromData->CvbEepromData, FALSE, FALSE)))
  {
    DEBUG ((DEBUG_ERROR, "Cvb Eeprom data validation failed(%r)\r\n", Status));
  } else {
    CvbBoardInfo = (TEGRA_EEPROM_BOARD_INFO *)AllocateZeroPool (sizeof (TEGRA_EEPROM_BOARD_INFO));
    if (CvbBoardInfo == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      return Status;
    }

    Status = PopulateEepromData (EepromData->CvbEepromData, CvbBoardInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Cvb Eeprom data population failed(%r)\r\n", Status));
      return Status;
    }

    DEBUG ((DEBUG_ERROR, "Cvb Eeprom Product Id: %a\r\n", CvbBoardInfo->ProductId));

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Handle,
                    &gNVIDIACvbEepromProtocolGuid,
                    CvbBoardInfo,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to install Cvb EEPROM protocols\n", __FUNCTION__));
      return Status;
    }
  }

  // TODO: Add component name support.
  return EfiLibInstallDriverBinding (
           ImageHandle,
           SystemTable,
           &gEepromDxeDriverBinding,
           ImageHandle
           );

  return Status;
}
