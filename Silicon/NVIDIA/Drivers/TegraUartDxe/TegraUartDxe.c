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
#include <Library/DtPlatformDtbLoaderLib.h>

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

#define UART_CLOCK_NAME "serial"
#define UART_CLOCK_RATE (115200 * 16)

/** Is combined UART supported

 **/
STATIC
BOOLEAN
EFIAPI
UseCombinedUART (
  VOID
  )
{
  VOID           *DTBBaseAddress;
  UINTN          DeviceTreeSize;
  INT32          NodeOffset;
  CONST VOID     *Property;
  EFI_STATUS     Status;

  Status = DtPlatformLoadDtb (&DTBBaseAddress, &DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  if (fdt_check_header (DTBBaseAddress) != 0) {
    return FALSE;
  }

  NodeOffset = fdt_path_offset (DTBBaseAddress, "/combined-uart");
  if (NodeOffset < 0) {
    NodeOffset = fdt_path_offset ((VOID *)DTBBaseAddress, "/tcu");
    if (NodeOffset < 0) {
      return FALSE;
    }
  }

  Property = fdt_getprop (DTBBaseAddress, NodeOffset, "status", NULL);
  if (NULL != Property) {
    if (0 != AsciiStrCmp (Property, "okay")) {
      return FALSE;
    }
  }

  return TRUE;
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

  switch (Phase) {
  case DeviceDiscoveryDriverStart:
    if (UseCombinedUART ()) {
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gNVIDIAConsoleEnabledProtocolGuid,
                      NULL,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to install console enabled protocol\r\n", __FUNCTION__));
      }
    }
    return EFI_SUCCESS;

  case DeviceDiscoveryDriverBindingSupported:
    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }
    if (Tegra16550SerialPortGetBaseAddress (TRUE) != BaseAddress) {
      return EFI_UNSUPPORTED;
    }
    return EFI_SUCCESS;

  case DeviceDiscoveryDriverBindingStart:
    Status = DeviceDiscoverySetClockFreq (ControllerHandle, UART_CLOCK_NAME, UART_CLOCK_RATE);
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ControllerHandle,
                    &gNVIDIAConsoleEnabledProtocolGuid,
                    NULL,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to install console enabled protocol\r\n", __FUNCTION__));
    }
    return Status;

  case DeviceDiscoveryDriverBindingStop:
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    ControllerHandle,
                    &gNVIDIAConsoleEnabledProtocolGuid,
                    NULL,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to uninstall protocol\r\n", __FUNCTION__));
      return Status;
    }

    return Status;

  default:
    return EFI_SUCCESS;
  }
}
