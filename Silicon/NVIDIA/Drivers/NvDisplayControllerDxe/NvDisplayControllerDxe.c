/** @file

  NV Display Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2021-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "NvDisplayController.h"

/* Discover driver */

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra234-display", &gNVIDIANonDiscoverableT234DisplayDeviceGuid },
  { "nvidia,tegra264-display", &gNVIDIANonDiscoverableT264DisplayDeviceGuid },
  { NULL,                      NULL                                         }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NV Display Controller Driver",
  .AutoDeassertPg                  = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE
};

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
  EFI_STATUS               Status;
  TEGRA_PLATFORM_TYPE      Platform;
  NON_DISCOVERABLE_DEVICE  *Device;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      Platform = TegraGetPlatform ();
      if (Platform != TEGRA_PLATFORM_SILICON) {
        return EFI_UNSUPPORTED;
      }

      return EFI_SUCCESS;

    case DeviceDiscoveryDriverBindingStart:
      Status = gBS->OpenProtocol (
                      ControllerHandle,
                      &gNVIDIANonDiscoverableDeviceProtocolGuid,
                      (VOID **)&Device,
                      DriverHandle,
                      ControllerHandle,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to open NVIDIA non-discoverable device protocol: %r\r\n",
          __FUNCTION__,
          Status
          ));
        return Status;
      }

      if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableT234DisplayDeviceGuid)) {
        return NvDisplayControllerStartT234 (DriverHandle, ControllerHandle);
      } else if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableT264DisplayDeviceGuid)) {
        return NvDisplayControllerStartT264 (DriverHandle, ControllerHandle);
      } else {
        ASSERT_EFI_ERROR (EFI_UNSUPPORTED);
      }

    case DeviceDiscoveryDriverBindingStop:
      return NvDisplayControllerStop (DriverHandle, ControllerHandle);

    case DeviceDiscoveryOnExit:
      return NvDisplayControllerOnExitBootServices (DriverHandle, ControllerHandle);

    default:
      return EFI_SUCCESS;
  }
}
