/** @file
  Serial I/O Port wrapper library for StandaloneMm

  SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/TegraSerialPortLib.h>
#include <Library/SerialPortLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/NVIDIADebugLib.h>

STATIC
TEGRA_UART_OBJ  *TegraUartObj = NULL;
STATIC
EFI_PHYSICAL_ADDRESS  SerialBaseAddress = MAX_UINTN;
STATIC
BOOLEAN  SerialPortInitialized = FALSE;

/** Identify the serial device hardware

 **/
VOID
EFIAPI
SerialPortIdentify (
  SERIAL_MAPPING  **SerialMapping OPTIONAL
  )
{
  EFI_STATUS           Status;
  EFI_VIRTUAL_ADDRESS  Base;
  UINTN                Size;

  // Ensure the fallback resource ready
  SetTegraUARTBaseAddress (0);

  NV_ASSERT_RETURN (IsOpteePresent (), return , "%a: not OPTEE!", __FUNCTION__);

  Status = GetDeviceRegion ("combuart-t234", &Base, &Size);
  if (!EFI_ERROR (Status)) {
    TegraUartObj = TegraCombinedSerialPortGetObject ();
  }

  if (TegraUartObj != NULL) {
    SetTegraUARTBaseAddress (Base);
    SerialBaseAddress = Base;
    TegraUartObj->SerialPortInitialize (SerialBaseAddress);
  }
}

/** Initialize the serial device hardware with default settings.

  @retval RETURN_SUCCESS            The serial device was initialised.
  @retval RETURN_INVALID_PARAMETER  One or more of the default settings
                                    has an unsupported value.
 **/
RETURN_STATUS
EFIAPI
SerialPortInitialize (
  VOID
  )
{
  NV_ASSERT_RETURN (IsOpteePresent (), return RETURN_UNSUPPORTED, "%a: not OPTEE!", __FUNCTION__);

  /* For OP-TEE configurations, return SUCCESS, since  this function is
   * called as part of the Library Constructors for Standalone Mm Entry
   * point at which point the Guided Hob list containing the device Mem
   * address mappings haven't been setup..
   */
  SerialPortInitialized = TRUE;
  return RETURN_SUCCESS;
}

/**
  Write data to serial device.

  @param  Buffer           Point of data buffer which need to be written.
  @param  NumberOfBytes    Number of output bytes which are cached in Buffer.

  @retval 0                Write data failed.
  @retval !0               Actual number of bytes written to serial device.

**/
UINTN
EFIAPI
SerialPortWrite (
  IN UINT8  *Buffer,
  IN UINTN  NumberOfBytes
  )
{
  /* Check if the DebugLib's constructor has been called. */
  if (SerialPortInitialized == TRUE) {
    if (TegraUartObj == NULL) {
      SerialPortIdentify (NULL);
      if (TegraUartObj == NULL) {
        return NumberOfBytes;
      }
    }

    return TegraUartObj->SerialPortWrite (
                           SerialBaseAddress,
                           Buffer,
                           NumberOfBytes
                           );
  } else {
    return NumberOfBytes;
  }
}

/**
  Read data from serial device and save the data in buffer.

  @param  Buffer           Point of data buffer which need to be written.
  @param  NumberOfBytes    Number of output bytes which are cached in Buffer.

  @retval 0                Read data failed.
  @retval !0               Actual number of bytes read from serial device.

**/
UINTN
EFIAPI
SerialPortRead (
  OUT UINT8  *Buffer,
  IN  UINTN  NumberOfBytes
  )
{
  /* UART Read isn't supported in StMM */
  return 0;
}

/**
  Check to see if any data is available to be read from the debug device.

  @retval TRUE       At least one byte of data is available to be read
  @retval FALSE      No data is available to be read

**/
BOOLEAN
EFIAPI
SerialPortPoll (
  VOID
  )
{
  if (SerialPortInitialized == TRUE) {
    if (TegraUartObj == NULL) {
      SerialPortIdentify (NULL);
      if (TegraUartObj == NULL) {
        return FALSE;
      }
    }

    return TegraUartObj->SerialPortPoll (SerialBaseAddress);
  } else {
    return FALSE;
  }
}

/**

  Assert or deassert the control signals on a serial port.
  The following control signals are set according their bit settings :
  . Request to Send
  . Data Terminal Ready

  @param[in]  Control  The following bits are taken into account :
                       . EFI_SERIAL_REQUEST_TO_SEND : assert/deassert the
                         "Request To Send" control signal if this bit is
                         equal to one/zero.
                       . EFI_SERIAL_DATA_TERMINAL_READY : assert/deassert
                         the "Data Terminal Ready" control signal if this
                         bit is equal to one/zero.
                       . EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE : enable/disable
                         the hardware loopback if this bit is equal to
                         one/zero.
                       . EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE : not supported.
                       . EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE : enable/
                         disable the hardware flow control based on CTS (Clear
                         To Send) and RTS (Ready To Send) control signals.

  @retval  RETURN_SUCCESS      The new control bits were set on the device.
  @retval  RETURN_UNSUPPORTED  The device does not support this operation.

**/
RETURN_STATUS
EFIAPI
SerialPortSetControl (
  IN UINT32  Control
  )
{
  if (SerialPortInitialized == TRUE) {
    if (TegraUartObj == NULL) {
      SerialPortIdentify (NULL);
      if (TegraUartObj == NULL) {
        return RETURN_SUCCESS;
      }
    }

    return TegraUartObj->SerialPortSetControl (SerialBaseAddress, Control);
  } else {
    return RETURN_SUCCESS;
  }
}

/**

  Retrieve the status of the control bits on a serial device.

  @param[out]  Control  Status of the control bits on a serial device :

                        . EFI_SERIAL_DATA_CLEAR_TO_SEND,
                          EFI_SERIAL_DATA_SET_READY,
                          EFI_SERIAL_RING_INDICATE,
                          EFI_SERIAL_CARRIER_DETECT,
                          EFI_SERIAL_REQUEST_TO_SEND,
                          EFI_SERIAL_DATA_TERMINAL_READY
                          are all related to the DTE (Data Terminal Equipment)
                          and DCE (Data Communication Equipment) modes of
                          operation of the serial device.
                        . EFI_SERIAL_INPUT_BUFFER_EMPTY : equal to one if the
                          receive buffer is empty, 0 otherwise.
                        . EFI_SERIAL_OUTPUT_BUFFER_EMPTY : equal to one if the
                          transmit buffer is empty, 0 otherwise.
                        . EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE : equal to one if
                          the hardware loopback is enabled (the output feeds
                          the receive buffer), 0 otherwise.
                        . EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE : equal to one
                          if a loopback is accomplished by software, else 0.
                        . EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE : equal to
                          one if the hardware flow control based on CTS (Clear
                          To Send) and RTS (Ready To Send) control signals is
                          enabled, 0 otherwise.

  @retval RETURN_SUCCESS  The control bits were read from the device.

**/
RETURN_STATUS
EFIAPI
SerialPortGetControl (
  OUT UINT32  *Control
  )
{
  if (SerialPortInitialized == TRUE) {
    if (TegraUartObj == NULL) {
      SerialPortIdentify (NULL);
      if (TegraUartObj == NULL) {
        return RETURN_SUCCESS;
      }
    }

    return TegraUartObj->SerialPortGetControl (SerialBaseAddress, Control);
  } else {
    return RETURN_SUCCESS;
  }
}

/**
  Set new attributes to PL011.

  @param  BaudRate                The baud rate of the serial device. If the
                                  baud rate is not supported, the speed will
                                  be reduced down to the nearest supported one
                                  and the variable's value will be updated
                                  accordingly.
  @param  ReceiveFifoDepth        The number of characters the device will
                                  buffer on input. If the specified value is
                                  not supported, the variable's value will
                                  be reduced down to the nearest supported one.
  @param  Timeout                 If applicable, the number of microseconds the
                                  device will wait before timing out a Read or
                                  a Write operation.
  @param  Parity                  If applicable, this is the EFI_PARITY_TYPE
                                  that is computed or checked as each character
                                  is transmitted or received. If the device
                                  does not support parity, the value is the
                                  default parity value.
  @param  DataBits                The number of data bits in each character
  @param  StopBits                If applicable, the EFI_STOP_BITS_TYPE number
                                  of stop bits per character. If the device
                                  does not support stop bits, the value is the
                                  default stop bit value.

  @retval EFI_SUCCESS             All attributes were set correctly.
  @retval EFI_INVALID_PARAMETERS  One or more attributes has an unsupported
                                  value.

**/
RETURN_STATUS
EFIAPI
SerialPortSetAttributes (
  IN OUT UINT64              *BaudRate,
  IN OUT UINT32              *ReceiveFifoDepth,
  IN OUT UINT32              *Timeout,
  IN OUT EFI_PARITY_TYPE     *Parity,
  IN OUT UINT8               *DataBits,
  IN OUT EFI_STOP_BITS_TYPE  *StopBits
  )
{
  if (SerialPortInitialized == TRUE) {
    if (TegraUartObj == NULL) {
      SerialPortIdentify (NULL);
      if (TegraUartObj == NULL) {
        return RETURN_SUCCESS;
      }
    }

    return TegraUartObj->SerialPortSetAttributes (
                           SerialBaseAddress,
                           BaudRate,
                           ReceiveFifoDepth,
                           Timeout,
                           Parity,
                           DataBits,
                           StopBits
                           );
  } else {
    return RETURN_SUCCESS;
  }
}
