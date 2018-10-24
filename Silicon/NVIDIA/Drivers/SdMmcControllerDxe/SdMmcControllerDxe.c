/** @file

  SD MMC Controller Driver

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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
#include <libfdt.h>

#include <Protocol/SdMmcOverride.h>
#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include <Protocol/ClockNodeProtocol.h>
#include <Protocol/ResetNodeProtocol.h>
#include <Protocol/ArmScmiClockProtocol.h>

#include "SdMmcControllerPrivate.h"

/**

  Override function for SDHCI capability bits

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in,out]  SdMmcHcSlotCapability The SDHCI capability structure.

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.
  @retval EFI_INVALID_PARAMETER SdMmcHcSlotCapability is NULL

**/
EFI_STATUS
SdMmcCapability (
  IN      EFI_HANDLE                      ControllerHandle,
  IN      UINT8                           Slot,
  IN  OUT VOID                            *SdMmcHcSlotCapability
  )
{
  SD_MMC_HC_SLOT_CAP  *Capability = (SD_MMC_HC_SLOT_CAP *)SdMmcHcSlotCapability;
  EFI_STATUS          Status;
  SCMI_CLOCK_PROTOCOL *ClockProtocol = NULL;
  NVIDIA_CLOCK_NODE_PROTOCOL *ClockNodeProtocol = NULL;
  UINT64              Rate;


  if (SdMmcHcSlotCapability == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateProtocol (&gArmScmiClockProtocolGuid, NULL, (VOID **)&ClockProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIAClockNodeProtocolGuid, (VOID **)&ClockNodeProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ClockProtocol->RateSet (ClockProtocol, ClockNodeProtocol->ClockEntries[0].ClockId, SD_MMC_MAX_CLOCK);
  if (!EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a failed to set clock rate %r\r\n", __FUNCTION__, Status));
  }

  Status = ClockProtocol->RateGet (ClockProtocol, ClockNodeProtocol->ClockEntries[0].ClockId, &Rate);
  if (!EFI_ERROR (Status)) {
    if (Rate > SD_MMC_MAX_CLOCK) {
      DEBUG ((EFI_D_ERROR, "%a: Clock rate %llu out of range for SDHCI\r\n",__FUNCTION__,Rate));
      return EFI_DEVICE_ERROR;
    }
    Capability->BaseClkFreq = Rate / 1000000;
  }

  Capability->Ddr50 = FALSE;
  Capability->SlotType = 0x1; //Embedded slot

  return EFI_SUCCESS;
}

/**

  Override function for SDHCI controller operations

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in]      PhaseType             The type of operation and whether the
                                        hook is invoked right before (pre) or
                                        right after (post)

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.
  @retval EFI_INVALID_PARAMETER PhaseType is invalid

**/
EFI_STATUS
SdMmcNotify (
  IN      EFI_HANDLE                      ControllerHandle,
  IN      UINT8                           Slot,
  IN      EDKII_SD_MMC_PHASE_TYPE         PhaseType
  )
{
  UINT64                              SlotBaseAddress  = 0;

  NON_DISCOVERABLE_DEVICE             *Device;
  EFI_STATUS                          Status;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR   *Desc;
  UINT8                               CurrentResource = 0;

  Status = gBS->HandleProtocol (ControllerHandle,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid, (VOID **)&Device);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // We only support MMIO devices, so iterate over the resources to ensure
  // that they only describe things that we can handle
  //
  for (Desc = Device->Resources; Desc->Desc != ACPI_END_TAG_DESCRIPTOR;
       Desc = (VOID *)((UINT8 *)Desc + Desc->Len + 3)) {
    if (Desc->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR ||
        Desc->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM) {
      break;
    }
    if (CurrentResource == Slot) {
      SlotBaseAddress = Desc->AddrRangeMin;
      break;
    }
  }

  if (SlotBaseAddress == 0) {
    DEBUG ((DEBUG_ERROR, "SdMmcNotify: Unable to locate address range for slot %d\n", Slot));
    return EFI_UNSUPPORTED;
  }

  if (PhaseType == EdkiiSdMmcInitHostPre) {
    // Scale SDMMC Clock to 102MHz.
  }
  else if (PhaseType == EdkiiSdMmcInitHostPost) {
    // Enable SDMMC Clock again.
    MmioOr32(SlotBaseAddress + SD_MMC_HC_CLOCK_CTRL, SD_MMC_CLK_CTRL_SD_CLK_EN);
  }

  return EFI_SUCCESS;
}


EDKII_SD_MMC_OVERRIDE gSdMmcOverride = {
  EDKII_SD_MMC_OVERRIDE_PROTOCOL_VERSION,
  SdMmcCapability,
  SdMmcNotify
};

/**
  This is function is caused to allow the system to check if this implementation supports
  the device tree node. If EFI_SUCCESS is returned then handle will be created and driver binding
  will occur.

  @param[in]  This                   The instance of the NVIDIA_DEVICE_TREE_BINDING_PROTOCOL.
  @param[in]  Node                   The pointer to the requested node info structure.
  @param[out] DeviceType             Pointer to allow the return of the device type
  @param[out] PciIoInitialize        Pointer to allow return of function that will be called
                                       when the PciIo subsystem connects to this device.
                                       Note that this will may not be called if the device
                                       is not in the boot path.

  @return EFI_SUCCESS               The node is supported by this instance
  @return EFI_UNSUPPORTED           The node is not supported by this instance
**/
EFI_STATUS
DeviceTreeIsSupported (
  IN NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL   *This,
  IN CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL      *Node,
  OUT EFI_GUID                                   **DeviceType,
  OUT NON_DISCOVERABLE_DEVICE_INIT               *PciIoInitialize
  )
{

  if ((Node == NULL) ||
      (DeviceType == NULL) ||
      (PciIoInitialize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((0 != fdt_node_check_compatible (Node->DeviceTreeBase, Node->NodeOffset, "nvidia,tegra186-sdhci")) &&
      (0 != fdt_node_check_compatible (Node->DeviceTreeBase, Node->NodeOffset, "nvidia,tegra194-sdhci"))) {
    return EFI_UNSUPPORTED;
  }

  *DeviceType = &gEdkiiNonDiscoverableSdhciDeviceGuid;
  *PciIoInitialize = NULL;

  return EFI_SUCCESS;
}

NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL gDeviceTreeCompatibilty = {
    DeviceTreeIsSupported
};

/**
  Supported function of Driver Binding protocol for this driver.
  Test to see if this driver supports ControllerHandle.

  @param This                   Protocol instance pointer.
  @param Controller             Handle of device to test.
  @param RemainingDevicePath    A pointer to the device path.
                                it should be ignored by device driver.

  @retval EFI_SUCCESS           This driver supports this device.
  @retval EFI_ALREADY_STARTED   This driver is already running on this device.
  @retval other                 This driver does not support this device.

**/
EFI_STATUS
EFIAPI
SdMmcControllerSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS            Status;
  NON_DISCOVERABLE_DEVICE *NonDiscoverableProtocol = NULL;

  //
  // Attempt to open NonDiscoverable Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **) &NonDiscoverableProtocol,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (CompareGuid (NonDiscoverableProtocol->Type, &gEdkiiNonDiscoverableSdhciDeviceGuid)) {
    Status = EFI_SUCCESS;
  } else {
    Status = EFI_UNSUPPORTED;
  }

  gBS->CloseProtocol (
         Controller,
         &gNVIDIANonDiscoverableDeviceProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  return Status;

}

/**
  This routine is called right after the .Supported() called and
  Start this driver on ControllerHandle.

  @param This                   Protocol instance pointer.
  @param Controller             Handle of device to bind driver to.
  @param RemainingDevicePath    A pointer to the device path.
                                it should be ignored by device driver.

  @retval EFI_SUCCESS           This driver is added to this device.
  @retval EFI_ALREADY_STARTED   This driver is already running on this device.
  @retval other                 Some error occurs when binding this driver to this device.

**/
EFI_STATUS
EFIAPI
SdMmcControllerStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS                        Status;
  NON_DISCOVERABLE_DEVICE           *NonDiscoverableProtocol = NULL;
  NVIDIA_CLOCK_NODE_PROTOCOL        *ClockProtocol = NULL;
  NVIDIA_RESET_NODE_PROTOCOL        *ResetProtocol = NULL;
  //
  // Attempt to open NonDiscoverable Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **) &NonDiscoverableProtocol,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  if (!CompareGuid (NonDiscoverableProtocol->Type, &gEdkiiNonDiscoverableSdhciDeviceGuid)) {
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  Status = gBS->HandleProtocol (Controller, &gNVIDIAClockNodeProtocolGuid, (VOID **)&ClockProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, no clock node protocol\r\n",__FUNCTION__));
    goto ErrorExit;
  }
  Status = ClockProtocol->EnableAll (ClockProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, failed to enable clocks %r\r\n",__FUNCTION__,Status));
    goto ErrorExit;
  }

  Status = gBS->HandleProtocol (Controller, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, no reset node protocol\r\n",__FUNCTION__));
    goto ErrorExit;
  }
  Status = ResetProtocol->DeassertAll (ResetProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, failed to deassert resets %r\r\n",__FUNCTION__,Status));
    goto ErrorExit;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  NonDiscoverableProtocol,
                  NULL
                  );
ErrorExit:
  if (EFI_ERROR (Status)) {

    gBS->CloseProtocol (
          Controller,
          &gNVIDIANonDiscoverableDeviceProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );
  }

  return Status;
}

/**
  Stop this driver on ControllerHandle.

  @param This               Protocol instance pointer.
  @param Controller         Handle of device to stop driver on.
  @param NumberOfChildren   Not used.
  @param ChildHandleBuffer  Not used.

  @retval EFI_SUCCESS   This driver is removed from this device.
  @retval other         Some error occurs when removing this driver from this device.

**/
EFI_STATUS
EFIAPI
SdMmcControllerStop (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN UINTN                          NumberOfChildren,
  IN EFI_HANDLE                     *ChildHandleBuffer
  )
{
  EFI_STATUS                        Status;
  NON_DISCOVERABLE_DEVICE           *NonDiscoverableProtocol = NULL;

  //
  // Attempt to open NonDiscoverable Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **) &NonDiscoverableProtocol,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if (CompareGuid (NonDiscoverableProtocol->Type, &gEdkiiNonDiscoverableSdhciDeviceGuid)) {
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    This->DriverBindingHandle,
                    &gEdkiiNonDiscoverableDeviceProtocolGuid,
                    NonDiscoverableProtocol,
                    NULL
                    );
  } else {
    Status = EFI_UNSUPPORTED;
  }

  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  //
  // Close protocols opened by SdMmc Controller driver
  //
  return gBS->CloseProtocol (
                Controller,
                &gNVIDIANonDiscoverableDeviceProtocolGuid,
                This->DriverBindingHandle,
                Controller
                );
}

///
/// EFI_DRIVER_BINDING_PROTOCOL instance
///
EFI_DRIVER_BINDING_PROTOCOL mDriverBindingProtocol = {
  SdMmcControllerSupported,
  SdMmcControllerStart,
  SdMmcControllerStop,
  0x0,
  NULL,
  NULL
};

/**
  Initialize the SD MMC Controller Protocol Driver

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
SdMmcControllerInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = EfiLibInstallDriverBinding (
             ImageHandle,
             SystemTable,
             &mDriverBindingProtocol,
             ImageHandle);

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to install driver binding protocol: %r\r\n",__FUNCTION__,Status));
    return Status;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mDriverBindingProtocol.DriverBindingHandle,
                  &gEdkiiSdMmcOverrideProtocolGuid,
                  &gSdMmcOverride,
                  &gNVIDIADeviceTreeCompatibilityProtocolGuid,
                  &gDeviceTreeCompatibilty,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}


