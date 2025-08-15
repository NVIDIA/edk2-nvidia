/** @file
  Gpio Boot Complete Dxe

  This driver toggles a GPIO pin on ReadyToBoot event to signal
  boot completion to the BMC.

  SPDX-FileCopyrightText: Copyright (c) 2025-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/NVIDIADebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Protocol/EmbeddedGpio.h>
#include <Guid/EventGroup.h>
#include <libfdt.h>

EMBEDDED_GPIO      *mGpio;
EMBEDDED_GPIO_PIN  mGpioPin;
EFI_EVENT          mReadyToBootEvent;

/**
  Notification function for ReadyToBoot event.
  This function toggles the GPIO pin to signal boot completion to BMC.

  @param  Event                 Event whose notification function is being invoked.
  @param  Context               Pointer to the notification function's context.

**/
VOID
EFIAPI
GpioBootCompleteNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "%a: ReadyToBoot event triggered, signaling boot complete to BMC\n", __FUNCTION__));

  // Set GPIO to high to signal boot completion
  Status = mGpio->Set (mGpio, mGpioPin, GPIO_MODE_OUTPUT_1);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set boot complete GPIO high. Status = %r\n", __FUNCTION__, Status));
    return;
  }

  DEBUG ((DEBUG_INFO, "%a: Boot complete signal sent to BMC via GPIO\n", __FUNCTION__));

  // Close the event as we only need to signal once
  gBS->CloseEvent (Event);
}

/**
  Entrypoint of Gpio Boot Complete Dxe.

  @param  ImageHandle           The firmware allocated handle for the EFI image.
  @param  SystemTable           A pointer to the EFI System Table.

  @return EFI_SUCCESS           The operation completed successfully.
  @return EFI_NOT_FOUND         No gpio-boot-complete node found in device tree.
  @return EFI_DEVICE_ERROR      GPIO configuration error.
  @return Others                Other errors from underlying functions.

**/
EFI_STATUS
EFIAPI
GpioBootCompleteDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  UINT32              NumGpioBootCompleteNodes;
  UINT32              GpioBootCompleteHandle;
  VOID                *Dtb;
  INT32               NodeOffset;
  CONST VOID          *Property;
  CONST UINT32        *Data;
  UINT32              GpioControllerPhandle;
  UINT32              GpioNum;
  EMBEDDED_GPIO_MODE  GpioMode;

  DEBUG ((DEBUG_INFO, "%a: Initializing GPIO Boot Complete driver\n", __FUNCTION__));

  // Look for gpio-boot-complete device tree node
  NumGpioBootCompleteNodes = 1;
  Status                   = GetMatchingEnabledDeviceTreeNodes ("gpio-boot-complete", &GpioBootCompleteHandle, &NumGpioBootCompleteNodes);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "%a: No gpio-boot-complete node found in device tree, exiting\n", __FUNCTION__));
    return EFI_SUCCESS;
  } else if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: System cannot have more than 1 gpio-boot-complete node.\n", __FUNCTION__));
    return Status;
  }

  // Get device tree node information
  Status = GetDeviceTreeNode (GpioBootCompleteHandle, &Dtb, &NodeOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get gpio boot complete dtb node information.\n", __FUNCTION__));
    return Status;
  }

  // Get GPIO information from device tree
  Property = NULL;
  Property = fdt_getprop (Dtb, NodeOffset, "gpios", NULL);
  if (Property == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get gpio information from boot complete dtb node.\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  // Parse GPIO specification: <controller_phandle gpio_number flags>
  Data                  = (CONST UINT32 *)Property;
  GpioControllerPhandle = SwapBytes32 (Data[0]);
  GpioNum               = SwapBytes32 (Data[1]);

  DEBUG ((DEBUG_INFO, "%a: GPIO controller phandle: 0x%x, GPIO number: %u\n", __FUNCTION__, GpioControllerPhandle, GpioNum));

  // Locate GPIO protocol
  Status = gBS->LocateProtocol (&gEmbeddedGpioProtocolGuid, NULL, (VOID **)&mGpio);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get embedded gpio protocol. Status = %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Create GPIO pin identifier
  mGpioPin = GPIO (GpioControllerPhandle, GpioNum);

  // Verify GPIO mode
  Status = mGpio->GetMode (mGpio, mGpioPin, &GpioMode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get mode of boot complete gpio. Status = %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (GpioMode != GPIO_MODE_OUTPUT_0) {
    DEBUG ((DEBUG_ERROR, "%a: Boot Complete GPIO mode not configured correctly: %d\n", __FUNCTION__, GpioMode));
    return EFI_DEVICE_ERROR;
  }

  // Create ReadyToBoot event
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  GpioBootCompleteNotify,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &mReadyToBootEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create ReadyToBoot event: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: GPIO Boot Complete driver initialized successfully\n", __FUNCTION__));

  return EFI_SUCCESS;
}
