/** @file

  QSPI Driver

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiRuntimeLib.h>

#include <Protocol/QspiController.h>


#define QSPI_CONTROLLER_SIGNATURE SIGNATURE_32('Q','S','P','I')


typedef struct {
  UINT32                          Signature;
  EFI_PHYSICAL_ADDRESS            QspiBaseAddress;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL QspiControllerProtocol;
  EFI_EVENT                       VirtualAddrChangeEvent;
} QSPI_CONTROLLER_PRIVATE_DATA;


#define QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL(a)   CR(a, QSPI_CONTROLLER_PRIVATE_DATA, QspiControllerProtocol, QSPI_CONTROLLER_SIGNATURE)


NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
  { "nvidia,tegra186-qspi", &gNVIDIANonDiscoverableQspiDeviceGuid },
  { "nvidia,tegra194-qspi", &gNVIDIANonDiscoverableQspiDeviceGuid },
  { "nvidia,tegra23x-qspi", &gNVIDIANonDiscoverableQspiDeviceGuid },
  { NULL, NULL }
};


NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
  .DriverName = L"NVIDIA Qspi controller driver",
  .UseDriverBinding = TRUE,
  .AutoEnableClocks = TRUE,
  .AutoDeassertReset = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE
};


/**
  Perform a single transaction on QSPI bus.

  @param[in] This                  Instance of protocol
  @param[in] Packet                Transaction context

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
EFIAPI
QspiControllerPerformTransaction(
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL *This,
  IN QSPI_TRANSACTION_PACKET *Packet
)
{
  QSPI_CONTROLLER_PRIVATE_DATA *Private;

  Private = QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL (This);

  return QspiPerformTransaction (Private->QspiBaseAddress, Packet);
}

/**
  Fixup internal data so that EFI can be call in virtual mode.
  Call the passed in Child Notify event and convert any pointers in
  lib to virtual mode.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
VirtualNotifyEvent (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  QSPI_CONTROLLER_PRIVATE_DATA *Private;

  Private = (QSPI_CONTROLLER_PRIVATE_DATA *)Context;
  EfiConvertPointer (0x0, (VOID**)&Private->QspiBaseAddress);
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES         Phase,
  IN  EFI_HANDLE                             DriverHandle,
  IN  EFI_HANDLE                             ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode OPTIONAL
)
{
  EFI_STATUS                      Status;
  QSPI_CONTROLLER_PRIVATE_DATA    *Private;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL *QspiControllerProtocol;
  EFI_PHYSICAL_ADDRESS            BaseAddress;
  UINTN                           RegionSize;
  EFI_DEVICE_PATH_PROTOCOL        *DevicePath;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR Descriptor;

  Private = NULL;

  switch (Phase) {
  case DeviceDiscoveryDriverBindingStart:
    Status = gBS->HandleProtocol (ControllerHandle,
                                  &gEfiDevicePathProtocolGuid,
                                  (VOID **)&DevicePath);
    if (EFI_ERROR (Status) ||
        (DevicePath == NULL) ||
        IsDevicePathEnd(DevicePath)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate device path\n", __FUNCTION__));
      return Status;
    }
    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
      return Status;
    }

    //Convert to runtime memory
    Status = gDS->GetMemorySpaceDescriptor (BaseAddress, &Descriptor);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to be memory descriptor\r\n", __FUNCTION__));
      return Status;
    }

    Status = gDS->SetMemorySpaceAttributes (BaseAddress, RegionSize, Descriptor.Attributes | EFI_MEMORY_RUNTIME);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set memory as runtime\r\n", __FUNCTION__));
      return Status;
    }

    Private = AllocateRuntimeZeroPool (sizeof (QSPI_CONTROLLER_PRIVATE_DATA));
    if (Private == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    Private->Signature = QSPI_CONTROLLER_SIGNATURE;
    Private->QspiBaseAddress = BaseAddress;
    Status = QspiInitialize (Private->QspiBaseAddress);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "QSPI Initialization Failed.\n"));
      goto ErrorExit;
    }
    Private->QspiControllerProtocol.PerformTransaction = QspiControllerPerformTransaction;

    Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                                 TPL_NOTIFY,
                                 VirtualNotifyEvent,
                                 Private,
                                 &gEfiEventVirtualAddressChangeGuid,
                                 &Private->VirtualAddrChangeEvent);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Failed to create virtual address event\r\n"));
      goto ErrorExit;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (&ControllerHandle,
                                                     &gNVIDIAQspiControllerProtocolGuid,
                                                     &Private->QspiControllerProtocol,
                                                     NULL);
    if (!EFI_ERROR (Status)) {
      return Status;
    }
    break;
  case DeviceDiscoveryDriverBindingStop:
    Status = gBS->HandleProtocol (ControllerHandle,
                                  &gNVIDIAQspiControllerProtocolGuid,
                                  (VOID **)&QspiControllerProtocol);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    Private = QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL (QspiControllerProtocol);
    Status =  gBS->UninstallMultipleProtocolInterfaces (ControllerHandle,
                                                        &gNVIDIAQspiControllerProtocolGuid,
                                                        &Private->QspiControllerProtocol,
                                                        NULL);
    if (!EFI_ERROR (Status)) {
      return Status;
    }

    gBS->CloseEvent (Private->VirtualAddrChangeEvent);
    break;
  default:
    return EFI_SUCCESS;
  }

ErrorExit:
  if (Private != NULL) {
    FreePool (Private);
  }

  return Status;
}
