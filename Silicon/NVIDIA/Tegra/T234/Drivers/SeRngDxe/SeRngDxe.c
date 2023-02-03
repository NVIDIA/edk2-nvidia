/** @file

  SE RNG Controller Driver

  Copyright (c) 2019-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>
#include <libfdt.h>
#include <Library/CacheMaintenanceLib.h>
#include <Protocol/NonDiscoverableDevice.h>

#include "SeRngPrivate.h"

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra234-se-ahb", &gNVIDIANonDiscoverableT234SeDeviceGuid },
  { NULL,                     NULL                                    }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA SE RNG controller driver",
  .UseDriverBinding                = TRUE,
  .AutoEnableClocks                = TRUE,
  .AutoDeassertReset               = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE
};

/**
  Gets 128-bits of random data from SE.

  @param[in]     This                The instance of the NVIDIA_SE_RNG_PROTOCOL.
  @param[in]     Buffer              Buffer to place data into

  @return EFI_SUCCESS               The data was returned.
  @return EFI_INVALID_PARAMETER     Buffer is NULL.
  @return EFI_DEVICE_ERROR          Failed to get random data.
**/
STATIC
EFI_STATUS
SeRngGetRandom128 (
  IN  NVIDIA_SE_RNG_PROTOCOL  *This,
  IN  UINT64                  *Buffer
  )
{
  SE_RNG_PRIVATE_DATA  *Private;
  EFI_STATUS           Status;
  UINT32               UpperAddress;
  UINT32               MaxPollCount = SE_MAX_POLL_COUNT;
  UINT32               AesStatus;

  if ((This == NULL) ||
      (Buffer == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private = SE_RNG_PRIVATE_DATA_FROM_THIS (This);

  // GENRNG command
  MmioWrite32 (
    Private->BaseAddress + SE0_AES0_CONFIG_0,
    SE0_AES0_CONFIG_0_DST_MEMORY |
    SE0_AES0_CONFIG_0_DEC_ALG_NOP |
    SE0_AES0_CONFIG_0_ENC_ALG_RNG
    );

  WriteBackDataCacheRange (Buffer, RANDOM_BYTES);

  MmioWrite32 (Private->BaseAddress + SE0_AES0_OUT_ADDR_0, (UINT32)(UINTN)Buffer);

  UpperAddress  = (((UINTN)(VOID *)Buffer >> 32) << SE0_AES0_OUT_ADDR_HI_0_MSB_SHIFT) & SE0_AES0_OUT_ADDR_HI_0_MSB_MASK;
  UpperAddress |= (RANDOM_BYTES << SE0_AES0_OUT_ADDR_HI_0_SZ_SHIFT) & SE0_AES0_OUT_ADDR_HI_0_SZ_MASK;
  MmioWrite32 (Private->BaseAddress + SE0_AES0_OUT_ADDR_HI_0, UpperAddress);

  // Always support 1 block
  MmioWrite32 (Private->BaseAddress + SE0_AES0_CRYPTO_LAST_BLOCK_0, 0);

  MmioWrite32 (
    Private->BaseAddress + SE0_AES0_OPERATION_0,
    SE0_AES0_OPERATION_0_LASTBUF_TRUE |
    SE0_AES0_OPERATION_0_OP_START
    );

  do {
    AesStatus = MmioRead32 (Private->BaseAddress + SE0_AES0_STATUS_0);
    MaxPollCount--;
  } while ((MaxPollCount > 0) && (AesStatus != 0));

  if (AesStatus != 0) {
    DEBUG ((DEBUG_ERROR, "%a, Timeout waiting for random\r\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto ErrorExit;
  }

  InvalidateDataCacheRange (Buffer, RANDOM_BYTES);

  Status = EFI_SUCCESS;

ErrorExit:

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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  SE_RNG_PRIVATE_DATA      *Private;
  EFI_STATUS               Status;
  UINTN                    RegionSize;
  NON_DISCOVERABLE_DEVICE  *Device;

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

      Private = (SE_RNG_PRIVATE_DATA *)AllocateZeroPool (sizeof (SE_RNG_PRIVATE_DATA));
      if (Private == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        DEBUG ((DEBUG_ERROR, "SeRngDxe: Failed to allocate private data structure\r\n"));
        break;
      }

      Private->Signature = SE_RNG_SIGNATURE;
      Status             = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &Private->BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "SeRngDxe: Failed to get region location (%r)\r\n", Status));
        FreePool (Private);
        break;
      }

      Private->SeRngProtocol.GetRandom128 = SeRngGetRandom128;

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gEfiCallerIdGuid,
                      Private,
                      &gNVIDIASeRngProtocolGuid,
                      &Private->SeRngProtocol,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "SeRngDxe: Failed to install protocol (%r)\r\n", Status));
        FreePool (Private);
        break;
      }

      break;

    case DeviceDiscoveryDriverBindingStop:
      Status = gBS->HandleProtocol (ControllerHandle, &gEfiCallerIdGuid, (VOID **)&Private);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "SeRng: Failed to get private data (%r)\r\n", Status));
        break;
      }

      Status = gBS->UninstallMultipleProtocolInterfaces (
                      ControllerHandle,
                      &gEfiCallerIdGuid,
                      Private,
                      &gNVIDIASeRngProtocolGuid,
                      &Private->SeRngProtocol,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "SeRng: Failed to uninstall procotol (%r)\r\n", Status));
        break;
      }

      FreePool (Private);
      Status = EFI_SUCCESS;
      break;

    default:
      Status = EFI_SUCCESS;
      break;
  }

  return Status;
}
