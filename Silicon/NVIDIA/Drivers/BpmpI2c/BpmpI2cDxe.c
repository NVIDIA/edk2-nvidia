/** @file

  Bpmp I2c Driver

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/Crc8Lib.h>
#include <libfdt.h>
#include <Protocol/DeviceTreeNode.h>

#include "BpmpI2c.h"

STATIC BPMP_I2C_DEVICE_TYPE_MAP mDeviceTypeMap[] = {
    { "maxim,max20024", &gNVIDIAI2cMaxim20024, 1, { { 0x22, 0x48 } } },
    { "maxim,max77620", &gNVIDIAI2cMaxim77620, 1, { { 0x22, 0x48 } } },
    { "maxim,max77851-pmic", &gNVIDIAI2cMaxim77851, 1, { { 0x22, 0x48 } } },
    { "nvidia,vrs-pseq", &gNVIDIAI2cVrsPseq, 0, { { 0x00, 0x00 } } },
    { NULL, NULL, 0, { { 0x00, 0x00 } } }
};

STATIC VENDOR_DEVICE_PATH mDevicePathNode = {
  { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH), 0 } },
  EFI_I2C_MASTER_PROTOCOL_GUID
};

/**
 * Function is signaled when a BpmpIpc call should be signaled.
 *
 * @param[in] Event     Event that was signaled
 * @param[in] Context   Pointer to private data
 */
VOID
BpmpIpcProcess (
 IN EFI_EVENT Event,
 IN VOID *Context
 )
{
  NVIDIA_BPMP_I2C_PRIVATE_DATA *Private = (NVIDIA_BPMP_I2C_PRIVATE_DATA *)Context;
  EFI_I2C_OPERATION            *Operation;
  EFI_STATUS                   Status;
  NVIDIA_BPMP_IPC_TOKEN        *Token;
  UINT32                       RequestSize;
  UINT32                       ResponseSize;
  VOID                         *ResponseData;
  UINT8                        Crc8;
  UINTN                        OperationIndex;
  UINTN                        BufferLocation = 0;
  BPMP_I2C_REQUEST_OP          *I2cRequest;


  if (NULL == Private) {
    return;
  }

  if (NULL == Event) {
    Token = NULL;
  } else {
    Token = &Private->BpmpIpcToken;
  }

  if (Private->TransferInProgress) {
    Private->TransferInProgress = FALSE;
    if (NULL != Token) {
      if (EFI_ERROR (Token->TransactionStatus)) {
        DEBUG ((EFI_D_ERROR, "%a: I2C transfer failed async: %r, %08x\r\n",__FUNCTION__,Token->TransactionStatus, Private->MessageError));
        *Private->TransactionStatus = EFI_DEVICE_ERROR;
        Private->RequestPacket = NULL;
        if (Private->TransactionEvent != NULL) {
          gBS->SignalEvent (Private->TransactionEvent);
        }
        return;
      }
    }
    BufferLocation = 0;
    for (OperationIndex = 0; OperationIndex < Private->RequestPacket->OperationCount; OperationIndex++) {
      Operation = &Private->RequestPacket->Operation[OperationIndex];
      //Sync failures are handled after return of BpmpIpc->Communicate
      if (Operation->Flags == I2C_FLAG_READ) {
        CopyMem (Operation->Buffer, Private->Response.Data + BufferLocation, MIN(Operation->LengthInBytes, Private->Response.DataSize));
        BufferLocation += MIN(Operation->LengthInBytes, Private->Response.DataSize);
      }
    }

    *Private->TransactionStatus = EFI_SUCCESS;
    Private->RequestPacket = NULL;
    if (Private->TransactionEvent != NULL) {
      gBS->SignalEvent (Private->TransactionEvent);
    }
    return;
  }

  BufferLocation = 0;
  Private->Request.Command = BPMP_I2C_CMD_TRANSFER;
  Private->Request.BusId = Private->BusId;
  ResponseSize = sizeof (UINT32);
  ResponseData = &Private->Response;
  for (OperationIndex = 0; OperationIndex < Private->RequestPacket->OperationCount; OperationIndex++) {
    Operation = &Private->RequestPacket->Operation[OperationIndex];
    I2cRequest = (BPMP_I2C_REQUEST_OP *)&Private->Request.Data [BufferLocation];

    I2cRequest->SlaveAddress = Private->SlaveAddress;
    I2cRequest->Length = Operation->LengthInBytes;
    I2cRequest->Flags = 0;

    if (Operation->Flags == I2C_FLAG_READ) {
      I2cRequest->Flags |= BPMP_I2C_READ;
      ResponseSize += Operation->LengthInBytes;
      BufferLocation += sizeof (BPMP_I2C_REQUEST_OP);
    } else if (Operation->Flags == I2C_FLAG_SMBUS_PEC) {
      //Write with PEC
      CopyMem (I2cRequest->Data, Operation->Buffer, Operation->LengthInBytes);

      I2cRequest->Length++;
      //Calculate PEC
      Crc8 = (Private->SlaveAddress << 1);
      Crc8 = CalculateCrc8 (&Crc8, 1, 0, TYPE_CRC8);
      I2cRequest->Data[Operation->LengthInBytes] = CalculateCrc8 (Operation->Buffer, Operation->LengthInBytes, Crc8, TYPE_CRC8);
      BufferLocation += sizeof (BPMP_I2C_REQUEST_OP) + I2cRequest->Length;
    } else if (Operation->Flags == 0) {
      //Write
      CopyMem (I2cRequest->Data, Operation->Buffer, Operation->LengthInBytes);
      BufferLocation += sizeof (BPMP_I2C_REQUEST_OP) + I2cRequest->Length;
    } else {
      //Unsupported
      *Private->TransactionStatus = EFI_UNSUPPORTED;
      if (Private->TransactionEvent != NULL) {
        gBS->SignalEvent (Private->TransactionEvent);
      }
      return;
    }

    if (OperationIndex == (Private->RequestPacket->OperationCount - 1)) {
      I2cRequest->Flags |= BPMP_I2C_STOP;
    }
  }

  Private->Request.DataSize = BufferLocation;
  //3 UINT32s in header (Command, BusId, DataSize) before buffer
  RequestSize = (3 * sizeof (UINT32)) + BufferLocation;

  Status = Private->BpmpIpc->Communicate (
                               Private->BpmpIpc,
                               Token,
                               MRQ_I2C,
                               &Private->Request,
                               RequestSize,
                               ResponseData,
                               ResponseSize,
                               &Private->MessageError
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: I2C transfer failed sync: %r, %08x\r\n",__FUNCTION__,Status, Private->MessageError));
    *Private->TransactionStatus = EFI_DEVICE_ERROR;
    Private->RequestPacket = NULL;
    if (Private->TransactionEvent != NULL) {
      gBS->SignalEvent (Private->TransactionEvent);
    }
    return;
  }

  Private->TransferInProgress = TRUE;

  if (Event == NULL) {
    BpmpIpcProcess (Event, Context);
  }
  return;
}

/**
  Set the frequency for the I2C clock line.

  This routine must be called at or below TPL_NOTIFY.

  The software and controller do a best case effort of using the specified
  frequency for the I2C bus.  If the frequency does not match exactly then
  the I2C master protocol selects the next lower frequency to avoid
  exceeding the operating conditions for any of the I2C devices on the bus.
  For example if 400 KHz was specified and the controller's divide network
  only supports 402 KHz or 398 KHz then the I2C master protocol selects 398
  KHz.  If there are not lower frequencies available, then return
  EFI_UNSUPPORTED.

  @param[in] This           Pointer to an EFI_I2C_MASTER_PROTOCOL structure
  @param[in] BusClockHertz  Pointer to the requested I2C bus clock frequency
                            in Hertz.  Upon return this value contains the
                            actual frequency in use by the I2C controller.

  @retval EFI_SUCCESS           The bus frequency was set successfully.
  @retval EFI_ALREADY_STARTED   The controller is busy with another transaction.
  @retval EFI_INVALID_PARAMETER BusClockHertz is NULL
  @retval EFI_UNSUPPORTED       The controller does not support this frequency.

**/
EFI_STATUS
BpmpI2cSetBusFrequency (
  IN CONST EFI_I2C_MASTER_PROTOCOL   *This,
  IN OUT UINTN                       *BusClockHertz
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Reset the I2C controller and configure it for use

  This routine must be called at or below TPL_NOTIFY.

  The I2C controller is reset.  The caller must call SetBusFrequench() after
  calling Reset().

  @param[in]     This       Pointer to an EFI_I2C_MASTER_PROTOCOL structure.

  @retval EFI_SUCCESS         The reset completed successfully.
  @retval EFI_ALREADY_STARTED The controller is busy with another transaction.
  @retval EFI_DEVICE_ERROR    The reset operation failed.

**/
EFI_STATUS
BpmpI2cReset (
  IN CONST EFI_I2C_MASTER_PROTOCOL *This
  )
{
  return EFI_SUCCESS;
}

/**
  Start an I2C transaction on the host controller.

  This routine must be called at or below TPL_NOTIFY.  For synchronous
  requests this routine must be called at or below TPL_CALLBACK.

  This function initiates an I2C transaction on the controller.  To
  enable proper error handling by the I2C protocol stack, the I2C
  master protocol does not support queuing but instead only manages
  one I2C transaction at a time.  This API requires that the I2C bus
  is in the correct configuration for the I2C transaction.

  The transaction is performed by sending a start-bit and selecting the
  I2C device with the specified I2C slave address and then performing
  the specified I2C operations.  When multiple operations are requested
  they are separated with a repeated start bit and the slave address.
  The transaction is terminated with a stop bit.

  When Event is NULL, StartRequest operates synchronously and returns
  the I2C completion status as its return value.

  When Event is not NULL, StartRequest synchronously returns EFI_SUCCESS
  indicating that the I2C transaction was started asynchronously.  The
  transaction status value is returned in the buffer pointed to by
  I2cStatus upon the completion of the I2C transaction when I2cStatus
  is not NULL.  After the transaction status is returned the Event is
  signaled.

  Note: The typical consumer of this API is the I2C host protocol.
  Extreme care must be taken by other consumers of this API to prevent
  confusing the third party I2C drivers due to a state change at the
  I2C device which the third party I2C drivers did not initiate.  I2C
  platform specific code may use this API within these guidelines.

  @param[in] This           Pointer to an EFI_I2C_MASTER_PROTOCOL structure.
  @param[in] SlaveAddress   Address of the device on the I2C bus.  Set the
                            I2C_ADDRESSING_10_BIT when using 10-bit addresses,
                            clear this bit for 7-bit addressing.  Bits 0-6
                            are used for 7-bit I2C slave addresses and bits
                            0-9 are used for 10-bit I2C slave addresses.
  @param[in] RequestPacket  Pointer to an EFI_I2C_REQUEST_PACKET
                            structure describing the I2C transaction.
  @param[in] Event          Event to signal for asynchronous transactions,
                            NULL for asynchronous transactions
  @param[out] I2cStatus     Optional buffer to receive the I2C transaction
                            completion status

  @retval EFI_SUCCESS           The asynchronous trBPMP_I2C_REQUEST_RESPONSEansaction was successfully
                                started when Event is not NULL.
  @retval EFI_SUCCESS           The transaction completed successfully when
                                Event is NULL.
  @retval EFI_ALREADY_STARTED   The controller is busy with another transaction.
  @retval EFI_BAD_BUFFER_SIZE   The RequestPacket->LengthInBytes value is too
                                large.
  @retval EFI_DEVICE_ERROR      There was an I2C error (NACK) during the
                                transaction.
  @retval EFI_INVALID_PARAMETER RequestPacket is NULL
  @retval EFI_NOT_FOUND         Reserved bit set in the SlaveAddress parameter
  @retval EFI_NO_RESPONSE       The I2C device is not responding to the slave
                                address.  EFI_DEVICE_ERROR will be returned if
                                the controller cannot distinguish when the NACK
                                occurred.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory for I2C transaction
  @retval EFI_UNSUPPORTED       The controller does not support the requested
                                transaction.

**/
EFI_STATUS
BpmpI2cStartRequest (
  IN CONST EFI_I2C_MASTER_PROTOCOL *This,
  IN UINTN                         SlaveAddress,
  IN EFI_I2C_REQUEST_PACKET        *RequestPacket,
  IN EFI_EVENT                     Event      OPTIONAL,
  OUT EFI_STATUS                   *I2cStatus OPTIONAL
  )
{
  NVIDIA_BPMP_I2C_PRIVATE_DATA *Private = NULL;
  EFI_STATUS                   Status;

  if ((This == NULL) ||
      (RequestPacket == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = BPMP_I2C_PRIVATE_DATA_FROM_MASTER(This);

  if (NULL != Private->RequestPacket) {
    return EFI_ALREADY_STARTED;
  }

  Private->SlaveAddress  = SlaveAddress;
  Private->RequestPacket = RequestPacket;
  Private->TransactionEvent = Event;
  if ((NULL == Event) || (NULL == I2cStatus)) {
    Private->TransactionStatus = &Status;
  } else {
    Private->TransactionStatus = I2cStatus;
  }
  Private->TransferInProgress = FALSE;

  if (NULL == Event) {
    BpmpIpcProcess (NULL, (VOID *)Private);
  } else {
    BpmpIpcProcess (Private->BpmpIpcToken.Event, (VOID *)Private);
  }

  if (NULL != Event) {
    return EFI_SUCCESS;
  } else {
    return *Private->TransactionStatus;
  }
}

/**
  Enumerate the I2C devices

  This function enables the caller to traverse the set of I2C devices
  on an I2C bus.

  @param[in]  This              The platform data for the next device on
                                the I2C bus was returned successfully.
  @param[in, out] Device        Pointer to a buffer containing an
                                EFI_I2C_DEVICE structure.  Enumeration is
                                started by setting the initial EFI_I2C_DEVICE
                                structure pointer to NULL.  The buffer
                                receives an EFI_I2C_DEVICE structure pointer
                                to the next I2C device.

  @retval EFI_SUCCESS           The platform data for the next device on
                                the I2C bus was returned successfully.
  @retval EFI_INVALID_PARAMETER Device is NULL
  @retval EFI_NO_MAPPING        *Device does not point to a valid
                                EFI_I2C_DEVICE structure returned in a
                                previous call Enumerate().

**/
EFI_STATUS
BpmpI2cEnumerate (
  IN CONST EFI_I2C_ENUMERATE_PROTOCOL *This,
  IN OUT CONST EFI_I2C_DEVICE         **Device
  )
{
  NVIDIA_BPMP_I2C_PRIVATE_DATA *Private;
  UINTN Index;

  if ((This == NULL) ||
      (Device == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  Private = BPMP_I2C_PRIVATE_DATA_FROM_ENUMERATE(This);

  if (*Device == NULL) {
    Index = 0;
  } else {
    for (Index = 0; Index < Private->NumberOfI2cDevices; Index++) {
      if (&Private->I2cDevices[Index] == *Device) {
        break;
      }
    }
    if (Index == Private->NumberOfI2cDevices) {
      return EFI_NO_MAPPING;
    }
    Index++;
  }
  if (Index == Private->NumberOfI2cDevices) {
    *Device = NULL;
    return EFI_NOT_FOUND;
  }

  *Device = &Private->I2cDevices[Index];
  return EFI_SUCCESS;
}

/**
  Get the requested I2C bus frequency for a specified bus configuration.

  This function returns the requested I2C bus clock frequency for the
  I2cBusConfiguration.  This routine is provided for diagnostic purposes
  and is meant to be called after calling Enumerate to get the
  I2cBusConfiguration value.

  @param[in] This                 Pointer to an EFI_I2C_ENUMERATE_PROTOCOL
                                  structure.
  @param[in] I2cBusConfiguration  I2C bus configuration to access the I2C
                                  device
  @param[out] *BusClockHertz      Pointer to a buffer to receive the I2C
                                  bus clock frequency in Hertz

  @retval EFI_SUCCESS           The I2C bus frequency was returned
                                successfully.
  @retval EFI_INVALID_PARAMETER BusClockHertz was NULL
  @retval EFI_NO_MAPPING        Invalid I2cBusConfiguration value

**/
EFI_STATUS
BpmpI2cGetBusFrequency (
  IN CONST EFI_I2C_ENUMERATE_PROTOCOL *This,
  IN UINTN                            I2cBusConfiguration,
  OUT UINTN                           *BusClockHertz
  )
{
  if (NULL == BusClockHertz) {
    return EFI_INVALID_PARAMETER;
  }
  if (0 != I2cBusConfiguration) {
    return EFI_NO_MAPPING;
  }
  return EFI_UNSUPPORTED;
}

/**
  Enable access to an I2C bus configuration.

  This routine must be called at or below TPL_NOTIFY.  For synchronous
  requests this routine must be called at or below TPL_CALLBACK.

  Reconfigure the switches and multiplexers in the I2C bus to enable
  access to a specific I2C bus configuration.  Also select the maximum
  clock frequency for this I2C bus configuration.

  This routine uses the I2C Master protocol to perform I2C transactions
  on the local bus.  This eliminates any recursion in the I2C stack for
  configuration transactions on the same I2C bus.  This works because the
  local I2C bus is idle while the I2C bus configuration is being enabled.

  If I2C transactions must be performed on other I2C busses, then the
  EFI_I2C_HOST_PROTOCOL, the EFI_I2C_IO_PROTCOL, or a third party I2C
  driver interface for a specific device must be used.  This requirement
  is because the I2C host protocol controls the flow of requests to the
  I2C controller.  Use the EFI_I2C_HOST_PROTOCOL when the I2C device is
  not enumerated by the EFI_I2C_ENUMERATE_PROTOCOL.  Use a protocol
  produced by a third party driver when it is available or the
  EFI_I2C_IO_PROTOCOL when the third party driver is not available but
  the device is enumerated with the EFI_I2C_ENUMERATE_PROTOCOL.

  When Event is NULL, EnableI2cBusConfiguration operates synchronously
  and returns the I2C completion status as its return value.

  @param[in]  This            Pointer to an EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL
                              structure.
  @param[in]  I2cBusConfiguration Index of an I2C bus configuration.  All
                                  values in the range of zero to N-1 are
                                  valid where N is the total number of I2C
                                  bus configurations for an I2C bus.
  @param[in]  Event           Event to signal when the transaction is complete
  @param[out] I2cStatus       Buffer to receive the transaction status.

  @return  When Event is NULL, EnableI2cBusConfiguration operates synchrouously
  and returns the I2C completion status as its return value.  In this case it is
  recommended to use NULL for I2cStatus.  The values returned from
  EnableI2cBusConfiguration are:

  @retval EFI_SUCCESS           The asynchronous bus configuration request
                                was successfully started when Event is not
                                NULL.BpmpIpcEvent
  @retval EFI_SUCCESS           The bus configuration request completed
                                successfully when Event is NULL.
  @retval EFI_DEVICE_ERROR      The bus configuration failed.
  @retval EFI_NO_MAPPING        Invalid I2cBusConfiguration value

**/
EFI_STATUS
BpmpI2cEnableI2cBusConfiguration (
  IN CONST EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL *This,
  IN UINTN                                               I2cBusConfiguration,
  IN EFI_EVENT                                           Event      OPTIONAL,
  IN EFI_STATUS                                          *I2cStatus OPTIONAL
  )
{
  if (I2cBusConfiguration != 0) {
    return EFI_NO_MAPPING;
  }

  if (NULL != Event) {
    if (NULL == I2cStatus) {
      return EFI_INVALID_PARAMETER;
    }
    *I2cStatus = EFI_SUCCESS;
    gBS->SignalEvent (Event);
  }
  return EFI_SUCCESS;
}

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
BpmpI2cSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS            Status;

  //
  // Attempt to open BpmpIpc Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIABpmpIpcProtocolGuid,
                  NULL,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // See if driver has already bound
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiCallerIdGuid,
                  NULL,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    return EFI_ALREADY_STARTED;
  }

  return EFI_SUCCESS;

}

/**
 * Builds list of i2c devices found in device tree
 *
 * @param Private  Pointer to i2c private date
 *
 * @return EFI_SUCCESS - List built
 * @return others      - Failed to enumerate list
 */
EFI_STATUS
BuildI2cDevices (
  IN NVIDIA_BPMP_I2C_PRIVATE_DATA *Private
  )
{
  Private->NumberOfI2cDevices = 0;
  INT32 Node = 0;
  INT32 ParentDepth = fdt_node_depth (Private->DeviceTreeBase, Private->DeviceTreeNodeOffset);
  UINTN Index;
  if (ParentDepth < 0) {
    return EFI_DEVICE_ERROR;
  }

  fdt_for_each_subnode (Node, Private->DeviceTreeBase, Private->DeviceTreeNodeOffset) {
    INT32 ChildDepth = 0;

    ChildDepth = fdt_node_depth (Private->DeviceTreeBase, Node);
    if ((ParentDepth + 1) != ChildDepth) {
      continue;
    }
    Private->NumberOfI2cDevices++;
  }
  if (0 == Private->NumberOfI2cDevices) {
    Private->I2cDevices = NULL;
    Private->SlaveAddressArray = NULL;
    return EFI_SUCCESS;
  }
  Private->I2cDevices = (EFI_I2C_DEVICE *)AllocateZeroPool (sizeof (EFI_I2C_DEVICE) * Private->NumberOfI2cDevices);
  if (NULL == Private->I2cDevices) {
    return EFI_OUT_OF_RESOURCES;
  }
  Private->SlaveAddressArray = (UINT32 *)AllocateZeroPool (sizeof (UINT32) * Private->NumberOfI2cDevices * (1 + BPMP_I2C_ADDL_SLAVES));
  if (NULL == Private->SlaveAddressArray) {
    FreePool (Private->I2cDevices);
    Private->I2cDevices = NULL;
    return EFI_OUT_OF_RESOURCES;
  }

  Index = 0;
  fdt_for_each_subnode (Node, Private->DeviceTreeBase, Private->DeviceTreeNodeOffset) {
    BPMP_I2C_DEVICE_TYPE_MAP *MapEntry = mDeviceTypeMap;
    CONST UINT32             *RegEntry = NULL;
    INT32                    RegLength;
    INT32                    ChildDepth = 0;
    UINTN                    AdditionalSlaves = 0;
    UINTN                    SlaveIndex;

    ChildDepth = fdt_node_depth (Private->DeviceTreeBase, Node);
    if ((ParentDepth + 1) != ChildDepth) {
      continue;
    }

    Private->I2cDevices[Index].DeviceGuid = &gNVIDIAI2cUnknown;
    while (MapEntry->Compatibility != NULL) {
      if (0 == fdt_node_check_compatible (Private->DeviceTreeBase, Node, MapEntry->Compatibility)) {
        DEBUG ((DEBUG_ERROR, "%a: %a detected\r\n", __FUNCTION__, MapEntry->Compatibility));
        Private->I2cDevices[Index].DeviceGuid = MapEntry->DeviceType;
        AdditionalSlaves = MapEntry->AdditionalSlaves;
        break;
      }
      MapEntry++;
    }
    Private->I2cDevices[Index].DeviceIndex = Index;
    Private->I2cDevices[Index].HardwareRevision = 1;
    Private->I2cDevices[Index].I2cBusConfiguration = 0;
    RegEntry = (CONST UINT32*)fdt_getprop (Private->DeviceTreeBase, Node, "reg", &RegLength);
    if ((RegEntry == NULL) || (RegLength != sizeof (UINT32))) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to locate reg property\r\n",__FUNCTION__));
      Private->I2cDevices[Index].SlaveAddressCount = 0;
      Private->I2cDevices[Index].SlaveAddressArray  = NULL;
      break;
    } else {
      Private->I2cDevices[Index].SlaveAddressCount = 1;
      Private->I2cDevices[Index].SlaveAddressArray = &Private->SlaveAddressArray[Index * (1 + BPMP_I2C_ADDL_SLAVES)];
      Private->SlaveAddressArray[Index * (1 + BPMP_I2C_ADDL_SLAVES)] = SwapBytes32 (*RegEntry);
      DEBUG ((DEBUG_ERROR, "%a: Address %02x\r\n", __FUNCTION__, Private->SlaveAddressArray[Index * (1 + BPMP_I2C_ADDL_SLAVES)]));
    }

    for (SlaveIndex = 0; SlaveIndex < AdditionalSlaves; SlaveIndex++) {
      UINTN NewSlave = Private->I2cDevices[Index].SlaveAddressArray[0];
      NewSlave &= MapEntry->SlaveMasks[SlaveIndex][BPMP_I2C_SLAVE_AND];
      NewSlave |= MapEntry->SlaveMasks[SlaveIndex][BPMP_I2C_SLAVE_OR];
      Private->SlaveAddressArray[Index * (1 + BPMP_I2C_ADDL_SLAVES) + SlaveIndex + 1] = NewSlave;
      Private->I2cDevices[Index].SlaveAddressCount++;
    }
    Index++;
  }
  if (Index == Private->NumberOfI2cDevices) {
    return EFI_SUCCESS;
  } else {
    FreePool (Private->I2cDevices);
    Private->I2cDevices = NULL;
    FreePool (Private->SlaveAddressArray);
    Private->SlaveAddressArray = NULL;
    return EFI_DEVICE_ERROR;
  }
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
BpmpI2cStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS                        Status;
  NVIDIA_BPMP_IPC_PROTOCOL          *BpmpIpc = NULL;
  NVIDIA_BPMP_I2C_PRIVATE_DATA      *Private = NULL;
  VOID                              *Interface;
  EFI_DEVICE_PATH_PROTOCOL          *ParentDevicePath = NULL;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode   = NULL;
  CONST UINT32                      *Adapter = NULL;
  INT32                             AdapterLength;

  //
  // Attempt to open BpmpI2c Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIABpmpIpcProtocolGuid,
                  (VOID **) &BpmpIpc,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get BpmpIpc protocol %r\r\n",__FUNCTION__,Status));
    goto ErrorExit;
  }

  //
  // Attempt to open DevicePath Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get device path protocol %r\r\n",__FUNCTION__,Status));
    goto ErrorExit;
  }

  //
  // Attempt to open DevicePath Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIADeviceTreeNodeProtocolGuid,
                  (VOID **) &DeviceTreeNode,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get device tree node protocol %r\r\n",__FUNCTION__,Status));
    goto ErrorExit;
  }

  Private = (NVIDIA_BPMP_I2C_PRIVATE_DATA *)AllocateZeroPool (sizeof (NVIDIA_BPMP_I2C_PRIVATE_DATA));
  if (NULL == Private) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate private data\r\n",__FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Private->ChildDevicePath = AppendDevicePathNode (ParentDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&mDevicePathNode);
  if (NULL == Private->ChildDevicePath) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate device path\r\n",__FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Private->Signature                                      = BPMP_I2C_SIGNATURE;
  Private->I2cMaster.SetBusFrequency                      = BpmpI2cSetBusFrequency;
  Private->I2cMaster.Reset                                = BpmpI2cReset;
  Private->I2cMaster.StartRequest                         = BpmpI2cStartRequest;
  Private->I2cMaster.I2cControllerCapabilities            = &Private->I2cControllerCapabilities;
  Private->I2cControllerCapabilities.MaximumReceiveBytes  = BPMP_I2C_MAX_SIZE;
  Private->I2cControllerCapabilities.MaximumTotalBytes    = BPMP_I2C_MAX_SIZE;
  Private->I2cControllerCapabilities.MaximumTransmitBytes = BPMP_I2C_MAX_SIZE + BPMP_I2C_MAX_SIZE;
  Private->I2cControllerCapabilities.StructureSizeInBytes = sizeof (Private->I2cControllerCapabilities);
  Private->I2cEnumerate.Enumerate                         = BpmpI2cEnumerate;
  Private->I2cEnumerate.GetBusFrequency                   = BpmpI2cGetBusFrequency;
  Private->I2CConfiguration.EnableI2cBusConfiguration     = BpmpI2cEnableI2cBusConfiguration;
  Private->ProtocolsInstalled                             = FALSE;
  Private->Parent                                         = Controller;
  Private->Child                                          = NULL;
  Private->DriverBindingHandle                            = This->DriverBindingHandle;
  Private->BpmpIpc                                        = BpmpIpc;
  Private->DeviceTreeBase                                 = DeviceTreeNode->DeviceTreeBase;
  Private->DeviceTreeNodeOffset = fdt_node_offset_by_compatible (
                                    DeviceTreeNode->DeviceTreeBase,
                                    0,
                                    "nvidia,tegra186-bpmp-i2c"
                                    );
  if (0 == Private->DeviceTreeNodeOffset) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to locate bpmp-i2c device tree node\r\n",__FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto ErrorExit;
  }

  Adapter = (CONST UINT32*)fdt_getprop (Private->DeviceTreeBase, Private->DeviceTreeNodeOffset, "nvidia,bpmp-bus-id", &AdapterLength);
  if ((Adapter == NULL) || (AdapterLength != sizeof (UINT32))) {
    Adapter = (CONST UINT32*)fdt_getprop (Private->DeviceTreeBase, Private->DeviceTreeNodeOffset, "adapter", &AdapterLength);
    if ((Adapter == NULL) || (AdapterLength != sizeof (UINT32))) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to locate adapter property\r\n",__FUNCTION__));
      Status = EFI_NOT_FOUND;
      goto ErrorExit;
    }
  }
  Private->BusId = SwapBytes32 (*Adapter);

  Status = BuildI2cDevices (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to enumerate i2c devices: %r\r\n",__FUNCTION__,Status));
    goto ErrorExit;
  }

  Private->SlaveAddress = 0;
  Private->RequestPacket = NULL;
  Private->TransactionEvent = NULL;
  Private->TransactionStatus = EFI_SUCCESS;

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  BpmpIpcProcess,
                  (VOID *)Private,
                  &Private->BpmpIpcToken.Event);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to create BpmpIpcEvent: %R\r\n",__FUNCTION__,Status));
    goto ErrorExit;
  }

  Private->TransferInProgress = FALSE;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->Child,
                  &gEfiI2cMasterProtocolGuid,
                  &Private->I2cMaster,
                  &gEfiI2cEnumerateProtocolGuid,
                  &Private->I2cEnumerate,
                  &gEfiI2cBusConfigurationManagementProtocolGuid,
                  &Private->I2CConfiguration,
                  &gEfiDevicePathProtocolGuid,
                  Private->ChildDevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to install i2c protocols:%r\r\n",__FUNCTION__,Status));
    goto ErrorExit;
  }

  Private->ProtocolsInstalled = TRUE;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gEfiCallerIdGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to install callerid protocol:%r\r\n",__FUNCTION__,Status));
    goto ErrorExit;
  }

  //
  // Attempt to open caller id Protocol for child
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiCallerIdGuid,
                  (VOID **) &Interface,
                  This->DriverBindingHandle,
                  Private->Child,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed open by child %r\r\n",__FUNCTION__,Status));
    goto ErrorExit;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (NULL != Private) {
      if (Private->ProtocolsInstalled) {
        if (NULL != Private->ChildDevicePath) {
          FreePool (Private->ChildDevicePath);
        }
        if (NULL != Private->I2cDevices) {
          FreePool (Private->I2cDevices);
        }
        if (NULL != Private->SlaveAddressArray) {
          FreePool (Private->SlaveAddressArray);
        }
        gBS->UninstallMultipleProtocolInterfaces (
               Private->Child,
               &gEfiI2cMasterProtocolGuid,
               &Private->I2cMaster,
               gEfiI2cEnumerateProtocolGuid,
               &Private->I2cEnumerate,
               &gEfiI2cBusConfigurationManagementProtocolGuid,
               &Private->I2CConfiguration,
               &gEfiDevicePathProtocolGuid,
               Private->ChildDevicePath,
               NULL
               );
      }
      FreePool (Private);
    }
    gBS->UninstallMultipleProtocolInterfaces (
           Controller,
           &gEfiCallerIdGuid,
           NULL,
           NULL
           );
  }

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
BpmpI2cStop (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN UINTN                          NumberOfChildren,
  IN EFI_HANDLE                     *ChildHandleBuffer
  )
{
  EFI_STATUS                        Status;

  if (NumberOfChildren == 0) {
    //Close controller
    Status = gBS->UninstallMultipleProtocolInterfaces (
                      Controller,
                      &gEfiCallerIdGuid,
                      NULL,
                      NULL
                      );
   return Status;
  } else {
    UINTN Index;
    for (Index = 0; Index < NumberOfChildren; Index++) {
      EFI_I2C_MASTER_PROTOCOL           *I2cMaster = NULL;
      NVIDIA_BPMP_I2C_PRIVATE_DATA      *Private = NULL;

      //
      // Attempt to open I2cMaster Protocol
      //
      Status = gBS->OpenProtocol (
                      ChildHandleBuffer[Index],
                      &gEfiI2cMasterProtocolGuid,
                      (VOID **) &I2cMaster,
                      This->DriverBindingHandle,
                      Controller,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL
                      );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      Private = BPMP_I2C_PRIVATE_DATA_FROM_MASTER(This);
      if (Private == NULL) {
        return EFI_DEVICE_ERROR;
      }
      Status = gBS->CloseProtocol (
                      Controller,
                      &gEfiCallerIdGuid,
                      This->DriverBindingHandle,
                      ChildHandleBuffer[Index]
                      );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = gBS->UninstallMultipleProtocolInterfaces (
                      ChildHandleBuffer[Index],
                      &gEfiI2cMasterProtocolGuid,
                      &Private->I2cMaster,
                      gEfiI2cEnumerateProtocolGuid,
                      &Private->I2cEnumerate,
                      &gEfiI2cBusConfigurationManagementProtocolGuid,
                      &Private->I2CConfiguration,
                      &gEfiDevicePathProtocolGuid,
                      Private->ChildDevicePath,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        return Status;
      }
      FreePool (Private);
    }
  }
  return EFI_SUCCESS;
}

///
/// EFI_DRIVER_BINDING_PROTOCOL instance
///
EFI_DRIVER_BINDING_PROTOCOL mDriverBindingProtocol = {
  BpmpI2cSupported,
  BpmpI2cStart,
  BpmpI2cStop,
  0x0,
  NULL,
  NULL
};

/**
  Initialize the Bpmp I2c Driver

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
BpmpI2cInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  HandleStatus;
  UINTN       NumberOfHandles;
  EFI_HANDLE  *HandleBuffer;

  Status = EfiLibInstallDriverBinding (
             ImageHandle,
             SystemTable,
             &mDriverBindingProtocol,
             ImageHandle);

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to install driver binding protocol: %r\r\n",__FUNCTION__,Status));
    return Status;
  }

  //
  //Look for existing controllers as I2C subsystem is needed for
  //variable support and thus prior to BDS.
  //
  HandleStatus = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gNVIDIABpmpIpcProtocolGuid,
                        NULL,
                        &NumberOfHandles,
                        &HandleBuffer);
  if (!EFI_ERROR (HandleStatus)) {
    EFI_HANDLE DriverHandles[2];
    UINTN Index;

    DriverHandles[0] = ImageHandle;
    DriverHandles[1] = NULL;
    for (Index = 0; Index < NumberOfHandles; Index++) {
      gBS->ConnectController (
             HandleBuffer[Index],
             DriverHandles,
             NULL,
             TRUE
             );
    }
	FreePool (HandleBuffer);
  }

  return Status;
}


