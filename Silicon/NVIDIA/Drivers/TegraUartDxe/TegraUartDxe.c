/** @file

  TegraUart Controller Driver

  Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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
#include <NVIDIAConfiguration.h>

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra20-uart",    &gNVIDIANonDiscoverable16550UartDeviceGuid    },
  { "nvidia,tegra186-hsuart", &gNVIDIANonDiscoverable16550UartDeviceGuid    },
  { "nvidia,tegra194-hsuart", &gNVIDIANonDiscoverable16550UartDeviceGuid    },
  { "nvidia,tegra194-tcu",    &gNVIDIANonDiscoverableCombinedUartDeviceGuid },
  { "arm,sbsa-uart",          &gNVIDIANonDiscoverableSbsaUartDeviceGuid     },
  { NULL,                     NULL                                          }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                                 = L"NVIDIA Serial Driver",
  .UseDriverBinding                           = TRUE,
  .AutoEnableClocks                           = TRUE,
  .AutoResetModule                            = TRUE,
  .SkipEdkiiNondiscoverableInstall            = FALSE,
  .SkipAutoDeinitControllerOnExitBootServices = TRUE
};

#define UART_CLOCK_NAME  "serial"
#define UART_CLOCK_RATE  (115200 * 16)

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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS              Status;
  UINT32                  ClockId;
  EFI_PHYSICAL_ADDRESS    BaseAddress = 0;
  UINTN                   RegionSize;
  EFI_SERIAL_IO_PROTOCOL  *Interface;
  UINT8                   SerialConfig;
  BOOLEAN                 InstallSerialIO;

  SerialConfig = PcdGet8 (PcdSerialPortConfig);

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      if (((fdt_node_check_compatible (
              DeviceTreeNode->DeviceTreeBase,
              DeviceTreeNode->NodeOffset,
              "nvidia,tegra20-uart"
              )) == 0) ||
          ((fdt_node_check_compatible (
              DeviceTreeNode->DeviceTreeBase,
              DeviceTreeNode->NodeOffset,
              "nvidia,tegra186-hsuart"
              )) == 0) ||
          ((fdt_node_check_compatible (
              DeviceTreeNode->DeviceTreeBase,
              DeviceTreeNode->NodeOffset,
              "nvidia,tegra194-hsuart"
              )) == 0))
      {
        if ((PcdGet8 (PcdSerialTypeConfig) != NVIDIA_SERIAL_PORT_TYPE_16550) ||
            (SerialConfig == NVIDIA_SERIAL_PORT_DISABLED))
        {
          return EFI_UNSUPPORTED;
        }
      } else if ((fdt_node_check_compatible (
                    DeviceTreeNode->DeviceTreeBase,
                    DeviceTreeNode->NodeOffset,
                    "arm,sbsa-uart"
                    )) == 0)
      {
        if ((PcdGet8 (PcdSerialTypeConfig) != NVIDIA_SERIAL_PORT_TYPE_SBSA) ||
            (SerialConfig == NVIDIA_SERIAL_PORT_DISABLED))
        {
          return EFI_UNSUPPORTED;
        }
      }

      return EFI_SUCCESS;

    case DeviceDiscoveryDriverBindingStart:
      if (((fdt_node_check_compatible (
              DeviceTreeNode->DeviceTreeBase,
              DeviceTreeNode->NodeOffset,
              "nvidia,tegra20-uart"
              )) == 0) ||
          ((fdt_node_check_compatible (
              DeviceTreeNode->DeviceTreeBase,
              DeviceTreeNode->NodeOffset,
              "nvidia,tegra186-hsuart"
              )) == 0) ||
          ((fdt_node_check_compatible (
              DeviceTreeNode->DeviceTreeBase,
              DeviceTreeNode->NodeOffset,
              "nvidia,tegra194-hsuart"
              )) == 0))
      {
        if ((PcdGet8 (PcdSerialTypeConfig) != NVIDIA_SERIAL_PORT_TYPE_16550) ||
            (SerialConfig == NVIDIA_SERIAL_PORT_DISABLED))
        {
          return EFI_UNSUPPORTED;
        }

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

        InstallSerialIO = (SerialConfig != NVIDIA_SERIAL_PORT_DBG2_NVIDIA_16550);
      } else if ((fdt_node_check_compatible (
                    DeviceTreeNode->DeviceTreeBase,
                    DeviceTreeNode->NodeOffset,
                    "arm,sbsa-uart"
                    )) == 0)
      {
        if ((PcdGet8 (PcdSerialTypeConfig) != NVIDIA_SERIAL_PORT_TYPE_SBSA) ||
            (SerialConfig == NVIDIA_SERIAL_PORT_DISABLED))
        {
          return EFI_UNSUPPORTED;
        }

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
          DEBUG ((DEBUG_ERROR, "%a: Sbsa Unable to locate address range\n", __FUNCTION__));
          return Status;
        }

        Interface = SerialSbsaIoInitialize (BaseAddress);
        if (Interface == NULL) {
          return EFI_NOT_STARTED;
        }

        InstallSerialIO = (SerialConfig != NVIDIA_SERIAL_PORT_DBG2_SBSA);
      } else {
        Interface       = SerialTCUIoInitialize ();
        InstallSerialIO = TRUE;
      }

      Status = Interface->Reset (Interface);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if (InstallSerialIO) {
        Status = gBS->InstallMultipleProtocolInterfaces (
                        &ControllerHandle,
                        &gEfiSerialIoProtocolGuid,
                        Interface,
                        NULL
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to install console enabled protocol\r\n", __FUNCTION__));
        }
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
