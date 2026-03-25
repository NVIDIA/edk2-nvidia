/** @file
  Fastboot Utility Library implementation.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/FastbootUtilityLib.h>
#include <Protocol/UsbIo.h>
#include <Protocol/DevicePath.h>

//
// Fastboot Key Combo Configuration
//
#define FASTBOOT_KEY_COMBO_WAIT_US           (500 * 1000) // 500 ms max polling time
#define FASTBOOT_KEY_COMBO_POLL_INTERVAL_US  (10 * 1000)  // 10 ms
#define FASTBOOT_KEY_COMBO_MAX_RETRIES       (FASTBOOT_KEY_COMBO_WAIT_US / FASTBOOT_KEY_COMBO_POLL_INTERVAL_US)

//
// Number of poll cycles to wait before giving up if no candidate
// USB HID device has been observed (keeps the cold-boot path fast).
//
#define FASTBOOT_KEY_COMBO_EARLY_EXIT_RETRIES  5

//
// USB HID Definitions
//
#define USB_CLASS_HID          3
#define USB_SUBCLASS_BOOT      1
#define USB_PROTOCOL_KEYBOARD  1

#define USB_REQ_TYPE_SET_IDLE    0x21
#define USB_REQ_SET_IDLE         0x0A
#define USB_REQ_TYPE_GET_REPORT  0xA1
#define USB_REQ_GET_REPORT       0x01
#define USB_REPORT_TYPE_INPUT    1

#define USB_ENDPOINT_INTERRUPT  0x03
#define USB_ENDPOINT_DIR_IN     0x80

//
// NVIDIA Thunderstrike Definitions
//
#define NVIDIA_VENDOR_ID              0x0955
#define THUNDERSTRIKE_PRODUCT_ID      0x7214
#define THUNDERSTRIKE_REPORT_ID       0x01
#define THUNDERSTRIKE_BUTTON_A_BIT    BIT1
#define THUNDERSTRIKE_BUTTON_B_BIT    BIT0
#define THUNDERSTRIKE_BUTTON_AB_MASK  (THUNDERSTRIKE_BUTTON_A_BIT | THUNDERSTRIKE_BUTTON_B_BIT)

//
// USB Keyboard Definitions
//
#define KEYBOARD_USAGE_ID_A  0x04
#define KEYBOARD_USAGE_ID_B  0x05

//
// USB transfer parameters
//
#define USB_TRANSFER_TIMEOUT_MS  10
#define USB_KEYBOARD_REPORT_LEN  8
#define USB_HID_REPORT_BUF_LEN   64

//
// Explicitly connect any USB device to force the USB bus to enumerate
// all devices, including gamepads like Thunderstrike.
//
typedef struct {
  USB_CLASS_DEVICE_PATH       UsbClass;
  EFI_DEVICE_PATH_PROTOCOL    End;
} USB_CLASS_FORMAT_DEVICE_PATH;

STATIC USB_CLASS_FORMAT_DEVICE_PATH  mUsbAny = {
  {
    {
      MESSAGING_DEVICE_PATH,
      MSG_USB_CLASS_DP,
      {
        (UINT8)(sizeof (USB_CLASS_DEVICE_PATH)),
        (UINT8)((sizeof (USB_CLASS_DEVICE_PATH)) >> 8)
      }
    },
    0xffff, // VendorId: any
    0xffff, // ProductId: any
    0xff,   // DeviceClass: any
    0xff,   // DeviceSubClass: any
    0xff    // DeviceProtocol: any
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(sizeof (EFI_DEVICE_PATH_PROTOCOL)),
      (UINT8)((sizeof (EFI_DEVICE_PATH_PROTOCOL)) >> 8)
    }
  }
};

/**
  Returns the first IN-direction interrupt endpoint on the given interface.

  @param[in]  UsbIo                The USB I/O Protocol instance.
  @param[in]  InterfaceDescriptor  The USB Interface Descriptor.
  @param[out] EndpointAddress      Receives the endpoint address (0 if none).
  @param[out] MaxPacketSize        Receives the endpoint max packet size.

  @retval TRUE   An interrupt-IN endpoint was found.
  @retval FALSE  No suitable endpoint exists on this interface.
**/
STATIC
BOOLEAN
GetInterruptInEndpoint (
  IN  EFI_USB_IO_PROTOCOL           *UsbIo,
  IN  EFI_USB_INTERFACE_DESCRIPTOR  *InterfaceDescriptor,
  OUT UINT8                         *EndpointAddress,
  OUT UINTN                         *MaxPacketSize
  )
{
  EFI_USB_ENDPOINT_DESCRIPTOR  EndpointDescriptor;
  UINT8                        Ep;

  *EndpointAddress = 0;
  *MaxPacketSize   = 0;

  for (Ep = 0; Ep < InterfaceDescriptor->NumEndpoints; Ep++) {
    if (EFI_ERROR (UsbIo->UsbGetEndpointDescriptor (UsbIo, Ep, &EndpointDescriptor))) {
      continue;
    }

    if (((EndpointDescriptor.Attributes & USB_ENDPOINT_INTERRUPT) == USB_ENDPOINT_INTERRUPT) &&
        ((EndpointDescriptor.EndpointAddress & USB_ENDPOINT_DIR_IN) != 0))
    {
      *EndpointAddress = EndpointDescriptor.EndpointAddress;
      *MaxPacketSize   = EndpointDescriptor.MaxPacketSize;
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Checks if the A+B key combination is pressed on a USB Keyboard.

  Tries the HID GET_REPORT control transfer first; if that fails, falls
  back to a one-shot interrupt-IN read.

  @param[in] UsbIo                The USB I/O Protocol instance.
  @param[in] InterfaceDescriptor  The USB Interface Descriptor.

  @retval TRUE   The A+B key combination was detected.
  @retval FALSE  The A+B key combination was not detected.
**/
STATIC
BOOLEAN
CheckKeyboardFastbootCombo (
  IN EFI_USB_IO_PROTOCOL           *UsbIo,
  IN EFI_USB_INTERFACE_DESCRIPTOR  *InterfaceDescriptor
  )
{
  EFI_STATUS              Status;
  EFI_USB_DEVICE_REQUEST  Request;
  UINT32                  TransferResult;
  UINT8                   Report[USB_KEYBOARD_REPORT_LEN];
  UINTN                   Index;
  BOOLEAN                 GotA = FALSE;
  BOOLEAN                 GotB = FALSE;

  ZeroMem (Report, sizeof (Report));

  Request.RequestType = USB_REQ_TYPE_GET_REPORT;
  Request.Request     = USB_REQ_GET_REPORT;
  Request.Value       = (USB_REPORT_TYPE_INPUT << 8) | 0;
  Request.Index       = InterfaceDescriptor->InterfaceNumber;
  Request.Length      = USB_KEYBOARD_REPORT_LEN;

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbDataIn,
                    USB_TRANSFER_TIMEOUT_MS,
                    Report,
                    Request.Length,
                    &TransferResult
                    );

  if (EFI_ERROR (Status) || (TransferResult != EFI_USB_NOERROR)) {
    UINT8  InterruptEndpoint;
    UINTN  MaxPacketSize;
    UINTN  DataLength;
    UINT8  BigReport[USB_HID_REPORT_BUF_LEN];

    if (!GetInterruptInEndpoint (UsbIo, InterfaceDescriptor, &InterruptEndpoint, &MaxPacketSize)) {
      return FALSE;
    }

    DataLength = MaxPacketSize;
    ZeroMem (BigReport, sizeof (BigReport));

    Status = UsbIo->UsbSyncInterruptTransfer (
                      UsbIo,
                      InterruptEndpoint,
                      BigReport,
                      &DataLength,
                      USB_TRANSFER_TIMEOUT_MS,
                      &TransferResult
                      );

    if (!EFI_ERROR (Status) || (Status == EFI_TIMEOUT)) {
      CopyMem (Report, BigReport, MIN (DataLength, USB_KEYBOARD_REPORT_LEN));
    }
  }

  if ((!EFI_ERROR (Status) || (Status == EFI_TIMEOUT)) && (TransferResult == EFI_USB_NOERROR)) {
    // Bytes 2-7 of an HID boot keyboard report contain pressed Usage IDs.
    for (Index = 2; Index < USB_KEYBOARD_REPORT_LEN; Index++) {
      if (Report[Index] == KEYBOARD_USAGE_ID_A) {
        GotA = TRUE;
      }

      if (Report[Index] == KEYBOARD_USAGE_ID_B) {
        GotB = TRUE;
      }
    }
  }

  return GotA && GotB;
}

/**
  Checks if the A+B key combination is pressed on a NVIDIA Thunderstrike controller.

  Thunderstrike does not respond to HID GET_REPORT, so we exclusively use
  the interrupt-IN endpoint. We send SET_IDLE(0) once on the first call so
  the controller streams reports without coalescing.

  @param[in] UsbIo                The USB I/O Protocol instance.
  @param[in] InterfaceDescriptor  The USB Interface Descriptor.

  @retval TRUE   The A+B key combination was detected.
  @retval FALSE  The A+B key combination was not detected.
**/
STATIC
BOOLEAN
CheckThunderstrikeFastbootCombo (
  IN EFI_USB_IO_PROTOCOL           *UsbIo,
  IN EFI_USB_INTERFACE_DESCRIPTOR  *InterfaceDescriptor
  )
{
  EFI_STATUS              Status;
  EFI_USB_DEVICE_REQUEST  Request;
  UINT32                  TransferResult;
  UINT8                   Report[USB_HID_REPORT_BUF_LEN];
  UINTN                   DataLength = 0;
  UINT8                   InterruptEndpoint;
  UINTN                   MaxPacketSize;
  STATIC BOOLEAN          TsInitialized = FALSE;

  if (!TsInitialized) {
    Request.RequestType = USB_REQ_TYPE_SET_IDLE;
    Request.Request     = USB_REQ_SET_IDLE;
    Request.Value       = 0;
    Request.Index       = InterfaceDescriptor->InterfaceNumber;
    Request.Length      = 0;
    UsbIo->UsbControlTransfer (
             UsbIo,
             &Request,
             EfiUsbNoData,
             USB_TRANSFER_TIMEOUT_MS,
             NULL,
             0,
             &TransferResult
             );
    TsInitialized = TRUE;
  }

  if (!GetInterruptInEndpoint (UsbIo, InterfaceDescriptor, &InterruptEndpoint, &MaxPacketSize)) {
    return FALSE;
  }

  DataLength = MaxPacketSize;
  ZeroMem (Report, sizeof (Report));

  Status = UsbIo->UsbSyncInterruptTransfer (
                    UsbIo,
                    InterruptEndpoint,
                    Report,
                    &DataLength,
                    USB_TRANSFER_TIMEOUT_MS,
                    &TransferResult
                    );

  if ((EFI_ERROR (Status) && (Status != EFI_TIMEOUT)) ||
      ((TransferResult != EFI_USB_NOERROR) && (DataLength == 0)))
  {
    return FALSE;
  }

  if (DataLength == 0) {
    return FALSE;
  }

  //
  // Thunderstrike report layout:
  //   With report ID:    Byte 0 = 0x01, Byte 3 = button bitmask
  //   Without report ID: Byte 2 = button bitmask
  //
  if (Report[0] == THUNDERSTRIKE_REPORT_ID) {
    return ((Report[3] & THUNDERSTRIKE_BUTTON_AB_MASK) == THUNDERSTRIKE_BUTTON_AB_MASK);
  }

  return ((Report[2] & THUNDERSTRIKE_BUTTON_AB_MASK) == THUNDERSTRIKE_BUTTON_AB_MASK);
}

/**
  Returns TRUE if the interface looks like an HID boot-protocol USB keyboard.
**/
STATIC
BOOLEAN
IsBootKeyboardInterface (
  IN EFI_USB_INTERFACE_DESCRIPTOR  *InterfaceDescriptor
  )
{
  return ((InterfaceDescriptor->InterfaceClass    == USB_CLASS_HID) &&
          (InterfaceDescriptor->InterfaceSubClass == USB_SUBCLASS_BOOT) &&
          (InterfaceDescriptor->InterfaceProtocol == USB_PROTOCOL_KEYBOARD));
}

/**
  Returns TRUE if the device + interface look like the NVIDIA Thunderstrike HID interface.
**/
STATIC
BOOLEAN
IsThunderstrikeHidInterface (
  IN EFI_USB_DEVICE_DESCRIPTOR     *DeviceDescriptor,
  IN EFI_USB_INTERFACE_DESCRIPTOR  *InterfaceDescriptor
  )
{
  return ((DeviceDescriptor->IdVendor      == NVIDIA_VENDOR_ID)         &&
          (DeviceDescriptor->IdProduct     == THUNDERSTRIKE_PRODUCT_ID) &&
          (InterfaceDescriptor->InterfaceClass == USB_CLASS_HID));
}

/**
  Polls connected USB HID devices for the Fastboot entry key combination.

  See FastbootUtilityLib.h for the contract.
**/
BOOLEAN
EFIAPI
CheckFastbootKeyCombo (
  VOID
  )
{
  EFI_STATUS                    Status;
  UINTN                         HandleCount;
  EFI_HANDLE                    *HandleBuffer;
  UINTN                         Index;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  EFI_USB_DEVICE_DESCRIPTOR     DeviceDescriptor;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;
  UINT32                        RetryCount     = 0;
  BOOLEAN                       ComboDetected  = FALSE;
  BOOLEAN                       AnyDeviceFound = FALSE;

  DEBUG ((
    DEBUG_INFO,
    "%a: Starting USB HID polling, MaxRetries=%u\n",
    __FUNCTION__,
    FASTBOOT_KEY_COMBO_MAX_RETRIES
    ));

  //
  // Force USB enumeration so newly attached gamepads/keyboards are visible.
  //
  EfiBootManagerConnectDevicePath ((EFI_DEVICE_PATH_PROTOCOL *)&mUsbAny, NULL);

  while (RetryCount < FASTBOOT_KEY_COMBO_MAX_RETRIES) {
    AnyDeviceFound = FALSE;
    HandleBuffer   = NULL;

    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEfiUsbIoProtocolGuid,
                    NULL,
                    &HandleCount,
                    &HandleBuffer
                    );

    if (EFI_ERROR (Status)) {
      if (RetryCount > FASTBOOT_KEY_COMBO_EARLY_EXIT_RETRIES) {
        DEBUG ((DEBUG_INFO, "%a: No USB devices found, exiting early.\n", __FUNCTION__));
        break;
      }

      gBS->Stall (FASTBOOT_KEY_COMBO_POLL_INTERVAL_US);
      RetryCount++;
      continue;
    }

    for (Index = 0; Index < HandleCount; Index++) {
      Status = gBS->HandleProtocol (
                      HandleBuffer[Index],
                      &gEfiUsbIoProtocolGuid,
                      (VOID **)&UsbIo
                      );
      if (EFI_ERROR (Status)) {
        continue;
      }

      Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &InterfaceDescriptor);
      if (EFI_ERROR (Status)) {
        continue;
      }

      if (IsBootKeyboardInterface (&InterfaceDescriptor)) {
        AnyDeviceFound = TRUE;
        if (CheckKeyboardFastbootCombo (UsbIo, &InterfaceDescriptor)) {
          ComboDetected = TRUE;
          break;
        }

        continue;
      }

      Status = UsbIo->UsbGetDeviceDescriptor (UsbIo, &DeviceDescriptor);
      if (EFI_ERROR (Status)) {
        continue;
      }

      if (IsThunderstrikeHidInterface (&DeviceDescriptor, &InterfaceDescriptor)) {
        AnyDeviceFound = TRUE;
        if (CheckThunderstrikeFastbootCombo (UsbIo, &InterfaceDescriptor)) {
          ComboDetected = TRUE;
          break;
        }
      }
    }

    if (HandleBuffer != NULL) {
      gBS->FreePool (HandleBuffer);
      HandleBuffer = NULL;
    }

    if (ComboDetected) {
      DEBUG ((DEBUG_ERROR, "%a: A+B key combo detected via USB HID!\n", __FUNCTION__));
      break;
    }

    //
    // No keyboard / gamepad observed for a while -> bail out so we
    // do not extend the boot path on systems without an HID device.
    //
    if (!AnyDeviceFound && (RetryCount > FASTBOOT_KEY_COMBO_EARLY_EXIT_RETRIES)) {
      DEBUG ((DEBUG_INFO, "%a: No USB Keyboard/Gamepad found, exiting early.\n", __FUNCTION__));
      break;
    }

    gBS->Stall (FASTBOOT_KEY_COMBO_POLL_INTERVAL_US);
    RetryCount++;
  }

  return ComboDetected;
}
