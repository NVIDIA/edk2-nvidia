/** @file

  TegraUart Controller Driver

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra20-uart", &gNVIDIANonDiscoverableUartDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA uart driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = TRUE,
    .AutoResetModule = TRUE,
    .SkipEdkiiNondiscoverableInstall = FALSE
};

typedef struct {
  EFI_PHYSICAL_ADDRESS BaseAddress;
  UINT32               RegisterStride;
  EFI_EVENT            OnExitEvent;
  EFI_HANDLE           ControllerHandle;
} TEGRA_UART_PRIVATE_DATA;

#define UART_CLOCK_NAME "serial"
#define UART_CLOCK_RATE (115200 * 16)

#define R_UART_BAUD_LOW       0   // LCR_DLAB = 1
#define R_UART_BAUD_HIGH      1   // LCR_DLAB = 1
#define R_UART_LCR            3
#define B_UART_LCR_DLAB       BIT7

STATIC
VOID
EFIAPI
NotifyExitBootServices (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_STATUS Status;
  UINT8      Lcr;

  TEGRA_UART_PRIVATE_DATA *Private = (TEGRA_UART_PRIVATE_DATA *)Context;
  Status = DeviceDiscoverySetClockFreq (Private->ControllerHandle, UART_CLOCK_NAME, UART_CLOCK_RATE);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to set clock frequency %r\r\n", __FUNCTION__, Status));
    return;
  }

  //
  // Configure baud rate, divisor should be 1 now
  //
  Lcr = MmioRead8 (Private->BaseAddress + R_UART_LCR * Private->RegisterStride);
  MmioWrite8 (Private->BaseAddress + R_UART_LCR * Private->RegisterStride, Lcr | B_UART_LCR_DLAB);
  MmioWrite8 (Private->BaseAddress + R_UART_BAUD_HIGH * Private->RegisterStride, 0);
  MmioWrite8 (Private->BaseAddress + R_UART_BAUD_LOW * Private->RegisterStride, 1);
  MmioWrite8 (Private->BaseAddress + R_UART_LCR * Private->RegisterStride, Lcr);
}

/**
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to the device tree node protocol is available.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_SUCCESS              Driver does not handle this phase
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryNotify (
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES         Phase,
  IN  EFI_HANDLE                             DriverHandle,
  IN  EFI_HANDLE                             ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                Status;
  EFI_PHYSICAL_ADDRESS      BaseAddress  = 0;
  UINTN                     RegionSize;
  TEGRA_UART_PRIVATE_DATA   *Private;

  switch (Phase) {
  case DeviceDiscoveryDriverBindingSupported:
    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }
   if (PcdGet64 (PcdSerialRegisterBase) != BaseAddress) {
     return EFI_UNSUPPORTED;
   }

   return EFI_SUCCESS;

  case DeviceDiscoveryDriverBindingStart:
    Private = (TEGRA_UART_PRIVATE_DATA *)AllocateZeroPool (sizeof (TEGRA_UART_PRIVATE_DATA));
    if (Private == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    Private->BaseAddress = PcdGet64 (PcdSerialRegisterBase);
    Private->RegisterStride = PcdGet32 (PcdSerialRegisterStride);
    Private->ControllerHandle = ControllerHandle;
    Status = gBS->CreateEvent (
                    EVT_SIGNAL_EXIT_BOOT_SERVICES,
                    TPL_NOTIFY,
                    NotifyExitBootServices,
                    Private,
                    &Private->OnExitEvent
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to create event (%r)\r\n", __FUNCTION__, Status));
      return Status;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ControllerHandle,
                    &gEfiCallerIdGuid,
                    Private,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to install private data\r\n", __FUNCTION__));
      gBS->CloseEvent (Private->OnExitEvent);
    }

    return Status;

  case DeviceDiscoveryDriverBindingStop:
    Status = gBS->HandleProtocol (ControllerHandle, &gEfiCallerIdGuid, (VOID **)&Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get private data\r\n", __FUNCTION__));
      return Status;
    }

    gBS->CloseEvent (Private->OnExitEvent);
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    ControllerHandle,
                    &gEfiCallerIdGuid,
                    Private,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to uninstall protocol\r\n", __FUNCTION__));
      return Status;
    }

    FreePool (Private);
    return Status;

  default:
    return EFI_SUCCESS;
  }
}
