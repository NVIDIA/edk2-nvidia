/** @file

  XHCI Controller Driver

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
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
#include <Library/UsbFalconLib.h>
#include <Library/UsbFirmwareLib.h>
#include <Protocol/ResetNodeProtocol.h>
#include <Protocol/UsbPadCtl.h>
#include "XhciControllerPrivate.h"

/* Falcon firmware image */
#define FalconFirmware     xusb_sil_prod_fw
#define FalconFirmwareSize xusb_sil_prod_fw_len

NVIDIA_USBPADCTL_PROTOCOL *mUsbPadCtlProtocol;

/* Discover driver */

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra186-xhci", &gEdkiiNonDiscoverableXhciDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    L"NVIDIA Xhci controller driver",   //DriverName
    TRUE,                               //UseDriverBinding
    TRUE,                               //AutoEnableClocks
    TRUE,                               //AutoSetParents
    TRUE,                               //AutoDeassertReset
    FALSE,                              //AutoDeassertPg
    FALSE                               //SkipEdkiiNondiscoverableInstall
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES         Phase,
  IN  EFI_HANDLE                             DriverHandle,
  IN  EFI_HANDLE                             ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS Status = EFI_SUCCESS;
  NVIDIA_RESET_NODE_PROTOCOL *ResetProtocol;
  UINT32 reg_val;
  EFI_PHYSICAL_ADDRESS      BaseAddress  = 0;
  EFI_PHYSICAL_ADDRESS      CfgAddress  = 0;
  UINTN                     RegionSize;

  DEBUG ((EFI_D_ERROR, "%a\r\n",__FUNCTION__));

  switch (Phase) {
  case DeviceDiscoveryDriverBindingStart:
    DEBUG ((EFI_D_ERROR, "%a: DeviceDiscoveryDriverBindingStart\r\n",
                                                      __FUNCTION__));

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress,
                                                                &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Unable to locate Xhci Base address range\n",
                                                              __FUNCTION__));
      goto ErrorExit;
    }
    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 1, &CfgAddress,
                                                                &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Unable to locate Xhci Config address range\n",
                                                                __FUNCTION__));
      goto ErrorExit;
    }

    Status = gBS->LocateProtocol (&gNVIDIAUsbPadCtlProtocolGuid, NULL,
                                                (VOID **)&mUsbPadCtlProtocol);
    if (EFI_ERROR (Status) || mUsbPadCtlProtocol == NULL) {
      DEBUG ((EFI_D_ERROR, "%a:Couldnt fine UsbPadCtl Protocol Handle %r\n",
                                                        __FUNCTION__, Status));
      goto ErrorExit;
    }

    Status = gBS->HandleProtocol (ControllerHandle,
                       &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, no reset node protocol\r\n",__FUNCTION__));
      goto ErrorExit;
    }
    Status = ResetProtocol->Deassert (ResetProtocol, 53);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to deassert reset 53 %r\r\n",
                                             __FUNCTION__, Status));
      goto ErrorExit;
    }
    Status = ResetProtocol->Deassert (ResetProtocol, 54);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to deassert reset 54 %r\r\n",
                                             __FUNCTION__, Status));
      goto ErrorExit;
    }
    Status = ResetProtocol->Deassert (ResetProtocol, 55);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to deassert reset 55 %r\r\n",
                                             __FUNCTION__, Status));
      goto ErrorExit;
    }
    Status = ResetProtocol->Deassert (ResetProtocol, 56);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to deassert reset 56 %r\r\n",
                                             __FUNCTION__, Status));
      goto ErrorExit;
    }

    /* Initialize USB Pad Registers */
    mUsbPadCtlProtocol->InitHw(mUsbPadCtlProtocol);


    /* Program Xhci PCI Cfg Registers */
    reg_val = MmioRead32(CfgAddress + XUSB_CFG_4_0);
    reg_val &= ~(CFG4_ADDR_MASK << CFG4_ADDR_SHIFT);
    reg_val |= BaseAddress & (CFG4_ADDR_MASK << CFG4_ADDR_SHIFT);
    MmioWrite32(CfgAddress + XUSB_CFG_4_0, reg_val);

    gBS->Stall(200);

    reg_val = MmioRead32(CfgAddress + XUSB_CFG_1_0);
    reg_val =
            NV_FLD_SET_DRF_DEF(XUSB_CFG, 1, MEMORY_SPACE, ENABLED, reg_val);
    reg_val = NV_FLD_SET_DRF_DEF(XUSB_CFG, 1, BUS_MASTER, ENABLED, reg_val);
    MmioWrite32(CfgAddress + XUSB_CFG_1_0, reg_val);

    /* Load xusb Firmware */
    Status = FalconFirmwareLoad (FalconFirmware, FalconFirmwareSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to load falcon firmware %r\r\n",
                                                __FUNCTION__, Status));
      goto ErrorExit;
    }
    break;
  default:
    break;
  }
ErrorExit:
    return Status;
}
