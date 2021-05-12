/** @file

  PINMUX Driver

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
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
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/ResetNodeProtocol.h>
#include <libfdt.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include "PinMuxDxePrivate.h"

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
  { "nvidia,tegra194-pinmux", &gNVIDIANonDiscoverablePinMuxDeviceGuid },
  { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA PinMux driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = TRUE,
    .AutoDeassertReset = TRUE,
    .AutoResetModule = FALSE,
    .AutoDeassertPg = FALSE,
    .SkipEdkiiNondiscoverableInstall = TRUE
};

/**
  This function reads and returns value of a specified PinMux Register

  @param[in]     This                The instance of the NVIDIA_PINMUX_PROTOCOL.
  @param[in]     RegisterOffset      Offset from the PINMUX Base address to read.
  @param[out]    RegisterValue       Value of the PinMux Register.

  @return EFI_SUCCESS                PinMux Register Value successfully returned.
  @return EFI_INVALID_PARAMETER      Register Offset param not in PINMUX Region
**/
STATIC
EFI_STATUS
PinMuxReadRegister (
  IN  NVIDIA_PINMUX_PROTOCOL  *This,
  IN  UINT32    RegisterOffset,
  OUT UINT32    *RegisterValue
  )
{
  PINMUX_DXE_PRIVATE *Private;

  if (NULL == This) {
    return EFI_INVALID_PARAMETER;
  }

  Private = PINMUX_PRIVATE_DATA_FROM_THIS (This);
  if ((RegisterOffset > (Private->RegionSize - sizeof(UINT32))) ||
                                     (RegisterValue == NULL)) {
    return EFI_INVALID_PARAMETER;
  } else {
    *RegisterValue = MmioRead32(Private->BaseAddress + RegisterOffset);
    return EFI_SUCCESS;
  }
}

/**
  This function writes provided value to specified PinMux Register

  @param[in]     This                The instance of NVIDIA_PINMUX_PROTOCOL.
  @param[in]     RegisterOffset      Offset from the PINMUX Base address to Write.
  @param[in]     Value               Value for writing to PinMux Register.

  @return EFI_SUCCESS                PinMux Register Value successfully Written.
  @return EFI_DEVICE_ERROR           Other error occured in writing to PINMUX Register.
**/
STATIC
EFI_STATUS
PinMuxWriteRegister (
  IN  NVIDIA_PINMUX_PROTOCOL  *This,
  IN  UINT32    RegisterOffset,
  IN  UINT32    Value
  )
{
  PINMUX_DXE_PRIVATE *Private;

  if (NULL == This) {
    return EFI_INVALID_PARAMETER;
  }

  Private = PINMUX_PRIVATE_DATA_FROM_THIS (This);
  if (RegisterOffset > (Private->RegionSize - sizeof(UINT32))) {
    return EFI_INVALID_PARAMETER;
  } else {
    MmioWrite32(Private->BaseAddress + RegisterOffset, Value);
    return EFI_SUCCESS;
  }
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES         Phase,
  IN  EFI_HANDLE                             DriverHandle,
  IN  EFI_HANDLE                             ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                Status;
  EFI_PHYSICAL_ADDRESS      BaseAddress;
  UINTN                     RegionSize;
  NVIDIA_PINMUX_PROTOCOL    *PinMuxProtocol;
  PINMUX_DXE_PRIVATE        *Private;

  Status = EFI_SUCCESS;
  BaseAddress = 0;
  Private = NULL;

  switch (Phase) {
  case DeviceDiscoveryDriverBindingStart:

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress,
                                                                &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR,
        "%a: Couldn't find PinMux address range\n", __FUNCTION__));
      return Status;
    }

    Private = AllocatePool (sizeof (PINMUX_DXE_PRIVATE));
    if (NULL == Private) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate Memory\r\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      return Status;
    }

    Private->Signature = PINMUX_SIGNATURE;
    Private->ImageHandle = DriverHandle;
    Private->BaseAddress = BaseAddress;
    Private->RegionSize = RegionSize;
    Private->PinMuxProtocol.ReadReg =  PinMuxReadRegister;
    Private->PinMuxProtocol.WriteReg = PinMuxWriteRegister;
    Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gNVIDIAPinMuxProtocolGuid,
                  &Private->PinMuxProtocol,
                  NULL
                  );
    if (EFI_ERROR (Status)) {
      FreePool (Private);
      return Status;
    }
    break;

  case DeviceDiscoveryDriverBindingStop:

    Status = gBS->HandleProtocol (
                  DriverHandle,
                  &gNVIDIAPinMuxProtocolGuid,
                  (VOID **)&PinMuxProtocol);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Private = PINMUX_PRIVATE_DATA_FROM_PROTOCOL (PinMuxProtocol);

    Status =  gBS->UninstallMultipleProtocolInterfaces (
                   DriverHandle,
                   &gNVIDIAPinMuxProtocolGuid,
                   &Private->PinMuxProtocol,
                   NULL);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    FreePool (Private);
    break;

  default:
    break;
  }

  return Status;

}
