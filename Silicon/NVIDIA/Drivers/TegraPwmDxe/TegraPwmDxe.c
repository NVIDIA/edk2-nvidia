/** @file

  TegraPwmDxe Controller Driver

  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

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
#include <libfdt.h>

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra194-pwm", &gNVIDIANonDiscoverablePwmDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA PWM driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = TRUE,
    .AutoResetModule = TRUE,
    .SkipEdkiiNondiscoverableInstall = FALSE
};

#define PWM_FAN_HIGH 0x81000000
#define PWM_FAN_MED  0x80800000
#define PWM_CLOCK_FREQ 19200000

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
  EFI_PHYSICAL_ADDRESS      BaseAddress;
  UINTN                     RegionSize;
  UINT32                    NodeHandle;
  INT32                     FanOffset;
  CONST UINT32              *FanPwm;
  INT32                     PwmLength;
  UINT32                    FanPwmHandle;

  switch (Phase) {
  case DeviceDiscoveryDriverBindingSupported:
    BaseAddress = 0;
    RegionSize = 0;
    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range for Tegra PWM\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }

    NodeHandle = fdt_get_phandle (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset);

    //Check that handle of PWM specified by pwm-fan node matches to only support fan pwm.
    FanOffset = fdt_node_offset_by_compatible (DeviceTreeNode->DeviceTreeBase, 0, "pwm-fan");
    if (FanOffset < 0) {
      return EFI_UNSUPPORTED;
    }

    FanPwm = fdt_getprop (DeviceTreeNode->DeviceTreeBase, FanOffset, "pwms", &PwmLength);
    if (FanPwm == NULL) {
      return EFI_UNSUPPORTED;
    }

    if (PwmLength < sizeof (UINT32)){
      return EFI_UNSUPPORTED;
    }

    FanPwmHandle = SwapBytes32 (FanPwm[0]);

    if (NodeHandle == FanPwmHandle) {
      return EFI_SUCCESS;
    }

    return EFI_UNSUPPORTED;

  case DeviceDiscoveryDriverBindingStart:
    BaseAddress = 0;
    RegionSize = 0;
    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range for Tegra PWM\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }

    DeviceDiscoverySetClockFreq (ControllerHandle, "pwm", PWM_CLOCK_FREQ);
    MmioWrite32 (BaseAddress, PWM_FAN_MED);

    return Status;

  default:
    return EFI_SUCCESS;
  }
}
