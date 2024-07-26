/** @file

  PCIe Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <IndustryStandard/Pci.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PciIo.h>
#include <Protocol/PciPlatform.h>

#pragma pack(1)
typedef struct {
  ACPI_HID_DEVICE_PATH        AcpiDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    EndDevicePath;
} EFI_PCI_ROOT_BRIDGE_DEVICE_PATH;
#pragma pack ()

BOOLEAN  gRootBridgeCapCfgComplete = FALSE;

EFI_STATUS
RootPortConfigPcieCapability (
  EFI_HANDLE  RootBridgeHandle
  );

/**
  Callback funciton for EndEnumeration notification from PCI stack.

  @param[in]  RootBridgeIndex       Index to identify of PCIE Root bridge.
  @param[in]  Phase                 The phase of enumeration as informed from PCI stack.
**/
VOID
NotifyPhaseCallBack (
  IN UINTN                                          RootBridgeIndex,
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE  Phase
  )
{
  PCI_ROOT_BRIDGE  *RootBridges = NULL;
  EFI_STATUS       Status       = EFI_SUCCESS;
  UINTN            NumberOfHandles;
  EFI_HANDLE       *Handles = NULL;
  UINTN            CurrentHandle;

  switch (Phase) {
    case EfiPciHostBridgeEndEnumeration:
      break;
    case EfiPciHostBridgeBeginEnumeration:
      break;
    case EfiPciHostBridgeBeginBusAllocation:
      if (!gRootBridgeCapCfgComplete) {
        Status = gBS->LocateHandleBuffer (ByProtocol, &gNVIDIAPciHostBridgeProtocolGuid, NULL, &NumberOfHandles, &Handles);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to locate host bridge protocols, %r.\r\n", __FUNCTION__, Status));
          goto handle_case_exit;
        }

        for (CurrentHandle = 0; CurrentHandle < NumberOfHandles; CurrentHandle++) {
          RootPortConfigPcieCapability (Handles[CurrentHandle]);
        }

        gRootBridgeCapCfgComplete = TRUE;
      }

handle_case_exit:
      if (Handles != NULL) {
        FreePool (Handles);
        Handles = NULL;
      }

      if (EFI_ERROR (Status)) {
        NumberOfHandles = 0;
        if (RootBridges != NULL) {
          FreePool (RootBridges);
          RootBridges = NULL;
        }
      }

      break;
    case EfiPciHostBridgeEndBusAllocation:
    case EfiPciHostBridgeBeginResourceAllocation:
    case EfiPciHostBridgeAllocateResources:
    case EfiPciHostBridgeSetResources:
    case EfiPciHostBridgeFreeResources:
    case EfiPciHostBridgeEndResourceAllocation:
    case EfiMaxPciHostBridgeEnumerationPhase:
      break;
  }
}

/**

  Perform initialization by the phase indicated.

  @param This                        Pointer to the EFI_PCI_PLATFORM_PROTOCOL instance.
  @param HostBridge                  The associated PCI host bridge handle.
  @param Phase                       The phase of the PCI controller enumeration.
  @param ChipsetPhase                Defines the execution phase of the PCI chipset driver.

  @retval EFI_SUCCESS                Must return with success.

**/
EFI_STATUS
EFIAPI
PhaseNotify (
  IN  EFI_PCI_PLATFORM_PROTOCOL                      *This,
  IN  EFI_HANDLE                                     HostBridge,
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE  Phase,
  IN  EFI_PCI_CHIPSET_EXECUTION_PHASE                ChipsetPhase
  )
{
  EFI_PCI_ROOT_BRIDGE_DEVICE_PATH                   *RootBridgeDevPath;
  EFI_HANDLE                                        RootBridgeHandle = NULL;
  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *ResAlloc        = NULL;
  EFI_STATUS                                        Status;

  if (ChipsetPhase != ChipsetExit) {
    return EFI_SUCCESS;
  }

  //
  // Get HostBridgeInstance from HostBridge handle.
  //
  Status = gBS->HandleProtocol (
                  HostBridge,
                  &gEfiPciHostBridgeResourceAllocationProtocolGuid,
                  (VOID **)&ResAlloc
                  );

  while (TRUE) {
    Status = ResAlloc->GetNextRootBridge (ResAlloc, &RootBridgeHandle);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = gBS->HandleProtocol (
                    RootBridgeHandle,
                    &gEfiDevicePathProtocolGuid,
                    (VOID **)&RootBridgeDevPath
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a %d: Failed to locate RootBridge DevicePath\n", __FUNCTION__, __LINE__));
      break;
    }

    NotifyPhaseCallBack (RootBridgeDevPath->AcpiDevicePath.UID, Phase);
  }

  return EFI_SUCCESS;
}

/**

  The PlatformPrepController() function can be used to notify the platform driver so that
  it can perform platform-specific actions. No specific actions are required.
  Several notification points are defined at this time. More synchronization points may be
  added as required in the future. The PCI bus driver calls the platform driver twice for
  every PCI controller-once before the PCI Host Bridge Resource Allocation Protocol driver
  is notified, and once after the PCI Host Bridge Resource Allocation Protocol driver has
  been notified.
  This member function may not perform any error checking on the input parameters. It also
  does not return any error codes. If this member function detects any error condition, it
  needs to handle those errors on its own because there is no way to surface any errors to
  the caller.

  @param This                       Pointer to the EFI_PCI_PLATFORM_PROTOCOL instance.
  @param HostBridge                 The associated PCI host bridge handle.
  @param RootBridge                 The associated PCI root bridge handle.
  @param PciAddress                 The address of the PCI device on the PCI bus.
  @param Phase                      The phase of the PCI controller enumeration.
  @param ChipsetPhase               Defines the execution phase of the PCI chipset driver.

  @retval EFI_SUCCESS               The function completed successfully.
  @retval EFI_UNSUPPORTED           Not supported.

**/
EFI_STATUS
EFIAPI
PlatformPrepController (
  IN  EFI_PCI_PLATFORM_PROTOCOL                     *This,
  IN  EFI_HANDLE                                    HostBridge,
  IN  EFI_HANDLE                                    RootBridge,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS   PciAddress,
  IN  EFI_PCI_CONTROLLER_RESOURCE_ALLOCATION_PHASE  Phase,
  IN  EFI_PCI_CHIPSET_EXECUTION_PHASE               ChipsetPhase
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Set the PciPolicy as EFI_RESERVE_ISA_IO_NO_ALIAS | EFI_RESERVE_VGA_IO_NO_ALIAS.

  @param This                        The pointer to the Protocol itself.
  @param PciPolicy                   The returned Policy.

  @retval EFI_UNSUPPORTED            Function not supported.
  @retval EFI_INVALID_PARAMETER      Invalid PciPolicy value.

**/
EFI_STATUS
EFIAPI
GetPlatformPolicy (
  IN  CONST EFI_PCI_PLATFORM_PROTOCOL  *This,
  OUT EFI_PCI_PLATFORM_POLICY          *PciPolicy
  )
{
  return EFI_UNSUPPORTED;
}

/**

  Return a PCI ROM image for the onboard device represented by PciHandle.

  @param This                        Protocol instance pointer.
  @param PciHandle                   PCI device to return the ROM image for.
  @param RomImage                    PCI Rom Image for onboard device.
  @param RomSize                     Size of RomImage in bytes.

  @retval EFI_SUCCESS                RomImage is valid.
  @retval EFI_NOT_FOUND              No RomImage.

**/
EFI_STATUS
EFIAPI
GetPciRom (
  IN  CONST EFI_PCI_PLATFORM_PROTOCOL  *This,
  IN  EFI_HANDLE                       PciHandle,
  OUT VOID                             **RomImage,
  OUT UINTN                            *RomSize
  )
{
  return EFI_NOT_FOUND;
}

//
// Interface defintion of PCI Platform protocol.
//
EFI_PCI_PLATFORM_PROTOCOL  mPciPlatformProtocol = {
  .PlatformNotify         = PhaseNotify,
  .PlatformPrepController = PlatformPrepController,
  .GetPlatformPolicy      = GetPlatformPolicy,
  .GetPciRom              = GetPciRom
};
