/** @file

  SMMUv3 Driver

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DeviceTreeHelperLib.h>

#include "SmmuV3DxePrivate.h"

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "arm,smmu-v3", &gNVIDIANonDiscoverableSmmuV3DeviceGuid },
  { NULL,          NULL                                    }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName = L"NVIDIA Smmu V3 Controller Driver"
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
  EFI_STATUS                       Status;
  EFI_PHYSICAL_ADDRESS             BaseAddress;
  UINTN                            RegionSize;
  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private;
  UINT32                           NodeHandle;

  Status      = EFI_SUCCESS;
  BaseAddress = 0;
  RegionSize  = 0;
  Private     = NULL;
  NodeHandle  = 0;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      Status = DeviceTreeGetNodePHandle (DeviceTreeNode->NodeOffset, &NodeHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to get phandle for node\n", __FUNCTION__));
        goto Exit;
      }

      break;

    case DeviceDiscoveryDriverBindingStart:
      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
        goto Exit;
      }

      Private = AllocateZeroPool (sizeof (SMMU_V3_CONTROLLER_PRIVATE_DATA));
      if (Private == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      Private->BaseAddress    = BaseAddress;
      Private->DeviceTreeBase = DeviceTreeNode->DeviceTreeBase;
      Private->NodeOffset     = DeviceTreeNode->NodeOffset;

      Status = DeviceTreeGetNodePHandle (DeviceTreeNode->NodeOffset, &Private->PHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to get phandle for node\n", __FUNCTION__));
        goto Exit;
      }

      DEBUG ((DEBUG_ERROR, "%a: Base Addr 0x%lx\n", __FUNCTION__, Private->BaseAddress));
      DEBUG ((DEBUG_ERROR, "%a: PHandle 0x%lx\n", __FUNCTION__, Private->PHandle));

      break;

    default:
      break;
  }

Exit:
  if (Private != NULL) {
    FreePool (Private);
  }

  return Status;
}
