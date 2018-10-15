/** @file
  NVIDIA Device Discovery Driver

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>
#include <Library/DxeServicesTableLib.h>
#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/DeviceTreeCompatibility.h>

#include "DeviceDiscoveryPrivate.h"

/**
  Function map region into GCD and MMU

  @param[in]  Private               Pointer to the private device discovery data structure.
  @param[in]  BaseAddress           Base address of region
  @param[in]  Size                  Size of region

  @return EFI_SUCCESS               GCD/MMU Updated.

**/
EFI_STATUS
AddMemoryRegion (
  IN  DEVICE_DISCOVERY_PRIVATE            *Private,
  IN  UINT64                              BaseAddress,
  IN  UINT64                              Size
  )
{
  EFI_STATUS Status;
  UINT64 AlignedBaseAddress = BaseAddress & ~(SIZE_4KB-1);
  UINT64 AlignedSize = Size + (BaseAddress - AlignedBaseAddress);
  AlignedSize = ALIGN_VALUE (Size, SIZE_4KB);

  Status = gDS->AddMemorySpace (EfiGcdMemoryTypeMemoryMappedIo,
                                AlignedBaseAddress,
                                AlignedSize,
                                EFI_MEMORY_UC | EFI_MEMORY_RUNTIME);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to AddMemorySpace: %r.\r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = gDS->SetMemorySpaceAttributes (AlignedBaseAddress,
                                          AlignedSize,
                                          EFI_MEMORY_UC);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to SetMemorySpaceAttributes: %r.\r\n", __FUNCTION__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}


/**
  Function detects MMIO resources of Node and creates resource descriptor.
  Will also map resources into GCD and MMU

  @param[in]  Private               Pointer to the private device discovery data structure.
  @param[in]  NodeOffset            Offset into the device tree for the node
  @param[out] Resources             Pointer that will contain the resources of the node.
                                    This structure should be freed once no longer needed.

  @return EFI_SUCCESS               Region structure created, MMU updated.
  @return EFI_UNSUPPORTED           Node regions not supported
  @return EFI_NOT_FOUND             reg entry not in device tree node
  @return EFI_OUT_OF_RESOURCES      Allocation failure
  @return EFI_DEVICE_ERROR          Failure to add to GCD/MMU
  @return EFI_INVALID_PARAMETER     Private or Resources are NULL

**/
EFI_STATUS
GetResources (
  IN  DEVICE_DISCOVERY_PRIVATE            *Private,
  IN  INT32                               NodeOffset,
  OUT EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR   **Resources
  )
{
  EFI_STATUS Status;
  INT32 AddressCells  = fdt_address_cells (Private->DeviceTreeBase, NodeOffset);
  INT32 SizeCells     = fdt_size_cells (Private->DeviceTreeBase, NodeOffset);
  CONST VOID  *Property = NULL;
  UINTN EntrySize     = 0;
  INT32 PropertySize  = 0;
  UINTN NumberOfRegions;
  UINTN RegionIndex = 0;
  UINTN AllocationSize;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR   *AllocResources = NULL;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR   *Desc;
  EFI_ACPI_END_TAG_DESCRIPTOR         *End;

  if ((NULL == Private) ||
      (NULL == Resources)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((AddressCells > 2) ||
      (AddressCells == 0) ||
      (SizeCells > 2) ||
      (SizeCells == 0)) {
    DEBUG ((EFI_D_ERROR, "%a: Bad cell values, %d, %d\r\n", __FUNCTION__, AddressCells, SizeCells));
    return EFI_UNSUPPORTED;
  }

  Property = fdt_getprop (Private->DeviceTreeBase,
                          NodeOffset,
                          "reg",
                          &PropertySize);
  if (NULL == Property) {
    DEBUG ((EFI_D_ERROR, "%a: No reg property\r\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  EntrySize = sizeof (UINT32) * (AddressCells + SizeCells);
  ASSERT ((PropertySize % EntrySize) == 0);
  NumberOfRegions = PropertySize / EntrySize;
  if (NumberOfRegions == 0) {
    DEBUG ((EFI_D_ERROR, "%a: no regions detected. \r\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  AllocationSize = NumberOfRegions * sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR);

  AllocResources = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)AllocateZeroPool (AllocationSize);
  if (NULL == AllocResources) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate ACPI resources.\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }
  *Resources = AllocResources;

  for (RegionIndex = 0; RegionIndex < NumberOfRegions; RegionIndex++) {
    UINT64 AddressBase = 0;
    UINT64 RegionSize  = 0;

    CopyMem ((VOID *)&AddressBase, Property + EntrySize * RegionIndex, AddressCells * sizeof (UINT32));
    CopyMem ((VOID *)&RegionSize, Property + EntrySize * RegionIndex + (AddressCells * sizeof (UINT32)),  SizeCells * sizeof (UINT32));
    if (AddressCells == 2) {
      AddressBase = SwapBytes64 (AddressBase);
    } else {
      AddressBase = SwapBytes32 (AddressBase);
    }

    if (SizeCells == 2) {
      RegionSize = SwapBytes64 (RegionSize);
    } else {
      RegionSize = SwapBytes32 (RegionSize);
    }

    Desc = &AllocResources [RegionIndex];
    Desc->Desc                  = ACPI_ADDRESS_SPACE_DESCRIPTOR;
    Desc->Len                   = sizeof (*Desc) - 3;
    Desc->AddrRangeMin          = AddressBase;
    Desc->AddrLen               = RegionSize;
    Desc->AddrRangeMax          = AddressBase + RegionSize - 1;
    Desc->ResType               = ACPI_ADDRESS_SPACE_TYPE_MEM;
    Desc->AddrSpaceGranularity  = ((EFI_PHYSICAL_ADDRESS)AddressBase + RegionSize > SIZE_4GB) ? 64 : 32;
    Desc->AddrTranslationOffset = 0;

    Status = AddMemoryRegion (Private, AddressBase, RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to add region 0x%016lx, 0x%016lx: %r.\r\n",
          __FUNCTION__,
          AddressBase,
          RegionSize,
          Status));
      FreePool (AllocResources);
      *Resources = NULL;
      return EFI_DEVICE_ERROR;
    }
  }


  End = (EFI_ACPI_END_TAG_DESCRIPTOR *)&AllocResources [NumberOfRegions];
  End->Desc     = ACPI_END_TAG_DESCRIPTOR;
  End->Checksum = 0;

  return EFI_SUCCESS;
}

/**
  Function that attempts connecting a device tree node to a driver.

  @param[in]  Private               Pointer to the private device discovery data structure.
  @param[in]  NodeOffset            Offset into the device tree for the node
  @param[in]  DriverHandle          Handle of the driver to test

  @return EFI_SUCCESS               Driver handles this node, protocols installed.
  @return EFI_UNSUPPORTED           Driver does not support this node.
  @return others                    Error occured during setup.

**/
EFI_STATUS
ProcessDeviceTreeNodeWithHandle(
  IN  DEVICE_DISCOVERY_PRIVATE *Private,
  IN  INT32                    NodeOffset,
  IN  EFI_HANDLE               DriverHandle
  )
{
  EFI_STATUS                                Status;
  NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL *CompatibilityProtocol = NULL;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL          NodeProtocol;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL          *NodeProtocolCopy = NULL;
  EFI_GUID                                  *DeviceType = NULL;
  NON_DISCOVERABLE_DEVICE_INIT              PciIoInitialize;
  NON_DISCOVERABLE_DEVICE                   *Device = NULL;
  VOID                                      *Interface = NULL;
  BOOLEAN                                   SupportsBinding = FALSE;
  EFI_GUID                                  *DeviceProtocolGuid;
  EFI_HANDLE                                DeviceHandle = NULL;
  DEVICE_DISCOVERY_DEVICE_PATH              *DevicePath = NULL;
  EFI_HANDLE                                ConnectHandles[2];

  NodeProtocol.DeviceTreeBase = Private->DeviceTreeBase;
  NodeProtocol.NodeOffset = NodeOffset;

  Status = gBS->HandleProtocol (DriverHandle,
                                &gNVIDIADeviceTreeCompatibilityProtocolGuid,
                                (VOID **)&CompatibilityProtocol);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = CompatibilityProtocol->Supported (CompatibilityProtocol,
                                             &NodeProtocol,
                                             &DeviceType,
                                             &PciIoInitialize);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }


  Device = (NON_DISCOVERABLE_DEVICE *)AllocatePool (sizeof (NON_DISCOVERABLE_DEVICE));
  if (NULL == Device) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate device protocol.\r\n", __FUNCTION__));
    goto ErrorExit;
  }
  Device->Type = DeviceType;
  Device->Initialize = PciIoInitialize;
  Device->Resources = NULL;

  //Check if DMA is coherent
  if (NULL != fdt_get_property (Private->DeviceTreeBase, NodeOffset, "dma-coherent", NULL)) {
    Device->DmaType = NonDiscoverableDeviceDmaTypeCoherent;
  } else {
    Device->DmaType = NonDiscoverableDeviceDmaTypeNonCoherent;
  }

  Status = GetResources (Private, NodeOffset, &Device->Resources);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get node resources: %r.\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  //First resource must be MMIO
  if ((Device->Resources == NULL) ||
      (Device->Resources->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR) ||
      (Device->Resources->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM)) {
    DEBUG ((EFI_D_ERROR, "%a: Invalid node resources.\r\n", __FUNCTION__));
    goto ErrorExit;
  }

  DevicePath = (DEVICE_DISCOVERY_DEVICE_PATH *)CreateDeviceNode (
                                                 HARDWARE_DEVICE_PATH,
                                                 HW_MEMMAP_DP,
                                                 sizeof (*DevicePath));
  if (NULL == DevicePath) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  DevicePath->MemMap.MemoryType = EfiMemoryMappedIO;
  DevicePath->MemMap.StartingAddress = Device->Resources->AddrRangeMin;
  DevicePath->MemMap.EndingAddress = Device->Resources->AddrRangeMax;

  SetDevicePathNodeLength (&DevicePath->MemMap, sizeof (DevicePath->MemMap));
  SetDevicePathEndNode (&DevicePath->End);


  NodeProtocolCopy = AllocateCopyPool (sizeof (NodeProtocol), (VOID *)&NodeProtocol);
  if (NULL == NodeProtocolCopy) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate node protocol.\r\n", __FUNCTION__));
    goto ErrorExit;
  }

  SupportsBinding = !EFI_ERROR (gBS->HandleProtocol (DriverHandle,
                                                     &gEfiDriverBindingProtocolGuid,
                                                     &Interface));
  if (SupportsBinding) {
    DeviceProtocolGuid = &gNVIDIANonDiscoverableDeviceProtocolGuid;
  } else {
    DeviceProtocolGuid = &gEdkiiNonDiscoverableDeviceProtocolGuid;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &DeviceHandle,
                  DeviceProtocolGuid,
                  Device,
                  &gNVIDIADeviceTreeNodeProtocolGuid,
                  NodeProtocolCopy,
                  &gEfiDevicePathProtocolGuid,
                  DevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get install protocols: %r.\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  //Any errors after here should uninstall protocols.
  if (SupportsBinding) {
    ConnectHandles[0] = DriverHandle;
    ConnectHandles[1] = NULL;
    gBS->ConnectController (DeviceHandle, ConnectHandles, NULL, FALSE);
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (NULL != Device) {
      if (NULL != Device->Resources) {
        FreePool (Device);
      }
      FreePool (Device);
    }
    if (NULL != NodeProtocolCopy) {
      FreePool (NodeProtocolCopy);
    }
    if (NULL != DevicePath) {
      FreePool (DevicePath);
    }
  }

  return Status;
}


/**
  Notification function that will be called each time gNVIDIADeviceTreeCompatibilityProtocolGuid
  is installed.

  @param[in]  Event                 Event whose notification function is being invoked.
  @param[in]  Context               The pointer to the notification function's context,
                                    which is implementation-dependent.

**/
VOID
CompatibilityProtocolNotification(
  IN  EFI_EVENT                Event,
  IN  VOID                     *Context
  )
{
  DEVICE_DISCOVERY_PRIVATE *Private = (DEVICE_DISCOVERY_PRIVATE *)Context;
  EFI_STATUS               Status = EFI_BUFFER_TOO_SMALL;
  EFI_HANDLE               *HandleBuffer = NULL;
  UINTN                    Handles = 0;
  INT32                    NodeOffset = 0;

  if (Context == NULL) {
    goto ErrorExit;
  }

  Status = gBS->LocateHandleBuffer (
                  ByRegisterNotify,
                  &gNVIDIADeviceTreeCompatibilityProtocolGuid,
                  Private->SearchKey,
                  &Handles,
                  &HandleBuffer);
  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((EFI_D_ERROR, "%a: LocateHandleBuffer returned %r.\r\n", __FUNCTION__, Status));
    }
    return;
  }

  NodeOffset = 0;
  do {
    UINTN HandleIndex;
    NodeOffset = fdt_next_node (Private->DeviceTreeBase, NodeOffset, NULL);
    if (NodeOffset < 0) {
      break;
    }

    for (HandleIndex = 0; HandleIndex < Handles; HandleIndex++) {
      Status = ProcessDeviceTreeNodeWithHandle (Private, NodeOffset, HandleBuffer[HandleIndex]);
      if (!EFI_ERROR (Status)) {
        break;
      }
    }
  } while (NodeOffset > 0);

ErrorExit:
  if (NULL != HandleBuffer) {
    FreePool (HandleBuffer);
  }
  gBS->SignalEvent (Event);
}

/**
  Initialize the state information for the Device Discovery Driver

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
DeviceDiscoveryDxeEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS               Status;
  DEVICE_DISCOVERY_PRIVATE *Private = NULL;
  BOOLEAN                  EventCreated = FALSE;

  Private = AllocatePool (sizeof(DEVICE_DISCOVERY_PRIVATE));
  if (NULL == Private) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Status = DtPlatformLoadDtb (&Private->DeviceTreeBase, &Private->DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get device tree: %r.\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  Private->ProtocolNotificationEvent = EfiCreateProtocolNotifyEvent (
                                         &gNVIDIADeviceTreeCompatibilityProtocolGuid,
                                         TPL_CALLBACK,
                                         CompatibilityProtocolNotification,
                                         (VOID *) Private,
                                         &Private->SearchKey
                                         );
  if (NULL == Private->ProtocolNotificationEvent) {
    //Non-fatal, event will get processed later
    DEBUG ((EFI_D_ERROR, "%a: Failed create event: %r.\r\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto ErrorExit;
  }

  //Register protocol to let drivers that do not use driver binding to declare depex.
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gNVIDIADeviceEnumerationPresentProtocolGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed install procotol: %r.\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (NULL != Private) {
      if (EventCreated) {
        gBS->CloseEvent (Private->ProtocolNotificationEvent);
      }
      FreePool (Private);
    }
  }

  return Status;
}

