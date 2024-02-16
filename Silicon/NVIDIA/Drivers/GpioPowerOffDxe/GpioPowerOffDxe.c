/** @file
  Gpio Power Off Dxe

  SPDX-FileCopyrightText: Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/NVIDIADebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Protocol/EmbeddedGpio.h>
#include <libfdt.h>

#define GPIO_POWER_OFF_POLL_INTERVAL  1000

EMBEDDED_GPIO      *Gpio;
EMBEDDED_GPIO_PIN  GpioPin;
EFI_EVENT          TimerEvent;

VOID
EFIAPI
GpioPowerOffTimerNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS           Status;
  UINTN                GpioValue;
  STATIC CONST CHAR16  ResetString[] = L"System power off requested via GPIO.";

  Status = Gpio->Get (Gpio, GpioPin, &GpioValue);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get value of power off gpio. Status = %r\n", __func__, Status));
  }

  if (GpioValue == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Shutdown requested via power off gpio.\n", __func__));
    Status = Gpio->Set (Gpio, GpioPin, GPIO_MODE_OUTPUT_0);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set value of power off gpio. Status = %r\n", __func__, Status));
    }

    gRT->ResetSystem (
           EfiResetShutdown,
           EFI_SUCCESS,
           StrSize (ResetString),
           (CHAR16 *)ResetString
           );
  }
}

/**
  Entrypoint of Gpio Power Off Dxe.

  @param  ImageHandle
  @param  SystemTable

  @return EFI_SUCCESS
  @return EFI_LOAD_ERROR
  @return EFI_OUT_OF_RESOURCES

**/
EFI_STATUS
EFIAPI
GpioPowerOffDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  UINT32              NumGpioPwrOffNodes;
  UINT32              GpioPwrOffHandle;
  VOID                *Dtb;
  INT32               NodeOffset;
  CONST VOID          *Property;
  CONST UINT32        *Data;
  UINT32              GpioControllerPhandle;
  UINT32              GpioNum;
  EMBEDDED_GPIO_MODE  GpioMode;
  UINT64              TimerTick;

  NumGpioPwrOffNodes = 1;
  Status             = GetMatchingEnabledDeviceTreeNodes ("gpio-poweroff", &GpioPwrOffHandle, &NumGpioPwrOffNodes);
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  } else if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: System cannot have more than 1 gpio-poweroff nodes.\n", __func__));
    return Status;
  }

  Status = GetDeviceTreeNode (GpioPwrOffHandle, &Dtb, &NodeOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get gpio poweroff dtb node information.\n", __func__));
    return Status;
  }

  Property = NULL;
  Property = fdt_getprop (Dtb, NodeOffset, "gpios", NULL);
  if (Property == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get gpio information from uphy configuration dtb node.\n", __func__));
    return EFI_NOT_FOUND;
  }

  Data                  = (CONST UINT32 *)Property;
  GpioControllerPhandle = SwapBytes32 (Data[0]);
  GpioNum               = SwapBytes32 (Data[1]);

  Status = gBS->LocateProtocol (&gEmbeddedGpioProtocolGuid, NULL, (VOID **)&Gpio);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get embedded gpio protocol. Status = %r\n", __func__, Status));
    return Status;
  }

  GpioPin = GPIO (GpioControllerPhandle, GpioNum);

  Status = Gpio->GetMode (Gpio, GpioPin, &GpioMode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get mode of power off gpio. Status = %r\n", __func__, Status));
    return Status;
  }

  if (GpioMode != GPIO_MODE_INPUT) {
    DEBUG ((DEBUG_ERROR, "%a: Power Off GPIO mode not configured correctly: %d\n", __func__, GpioMode));
    return EFI_DEVICE_ERROR;
  }

  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  GpioPowerOffTimerNotify,
                  NULL,
                  &TimerEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create timer event: %r\n", __func__, Status));
    return Status;
  }

  TimerTick = GPIO_POWER_OFF_POLL_INTERVAL;
  Status    = gBS->SetTimer (
                     TimerEvent,
                     TimerPeriodic,
                     TimerTick
                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set timer:%r\n", __func__, Status));
  }

  return Status;
}
