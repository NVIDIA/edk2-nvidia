/** @file
  Serial I/O Port wrapper library

  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/TegraSerialPortLib.h>
#include <Library/SerialPortLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <libfdt.h>

#include "TegraSerialPortLibPrivate.h"

STATIC
SERIAL_MAPPING  gSerialCompatibilityMap[] = {
  { TEGRA_UART_TYPE_TCU,   TegraCombinedSerialPortGetObject, "nvidia,tegra194-tcu" },
  { TEGRA_UART_TYPE_TCU,   TegraCombinedSerialPortGetObject, "nvidia,tegra186-tcu" },
  { TEGRA_UART_TYPE_SBSA,  TegraSbsaSerialPortGetObject,     "arm,sbsa-uart"       },
  { TEGRA_UART_TYPE_16550, Tegra16550SerialPortGetObject,    "nvidia,tegra20-uart" },
  { TEGRA_UART_TYPE_NONE,  NULL,                             NULL                  },
};

/** Identify the serial device hardware

 **/
STATIC
VOID
GetRawDeviceTreePointer (
  OUT VOID   **DeviceTree,
  OUT UINTN  *DeviceTreeSize
  )
{
  UINT64  DtbBase;
  UINT64  DtbSize;

  DtbBase = GetDTBBaseAddress ();
  ASSERT ((VOID *)DtbBase != NULL);
  DtbSize = fdt_totalsize ((VOID *)DtbBase);
  // DTB Base may not be aligned to page boundary. Add overlay to size.
  DtbSize += (DtbBase & EFI_PAGE_MASK);
  DtbSize  = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (DtbSize));
  // Align DTB Base to page boundary.
  DtbBase &= ~(EFI_PAGE_MASK);

  *DeviceTree     = (VOID *)DtbBase;
  *DeviceTreeSize = (UINTN)DtbSize;
}

/** Identify the serial device hardware

 **/
VOID
EFIAPI
SerialPortIdentify (
  VOID
  )
{
  EFI_STATUS                        Status;
  UINT32                            Handles;
  UINT32                            NumberOfUart;
  SERIAL_MAPPING                    *Mapping;
  BOOLEAN                           UartFound;
  TEGRA_PLATFORM_TYPE               Platform;
  UINT32                            Size;
  VOID                              *DeviceTree;
  UINTN                             DeviceTreeSize;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  RegData;

  // Ensure the fallback resource ready
  SetTegraUARTBaseAddress (0);

  // Get the pointer to the raw DTB and set them to DTB helper prior to using the helper
  GetRawDeviceTreePointer (&DeviceTree, &DeviceTreeSize);
  SetDeviceTreePointer (DeviceTree, DeviceTreeSize);

  UartFound = FALSE;
  Platform  = TegraGetPlatform ();
  for (Mapping = gSerialCompatibilityMap; Mapping->Compatibility != NULL; Mapping++) {
    if ((Platform == TEGRA_PLATFORM_SILICON) &&
        (Mapping->Type == TEGRA_UART_TYPE_16550))
    {
      continue;
    }

    // Only one UART controller is expected
    NumberOfUart = 1;
    Status       = GetMatchingEnabledDeviceTreeNodes (Mapping->Compatibility, &Handles, &NumberOfUart);
    if ((Status == EFI_SUCCESS) || (Status == EFI_BUFFER_TOO_SMALL)) {
      UartFound = TRUE;
      if (Mapping->Type != TEGRA_UART_TYPE_TCU) {
        // Retreive UART register space
        Size   = 1;
        Status = GetDeviceTreeRegisters (Handles, &RegData, &Size);
        if (EFI_ERROR (Status)) {
          return;
        }

        Mapping->BaseAddress = RegData.BaseAddress;
        // Update UART base address
        SetTegraUARTBaseAddress (Mapping->BaseAddress);
      }

      Mapping->IsFound = TRUE;
    }
  }

  if (UartFound == FALSE) {
    return;
  }

  // Zero initialize to help the DTB helper get them from the HOB list
  SetDeviceTreePointer (NULL, 0);

  return;
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
  RETURN_STATUS   Status;
  SERIAL_MAPPING  *Mapping;

  SerialPortIdentify ();

  for (Mapping = gSerialCompatibilityMap; Mapping->Compatibility != NULL; Mapping++) {
    if (Mapping->IsFound == TRUE) {
      Status = Mapping->GetObject ()->SerialPortInitialize (Mapping->BaseAddress);
      if (RETURN_ERROR (Status)) {
        return RETURN_DEVICE_ERROR;
      }
    }
  }

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
  RETURN_STATUS   Status;
  SERIAL_MAPPING  *Mapping;

  for (Mapping = gSerialCompatibilityMap; Mapping->Compatibility != NULL; Mapping++) {
    if (Mapping->IsFound == TRUE) {
      Status = Mapping->GetObject ()->SerialPortWrite (Mapping->BaseAddress, Buffer, NumberOfBytes);
      if (RETURN_ERROR (Status)) {
        return 0;
      }
    }
  }

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
SerialPortRead (
  OUT UINT8  *Buffer,
  IN  UINTN  NumberOfBytes
  )
{
  SERIAL_MAPPING  *Mapping;

  for (Mapping = gSerialCompatibilityMap; Mapping->Compatibility != NULL; Mapping++) {
    if (Mapping->IsFound == TRUE) {
      break;
    }
  }

  if (Mapping->Compatibility == NULL) {
    return 0;
  }

  return Mapping->GetObject ()->SerialPortRead (Mapping->BaseAddress, Buffer, NumberOfBytes);
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
  SERIAL_MAPPING  *Mapping;

  for (Mapping = gSerialCompatibilityMap; Mapping->Compatibility != NULL; Mapping++) {
    if (Mapping->IsFound == TRUE) {
      break;
    }
  }

  if (Mapping->Compatibility == NULL) {
    return FALSE;
  }

  return Mapping->GetObject ()->SerialPortPoll (Mapping->BaseAddress);
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
  SERIAL_MAPPING  *Mapping;

  for (Mapping = gSerialCompatibilityMap; Mapping->Compatibility != NULL; Mapping++) {
    if (Mapping->IsFound == TRUE) {
      break;
    }
  }

  if (Mapping->Compatibility == NULL) {
    return RETURN_DEVICE_ERROR;
  }

  return Mapping->GetObject ()->SerialPortSetControl (Mapping->BaseAddress, Control);
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
  SERIAL_MAPPING  *Mapping;

  for (Mapping = gSerialCompatibilityMap; Mapping->Compatibility != NULL; Mapping++) {
    if (Mapping->IsFound == TRUE) {
      break;
    }
  }

  if (Mapping->Compatibility == NULL) {
    return RETURN_DEVICE_ERROR;
  }

  return Mapping->GetObject ()->SerialPortGetControl (Mapping->BaseAddress, Control);
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
  SERIAL_MAPPING  *Mapping;

  for (Mapping = gSerialCompatibilityMap; Mapping->Compatibility != NULL; Mapping++) {
    if (Mapping->IsFound == TRUE) {
      break;
    }
  }

  if (Mapping->Compatibility == NULL) {
    return RETURN_DEVICE_ERROR;
  }

  return Mapping->GetObject ()->SerialPortSetAttributes (
                                  Mapping->BaseAddress,
                                  BaudRate,
                                  ReceiveFifoDepth,
                                  Timeout,
                                  Parity,
                                  DataBits,
                                  StopBits
                                  );
}
