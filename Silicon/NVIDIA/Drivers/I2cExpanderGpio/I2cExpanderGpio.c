/** @file

  I2C GPIO expander Driver

  Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DeviceTreeHelperLib.h>

#include <Protocol/EmbeddedGpio.h>
#include <Protocol/I2cIo.h>

#define GPIO_PER_CONTROLLER  16

typedef struct {
  UINT32                      NumberOfControllers;
  PLATFORM_GPIO_CONTROLLER    PlatformGpioController;
  VOID                        *I2cIoSearchToken;
  EFI_I2C_IO_PROTOCOL         **I2cIoArray;
  EMBEDDED_GPIO               EmbeddedGpio;
} I2C_EXPANDER_DATA;

I2C_EXPANDER_DATA  mI2cExpanderData;

#define TCA9539_INPUT_BASE   0x0
#define TCA9539_OUTPUT_BASE  0x2
#define TCA9539_CONFIG_BASE  0x6

///
/// I2C device request
///
/// The EFI_I2C_REQUEST_PACKET describes a single I2C transaction.  The
/// transaction starts with a start bit followed by the first operation
/// in the operation array.  Subsequent operations are separated with
/// repeated start bits and the last operation is followed by a stop bit
/// which concludes the transaction.  Each operation is described by one
/// of the elements in the Operation array.
///
typedef struct {
  ///
  /// Number of elements in the operation array
  ///
  UINTN                OperationCount;

  ///
  /// Description of the I2C operation
  ///
  EFI_I2C_OPERATION    Operation[2];
} I2C_REQUEST_PACKET_2_OPS;

STATIC
EFI_STATUS
GetGpioController (
  IN  EMBEDDED_GPIO_PIN    Gpio,
  OUT EFI_I2C_IO_PROTOCOL  **I2cController
  )
{
  UINTN   Index;
  UINT32  Controller = GPIO_PORT (Gpio);

  if (I2cController == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < mI2cExpanderData.NumberOfControllers; Index++) {
    if (mI2cExpanderData.I2cIoArray[Index]->DeviceIndex == Controller) {
      *I2cController = mI2cExpanderData.I2cIoArray[Index];
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
 * Gets the state of a GPIO pin
 *
 * @param[in]  This       pointer to protocol
 * @param[in]  Gpio       which pin to read
 * @param[out] Value      state of the pin
 *
 * @return EFI_SUCCESS - GPIO state returned in Value
 */
EFI_STATUS
GetGpioState (
  IN  EMBEDDED_GPIO      *This,
  IN  EMBEDDED_GPIO_PIN  Gpio,
  OUT UINTN              *Value
  )
{
  EFI_STATUS                Status;
  EFI_I2C_IO_PROTOCOL       *I2cIo;
  I2C_REQUEST_PACKET_2_OPS  RequestData;
  EFI_I2C_REQUEST_PACKET    *RequestPacket = (EFI_I2C_REQUEST_PACKET *)&RequestData;
  UINT16                    Pin            = GPIO_PIN (Gpio);
  UINT8                     Address;
  UINT8                     Data;

  if ((NULL == This) || (NULL == Value)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetGpioController (Gpio, &I2cIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Pin >= GPIO_PER_CONTROLLER) {
    return EFI_NOT_FOUND;
  }

  Address                                = TCA9539_INPUT_BASE + (Pin / 8);
  RequestData.OperationCount             = 2;
  RequestData.Operation[0].Buffer        = (VOID *)&Address;
  RequestData.Operation[0].LengthInBytes = sizeof (Address);
  RequestData.Operation[0].Flags         = 0;
  RequestData.Operation[1].Buffer        = (VOID *)&Data;
  RequestData.Operation[1].LengthInBytes = sizeof (Data);
  RequestData.Operation[1].Flags         = I2C_FLAG_READ;
  Status                                 = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get input register: %r.\r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  *Value = (Data >> (Pin % 8)) & 0x1;
  return EFI_SUCCESS;
}

/**
 * Sets the state of a GPIO pin
 *
 * @param[in]  This       pointer to protocol
 * @param[in]  Gpio       which pin to modify
 * @param[in]  Mode       mode to set
 *
 * @return EFI_SUCCESS - GPIO set as requested
 */
EFI_STATUS
SetGpioState (
  IN EMBEDDED_GPIO       *This,
  IN EMBEDDED_GPIO_PIN   Gpio,
  IN EMBEDDED_GPIO_MODE  Mode
  )
{
  EFI_STATUS                Status;
  EFI_I2C_IO_PROTOCOL       *I2cIo;
  I2C_REQUEST_PACKET_2_OPS  RequestData;
  EFI_I2C_REQUEST_PACKET    *RequestPacket = (EFI_I2C_REQUEST_PACKET *)&RequestData;
  UINT16                    Pin            = GPIO_PIN (Gpio);
  UINT8                     Address;
  UINT8                     Config;
  UINT8                     Data;
  BOOLEAN                   UpdateData;
  UINT8                     WriteData[2];

  if (NULL == This) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetGpioController (Gpio, &I2cIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Pin >= GPIO_PER_CONTROLLER) {
    return EFI_NOT_FOUND;
  }

  Address                                = TCA9539_CONFIG_BASE + (Pin / 8);
  RequestData.OperationCount             = 2;
  RequestData.Operation[0].Buffer        = (VOID *)&Address;
  RequestData.Operation[0].LengthInBytes = sizeof (Address);
  RequestData.Operation[0].Flags         = 0;
  RequestData.Operation[1].Buffer        = (VOID *)&Config;
  RequestData.Operation[1].LengthInBytes = sizeof (Config);
  RequestData.Operation[1].Flags         = I2C_FLAG_READ;
  Status                                 = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get config register: %r.\r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  Address                                = TCA9539_OUTPUT_BASE + (Pin / 8);
  RequestData.OperationCount             = 2;
  RequestData.Operation[0].Buffer        = (VOID *)&Address;
  RequestData.Operation[0].LengthInBytes = sizeof (Address);
  RequestData.Operation[0].Flags         = 0;
  RequestData.Operation[1].Buffer        = (VOID *)&Data;
  RequestData.Operation[1].LengthInBytes = sizeof (Data);
  RequestData.Operation[1].Flags         = I2C_FLAG_READ;
  Status                                 = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get config register: %r.\r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  switch (Mode) {
    case GPIO_MODE_INPUT:
      UpdateData = FALSE;
      Config    |= (1 << (Pin % 8));
      break;

    case GPIO_MODE_OUTPUT_1:
      UpdateData = TRUE;
      Config    &= ~(1 << (Pin % 8));
      Data      |= (1 << (Pin % 8));
      break;

    case GPIO_MODE_OUTPUT_0:
      UpdateData = TRUE;
      Config    &= ~(1 << (Pin % 8));
      Data      &= ~(1 << (Pin % 8));
      break;

    default:
      return EFI_UNSUPPORTED;
  }

  WriteData[0]                           = TCA9539_CONFIG_BASE + (Pin / 8);
  WriteData[1]                           = Config;
  RequestData.OperationCount             = 1;
  RequestData.Operation[0].Buffer        = (VOID *)&WriteData;
  RequestData.Operation[0].LengthInBytes = sizeof (WriteData);
  RequestData.Operation[0].Flags         = 0;
  Status                                 = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set config register: %r.\r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  if (UpdateData) {
    WriteData[0]                           = TCA9539_OUTPUT_BASE + (Pin / 8);
    WriteData[1]                           = Data;
    RequestData.OperationCount             = 1;
    RequestData.Operation[0].Buffer        = (VOID *)&WriteData;
    RequestData.Operation[0].LengthInBytes = sizeof (WriteData);
    RequestData.Operation[0].Flags         = 0;
    Status                                 = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set output register: %r.\r\n", __FUNCTION__, Status));
      return EFI_DEVICE_ERROR;
    }
  }

  return EFI_SUCCESS;
}

/**
 * Gets the mode (function) of a GPIO pin
 *
 * @param[in]  This       pointer to protocol
 * @param[in]  Gpio       which pin
 * @param[out] Mode       pointer to output mode value
 *
 * @return EFI_SUCCESS - mode value retrieved
 */
EFI_STATUS
GetGpioMode (
  IN  EMBEDDED_GPIO       *This,
  IN  EMBEDDED_GPIO_PIN   Gpio,
  OUT EMBEDDED_GPIO_MODE  *Mode
  )
{
  EFI_STATUS                Status;
  EFI_I2C_IO_PROTOCOL       *I2cIo;
  I2C_REQUEST_PACKET_2_OPS  RequestData;
  EFI_I2C_REQUEST_PACKET    *RequestPacket = (EFI_I2C_REQUEST_PACKET *)&RequestData;
  UINT16                    Pin            = GPIO_PIN (Gpio);
  UINT8                     Address;
  UINT8                     Config;
  UINT8                     Data;

  if ((NULL == This) || (NULL == Mode)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetGpioController (Gpio, &I2cIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Pin >= GPIO_PER_CONTROLLER) {
    return EFI_NOT_FOUND;
  }

  Address                                = TCA9539_CONFIG_BASE + (Pin / 8);
  RequestData.OperationCount             = 2;
  RequestData.Operation[0].Buffer        = (VOID *)&Address;
  RequestData.Operation[0].LengthInBytes = sizeof (Address);
  RequestData.Operation[0].Flags         = 0;
  RequestData.Operation[1].Buffer        = (VOID *)&Config;
  RequestData.Operation[1].LengthInBytes = sizeof (Config);
  RequestData.Operation[1].Flags         = I2C_FLAG_READ;
  Status                                 = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get config register: %r.\r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  Address                                = TCA9539_OUTPUT_BASE + (Pin / 8);
  RequestData.OperationCount             = 2;
  RequestData.Operation[0].Buffer        = (VOID *)&Address;
  RequestData.Operation[0].LengthInBytes = sizeof (Address);
  RequestData.Operation[0].Flags         = 0;
  RequestData.Operation[1].Buffer        = (VOID *)&Data;
  RequestData.Operation[1].LengthInBytes = sizeof (Data);
  RequestData.Operation[1].Flags         = I2C_FLAG_READ;
  Status                                 = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get config register: %r.\r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  if ((Config & (1 << (Pin % 8))) != 0) {
    *Mode = GPIO_MODE_INPUT;
  } else if ((Data & (1 << (Pin % 8))) != 0) {
    *Mode = GPIO_MODE_OUTPUT_1;
  } else {
    *Mode = GPIO_MODE_OUTPUT_0;
  }

  return EFI_SUCCESS;
}

/**
 * Sets the pull-up / pull-down resistor of a GPIO pin
 *
 * @param[in]  This       pointer to protocol
 * @param[in]  Gpio       which pin
 * @param[in]  Direction  pull-up, pull-down, or none
 *
 * @return EFI_SUCCESS - pin was set
 */
EFI_STATUS
SetGpioPull (
  IN  EMBEDDED_GPIO       *This,
  IN  EMBEDDED_GPIO_PIN   Gpio,
  IN  EMBEDDED_GPIO_PULL  Direction
  )
{
  return EFI_UNSUPPORTED;
}

STATIC CONST EMBEDDED_GPIO  mGpioEmbeddedProtocol = {
  .Get     = GetGpioState,
  .Set     = SetGpioState,
  .GetMode = GetGpioMode,
  .SetPull = SetGpioPull
};

/**
 * Install GPIO Protocols
 */
STATIC
VOID
InstallI2cExpanderProtocols (
  VOID
  )
{
  EFI_HANDLE  ImageHandle = 0;

  gBS->InstallMultipleProtocolInterfaces (
         &ImageHandle,
         &gNVIDIAI2cExpanderGpioProtocolGuid,
         &mGpioEmbeddedProtocol,
         &gNVIDIAI2cExpanderPlatformGpioProtocolGuid,
         &mI2cExpanderData.PlatformGpioController,
         NULL
         );
}

/**
 * Notification when i2cio protocol is installed
 * @param Event   - Event that is notified
 * @param Context - Context that was present when registed.
 */
STATIC
VOID
I2cIoProtocolReady (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS           Status = EFI_SUCCESS;
  EFI_I2C_IO_PROTOCOL  *I2cIoProtocol;
  UINT32               Index;

  while (!EFI_ERROR (Status)) {
    Status = gBS->LocateProtocol (
                    &gEfiI2cIoProtocolGuid,
                    mI2cExpanderData.I2cIoSearchToken,
                    (VOID **)&I2cIoProtocol
                    );
    if (EFI_ERROR (Status)) {
      return;
    }

    if (CompareGuid (I2cIoProtocol->DeviceGuid, &gNVIDIAI2cTca9539)) {
      Index                                                                           = mI2cExpanderData.PlatformGpioController.GpioControllerCount;
      mI2cExpanderData.I2cIoArray[Index]                                              = I2cIoProtocol;
      mI2cExpanderData.PlatformGpioController.GpioController[Index].GpioIndex         = GPIO (I2cIoProtocol->DeviceIndex, 0);
      mI2cExpanderData.PlatformGpioController.GpioController[Index].RegisterBase      = 0;
      mI2cExpanderData.PlatformGpioController.GpioController[Index].InternalGpioCount = GPIO_PER_CONTROLLER;
      mI2cExpanderData.PlatformGpioController.GpioControllerCount++;

      if (mI2cExpanderData.NumberOfControllers == mI2cExpanderData.PlatformGpioController.GpioControllerCount) {
        gBS->CloseEvent (Event);
        InstallI2cExpanderProtocols ();
        return;
      }
    }
  }
}

/**
  The user Entry Point for module. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some errors occur when executing this entry point.
**/
EFI_STATUS
EFIAPI
InitializeI2cExpanderGpio (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   I2cIoReadyEvent;

  I2cIoReadyEvent = NULL;
  ZeroMem (&mI2cExpanderData, sizeof (mI2cExpanderData));

  Status = GetMatchingEnabledDeviceTreeNodes ("ti,tca9539", NULL, &mI2cExpanderData.NumberOfControllers);
  if (Status == EFI_NOT_FOUND) {
    mI2cExpanderData.NumberOfControllers = 0;
    InstallI2cExpanderProtocols ();
    Status = EFI_SUCCESS;
  } else if (Status == EFI_BUFFER_TOO_SMALL) {
    Status                                                 = EFI_SUCCESS;
    mI2cExpanderData.PlatformGpioController.GpioController = (GPIO_CONTROLLER *)AllocateZeroPool (sizeof (GPIO_CONTROLLER) * mI2cExpanderData.NumberOfControllers);
    mI2cExpanderData.I2cIoArray                            = (EFI_I2C_IO_PROTOCOL **)AllocateZeroPool (sizeof (EFI_I2C_IO_PROTOCOL *) * mI2cExpanderData.NumberOfControllers);

    if ((mI2cExpanderData.PlatformGpioController.GpioController != NULL) &&
        (mI2cExpanderData.I2cIoArray != NULL))
    {
      I2cIoReadyEvent = EfiCreateProtocolNotifyEvent (
                          &gEfiI2cIoProtocolGuid,
                          TPL_CALLBACK,
                          I2cIoProtocolReady,
                          NULL,
                          &mI2cExpanderData.I2cIoSearchToken
                          );
      if (I2cIoReadyEvent == NULL) {
        DEBUG ((DEBUG_ERROR, "%a, Failed to create I2cIo notification event\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
      }
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate I2CExpander structures\r\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
    }
  }

  if (EFI_ERROR (Status)) {
    if (I2cIoReadyEvent != NULL) {
      gBS->CloseEvent (I2cIoReadyEvent);
    }

    if (mI2cExpanderData.PlatformGpioController.GpioController != NULL) {
      FreePool (mI2cExpanderData.PlatformGpioController.GpioController);
      mI2cExpanderData.PlatformGpioController.GpioController = NULL;
    }

    if (mI2cExpanderData.I2cIoArray != NULL) {
      FreePool (mI2cExpanderData.I2cIoArray);
      mI2cExpanderData.I2cIoArray = NULL;
    }
  }

  return Status;
}
