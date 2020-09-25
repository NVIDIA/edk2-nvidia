/** @file

  TegraUart Controller Driver

  Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
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
#include <Library/TegraSerialPortLib.h>
#include <libfdt.h>

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra20-uart", &gNVIDIANonDiscoverable16550UartDeviceGuid },
    { "nvidia,tegra194-tcu", &gNVIDIANonDiscoverableCombinedUartDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA Serial Driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = TRUE,
    .AutoResetModule = TRUE,
    .SkipEdkiiNondiscoverableInstall = FALSE
};

#define UART_CLOCK_NAME "serial"
#define UART_CLOCK_RATE (115200 * 16)

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
  UINT32                    ClockId;
  EFI_PHYSICAL_ADDRESS      BaseAddress  = 0;
  UINTN                     RegionSize;
  EFI_SERIAL_IO_PROTOCOL    *Interface;

  switch (Phase) {
  case DeviceDiscoveryDriverBindingStart:
    if ((fdt_node_check_compatible(DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset,
                                       "nvidia,tegra20-uart")) == 0) {
      Status = DeviceDiscoveryGetClockId (ControllerHandle, UART_CLOCK_NAME, &ClockId);
      if (!EFI_ERROR (Status)) {
        Status = DeviceDiscoverySetClockFreq (ControllerHandle, UART_CLOCK_NAME, UART_CLOCK_RATE);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Unable to set clock frequency\n", __FUNCTION__));
          return Status;
        }
      }
      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
        return Status;
      }
      Interface = Serial16550IoInitialize (BaseAddress);
      if (Interface == NULL) {
        return EFI_NOT_STARTED;
      }
    } else {
      Interface = SerialTCUIoInitialize ();
    }
    Status = Interface->Reset (Interface);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ControllerHandle,
                    &gEfiSerialIoProtocolGuid,
                    Interface,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to install console enabled protocol\r\n", __FUNCTION__));
    }
    return Status;

  case DeviceDiscoveryDriverBindingStop:
    Status = gBS->HandleProtocol (
                    ControllerHandle,
                    &gEfiSerialIoProtocolGuid,
                    (VOID **)&Interface
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get interface on handle\r\n", __FUNCTION__));
      return Status;
    }
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    ControllerHandle,
                    &gEfiSerialIoProtocolGuid,
                    Interface,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to uninstall protocol\r\n", __FUNCTION__));
      return Status;
    }

    gBS->FreePool (Interface);
    return Status;

  default:
    return EFI_SUCCESS;
  }
}
