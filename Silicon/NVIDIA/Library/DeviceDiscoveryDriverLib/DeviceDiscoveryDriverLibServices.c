/** @file

  Device Discovery Driver Library

  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <libfdt.h>

#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include <Protocol/ClockNodeProtocol.h>
#include <Protocol/ResetNodeProtocol.h>
#include <Protocol/ArmScmiClockProtocol.h>

#include "DeviceDiscoveryDriverLibPrivate.h"

/**
  Retrieves the count of MMIO regions on this controller

  @param[in]  ControllerHandle         Handle of the controller.
  @param[out] RegionCount              Pointer that will contain the number of MMIO regions

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryGetMmioRegionCount (
  IN  EFI_HANDLE  ControllerHandle,
  OUT UINTN       *RegionCount
  )
{
  NON_DISCOVERABLE_DEVICE            *Device;
  EFI_STATUS                         Status;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Desc;
  UINTN                              CurrentResource = 0;

  if (NULL == RegionCount) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  (VOID **)&Device
                  );
  if (EFI_ERROR (Status)) {
    Status = gBS->HandleProtocol (
                    ControllerHandle,
                    &gNVIDIANonDiscoverableDeviceProtocolGuid,
                    (VOID **)&Device
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (Device->Resources != NULL) {
    for (Desc = Device->Resources; Desc->Desc != ACPI_END_TAG_DESCRIPTOR;
         Desc = (VOID *)((UINT8 *)Desc + Desc->Len + 3))
    {
      if ((Desc->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR) ||
          (Desc->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM))
      {
        continue;
      }

      CurrentResource++;
    }
  }

  *RegionCount = CurrentResource;
  return EFI_SUCCESS;
}

/**
  Retrieves the info for a MMIO region on this controller

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  Region                   Region of interest.
  @param[out] RegionBase               Pointer that will contain the base address of the region.
  @param[out] RegionSize               Pointer that will contain the size of the region.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Region is not valid
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryGetMmioRegion (
  IN  EFI_HANDLE            ControllerHandle,
  IN  UINTN                 Region,
  OUT EFI_PHYSICAL_ADDRESS  *RegionBase,
  OUT UINTN                 *RegionSize
  )
{
  NON_DISCOVERABLE_DEVICE            *Device;
  EFI_STATUS                         Status;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Desc;
  UINTN                              CurrentResource = 0;

  if ((NULL == RegionBase) || (NULL == RegionSize)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  (VOID **)&Device
                  );
  if (EFI_ERROR (Status)) {
    Status = gBS->HandleProtocol (
                    ControllerHandle,
                    &gNVIDIANonDiscoverableDeviceProtocolGuid,
                    (VOID **)&Device
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (Device->Resources != NULL) {
    for (Desc = Device->Resources; Desc->Desc != ACPI_END_TAG_DESCRIPTOR;
         Desc = (VOID *)((UINT8 *)Desc + Desc->Len + 3))
    {
      if ((Desc->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR) ||
          (Desc->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM))
      {
        continue;
      }

      if (CurrentResource == Region) {
        *RegionBase = Desc->AddrRangeMin;
        *RegionSize = Desc->AddrLen;
        return EFI_SUCCESS;
      }

      CurrentResource++;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Retrieves the reset id for the specified reset name

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ResetkName               String for the reset name.
  @param[out] ResetId                  Returns the reset id

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Reset name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryGetResetId (
  IN  EFI_HANDLE   ControllerHandle,
  IN  CONST CHAR8  *ResetName,
  OUT UINT32       *ResetId
  )
{
  NVIDIA_RESET_NODE_PROTOCOL  *ResetNodeProtocol = NULL;
  UINTN                       Index;
  EFI_STATUS                  Status;

  if ((NULL == ResetName) || (NULL == ResetId)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetNodeProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < ResetNodeProtocol->Resets; Index++) {
    if (0 == AsciiStrCmp (ResetName, ResetNodeProtocol->ResetEntries[Index].ResetName)) {
      *ResetId = ResetNodeProtocol->ResetEntries[Index].ResetId;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Configures the reset with the specified reset name

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ResetName                String for the reset name.
  @param[in]  Enable                   TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Reset name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryConfigReset (
  IN  EFI_HANDLE   ControllerHandle,
  IN  CONST CHAR8  *ResetName,
  IN  BOOLEAN      Enable
  )
{
  EFI_STATUS                  Status;
  UINT32                      ResetId;
  NVIDIA_RESET_NODE_PROTOCOL  *ResetNodeProtocol = NULL;

  Status = DeviceDiscoveryGetResetId (ControllerHandle, ResetName, &ResetId);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIAResetNodeProtocolGuid, (VOID **)&ResetNodeProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a, no reset node protocol\r\n", __FUNCTION__));
    return Status;
  }

  if (Enable) {
    Status = ResetNodeProtocol->Assert (ResetNodeProtocol, ResetId);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to assert resets %r\r\n", __FUNCTION__, Status));
      return Status;
    }
  } else {
    Status = ResetNodeProtocol->Deassert (ResetNodeProtocol, ResetId);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, failed to deassert resets %r\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Retrieves gets the clock id for the specified clock name

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ClockName                String for the clock name.
  @param[out] ClockId                  Returns the clock id that can be used in the SCMI protocol.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Clock name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryGetClockId (
  IN  EFI_HANDLE   ControllerHandle,
  IN  CONST CHAR8  *ClockName,
  OUT UINT32       *ClockId
  )
{
  NVIDIA_CLOCK_NODE_PROTOCOL  *ClockNodeProtocol = NULL;
  UINTN                       Index;
  EFI_STATUS                  Status;

  if ((NULL == ClockName) || (NULL == ClockId)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIAClockNodeProtocolGuid, (VOID **)&ClockNodeProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* If there is only one clock and the clock name is not valid, then return this clock */
  if ((ClockNodeProtocol->Clocks == 1) && (ClockNodeProtocol->ClockEntries[0].ClockName == NULL)) {
    *ClockId = ClockNodeProtocol->ClockEntries[0].ClockId;
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < ClockNodeProtocol->Clocks; Index++) {
    if (0 == AsciiStrCmp (ClockName, ClockNodeProtocol->ClockEntries[Index].ClockName)) {
      *ClockId = ClockNodeProtocol->ClockEntries[Index].ClockId;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Enables the clock with the specified clock name

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ClockName                String for the clock name.
  @param[in]  Enable                   TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Clock name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryEnableClock (
  IN  EFI_HANDLE   ControllerHandle,
  IN  CONST CHAR8  *ClockName,
  IN  BOOLEAN      Enable
  )
{
  EFI_STATUS  Status;
  UINT32      ClockId;

  if (NULL == gScmiClockProtocol) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceDiscoveryGetClockId (ControllerHandle, ClockName, &ClockId);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return gScmiClockProtocol->Enable (gScmiClockProtocol, ClockId, Enable);
}

/**
  Sets the clock frequency for the clock with the specified clock name

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ClockName                String for the clock name.
  @param[in]  Frequency                Frequency in hertz.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Clock name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoverySetClockFreq (
  IN  EFI_HANDLE   ControllerHandle,
  IN  CONST CHAR8  *ClockName,
  IN  UINT64       Frequency
  )
{
  EFI_STATUS  Status;
  UINT32      ClockId;

  if (NULL == gScmiClockProtocol) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceDiscoveryGetClockId (ControllerHandle, ClockName, &ClockId);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return gScmiClockProtocol->RateSet (gScmiClockProtocol, ClockId, Frequency);
}

/**
  Gets the clock frequency for the clock with the specified clock name

  @param[in]  ControllerHandle   IN  UINT64               Frequency
          Handle of the controller.
  @param[in]  ClockName                String for the clock name.
  @param[out] Frequency                Frequency in hertz.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Clock name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryGetClockFreq (
  IN  EFI_HANDLE   ControllerHandle,
  IN  CONST CHAR8  *ClockName,
  OUT UINT64       *Frequency
  )
{
  EFI_STATUS  Status;
  UINT32      ClockId;

  if (NULL == gScmiClockProtocol) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceDiscoveryGetClockId (ControllerHandle, ClockName, &ClockId);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return gScmiClockProtocol->RateGet (gScmiClockProtocol, ClockId, Frequency);
}

/**
  Sets the parent clock for a given clock with names

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ClockName                String for the clock name.
  @param[in]  ParentClockName          String for the parent clock name.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Clock name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoverySetClockParent (
  IN  EFI_HANDLE   ControllerHandle,
  IN  CONST CHAR8  *ClockName,
  IN  CONST CHAR8  *ParentClockName
  )
{
  EFI_STATUS  Status;
  UINT32      ClockId;
  UINT32      ParentClockId;

  Status = DeviceDiscoveryGetClockId (ControllerHandle, ClockName, &ClockId);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceDiscoveryGetClockId (ControllerHandle, ParentClockName, &ParentClockId);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return gClockParentsProtocol->SetParent (gClockParentsProtocol, ClockId, ParentClockId);
}

/**
  Enable device tree based prod settings

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  DeviceTreeNode           Device tree information
  @param[in]  ProdSetting              String for the prod settings.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Prod setting not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoverySetProd (
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode,
  IN  CONST CHAR8                             *ProdSetting
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  RegionBase = 0;
  UINTN                 RegionSize = 0;
  UINT32                LastRegion;
  CONST UINT32          *Property;
  INT32                 PropertySize;
  UINTN                 Index = 0;
  UINT32                ProdCells;

  INT32  ProdParentOffset;
  INT32  ProdSettingOffset;

  ProdParentOffset = fdt_subnode_offset (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "prod-settings");
  if (ProdParentOffset < 0) {
    return EFI_NOT_FOUND;
  }

  Property = (CONST UINT32 *)fdt_getprop (DeviceTreeNode->DeviceTreeBase, ProdParentOffset, "#prod-cells", NULL);
  if (NULL != Property) {
    ProdCells = SwapBytes32 (*Property);
  } else {
    ProdCells = 3;
  }

  ProdSettingOffset = fdt_subnode_offset (DeviceTreeNode->DeviceTreeBase, ProdParentOffset, ProdSetting);
  if (ProdSettingOffset < 0) {
    return EFI_NOT_FOUND;
  }

  Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, ProdSettingOffset, "prod", &PropertySize);
  if (Property == NULL) {
    return EFI_NOT_FOUND;
  }

  if ((PropertySize % (ProdCells * sizeof (UINT32))) != 0) {
    DEBUG ((DEBUG_ERROR, "Invalid prod size (%d)\r\n", PropertySize));
    return EFI_DEVICE_ERROR;
  }

  LastRegion = 0;
  Status     = DeviceDiscoveryGetMmioRegion (ControllerHandle, LastRegion, &RegionBase, &RegionSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get MMIO region %u\r\n", LastRegion));
    return Status;
  }

  while (Index < (PropertySize/sizeof (UINT32))) {
    UINTN  IndexOffset = 0;
    if (ProdCells == 4) {
      UINT32  Region = SwapBytes32 (Property[Index]);
      if (Region == MAX_UINT32) {
        DEBUG ((DEBUG_ERROR, "Invalid region in prod settings\r\n"));
        return EFI_DEVICE_ERROR;
      }

      if (Region != LastRegion) {
        Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, Region, &RegionBase, &RegionSize);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Failed to get MMIO region %u\r\n", Region));
          return Status;
        }

        LastRegion = Region;
      }

      IndexOffset = 1;
    }

    if (SwapBytes32 (Property[Index+IndexOffset]) >= RegionSize) {
      DEBUG ((DEBUG_ERROR, "Bad offset value %x >= %x\r\n", SwapBytes32 (Property[Index+IndexOffset]), RegionSize));
      return EFI_DEVICE_ERROR;
    }

    MmioAndThenOr32 (RegionBase + SwapBytes32 (Property[Index+IndexOffset]), ~SwapBytes32 (Property[Index+IndexOffset+1]), SwapBytes32 (Property[Index+IndexOffset+2]));
    Index += ProdCells;
  }

  return EFI_SUCCESS;
}
