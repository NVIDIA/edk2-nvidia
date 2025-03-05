/** @file

  SMMUv3 Driver

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "SmmuV3DxePrivate.h"

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "arm,smmu-v3", &gNVIDIANonDiscoverableSmmuV3DeviceGuid },
  { NULL,          NULL                                    }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName = L"NVIDIA Smmu V3 Controller Driver"
};

STATIC
EFI_STATUS
EFIAPI
ResetSmmuV3Controller (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  UINT32  GbpSetting;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Set the controller in global bypass mode
  GbpSetting = FIELD_PREP (1U, SMMU_V3_GBPA_UPDATE_MASK, SMMU_V3_GBPA_UPDATE_SHIFT);
  GbpSetting = GbpSetting | FIELD_PREP (0, SMMU_V3_GBPA_ABORT_MASK, SMMU_V3_GBPA_ABORT_SHIFT);
  GbpSetting = GbpSetting | FIELD_PREP (0, SMMU_V3_GBPA_INSTCFG_MASK, SMMU_V3_GBPA_INSTCFG_SHIFT);
  GbpSetting = GbpSetting | FIELD_PREP (0, SMMU_V3_GBPA_PRIVCFG_MASK, SMMU_V3_GBPA_PRIVCFG_SHIFT);
  GbpSetting = GbpSetting | FIELD_PREP (1, SMMU_V3_GBPA_SHCFG_MASK, SMMU_V3_GBPA_SHCFG_SHIFT);
  GbpSetting = GbpSetting | FIELD_PREP (0, SMMU_V3_GBPA_ALLOCFG_MASK, SMMU_V3_GBPA_ALLOCFG_SHIFT);
  GbpSetting = GbpSetting | FIELD_PREP (0, SMMU_V3_GBPA_MTCFG_MASK, SMMU_V3_GBPA_MTCFG_SHIFT);
  MmioWrite32 (Private->BaseAddress + SMMU_V3_GBPA_OFFSET, GbpSetting);

  // Wait for the controller to enter global bypass mode
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_GBPA_OFFSET) >> SMMU_V3_GBPA_UPDATE_SHIFT) & SMMU_V3_GBPA_UPDATE_MASK) == 1) {
    return EFI_TIMEOUT;
  }

  MmioBitFieldWrite32 (
    Private->BaseAddress + SMMU_V3_CR0_OFFSET,
    SMMU_V3_CR0_SMMUEN_BIT,
    SMMU_V3_CR0_SMMUEN_BIT,
    0
    );

  // Wait for the controller to disable SMMU operation
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_CR0ACK_OFFSET) >> SMMU_V3_CR0_SMMUEN_SHIFT) & SMMU_V3_CR0_SMMUEN_MASK) != 0) {
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

/**
  Initialize the SMMUv3 controller.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 was initialized successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval EFI_DEVICE_ERROR        The SMMUv3 hardware initialization failed.
**/
EFI_STATUS
EFIAPI
InitializeSmmuV3 (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%a: Initializing SMMUv3 at 0x%lx\n", __FUNCTION__, Private->BaseAddress));

  Status = ResetSmmuV3Controller (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to reset SMMUv3\n", __FUNCTION__));
    return Status;
  }

  // TODO: Implement SMMUv3 initialization steps:
  // 1. Check hardware status
  // 2. Configure global settings
  // 3. Setup command queue
  // 4. Setup event queue
  // 5. Enable SMMU operation

  // Temporary placeholder - just return success
  return EFI_SUCCESS;
}

/**
  Exit Boot Services Event notification handler.

  @param[in]  Event     Event whose notification function is being invoked.
  @param[in]  Context   Pointer to the notification function's context.

**/
VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private;

  gBS->CloseEvent (Event);

  Private = (SMMU_V3_CONTROLLER_PRIVATE_DATA *)Context;

  // TODO: Implement SMMUv3 exit boot services steps:
  // 1. Disable SMMU operation
  // 2. Disable command queue
  // 3. Disable event queue
  // 4. Disable SMMU operation

  DEBUG ((DEBUG_ERROR, "%a: Put SMMU at 0x%lx back in global bypass\n", __FUNCTION__, Private->BaseAddress));
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
  EFI_PHYSICAL_ADDRESS             BaseAddress;
  UINTN                            RegionSize;
  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private;
  UINT32                           NodeHandle;

  Status      = EFI_SUCCESS;
  BaseAddress = 0;
  RegionSize  = 0;
  Private     = NULL;
  NodeHandle  = 0;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      Status = DeviceTreeGetNodePHandle (DeviceTreeNode->NodeOffset, &NodeHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to get phandle for node\n", __FUNCTION__));
        goto Exit;
      }

      break;

    case DeviceDiscoveryDriverBindingStart:
      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
        goto Exit;
      }

      Private = AllocateZeroPool (sizeof (SMMU_V3_CONTROLLER_PRIVATE_DATA));
      if (Private == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      Private->Signature      = SMMU_V3_CONTROLLER_SIGNATURE;
      Private->BaseAddress    = BaseAddress;
      Private->DeviceTreeBase = DeviceTreeNode->DeviceTreeBase;
      Private->NodeOffset     = DeviceTreeNode->NodeOffset;

      Status = DeviceTreeGetNodePHandle (DeviceTreeNode->NodeOffset, &Private->SmmuV3ControllerProtocol.PHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to get phandle for node\n", __FUNCTION__));
        goto Exit;
      }

      DEBUG ((DEBUG_ERROR, "%a: Base Addr 0x%lx\n", __FUNCTION__, Private->BaseAddress));
      DEBUG ((DEBUG_ERROR, "%a: PHandle 0x%lx\n", __FUNCTION__, Private->SmmuV3ControllerProtocol.PHandle));

      Status = InitializeSmmuV3 (Private);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to initialize SMMUv3\n", __FUNCTION__));
        goto Exit;
      }

      // Create an event to notify when the system is ready to exit boot services.
      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_NOTIFY,
                      OnExitBootServices,
                      Private,
                      &gEfiEventExitBootServicesGuid,
                      &Private->ExitBootServicesEvent
                      );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      // Install the SMMUv3 protocol.
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gNVIDIASmmuV3ProtocolGuid,
                      &Private->SmmuV3ControllerProtocol,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      break;

    default:
      break;
  }

Exit:
  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      FreePool (Private);
    }
  }

  return Status;
}
