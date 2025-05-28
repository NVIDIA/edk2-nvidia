/** @file

  TegraPwmDxe Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2020-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Uefi/UefiBaseType.h"
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

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra264-pwm", NULL                                 },
  { "nvidia,*-pwm",        &gNVIDIANonDiscoverablePwmDeviceGuid },
  { NULL,                  NULL                                 }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA PWM driver",
  .AutoEnableClocks                = TRUE,
  .AutoResetModule                 = TRUE,
  .SkipEdkiiNondiscoverableInstall = FALSE
};

#define PWM_FAN_HIGH    0x81000000
#define PWM_FAN_MED     0x80800000
#define PWM_CLOCK_FREQ  19200000
#define PWM_MAX         0x100
#define PWM_OFFSET      16
#define PWM_MASK        ((PWM_MAX - 1) << PWM_OFFSET)

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
  EFI_PHYSICAL_ADDRESS  BaseAddress;
  UINTN                 RegionSize;
  UINT32                NodeHandle;
  INT32                 FanOffset;
  CONST UINT32          *FanPwm;
  UINT32                PwmLength;
  UINT32                FanPwmHandle;
  CONST CHAR8           *CompatArray[2];
  UINT32                PwmPulseWidth;
  UINT32                PwmCsrVal;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      BaseAddress = 0;
      RegionSize  = 0;
      Status      = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range for Tegra PWM\n", __FUNCTION__));
        return EFI_UNSUPPORTED;
      }

      Status = DeviceTreeGetNodePHandle (DeviceTreeNode->NodeOffset, &NodeHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to get phandle for node\n", __FUNCTION__));
        return EFI_UNSUPPORTED;
      }

      FanOffset      = -1;
      CompatArray[0] = "pwm-fan";
      CompatArray[1] = NULL;
      // Check that handle of PWM specified by pwm-fan node matches to only support fan pwm.
      Status = DeviceTreeGetNextCompatibleNode (CompatArray, &FanOffset);
      if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
      }

      Status = DeviceTreeGetNodeProperty (FanOffset, "pwms", (CONST VOID **)&FanPwm, &PwmLength);
      if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
      }

      if (PwmLength < sizeof (UINT32)) {
        return EFI_UNSUPPORTED;
      }

      FanPwmHandle = SwapBytes32 (FanPwm[0]);

      if (NodeHandle == FanPwmHandle) {
        return EFI_SUCCESS;
      }

      return EFI_UNSUPPORTED;

    case DeviceDiscoveryDriverBindingStart:
      BaseAddress = 0;
      RegionSize  = 0;
      Status      = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range for Tegra PWM\n", __FUNCTION__));
        return EFI_UNSUPPORTED;
      }

      FanOffset      = -1;
      CompatArray[0] = "pwm-fan";
      CompatArray[1] = NULL;
      Status         = DeviceTreeGetNextCompatibleNode (CompatArray, &FanOffset);
      if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
      }

      PwmCsrVal = PWM_FAN_MED;
      Status    = DeviceTreeGetNodePropertyValue32 (FanOffset, "pulse-width", &PwmPulseWidth);
      // Pulse width of high level is supposed to be in the range [0, 255].
      if (!EFI_ERROR (Status) && (PwmPulseWidth < PWM_MAX)) {
        PwmCsrVal = (PwmCsrVal & ~PWM_MASK) | (PwmPulseWidth << PWM_OFFSET);
      }

      DeviceDiscoverySetClockFreq (ControllerHandle, "pwm", PWM_CLOCK_FREQ);
      MmioWrite32 (BaseAddress, PwmCsrVal);

      return EFI_SUCCESS;

    default:
      return EFI_SUCCESS;
  }
}
