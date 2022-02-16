/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __TEGRA_SERIAL_PORT_LIB_H__
#define __TEGRA_SERIAL_PORT_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/SerialIo.h>

/**
  Initialize the serial device hardware.

  If no initialization is required, then return RETURN_SUCCESS.
  If the serial device was successfully initialized, then return RETURN_SUCCESS.
  If the serial device could not be initialized, then return RETURN_DEVICE_ERROR.

  @retval RETURN_SUCCESS        The serial device was initialized.
  @retval RETURN_DEVICE_ERROR   The serial device could not be initialized.

**/
typedef
RETURN_STATUS
(EFIAPI * SERIAL_PORT_INITIALIZE) (
  IN UINTN SerialBaseAddress
  );

/**
  Write data from buffer to serial device.

  Writes NumberOfBytes data bytes from Buffer to the serial device.
  The number of bytes actually written to the serial device is returned.
  If the return value is less than NumberOfBytes, then the write operation failed.
  If Buffer is NULL, then ASSERT().
  If NumberOfBytes is zero, then return 0.

  @param  Buffer           Pointer to the data buffer to be written.
  @param  NumberOfBytes    Number of bytes to written to the serial device.

  @retval 0                NumberOfBytes is 0.
  @retval >0               The number of bytes written to the serial device.
                           If this value is less than NumberOfBytes, then the write operation failed.

**/
typedef
UINTN
(EFIAPI * SERIAL_PORT_WRITE) (
  IN UINTN     SerialBaseAddress,
  IN UINT8     *Buffer,
  IN UINTN     NumberOfBytes
  );

/**
  Read data from serial device and save the datas in buffer.

  Reads NumberOfBytes data bytes from a serial device into the buffer
  specified by Buffer. The number of bytes actually read is returned.
  If the return value is less than NumberOfBytes, then the rest operation failed.
  If Buffer is NULL, then ASSERT().
  If NumberOfBytes is zero, then return 0.

  @param  Buffer           Pointer to the data buffer to store the data read from the serial device.
  @param  NumberOfBytes    Number of bytes which will be read.

  @retval 0                Read data failed, no data is to be read.
  @retval >0               Actual number of bytes read from serial device.

**/
typedef
UINTN
(EFIAPI * SERIAL_PORT_READ) (
  IN  UINTN   SerialBaseAddress,
  OUT UINT8   *Buffer,
  IN  UINTN   NumberOfBytes
  );

/**
  Polls a serial device to see if there is any data waiting to be read.

  Polls a serial device to see if there is any data waiting to be read.
  If there is data waiting to be read from the serial device, then TRUE is returned.
  If there is no data waiting to be read from the serial device, then FALSE is returned.

  @retval TRUE             Data is waiting to be read from the serial device.
  @retval FALSE            There is no data waiting to be read from the serial device.

**/
typedef
BOOLEAN
(EFIAPI * SERIAL_PORT_POLL) (
  IN UINTN SerialBaseAddress
  );

/**
  Sets the control bits on a serial device.

  @param Control                Sets the bits of Control that are settable.

  @retval RETURN_SUCCESS        The new control bits were set on the serial device.
  @retval RETURN_UNSUPPORTED    The serial device does not support this operation.
  @retval RETURN_DEVICE_ERROR   The serial device is not functioning correctly.

**/
typedef
RETURN_STATUS
(EFIAPI * SERIAL_PORT_SET_CONTROL) (
  IN UINTN  SerialBaseAddress,
  IN UINT32 Control
  );

/**
  Retrieve the status of the control bits on a serial device.

  @param Control                A pointer to return the current control signals from the serial device.

  @retval RETURN_SUCCESS        The control bits were read from the serial device.
  @retval RETURN_UNSUPPORTED    The serial device does not support this operation.
  @retval RETURN_DEVICE_ERROR   The serial device is not functioning correctly.

**/
typedef
RETURN_STATUS
(EFIAPI * SERIAL_PORT_GET_CONTROL) (
  IN  UINTN  SerialBaseAddress,
  OUT UINT32 *Control
  );

/**
  Sets the baud rate, receive FIFO depth, transmit/receice time out, parity,
  data bits, and stop bits on a serial device.

  @param BaudRate           The requested baud rate. A BaudRate value of 0 will use the
                            device's default interface speed.
                            On output, the value actually set.
  @param ReveiveFifoDepth   The requested depth of the FIFO on the receive side of the
                            serial interface. A ReceiveFifoDepth value of 0 will use
                            the device's default FIFO depth.
                            On output, the value actually set.
  @param Timeout            The requested time out for a single character in microseconds.
                            This timeout applies to both the transmit and receive side of the
                            interface. A Timeout value of 0 will use the device's default time
                            out value.
                            On output, the value actually set.
  @param Parity             The type of parity to use on this serial device. A Parity value of
                            DefaultParity will use the device's default parity value.
                            On output, the value actually set.
  @param DataBits           The number of data bits to use on the serial device. A DataBits
                            vaule of 0 will use the device's default data bit setting.
                            On output, the value actually set.
  @param StopBits           The number of stop bits to use on this serial device. A StopBits
                            value of DefaultStopBits will use the device's default number of
                            stop bits.
                            On output, the value actually set.

  @retval RETURN_SUCCESS            The new attributes were set on the serial device.
  @retval RETURN_UNSUPPORTED        The serial device does not support this operation.
  @retval RETURN_INVALID_PARAMETER  One or more of the attributes has an unsupported value.
  @retval RETURN_DEVICE_ERROR       The serial device is not functioning correctly.

**/
typedef
RETURN_STATUS
(EFIAPI * SERIAL_PORT_SET_ATTRIBUTES) (
  IN     UINTN              SerialBaseAddress,
  IN OUT UINT64             *BaudRate,
  IN OUT UINT32             *ReceiveFifoDepth,
  IN OUT UINT32             *Timeout,
  IN OUT EFI_PARITY_TYPE    *Parity,
  IN OUT UINT8              *DataBits,
  IN OUT EFI_STOP_BITS_TYPE *StopBits
  );

typedef struct {
  SERIAL_PORT_INITIALIZE     SerialPortInitialize;
  SERIAL_PORT_WRITE          SerialPortWrite;
  SERIAL_PORT_READ           SerialPortRead;
  SERIAL_PORT_POLL           SerialPortPoll;
  SERIAL_PORT_SET_CONTROL    SerialPortSetControl;
  SERIAL_PORT_GET_CONTROL    SerialPortGetControl;
  SERIAL_PORT_SET_ATTRIBUTES SerialPortSetAttributes;
  } TEGRA_UART_OBJ;

/**
  Retrieve the object of tegra SBSA serial port library.

  @param[out]  Tegra SBSA uart library object

**/
TEGRA_UART_OBJ *
EFIAPI
TegraSbsaSerialPortGetObject (
  VOID
  );

/**
  Retrieve the object of tegra combined serial port library.

  @param[out]  Tegra combined uart library object

**/
TEGRA_UART_OBJ *
EFIAPI
TegraCombinedSerialPortGetObject (
  VOID
  );

/**
  Retrieve the object of tegra 16650 serial port library.

  @param[out]  Tegra 16650 uart library object

**/
TEGRA_UART_OBJ *
EFIAPI
Tegra16550SerialPortGetObject (
  VOID
  );

/**
  Initialize SBSA Serial Console

**/
EFI_SERIAL_IO_PROTOCOL *
EFIAPI
SerialSbsaIoInitialize (
  IN UINTN SerialBaseAddress
   );

/**
  Initialize 16550 Serial Console

**/
EFI_SERIAL_IO_PROTOCOL *
EFIAPI
Serial16550IoInitialize (
  IN UINTN SerialBaseAddress
  );

/**
  Initialize TCU Serial Console

**/
EFI_SERIAL_IO_PROTOCOL *
EFIAPI
SerialTCUIoInitialize (
  VOID
  );

#endif //__TEGRA_SERIAL_PORT_LIB_H__
