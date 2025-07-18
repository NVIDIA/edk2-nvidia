/** @file

  Tegra Controller Enable Driver

  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DeviceTreeHelperLib.h>

#define HWPM_LA_CLOCK_NAME  "la"
#define HWPM_LA_MAX_CLOCK   625000000

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,gv11b",      &gNVIDIANonDiscoverableEnableOnlyDeviceGuid },
  { "nvidia,*-hda",      &gNVIDIANonDiscoverableEnableOnlyDeviceGuid },
  { "nvidia,ga10b",      &gNVIDIANonDiscoverableEnableOnlyDeviceGuid },
  { "nvidia,*-soc-hwpm", &gNVIDIANonDiscoverableEnableOnlyDeviceGuid },
  { "nvidia,*-nvdla",    &gNVIDIANonDiscoverableEnableOnlyDeviceGuid },
  { "nvidia,*-host1x",   &gNVIDIANonDiscoverableEnableOnlyDeviceGuid },
  { "nvidia,*-rce",      &gNVIDIANonDiscoverableEnableOnlyDeviceGuid },
  { NULL,                NULL                                        }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                                 = L"NVIDIA Controller Enable Driver",
  .AutoEnableClocks                           = TRUE,
  .AutoResetModule                            = TRUE,
  .AutoDeassertPg                             = TRUE,
  .SkipEdkiiNondiscoverableInstall            = TRUE,
  .SkipAutoDeinitControllerOnExitBootServices = TRUE
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
  EFI_STATUS   Status;
  CONST CHAR8  *ClockName;
  UINT32       ClockId;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:
      if (DeviceTreeNode != NULL) {
        if (!EFI_ERROR (DeviceTreeCheckNodeSingleCompatibility ("nvidia,*-soc-hwpm", DeviceTreeNode->NodeOffset))) {
          ClockName = HWPM_LA_CLOCK_NAME;
          Status    = DeviceDiscoveryGetClockId (ControllerHandle, ClockName, &ClockId);
          if (!EFI_ERROR (Status)) {
            Status = DeviceDiscoverySetClockFreq (ControllerHandle, ClockName, HWPM_LA_MAX_CLOCK);
            if (EFI_ERROR (Status)) {
              DEBUG ((DEBUG_ERROR, "%a, Failed to set hwpm la clock frequency %r\r\n", __FUNCTION__, Status));
            }
          }
        }
      }

      return EFI_SUCCESS;

    default:
      return EFI_SUCCESS;
  }
}
