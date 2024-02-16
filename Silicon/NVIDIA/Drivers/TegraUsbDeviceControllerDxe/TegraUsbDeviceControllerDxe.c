/** @file

  Tegra USB Device controller

  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <IndustryStandard/Usb.h>
#include <Protocol/UsbDevice.h>

EFI_STATUS
EFIAPI
TegraUsbDeviceStart (
  IN USB_DEVICE_DESCRIPTOR   *DeviceDescriptor,
  IN VOID                    **Descriptors,
  IN USB_DEVICE_RX_CALLBACK  RxCallback,
  IN USB_DEVICE_TX_CALLBACK  TxCallback
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;

  return Status;
}

EFI_STATUS
TegraUsbDeviceSend (
  IN        UINT8  EndpointIndex,
  IN        UINTN  Size,
  IN  CONST VOID   *Buffer
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;

  return Status;
}

USB_DEVICE_PROTOCOL  mTegraUsbDevice = {
  TegraUsbDeviceStart,
  TegraUsbDeviceSend
};

EFI_STATUS
TegraUsbDeviceControllerEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gUsbDeviceProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mTegraUsbDevice
                  );
  return Status;
}
