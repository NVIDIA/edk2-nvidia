/** @file
  NVIDIA Device Discovery Driver

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
  IN  DEVICE_DISCOVERY_PRIVATE  *Private,
  IN  UINT64                    BaseAddress,
  IN  UINT64                    Size
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
      DEBUG ((EFI_D_ERROR, "%a: Failed to GetMemorySpaceDescriptor (0x%llx): %r.\r\n", __FUNCTION__, ScanLocation, Status));
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
        DEBUG ((EFI_D_ERROR, "%a: Failed to AddMemorySpace: (0x%llx, 0x%llx) %r.\r\n", __FUNCTION__, ScanLocation, OverlapSize, Status));
        return Status;
      }

      Status = gDS->SetMemorySpaceAttributes (
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to SetMemorySpaceAttributes: (0x%llx, 0x%llx) %r.\r\n", __FUNCTION__, ScanLocation, OverlapSize, Status));
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
  IN  DEVICE_DISCOVERY_PRIVATE           *Private,
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

  if ((NULL == Private) ||
      (NULL == Resources))
  {
    return EFI_INVALID_PARAMETER;
  }

  AddressCells = fdt_address_cells (Private->DeviceTreeBase, fdt_parent_offset (Private->DeviceTreeBase, NodeOffset));
  SizeCells    = fdt_size_cells (Private->DeviceTreeBase, fdt_parent_offset (Private->DeviceTreeBase, NodeOffset));

  if ((AddressCells > 2) ||
      (AddressCells == 0) ||
      (SizeCells > 2) ||
      (SizeCells == 0))
  {
    DEBUG ((EFI_D_ERROR, "%a: Bad cell values, %d, %d\r\n", __FUNCTION__, AddressCells, SizeCells));
    return EFI_UNSUPPORTED;
  }

  RegProperty = fdt_getprop (
                  Private->DeviceTreeBase,
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
                        Private->DeviceTreeBase,
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
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate ACPI resources.\r\n", __FUNCTION__));
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

    Status = AddMemoryRegion (Private, AddressBase, RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        EFI_D_ERROR,
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
    INT32   SharedMemOffset = fdt_node_offset_by_phandle (Private->DeviceTreeBase, Handle);
    INT32   ParentOffset;
    UINT64  ParentAddressBase = 0;
    UINT64  AddressBase       = 0;
    UINT64  RegionSize        = 0;

    if (SharedMemOffset <= 0) {
      DEBUG ((
        EFI_D_ERROR,
        "%a: Unable to locate shared memory handle %u\r\n",
        __FUNCTION__,
        Handle
        ));
      FreePool (AllocResources);
      *Resources = NULL;
      return EFI_DEVICE_ERROR;
    }

    ParentOffset = fdt_parent_offset (Private->DeviceTreeBase, SharedMemOffset);
    if (ParentOffset < 0) {
      DEBUG ((
        EFI_D_ERROR,
        "%a: Unable to locate shared memory handle's parent %u\r\n",
        __FUNCTION__,
        Handle
        ));
      FreePool (AllocResources);
      *Resources = NULL;
      return EFI_DEVICE_ERROR;
    }

    AddressCells = fdt_address_cells (Private->DeviceTreeBase, fdt_parent_offset (Private->DeviceTreeBase, NodeOffset));
    SizeCells    = fdt_size_cells (Private->DeviceTreeBase, fdt_parent_offset (Private->DeviceTreeBase, NodeOffset));

    if ((AddressCells > 2) ||
        (AddressCells == 0) ||
        (SizeCells > 2) ||
        (SizeCells == 0))
    {
      DEBUG ((EFI_D_ERROR, "%a: Bad cell values, %d, %d\r\n", __FUNCTION__, AddressCells, SizeCells));
      return EFI_UNSUPPORTED;
    }

    RegProperty = fdt_getprop (
                    Private->DeviceTreeBase,
                    ParentOffset,
                    "reg",
                    &PropertySize
                    );
    if ((RegProperty == NULL) || (PropertySize == 0)) {
      DEBUG ((
        EFI_D_ERROR,
        "%a: Invalid reg entry %p, %u, for handle %u\r\n",
        __FUNCTION__,
        RegProperty,
        PropertySize,
        Handle
        ));
    } else {
      EntrySize = sizeof (UINT32) * (AddressCells + SizeCells);
      ASSERT ((PropertySize % EntrySize) == 0);
      if (PropertySize != EntrySize) {
        DEBUG ((EFI_D_ERROR, "%a: Ignoring secondary parent regions\r\n", __FUNCTION__));
      }

      CopyMem ((VOID *)&ParentAddressBase, RegProperty, AddressCells * sizeof (UINT32));
      if (AddressCells == 2) {
        ParentAddressBase = SwapBytes64 (ParentAddressBase);
      } else {
        ParentAddressBase = SwapBytes32 (ParentAddressBase);
      }
    }

    AddressCells = fdt_address_cells (Private->DeviceTreeBase, ParentOffset);
    SizeCells    = fdt_size_cells (Private->DeviceTreeBase, ParentOffset);

    if ((AddressCells > 2) ||
        (AddressCells == 0) ||
        (SizeCells > 2) ||
        (SizeCells == 0))
    {
      DEBUG ((EFI_D_ERROR, "%a: Bad cell values, %d, %d\r\n", __FUNCTION__, AddressCells, SizeCells));
      return EFI_UNSUPPORTED;
    }

    RegProperty = fdt_getprop (
                    Private->DeviceTreeBase,
                    SharedMemOffset,
                    "reg",
                    &PropertySize
                    );
    if ((RegProperty == NULL) || (PropertySize == 0)) {
      DEBUG ((
        EFI_D_ERROR,
        "%a: Invalid reg entry %p, %u, for handle %u\r\n",
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
      DEBUG ((EFI_D_ERROR, "%a: Ignoring secondary smem regions\r\n", __FUNCTION__));
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

    Status = AddMemoryRegion (Private, AddressBase, RegionSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        EFI_D_ERROR,
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
    Status = BpmpProcessResetCommand (BpmpIpcProtocol, This->ResetEntries[Index].ResetId, CmdResetDeassert);
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
    Status = BpmpProcessResetCommand (BpmpIpcProtocol, This->ResetEntries[Index].ResetId, CmdResetAssert);
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
    Status = BpmpProcessResetCommand (BpmpIpcProtocol, This->ResetEntries[Index].ResetId, CmdResetModule);
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

  return BpmpProcessResetCommand (BpmpIpcProtocol, ResetId, CmdResetDeassert);
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

  return BpmpProcessResetCommand (BpmpIpcProtocol, ResetId, CmdResetAssert);
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

  return BpmpProcessResetCommand (BpmpIpcProtocol, ResetId, CmdResetModule);
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
      DEBUG ((EFI_D_ERROR, "%a, Resets length unexpected %d\r\n", __FUNCTION__, ResetsLength));
      return;
    }

    NumberOfResets = ResetsLength / (sizeof (UINT32) * 2);
  }

  ResetNode = (NVIDIA_RESET_NODE_PROTOCOL *)AllocatePool (sizeof (NVIDIA_RESET_NODE_PROTOCOL) + (NumberOfResets * sizeof (NVIDIA_RESET_NODE_ENTRY)));
  if (NULL == ResetNode) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to allocate reset node\r\n", __FUNCTION__));
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
      DEBUG ((EFI_D_ERROR, "%a, Clock length unexpected %d\r\n", __FUNCTION__, ClocksLength));
      return;
    }

    NumberOfClocks = ClocksLength / (sizeof (UINT32) * 2);
  }

  ClockNode = (NVIDIA_CLOCK_NODE_PROTOCOL *)AllocatePool (sizeof (NVIDIA_CLOCK_NODE_PROTOCOL) + (NumberOfClocks * sizeof (NVIDIA_CLOCK_NODE_ENTRY)));
  if (NULL == ClockNode) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to allocate clock node\r\n", __FUNCTION__));
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

  for (Index = 0; Index < NumberOfClocks; Index++) {
    ClockNode->ClockEntries[Index].ClockId   = SwapBytes32 (ClockIds[2 * Index + 1]);
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

  return BpmpProcessPgCommand (BpmpIpcProtocol, &Request, PowerGateState, 4);
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

    return BpmpProcessPgCommand (BpmpIpcProtocol, &Request, NULL, 0);
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

    return BpmpProcessPgCommand (BpmpIpcProtocol, &Request, NULL, 0);
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

  return BpmpProcessC2cCommand (BpmpIpcProtocol, &Request, C2cStatus, sizeof (UINT8));
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
    DEBUG ((EFI_D_ERROR, "%a, C2C partitions length unexpected %d\r\n", __FUNCTION__, PartitionsLength));
    return;
  }

  C2c = (NVIDIA_C2C_NODE_PROTOCOL *)AllocatePool (sizeof (NVIDIA_POWER_GATE_NODE_PROTOCOL));
  if (NULL == C2c) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to allocate c2c node\r\n", __FUNCTION__));
    return;
  }

  C2c->Init       = InitC2cPartitions;
  C2c->Partitions = SwapBytes32 (Partitions[1]);

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
    DEBUG ((EFI_D_ERROR, "%a, Power Gate length unexpected %d\r\n", __FUNCTION__, PgLength));
    return;
  }

  NumberOfPgs = PgLength / (sizeof (UINT32) * 2);

  PgNode = (NVIDIA_POWER_GATE_NODE_PROTOCOL *)AllocatePool (sizeof (NVIDIA_POWER_GATE_NODE_PROTOCOL) + (NumberOfPgs * sizeof (UINT32)));
  if (NULL == PgNode) {
    DEBUG ((EFI_D_ERROR, "%a, Failed to allocate power gate node\r\n", __FUNCTION__));
    return;
  }

  PgNode->Deassert           = DeassertPgNodes;
  PgNode->Assert             = AssertPgNodes;
  PgNode->GetState           = GetStatePgNodes;
  PgNode->NumberOfPowerGates = NumberOfPgs;
  for (Index = 0; Index < PgNode->NumberOfPowerGates; Index++) {
    PgNode->PowerGateId[Index] = SwapBytes32 (PgIds[(Index *2) + 1]);
  }

  PowerGateNodeInterface[ListEntry] = (VOID *)PgNode;
  PowerGateNodeProtocol[ListEntry]  = &gNVIDIAPowerGateNodeProtocolGuid;
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
ProcessDeviceTreeNodeWithHandle (
  IN  DEVICE_DISCOVERY_PRIVATE  *Private,
  IN  INT32                     NodeOffset,
  IN  EFI_HANDLE                DriverHandle
  )
{
  EFI_STATUS                                 Status;
  NVIDIA_DEVICE_TREE_COMPATIBILITY_PROTOCOL  *CompatibilityProtocol = NULL;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL           NodeProtocol;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL           *NodeProtocolCopy = NULL;
  EFI_GUID                                   *DeviceType       = NULL;
  NON_DISCOVERABLE_DEVICE_INIT               PciIoInitialize;
  NON_DISCOVERABLE_DEVICE                    *Device         = NULL;
  VOID                                       *Interface      = NULL;
  BOOLEAN                                    SupportsBinding = FALSE;
  EFI_GUID                                   *DeviceProtocolGuid;
  EFI_HANDLE                                 DeviceHandle = NULL;
  DEVICE_DISCOVERY_DEVICE_PATH               *DevicePath  = NULL;
  EFI_HANDLE                                 ConnectHandles[2];
  EFI_GUID                                   *ProtocolGuidList[NUMBER_OF_OPTIONAL_PROTOCOLS] = { NULL, NULL, NULL };
  VOID                                       *InterfaceList[NUMBER_OF_OPTIONAL_PROTOCOLS]    = { NULL, NULL, NULL };
  UINTN                                      ProtocolIndex;
  CONST VOID                                 *Property    = NULL;
  INT32                                      PropertySize = 0;

  if (NULL == Private) {
    return EFI_INVALID_PARAMETER;
  }

  NodeProtocol.DeviceTreeBase = Private->DeviceTreeBase;
  NodeProtocol.NodeOffset     = NodeOffset;

  Property = fdt_getprop (
               Private->DeviceTreeBase,
               NodeOffset,
               "status",
               &PropertySize
               );
  if (Property != NULL) {
    if (0 != AsciiStrCmp (Property, "okay")) {
      return EFI_UNSUPPORTED;
    }
  }

  Status = gBS->HandleProtocol (
                  DriverHandle,
                  &gNVIDIADeviceTreeCompatibilityProtocolGuid,
                  (VOID **)&CompatibilityProtocol
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = CompatibilityProtocol->Supported (
                                    CompatibilityProtocol,
                                    &NodeProtocol,
                                    &DeviceType,
                                    &PciIoInitialize
                                    );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Device = (NON_DISCOVERABLE_DEVICE *)AllocatePool (sizeof (NON_DISCOVERABLE_DEVICE));
  if (NULL == Device) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate device protocol.\r\n", __FUNCTION__));
    goto ErrorExit;
  }

  Device->Type       = DeviceType;
  Device->Initialize = PciIoInitialize;
  Device->Resources  = NULL;

  // Check if DMA is coherent
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
    DevicePath->Controller.Controller.ControllerNumber = NodeOffset;

    SetDevicePathNodeLength (&DevicePath->Controller.Controller, sizeof (DevicePath->Controller.Controller));
    SetDevicePathEndNode (&DevicePath->Controller.End);
  } else {
    // First resource must be MMIO
    if ((Device->Resources->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR) ||
        (Device->Resources->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM))
    {
      DEBUG ((EFI_D_ERROR, "%a: Invalid node resources.\r\n", __FUNCTION__));
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
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate node protocol.\r\n", __FUNCTION__));
    goto ErrorExit;
  }

  SupportsBinding = !EFI_ERROR (
                       gBS->HandleProtocol (
                              DriverHandle,
                              &gEfiDriverBindingProtocolGuid,
                              &Interface
                              )
                       );
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

  for (ProtocolIndex = 0; ProtocolIndex < NUMBER_OF_OPTIONAL_PROTOCOLS; ProtocolIndex++) {
    if (ProtocolGuidList[ProtocolIndex] == NULL) {
      break;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &DeviceHandle,
                    ProtocolGuidList[ProtocolIndex],
                    InterfaceList[ProtocolIndex],
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to get install optional protocols: %r.\r\n", __FUNCTION__, Status));
      UINTN  ProtocolUninstallIndex = 0;
      for (ProtocolUninstallIndex = 0; ProtocolUninstallIndex < ProtocolIndex; ProtocolUninstallIndex++) {
        gBS->UninstallMultipleProtocolInterfaces (
               DeviceHandle,
               ProtocolGuidList[ProtocolUninstallIndex],
               InterfaceList[ProtocolUninstallIndex],
               NULL
               );
      }

      gBS->UninstallMultipleProtocolInterfaces (
             DeviceHandle,
             DeviceProtocolGuid,
             Device,
             &gNVIDIADeviceTreeNodeProtocolGuid,
             NodeProtocolCopy,
             &gEfiDevicePathProtocolGuid,
             DevicePath,
             NULL
             );
      goto ErrorExit;
    }
  }

  // Any errors after here should uninstall protocols.
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
CompatibilityProtocolNotification (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  DEVICE_DISCOVERY_PRIVATE  *Private      = (DEVICE_DISCOVERY_PRIVATE *)Context;
  EFI_STATUS                Status        = EFI_BUFFER_TOO_SMALL;
  EFI_HANDLE                *HandleBuffer = NULL;
  UINTN                     Handles       = 0;
  INT32                     NodeOffset    = 0;

  if (Context == NULL) {
    goto ErrorExit;
  }

  Status = gBS->LocateHandleBuffer (
                  ByRegisterNotify,
                  &gNVIDIADeviceTreeCompatibilityProtocolGuid,
                  Private->SearchKey,
                  &Handles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((EFI_D_ERROR, "%a: LocateHandleBuffer returned %r.\r\n", __FUNCTION__, Status));
    }

    return;
  }

  NodeOffset = 0;
  do {
    UINTN  HandleIndex;
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
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  DEVICE_DISCOVERY_PRIVATE  *Private     = NULL;
  BOOLEAN                   EventCreated = FALSE;

  Private = AllocatePool (sizeof (DEVICE_DISCOVERY_PRIVATE));
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
                                         (VOID *)Private,
                                         &Private->SearchKey
                                         );
  if (NULL == Private->ProtocolNotificationEvent) {
    // Non-fatal, event will get processed later
    DEBUG ((EFI_D_ERROR, "%a: Failed create event: %r.\r\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto ErrorExit;
  }

  EventCreated = TRUE;

  // Register protocol to let drivers that do not use driver binding to declare depex.
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
