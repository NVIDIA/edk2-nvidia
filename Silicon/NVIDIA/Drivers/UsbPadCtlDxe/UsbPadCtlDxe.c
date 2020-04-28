/** @file

  Usb Pad Control Driver

  Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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
#include <Protocol/Regulator.h>
#include <Protocol/EFuse.h>
#include <Protocol/PinMux.h>
#include "UsbPadCtlPrivate.h"
#include <libfdt.h>

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra18x-xusb-padctl", &gNVIDIANonDiscoverableT186UsbPadDeviceGuid },
    { "nvidia,tegra19x-xusb-padctl", &gNVIDIANonDiscoverableT194UsbPadDeviceGuid },
    { "nvidia,tegra194-xusb-padctl", &gNVIDIANonDiscoverableT194UsbPadDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA USB Pad controller driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = TRUE,
    .AutoDeassertReset = TRUE,
    .AutoResetModule = FALSE,
    .AutoDeassertPg = FALSE,
    .SkipEdkiiNondiscoverableInstall = TRUE
};

/**
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to device tree node protocol is available.

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
  EFI_STATUS Status = EFI_SUCCESS;
  USBPADCTL_DXE_PRIVATE *Private = NULL;
  NVIDIA_REGULATOR_PROTOCOL    *mRegulator = NULL;
  NVIDIA_EFUSE_PROTOCOL        *mEfuse = NULL;
  NVIDIA_PINMUX_PROTOCOL       *mPmux = NULL;
  SCMI_CLOCK2_PROTOCOL         *mClockProtocol = NULL;

  EFI_PHYSICAL_ADDRESS      BaseAddress = 0;
  UINTN                     RegionSize;

  switch (Phase) {
  case DeviceDiscoveryDriverBindingStart:

    Private = AllocatePool (sizeof (USBPADCTL_DXE_PRIVATE));
    if (NULL == Private) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate private data stucture\r\n",
                                                               __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    Status = gBS->LocateProtocol (&gNVIDIARegulatorProtocolGuid, NULL,
                                                (VOID **)&mRegulator);
    if (EFI_ERROR (Status) || mRegulator == NULL) {
      DEBUG ((EFI_D_ERROR,
      "%a: Couldn't get gNVIDIARegulatorProtocolGuid Handle: %r\n",
      __FUNCTION__, Status));
      goto ErrorExit;
    }

    Status = gBS->LocateProtocol (&gNVIDIAEFuseProtocolGuid, NULL,
                                                (VOID **)&mEfuse);
    if (EFI_ERROR (Status) || mEfuse == NULL) {
      DEBUG ((EFI_D_ERROR,
      "%a: Couldn't get gNVIDIAEFuseProtocolGuid Handle: %r\n",
      __FUNCTION__, Status));
      goto ErrorExit;
    }

    Status = gBS->LocateProtocol (&gNVIDIAPinMuxProtocolGuid, NULL,
                                                (VOID **)&mPmux);
    if (EFI_ERROR (Status) || mPmux == NULL) {
      DEBUG ((EFI_D_ERROR,
      "%a: Couldn't get gNVIDIAPinMuxProtocolGuid Handle: %r\n",
      __FUNCTION__, Status));
      goto ErrorExit;
    }

    Status = gBS->LocateProtocol (&gArmScmiClock2ProtocolGuid, NULL,
                                         (VOID **) &mClockProtocol);
    if (EFI_ERROR (Status) || mClockProtocol == NULL) {
      DEBUG ((EFI_D_ERROR,
      "%a: Couldn't get gArmScmiClock2ProtocolGuid Handle: %r\n",
      __FUNCTION__, Status));
      goto ErrorExit;
    }

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress,
                                                                &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Unable to locate UsbPadCtl Base address range\n",
                                                              __FUNCTION__));
      goto ErrorExit;
    }

    Private->Signature = PADCTL_SIGNATURE;
    Private->BaseAddress = BaseAddress;
    Private->ImageHandle = DriverHandle;
    Private->DeviceTreeNode = DeviceTreeNode;
    Private->mRegulator = mRegulator;
    Private->mEfuse = mEfuse;
    Private->mPmux = mPmux;
    Private->mClockProtocol = mClockProtocol;
    /* Assign Platform Specific Parameters */
    if (fdt_node_offset_by_compatible(DeviceTreeNode->DeviceTreeBase, 0,
                                  "nvidia,tegra18x-xusb-padctl") > 0) {
      Private->UsbPadCtlProtocol.InitHw = InitUsbHw186;
      Private->UsbPadCtlProtocol.DeInitHw = DeInitUsbHw186;
      Private->PlatConfig = Tegra186UsbConfig;
    } else if ((fdt_node_offset_by_compatible(DeviceTreeNode->DeviceTreeBase,
                                0, "nvidia,tegra19x-xusb-padctl") > 0) ||
               (fdt_node_offset_by_compatible(DeviceTreeNode->DeviceTreeBase,
                                0, "nvidia,tegra194-xusb-padctl") > 0)) {
      Private->UsbPadCtlProtocol.InitHw = InitUsbHw194;
      Private->UsbPadCtlProtocol.DeInitHw = DeInitUsbHw194;
      Private->PlatConfig = Tegra194UsbConfig;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gNVIDIAUsbPadCtlProtocolGuid,
                  &Private->UsbPadCtlProtocol,
                  NULL
                  );
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, Failed to install protocols: %r\r\n",
                                              __FUNCTION__, Status));
      goto ErrorExit;
    }
    break;
  default:
    break;
  }
  return EFI_SUCCESS;
ErrorExit:
  FreePool (Private);
  return Status;
}
