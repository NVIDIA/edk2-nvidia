/** @file

  Device Discovery Driver Library

  SPDX-FileCopyrightText: Copyright (c) 2018-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>
#include <Library/SystemContextLib.h>
#include <libfdt.h>

#include <Protocol/AsyncDriverStatus.h>
#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include <Protocol/ClockNodeProtocol.h>
#include <Protocol/ResetNodeProtocol.h>
#include <Protocol/PowerGateNodeProtocol.h>
#include <Protocol/ArmScmiClock2Protocol.h>

#include "DeviceDiscoveryDriverLibPrivate.h"

// These globals are defined be driver not for the full system context
SCMI_CLOCK2_PROTOCOL                           *gScmiClockProtocol    = NULL;
NVIDIA_CLOCK_PARENTS_PROTOCOL                  *gClockParentsProtocol = NULL;
STATIC EFI_HANDLE                              mImageHandle           = NULL;
STATIC EFI_SYSTEM_CONTEXT_AARCH64              MainContext            = { 0 };
STATIC UINTN                                   SubThreadsRunning      = 0;
STATIC NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL     *AsyncProtocol         = NULL;
STATIC NVIDIA_DEVICE_DISCOVERY_THREAD_CONTEXT  *CurrentThread         = NULL;
BOOLEAN                                        EnumerationCompleted   = FALSE;

/**
  Gets info on if an async driver is still running.

  @param[in]     This                The instance of the NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL.
  @param[out]    StillPending        This driver is still running setup

  @retval EFI_SUCCESS                Status was returned.
  @retval EFI_INVALID_PARAMETER      StillPending is NULL.
  @retval others                     Error processing status
**/
EFI_STATUS
DeviceDiscoveryAsyncStatus (
  IN  NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL  *This,
  IN  BOOLEAN                              *StillPending
  )
{
  if (StillPending == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *StillPending = (SubThreadsRunning != 0);
  return EFI_SUCCESS;
}

VOID
EFIAPI
DeviceDiscoveryHideResources (
  EFI_HANDLE  ControllerHandle
  )
{
  EFI_STATUS                         Status;
  NON_DISCOVERABLE_DEVICE            *Device;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Desc;

  Device = NULL;
  Desc   = NULL;

  Status = gBS->HandleProtocol (ControllerHandle, &gEdkiiNonDiscoverableDeviceProtocolGuid, (VOID **)&Device);
  if (EFI_ERROR (Status)) {
    Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIANonDiscoverableDeviceProtocolGuid, (VOID **)&Device);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, no non discoverable device protocol\r\n", __FUNCTION__));
      return;
    }
  }

  if ((Device != NULL) &&
      (Device->Resources != NULL))
  {
    Desc       = Device->Resources;
    Desc->Desc = ACPI_END_TAG_DESCRIPTOR;
  }
}

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

  Controller = Context;

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryOnExit,
             mImageHandle,
             Controller,
             NULL
             );
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    return;
  }

  if (gDeviceDiscoverDriverConfig.AutoDeassertPg) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAPowerGateNodeProtocolGuid, (VOID **)&PgProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, no Pg node protocol\r\n", __FUNCTION__));
      return;
    }

    for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
      Status = PgProtocol->Assert (PgProtocol, PgProtocol->PowerGateId[Index]);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a, failed to assert Pg %x: %r\r\n", __FUNCTION__, PgProtocol->PowerGateId[Index], Status));
        return;
      }
    }
  }

  if (gDeviceDiscoverDriverConfig.AutoEnableClocks) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAClockNodeProtocolGuid, (VOID **)&ClockProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, no clock node protocol\r\n", __FUNCTION__));
      return;
    }

    Status = ClockProtocol->DisableAll (ClockProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, failed to disable clocks %r\r\n", __FUNCTION__, Status));
      return;
    }
  }

  if (gDeviceDiscoverDriverConfig.AutoResetModule) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, no reset node protocol\r\n", __FUNCTION__));
      return;
    }

    Status = ResetProtocol->ModuleResetAll (ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, failed to reset module%r\r\n", __FUNCTION__, Status));
      return;
    }
  } else if (gDeviceDiscoverDriverConfig.AutoDeassertReset) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, no reset node protocol\r\n", __FUNCTION__));
      return;
    }

    Status = ResetProtocol->AssertAll (ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, failed to assert resets %r\r\n", __FUNCTION__, Status));
      return;
    }
  }

  DeviceDiscoveryHideResources (Controller);

  return;
}

/**
  @brief Timer event callback. When this fires we switch back to the driver
  context until it yields again.

  @param Context Thread Context
**/
VOID
DeviceDiscoveryThreadCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  CurrentThread = (NVIDIA_DEVICE_DISCOVERY_THREAD_CONTEXT *)Context;
  SwapSystemContext (
    (EFI_SYSTEM_CONTEXT)&MainContext,
    (EFI_SYSTEM_CONTEXT)&CurrentThread->Context
    );
}

/**
  Stalls for at least the given number of microseconds.

  Switches back to main thread for at least the given number of microseconds

  @param  MicroSeconds  The minimum number of microseconds to delay.

  @return The value of MicroSeconds specified.

**/
UINTN
EFIAPI
DeviceDiscoveryThreadMicroSecondDelay (
  IN      UINTN  MicroSeconds
  )
{
  NVIDIA_DEVICE_DISCOVERY_THREAD_CONTEXT  *ThreadContext;

  if (CurrentThread == NULL) {
    return MicroSecondDelay (MicroSeconds);
  }

  ThreadContext = CurrentThread;
  gBS->SetTimer (ThreadContext->Timer, TimerRelative, MicroSeconds*10);
  CurrentThread = NULL;
  SwapSystemContext (
    (EFI_SYSTEM_CONTEXT)&ThreadContext->Context,
    (EFI_SYSTEM_CONTEXT)&MainContext
    );
  return MicroSeconds;
}

/**
  @brief Wrapper function for driver start

  @param ThreadContext Thread context info

**/
STATIC
VOID
DeviceThreadMain (
  IN NVIDIA_DEVICE_DISCOVERY_THREAD_CONTEXT  *ThreadContext
  )
{
  EFI_STATUS  Status;

  CurrentThread = ThreadContext;
  SubThreadsRunning++;

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryDriverBindingStart,
             ThreadContext->DriverHandle,
             ThreadContext->Controller,
             ThreadContext->Node
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, driver returned %r to start notification\r\n", __FUNCTION__, Status));
  }

  CurrentThread = NULL;
  SubThreadsRunning--;
  gBS->CloseEvent (ThreadContext->Timer);

  if (EnumerationCompleted && (SubThreadsRunning == 0)) {
    Status = DeviceDiscoveryNotify (
               DeviceDiscoveryEnumerationCompleted,
               mImageHandle,
               NULL,
               NULL
               );
  }

  SwapSystemContext (
    (EFI_SYSTEM_CONTEXT)&ThreadContext->Context,
    (EFI_SYSTEM_CONTEXT)&MainContext
    );

  // Should never get here
  ASSERT (FALSE);
  CpuDeadLoop ();
}

/**
  @brief Start device initialization in a subthread

  @param DriverHandle  Handle of Driver
  @param Controller    Handle of Contoller
  @param Node          DeviceTree Node of controller

  @retval EFI_SUCCESS  Thread was started
  @retval others       Failure to start thread
**/
STATIC
EFI_STATUS
EFIAPI
ThreadedDeviceStart (
  IN EFI_HANDLE                        DriverHandle,
  IN EFI_HANDLE                        Controller,
  IN NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node
  )
{
  EFI_STATUS                              Status;
  NVIDIA_DEVICE_DISCOVERY_THREAD_CONTEXT  *NewContext;
  EFI_TPL                                 OldTpl;
  UINTN                                   ThreadStackPages;

  NewContext = (NVIDIA_DEVICE_DISCOVERY_THREAD_CONTEXT *)AllocateZeroPool (sizeof (NVIDIA_DEVICE_DISCOVERY_THREAD_CONTEXT));
  if (NewContext == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  ThreadStackPages = EFI_SIZE_TO_PAGES (THREAD_STACK_SIZE);

  NewContext->StackBase = (EFI_PHYSICAL_ADDRESS)AllocatePages (ThreadStackPages);
  if (NewContext->StackBase == 0) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Status = gBS->CreateEvent (
                  EVT_TIMER|EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  DeviceDiscoveryThreadCallback,
                  NewContext,
                  &NewContext->Timer
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  NewContext->Controller   = Controller;
  NewContext->DriverHandle = DriverHandle;
  NewContext->Node         = Node;

  // Don't change the special registers
  GetSystemContext ((EFI_SYSTEM_CONTEXT)&MainContext);
  NewContext->Context.ELR  = MainContext.ELR;
  NewContext->Context.SPSR = MainContext.SPSR;
  NewContext->Context.FPSR = MainContext.FPSR;
  NewContext->Context.ESR  = MainContext.ESR;
  NewContext->Context.FAR  = MainContext.FAR;

  NewContext->Context.LR = (UINT64)DeviceThreadMain;
  NewContext->Context.SP = NewContext->StackBase + THREAD_STACK_SIZE;
  NewContext->Context.X0 = (UINT64)NewContext;

  if (AsyncProtocol == NULL) {
    AsyncProtocol = (NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL *)AllocatePool (sizeof (NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL));
    if (AsyncProtocol == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    AsyncProtocol->GetStatus = DeviceDiscoveryAsyncStatus;
    gBS->InstallMultipleProtocolInterfaces (&DriverHandle, &gNVIDIAAsyncDriverStatusProtocol, (VOID *)AsyncProtocol, NULL);
  }

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);
  SwapSystemContext (
    (EFI_SYSTEM_CONTEXT)&MainContext,
    (EFI_SYSTEM_CONTEXT)&NewContext->Context
    );
  gBS->RestoreTPL (OldTpl);

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (NewContext != NULL) {
      if (NewContext->Timer != 0) {
        gBS->CloseEvent (NewContext->Timer);
        NewContext->Timer = 0;
      }

      if (NewContext->StackBase != 0) {
        FreePages ((VOID *)NewContext->StackBase, ThreadStackPages);
        NewContext->StackBase = 0;
      }

      FreePool (NewContext);
      NewContext = NULL;
    }
  }

  return Status;
}

/**
  Start this driver on ControllerHandle.

  @param Controller             Handle of device to bind driver to.

  @retval EFI_SUCCESS           This driver is added to this device.
  @retval EFI_ALREADY_STARTED   This driver is already running on this device.
  @retval other                 Some error occurs when binding this driver to this device.

**/
STATIC
EFI_STATUS
EFIAPI
DeviceDiscoveryStart (
  IN EFI_HANDLE  Controller
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
                  mImageHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, no NonDiscoverableProtocol\r\n", __FUNCTION__));
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
    DEBUG ((DEBUG_ERROR, "%a, no guid mapping\r\n", __FUNCTION__));
    goto ErrorExit;
  }

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryDriverBindingSupported,
             mImageHandle,
             Controller,
             Node
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, Failed supported check\r\n", __FUNCTION__));
    goto ErrorExit;
  }

  if (gDeviceDiscoverDriverConfig.AutoDeassertPg) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAPowerGateNodeProtocolGuid, (VOID **)&PgProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, no Pg node protocol\r\n", __FUNCTION__));
      goto ErrorExit;
    }

    for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
      Status = PgProtocol->Deassert (PgProtocol, PgProtocol->PowerGateId[Index]);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a, failed to deassert Pg %x: %r\r\n", __FUNCTION__, PgProtocol->PowerGateId[Index], Status));
        goto ErrorExit;
      }
    }
  }

  if (gDeviceDiscoverDriverConfig.AutoEnableClocks) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAClockNodeProtocolGuid, (VOID **)&ClockProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, no clock node protocol\r\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = ClockProtocol->EnableAll (ClockProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, failed to enable clocks %r\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  }

  if (gDeviceDiscoverDriverConfig.AutoResetModule) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, no reset node protocol\r\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = ResetProtocol->ModuleResetAll (ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, failed to reset module%r\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  } else if (gDeviceDiscoverDriverConfig.AutoDeassertReset) {
    Status = gBS->HandleProtocol (Controller, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, no reset node protocol\r\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = ResetProtocol->DeassertAll (ResetProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, failed to deassert resets %r\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (NVIDIA_DEVICE_DISCOVERY_CONTEXT),
                  (VOID **)&DeviceDiscoveryContext
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, driver returned %r to allocate context\r\n", __FUNCTION__, Status));
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
                    TPL_CALLBACK,
                    DeviceDiscoveryOnExitBootServices,
                    Controller,
                    &gEfiEventExitBootServicesGuid,
                    &DeviceDiscoveryContext->OnExitBootServicesEvent
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, driver returned %r to create event callback\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  }

  if (gDeviceDiscoverDriverConfig.ThreadedDeviceStart) {
    Status = ThreadedDeviceStart (
               mImageHandle,
               Controller,
               Node
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, threaded device start returned %r\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  } else {
    Status = DeviceDiscoveryNotify (
               DeviceDiscoveryDriverBindingStart,
               mImageHandle,
               Controller,
               Node
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, driver returned %r to start notification\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gNVIDIADeviceDiscoveryContextGuid,
                  DeviceDiscoveryContext,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, driver returned %r to install device discovery context guid\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  if (!gDeviceDiscoverDriverConfig.SkipEdkiiNondiscoverableInstall) {
    ASSERT (!gDeviceDiscoverDriverConfig.ThreadedDeviceStart);
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Controller,
                    &gEdkiiNonDiscoverableDeviceProtocolGuid,
                    NonDiscoverableProtocol,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DeviceDiscoveryNotify (
        DeviceDiscoveryDriverBindingStop,
        mImageHandle,
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
           mImageHandle,
           Controller
           );
  }

  return Status;
}

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
           mImageHandle,
           NULL,
           Node
           );
}

/**
  This is function is caused to allow the system to check if this implementation supports
  the device tree node. If EFI_SUCCESS is returned then handle will be created and driver binding
  will occur.

  @param[in]  Node                   The pointer to the requested node info structure.

  @return EFI_SUCCESS               The node is supported by this instance
  @return EFI_UNSUPPORTED           The node is not supported by this instance
**/
STATIC
EFI_STATUS
EnumerationIsNodeSupported (
  IN OUT  NVIDIA_DT_NODE_INFO  *DeviceInfo
  )
{
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  Node;

  if (DeviceInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Node.DeviceTreeBase = DeviceInfo->DeviceTreeBase;
  Node.NodeOffset     = DeviceInfo->NodeOffset;

  return DeviceTreeIsSupported (NULL, &Node, &DeviceInfo->DeviceType, &DeviceInfo->PciIoInitialize);
}

/**
  Enumerate all matching devices. Called automatically if DelayEnumeration is
  false. Used if device enumeration needs to not happen at driver start.
  For example, if device needs to wait for a protocol notification.

  @retval EFI_SUCCESS             Device Enumeration started
  @retval others                  Error occured
**/
EFI_STATUS
DeviceDiscoveryEnumerateDevices (
  VOID
  )
{
  EFI_STATUS               Status;
  NON_DISCOVERABLE_DEVICE  *Device;
  EFI_HANDLE               DeviceHandle;
  NVIDIA_DT_NODE_INFO      *DtNodeInfo;
  UINT32                   DeviceCount;
  UINT32                   Index;

  DeviceCount = 0;
  DtNodeInfo  = NULL;

  Status = GetSupportedDeviceTreeNodes (
             NULL,
             EnumerationIsNodeSupported,
             &DeviceCount,
             NULL
             );
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get supported nodes - %r\r\n", __FUNCTION__, Status));
    return Status;
  }

  if (!EFI_ERROR (Status)) {
    DtNodeInfo = (NVIDIA_DT_NODE_INFO *)AllocateZeroPool (DeviceCount * sizeof (NVIDIA_DT_NODE_INFO));
    if (DtNodeInfo == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to allocate node structure\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    Status = GetSupportedDeviceTreeNodes (
               NULL,
               EnumerationIsNodeSupported,
               &DeviceCount,
               DtNodeInfo
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get supported nodes - %r\r\n", __FUNCTION__, Status));
      return Status;
    }
  } else {
    DeviceCount = 0;
  }

  for (Index = 0; Index < DeviceCount; Index++) {
    DeviceHandle = NULL;
    Device       = (NON_DISCOVERABLE_DEVICE *)AllocatePool (sizeof (NON_DISCOVERABLE_DEVICE));
    if (Device == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate device protocol.\r\n", __FUNCTION__));
      return EFI_DEVICE_ERROR;
    }

    Status = ProcessDeviceTreeNodeWithHandle (
               &DtNodeInfo[Index],
               Device,
               mImageHandle,
               &DeviceHandle
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to process device node - %r\r\n", __FUNCTION__, Status));
      continue;
    }

    Status = DeviceDiscoveryStart (
               DeviceHandle
               );
    if (EFI_ERROR (Status)) {
      continue;
    }
  }

  if (DtNodeInfo != NULL) {
    FreePool (DtNodeInfo);
    DtNodeInfo = NULL;
  }

  EnumerationCompleted = TRUE;
  if (!gDeviceDiscoverDriverConfig.ThreadedDeviceStart || (SubThreadsRunning == 0)) {
    Status = DeviceDiscoveryNotify (
               DeviceDiscoveryEnumerationCompleted,
               mImageHandle,
               NULL,
               NULL
               );
  }

  return Status;
}

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

  mImageHandle = ImageHandle;

  Status = gBS->LocateProtocol (&gArmScmiClock2ProtocolGuid, NULL, (VOID **)&gScmiClockProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->LocateProtocol (&gNVIDIAClockParentsProtocolGuid, NULL, (VOID **)&gClockParentsProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceDiscoveryNotify (
             DeviceDiscoveryDriverStart,
             mImageHandle,
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!gDeviceDiscoverDriverConfig.DelayEnumeration) {
    Status = DeviceDiscoveryEnumerateDevices ();
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}
