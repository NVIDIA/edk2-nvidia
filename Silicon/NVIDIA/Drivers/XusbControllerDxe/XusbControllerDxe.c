/** @file

  XUDC Driver

  Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/PowerGateNodeProtocol.h>

#define XUSB_DEV_XHCI_CTRL_0_OFFSET   0x30
#define XUSB_DEV_XHCI_CTRL_0_RUN_BIT  0

typedef struct {
  EFI_PHYSICAL_ADDRESS    XudcBaseAddress;
  EFI_HANDLE              ControllerHandle;
  EFI_EVENT               ExitBootServicesEvent;
} XUDC_CONTROLLER_PRIVATE_DATA;

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra194-xudc", &gNVIDIANonDiscoverableXudcDeviceGuid },
  { "nvidia,tegra234-xudc", &gNVIDIANonDiscoverableXudcDeviceGuid },
  { NULL,                   NULL                                  }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA Xudc controller driver",
  .UseDriverBinding                = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE
};

VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  XUDC_CONTROLLER_PRIVATE_DATA     *Private;
  EFI_STATUS                       Status;
  NVIDIA_POWER_GATE_NODE_PROTOCOL  *PgProtocol;
  UINT32                           Index;
  UINT32                           PgState;
  VOID                             *AcpiBase;
  VOID                             *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO     *PlatformResourceInfo;

  Private = (XUDC_CONTROLLER_PRIVATE_DATA *)Context;

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    return;
  }

  PgProtocol = NULL;
  PgState    = CmdPgStateOn;
  Status     = gBS->HandleProtocol (Private->ControllerHandle, &gNVIDIAPowerGateNodeProtocolGuid, (VOID **)&PgProtocol);
  if (EFI_ERROR (Status)) {
    return;
  }

  for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
    Status = PgProtocol->GetState (PgProtocol, PgProtocol->PowerGateId[Index], &PgState);
    if (EFI_ERROR (Status)) {
      return;
    }

    if (PgState != CmdPgStateOn) {
      break;
    }
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to get PlatformResourceInfo\n"));
    return;
  }

  if ((PlatformResourceInfo->BootType == TegrablBootRcm) &&
      (Private->XudcBaseAddress != 0) &&
      (PgState == CmdPgStateOn))
  {
    MmioBitFieldWrite32 (
      Private->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0_OFFSET,
      XUSB_DEV_XHCI_CTRL_0_RUN_BIT,
      XUSB_DEV_XHCI_CTRL_0_RUN_BIT,
      0
      );
  }

  for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
    Status = PgProtocol->Deassert (PgProtocol, PgProtocol->PowerGateId[Index]);
    if (EFI_ERROR (Status)) {
      return;
    }
  }

  for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
    Status = PgProtocol->Assert (PgProtocol, PgProtocol->PowerGateId[Index]);
    if (EFI_ERROR (Status)) {
      return;
    }
  }

  return;
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
  EFI_STATUS                    Status;
  NON_DISCOVERABLE_DEVICE       *Device;
  EFI_PHYSICAL_ADDRESS          BaseAddress;
  UINTN                         RegionSize;
  XUDC_CONTROLLER_PRIVATE_DATA  *Private;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:
      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gNVIDIANonDiscoverableDeviceProtocolGuid,
                      (VOID **)&Device
                      );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      BaseAddress = 0;
      if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableXudcDeviceGuid)) {
        Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
        if (EFI_ERROR (Status)) {
          return Status;
        }
      }

      Private = NULL;
      Private = AllocateZeroPool (sizeof (XUDC_CONTROLLER_PRIVATE_DATA));
      if (Private == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      Private->XudcBaseAddress  = BaseAddress;
      Private->ControllerHandle = ControllerHandle;

      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_NOTIFY,
                      OnExitBootServices,
                      Private,
                      &gEfiEventExitBootServicesGuid,
                      &Private->ExitBootServicesEvent
                      );
      if (EFI_ERROR (Status)) {
        FreePool (Private);
      }

      break;

    default:
      return EFI_SUCCESS;
  }

  return Status;
}
