/** @file

  NV Display Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2021-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/TegraPlatformInfoLib.h>

#include "NvDisplayController.h"

/* Discover driver */

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra234-display", &gNVIDIANonDiscoverableT234DisplayDeviceGuid },
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
  TEGRA_PLATFORM_TYPE  Platform;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      Platform = TegraGetPlatform ();
      if (Platform != TEGRA_PLATFORM_SILICON) {
        return EFI_UNSUPPORTED;
      }

      return EFI_SUCCESS;

    case DeviceDiscoveryDriverBindingStart:
      return NvDisplayControllerStartT234 (DriverHandle, ControllerHandle);

    case DeviceDiscoveryDriverBindingStop:
      return NvDisplayControllerStop (DriverHandle, ControllerHandle);

    case DeviceDiscoveryOnExit:
      return NvDisplayControllerOnExitBootServices (DriverHandle, ControllerHandle);

    default:
      return EFI_SUCCESS;
  }
}
