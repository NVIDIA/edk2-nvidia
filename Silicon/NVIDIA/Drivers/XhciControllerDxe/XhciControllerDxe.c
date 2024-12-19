/** @file

  XHCI Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

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
#include <Library/UsbFalconLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformResourceLib.h>
#include <Protocol/UsbPadCtl.h>
#include <Protocol/UsbFwProtocol.h>
#include <Protocol/XhciController.h>
#include <Protocol/PowerGateNodeProtocol.h>
#include "XhciControllerPrivate.h"

/* Discover driver */

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,*-xhci", &gEdkiiNonDiscoverableXhciDeviceGuid },
  { "nvidia,*-xusb", &gEdkiiNonDiscoverableXhciDeviceGuid },
  { NULL,            NULL                                 }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA Xhci controller driver",
  .AutoEnableClocks                = TRUE,
  .AutoDeassertReset               = TRUE,
  .AutoResetModule                 = FALSE,
  .AutoDeassertPg                  = FALSE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .ThreadedDeviceStart             = FALSE,
};

/* XhciController Protocol Function used to return the Xhci
 * Registers Base Address */
EFI_STATUS
XhciGetBaseAddr (
  IN  NVIDIA_XHCICONTROLLER_PROTOCOL  *This,
  OUT EFI_PHYSICAL_ADDRESS            *BaseAddress
  )
{
  XHCICONTROLLER_DXE_PRIVATE  *Private;

  if (NULL == This) {
    return EFI_INVALID_PARAMETER;
  }

  Private = XHCICONTROLLER_PRIVATE_DATA_FROM_THIS (This);

  *BaseAddress = Private->XusbSoc->BaseAddress;
  return EFI_SUCCESS;
}

/* XhciController Protocol Function used to return the Address
 * of XHCI Configuration Registers
 */
EFI_STATUS
XhciGetCfgAddr (
  IN  NVIDIA_XHCICONTROLLER_PROTOCOL  *This,
  OUT EFI_PHYSICAL_ADDRESS            *CfgAddress
  )
{
  XHCICONTROLLER_DXE_PRIVATE  *Private;

  if (NULL == This) {
    return EFI_INVALID_PARAMETER;
  }

  Private     = XHCICONTROLLER_PRIVATE_DATA_FROM_THIS (This);
  *CfgAddress = Private->XusbSoc->CfgAddress;

  return EFI_SUCCESS;
}

VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  XHCICONTROLLER_DXE_PRIVATE       *Private;
  EFI_STATUS                       Status;
  NVIDIA_POWER_GATE_NODE_PROTOCOL  *PgProtocol;
  UINT32                           Index;
  UINT32                           PgState;
  VOID                             *AcpiBase;
  EFI_PHYSICAL_ADDRESS             BaseAddress;
  UINT8                            CapLength;
  UINT32                           val;

  Private    = (XHCICONTROLLER_DXE_PRIVATE *)Context;
  PgProtocol = NULL;
  PgState    = CmdPgStateOn;

  /* Leave USB active for ACPI boot. */
  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    return;
  }

  BaseAddress = Private->XusbSoc->BaseAddress;
  CapLength   = MmioRead8 (BaseAddress);
  val         = MmioRead32 (BaseAddress + CapLength);

  val = 0;
  MmioWrite32 (BaseAddress + CapLength, val);

  val = MmioRead32 (BaseAddress + CapLength + XUSB_OP_USBSTS);
  DEBUG ((DEBUG_ERROR, "Xhci OnExitBootServices usbsts after stop write: %x\r\n", val));

  /* Do UsbPadCtlDxe DeInit */
  Private->mUsbPadCtlProtocol->DeInitHw (Private->mUsbPadCtlProtocol);

  Status = gBS->HandleProtocol (Private->ControllerHandle, &gNVIDIAPowerGateNodeProtocolGuid, (VOID **)&PgProtocol);
  if (EFI_ERROR (Status)) {
    return;
  }

  for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
    Status = PgProtocol->GetState (PgProtocol, PgProtocol->PowerGateId[Index], &PgState);
    if (EFI_ERROR (Status)) {
      return;
    }

    if (PgState == CmdPgStateOn) {
      Status = PgProtocol->Assert (PgProtocol, PgProtocol->PowerGateId[Index]);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Xhci Assert pg fail: %d\r\n", PgProtocol->PowerGateId[Index]));
        return;
      }
    }
  }

  DeviceDiscoveryHideResources (Private->ControllerHandle);
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
  EFI_STATUS                       Status;
  UINT32                           reg_val;
  EFI_PHYSICAL_ADDRESS             BaseAddress;
  EFI_PHYSICAL_ADDRESS             CfgAddress;
  UINTN                            RegionSize;
  UINT8                            CapLength;
  UINT32                           StatusRegister;
  UINTN                            i;
  XHCICONTROLLER_DXE_PRIVATE       *Private;
  NON_DISCOVERABLE_DEVICE          *Device;
  BOOLEAN                          LoadIfrRom;
  TEGRA_PLATFORM_TYPE              PlatformType;
  NVIDIA_POWER_GATE_NODE_PROTOCOL  *PgProtocol = NULL;
  UINT32                           Index;
  UINT32                           PgState;
  EFI_STATUS                       ErrorStatus;
  VOID                             *ErrorProtocol;

  LoadIfrRom   = FALSE;
  PlatformType = TegraGetPlatform ();

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:

      Private = AllocateZeroPool (sizeof (XHCICONTROLLER_DXE_PRIVATE));
      if (NULL == Private) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory\r\n", __FUNCTION__));
        return EFI_OUT_OF_RESOURCES;
      }

      Device = NULL;
      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gNVIDIANonDiscoverableDeviceProtocolGuid,
                      (VOID **)&Device
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate non discoverable device\n", __FUNCTION__));
        return Status;
      }

      // Force dma-coherent DMA type device.
      Device->DmaType = NonDiscoverableDeviceDmaTypeCoherent;

      /* Assign Platform Specific Parameters */
      if (!EFI_ERROR (
             DeviceTreeCheckNodeSingleCompatibility (
               "nvidia,tegra186-*",
               DeviceTreeNode->NodeOffset
               )
             ))
      {
        Private->XusbSoc = &Tegra186Soc;
      } else if (!EFI_ERROR (
                    DeviceTreeCheckNodeSingleCompatibility (
                      "nvidia,tegra194-*",
                      DeviceTreeNode->NodeOffset
                      )
                    ))
      {
        Private->XusbSoc = &Tegra194Soc;
      } else {
        // Only other supported platform is Tegra234 other targets will use this by default
        Private->XusbSoc = &Tegra234Soc;
        if (!EFI_ERROR (
               DeviceTreeCheckNodeSingleCompatibility (
                 "nvidia,tegra234-*",
                 DeviceTreeNode->NodeOffset
                 )
               ))
        {
          Private->T234Platform = TRUE;
        } else {
          Private->T264Platform = TRUE;
        }
      }

      Status = DeviceDiscoveryGetMmioRegion (
                 ControllerHandle,
                 0,
                 &BaseAddress,
                 &RegionSize
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Unable to locate Xhci Base address range\n",
          __FUNCTION__
          ));
        goto ErrorExit;
      }

      Private->XusbSoc->BaseAddress = BaseAddress;

      Status = DeviceDiscoveryGetMmioRegion (
                 ControllerHandle,
                 1,
                 &CfgAddress,
                 &RegionSize
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Unable to locate Xhci Config address range\n",
          __FUNCTION__
          ));
        goto ErrorExit;
      }

      Private->XusbSoc->CfgAddress = CfgAddress;

      if (Private->T234Platform || Private->T264Platform) {
        Status = DeviceDiscoveryGetMmioRegion (
                   ControllerHandle,
                   2,
                   &BaseAddress,
                   &RegionSize
                   );
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Unable to locate Xhci Base 2 address range\n",
            __FUNCTION__
            ));
          goto ErrorExit;
        }

        Private->XusbSoc->Base2Address = BaseAddress;
      } else {
        Private->XusbSoc->Base2Address = 0;
      }

      Private->Signature                          = XHCICONTROLLER_SIGNATURE;
      Private->ImageHandle                        = DriverHandle;
      Private->XhciControllerProtocol.GetBaseAddr = XhciGetBaseAddr;
      Private->XhciControllerProtocol.GetCfgAddr  = XhciGetCfgAddr;
      Private->ControllerHandle                   = ControllerHandle;

      /* Install the XhciController Protocol */
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &DriverHandle,
                      &gNVIDIAXhciControllerProtocolGuid,
                      &Private->XhciControllerProtocol,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a, Failed to install protocols: %r\r\n",
          __FUNCTION__,
          Status
          ));
        goto ErrorExit;
      }

      Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIAPowerGateNodeProtocolGuid, (VOID **)&PgProtocol);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "PowerGateNodeProtocol not found\r\n"));
        goto ErrorExit;
      }

      // Unpowergate XUSBA/XUSBC partition first in XHCI DT
      for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
        DEBUG ((DEBUG_VERBOSE, "Deassert pg: %d\r\n", PgProtocol->PowerGateId[Index]));
        Status = PgProtocol->Deassert (PgProtocol, PgProtocol->PowerGateId[Index]);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Deassert pg not found\r\n"));
          goto ErrorExit;
        }
      }

      // Powergate XUSBA/XUSBC partition again to make it in default state
      for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
        DEBUG ((DEBUG_VERBOSE, "Assert pg: %d\r\n", PgProtocol->PowerGateId[Index]));
        Status = PgProtocol->Assert (PgProtocol, PgProtocol->PowerGateId[Index]);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Assert pg not found\r\n"));
        }
      }

      // Only unpowergate XUSBA/XUSBC in XHCI DT
      for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
        DEBUG ((DEBUG_VERBOSE, "Deassert pg: %d\r\n", PgProtocol->PowerGateId[Index]));
        Status = PgProtocol->Deassert (PgProtocol, PgProtocol->PowerGateId[Index]);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Deassert pg not found\r\n"));
        }
      }

      Status = gBS->LocateProtocol (
                      &gNVIDIAUsbPadCtlProtocolGuid,
                      NULL,
                      (VOID **)&(Private->mUsbPadCtlProtocol)
                      );
      if (EFI_ERROR (Status) || (Private->mUsbPadCtlProtocol == NULL)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Couldn't find UsbPadCtl Protocol Handle %r\n",
          __FUNCTION__,
          Status
          ));
        goto ErrorExit;
      }

      /* Pass Xhci Config Address to Falcon Library before using Library's
       * other functions */
      FalconSetHostCfgAddr (CfgAddress);

      /* Set Base 2 adress, only valid in T234 & T264 */
      if (Private->T234Platform || Private->T264Platform) {
        FalconSetHostBase2Addr (Private->XusbSoc->Base2Address);
      }

      DEBUG ((DEBUG_INFO, "%a: before UsbPadCtl Init\n", __FUNCTION__));
      /* Initialize USB Pad Registers */
      Status = Private->mUsbPadCtlProtocol->InitHw (Private->mUsbPadCtlProtocol);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a, Failed to Initailize USB HW: %r\r\n",
          __FUNCTION__,
          Status
          ));
        goto ErrorExit;
      }

      DEBUG ((DEBUG_INFO, "%a: before XUSB_CFG_4_0 Init\n", __FUNCTION__));
      /* Program Xhci PCI Cfg Registers */
      reg_val  = MmioRead32 (CfgAddress + XUSB_CFG_4_0);
      reg_val &= ~(Private->XusbSoc->Cfg4AddrMask << Private->XusbSoc->Cfg4AddrShift);
      reg_val |= Private->XusbSoc->BaseAddress &
                 (Private->XusbSoc->Cfg4AddrMask << Private->XusbSoc->Cfg4AddrShift);
      MmioWrite32 (CfgAddress + XUSB_CFG_4_0, reg_val);

      DeviceDiscoveryThreadMicroSecondDelay (200);

      if (Private->T234Platform || Private->T264Platform) {
        DEBUG ((DEBUG_INFO, "%a: before XUSB_CFG_7_0 Init\n", __FUNCTION__));
        reg_val  = MmioRead32 (CfgAddress + XUSB_CFG_7_0);
        reg_val &= ~(Private->XusbSoc->Cfg7AddrMask << Private->XusbSoc->Cfg7AddrShift);
        reg_val |= Private->XusbSoc->Base2Address &
                   (Private->XusbSoc->Cfg7AddrMask << Private->XusbSoc->Cfg7AddrShift);
        MmioWrite32 (CfgAddress + XUSB_CFG_7_0, reg_val);

        DeviceDiscoveryThreadMicroSecondDelay (200);

        reg_val = MmioRead32 (CfgAddress + XUSB_CFG_AXI_CFG_0);
        reg_val = 0x5;
        MmioWrite32 (CfgAddress + XUSB_CFG_AXI_CFG_0, reg_val);

        DeviceDiscoveryThreadMicroSecondDelay (100);
      }

      DEBUG ((DEBUG_INFO, "%a: before XUSB_CFG_1_0 Init\n", __FUNCTION__));
      reg_val = MmioRead32 (CfgAddress + XUSB_CFG_1_0);
      reg_val =
        NV_FLD_SET_DRF_DEF (XUSB_CFG, 1, MEMORY_SPACE, ENABLED, reg_val);
      reg_val = NV_FLD_SET_DRF_DEF (XUSB_CFG, 1, BUS_MASTER, ENABLED, reg_val);
      MmioWrite32 (CfgAddress + XUSB_CFG_1_0, reg_val);

      BaseAddress = Private->XusbSoc->BaseAddress;

      if (Private->T234Platform || Private->T264Platform) {
        /* Check if HW/FW Clears Controller Not Ready Flag */
        CapLength = MmioRead8 (BaseAddress);

        for (i = 0; i < 200; i++) {
          StatusRegister = MmioRead32 (BaseAddress + CapLength + XUSB_OP_USBSTS);
          if (!(StatusRegister & USBSTS_CNR)) {
            break;
          }

          DeviceDiscoveryThreadMicroSecondDelay (1000);
        }
      }

      /* Return Error if CNR is not cleared or Host Controller Error is set */
      if (StatusRegister & (USBSTS_CNR | USBSTS_HCE)) {
        DEBUG ((DEBUG_ERROR, "%a:%d %llx - %r\r\n", __func__, __LINE__, BaseAddress, Status));
        DEBUG ((DEBUG_ERROR, "Usb Host Controller Initialization Failed\n"));
        DEBUG ((DEBUG_ERROR, "UsbStatus: 0x%x Falcon CPUCTL: 0x%x\n", StatusRegister, FalconRead32 (FALCON_CPUCTL_0)));
        Status = EFI_DEVICE_ERROR;
        goto ErrorExit;
      }

      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_NOTIFY,
                      OnExitBootServices,
                      Private,
                      &gEfiEventExitBootServicesGuid,
                      &Private->ExitBootServicesEvent
                      );
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      if (gDeviceDiscoverDriverConfig.SkipEdkiiNondiscoverableInstall) {
        Status = gBS->InstallMultipleProtocolInterfaces (
                        &ControllerHandle,
                        &gEdkiiNonDiscoverableDeviceProtocolGuid,
                        Device,
                        NULL
                        );
        ASSERT_EFI_ERROR (Status);
      }

      break;
    default:
      break;
  }

  return EFI_SUCCESS;

ErrorExit:
  if (PgProtocol != NULL) {
    /* Assert the PG in error case */
    for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
      ErrorStatus = PgProtocol->GetState (PgProtocol, PgProtocol->PowerGateId[Index], &PgState);
      if (EFI_ERROR (ErrorStatus)) {
        DEBUG ((DEBUG_ERROR, "Xhci pg GetState fail: %d\r\n", PgProtocol->PowerGateId[Index]));
      } else {
        /* Assert the PG if it is ON */
        if (PgState == CmdPgStateOn) {
          ErrorStatus = PgProtocol->Assert (PgProtocol, PgProtocol->PowerGateId[Index]);
          if (EFI_ERROR (ErrorStatus)) {
            DEBUG ((DEBUG_ERROR, "Xhci Assert pg fail: %d\r\n", PgProtocol->PowerGateId[Index]));
          }
        }
      }
    }
  }

  ErrorStatus = gBS->HandleProtocol (
                       DriverHandle,
                       &gNVIDIAXhciControllerProtocolGuid,
                       &ErrorProtocol
                       );
  if (!EFI_ERROR (ErrorStatus)) {
    ErrorStatus = gBS->UninstallMultipleProtocolInterfaces (
                         DriverHandle,
                         &gNVIDIAXhciControllerProtocolGuid,
                         ErrorProtocol,
                         NULL
                         );
    DEBUG ((DEBUG_ERROR, "%a: uninstalled xhci: %r\n", __FUNCTION__, ErrorStatus));
  }

  if (Private->ExitBootServicesEvent != NULL) {
    ErrorStatus = gBS->CloseEvent (Private->ExitBootServicesEvent);
    DEBUG ((DEBUG_ERROR, "%a: closed event:%r\n", __FUNCTION__, ErrorStatus));
  }

  FreePool (Private);
  return Status;
}
