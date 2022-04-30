/** @file
  Serial I/O Port library functions for Combined UART in StMM.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

#include <Library/TegraSerialPortLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>

typedef struct {
  UINT8   Data[3];
  UINT8   NumberOfBytes:2;
  BOOLEAN Flush:1;
  BOOLEAN HwFlush:1;
  UINT8   Reserved:3;
  BOOLEAN Interrupt:1;
} TEGRA_COMBINED_UART_PIO;

typedef union {
  UINT32                  RawValue;
  TEGRA_COMBINED_UART_PIO Pio;
} TEGRA_COMBINED_UART;

/**
  Check to see if any data is currently pending on the mailbox.

  @param  MailboxAddress Address of the mailbox to check

  @retval TRUE       At least one byte of data is present
  @retval FALSE      No data is present

**/
STATIC
BOOLEAN
EFIAPI
IsDataPresent (
  UINTN MailboxAddress
  )
{
  TEGRA_COMBINED_UART CombinedUartData;

  CombinedUartData.RawValue = MmioRead32 (MailboxAddress);

  return CombinedUartData.Pio.Interrupt;
}

/** Initialise the serial device hardware with default settings.

  @retval RETURN_SUCCESS            The serial device was initialised.
  @retval RETURN_INVALID_PARAMETER  One or more of the default settings
                                    has an unsupported value.
 **/
RETURN_STATUS
EFIAPI
TegraCombinedSerialPortInitialize (
  IN UINTN SerialBaseAddress
  )
{
  TEGRA_COMBINED_UART CombinedUartData;
  UINTN               TxMailbox;

  TxMailbox = SerialBaseAddress;

  //Wait until all prior data is sent
  while (IsDataPresent(TxMailbox) == TRUE);

  MmioWrite32 (TxMailbox, 0);

  CombinedUartData.Pio.Data[0] = '\n';
  CombinedUartData.Pio.NumberOfBytes = 1;
  CombinedUartData.Pio.Flush = TRUE;
  CombinedUartData.Pio.HwFlush = TRUE;
  CombinedUartData.Pio.Interrupt = TRUE;

  MmioWrite32 (TxMailbox, CombinedUartData.RawValue);

  //Wait until new data is sent
  while (IsDataPresent(TxMailbox) == TRUE);

  return EFI_SUCCESS;
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
TegraCombinedSerialPortWrite (
  IN UINTN     SerialBaseAddress,
  IN UINT8     *Buffer,
  IN UINTN     NumberOfBytes
  )
{
  UINT8*              Final;
  UINTN               TxMailbox;
  TEGRA_COMBINED_UART CombinedUartData;

  Final = &Buffer[NumberOfBytes];
  TxMailbox = SerialBaseAddress;
  CombinedUartData.RawValue = 0;

  while (Buffer < Final) {
    //Wait until all prior data is sent
    while (IsDataPresent(TxMailbox) == TRUE);

    CombinedUartData.Pio.NumberOfBytes = 0;
    CombinedUartData.Pio.Reserved = 0;
    CombinedUartData.Pio.Flush = TRUE;

    while ((Buffer < Final) &&
           (CombinedUartData.Pio.NumberOfBytes < 3)) {
      CombinedUartData.Pio.Data[CombinedUartData.Pio.NumberOfBytes] = *Buffer;
      CombinedUartData.Pio.NumberOfBytes++;
      Buffer++;
    }

    CombinedUartData.Pio.Interrupt = TRUE;

    MmioWrite32 (TxMailbox, CombinedUartData.RawValue);

    //Wait until new data is sent
    while (IsDataPresent(TxMailbox) == TRUE);
  };

  return NumberOfBytes;
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
TegraCombinedSerialPortRead (
  IN  UINTN     SerialBaseAddress,
  OUT UINT8     *Buffer,
  IN  UINTN     NumberOfBytes
)
{
  return 0;
}

/**
  Check to see if any data is available to be read from the debug device.

  @retval TRUE       At least one byte of data is available to be read
  @retval FALSE      No data is available to be read

**/
BOOLEAN
EFIAPI
TegraCombinedSerialPortPoll (
  IN UINTN SerialBaseAddress
  )
{
  return FALSE;
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
TegraCombinedSerialPortSetControl (
  IN UINTN   SerialBaseAddress,
  IN UINT32  Control
  )
{
  return EFI_UNSUPPORTED;
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
TegraCombinedSerialPortGetControl (
  IN  UINTN   SerialBaseAddress,
  OUT UINT32  *Control
  )
{
  UINTN TxMailbox = SerialBaseAddress;
  if (NULL == Control) {
    return EFI_INVALID_PARAMETER;
  }

  *Control = 0;
  if (IsDataPresent (TxMailbox) == FALSE) {
    *Control |= EFI_SERIAL_OUTPUT_BUFFER_EMPTY;
  }

  return EFI_SUCCESS;
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
TegraCombinedSerialPortSetAttributes (
  IN     UINTN               SerialBaseAddress,
  IN OUT UINT64              *BaudRate,
  IN OUT UINT32              *ReceiveFifoDepth,
  IN OUT UINT32              *Timeout,
  IN OUT EFI_PARITY_TYPE     *Parity,
  IN OUT UINT8               *DataBits,
  IN OUT EFI_STOP_BITS_TYPE  *StopBits
  )
{
  return EFI_UNSUPPORTED;
}

TEGRA_UART_OBJ TegraCombinedUart = {
  TegraCombinedSerialPortInitialize,
  TegraCombinedSerialPortWrite,
  TegraCombinedSerialPortRead,
  TegraCombinedSerialPortPoll,
  TegraCombinedSerialPortSetControl,
  TegraCombinedSerialPortGetControl,
  TegraCombinedSerialPortSetAttributes
};

/**

  Retrieve the object of tegra combined serial port library.

  @param[out]  Tegra combined uart library object

**/
TEGRA_UART_OBJ *
EFIAPI
TegraCombinedSerialPortGetObject (
  VOID
  )
{
  return &TegraCombinedUart;
}
