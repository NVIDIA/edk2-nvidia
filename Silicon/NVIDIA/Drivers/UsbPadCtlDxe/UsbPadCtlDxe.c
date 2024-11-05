/** @file

  Usb Pad Control Driver

  SPDX-FileCopyrightText: Copyright (c) 2019-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/MemoryAllocationLib.h>
#include <Library/UsbFalconLib.h>
#include <Protocol/Regulator.h>
#include <Protocol/EFuse.h>
#include <Protocol/PinMux.h>
#include "Uefi/UefiBaseType.h"
#include "UsbPadCtlPrivate.h"

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra194-xusb-padctl", &gNVIDIANonDiscoverableT194UsbPadDeviceGuid    },
  { "nvidia,tegra234-xusb-padctl", &gNVIDIANonDiscoverableT234UsbPadDeviceGuid    },
  { "nvidia,tegra264-xusb-padctl", &gNVIDIANonDiscoverableCurrentUsbPadDeviceGuid },
  { NULL,                          NULL                                           }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA USB Pad controller driver",
  .AutoEnableClocks                = TRUE,
  .AutoDeassertReset               = TRUE,
  .AutoResetModule                 = FALSE,
  .AutoDeassertPg                  = FALSE,
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                 Status;
  USBPADCTL_DXE_PRIVATE      *Private;
  NVIDIA_REGULATOR_PROTOCOL  *mRegulator;
  NVIDIA_EFUSE_PROTOCOL      *mEfuse;
  NVIDIA_PINMUX_PROTOCOL     *mPmux;
  SCMI_CLOCK2_PROTOCOL       *mClockProtocol;
  NVIDIA_USBPADCTL_PROTOCOL  *mUsbPadCtlProtocol;
  EFI_PHYSICAL_ADDRESS       BaseAddress;
  UINTN                      RegionSize;
  NON_DISCOVERABLE_DEVICE    *NonDiscoverableProtocol;

  Status         = EFI_SUCCESS;
  Private        = NULL;
  mRegulator     = NULL;
  mEfuse         = NULL;
  mPmux          = NULL;
  mClockProtocol = NULL;
  BaseAddress    = 0;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:

      Private = AllocateZeroPool (sizeof (USBPADCTL_DXE_PRIVATE));
      if (NULL == Private) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to allocate private data stucture\r\n",
          __FUNCTION__
          ));
        return EFI_OUT_OF_RESOURCES;
      }

      Status = gBS->LocateProtocol (
                      &gNVIDIARegulatorProtocolGuid,
                      NULL,
                      (VOID **)&mRegulator
                      );
      if (EFI_ERROR (Status) || (mRegulator == NULL)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Couldn't get gNVIDIARegulatorProtocolGuid Handle: %r\n",
          __FUNCTION__,
          Status
          ));
        goto ErrorExit;
      }

      Status = gBS->LocateProtocol (
                      &gNVIDIAEFuseProtocolGuid,
                      NULL,
                      (VOID **)&mEfuse
                      );
      if (EFI_ERROR (Status) || (mEfuse == NULL)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Couldn't get gNVIDIAEFuseProtocolGuid Handle: %r\n",
          __FUNCTION__,
          Status
          ));
        goto ErrorExit;
      }

      Status = gBS->LocateProtocol (
                      &gArmScmiClock2ProtocolGuid,
                      NULL,
                      (VOID **)&mClockProtocol
                      );
      if (EFI_ERROR (Status) || (mClockProtocol == NULL)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Couldn't get gArmScmiClock2ProtocolGuid Handle: %r\n",
          __FUNCTION__,
          Status
          ));
        goto ErrorExit;
      }

      /* Assign Platform Specific Parameters */
      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gNVIDIANonDiscoverableDeviceProtocolGuid,
                      (VOID **)&NonDiscoverableProtocol
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Couldn't get gNVIDIANonDiscoverableDeviceProtocolGuid Handle: %r\n",
          __FUNCTION__,
          Status
          ));
        goto ErrorExit;
      }

      if (CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableT194UsbPadDeviceGuid)) {
        Private->mUsbPadCtlProtocol.InitHw      = InitUsbHw194;
        Private->mUsbPadCtlProtocol.DeInitHw    = DeInitUsbHw194;
        Private->mUsbPadCtlProtocol.InitDevHw   = NULL;
        Private->mUsbPadCtlProtocol.DeInitDevHw = NULL;
        Private->PlatConfig                     = Tegra194UsbConfig;

        /* Initialize Platform specific USB Ports information from DT */
        Status = InitPlatInfo194 (DeviceTreeNode, &Private->PlatConfig);
        if (Status != EFI_SUCCESS) {
          return Status;
        }
      } else if ((CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableT234UsbPadDeviceGuid)) ||
                 (CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableCurrentUsbPadDeviceGuid)))
      {
        // Both T234 and current USB Pad controllers are handled by this path.
        // If new hardare strings are not compatible with the T234 path new logic
        // will be needed.
        Private->mUsbPadCtlProtocol.InitHw      = InitUsbHw234;
        Private->mUsbPadCtlProtocol.DeInitHw    = DeInitUsbHw234;
        Private->mUsbPadCtlProtocol.InitDevHw   = InitUsbDevHw234;
        Private->mUsbPadCtlProtocol.DeInitDevHw = DeInitUsbDevHw234;
        Private->PlatConfig                     = Tegra234UsbConfig;

        if (CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableT234UsbPadDeviceGuid)) {
          Private->T234Platform = TRUE;
        } else {
          Private->T264Platform = TRUE;
        }

        /* Initialize Platform specific USB Ports information from DT */
        Status = InitPlatInfo234 (DeviceTreeNode, Private);
        if (Status != EFI_SUCCESS) {
          return Status;
        }
      } else {
        // This path is a catch all for updating the compatiblity mapping array
        // Without updating the logic here. This should never be hit.
        DEBUG ((
          DEBUG_ERROR,
          "%a: Unexepected UsbPadCtl Device\n",
          __FUNCTION__
          ));
        ASSERT (FALSE);
        Status = EFI_UNSUPPORTED;
        goto ErrorExit;
      }

      Status = gBS->LocateProtocol (
                      &gNVIDIAPinMuxProtocolGuid,
                      NULL,
                      (VOID **)&mPmux
                      );
      if (EFI_ERROR (Status) || (mPmux == NULL)) {
        if (!Private->T234Platform && !Private->T264Platform) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Couldn't get gNVIDIAPinMuxProtocolGuid Handle: %r\n",
            __FUNCTION__,
            Status
            ));
          goto ErrorExit;
        }
      }

      if (Private->T234Platform || Private->T264Platform) {
        Status = DeviceDiscoveryGetMmioRegion (
                   ControllerHandle,
                   1,
                   &BaseAddress,
                   &RegionSize
                   );
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Unable to locate Xhci AO address range\n",
            __FUNCTION__
            ));
          goto ErrorExit;
        }

        FalconSetAoAddr (BaseAddress);
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
          "%a: Unable to locate UsbPadCtl Base address range\n",
          __FUNCTION__
          ));
        goto ErrorExit;
      }

      Private->Signature      = PADCTL_SIGNATURE;
      Private->BaseAddress    = BaseAddress;
      Private->ImageHandle    = DriverHandle;
      Private->mRegulator     = mRegulator;
      Private->mEfuse         = mEfuse;
      Private->mPmux          = mPmux;
      Private->mClockProtocol = mClockProtocol;

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &DriverHandle,
                      &gNVIDIAUsbPadCtlProtocolGuid,
                      &Private->mUsbPadCtlProtocol,
                      NULL
                      );
      if (!EFI_ERROR (Status)) {
        return Status;
      }

ErrorExit:
      if (Private != NULL) {
        FreePool (Private);
      }

      break;

    case DeviceDiscoveryDriverBindingStop:

      Status = gBS->HandleProtocol (
                      DriverHandle,
                      &gNVIDIAUsbPadCtlProtocolGuid,
                      (VOID **)&mUsbPadCtlProtocol
                      );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Private = PADCTL_PRIVATE_DATA_FROM_PROTOCOL (mUsbPadCtlProtocol);

      Status =  gBS->UninstallMultipleProtocolInterfaces (
                       DriverHandle,
                       &gNVIDIAUsbPadCtlProtocolGuid,
                       &Private->mUsbPadCtlProtocol,
                       NULL
                       );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if (Private != NULL) {
        FreePool (Private);
      }

      break;

    default:
      break;
  }

  return Status;
}
