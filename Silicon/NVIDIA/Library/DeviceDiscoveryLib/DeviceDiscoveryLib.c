/** @file
  NVIDIA Device Discovery Driver

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
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>
#include <Library/DxeServicesTableLib.h>
#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include <Protocol/ClockNodeProtocol.h>
#include <Protocol/ResetNodeProtocol.h>
#include <Protocol/PowerGateNodeProtocol.h>
#include <Protocol/C2CNodeProtocol.h>
#include <Protocol/ArmScmiClock2Protocol.h>
#include <Protocol/ClockParents.h>
#include <Protocol/BpmpIpc.h>

#include <Library/DeviceDiscoveryLib.h>
#include "DeviceDiscoveryLibPrivate.h"

/**
  Function map region into GCD and MMU

  @param[in]  Private               Pointer to the private device discovery data structure.
  @param[in]  BaseAddress           Base address of region
  @param[in]  Size                  Size of region

  @return EFI_SUCCESS               GCD/MMU Updated.

**/
EFI_STATUS
AddMemoryRegion (
  IN  UINT64  BaseAddress,
  IN  UINT64  Size
  )
{
  EFI_STATUS  Status;
  UINT64      AlignedBaseAddress = BaseAddress & ~(SIZE_4KB-1);
  UINT64      AlignedSize        = Size + (BaseAddress - AlignedBaseAddress);
  UINT64      AlignedEnd;
  UINT64      ScanLocation;

  AlignedSize = ALIGN_VALUE (Size, SIZE_4KB);
  AlignedEnd  = AlignedBaseAddress + AlignedSize;

  ScanLocation = AlignedBaseAddress;
  while (ScanLocation < AlignedEnd) {
    EFI_GCD_MEMORY_SPACE_DESCRIPTOR  MemorySpace;
    UINT64                           OverlapSize;

    Status = gDS->GetMemorySpaceDescriptor (ScanLocation, &MemorySpace);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to GetMemorySpaceDescriptor (0x%llx): %r.\r\n", __FUNCTION__, ScanLocation, Status));
      return Status;
    }

    OverlapSize = MIN (MemorySpace.BaseAddress + MemorySpace.Length, AlignedEnd) - ScanLocation;
    if (MemorySpace.GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
      Status = gDS->AddMemorySpace (
                      EfiGcdMemoryTypeMemoryMappedIo,
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to AddMemorySpace: (0x%llx, 0x%llx) %r.\r\n", __FUNCTION__, ScanLocation, OverlapSize, Status));
        return Status;
      }

      Status = gDS->SetMemorySpaceAttributes (
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to SetMemorySpaceAttributes: (0x%llx, 0x%llx) %r.\r\n", __FUNCTION__, ScanLocation, OverlapSize, Status));
        return Status;
      }
    }

    ScanLocation += OverlapSize;
  }

  return EFI_SUCCESS;
}

/**
  Function detects MMIO resources of Node and creates resource descriptor.
  Will also map resources into GCD and MMU

  @param[in]  DeviceTreeBase               Pointer to the private device discovery data structure.
  @param[in]  NodeOffset            Offset into the device tree for the node
  @param[out] Resources             Pointer that will contain the resources of the node.
                                    This structure should be freed once no longer needed.

  @return EFI_SUCCESS               Region structure created, MMU updated.
  @return EFI_UNSUPPORTED           Node regions not supported
  @return EFI_NOT_FOUND             reg entry not in device tree node
  @return EFI_OUT_OF_RESOURCES      Allocation failure
  @return EFI_DEVICE_ERROR          Failure to add to GCD/MMU
  @return EFI_INVALID_PARAMETER     DeviceTreeBase or Resources are NULL

**/
EFI_STATUS
GetResources (
  IN  VOID                               *DeviceTreeBase,
  IN  INT32                              NodeOffset,
  OUT EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  **Resources
  )
{
  EFI_STATUS                         Status;
  INT32                              AddressCells;
  INT32                              SizeCells;
  CONST VOID                         *RegProperty             = NULL;
  CONST VOID                         *SharedMemProperty       = NULL;
  UINTN                              EntrySize                = 0;
  INT32                              PropertySize             = 0;
  UINTN                              NumberOfRegions          = 0;
  UINTN                              NumberOfRegRegions       = 0;
  UINTN                              NumberOfSharedMemRegions = 0;
  UINTN                              RegionIndex              = 0;
  UINTN                              SharedMemoryIndex        = 0;
  UINTN                              AllocationSize;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *AllocResources = NULL;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Desc;
  EFI_ACPI_END_TAG_DESCRIPTOR        *End;

  if ((NULL == DeviceTreeBase) ||
      (NULL == Resources))
  {
    return EFI_INVALID_PARAMETER;
  }

  AddressCells = fdt_address_cells (DeviceTreeBase, fdt_parent_offset (DeviceTreeBase, NodeOffset));
  SizeCells    = fdt_size_cells (DeviceTreeBase, fdt_parent_offset (DeviceTreeBase, NodeOffset));

  if ((AddressCells > 2) ||
      (AddressCells == 0) ||
      (SizeCells > 2) ||
      (SizeCells == 0))
  {
    DEBUG ((DEBUG_ERROR, "%a: Bad cell values, %d, %d\r\n", __FUNCTION__, AddressCells, SizeCells));
    return EFI_UNSUPPORTED;
  }

  RegProperty = fdt_getprop (
                  DeviceTreeBase,
                  NodeOffset,
                  "reg",
                  &PropertySize
                  );
  if (NULL != RegProperty) {
    EntrySize = sizeof (UINT32) * (AddressCells + SizeCells);
    ASSERT ((PropertySize % EntrySize) == 0);
    NumberOfRegRegions = PropertySize / EntrySize;
  }

  SharedMemProperty = fdt_getprop (
                        DeviceTreeBase,
                        NodeOffset,
                        "shmem",
                        &PropertySize
                        );
  if (NULL != SharedMemProperty) {
    ASSERT ((PropertySize % sizeof (UINT32)) == 0);
    NumberOfSharedMemRegions = PropertySize / sizeof (UINT32);
  }

  NumberOfRegions = NumberOfRegRegions + NumberOfSharedMemRegions;

  if (NumberOfRegions != 0) {
    AllocationSize = NumberOfRegions * sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR);

    AllocResources = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)AllocateZeroPool (AllocationSize);
    if (NULL == AllocResources) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate ACPI resources.\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }
  } else {
    *Resources = NULL;
    return EFI_SUCCESS;
  }

  *Resources = AllocResources;

  for (RegionIndex = 0; RegionIndex < NumberOfRegRegions; RegionIndex++) {
    UINT64  AddressBase = 0;
    UINT64  RegionSize  = 0;

    CopyMem ((VOID *)&AddressBase, RegProperty + EntrySize * RegionIndex, AddressCells * sizeof (UINT32));
    CopyMem ((VOID *)&RegionSize, RegProperty + EntrySize * RegionIndex + (AddressCells * sizeof (UINT32)), SizeCells * sizeof (UINT32));
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

    Desc                        = &AllocResources[RegionIndex];
    Desc->Desc                  = ACPI_ADDRESS_SPACE_DESCRIPTOR;
    Desc->Len                   = sizeof (*Desc) - 3;
    Desc->AddrRangeMin          = AddressBase;
    Desc->AddrLen               = RegionSize;
    Desc->AddrRangeMax          = AddressBase + RegionSize - 1;
    Desc->ResType               = ACPI_ADDRESS_SPACE_TYPE_MEM;
    Desc->AddrSpaceGranularity  = ((EFI_PHYSICAL_ADDRESS)AddressBase + RegionSize > SIZE_4GB) ? 64 : 32;
    Desc->AddrTranslationOffset = 0;

    Status = AddMemoryRegion (AddressBase, RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to add region 0x%016lx, 0x%016lx: %r.\r\n",
        __FUNCTION__,
        AddressBase,
        RegionSize,
        Status
        ));
      FreePool (AllocResources);
      *Resources = NULL;
      return EFI_DEVICE_ERROR;
    }
  }

  for (SharedMemoryIndex = 0; SharedMemoryIndex < NumberOfSharedMemRegions; SharedMemoryIndex++) {
    UINT32  *HandleArray    = (UINT32 *)SharedMemProperty;
    UINT32  Handle          = SwapBytes32 (HandleArray[SharedMemoryIndex]);
    INT32   SharedMemOffset = fdt_node_offset_by_phandle (DeviceTreeBase, Handle);
    INT32   ParentOffset;
    UINT64  ParentAddressBase = 0;
    UINT64  AddressBase       = 0;
    UINT64  RegionSize        = 0;

    if (SharedMemOffset <= 0) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Unable to locate shared memory handle %u\r\n",
        __FUNCTION__,
        Handle
        ));
      FreePool (AllocResources);
      *Resources = NULL;
      return EFI_DEVICE_ERROR;
    }

    ParentOffset = fdt_parent_offset (DeviceTreeBase, SharedMemOffset);
    if (ParentOffset < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Unable to locate shared memory handle's parent %u\r\n",
        __FUNCTION__,
        Handle
        ));
      FreePool (AllocResources);
      *Resources = NULL;
      return EFI_DEVICE_ERROR;
    }

    AddressCells = fdt_address_cells (DeviceTreeBase, fdt_parent_offset (DeviceTreeBase, NodeOffset));
    SizeCells    = fdt_size_cells (DeviceTreeBase, fdt_parent_offset (DeviceTreeBase, NodeOffset));

    if ((AddressCells > 2) ||
        (AddressCells == 0) ||
        (SizeCells > 2) ||
        (SizeCells == 0))
    {
      DEBUG ((DEBUG_ERROR, "%a: Bad cell values, %d, %d\r\n", __FUNCTION__, AddressCells, SizeCells));
      return EFI_UNSUPPORTED;
    }

    RegProperty = fdt_getprop (
                    DeviceTreeBase,
                    ParentOffset,
                    "reg",
                    &PropertySize
                    );
    if ((RegProperty == NULL) || (PropertySize == 0)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Invalid reg entry %p, %d, for handle %u\r\n",
        __FUNCTION__,
        RegProperty,
        PropertySize,
        Handle
        ));
    } else {
      EntrySize = sizeof (UINT32) * (AddressCells + SizeCells);
      ASSERT ((PropertySize % EntrySize) == 0);
      if (PropertySize != EntrySize) {
        DEBUG ((DEBUG_ERROR, "%a: Ignoring secondary parent regions\r\n", __FUNCTION__));
      }

      CopyMem ((VOID *)&ParentAddressBase, RegProperty, AddressCells * sizeof (UINT32));
      if (AddressCells == 2) {
        ParentAddressBase = SwapBytes64 (ParentAddressBase);
      } else {
        ParentAddressBase = SwapBytes32 (ParentAddressBase);
      }
    }

    AddressCells = fdt_address_cells (DeviceTreeBase, ParentOffset);
    SizeCells    = fdt_size_cells (DeviceTreeBase, ParentOffset);

    if ((AddressCells > 2) ||
        (AddressCells == 0) ||
        (SizeCells > 2) ||
        (SizeCells == 0))
    {
      DEBUG ((DEBUG_ERROR, "%a: Bad cell values, %d, %d\r\n", __FUNCTION__, AddressCells, SizeCells));
      return EFI_UNSUPPORTED;
    }

    RegProperty = fdt_getprop (
                    DeviceTreeBase,
                    SharedMemOffset,
                    "reg",
                    &PropertySize
                    );
    if ((RegProperty == NULL) || (PropertySize == 0)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Invalid reg entry %p, %d, for handle %u\r\n",
        __FUNCTION__,
        RegProperty,
        PropertySize,
        Handle
        ));
      FreePool (AllocResources);
      *Resources = NULL;
      return EFI_DEVICE_ERROR;
    }

    EntrySize = sizeof (UINT32) * (AddressCells + SizeCells);
    ASSERT ((PropertySize % EntrySize) == 0);
    if (PropertySize != EntrySize) {
      DEBUG ((DEBUG_ERROR, "%a: Ignoring secondary smem regions\r\n", __FUNCTION__));
    }

    CopyMem ((VOID *)&AddressBase, RegProperty, AddressCells * sizeof (UINT32));
    CopyMem ((VOID *)&RegionSize, RegProperty + (AddressCells * sizeof (UINT32)), SizeCells * sizeof (UINT32));
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

    AddressBase += ParentAddressBase;

    Desc = &AllocResources[RegionIndex];
    RegionIndex++;
    Desc->Desc                  = ACPI_ADDRESS_SPACE_DESCRIPTOR;
    Desc->Len                   = sizeof (*Desc) - 3;
    Desc->AddrRangeMin          = AddressBase;
    Desc->AddrLen               = RegionSize;
    Desc->AddrRangeMax          = AddressBase + RegionSize - 1;
    Desc->ResType               = ACPI_ADDRESS_SPACE_TYPE_MEM;
    Desc->AddrSpaceGranularity  = ((EFI_PHYSICAL_ADDRESS)AddressBase + RegionSize > SIZE_4GB) ? 64 : 32;
    Desc->AddrTranslationOffset = 0;

    Status = AddMemoryRegion (AddressBase, RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to add region 0x%016lx, 0x%016lx: %r.\r\n",
        __FUNCTION__,
        AddressBase,
        RegionSize,
        Status
        ));
      FreePool (AllocResources);
      *Resources = NULL;
      return EFI_DEVICE_ERROR;
    }
  }

  End           = (EFI_ACPI_END_TAG_DESCRIPTOR *)&AllocResources[RegionIndex];
  End->Desc     = ACPI_END_TAG_DESCRIPTOR;
  End->Checksum = 0;

  return EFI_SUCCESS;
}

/**
  This function processes a c2c command.

  @param[in]     BpmpIpcProtocol     The instance of the NVIDIA_BPMP_IPC_PROTOCOL.
  @param[in]     Partitions          partitions to process
  @param[in]     Command             c2c command
  @param[out]    Response            c2c response
  @param[in]     ResponseSize        c2c response size

  @return EFI_SUCCESS                c2c initialized
  @return EFI_DEVICE_ERROR           Failed to initialized c2c
**/
EFI_STATUS
BpmpProcessC2cCommand (
  IN  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol,
  IN  UINT32                    BpmpPhandle,
  IN  MRQ_C2C_COMMAND_PACKET    *Request,
  OUT VOID                      *Response,
  IN  UINTN                     ResponseSize
  )
{
  EFI_STATUS  Status;

  if ((Request->Partitions == CmdC2cPartitionNone) ||
      (Request->Partitions >= CmdC2cPartitionMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (BpmpIpcProtocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BpmpIpcProtocol->Communicate (
                              BpmpIpcProtocol,
                              NULL,
                              BpmpPhandle,
                              MRQ_C2C,
                              (VOID *)Request,
                              sizeof (MRQ_C2C_COMMAND_PACKET),
                              Response,
                              ResponseSize,
                              NULL
                              );
  if (Status == EFI_UNSUPPORTED) {
    Status = EFI_SUCCESS;
  } else if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}

/**
  This function processes a power gate command.

  @param[in]     BpmpIpcProtocol     The instance of the NVIDIA_BPMP_IPC_PROTOCOL.
  @param[in]     PgId                power gate id to process
  @param[in]     Command             powergate command
  @param[out]    Response            powergate response
  @param[in]     ResponseSize        powergate response size

  @return EFI_SUCCESS                powergate asserted/deasserted.
  @return EFI_DEVICE_ERROR           Failed to assert/deassert powergate id
**/
EFI_STATUS
BpmpProcessPgCommand (
  IN  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol,
  IN  UINT32                    BpmpPhandle,
  IN  MRQ_PG_COMMAND_PACKET     *Request,
  OUT VOID                      *Response,
  IN  UINTN                     ResponseSize
  )
{
  EFI_STATUS  Status;

  if (Request->PgId == MAX_UINT32) {
    return EFI_SUCCESS;
  }

  if (BpmpIpcProtocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BpmpIpcProtocol->Communicate (
                              BpmpIpcProtocol,
                              NULL,
                              BpmpPhandle,
                              MRQ_PG,
                              (VOID *)Request,
                              sizeof (MRQ_PG_COMMAND_PACKET),
                              Response,
                              ResponseSize,
                              NULL
                              );
  if (Status == EFI_UNSUPPORTED) {
    Status = EFI_SUCCESS;
  } else if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}

/**
  This function processes a reset command.

  @param[in]     BpmpIpcProtocol     The instance of the NVIDIA_BPMP_IPC_PROTOCOL.
  @param[in]     ResetId             Reset to process
  @param[in]     Command             Reset command

  @return EFI_SUCCESS                All resets deasserted.
  @return EFI_DEVICE_ERROR           Failed to deassert all resets
**/
EFI_STATUS
BpmpProcessResetCommand (
  IN NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol,
  IN UINT32                    BpmpPhandle,
  IN UINT32                    ResetId,
  IN MRQ_RESET_COMMANDS        Command
  )
{
  EFI_STATUS  Status;
  UINT32      Request[2];

  if (BpmpIpcProtocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Request[0] = (UINT32)Command;
  Request[1] = ResetId;

  Status = BpmpIpcProtocol->Communicate (
                              BpmpIpcProtocol,
                              NULL,
                              BpmpPhandle,
                              MRQ_RESET,
                              (VOID *)&Request,
                              sizeof (Request),
                              NULL,
                              0,
                              NULL
                              );
  if (Status == EFI_UNSUPPORTED) {
    Status = EFI_SUCCESS;
  } else if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}

/**
  This function allows for deassert of all reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.

  @return EFI_SUCCESS                All resets deasserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to deassert all resets
**/
EFI_STATUS
DeassertAllResetNodes (
  IN  NVIDIA_RESET_NODE_PROTOCOL  *This
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol = NULL;
  EFI_STATUS                Status;
  UINTN                     Index;

  if (This->Resets == 0) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  for (Index = 0; Index < This->Resets; Index++) {
    Status = BpmpProcessResetCommand (BpmpIpcProtocol, This->BpmpPhandle, This->ResetEntries[Index].ResetId, CmdResetDeassert);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
  }

  return EFI_SUCCESS;
}

/**
  This function allows for assert of all reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.

  @return EFI_SUCCESS                All resets deasserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to de-assert all resets
**/
EFI_STATUS
AssertAllResetNodes (
  IN  NVIDIA_RESET_NODE_PROTOCOL  *This
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol = NULL;
  EFI_STATUS                Status;
  UINTN                     Index;

  if (This->Resets == 0) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  for (Index = 0; Index < This->Resets; Index++) {
    Status = BpmpProcessResetCommand (BpmpIpcProtocol, This->BpmpPhandle, This->ResetEntries[Index].ResetId, CmdResetAssert);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
  }

  return EFI_SUCCESS;
}

/**
  This function allows for module reset of all reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.

  @return EFI_SUCCESS                All resets deasserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to reset all modules
**/
EFI_STATUS
ModuleResetAllResetNodes (
  IN  NVIDIA_RESET_NODE_PROTOCOL  *This
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol = NULL;
  EFI_STATUS                Status;
  UINTN                     Index;

  if (This->Resets == 0) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  for (Index = 0; Index < This->Resets; Index++) {
    Status = BpmpProcessResetCommand (BpmpIpcProtocol, This->BpmpPhandle, This->ResetEntries[Index].ResetId, CmdResetModule);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
  }

  return EFI_SUCCESS;
}

/**
  This function allows for deassert of specified reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.
  @param[in]     ResetId             Id to de-assert

  @return EFI_SUCCESS                Resets deasserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to deassert resets
**/
EFI_STATUS
DeassertResetNodes (
  IN  NVIDIA_RESET_NODE_PROTOCOL  *This,
  IN  UINT32                      ResetId
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol = NULL;
  EFI_STATUS                Status;

  if (This->Resets == 0) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  return BpmpProcessResetCommand (BpmpIpcProtocol, This->BpmpPhandle, ResetId, CmdResetDeassert);
}

/**
  This function allows for assert of specified reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.
  @param[in]     ResetId             Id to assert

  @return EFI_SUCCESS                Resets asserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to assert resets
**/
EFI_STATUS
AssertResetNodes (
  IN  NVIDIA_RESET_NODE_PROTOCOL  *This,
  IN  UINT32                      ResetId
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol = NULL;
  EFI_STATUS                Status;

  if (This->Resets == 0) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  return BpmpProcessResetCommand (BpmpIpcProtocol, This->BpmpPhandle, ResetId, CmdResetAssert);
}

/**
  This function allows for module reset of specified reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.
  @param[in]     ResetId             Id to reset

  @return EFI_SUCCESS                Resets asserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to reset module
**/
EFI_STATUS
ModuleResetNodes (
  IN  NVIDIA_RESET_NODE_PROTOCOL  *This,
  IN  UINT32                      ResetId
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol = NULL;
  EFI_STATUS                Status;

  if (This->Resets == 0) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  return BpmpProcessResetCommand (BpmpIpcProtocol, This->BpmpPhandle, ResetId, CmdResetModule);
}

/**
  Function builds the reset node protocol if supported but device tree.

  @param[in]  Node                  Pointer to the device tree node
  @param[out] ClockNodeProtocol     Pointer to where to store the guid for the reset node protocol
  @param[out] ClockNodeInterface    Pointer to the reset node interface
  @param[out] ProtocolListSize      Number of entries in the protocol lists

  @return EFI_SUCCESS               Driver handles this node, protocols installed.
  @return EFI_UNSUPPORTED           Driver does not support this node.
  @return others                    Error occured during setup.

**/
VOID
GetResetNodeProtocol (
  IN  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node,
  OUT EFI_GUID                          **ResetNodeProtocol,
  OUT VOID                              **ResetNodeInterface,
  IN  UINTN                             ProtocolListSize
  )
{
  CONST CHAR8                 *ResetNames = NULL;
  INT32                       ResetNamesLength;
  CONST UINT32                *ResetIds = NULL;
  INT32                       ResetsLength;
  UINTN                       NumberOfResets;
  NVIDIA_RESET_NODE_PROTOCOL  *ResetNode = NULL;
  UINTN                       Index;
  UINTN                       ListEntry;

  if ((NULL == Node) ||
      (NULL == ResetNodeProtocol) ||
      (NULL == ResetNodeInterface))
  {
    return;
  }

  for (ListEntry = 0; ListEntry < ProtocolListSize; ListEntry++) {
    if (ResetNodeProtocol[ListEntry] == NULL) {
      break;
    }
  }

  if (ListEntry == ProtocolListSize) {
    return;
  }

  ResetIds = (CONST UINT32 *)fdt_getprop (Node->DeviceTreeBase, Node->NodeOffset, "resets", &ResetsLength);

  if ((ResetIds == 0) ||
      (ResetsLength == 0))
  {
    NumberOfResets = 0;
  } else {
    if ((ResetsLength % (sizeof (UINT32) * 2)) != 0) {
      DEBUG ((DEBUG_ERROR, "%a, Resets length unexpected %d\r\n", __FUNCTION__, ResetsLength));
      return;
    }

    NumberOfResets = ResetsLength / (sizeof (UINT32) * 2);
  }

  ResetNode = (NVIDIA_RESET_NODE_PROTOCOL *)AllocateZeroPool (sizeof (NVIDIA_RESET_NODE_PROTOCOL) + (NumberOfResets * sizeof (NVIDIA_RESET_NODE_ENTRY)));
  if (NULL == ResetNode) {
    DEBUG ((DEBUG_ERROR, "%a, Failed to allocate reset node\r\n", __FUNCTION__));
    return;
  }

  ResetNode->DeassertAll    = DeassertAllResetNodes;
  ResetNode->AssertAll      = AssertAllResetNodes;
  ResetNode->ModuleResetAll = ModuleResetAllResetNodes;
  ResetNode->Deassert       = DeassertResetNodes;
  ResetNode->Assert         = AssertResetNodes;
  ResetNode->ModuleReset    = ModuleResetNodes;
  ResetNode->Resets         = NumberOfResets;
  ResetNames                = (CONST CHAR8 *)fdt_getprop (Node->DeviceTreeBase, Node->NodeOffset, "reset-names", &ResetNamesLength);
  if (ResetNamesLength == 0) {
    ResetNames = NULL;
  }

  if (NumberOfResets > 0) {
    ResetNode->BpmpPhandle =  SwapBytes32 (ResetIds[0]);
  }

  for (Index = 0; Index < NumberOfResets; Index++) {
    ResetNode->ResetEntries[Index].ResetId   = SwapBytes32 (ResetIds[2 * Index + 1]);
    ResetNode->ResetEntries[Index].ResetName = NULL;
    if (ResetNames != NULL) {
      INT32  Size = AsciiStrSize (ResetNames);
      if ((Size <= 0) || (Size > ResetNamesLength)) {
        ResetNames = NULL;
        continue;
      }

      ResetNode->ResetEntries[Index].ResetName = ResetNames;
      ResetNames                              += Size;
      ResetNamesLength                        -= Size;
    }
  }

  ResetNodeInterface[ListEntry] = (VOID *)ResetNode;
  ResetNodeProtocol[ListEntry]  = &gNVIDIAResetNodeProtocolGuid;
}

/**
  This function allows for simple enablement of all clock nodes.

  @param[in]     This                The instance of the NVIDIA_CLOCK_NODE_PROTOCOL.

  @return EFI_SUCCESS                All clocks enabled.
  @return EFI_NOT_READY              Clock control protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to enable all clocks
**/
EFI_STATUS
EnableAllClockNodes (
  IN  NVIDIA_CLOCK_NODE_PROTOCOL  *This
  )
{
  SCMI_CLOCK2_PROTOCOL  *ClockProtocol = NULL;
  EFI_STATUS            Status;
  UINTN                 Index;
  UINT32                ClockId;
  BOOLEAN               ClockStatus;
  CHAR8                 ClockName[SCMI_MAX_STR_LEN];

  if (This->Clocks == 0) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gArmScmiClock2ProtocolGuid, NULL, (VOID **)&ClockProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  for (Index = 0; Index < This->Clocks; Index++) {
    ClockId = This->ClockEntries[This->Clocks - Index - 1].ClockId;
    Status  = ClockProtocol->GetClockAttributes (ClockProtocol, ClockId, &ClockStatus, ClockName);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

    if (!ClockStatus) {
      Status = ClockProtocol->Enable (ClockProtocol, ClockId, TRUE);
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  This function allows for simple disablement of all clock nodes.

  @param[in]     This                The instance of the NVIDIA_CLOCK_NODE_PROTOCOL.

  @return EFI_SUCCESS                All clocks disabled.
  @return EFI_NOT_READY              Clock control protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to disable all clocks
**/
EFI_STATUS
DisableAllClockNodes (
  IN  NVIDIA_CLOCK_NODE_PROTOCOL  *This
  )
{
  SCMI_CLOCK2_PROTOCOL  *ClockProtocol = NULL;
  EFI_STATUS            Status;
  UINTN                 Index;
  UINT32                ClockId;
  BOOLEAN               ClockStatus;
  CHAR8                 ClockName[SCMI_MAX_STR_LEN];

  if (This->Clocks == 0) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gArmScmiClock2ProtocolGuid, NULL, (VOID **)&ClockProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  for (Index = 0; Index < This->Clocks; Index++) {
    ClockId = This->ClockEntries[This->Clocks - Index - 1].ClockId;
    Status  = ClockProtocol->GetClockAttributes (ClockProtocol, ClockId, &ClockStatus, ClockName);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

    if (ClockStatus) {
      Status = ClockProtocol->Enable (ClockProtocol, ClockId, FALSE);
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Function builds the clock node protocol if supported but device tree.

  @param[in]  Node                  Pointer to the device tree node
  @param[out] ClockNodeProtocol     Pointer to where to store the guid fro clock node protocol
  @param[out] ClockNodeInterface    Pointer to the clock node interface
  @param[out] ProtocolListSize      Number of entries in the protocol lists

  @return EFI_SUCCESS               Driver handles this node, protocols installed.
  @return EFI_UNSUPPORTED           Driver does not support this node.
  @return others                    Error occured during setup.

**/
VOID
GetClockNodeProtocol (
  IN  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node,
  OUT EFI_GUID                          **ClockNodeProtocol,
  OUT VOID                              **ClockNodeInterface,
  IN  UINTN                             ProtocolListSize
  )
{
  CONST CHAR8                 *ClockNames       = NULL;
  CONST CHAR8                 *ClockParentNames = NULL;
  CONST UINT32                *ClockIds         = NULL;
  INT32                       ClocksLength;
  INT32                       ClockNamesLength;
  INT32                       ClockParentsLength;
  UINTN                       NumberOfClocks;
  NVIDIA_CLOCK_NODE_PROTOCOL  *ClockNode = NULL;
  UINTN                       Index;
  UINTN                       ListEntry;
  UINT32                      BpmpPhandle;

  if ((NULL == Node) ||
      (NULL == ClockNodeProtocol) ||
      (NULL == ClockNodeInterface))
  {
    return;
  }

  for (ListEntry = 0; ListEntry < ProtocolListSize; ListEntry++) {
    if (ClockNodeProtocol[ListEntry] == NULL) {
      break;
    }
  }

  if (ListEntry == ProtocolListSize) {
    return;
  }

  ClockIds = (CONST UINT32 *)fdt_getprop (Node->DeviceTreeBase, Node->NodeOffset, "clocks", &ClocksLength);

  if ((ClockIds == 0) ||
      (ClocksLength == 0))
  {
    NumberOfClocks = 0;
  } else {
    if ((ClocksLength % (sizeof (UINT32) * 2)) != 0) {
      DEBUG ((DEBUG_ERROR, "%a, Clock length unexpected %d\r\n", __FUNCTION__, ClocksLength));
      return;
    }

    NumberOfClocks = ClocksLength / (sizeof (UINT32) * 2);
  }

  ClockNode = (NVIDIA_CLOCK_NODE_PROTOCOL *)AllocateZeroPool (sizeof (NVIDIA_CLOCK_NODE_PROTOCOL) + (NumberOfClocks * sizeof (NVIDIA_CLOCK_NODE_ENTRY)));
  if (NULL == ClockNode) {
    DEBUG ((DEBUG_ERROR, "%a, Failed to allocate clock node\r\n", __FUNCTION__));
    return;
  }

  ClockNode->EnableAll  = EnableAllClockNodes;
  ClockNode->DisableAll = DisableAllClockNodes;
  ClockNode->Clocks     = NumberOfClocks;
  ClockNames            = (CONST CHAR8 *)fdt_getprop (Node->DeviceTreeBase, Node->NodeOffset, "clock-names", &ClockNamesLength);
  if (ClockNamesLength == 0) {
    ClockNames = NULL;
  }

  ClockParentNames = (CONST CHAR8 *)fdt_getprop (Node->DeviceTreeBase, Node->NodeOffset, "pll_source", &ClockParentsLength);
  if (ClockParentsLength == 0) {
    ClockParentNames = NULL;
  }

  if (NumberOfClocks > 0) {
    BpmpPhandle = SwapBytes32 (ClockIds[0]);
    ASSERT (BpmpPhandle <= MAX_UINT16);
  }

  for (Index = 0; Index < NumberOfClocks; Index++) {
    ClockNode->ClockEntries[Index].ClockId = SwapBytes32 (ClockIds[2 * Index + 1]);
    ASSERT (ClockNode->ClockEntries[Index].ClockId <= MAX_UINT16);
    ClockNode->ClockEntries[Index].ClockId   = ClockNode->ClockEntries[Index].ClockId | (BpmpPhandle << 16);
    ClockNode->ClockEntries[Index].ClockName = NULL;
    ClockNode->ClockEntries[Index].Parent    = FALSE;
    if (ClockNames != NULL) {
      INT32  Size = AsciiStrSize (ClockNames);
      if ((Size <= 0) || (Size > ClockNamesLength)) {
        ClockNames = NULL;
        continue;
      }

      ClockNode->ClockEntries[Index].ClockName = ClockNames;
      ClockNames                              += Size;
      ClockNamesLength                        -= Size;

      if ((ClockNode->ClockEntries[Index].ClockName != NULL) &&
          (ClockParentNames != NULL))
      {
        CONST CHAR8  *ParentScan    = ClockParentNames;
        INT32        ParentScanSize = ClockParentsLength;
        while (ParentScanSize > 0) {
          INT32  ParentSize = AsciiStrSize (ParentScan);
          if ((ParentSize <= 0) || (ParentSize > ParentScanSize)) {
            break;
          }

          if (0 == AsciiStrCmp (ClockNode->ClockEntries[Index].ClockName, ParentScan)) {
            ClockNode->ClockEntries[Index].Parent = TRUE;
            break;
          }

          ParentScan     += ParentSize;
          ParentScanSize -= ParentSize;
        }
      }
    }
  }

  ClockNodeInterface[ListEntry] = (VOID *)ClockNode;
  ClockNodeProtocol[ListEntry]  = &gNVIDIAClockNodeProtocolGuid;
}

/**
  This function allows for getting state of specified power gate nodes.

  @param[in]     This                The instance of the NVIDIA_POWER_GATE_NODE_PROTOCOL.
  @param[in]     PgId                Id to get state of
  @param[out]    PowerGateState      State of PgId

  @return EFI_SUCCESS                Pg asserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to get Pg state
**/
EFI_STATUS
GetStatePgNodes (
  IN  NVIDIA_POWER_GATE_NODE_PROTOCOL  *This,
  IN  UINT32                           PgId,
  OUT UINT32                           *PowerGateState
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol = NULL;
  EFI_STATUS                Status;
  MRQ_PG_COMMAND_PACKET     Request;

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  Request.Command  = CmdPgGetState;
  Request.PgId     = PgId;
  Request.Argument = MAX_UINT32;

  return BpmpProcessPgCommand (BpmpIpcProtocol, This->BpmpPhandle, &Request, PowerGateState, 4);
}

/**
  This function allows for deassert of specified power gate nodes.

  @param[in]     This                The instance of the NVIDIA_POWER_GATE_NODE_PROTOCOL.
  @param[in]     PgId                Id to de-assert

  @return EFI_SUCCESS                power gate deasserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to deassert powergate
**/
EFI_STATUS
DeassertPgNodes (
  IN  NVIDIA_POWER_GATE_NODE_PROTOCOL  *This,
  IN  UINT32                           PgId
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol = NULL;
  EFI_STATUS                Status;
  MRQ_PG_COMMAND_PACKET     Request;
  UINT32                    PowerGateState;

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  Status = GetStatePgNodes (This, PgId, &PowerGateState);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if (PowerGateState == CmdPgStateOff) {
    Request.Command  = CmdPgSetState;
    Request.PgId     = PgId;
    Request.Argument = CmdPgStateOn;

    return BpmpProcessPgCommand (BpmpIpcProtocol, This->BpmpPhandle, &Request, NULL, 0);
  }

  return EFI_SUCCESS;
}

/**
  This function allows for assert of specified power gate nodes.

  @param[in]     This                The instance of the NVIDIA_POWER_GATE_NODE_PROTOCOL.
  @param[in]     PgId                Id to assert

  @return EFI_SUCCESS                Pg asserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to assert Pg
**/
EFI_STATUS
AssertPgNodes (
  IN  NVIDIA_POWER_GATE_NODE_PROTOCOL  *This,
  IN  UINT32                           PgId
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol = NULL;
  EFI_STATUS                Status;
  MRQ_PG_COMMAND_PACKET     Request;
  UINT32                    PowerGateState;

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  Status = GetStatePgNodes (This, PgId, &PowerGateState);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if (PowerGateState == CmdPgStateOn) {
    Request.Command  = CmdPgSetState;
    Request.PgId     = PgId;
    Request.Argument = CmdPgStateOff;

    return BpmpProcessPgCommand (BpmpIpcProtocol, This->BpmpPhandle, &Request, NULL, 0);
  }

  return EFI_SUCCESS;
}

/**
  This function allows for initialization of C2C.

  @param[in]     This                The instance of the NVIDIA_C2C_NODE_PROTOCOL.
  @param[in]     Partitions          Partitions to be initialized.
  @param[out]    C2cStatus           Status of init.

  @return EFI_SUCCESS                C2C initialized.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to initialize C2C.
**/
EFI_STATUS
InitC2cPartitions (
  IN   NVIDIA_C2C_NODE_PROTOCOL  *This,
  IN   UINT8                     Partitions,
  OUT  UINT8                     *C2cStatus
  )
{
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol = NULL;
  EFI_STATUS                Status;
  MRQ_C2C_COMMAND_PACKET    Request;

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  Request.Command    = CmdC2cStartInitialization;
  Request.Partitions = Partitions;

  return BpmpProcessC2cCommand (BpmpIpcProtocol, This->BpmpPhandle, &Request, C2cStatus, sizeof (UINT8));
}

/**
  Function builds the C2C node protocol if supported by device tree.

  @param[in]  Node                  Pointer to the device tree node
  @param[out] C2cNodeProtocol       Pointer to where to store the guid for c2c node protocol
  @param[out] C2cNodeInterface      Pointer to the c2c node interface
  @param[out] ProtocolListSize      Number of entries in the protocol lists

  @return EFI_SUCCESS               Driver handles this node, protocols installed.
  @return EFI_UNSUPPORTED           Driver does not support this node.
  @return others                    Error occured during setup.

**/
VOID
GetC2cNodeProtocol (
  IN  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node,
  OUT EFI_GUID                          **C2cNodeProtocol,
  OUT VOID                              **C2cNodeInterface,
  IN  UINTN                             ProtocolListSize
  )
{
  NVIDIA_C2C_NODE_PROTOCOL  *C2c = NULL;
  UINTN                     ListEntry;
  CONST UINT32              *Partitions = NULL;
  INT32                     PartitionsLength;

  if ((NULL == Node) ||
      (NULL == C2cNodeProtocol) ||
      (NULL == C2cNodeInterface))
  {
    return;
  }

  for (ListEntry = 0; ListEntry < ProtocolListSize; ListEntry++) {
    if (C2cNodeProtocol[ListEntry] == NULL) {
      break;
    }
  }

  if (ListEntry == ProtocolListSize) {
    return;
  }

  Partitions = (CONST UINT32 *)fdt_getprop (Node->DeviceTreeBase, Node->NodeOffset, "c2c-partitions", &PartitionsLength);

  if (Partitions == NULL) {
    return;
  }

  if (PartitionsLength != (sizeof (UINT32) * 2)) {
    DEBUG ((DEBUG_ERROR, "%a, C2C partitions length unexpected %d\r\n", __FUNCTION__, PartitionsLength));
    return;
  }

  C2c = (NVIDIA_C2C_NODE_PROTOCOL *)AllocateZeroPool (sizeof (NVIDIA_POWER_GATE_NODE_PROTOCOL));
  if (NULL == C2c) {
    DEBUG ((DEBUG_ERROR, "%a, Failed to allocate c2c node\r\n", __FUNCTION__));
    return;
  }

  C2c->Init        = InitC2cPartitions;
  C2c->BpmpPhandle = SwapBytes32 (Partitions[0]);
  C2c->Partitions  = SwapBytes32 (Partitions[1]);

  C2cNodeInterface[ListEntry] = (VOID *)C2c;
  C2cNodeProtocol[ListEntry]  = &gNVIDIAC2cNodeProtocolGuid;
}

/**
  Function builds the PowerGate node protocol if supported by device tree.

  @param[in]  Node                  Pointer to the device tree node
  @param[out] PowerGateNodeProtocol Pointer to where to store the guid for power gate node protocol
  @param[out] PowerGateNodeInterface    Pointer to the power gate node interface
  @param[out] ProtocolListSize      Number of entries in the protocol lists

  @return EFI_SUCCESS               Driver handles this node, protocols installed.
  @return EFI_UNSUPPORTED           Driver does not support this node.
  @return others                    Error occured during setup.

**/
VOID
GetPowerGateNodeProtocol (
  IN  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node,
  OUT EFI_GUID                          **PowerGateNodeProtocol,
  OUT VOID                              **PowerGateNodeInterface,
  IN  UINTN                             ProtocolListSize
  )
{
  CONST UINT32                     *PgIds = NULL;
  INT32                            PgLength;
  UINTN                            NumberOfPgs;
  NVIDIA_POWER_GATE_NODE_PROTOCOL  *PgNode = NULL;
  UINTN                            ListEntry;
  UINT32                           Index;

  if ((NULL == Node) ||
      (NULL == PowerGateNodeProtocol) ||
      (NULL == PowerGateNodeInterface))
  {
    return;
  }

  for (ListEntry = 0; ListEntry < ProtocolListSize; ListEntry++) {
    if (PowerGateNodeProtocol[ListEntry] == NULL) {
      break;
    }
  }

  if (ListEntry == ProtocolListSize) {
    return;
  }

  PgIds = (CONST UINT32 *)fdt_getprop (Node->DeviceTreeBase, Node->NodeOffset, "power-domains", &PgLength);

  if (PgIds == NULL) {
    PgLength = 0;
  }

  if ((PgLength % (sizeof (UINT32) * 2)) != 0) {
    DEBUG ((DEBUG_ERROR, "%a, Power Gate length unexpected %d\r\n", __FUNCTION__, PgLength));
    return;
  }

  NumberOfPgs = PgLength / (sizeof (UINT32) * 2);

  PgNode = (NVIDIA_POWER_GATE_NODE_PROTOCOL *)AllocateZeroPool (sizeof (NVIDIA_POWER_GATE_NODE_PROTOCOL) + (NumberOfPgs * sizeof (UINT32)));
  if (NULL == PgNode) {
    DEBUG ((DEBUG_ERROR, "%a, Failed to allocate power gate node\r\n", __FUNCTION__));
    return;
  }

  PgNode->Deassert           = DeassertPgNodes;
  PgNode->Assert             = AssertPgNodes;
  PgNode->GetState           = GetStatePgNodes;
  PgNode->NumberOfPowerGates = NumberOfPgs;
  if (NumberOfPgs > 0) {
    PgNode->BpmpPhandle = SwapBytes32 (PgIds[0]);
  }

  for (Index = 0; Index < PgNode->NumberOfPowerGates; Index++) {
    PgNode->PowerGateId[Index] = SwapBytes32 (PgIds[(Index *2) + 1]);
  }

  PowerGateNodeInterface[ListEntry] = (VOID *)PgNode;
  PowerGateNodeProtocol[ListEntry]  = &gNVIDIAPowerGateNodeProtocolGuid;
}

/**
 * @brief Process a given device node, this creates the memory map for it and registers support protocols.
 *
 * @param DeviceInfo     - Info regarding device tree base address,node offset,
 *                         device type and init function.
 * @param Device         - Device structure that contains memory information
 * @param DriverHandle   - Handle of the driver that is connecting to the device
 * @param DeviceHandle   - Handle of the device that was registered
 * @return EFI_STATUS    - EFI_SUCCESS on success, others for error
 **/
EFI_STATUS
ProcessDeviceTreeNodeWithHandle (
  IN  NVIDIA_DT_NODE_INFO      *DeviceInfo,
  IN  NON_DISCOVERABLE_DEVICE  *Device,
  IN  EFI_HANDLE               DriverHandle,
  IN OUT EFI_HANDLE            *DeviceHandle
  )
{
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  NodeProtocol;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *NodeProtocolCopy = NULL;
  EFI_GUID                          *DeviceProtocolGuid;
  DEVICE_DISCOVERY_DEVICE_PATH      *DevicePath                                     = NULL;
  EFI_GUID                          *ProtocolGuidList[NUMBER_OF_OPTIONAL_PROTOCOLS] = { NULL, NULL, NULL };
  VOID                              *InterfaceList[NUMBER_OF_OPTIONAL_PROTOCOLS]    = { NULL, NULL, NULL };
  UINTN                             ProtocolIndex;

  if ((DeviceInfo == NULL) ||
      (DeviceInfo->DeviceTreeBase == NULL) ||
      (DeviceHandle == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  NodeProtocol.DeviceTreeBase = DeviceInfo->DeviceTreeBase;
  NodeProtocol.NodeOffset     = DeviceInfo->NodeOffset;

  Device->Type       = DeviceInfo->DeviceType;
  Device->Initialize = DeviceInfo->PciIoInitialize;
  Device->Resources  = NULL;

  // Check if DMA is coherent
  if (NULL != fdt_get_property (DeviceInfo->DeviceTreeBase, DeviceInfo->NodeOffset, "dma-coherent", NULL)) {
    Device->DmaType = NonDiscoverableDeviceDmaTypeCoherent;
  } else {
    Device->DmaType = NonDiscoverableDeviceDmaTypeNonCoherent;
  }

  Status = GetResources (DeviceInfo->DeviceTreeBase, DeviceInfo->NodeOffset, &Device->Resources);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get node resources: %r.\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  DevicePath = (DEVICE_DISCOVERY_DEVICE_PATH *)AllocateZeroPool (sizeof (DEVICE_DISCOVERY_DEVICE_PATH));
  if (DevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  // All paths start with vendor device path node.
  DevicePath->MemMap.Vendor.Header.Type    = HARDWARE_DEVICE_PATH;
  DevicePath->MemMap.Vendor.Header.SubType = HW_VENDOR_DP;
  gBS->CopyMem (&DevicePath->MemMap.Vendor.Guid, &gNVIDIAVendorDeviceDiscoveryGuid, sizeof (EFI_GUID));
  SetDevicePathNodeLength (&DevicePath->MemMap.Vendor, sizeof (DevicePath->MemMap.Vendor));

  if (Device->Resources == NULL) {
    DevicePath->Controller.Controller.Header.Type      = HARDWARE_DEVICE_PATH;
    DevicePath->Controller.Controller.Header.SubType   = HW_CONTROLLER_DP;
    DevicePath->Controller.Controller.ControllerNumber = DeviceInfo->NodeOffset;

    SetDevicePathNodeLength (&DevicePath->Controller.Controller, sizeof (DevicePath->Controller.Controller));
    SetDevicePathEndNode (&DevicePath->Controller.End);
  } else {
    // First resource must be MMIO
    if ((Device->Resources->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR) ||
        (Device->Resources->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM))
    {
      DEBUG ((DEBUG_ERROR, "%a: Invalid node resources.\r\n", __FUNCTION__));
      goto ErrorExit;
    } else {
      DevicePath->MemMap.MemMap.Header.Type     = HARDWARE_DEVICE_PATH;
      DevicePath->MemMap.MemMap.Header.SubType  = HW_MEMMAP_DP;
      DevicePath->MemMap.MemMap.MemoryType      = EfiMemoryMappedIO;
      DevicePath->MemMap.MemMap.StartingAddress = Device->Resources->AddrRangeMin;
      DevicePath->MemMap.MemMap.EndingAddress   = Device->Resources->AddrRangeMax;
      SetDevicePathNodeLength (&DevicePath->MemMap.MemMap, sizeof (DevicePath->MemMap.MemMap));

      SetDevicePathEndNode (&DevicePath->MemMap.End);
    }
  }

  GetC2cNodeProtocol (&NodeProtocol, ProtocolGuidList, InterfaceList, NUMBER_OF_OPTIONAL_PROTOCOLS);
  GetPowerGateNodeProtocol (&NodeProtocol, ProtocolGuidList, InterfaceList, NUMBER_OF_OPTIONAL_PROTOCOLS);
  GetClockNodeProtocol (&NodeProtocol, ProtocolGuidList, InterfaceList, NUMBER_OF_OPTIONAL_PROTOCOLS);
  GetResetNodeProtocol (&NodeProtocol, ProtocolGuidList, InterfaceList, NUMBER_OF_OPTIONAL_PROTOCOLS);

  NodeProtocolCopy = AllocateCopyPool (sizeof (NodeProtocol), (VOID *)&NodeProtocol);
  if (NULL == NodeProtocolCopy) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate node protocol.\r\n", __FUNCTION__));
    goto ErrorExit;
  }

  DeviceProtocolGuid = &gNVIDIANonDiscoverableDeviceProtocolGuid;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  DeviceHandle,
                  DeviceProtocolGuid,
                  Device,
                  Device->Type,
                  NULL,
                  &gNVIDIADeviceTreeNodeProtocolGuid,
                  NodeProtocolCopy,
                  &gEfiDevicePathProtocolGuid,
                  DevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get install protocols: %r.\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  for (ProtocolIndex = 0; ProtocolIndex < NUMBER_OF_OPTIONAL_PROTOCOLS; ProtocolIndex++) {
    if (ProtocolGuidList[ProtocolIndex] == NULL) {
      break;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    DeviceHandle,
                    ProtocolGuidList[ProtocolIndex],
                    InterfaceList[ProtocolIndex],
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get install optional protocols: %r.\r\n", __FUNCTION__, Status));
      UINTN  ProtocolUninstallIndex = 0;
      for (ProtocolUninstallIndex = 0; ProtocolUninstallIndex < ProtocolIndex; ProtocolUninstallIndex++) {
        gBS->UninstallMultipleProtocolInterfaces (
               *DeviceHandle,
               ProtocolGuidList[ProtocolUninstallIndex],
               InterfaceList[ProtocolUninstallIndex],
               NULL
               );
      }

      gBS->UninstallMultipleProtocolInterfaces (
             *DeviceHandle,
             DeviceProtocolGuid,
             Device,
             Device->Type,
             NULL,
             &gNVIDIADeviceTreeNodeProtocolGuid,
             NodeProtocolCopy,
             &gEfiDevicePathProtocolGuid,
             DevicePath,
             NULL
             );
      goto ErrorExit;
    }
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (NULL != Device) {
      if (NULL != Device->Resources) {
        FreePool (Device->Resources);
      }
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
 * @brief Get the Next Supported Device Tree Node object
 *
 * @param IsNodeSupported - Function to check if this driver supports a given node
 * @param DeviceInfo      - Info regarding node offset, device type and init function.
 *                          device type and init function.
 * @return EFI_STATUS     - EFI_SUCCESS if node found, EFI_NOT_FOUND for no more remaining, others for error
 **/
EFI_STATUS
GetNextSupportedDeviceTreeNode (
  IN  DEVICE_TREE_NODE_SUPPORTED  IsNodeSupported,
  IN OUT NVIDIA_DT_NODE_INFO      *DeviceInfo
  )
{
  EFI_STATUS  Status;
  CONST VOID  *Property    = NULL;
  INT32       PropertySize = 0;
  VOID        *DTBase;
  UINTN       DTSize;

  if ((DeviceInfo == NULL) ||
      ((DeviceInfo->DeviceTreeBase == NULL) && (DeviceInfo->NodeOffset != 0)))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (DeviceInfo->DeviceTreeBase == NULL) {
    Status = DtPlatformLoadDtb (&DTBase, &DTSize);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

    DeviceInfo->DeviceTreeBase = DTBase;
  }

  while (DeviceInfo->NodeOffset >= 0) {
    DeviceInfo->NodeOffset = fdt_next_node (DeviceInfo->DeviceTreeBase, DeviceInfo->NodeOffset, NULL);
    if (DeviceInfo->NodeOffset < 0) {
      break;
    }

    Status = IsNodeSupported (DeviceInfo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Property = fdt_getprop (
                 DeviceInfo->DeviceTreeBase,
                 DeviceInfo->NodeOffset,
                 "status",
                 &PropertySize
                 );
    if ((Property == NULL) ||
        (AsciiStrCmp (Property, "okay") == 0))
    {
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
 * @brief Get all Supported Device Tree Node objects
 *
 * @param DeviceTreeBase  - Pointer to the base of the device tree of the system
 * @param IsNodeSupported - Function to check if this driver supports a given node
 * @param DeviceCount     - Number of matching nodes/devices.
 * @param DTNodeInfo      - Device type and offsets of all nodes that was matched.
 * @return EFI_STATUS     - EFI_SUCCESS if node found, EFI_NOT_FOUND for no more remaining, others for error
 **/
EFI_STATUS
GetSupportedDeviceTreeNodes (
  IN VOID                        *DeviceTreeBase,
  IN DEVICE_TREE_NODE_SUPPORTED  IsNodeSupported,
  IN OUT UINT32                  *DeviceCount,
  IN OUT NVIDIA_DT_NODE_INFO     *DTNodeInfo
  )
{
  EFI_STATUS           Status;
  VOID                 *DTBase;
  UINTN                DeviceTreeSize;
  CONST VOID           *Property    = NULL;
  INT32                PropertySize = 0;
  INT32                NodeOffset   = 0;
  UINT32               NodeCount    = 0;
  NVIDIA_DT_NODE_INFO  NodeInfo     = { NULL, 0, NULL, NULL };

  if ((IsNodeSupported == NULL) ||
      (DeviceCount == NULL) ||
      ((*DeviceCount != 0) && (DTNodeInfo == NULL)))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (DeviceTreeBase == NULL) {
    Status = DtPlatformLoadDtb (&DTBase, &DeviceTreeSize);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
  } else {
    DTBase = DeviceTreeBase;
  }

  NodeInfo.DeviceTreeBase = DTBase;
  do {
    NodeOffset = fdt_next_node (DTBase, NodeOffset, NULL);
    if (NodeOffset < 0) {
      break;
    }

    NodeInfo.NodeOffset = NodeOffset;
    Status              = IsNodeSupported (&NodeInfo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Property = fdt_getprop (
                 DTBase,
                 NodeOffset,
                 "status",
                 &PropertySize
                 );
    if ((Property == NULL) ||
        (AsciiStrCmp (Property, "okay") == 0))
    {
      if (NodeCount < *DeviceCount) {
        NodeInfo.Phandle      = fdt_get_phandle (DTBase, NodeOffset);
        DTNodeInfo[NodeCount] = NodeInfo;
      }

      NodeCount++;
    }
  } while (NodeOffset > 0);

  if ((NodeCount > *DeviceCount) && (DTNodeInfo != NULL)) {
    Status = EFI_BUFFER_TOO_SMALL;
  } else if (NodeCount == 0) {
    Status = EFI_NOT_FOUND;
  } else {
    *DeviceCount = NodeCount;
    Status       = EFI_SUCCESS;
  }

  return Status;
}
