/** @file

  XHCI Controller Driver

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
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
#include <Library/UsbFalconLib.h>
#include <Protocol/UsbPadCtl.h>
#include <Protocol/UsbFwProtocol.h>
#include <Protocol/XhciController.h>
#include <Protocol/BpmpIpc.h>
#include "XhciControllerPrivate.h"
#include <libfdt.h>

/* Discover driver */

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra186-xhci", &gEdkiiNonDiscoverableXhciDeviceGuid },
    { "nvidia,tegra186-xusb", &gEdkiiNonDiscoverableXhciDeviceGuid },
    { "nvidia,tegra194-xhci", &gEdkiiNonDiscoverableXhciDeviceGuid },
    { "nvidia,tegra194-xusb", &gEdkiiNonDiscoverableXhciDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA Xhci controller driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = TRUE,
    .AutoDeassertReset = TRUE,
    .AutoResetModule = FALSE,
    .AutoDeassertPg = TRUE,
    .SkipEdkiiNondiscoverableInstall = FALSE
};

/* XhciController Protocol Function used to return the Xhci
 * Registers Base Address */
EFI_STATUS
XhciGetBaseAddr (
  IN  NVIDIA_XHCICONTROLLER_PROTOCOL *This,
  OUT EFI_PHYSICAL_ADDRESS *BaseAddress
  )
{
  XHCICONTROLLER_DXE_PRIVATE *Private;

  if (NULL == This)
    return EFI_INVALID_PARAMETER;

  Private = XHCICONTROLLER_PRIVATE_DATA_FROM_THIS (This);

  *BaseAddress = Private->XusbSoc->BaseAddress;
  return EFI_SUCCESS;
}

/* XhciController Protocol Function used to return the Address
 * of XHCI Configuration Registers
 */
EFI_STATUS
XhciGetCfgAddr (
  IN  NVIDIA_XHCICONTROLLER_PROTOCOL *This,
  OUT EFI_PHYSICAL_ADDRESS *CfgAddress
  )
{
  XHCICONTROLLER_DXE_PRIVATE *Private;

  if (NULL == This)
    return EFI_INVALID_PARAMETER;

  Private = XHCICONTROLLER_PRIVATE_DATA_FROM_THIS (This);
  *CfgAddress = Private->XusbSoc->CfgAddress;
  return EFI_SUCCESS;
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES         Phase,
  IN  EFI_HANDLE                             DriverHandle,
  IN  EFI_HANDLE                             ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                 Status;
  UINT32                     reg_val;
  EFI_PHYSICAL_ADDRESS       BaseAddress;
  EFI_PHYSICAL_ADDRESS       CfgAddress;
  UINTN                      RegionSize;
  UINT8                      CapLength;
  UINT32                     StatusRegister;
  UINTN                      i;
  INTN                       Offset;
  XHCICONTROLLER_DXE_PRIVATE *Private;
  NON_DISCOVERABLE_DEVICE    *Device;

  switch (Phase) {
  case DeviceDiscoveryDriverBindingStart:

    Private = AllocatePool (sizeof (XHCICONTROLLER_DXE_PRIVATE));
    if (NULL == Private) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate memory\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    Device = NULL;
    Status = gBS->HandleProtocol (ControllerHandle,
                                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                                  (VOID **)&Device);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate non discoverable device\n", __FUNCTION__));
      return Status;
    }

    // Force coherent DMA type device.
    Device->DmaType = NonDiscoverableDeviceDmaTypeCoherent;

    /* Assign Platform Specific Parameters */
    if (((Offset = fdt_node_offset_by_compatible(DeviceTreeNode->DeviceTreeBase, 0,
                                          "nvidia,tegra186-xhci")) > 0) ||
        ((Offset = fdt_node_offset_by_compatible(DeviceTreeNode->DeviceTreeBase, 0,
                                          "nvidia,tegra186-xusb")) > 0)){
      Private->XusbSoc = &Tegra186Soc;
    } else if (((Offset = fdt_node_offset_by_compatible(DeviceTreeNode->DeviceTreeBase, 0,
                                          "nvidia,tegra194-xhci")) > 0) ||
               ((Offset = fdt_node_offset_by_compatible(DeviceTreeNode->DeviceTreeBase, 0,
                                          "nvidia,tegra194-xusb")) > 0)) {
      Private->XusbSoc = &Tegra194Soc;
    }

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress,
                                                                &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Unable to locate Xhci Base address range\n",
                                                              __FUNCTION__));
      goto ErrorExit;
    }
    Private->XusbSoc->BaseAddress = BaseAddress;

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 1, &CfgAddress,
                                                                &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Unable to locate Xhci Config address range\n",
                                                                __FUNCTION__));
      goto ErrorExit;
    }
    Private->XusbSoc->CfgAddress = CfgAddress;
    Private->Signature = XHCICONTROLLER_SIGNATURE;
    Private->ImageHandle = DriverHandle;
    Private->XhciControllerProtocol.GetBaseAddr = XhciGetBaseAddr;
    Private->XhciControllerProtocol.GetCfgAddr =  XhciGetCfgAddr;
    /* Install the XhciController Protocol */
    Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gNVIDIAXhciControllerProtocolGuid,
                  &Private->XhciControllerProtocol,
                  NULL
                  );
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, Failed to install protocols: %r\r\n",
                                              __FUNCTION__, Status));
      goto ErrorExit;
    }

    /* Pass Xhci Config Address to Falcon Library before using Library's
     * other functions */
    FalconSetHostCfgAddr(CfgAddress);

    Status = gBS->LocateProtocol (&gNVIDIAUsbPadCtlProtocolGuid, NULL,
                                                (VOID **)&(Private->mUsbPadCtlProtocol));
    if (EFI_ERROR (Status) || Private->mUsbPadCtlProtocol == NULL) {
      DEBUG ((EFI_D_ERROR, "%a: Couldn't find UsbPadCtl Protocol Handle %r\n",
                                                        __FUNCTION__, Status));
      goto ErrorExit;
    }

    /* Initialize USB Pad Registers */
    Status = Private->mUsbPadCtlProtocol->InitHw(Private->mUsbPadCtlProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, Failed to Initailize USB HW: %r\r\n",
                 __FUNCTION__, Status));
      goto ErrorExit;
    }

    /* Program Xhci PCI Cfg Registers */
    reg_val = MmioRead32(CfgAddress + XUSB_CFG_4_0);
    reg_val &= ~(Private->XusbSoc->Cfg4AddrMask << Private->XusbSoc->Cfg4AddrShift);
    reg_val |= BaseAddress & (Private->XusbSoc->Cfg4AddrMask << Private->XusbSoc->Cfg4AddrShift);
    MmioWrite32(CfgAddress + XUSB_CFG_4_0, reg_val);

    gBS->Stall(200);

    reg_val = MmioRead32(CfgAddress + XUSB_CFG_1_0);
    reg_val =
            NV_FLD_SET_DRF_DEF(XUSB_CFG, 1, MEMORY_SPACE, ENABLED, reg_val);
    reg_val = NV_FLD_SET_DRF_DEF(XUSB_CFG, 1, BUS_MASTER, ENABLED, reg_val);
    MmioWrite32(CfgAddress + XUSB_CFG_1_0, reg_val);

    /* Load xusb Firmware */
    Status = gBS->LocateProtocol (&gNVIDIAUsbFwProtocolGuid, NULL,
                                                (VOID **)&(Private->mUsbFwProtocol));
    if (EFI_ERROR (Status) || Private->mUsbFwProtocol == NULL) {
      DEBUG ((EFI_D_ERROR, "%a: Couldn't find UsbFw Protocol Handle %r\n",
                                                        __FUNCTION__, Status));
      goto ErrorExit;
    }

    Status = FalconFirmwareLoad (Private->mUsbFwProtocol->UsbFwBase, Private->mUsbFwProtocol->UsbFwSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to load falcon firmware %r\r\n",
                                                __FUNCTION__, Status));
      goto ErrorExit;
    }

    /* Wait till HW/FW Clears Controller Not Ready Flag */
    CapLength = MmioRead8(BaseAddress);
    for (i = 0; i < 200; i++)
    {
      StatusRegister = MmioRead32(BaseAddress + CapLength + XUSB_OP_USBSTS);
      if (!(StatusRegister & USBSTS_CNR)) {
        break;
      }
      gBS->Stall(1000);
    }

    /* Return Error if CNR is not cleared or Host Controller Error is set */
    if (StatusRegister & (USBSTS_CNR | USBSTS_HCE)) {
      DEBUG ((EFI_D_ERROR, "Usb Host Controller Initialization Failed\n"));
      DEBUG ((EFI_D_ERROR, "UsbStatus: 0x%x Falcon CPUCTL: 0x%x\n", StatusRegister, FalconRead32(FALCON_CPUCTL_0)));
      Status = EFI_DEVICE_ERROR;
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
