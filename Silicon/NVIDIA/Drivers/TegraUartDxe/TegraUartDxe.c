/** @file

  TegraUart Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2019-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraSerialPortLib.h>
#include <libfdt.h>
#include <NVIDIAConfiguration.h>

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,*-tcu",  &gNVIDIANonDiscoverableCombinedUartDeviceGuid },
  { "arm,sbsa-uart", &gNVIDIANonDiscoverableSbsaUartDeviceGuid     },
  { "arm,pl011",     &gNVIDIANonDiscoverableSbsaUartDeviceGuid     },
  { "nvidia,*-utc",  &gNVIDIANonDiscoverableUtcUartDeviceGuid      },
  { NULL,            NULL                                          }
};

CONST CHAR8  *gSbsaUartCompatible[] = {
  "arm,sbsa-uart", "arm,pl011", NULL
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                                 = L"NVIDIA Serial Driver",
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
  CONST CHAR8             *ClockName;
  UINT32                  ClockNameLength;
  UINT64                  ClockRate;

  SerialConfig = PcdGet8 (PcdSerialPortConfig);

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      if (!EFI_ERROR (DeviceTreeCheckNodeCompatibility (gSbsaUartCompatible, DeviceTreeNode->NodeOffset))) {
        if ((PcdGet8 (PcdSerialTypeConfig) != NVIDIA_SERIAL_PORT_TYPE_SBSA) ||
            (SerialConfig == NVIDIA_SERIAL_PORT_DISABLED))
        {
          return EFI_UNSUPPORTED;
        }
      }

      return EFI_SUCCESS;

    case DeviceDiscoveryDriverBindingStart:
      if (!EFI_ERROR (DeviceTreeCheckNodeCompatibility (gSbsaUartCompatible, DeviceTreeNode->NodeOffset))) {
        if ((PcdGet8 (PcdSerialTypeConfig) != NVIDIA_SERIAL_PORT_TYPE_SBSA) ||
            (SerialConfig == NVIDIA_SERIAL_PORT_DISABLED))
        {
          return EFI_UNSUPPORTED;
        }

        Status = DeviceTreeGetNodeProperty (
                   DeviceTreeNode->NodeOffset,
                   "clock-names",
                   (CONST VOID **)&ClockName,
                   &ClockNameLength
                   );
        if (EFI_ERROR (Status)) {
          ClockName = UART_CLOCK_NAME;
        }

        DEBUG ((DEBUG_INFO, "%a: using %a\n", __FUNCTION__, ClockName));

        Status = DeviceDiscoveryGetClockId (ControllerHandle, ClockName, &ClockId);
        if (!EFI_ERROR (Status)) {
          ClockRate = UART_CLOCK_RATE;
          Status    = DeviceDiscoverySetClockFreq (ControllerHandle, ClockName, ClockRate);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a: Unable to set clock %a frequency\n", __FUNCTION__, ClockName));
            return Status;
          }

          DEBUG ((DEBUG_INFO, "%a: set %a clk freq to 0x%llu\n", __FUNCTION__, ClockName, ClockRate));
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
      } else if (!EFI_ERROR (DeviceTreeCheckNodeSingleCompatibility ("nvidia,*-utc", DeviceTreeNode->NodeOffset))) {
        // Region 1 is TX base
        Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 1, &BaseAddress, &RegionSize);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Utc Unable to locate address range\n", __FUNCTION__));
          return Status;
        }

        Interface = SerialUtcIoInitialize (BaseAddress);
        if (Interface == NULL) {
          return EFI_NOT_STARTED;
        }

        InstallSerialIO = TRUE;
      } else if (!EFI_ERROR (DeviceTreeCheckNodeSingleCompatibility ("nvidia,*-tcu", DeviceTreeNode->NodeOffset))) {
        Interface       = SerialTCUIoInitialize ();
        InstallSerialIO = TRUE;
      } else {
        DEBUG ((DEBUG_ERROR, "%a: no init for %a\n", __FUNCTION__, DeviceTreeGetNodeName (DeviceTreeNode->NodeOffset)));
        ASSERT (FALSE);
        return EFI_NOT_FOUND;
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
