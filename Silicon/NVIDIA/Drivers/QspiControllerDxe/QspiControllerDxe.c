/** @file

  QSPI Driver

  Copyright (c) 2019-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Protocol/ClockNodeProtocol.h>
#include <Protocol/ArmScmiClock2Protocol.h>
#include <Protocol/QspiController.h>
#include <libfdt.h>


#define QSPI_CONTROLLER_SIGNATURE SIGNATURE_32('Q','S','P','I')


typedef struct {
  UINT32                          Signature;
  EFI_PHYSICAL_ADDRESS            QspiBaseAddress;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL QspiControllerProtocol;
  EFI_EVENT                       VirtualAddrChangeEvent;
  BOOLEAN                         WaitCyclesSupported;
} QSPI_CONTROLLER_PRIVATE_DATA;


#define QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL(a)   CR(a, QSPI_CONTROLLER_PRIVATE_DATA, QspiControllerProtocol, QSPI_CONTROLLER_SIGNATURE)


NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
  { "nvidia,tegra186-spi", &gNVIDIANonDiscoverableSpiDeviceGuid },
  { "nvidia,tegra194-spi", &gNVIDIANonDiscoverableSpiDeviceGuid },
  { "nvidia,tegra234-spi", &gNVIDIANonDiscoverableSpiDeviceGuid },
  { "nvidia,tegra23x-spi", &gNVIDIANonDiscoverableSpiDeviceGuid },
  { "nvidia,tegra186-qspi", &gNVIDIANonDiscoverableQspiDeviceGuid },
  { "nvidia,tegra194-qspi", &gNVIDIANonDiscoverableQspiDeviceGuid },
  { "nvidia,tegra234-qspi", &gNVIDIANonDiscoverableQspiDeviceGuid },
  { "nvidia,tegra23x-qspi", &gNVIDIANonDiscoverableQspiDeviceGuid },
  { NULL, NULL }
};


NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
  .DriverName = L"NVIDIA Qspi controller driver",
  .UseDriverBinding = TRUE,
  .AutoEnableClocks = TRUE,
  .AutoDeassertReset = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .SkipAutoDeinitControllerOnExitBootServices = TRUE
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

  if (!Private->WaitCyclesSupported && Packet->WaitCycles != 0) {
    return EFI_UNSUPPORTED;
  }

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
  Setup clock frequency for the spi controller.

  @param[in]    DeviceTreeNode  Interface to controller's DTB entry
  @param[in]    Controller      Controller handle
  @param[in]    ClockFreq       Frequency to be setup

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
EFIAPI
SetSpiFrequency (
  IN CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode,
  IN EFI_HANDLE Controller,
  IN UINT32 ClockFreq
)
{
  EFI_STATUS                 Status;
  CONST UINT32               *DtClockIds;
  INT32                      ClocksLength;
  CONST CHAR8                *ClockName;
  NVIDIA_CLOCK_NODE_PROTOCOL *ClockNodeProtocol;
  UINTN                      Index;
  UINT32                     ClockId;
  SCMI_CLOCK2_PROTOCOL       *ScmiClockProtocol;

  Status = EFI_SUCCESS;

  DtClockIds = (CONST UINT32*)fdt_getprop (DeviceTreeNode->DeviceTreeBase,
                                           DeviceTreeNode->NodeOffset,
                                           "clocks", &ClocksLength);
  if ((DtClockIds != NULL) && (ClocksLength != 0)) {
    ClockName = "spi";
    ClockNodeProtocol = NULL;
    Status = gBS->HandleProtocol (Controller,
                                  &gNVIDIAClockNodeProtocolGuid,
                                  (VOID **)&ClockNodeProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to locate Clock Protocol\n", __FUNCTION__));
      return Status;
    }
    for (Index = 0; Index < ClockNodeProtocol->Clocks; Index++) {
      if (0 == AsciiStrCmp (ClockName, ClockNodeProtocol->ClockEntries[Index].ClockName)) {
        ClockId = ClockNodeProtocol->ClockEntries[Index].ClockId;
        Status = gBS->LocateProtocol (&gArmScmiClock2ProtocolGuid,
                                      NULL,
                                      (VOID **)&ScmiClockProtocol);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to locate ARM SCMI Clock2 Protocol\n", __FUNCTION__));
          return Status;
        }
        Status = ScmiClockProtocol->RateSet (ScmiClockProtocol, ClockId, ClockFreq);
      }
    }
  }

  return Status;
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
  NON_DISCOVERABLE_DEVICE         *Device;
  BOOLEAN                         WaitCyclesSupported;
  UINT32                          SpiClockFreq;
  QSPI_CONTROLLER_PRIVATE_DATA    *Private;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL *QspiControllerProtocol;
  EFI_PHYSICAL_ADDRESS            BaseAddress;
  UINTN                           RegionSize;
  EFI_DEVICE_PATH_PROTOCOL        *DevicePath;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR Descriptor;

  Device = NULL;
  Private = NULL;

  switch (Phase) {
  case DeviceDiscoveryDriverBindingStart:
    Status = gBS->HandleProtocol (ControllerHandle,
                                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                                  (VOID **)&Device);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to locate non discoverable device\n", __FUNCTION__));
      return Status;
    }

    if (CompareMem (Device->Type, &gNVIDIANonDiscoverableSpiDeviceGuid, sizeof (EFI_GUID)) == 0) {
      WaitCyclesSupported = FALSE;
      // SPI controller is usually going to be used for non flash
      // peripherals. Because of this reason, it would not be set
      // to its default clock rate by previous stage bootloaders.
      // Set the clock rate here based on the PCD value.
      SpiClockFreq = PcdGet32 (PcdSpiClockFrequency);
      if (SpiClockFreq > 0) {
        Status = SetSpiFrequency (DeviceTreeNode, ControllerHandle, SpiClockFreq);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to Set Clock Frequency %r\n", __FUNCTION__, Status));
          return Status;
        }
      }
    } else {
      WaitCyclesSupported = TRUE;
    }

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
    Private->WaitCyclesSupported = WaitCyclesSupported;
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
