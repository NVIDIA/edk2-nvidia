/** @file

  EFUSE Driver

  SPDX-FileCopyrightText: Copyright (c) 2019-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/ResetNodeProtocol.h>
#include <libfdt.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include "EFuseDxePrivate.h"

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,*-efuse", &gNVIDIANonDiscoverableEFuseDeviceGuid },
  { NULL,             NULL                                   }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                                 = L"NVIDIA EFuse driver",
  .AutoEnableClocks                           = TRUE,
  .AutoDeassertReset                          = TRUE,
  .AutoResetModule                            = FALSE,
  .AutoDeassertPg                             = FALSE,
  .SkipEdkiiNondiscoverableInstall            = TRUE,
  .SkipAutoDeinitControllerOnExitBootServices = TRUE
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
  IN  UINT32                 RegisterOffset,
  OUT UINT32                 *RegisterValue
  )
{
  EFI_STATUS         Status;
  EFUSE_DXE_PRIVATE  *Private;

  Status = EFI_SUCCESS;

  Private = EFUSE_PRIVATE_DATA_FROM_THIS (This);
  if ((RegisterOffset > (Private->RegionSize - sizeof (UINT32))) ||
      (RegisterValue == NULL))
  {
    Status = EFI_INVALID_PARAMETER;
  } else {
    *RegisterValue = MmioRead32 (Private->BaseAddress + RegisterOffset);
    Status         = EFI_SUCCESS;
  }

  return Status;
}

/**
  This function writes and returns the value of a specified Fuse Register

  @param[in]        This                The instance of NVIDIA_EFUSE_PROTOCOL.
  @param[in]        RegisterOffset      Offset from the EFUSE Base address to write.
  @param[in out]    RegisterValue       Value of the Write Fuse Register.

  @return EFI_SUCCESS                Fuse Register Value successfully returned.
  @return EFI_INVALID_PARAMETER      Register Offset param not in EFUSE Region
  @return EFI_DEVICE_ERROR           Other error occured in reading FUSE Registers.
**/
STATIC
EFI_STATUS
EfuseWriteRegister (
  IN     NVIDIA_EFUSE_PROTOCOL  *This,
  IN     UINT32                 RegisterOffset,
  IN OUT UINT32                 *RegisterValue
  )
{
  // Write is not supported on existing platforms.
  NV_ASSERT_RETURN (FALSE, return EFI_DEVICE_ERROR, "Efuse write is not supported\r\n");
  return EFI_DEVICE_ERROR;
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS             Status;
  EFI_PHYSICAL_ADDRESS   BaseAddress;
  UINTN                  RegionSize;
  CONST VOID             *Property;
  NVIDIA_EFUSE_PROTOCOL  *EFuseProtocol;
  EFUSE_DXE_PRIVATE      *Private;

  Status      = EFI_SUCCESS;
  BaseAddress = 0;
  Private     = NULL;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:

      Status = DeviceDiscoveryGetMmioRegion (
                 ControllerHandle,
                 0,
                 &BaseAddress,
                 &RegionSize
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Couldn't find Efuse address range\n",
          __FUNCTION__
          ));
        return Status;
      }

      Private = AllocatePool (sizeof (EFUSE_DXE_PRIVATE));
      if (NULL == Private) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Memory\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        return Status;
      }

      Private->Signature              = EFUSE_SIGNATURE;
      Private->ImageHandle            = DriverHandle;
      Private->BaseAddress            = BaseAddress;
      Private->RegionSize             = RegionSize;
      Private->EFuseProtocol.ReadReg  = EfuseReadRegister;
      Private->EFuseProtocol.WriteReg = EfuseWriteRegister;

      Property = NULL;
      Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "nvidia,hw-instance-id", NULL);
      if (Property == NULL) {
        Private->EFuseProtocol.Socket = 0;
      } else {
        Private->EFuseProtocol.Socket = SwapBytes32 (*(CONST UINT32 *)Property);
      }

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gNVIDIAEFuseProtocolGuid,
                      &Private->EFuseProtocol,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a, Failed to install protocols: %r\r\n",
          __FUNCTION__,
          Status
          ));
        FreePool (Private);
        return Status;
      }

      DEBUG ((DEBUG_ERROR, "%a: Efuse Installed\r\n", __FUNCTION__));
      break;

    case DeviceDiscoveryDriverBindingStop:

      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gNVIDIAEFuseProtocolGuid,
                      (VOID **)&EFuseProtocol
                      );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Private = EFUSE_PRIVATE_DATA_FROM_PROTOCOL (EFuseProtocol);

      Status =  gBS->UninstallMultipleProtocolInterfaces (
                       ControllerHandle,
                       &gNVIDIAEFuseProtocolGuid,
                       &Private->EFuseProtocol,
                       NULL
                       );
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
