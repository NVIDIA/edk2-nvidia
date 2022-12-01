/** @file
  Serial driver that layers on top of a Serial Port Library instance.

  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2013-2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/UefiBootServicesTableLib.h>
#include <Library/SerialPortLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraSerialPortLib.h>

#include <TegraUartDxe.h>

/**
  Reset the serial device.

  @param  This              Protocol instance pointer.

  @retval EFI_SUCCESS       The device was reset.
  @retval EFI_DEVICE_ERROR  The serial device could not be reset.

**/
STATIC
EFI_STATUS
EFIAPI
SerialReset (
  IN EFI_SERIAL_IO_PROTOCOL  *This
  )
{
  EFI_STATUS               Status;
  TEGRA_UART_PRIVATE_DATA  *Private;

  Private = SERIAL_TCU_IO_PRIVATE_DATA_FROM_PROTOCOL (This);
  Status  = Private->TegraUartObj->SerialPortInitialize (Private->SerialBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Go set the current attributes
  //
  Status = This->SetAttributes (
                   This,
                   This->Mode->BaudRate,
                   This->Mode->ReceiveFifoDepth,
                   This->Mode->Timeout,
                   (EFI_PARITY_TYPE)This->Mode->Parity,
                   (UINT8)This->Mode->DataBits,
                   (EFI_STOP_BITS_TYPE)This->Mode->StopBits
                   );

  //
  // The serial device may not support some of the attributes. To prevent
  // later failure, always return EFI_SUCCESS when SetAttributes is returning
  // EFI_INVALID_PARAMETER.
  //
  if (Status == EFI_INVALID_PARAMETER) {
    return EFI_SUCCESS;
  }

  return Status;
}

/**
  Sets the baud rate, receive FIFO depth, transmit/receive time out, parity,
  data bits, and stop bits on a serial device.

  @param  This             Protocol instance pointer.
  @param  BaudRate         The requested baud rate. A BaudRate value of 0 will use the the
                           device's default interface speed.
  @param  ReceiveFifoDepth The requested depth of the FIFO on the receive side of the
                           serial interface. A ReceiveFifoDepth value of 0 will use
                           the device's default FIFO depth.
  @param  Timeout          The requested time out for a single character in microseconds.
                           This timeout applies to both the transmit and receive side of the
                           interface. A Timeout value of 0 will use the device's default time
                           out value.
  @param  Parity           The type of parity to use on this serial device. A Parity value of
                           DefaultParity will use the device's default parity value.
  @param  DataBits         The number of data bits to use on the serial device. A DataBits
                           value of 0 will use the device's default data bit setting.
  @param  StopBits         The number of stop bits to use on this serial device. A StopBits
                           value of DefaultStopBits will use the device's default number of
                           stop bits.

  @retval EFI_SUCCESS           The device was reset.
  @retval EFI_INVALID_PARAMETER One or more attributes has an unsupported value.
  @retval EFI_DEVICE_ERROR      The serial device is not functioning correctly.

**/
STATIC
EFI_STATUS
EFIAPI
SerialSetAttributes (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT64                  BaudRate,
  IN UINT32                  ReceiveFifoDepth,
  IN UINT32                  Timeout,
  IN EFI_PARITY_TYPE         Parity,
  IN UINT8                   DataBits,
  IN EFI_STOP_BITS_TYPE      StopBits
  )
{
  EFI_STATUS               Status;
  TEGRA_UART_PRIVATE_DATA  *Private;
  UINT64                   OriginalBaudRate;
  UINT32                   OriginalReceiveFifoDepth;
  UINT32                   OriginalTimeout;
  EFI_PARITY_TYPE          OriginalParity;
  UINT8                    OriginalDataBits;
  EFI_STOP_BITS_TYPE       OriginalStopBits;

  //
  // Preserve the original input values in case
  // SerialPortSetAttributes() updates the input/output
  // parameters even on error.
  //
  OriginalBaudRate         = BaudRate;
  OriginalReceiveFifoDepth = ReceiveFifoDepth;
  OriginalTimeout          = Timeout;
  OriginalParity           = Parity;
  OriginalDataBits         = DataBits;
  OriginalStopBits         = StopBits;

  Private = SERIAL_TCU_IO_PRIVATE_DATA_FROM_PROTOCOL (This);
  Status  = Private->TegraUartObj->SerialPortSetAttributes (Private->SerialBaseAddress, &BaudRate, &ReceiveFifoDepth, &Timeout, &Parity, &DataBits, &StopBits);
  if (EFI_ERROR (Status)) {
    //
    // If it is just to set Timeout value and unsupported is returned,
    // do not return error.
    //
    if ((Status == EFI_UNSUPPORTED) &&
        (This->Mode->Timeout          != OriginalTimeout) &&
        (This->Mode->ReceiveFifoDepth == OriginalReceiveFifoDepth) &&
        (This->Mode->BaudRate         == OriginalBaudRate) &&
        (This->Mode->DataBits         == (UINT32)OriginalDataBits) &&
        (This->Mode->Parity           == (UINT32)OriginalParity) &&
        (This->Mode->StopBits         == (UINT32)OriginalStopBits))
    {
      //
      // Restore to the original input values.
      //
      BaudRate         = OriginalBaudRate;
      ReceiveFifoDepth = OriginalReceiveFifoDepth;
      Timeout          = OriginalTimeout;
      Parity           = OriginalParity;
      DataBits         = OriginalDataBits;
      StopBits         = OriginalStopBits;
      Status           = EFI_SUCCESS;
    } else if ((Status == EFI_INVALID_PARAMETER) || (Status == EFI_UNSUPPORTED)) {
      return EFI_INVALID_PARAMETER;
    } else {
      return EFI_DEVICE_ERROR;
    }
  }

  //
  // Set the Serial I/O mode
  //
  if (ReceiveFifoDepth == 0) {
    This->Mode->ReceiveFifoDepth = PcdGet16 (PcdUartDefaultReceiveFifoDepth);
  } else {
    This->Mode->ReceiveFifoDepth = ReceiveFifoDepth;
  }

  if (Timeout == 0) {
    This->Mode->Timeout = SERIAL_DEFAULT_TIMEOUT;
  } else {
    This->Mode->Timeout = Timeout;
  }

  if (BaudRate == 0) {
    This->Mode->BaudRate = PcdGet64 (PcdUartDefaultBaudRate);
  } else {
    This->Mode->BaudRate = BaudRate;
  }

  if (DataBits == 0) {
    This->Mode->DataBits = (UINT32)PcdGet8 (PcdUartDefaultDataBits);
  } else {
    This->Mode->DataBits = (UINT32)DataBits;
  }

  This->Mode->Parity   = (UINT32)Parity;
  This->Mode->StopBits = (UINT32)StopBits;

  return Status;
}

/**
  Set the control bits on a serial device

  @param  This             Protocol instance pointer.
  @param  Control          Set the bits of Control that are settable.

  @retval EFI_SUCCESS      The new control bits were set on the serial device.
  @retval EFI_UNSUPPORTED  The serial device does not support this operation.
  @retval EFI_DEVICE_ERROR The serial device is not functioning correctly.

**/
STATIC
EFI_STATUS
EFIAPI
SerialSetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT32                  Control
  )
{
  TEGRA_UART_PRIVATE_DATA  *Private;

  Private = SERIAL_TCU_IO_PRIVATE_DATA_FROM_PROTOCOL (This);
  return Private->TegraUartObj->SerialPortSetControl (Private->SerialBaseAddress, Control);
}

/**
  Retrieves the status of the control bits on a serial device

  @param  This              Protocol instance pointer.
  @param  Control           A pointer to return the current Control signals from the serial device.

  @retval EFI_SUCCESS       The control bits were read from the serial device.
  @retval EFI_DEVICE_ERROR  The serial device is not functioning correctly.

**/
STATIC
EFI_STATUS
EFIAPI
SerialGetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  OUT UINT32                 *Control
  )
{
  TEGRA_UART_PRIVATE_DATA  *Private;

  Private = SERIAL_TCU_IO_PRIVATE_DATA_FROM_PROTOCOL (This);
  return Private->TegraUartObj->SerialPortGetControl (Private->SerialBaseAddress, Control);
}

/**
  Writes data to a serial device.

  @param  This              Protocol instance pointer.
  @param  BufferSize        On input, the size of the Buffer. On output, the amount of
                            data actually written.
  @param  Buffer            The buffer of data to write

  @retval EFI_SUCCESS       The data was written.
  @retval EFI_DEVICE_ERROR  The device reported an error.
  @retval EFI_TIMEOUT       The data write was stopped due to a timeout.

**/
STATIC
EFI_STATUS
EFIAPI
SerialWrite (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN               *BufferSize,
  IN VOID                    *Buffer
  )
{
  UINTN                    Count;
  TEGRA_UART_PRIVATE_DATA  *Private;

  Private = SERIAL_TCU_IO_PRIVATE_DATA_FROM_PROTOCOL (This);
  Count   = Private->TegraUartObj->SerialPortWrite (Private->SerialBaseAddress, Buffer, *BufferSize);

  if (Count != *BufferSize) {
    *BufferSize = Count;
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

/**
  Reads data from a serial device.

  @param  This              Protocol instance pointer.
  @param  BufferSize        On input, the size of the Buffer. On output, the amount of
                            data returned in Buffer.
  @param  Buffer            The buffer to return the data into.

  @retval EFI_SUCCESS       The data was read.
  @retval EFI_DEVICE_ERROR  The device reported an error.
  @retval EFI_TIMEOUT       The data write was stopped due to a timeout.

**/
STATIC
EFI_STATUS
EFIAPI
SerialRead (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN               *BufferSize,
  OUT VOID                   *Buffer
  )
{
  UINTN                    Count;
  UINTN                    TimeOut;
  TEGRA_UART_PRIVATE_DATA  *Private;

  Count = 0;

  Private = SERIAL_TCU_IO_PRIVATE_DATA_FROM_PROTOCOL (This);
  while (Count < *BufferSize) {
    TimeOut = 0;
    while (TimeOut < This->Mode->Timeout) {
      if (Private->TegraUartObj->SerialPortPoll (Private->SerialBaseAddress)) {
        break;
      }

      gBS->Stall (10);
      TimeOut += 10;
    }

    if (TimeOut >= This->Mode->Timeout) {
      break;
    }

    Private->TegraUartObj->SerialPortRead (Private->SerialBaseAddress, Buffer, 1);
    Count++;
    Buffer = (VOID *)((UINT8 *)Buffer + 1);
  }

  if (Count != *BufferSize) {
    *BufferSize = Count;
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

/**
  Initialization for the Serial Io.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_SERIAL_IO_PROTOCOL *
EFIAPI
SerialTCUIoInitialize (
  VOID
  )
{
  EFI_STATUS               Status;
  EFI_SERIAL_IO_MODE       *SerialIoMode;
  TEGRA_UART_PRIVATE_DATA  *Private;

  Status = gBS->AllocatePool (EfiBootServicesData, sizeof (EFI_SERIAL_IO_MODE), (VOID **)&SerialIoMode);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  gBS->SetMem (SerialIoMode, sizeof (EFI_SERIAL_IO_MODE), 0);
  SerialIoMode->ControlMask      = 0;
  SerialIoMode->Timeout          = SERIAL_DEFAULT_TIMEOUT;
  SerialIoMode->BaudRate         = PcdGet64 (PcdUartDefaultBaudRate);
  SerialIoMode->ReceiveFifoDepth = PcdGet16 (PcdUartDefaultReceiveFifoDepth);
  SerialIoMode->DataBits         = (UINT32)PcdGet8 (PcdUartDefaultDataBits);
  SerialIoMode->Parity           = (UINT32)PcdGet8 (PcdUartDefaultParity);
  SerialIoMode->StopBits         = (UINT32)PcdGet8 (PcdUartDefaultStopBits);

  Status = gBS->AllocatePool (EfiBootServicesData, sizeof (TEGRA_UART_PRIVATE_DATA), (VOID **)&Private);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  gBS->SetMem (Private, sizeof (TEGRA_UART_PRIVATE_DATA), 0);
  Private->SerialIoMode.Revision      = SERIAL_IO_INTERFACE_REVISION;
  Private->SerialIoMode.Reset         = SerialReset;
  Private->SerialIoMode.SetAttributes = SerialSetAttributes;
  Private->SerialIoMode.SetControl    = SerialSetControl;
  Private->SerialIoMode.GetControl    = SerialGetControl;
  Private->SerialIoMode.Write         = SerialWrite;
  Private->SerialIoMode.Read          = SerialRead;
  Private->SerialIoMode.Mode          = SerialIoMode;
  Private->Signature                  = SERIAL_TCU_IO_SIGNATURE;
  Private->TegraUartObj               = TegraCombinedSerialPortGetObject ();
  Private->SerialBaseAddress          = 0;

  return (EFI_SERIAL_IO_PROTOCOL *)Private;
}
