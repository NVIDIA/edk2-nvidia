/** @file

  NV Display Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2021-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "NvDisplayController.h"

#define DISP_SW_SOC_CHIP_ID_T234  0x2350
#define DISP_SW_SOC_CHIP_ID_T238  0x23b0

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
  Starts the NV T23x display controller driver on the given
  controller handle.

  Reads the nvidia,disp-sw-soc-chip-id device tree property to
  determine the specific chip variant (T234 or T238) and calls
  the appropriate start function.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.
  @param[in] DeviceTreeNode    Pointer to the device tree node protocol.

  @retval EFI_SUCCESS            Operation successful.
  @retval EFI_INVALID_PARAMETER  DeviceTreeNode is NULL.
  @retval EFI_NOT_FOUND          nvidia,disp-sw-soc-chip-id property not found.
  @retval EFI_UNSUPPORTED        Unsupported chip ID.
  @retval !=EFI_SUCCESS          Operation failed.
*/
STATIC
EFI_STATUS
NvDisplayControllerStartT23x (
  IN CONST EFI_HANDLE                        DriverHandle,
  IN CONST EFI_HANDLE                        ControllerHandle,
  IN CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode
  )
{
  EFI_STATUS  Status;
  UINT32      DispSwSocChipId;

  if (DeviceTreeNode == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DeviceTreeNode is NULL\r\n",
      __FUNCTION__
      ));
    return EFI_INVALID_PARAMETER;
  }

  Status = DeviceTreeGetNodePropertyValue32 (
             DeviceTreeNode->NodeOffset,
             "nvidia,disp-sw-soc-chip-id",
             &DispSwSocChipId
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to get nvidia,disp-sw-soc-chip-id: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  switch (DispSwSocChipId) {
    case DISP_SW_SOC_CHIP_ID_T234:
      return NvDisplayControllerStartT234 (DriverHandle, ControllerHandle);

    case DISP_SW_SOC_CHIP_ID_T238:
      return NvDisplayControllerStartT238 (DriverHandle, ControllerHandle);

    default:
      DEBUG ((
        DEBUG_ERROR,
        "%a: unsupported disp-sw-soc-chip-id: 0x%x\r\n",
        __FUNCTION__,
        DispSwSocChipId
        ));
      return EFI_UNSUPPORTED;
  }
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
        return NvDisplayControllerStartT23x (DriverHandle, ControllerHandle, DeviceTreeNode);
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
