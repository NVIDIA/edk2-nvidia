/** @file

  Device Discovery Driver Library

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <libfdt.h>

#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include <Protocol/ClockNodeProtocol.h>
#include <Protocol/ResetNodeProtocol.h>
#include <Protocol/PowerGateNodeProtocol.h>
#include <Protocol/ArmScmiClock2Protocol.h>

#include "DeviceDiscoveryDriverLibPrivate.h"

SCMI_CLOCK2_PROTOCOL           *gScmiClockProtocol    = NULL;
NVIDIA_CLOCK_PARENTS_PROTOCOL  *gClockParentsProtocol = NULL;

VOID
EFIAPI
DeviceDiscoveryOnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                       Status;
  VOID                             *AcpiBase;
  EFI_HANDLE                       Controller;
  NVIDIA_CLOCK_NODE_PROTOCOL       *ClockProtocol = NULL;
  NVIDIA_RESET_NODE_PROTOCOL       *ResetProtocol = NULL;
  NVIDIA_POWER_GATE_NODE_PROTOCOL  *PgProtocol    = NULL;
  UINTN                            Index;

  gBS->CloseEvent (Event);

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    return;
  }

  Controller = Context;

  if (gDeviceDiscoverDriverConfig.AutoDeassertPg) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAPowerGateNodeProtocolGuid, (VOID **)&PgProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, no Pg node protocol\r\n", __FUNCTION__));
      return;
    }

    for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
      Status = PgProtocol->Assert (PgProtocol, PgProtocol->PowerGateId[Index]);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a, failed to assert Pg %x: %r\r\n", __FUNCTION__, PgProtocol->PowerGateId[Index], Status));
        return;
      }
    }
  }

  if (gDeviceDiscoverDriverConfig.AutoEnableClocks) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAClockNodeProtocolGuid, (VOID **)&ClockProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, no clock node protocol\r\n", __FUNCTION__));
      return;
    }

    Status = ClockProtocol->DisableAll (ClockProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to disable clocks %r\r\n", __FUNCTION__, Status));
      return;
    }
  }

  if (gDeviceDiscoverDriverConfig.AutoResetModule) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, no reset node protocol\r\n", __FUNCTION__));
      return;
    }

    Status = ResetProtocol->ModuleResetAll (ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to reset module%r\r\n", __FUNCTION__, Status));
      return;
    }
  } else if (gDeviceDiscoverDriverConfig.AutoDeassertReset) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, no reset node protocol\r\n", __FUNCTION__));
      return;
    }

    Status = ResetProtocol->AssertAll (ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to assert resets %r\r\n", __FUNCTION__, Status));
      return;
    }
  }

  return;
}

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
STATIC
EFI_STATUS
EFIAPI
DeviceDiscoveryBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                        Status;
  NON_DISCOVERABLE_DEVICE           *NonDiscoverableProtocol = NULL;
  NVIDIA_COMPATIBILITY_MAPPING      *MappingNode             = gDeviceCompatibilityMap;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node                    = NULL;

  //
  // Attempt to open NonDiscoverable Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **)&NonDiscoverableProtocol,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->HandleProtocol (Controller, &gNVIDIADeviceTreeNodeProtocolGuid, (VOID **)&Node);
  if (EFI_ERROR (Status)) {
    Node = NULL;
  }

  Status = EFI_UNSUPPORTED;
  while (MappingNode->Compatibility != NULL) {
    if (CompareGuid (NonDiscoverableProtocol->Type, MappingNode->DeviceType)) {
      Status = EFI_SUCCESS;
      break;
    }

    MappingNode++;
  }

  if (!EFI_ERROR (Status)) {
    Status = DeviceDiscoveryNotify (
               DeviceDiscoveryDriverBindingSupported,
               This->DriverBindingHandle,
               Controller,
               Node
               );
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
STATIC
EFI_STATUS
EFIAPI
DeviceDiscoveryBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                        Status;
  NON_DISCOVERABLE_DEVICE           *NonDiscoverableProtocol = NULL;
  NVIDIA_CLOCK_NODE_PROTOCOL        *ClockProtocol           = NULL;
  NVIDIA_RESET_NODE_PROTOCOL        *ResetProtocol           = NULL;
  NVIDIA_POWER_GATE_NODE_PROTOCOL   *PgProtocol              = NULL;
  NVIDIA_COMPATIBILITY_MAPPING      *MappingNode             = gDeviceCompatibilityMap;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node                    = NULL;
  UINTN                             Index;
  NVIDIA_DEVICE_DISCOVERY_CONTEXT   *DeviceDiscoveryContext = NULL;

  //
  // Attempt to open NonDiscoverable Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **)&NonDiscoverableProtocol,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, no NonDiscoverableProtocol\r\n", __FUNCTION__));
    return Status;
  }

  Status = gBS->HandleProtocol (Controller, &gNVIDIADeviceTreeNodeProtocolGuid, (VOID **)&Node);
  if (EFI_ERROR (Status)) {
    Node = NULL;
  }

  Status = EFI_UNSUPPORTED;
  while (MappingNode->Compatibility != NULL) {
    if (CompareGuid (NonDiscoverableProtocol->Type, MappingNode->DeviceType)) {
      Status = EFI_SUCCESS;
      break;
    }

    MappingNode++;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, no guid mapping\r\n", __FUNCTION__));
    goto ErrorExit;
  }

  if (gDeviceDiscoverDriverConfig.AutoDeassertPg) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAPowerGateNodeProtocolGuid, (VOID **)&PgProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, no Pg node protocol\r\n", __FUNCTION__));
      goto ErrorExit;
    }

    for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
      Status = PgProtocol->Deassert (PgProtocol, PgProtocol->PowerGateId[Index]);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a, failed to deassert Pg %x: %r\r\n", __FUNCTION__, PgProtocol->PowerGateId[Index], Status));
        goto ErrorExit;
      }
    }
  }

  if (gDeviceDiscoverDriverConfig.AutoEnableClocks) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAClockNodeProtocolGuid, (VOID **)&ClockProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, no clock node protocol\r\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = ClockProtocol->EnableAll (ClockProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to enable clocks %r\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  }

  if (gDeviceDiscoverDriverConfig.AutoResetModule) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, no reset node protocol\r\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = ResetProtocol->ModuleResetAll (ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to reset module%r\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  } else if (gDeviceDiscoverDriverConfig.AutoDeassertReset) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, no reset node protocol\r\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = ResetProtocol->DeassertAll (ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to deassert resets %r\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (NVIDIA_DEVICE_DISCOVERY_CONTEXT),
                  (VOID **)&DeviceDiscoveryContext
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, driver returned %r to allocate context\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  gBS->SetMem (
         DeviceDiscoveryContext,
         sizeof (NVIDIA_DEVICE_DISCOVERY_CONTEXT),
         0
         );

  if (!gDeviceDiscoverDriverConfig.SkipAutoDeinitControllerOnExitBootServices) {
    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_NOTIFY,
                    DeviceDiscoveryOnExitBootServices,
                    Controller,
                    &gEfiEventExitBootServicesGuid,
                    &DeviceDiscoveryContext->OnExitBootServicesEvent
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, driver returned %r to create event callback\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  }

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryDriverBindingStart,
             This->DriverBindingHandle,
             Controller,
             Node
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, driver returned %r to start notification\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gNVIDIADeviceDiscoveryContextGuid,
                  DeviceDiscoveryContext,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, driver returned %r to install device discovery context guid\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  if (!gDeviceDiscoverDriverConfig.SkipEdkiiNondiscoverableInstall) {
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Controller,
                    &gEdkiiNonDiscoverableDeviceProtocolGuid,
                    NonDiscoverableProtocol,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DeviceDiscoveryNotify (
        DeviceDiscoveryDriverBindingStop,
        This->DriverBindingHandle,
        Controller,
        Node
        );
    }
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (DeviceDiscoveryContext != NULL) {
      if (DeviceDiscoveryContext->OnExitBootServicesEvent != NULL) {
        gBS->CloseEvent (DeviceDiscoveryContext->OnExitBootServicesEvent);
      }

      gBS->FreePool (DeviceDiscoveryContext);
    }

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
STATIC
EFI_STATUS
EFIAPI
DeviceDiscoveryBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS                        Status;
  NON_DISCOVERABLE_DEVICE           *NonDiscoverableProtocol = NULL;
  NVIDIA_COMPATIBILITY_MAPPING      *MappingNode             = gDeviceCompatibilityMap;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node                    = NULL;
  NVIDIA_DEVICE_DISCOVERY_CONTEXT   *DeviceDiscoveryContext  = NULL;

  if (NumberOfChildren != 0) {
    return EFI_UNSUPPORTED;
  }

  //
  // Attempt to open NonDiscoverable Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **)&NonDiscoverableProtocol,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = EFI_UNSUPPORTED;
  while (MappingNode->Compatibility != NULL) {
    if (CompareGuid (NonDiscoverableProtocol->Type, MappingNode->DeviceType)) {
      Status = EFI_SUCCESS;
      break;
    }

    MappingNode++;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->HandleProtocol (Controller, &gNVIDIADeviceTreeNodeProtocolGuid, (VOID **)&Node);
  if (EFI_ERROR (Status)) {
    Node = NULL;
  }

  if (!gDeviceDiscoverDriverConfig.SkipEdkiiNondiscoverableInstall) {
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    This->DriverBindingHandle,
                    &gEdkiiNonDiscoverableDeviceProtocolGuid,
                    NonDiscoverableProtocol,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
  }

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryDriverBindingStop,
             This->DriverBindingHandle,
             Controller,
             Node
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->HandleProtocol (
                  Controller,
                  &gNVIDIADeviceDiscoveryContextGuid,
                  (VOID **)&DeviceDiscoveryContext
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!gDeviceDiscoverDriverConfig.SkipAutoDeinitControllerOnExitBootServices) {
    gBS->CloseEvent (DeviceDiscoveryContext->OnExitBootServicesEvent);
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Controller,
                  &gNVIDIADeviceDiscoveryContextGuid,
                  DeviceDiscoveryContext,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, driver returned %r to uninstall device discovery context guid\r\n", __FUNCTION__, Status));
    return Status;
  }

  gBS->FreePool (DeviceDiscoveryContext);

  //
  // Close protocols opened by Controller driver
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
STATIC EFI_DRIVER_BINDING_PROTOCOL  mDriverBindingProtocol = {
  DeviceDiscoveryBindingSupported,
  DeviceDiscoveryBindingStart,
  DeviceDiscoveryBindingStop,
  0x0,
  NULL,
  NULL
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
STATIC
EFI_STATUS
DeviceTreeIsSupported (
  IN NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL  *This,
  IN CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL     *Node,
  OUT EFI_GUID                                  **DeviceType,
  OUT NON_DISCOVERABLE_DEVICE_INIT              *PciIoInitialize
  )
{
  NVIDIA_COMPATIBILITY_MAPPING  *MappingNode = gDeviceCompatibilityMap;

  if ((Node == NULL) ||
      (DeviceType == NULL) ||
      (PciIoInitialize == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  while (MappingNode->Compatibility != NULL) {
    if (0 == fdt_node_check_compatible (Node->DeviceTreeBase, Node->NodeOffset, MappingNode->Compatibility)) {
      break;
    }

    MappingNode++;
  }

  if (MappingNode->Compatibility == NULL) {
    return EFI_UNSUPPORTED;
  }

  *DeviceType      = MappingNode->DeviceType;
  *PciIoInitialize = NULL;

  return DeviceDiscoveryNotify (
           DeviceDiscoveryDeviceTreeCompatibility,
           mDriverBindingProtocol.DriverBindingHandle,
           NULL,
           Node
           );
}

STATIC NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL  gDeviceTreeCompatibilty = {
  DeviceTreeIsSupported
};

/**
  Initialize the Device Discovery Driver

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
DeviceDiscoveryDriverInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  mDriverBindingProtocol.DriverBindingHandle = ImageHandle;

  Status = gBS->LocateProtocol (&gArmScmiClock2ProtocolGuid, NULL, (VOID **)&gScmiClockProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->LocateProtocol (&gNVIDIAClockParentsProtocolGuid, NULL, (VOID **)&gClockParentsProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (gDeviceDiscoverDriverConfig.UseDriverBinding) {
    Status = EfiLibInstallDriverBinding (
               ImageHandle,
               SystemTable,
               &mDriverBindingProtocol,
               ImageHandle
               );

    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to install driver binding protocol: %r\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryDriverStart,
             mDriverBindingProtocol.DriverBindingHandle,
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mDriverBindingProtocol.DriverBindingHandle,
                  &gNVIDIADeviceTreeCompatibilityProtocolGuid,
                  &gDeviceTreeCompatibilty,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
