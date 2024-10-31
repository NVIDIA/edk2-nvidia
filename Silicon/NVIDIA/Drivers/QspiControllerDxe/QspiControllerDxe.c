/** @file

  QSPI Driver

  SPDX-FileCopyrightText: Copyright (c) 2019-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/TegraPlatformInfoLib.h>

#include <Protocol/ClockNodeProtocol.h>
#include <Protocol/ArmScmiClock2Protocol.h>
#include <Protocol/QspiController.h>
#include <libfdt.h>

#define QSPI_CONTROLLER_SIGNATURE  SIGNATURE_32('Q','S','P','I')

#define QSPI_NUM_CHIP_SELECTS_DEFAULT  1
#define QSPI_NUM_CHIP_SELECTS_T234     1
#define QSPI_NUM_CHIP_SELECTS_T264     1
#define QSPI_NUM_CHIP_SELECTS_TH500    4

typedef enum {
  CONTROLLER_TYPE_QSPI,
  CONTROLLER_TYPE_SPI,
  CONTROLLER_TYPE_UNSUPPORTED
} QSPI_CONTROLLER_TYPE;

typedef struct {
  UINT32                             Signature;
  EFI_PHYSICAL_ADDRESS               QspiBaseAddress;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL    QspiControllerProtocol;
  EFI_EVENT                          VirtualAddrChangeEvent;
  BOOLEAN                            WaitCyclesSupported;
  QSPI_CONTROLLER_TYPE               ControllerType;
  UINT32                             ClockId;
  UINT8                              NumChipSelects;
} QSPI_CONTROLLER_PRIVATE_DATA;

#define QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL(a)  CR(a, QSPI_CONTROLLER_PRIVATE_DATA, QspiControllerProtocol, QSPI_CONTROLLER_SIGNATURE)

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,*-spi",  &gNVIDIANonDiscoverableSpiDeviceGuid  },
  { "nvidia,*-qspi", &gNVIDIANonDiscoverableQspiDeviceGuid },
  { NULL,            NULL                                  }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                                 = L"NVIDIA Qspi controller driver",
  .AutoEnableClocks                           = TRUE,
  .AutoDeassertReset                          = TRUE,
  .SkipEdkiiNondiscoverableInstall            = TRUE,
  .SkipAutoDeinitControllerOnExitBootServices = TRUE,
  .DisableInRcm                               = TRUE
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
QspiControllerPerformTransaction (
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL  *This,
  IN QSPI_TRANSACTION_PACKET          *Packet
  )
{
  QSPI_CONTROLLER_PRIVATE_DATA  *Private;
  EFI_STATUS                    Status;

  Private = QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL (This);

  if (!Private->WaitCyclesSupported && (Packet->WaitCycles != 0)) {
    return EFI_UNSUPPORTED;
  }

  if (IsQspiControllerReset (Private->QspiBaseAddress) == TRUE) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:%d Controller is in Reset State, re-initializing",
      __FUNCTION__,
      __LINE__
      ));
    Status = QspiInitialize (Private->QspiBaseAddress, Private->NumChipSelects);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "QSPI Initialization Failed for 0x%x %r.\n",
        Private->QspiBaseAddress,
        Status
        ));
      return Status;
    }
  }

  return QspiPerformTransaction (Private->QspiBaseAddress, Packet);
}

/**
  Get QSPI clock speed.

  @param[in] This                  Instance of protocol
  @param[in] ClockSpeed            Pointer to get clock speed

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
EFIAPI
QspiControllerGetClockSpeed (
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL  *This,
  IN UINT64                           *ClockSpeed
  )
{
  EFI_STATUS                    Status;
  SCMI_CLOCK2_PROTOCOL          *ScmiClockProtocol;
  QSPI_CONTROLLER_PRIVATE_DATA  *Private;

  Private = QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL (This);

  Status = gBS->LocateProtocol (
                  &gArmScmiClock2ProtocolGuid,
                  NULL,
                  (VOID **)&ScmiClockProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate ARM SCMI Clock2 Protocol\n", __FUNCTION__));
    return Status;
  }

  if (Private->ClockId != MAX_UINT32) {
    return ScmiClockProtocol->RateGet (ScmiClockProtocol, Private->ClockId, ClockSpeed);
  } else {
    return EFI_UNSUPPORTED;
  }
}

/**
  Set QSPI clock speed.

  @param[in] This                  Instance of protocol
  @param[in] ClockSpeed            Clock speed

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
EFIAPI
QspiControllerSetClockSpeed (
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL  *This,
  IN UINT64                           ClockSpeed
  )
{
  EFI_STATUS                    Status;
  SCMI_CLOCK2_PROTOCOL          *ScmiClockProtocol;
  QSPI_CONTROLLER_PRIVATE_DATA  *Private;

  Private = QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL (This);

  Status = gBS->LocateProtocol (
                  &gArmScmiClock2ProtocolGuid,
                  NULL,
                  (VOID **)&ScmiClockProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate ARM SCMI Clock2 Protocol\n", __FUNCTION__));
    return Status;
  }

  if (Private->ClockId != MAX_UINT32) {
    return ScmiClockProtocol->RateSet (ScmiClockProtocol, Private->ClockId, ClockSpeed);
  } else {
    return EFI_UNSUPPORTED;
  }
}

/**
  Get QSPI number of chip selects

  @param[in]  This                 Instance of protocol
  @param[out] NumChipSelects       Pointer to store number of chip selects

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
EFIAPI
QspiControllerGetNumChipSelects (
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL  *This,
  OUT UINT8                           *NumChipSelects
  )
{
  QSPI_CONTROLLER_PRIVATE_DATA  *Private;

  Private         = QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL (This);
  *NumChipSelects = Private->NumChipSelects;

  return EFI_SUCCESS;
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
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  QSPI_CONTROLLER_PRIVATE_DATA  *Private;

  Private = (QSPI_CONTROLLER_PRIVATE_DATA *)Context;
  EfiConvertPointer (0x0, (VOID **)&Private->QspiBaseAddress);
  return;
}

/**
  Setup clock frequency for the spi controller.

  @param[in]    ClockId         Id of clock
  @param[in]    ClockFreq       Frequency to be setup

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
EFIAPI
SetSpiFrequency (
  IN UINT32  ClockId,
  IN UINT32  ClockSpeed
  )
{
  EFI_STATUS            Status;
  SCMI_CLOCK2_PROTOCOL  *ScmiClockProtocol;

  Status = gBS->LocateProtocol (
                  &gArmScmiClock2ProtocolGuid,
                  NULL,
                  (VOID **)&ScmiClockProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate ARM SCMI Clock2 Protocol\n", __FUNCTION__));
    return Status;
  }

  if (ClockId != MAX_UINT32) {
    return ScmiClockProtocol->RateSet (ScmiClockProtocol, ClockId, ClockSpeed);
  } else {
    return EFI_UNSUPPORTED;
  }
}

/**
  Apply QSPI controller settings for a specific device

  @param[in] This                  Instance of protocol
  @param[in] DeviceFeature         Device feature to initialize

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
EFIAPI
QspiControllerApplyDeviceSpecificSettings (
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL  *This,
  IN QSPI_DEV_FEATURE                 DeviceFeature
  )
{
  EFI_STATUS                    Status;
  QSPI_CONTROLLER_PRIVATE_DATA  *Private;

  Private = QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL (This);

  //
  // Enable/Disable wait state
  //
  if ((Private->ControllerType == CONTROLLER_TYPE_QSPI)) {
    if (DeviceFeature == QspiDevFeatWaitStateEn) {
      Status = QspiEnableWaitState (Private->QspiBaseAddress, TRUE);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Fail to enable wait state\n", __FUNCTION__));
        return Status;
      }
    }

    if (DeviceFeature == QspiDevFeatWaitStateDis) {
      Status = QspiEnableWaitState (Private->QspiBaseAddress, FALSE);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Fail to disable wait state\n", __FUNCTION__));
        return Status;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Detect Controller Type

  @param[in]    Device          Non discoverable device
  @param[in]    ControllerType  Pointer to controller type

  @retval ControllerType        Type of controller

**/
QSPI_CONTROLLER_TYPE
EFIAPI
DetectControllerType (
  IN NON_DISCOVERABLE_DEVICE  *Device
  )
{
  if (CompareMem (Device->Type, &gNVIDIANonDiscoverableSpiDeviceGuid, sizeof (EFI_GUID)) == 0) {
    return CONTROLLER_TYPE_SPI;
  }

  return CONTROLLER_TYPE_QSPI;
}

/**
  Detect Number of Chip Selects

  @retval UINT8                    Number of chip selects

**/
STATIC
UINT8
EFIAPI
DetectNumChipSelects (
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode
  )
{
  UINTN         ChipID;
  CONST UINT32  *NumCs;
  INT32         Length;
  UINT8         NumChipSelects;

  NumCs = (CONST UINT32 *)fdt_getprop (
                            DeviceTreeNode->DeviceTreeBase,
                            DeviceTreeNode->NodeOffset,
                            "num-cs",
                            &Length
                            );

  if ((NumCs != NULL) && (Length == sizeof (UINT32))) {
    NumChipSelects = (UINT8)fdt32_to_cpu (*(CONST UINT32 *)NumCs);
    DEBUG ((DEBUG_INFO, "%a: num-cs=%u\n", __FUNCTION__, NumChipSelects));
    return NumChipSelects;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T234_CHIP_ID:
      NumChipSelects = QSPI_NUM_CHIP_SELECTS_T234;
      break;
    case T264_CHIP_ID:
      NumChipSelects = QSPI_NUM_CHIP_SELECTS_T264;
      break;
    case TH500_CHIP_ID:
      NumChipSelects = QSPI_NUM_CHIP_SELECTS_TH500;
      break;
    default:
      NumChipSelects = QSPI_NUM_CHIP_SELECTS_DEFAULT;
      break;
  }

  DEBUG ((DEBUG_INFO, "%a: NumChipSelects = %u\n", __FUNCTION__, NumChipSelects));

  return NumChipSelects;
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
  NON_DISCOVERABLE_DEVICE          *Device;
  QSPI_CONTROLLER_TYPE             ControllerType;
  CONST UINT32                     *SecureController;
  CONST UINT32                     *DtClockIds;
  INT32                            ClocksLength;
  CONST CHAR8                      *ClockName;
  NVIDIA_CLOCK_NODE_PROTOCOL       *ClockNodeProtocol;
  UINTN                            Index;
  UINT32                           ClockId;
  BOOLEAN                          WaitCyclesSupported;
  UINT32                           SpiClockFreq;
  QSPI_CONTROLLER_PRIVATE_DATA     *Private;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiControllerProtocol;
  EFI_PHYSICAL_ADDRESS             BaseAddress;
  UINTN                            RegionSize;
  EFI_DEVICE_PATH_PROTOCOL         *DevicePath;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Descriptor;
  VOID                             *Interface;
  UINT8                            NumChipSelects;
  BOOLEAN                          MMPresent;

  Device  = NULL;
  Private = NULL;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      SecureController = (CONST UINT32 *)fdt_getprop (
                                           DeviceTreeNode->DeviceTreeBase,
                                           DeviceTreeNode->NodeOffset,
                                           "nvidia,secure-qspi-controller",
                                           NULL
                                           );

      Status = gBS->LocateProtocol (
                      &gEfiMmCommunication2ProtocolGuid,
                      NULL,
                      &Interface
                      );
      MMPresent = !EFI_ERROR (Status);

      if (SecureController == NULL) {
        // Non-secure controller
        if (!MMPresent || PcdGetBool (PcdNonSecureQspiAvailable)) {
          return EFI_SUCCESS;
        } else {
          return EFI_UNSUPPORTED;
        }
      } else {
        // Secure controller
        // If MM is not available and we are not using emulated variable expose the SPI controllers to NS UEFI
        if (!MMPresent &&
            !PcdGetBool (PcdEmuVariableNvModeEnable))
        {
          return EFI_SUCCESS;
        } else {
          return EFI_UNSUPPORTED;
        }
      }

      return EFI_UNSUPPORTED;

    case DeviceDiscoveryDriverBindingStart:
      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gNVIDIANonDiscoverableDeviceProtocolGuid,
                      (VOID **)&Device
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate non discoverable device\n", __FUNCTION__));
        return Status;
      }

      ControllerType = DetectControllerType (Device);
      ClockId        = MAX_UINT32;

      DtClockIds = (CONST UINT32 *)fdt_getprop (
                                     DeviceTreeNode->DeviceTreeBase,
                                     DeviceTreeNode->NodeOffset,
                                     "clocks",
                                     &ClocksLength
                                     );
      if ((DtClockIds != NULL) && (ClocksLength != 0)) {
        if (ControllerType == CONTROLLER_TYPE_SPI) {
          ClockName = "spi";
        } else {
          ClockName = "qspi";
        }

        ClockNodeProtocol = NULL;
        Status            = gBS->HandleProtocol (
                                   ControllerHandle,
                                   &gNVIDIAClockNodeProtocolGuid,
                                   (VOID **)&ClockNodeProtocol
                                   );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to locate Clock Protocol\n", __FUNCTION__));
          return Status;
        }

        for (Index = 0; Index < ClockNodeProtocol->Clocks; Index++) {
          if (0 == AsciiStrCmp (ClockName, ClockNodeProtocol->ClockEntries[Index].ClockName)) {
            ClockId = ClockNodeProtocol->ClockEntries[Index].ClockId;
            break;
          }
        }
      }

      if (ControllerType == CONTROLLER_TYPE_SPI) {
        WaitCyclesSupported = FALSE;
        // SPI controller is usually going to be used for non flash
        // peripherals. Because of this reason, it would not be set
        // to its default clock rate by previous stage bootloaders.
        // Set the clock rate here based on the PCD value.
        SpiClockFreq = PcdGet32 (PcdSpiClockFrequency);
        if ((SpiClockFreq > 0) && (ClockId != MAX_UINT32)) {
          Status = SetSpiFrequency (ClockId, SpiClockFreq);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a: Failed to Set Clock Frequency %r\n", __FUNCTION__, Status));
            return Status;
          }
        }
      } else {
        WaitCyclesSupported = TRUE;
      }

      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gEfiDevicePathProtocolGuid,
                      (VOID **)&DevicePath
                      );
      if (EFI_ERROR (Status) ||
          (DevicePath == NULL) ||
          IsDevicePathEnd (DevicePath))
      {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate device path\n", __FUNCTION__));
        return Status;
      }

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
        return Status;
      }

      // Convert to runtime memory
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

      NumChipSelects = DetectNumChipSelects (DeviceTreeNode);

      Private = AllocateRuntimeZeroPool (sizeof (QSPI_CONTROLLER_PRIVATE_DATA));
      if (Private == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      Private->Signature           = QSPI_CONTROLLER_SIGNATURE;
      Private->QspiBaseAddress     = BaseAddress;
      Private->WaitCyclesSupported = WaitCyclesSupported;
      Private->ControllerType      = ControllerType;
      Private->ClockId             = ClockId;
      Private->NumChipSelects      = NumChipSelects;

      Status = QspiInitialize (Private->QspiBaseAddress, NumChipSelects);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "QSPI Initialization Failed.\n"));
        goto ErrorExit;
      }

      Private->QspiControllerProtocol.PerformTransaction          = QspiControllerPerformTransaction;
      Private->QspiControllerProtocol.GetNumChipSelects           = QspiControllerGetNumChipSelects;
      Private->QspiControllerProtocol.ApplyDeviceSpecificSettings = QspiControllerApplyDeviceSpecificSettings;
      if (Private->ClockId != MAX_UINT32) {
        Private->QspiControllerProtocol.GetClockSpeed = QspiControllerGetClockSpeed;
        Private->QspiControllerProtocol.SetClockSpeed = QspiControllerSetClockSpeed;
      }

      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_NOTIFY,
                      VirtualNotifyEvent,
                      Private,
                      &gEfiEventVirtualAddressChangeGuid,
                      &Private->VirtualAddrChangeEvent
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to create virtual address event\r\n"));
        goto ErrorExit;
      }

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gNVIDIAQspiControllerProtocolGuid,
                      &Private->QspiControllerProtocol,
                      NULL
                      );
      if (!EFI_ERROR (Status)) {
        return Status;
      }

      break;
    case DeviceDiscoveryDriverBindingStop:
      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gNVIDIAQspiControllerProtocolGuid,
                      (VOID **)&QspiControllerProtocol
                      );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Private = QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL (QspiControllerProtocol);
      Status  =  gBS->UninstallMultipleProtocolInterfaces (
                        ControllerHandle,
                        &gNVIDIAQspiControllerProtocolGuid,
                        &Private->QspiControllerProtocol,
                        NULL
                        );
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
