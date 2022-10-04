/** @file

  Tegra Pin Control Driver

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Protocol/ResetNodeProtocol.h>
#include <libfdt.h>
#include <Protocol/DeviceTreeCompatibility.h>

#include "TegraPinControlPrivate.h"

#define DPAUX_HYBRID_PADCTL_0  0x124
#define I2C_SDA_INPUT_RCV      BIT15
#define I2C_SCL_INPUT_RCV      BIT14
#define MODE_I2C               BIT0

#define DPAUX_HYBRID_SPARE_0  0x134
#define PAD_POWER             BIT0

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra194-dpaux3-padctl", &gNVIDIANonDiscoverableNVIDIADpAuxDeviceGuid },
  { NULL,                            NULL                                         }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA Tegra Pin Control driver",
  .UseDriverBinding                = FALSE,
  .AutoEnableClocks                = TRUE,
  .AutoDeassertReset               = TRUE,
  .AutoResetModule                 = FALSE,
  .AutoDeassertPg                  = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .DirectEnumerationSupport        = TRUE
};

STATIC
EFI_STATUS
DpAuxPinControlEnable (
  IN  NVIDIA_PIN_CONTROL_PROTOCOL  *This,
  IN  UINT32                       PinControlId
  )
{
  DP_AUX_CONTROL_PRIVATE  *DpAuxPrivate;

  DpAuxPrivate = DP_AUX_CONTROL_PRIVATE_FROM_THIS (This);

  if (PinControlId != DpAuxPrivate->PinControlId) {
    return EFI_NOT_FOUND;
  }

  MmioOr32 (DpAuxPrivate->BaseAddress + DPAUX_HYBRID_PADCTL_0, I2C_SDA_INPUT_RCV|I2C_SCL_INPUT_RCV|MODE_I2C);
  MmioAnd32 (DpAuxPrivate->BaseAddress + DPAUX_HYBRID_SPARE_0, ~PAD_POWER);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
PinControlEnable (
  IN  NVIDIA_PIN_CONTROL_PROTOCOL  *This,
  IN  UINT32                       PinControlId
  )
{
  EFI_STATUS                   Status;
  PIN_CONTROL_PRIVATE          *PinControlPrivate;
  UINTN                        Index;
  NVIDIA_PIN_CONTROL_PROTOCOL  *SubProtocol;

  PinControlPrivate = PIN_CONTROL_PRIVATE_FROM_THIS (This);

  for (Index = 0; Index < PinControlPrivate->NumberOfHandles; Index++) {
    Status = gBS->HandleProtocol (PinControlPrivate->HandleArray[Index], &gNVIDIASubPinControlProtocolGuid, (VOID **)&SubProtocol);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = SubProtocol->Enable (SubProtocol, PinControlId);
    if (Status == EFI_NOT_FOUND) {
      continue;
    } else if (EFI_ERROR (Status)) {
      return Status;
    } else {
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to the device tree node protocol.

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
  EFI_STATUS              Status;
  UINTN                   RegionSize;
  DP_AUX_CONTROL_PRIVATE  *DpAuxPrivate;
  PIN_CONTROL_PRIVATE     *PinControlPrivate;
  INT32                   SubNodeOffset;

  Status = EFI_SUCCESS;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:

      DpAuxPrivate = (DP_AUX_CONTROL_PRIVATE *)AllocateZeroPool (sizeof (DP_AUX_CONTROL_PRIVATE));
      if (DpAuxPrivate == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      DpAuxPrivate->Signature                 = DP_AUX_CONTROL_SIGNATURE;
      DpAuxPrivate->PinControlProtocol.Enable = DpAuxPinControlEnable;

      Status = DeviceDiscoveryGetMmioRegion (
                 ControllerHandle,
                 0,
                 &DpAuxPrivate->BaseAddress,
                 &RegionSize
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          EFI_D_ERROR,
          "%a: Couldn't find PadCtl address range\n",
          __FUNCTION__
          ));
        return Status;
      }

      // Locate the pinmux child.
      SubNodeOffset = fdt_subnode_offset (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "pinmux@0");
      if (SubNodeOffset < 0) {
        return EFI_NOT_FOUND;
      }

      DpAuxPrivate->PinControlId = fdt_get_phandle (DeviceTreeNode->DeviceTreeBase, SubNodeOffset);
      DEBUG ((DEBUG_ERROR, "!!!!PinCtr -%x\r\n", DpAuxPrivate->PinControlId));

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gNVIDIASubPinControlProtocolGuid,
                      &DpAuxPrivate->PinControlProtocol,
                      NULL
                      );
      break;

    case DeviceDiscoveryEnumerationCompleted:
      PinControlPrivate = (PIN_CONTROL_PRIVATE *)AllocateZeroPool (sizeof (PIN_CONTROL_PRIVATE));
      if (PinControlPrivate == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      PinControlPrivate->Signature                 = PIN_CONTROL_SIGNATURE;
      PinControlPrivate->PinControlProtocol.Enable = PinControlEnable;
      Status                                       = gBS->LocateHandleBuffer (ByProtocol, &gNVIDIASubPinControlProtocolGuid, NULL, &PinControlPrivate->NumberOfHandles, &PinControlPrivate->HandleArray);
      if (Status == EFI_NOT_FOUND) {
        PinControlPrivate->NumberOfHandles = 0;
      } else if (EFI_ERROR (Status)) {
        FreePool (PinControlPrivate);
        return Status;
      }

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &DriverHandle,
                      &gNVIDIAPinControlProtocolGuid,
                      &PinControlPrivate->PinControlProtocol,
                      NULL
                      );
      break;

    default:
      break;
  }

  return Status;
}
