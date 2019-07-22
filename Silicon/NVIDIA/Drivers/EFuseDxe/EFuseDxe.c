/** @file

  EFUSE Driver

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
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
#include "EFuseDxePrivate.h"

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra186-efuse", &gNVIDIANonDiscoverableT186EFuseDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA EFuse driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = TRUE,
    .AutoSetParents = TRUE,
    .AutoDeassertReset = TRUE,
    .AutoResetModule = FALSE,
    .AutoDeassertPg = FALSE,
    .SkipEdkiiNondiscoverableInstall = TRUE
};

/**
  This function reads and returns value of a specified Fuse Register

  @param[in]     This                The instance of the NVIDIA_EFUSE_PROTOCOL.
  @param[in]     RegisterOffset      Offset from the EFUSE Base address to read.
  @param[out]    RegisterValue       Value of the Fuse Register.

  @return EFI_SUCCESS                Fuse Register Value successfully returned.
  @return EFI_INVALID_PARAMETER      Register Offset param not in EFUSE Region
**/
STATIC
EFI_STATUS
EfuseReadRegister (
  IN  NVIDIA_EFUSE_PROTOCOL  *This,
  IN  UINT32    RegisterOffset,
  OUT UINT32    *RegisterValue
  )
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFUSE_DXE_PRIVATE *Private;

  Private = EFUSE_PRIVATE_DATA_FROM_THIS (This);
  if ((RegisterOffset > (Private->RegionSize - sizeof(UINT32))) ||
                                     (RegisterValue == NULL)) {
    Status = EFI_INVALID_PARAMETER;
  } else {
    *RegisterValue = MmioRead32(Private->BaseAddress + RegisterOffset);
    Status = EFI_SUCCESS;
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
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_PHYSICAL_ADDRESS      BaseAddress  = 0;
  UINTN                     RegionSize;
  EFUSE_DXE_PRIVATE *Private = NULL;

  switch (Phase) {
  case DeviceDiscoveryDriverBindingStart:

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress,
                                                                &RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR,
        "%a: Couldn't find Efuse address range\n", __FUNCTION__));
      goto ErrorExit;
    }

    Private = AllocatePool (sizeof (EFUSE_DXE_PRIVATE));
    if (NULL == Private) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate Memory\r\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Private->Signature = EFUSE_SIGNATURE;
    Private->ImageHandle = DriverHandle;
    Private->BaseAddress = BaseAddress;
    Private->RegionSize = RegionSize;
    Private->EFuseProtocol.ReadReg = EfuseReadRegister;

    Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gNVIDIAEFuseProtocolGuid,
                  &Private->EFuseProtocol,
                  NULL
                  );
    if (EFI_ERROR (Status)) {
      FreePool (Private);
      Status = EFI_PROTOCOL_ERROR;
      goto ErrorExit;
    }
    break;
  default:
    break;
  }
ErrorExit:
    return Status;
}
