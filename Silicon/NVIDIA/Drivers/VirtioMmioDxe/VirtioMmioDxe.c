/** @file

  Device discovery based  Virtio MMIO driver

  SPDX-FileCopyrightText: Copyright (c) 2020- 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DebugLib.h>
#include <Library/VirtioMmioDeviceLib.h>

#include <Guid/VirtioMmioTransport.h>

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "virtio,mmio", &gVirtioMmioTransportGuid },
  { NULL,          NULL                      }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                                 = L"NVIDIA Virtio MMIO Driver",
  .AutoEnableClocks                           = TRUE,
  .AutoResetModule                            = TRUE,
  .AutoDeassertPg                             = TRUE,
  .SkipEdkiiNondiscoverableInstall            = TRUE,
  .SkipAutoDeinitControllerOnExitBootServices = FALSE,
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
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  RegBase;
  UINTN                 RegSize;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:
      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &RegBase, &RegSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate VIRTIO address range\n", __FUNCTION__));
        Status = EFI_UNSUPPORTED;
        break;
      }

      Status = VirtioMmioInstallDevice (RegBase, ControllerHandle);
      return EFI_SUCCESS;

    default:
      return EFI_SUCCESS;
  }

  return Status;
}
