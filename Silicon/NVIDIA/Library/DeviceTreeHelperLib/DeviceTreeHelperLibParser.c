/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/FdtLib.h>
#include <Library/MemoryAllocationLib.h>
#include "DeviceTreeHelperLibPrivate.h"

#define DEVICE_TREE_MAX_NAME_LENGTH  32

/**
  Returns parent node offset of the device

  @param[in]  DeviceTree   - Devicetree pointer
  @param[in]  NodeOffset   - Offset of the node
  @param[out] ParentOffset - Node Offset of the parent.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - DeviceTree pointer is NULL
  @retval EFI_INVALID_PARAMETER - ParentOffset pointer is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetParent (
  CONST VOID  *DeviceTree,
  INT32       NodeOffset,
  INT32       *ParentOffset
  )
{
  EFI_STATUS  Status;
  INT32       NodeArray[GET_NODE_HIERARCHY_DEPTH_GUESS];
  UINT32      NodeDepth;

  if ((DeviceTree == NULL) || (ParentOffset == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  NodeDepth = 0;
  Status    = GetNodeHierarchyInfo (DeviceTree, NodeOffset, NodeArray, GET_NODE_HIERARCHY_DEPTH_GUESS, &NodeDepth);

  if (Status == EFI_BUFFER_TOO_SMALL) {
    Status = GetNodeHierarchyInfo (DeviceTree, NodeOffset, NodeArray, GET_NODE_HIERARCHY_DEPTH_GUESS, &NodeDepth);
  }

  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  ASSERT (NodeDepth != 0);

  if (NodeDepth == 1) {
    *ParentOffset = 0;
  } else if (NodeDepth > GET_NODE_HIERARCHY_DEPTH_GUESS) {
    // Had to go through BUFFER_TOO_SMALL path
    *ParentOffset = NodeArray[GET_NODE_HIERARCHY_DEPTH_GUESS-2];
  } else {
    *ParentOffset = NodeArray[NodeDepth-2];
  }

  return Status;
}

/**
  Returns information about the registers of a given device tree node

  @param[in]  NodeOffset        - NodeHandle
  @param[out] RegisterArray     - Buffer of size NumberOfRegisters that will contain the list of register information
  @param[in]  NumberOfRegisters - On input contains size of RegisterArray, on output number of required registers.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfRegisters is less than required registers
  @retval EFI_INVALID_PARAMETER - NumberOfRegisters pointer is NULL
  @retval EFI_INVALID_PARAMETER - RegisterArray is NULL when *NumberOfRegisters is not 0
  @retval EFI_NOT_FOUND         - No registers
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetRegisters (
  IN INT32                              NodeOffset,
  OUT NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray OPTIONAL,
  IN OUT UINT32                         *NumberOfRegisters
  )
{
  EFI_STATUS  Status;
  VOID        *DeviceTree;
  INT32       ParentOffset;
  UINT64      AddressCells;
  UINT64      SizeCells;
  CONST VOID  *RegProperty;
  CONST VOID  *RegNames;
  UINT32      PropertySize;
  UINT32      NameSize;
  INT32       NameOffset;
  UINTN       EntrySize;
  UINTN       NumberOfRegRegions;
  UINTN       RegionIndex;

  if ((NumberOfRegisters == NULL) ||
      ((*NumberOfRegisters != 0) && (RegisterArray == NULL)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetParent (DeviceTree, NodeOffset, &ParentOffset);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceTreeGetNodePropertyValue64 (ParentOffset, "#address-cells", &AddressCells);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodePropertyValue64 (ParentOffset, "#size-cells", &SizeCells);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if ((AddressCells > 2) ||
      (AddressCells == 0) ||
      (SizeCells > 2) ||
      (SizeCells == 0))
  {
    DEBUG ((DEBUG_ERROR, "%a: Bad cell values, %llu, %llu\r\n", __FUNCTION__, AddressCells, SizeCells));
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, "reg", &RegProperty, &PropertySize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  EntrySize = sizeof (UINT32) * (AddressCells + SizeCells);
  if ((PropertySize % EntrySize) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Bad DTB \"reg\" property found at NodeOffset 0x%x (#address-cells = %lu, #size-cells = %lu, entry size = 0x%lx, total size = 0x%x)\n", __FUNCTION__, NodeOffset, AddressCells, SizeCells, EntrySize, PropertySize));
    return EFI_DEVICE_ERROR;
  }

  NumberOfRegRegions = PropertySize / EntrySize;

  if (NumberOfRegRegions > *NumberOfRegisters) {
    *NumberOfRegisters = NumberOfRegRegions;
    return EFI_BUFFER_TOO_SMALL;
  } else if (NumberOfRegRegions == 0) {
    return EFI_NOT_FOUND;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, "reg-names", &RegNames, &NameSize);
  if (Status == EFI_NOT_FOUND) {
    NameSize = 0;
  } else if (EFI_ERROR (Status)) {
    return Status;
  }

  NameOffset = 0;

  for (RegionIndex = 0; RegionIndex < NumberOfRegRegions; RegionIndex++) {
    UINT64  AddressBase = 0;
    UINT64  RegionSize  = 0;

    CopyMem ((VOID *)&AddressBase, RegProperty + EntrySize * RegionIndex, AddressCells * sizeof (UINT32));
    CopyMem ((VOID *)&RegionSize, RegProperty + EntrySize * RegionIndex + (AddressCells * sizeof (UINT32)), SizeCells * sizeof (UINT32));
    if (AddressCells == 2) {
      AddressBase = Fdt64ToCpu (AddressBase);
    } else {
      AddressBase = Fdt32ToCpu (AddressBase);
    }

    if (SizeCells == 2) {
      RegionSize = Fdt64ToCpu (RegionSize);
    } else {
      RegionSize = Fdt32ToCpu (RegionSize);
    }

    RegisterArray[RegionIndex].BaseAddress = AddressBase;
    RegisterArray[RegionIndex].Size        = RegionSize;
    RegisterArray[RegionIndex].Name        = NULL;

    if (NameOffset < NameSize) {
      RegisterArray[RegionIndex].Name = RegNames + NameOffset;
      NameOffset                     += AsciiStrSize (RegisterArray[RegionIndex].Name);
    }
  }

  *NumberOfRegisters = NumberOfRegRegions;
  return EFI_SUCCESS;
}

/**
Returns information about the ranges of a given device tree node

@param[in]      NodeOffset      - NodeHandle
@param[in]      RangeName       - Name of the ranges property ("ranges", "dma-ranges", "hbm-ranges", etc)
@param[out]     RangesArray     - Buffer of size NumberOfRanges that will contain the list of ranges information
@param[in,out]  NumberOfRanges  - On input contains size of RangesArray, on output number of required ranges.

@retval EFI_SUCCESS           - Operation successful
@retval EFI_BUFFER_TOO_SMALL  - NumberOfRanges is less than required ranges
@retval EFI_INVALID_PARAMETER - NumberOfRanges pointer is NULL
@retval EFI_INVALID_PARAMETER - RangesArray is NULL when *NumberOfRanges is not 0
@retval EFI_NOT_FOUND         - No ranges
@retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetRanges (
  IN INT32                            NodeOffset,
  IN CONST CHAR8                      *RangeName,
  OUT NVIDIA_DEVICE_TREE_RANGES_DATA  *RangesArray OPTIONAL,
  IN OUT UINT32                       *NumberOfRanges
  )
{
  EFI_STATUS  Status;
  VOID        *DeviceTree;
  INT32       ParentOffset;
  UINT64      ChildAddressCells;
  UINT64      ParentAddressCells;
  UINT64      SizeCells;
  CONST VOID  *RangeProperty;
  CONST VOID  *RangeNames;
  UINT32      PropertySize;
  UINT32      NameSize;
  INT32       NameOffset;
  UINTN       EntrySize;
  UINTN       NumberOfRangeRegions;
  UINTN       RegionIndex;
  CHAR8       NamePropertyString[DEVICE_TREE_MAX_NAME_LENGTH];

  if ((RangeName == NULL) ||
      (NumberOfRanges == NULL) ||
      ((*NumberOfRanges != 0) && (RangesArray == NULL)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, RangeName, &RangeProperty, &PropertySize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceTreeGetParent (DeviceTree, NodeOffset, &ParentOffset);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceTreeGetNodePropertyValue64 (NodeOffset, "#address-cells", &ChildAddressCells);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodePropertyValue64 (ParentOffset, "#address-cells", &ParentAddressCells);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodePropertyValue64 (NodeOffset, "#size-cells", &SizeCells);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if ((ParentAddressCells > 2) ||
      (ParentAddressCells == 0) ||
      (ChildAddressCells > 3) ||
      (ChildAddressCells == 0) ||
      (SizeCells > 2) ||
      (SizeCells == 0))
  {
    DEBUG ((DEBUG_ERROR, "%a: Bad cell values, %llu, %llu, %llu\r\n", __FUNCTION__, ChildAddressCells, ParentAddressCells, SizeCells));
    return EFI_DEVICE_ERROR;
  }

  // "hbm-ranges" doesn't have the ChildAddress field
  if (AsciiStrCmp (RangeName, "hbm-ranges") == 0) {
    ChildAddressCells = 0;
  }

  EntrySize = sizeof (UINT32) * (ChildAddressCells + ParentAddressCells + SizeCells);
  ASSERT ((PropertySize % EntrySize) == 0);
  NumberOfRangeRegions = PropertySize / EntrySize;

  if (NumberOfRangeRegions > *NumberOfRanges) {
    *NumberOfRanges = NumberOfRangeRegions;
    return EFI_BUFFER_TOO_SMALL;
  } else if (NumberOfRangeRegions == 0) {
    return EFI_NOT_FOUND;
  }

  Status = AsciiStrCpyS (NamePropertyString, sizeof (NamePropertyString), RangeName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AsciiStrCatS (NamePropertyString, sizeof (NamePropertyString), "-names");
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, NamePropertyString, &RangeNames, &NameSize);
  if (Status == EFI_NOT_FOUND) {
    NameSize = 0;
  } else if (EFI_ERROR (Status)) {
    return Status;
  }

  NameOffset = 0;

  for (RegionIndex = 0; RegionIndex < NumberOfRangeRegions; RegionIndex++) {
    UINT64  ChildAddressBase  = 0;
    UINT64  ParentAddressBase = 0;
    UINT64  RegionSize        = 0;
    UINT32  ChildAddressHigh  = 0;

    if (ChildAddressCells == 3) {
      CopyMem (
        (VOID *)&ChildAddressHigh,
        RangeProperty + EntrySize * RegionIndex,
        sizeof (UINT32)
        );
      ChildAddressHigh = Fdt32ToCpu (ChildAddressHigh);

      CopyMem (
        (VOID *)&ChildAddressBase,
        RangeProperty + (EntrySize * RegionIndex) + sizeof (UINT32),
        sizeof (UINT64)
        );
      ChildAddressBase = Fdt64ToCpu (ChildAddressBase);
    } else if (ChildAddressCells == 2) {
      CopyMem (
        (VOID *)&ChildAddressBase,
        RangeProperty + (EntrySize * RegionIndex),
        sizeof (UINT64)
        );
      ChildAddressBase = Fdt64ToCpu (ChildAddressBase);
    } else if (ChildAddressCells == 1) {
      CopyMem (
        (VOID *)&ChildAddressBase,
        RangeProperty + (EntrySize * RegionIndex),
        sizeof (UINT32)
        );
      ChildAddressBase = Fdt32ToCpu (ChildAddressBase);
    }

    CopyMem (
      (VOID *)&ParentAddressBase,
      RangeProperty + EntrySize * RegionIndex + (ChildAddressCells * sizeof (UINT32)),
      ParentAddressCells * sizeof (UINT32)
      );
    if (ParentAddressCells == 2) {
      ParentAddressBase = Fdt64ToCpu (ParentAddressBase);
    } else {
      ParentAddressBase = Fdt32ToCpu (ParentAddressBase);
    }

    CopyMem (
      (VOID *)&RegionSize,
      RangeProperty + EntrySize * RegionIndex + ((ChildAddressCells + ParentAddressCells) * sizeof (UINT32)),
      SizeCells * sizeof (UINT32)
      );
    if (SizeCells == 2) {
      RegionSize = Fdt64ToCpu (RegionSize);
    } else {
      RegionSize = Fdt32ToCpu (RegionSize);
    }

    RangesArray[RegionIndex].ChildAddressHigh = ChildAddressHigh;
    RangesArray[RegionIndex].ChildAddress     = ChildAddressBase;
    RangesArray[RegionIndex].ParentAddress    = ParentAddressBase;
    RangesArray[RegionIndex].Size             = RegionSize;
    RangesArray[RegionIndex].Name             = NULL;

    if (NameOffset < NameSize) {
      RangesArray[RegionIndex].Name = RangeNames + NameOffset;
      NameOffset                   += AsciiStrSize (RangesArray[RegionIndex].Name);
    }
  }

  *NumberOfRanges = NumberOfRangeRegions;
  return EFI_SUCCESS;
}

/**
  Gets the offset of the interrupt-parent of the specified node

  This function is currently marked as deprecated in header as it is only used
  internally.

  @param  [in]  DeviceTreeBase   - Base Address of the device tree.
  @param  [in]  NodeOffset       - Offset from DeviceTreeBase to the specified node.
  @param  [out] ParentNodeOffset - The interrupt parent node offset

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - DeviceTreeBase pointer is NULL
  @retval EFI_INVALID_PARAMETER - NodeOffset is 0
  @retval EFI_INVALID_PARAMETER - ParentNodeOffset pointer is NULL

**/
EFI_STATUS
EFIAPI
GetInterruptParentOffset (
  IN CONST VOID  *DeviceTree,
  IN INT32       NodeOffset,
  OUT INT32      *ParentNodeOffset
  )
{
  EFI_STATUS  Status;
  UINT64      ParentPhandle;

  if ((DeviceTree == NULL) || (NodeOffset == 0) || (ParentNodeOffset == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = DeviceTreeGetNodePropertyValue64 (NodeOffset, "interrupt-parent", &ParentPhandle);
  if (!EFI_ERROR (Status)) {
    if (ParentPhandle >= MAX_UINT32) {
      return EFI_DEVICE_ERROR;
    }

    Status = DeviceTreeGetNodeByPHandle (ParentPhandle, ParentNodeOffset);
  } else if (Status == EFI_NOT_FOUND) {
    // Function is not in FdtLib
    Status = DeviceTreeGetParent (DeviceTree, NodeOffset, ParentNodeOffset);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  } else {
    return Status;
  }

  // Verify that this node is an interrupt-controller, and if not, recursively look for its parent
  Status = DeviceTreeGetNodeProperty (*ParentNodeOffset, "interrupt-controller", NULL, NULL);
  if (!EFI_ERROR (Status)) {
    return Status;
  } else {
    return GetInterruptParentOffset (DeviceTree, *ParentNodeOffset, ParentNodeOffset);
  }
}

STATIC
EFI_STATUS
ParseInterruptCells (
  IN CONST UINT32                        *IntProperty,
  IN UINT32                              InterruptCells,
  OUT NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptData
  )
{
  UINT32  CellIndex;

  DEBUG ((DEBUG_VERBOSE, "%a: IntProperty = 0x%p, InterruptCells = %u, InterruptData = 0x%p\n", __FUNCTION__, IntProperty, InterruptCells, InterruptData));
  NV_ASSERT_RETURN (InterruptCells <= 3, return EFI_UNSUPPORTED, "%a: Don't know how to parse interrupts that have more than 3 cells\n", __FUNCTION__);
  NV_ASSERT_RETURN (IntProperty != NULL, return EFI_INVALID_PARAMETER, "%a: IntProperty was NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (InterruptData != NULL, return EFI_INVALID_PARAMETER, "%a: InterruptData was NULL\n", __FUNCTION__);

  CellIndex = 0;
  if (InterruptCells > 2) {
    InterruptData->Type = Fdt32ToCpu (IntProperty[CellIndex++]);
    DEBUG ((DEBUG_INFO, "%a: IntProperty[%u] - Type = %d\n", __FUNCTION__, CellIndex, InterruptData->Type));
  }

  if (InterruptCells > 0) {
    InterruptData->Interrupt = Fdt32ToCpu (IntProperty[CellIndex++]);
    DEBUG ((DEBUG_INFO, "%a: IntProperty[%u] - Interrupt = %u\n", __FUNCTION__, CellIndex, InterruptData->Interrupt));
  }

  if (InterruptCells > 1) {
    InterruptData->Flag = Fdt32ToCpu (IntProperty[CellIndex++]);
    DEBUG ((DEBUG_INFO, "%a: IntProperty[%u] - Flag = %d\n", __FUNCTION__, CellIndex, InterruptData->Flag));
  }

  NV_ASSERT_RETURN (CellIndex == InterruptCells, return EFI_DEVICE_ERROR, "%a: Code bug parsing %u InterruptCells\n", __FUNCTION__, InterruptCells);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
GetPhandleCells (
  IN INT32        Phandle,
  IN CONST CHAR8  *CellsName,
  OUT UINT32      *Cells
  )
{
  EFI_STATUS  Status;
  INT32       NodeOffset;

  NV_ASSERT_RETURN (CellsName != NULL, return EFI_INVALID_PARAMETER, "%a: CellsName pointer can't be NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (Cells != NULL, return EFI_INVALID_PARAMETER, "%a: Cells pointer can't be NULL\n", __FUNCTION__);

  Status = DeviceTreeGetNodeByPHandle (Phandle, &NodeOffset);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, CellsName, Cells);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting %a for NodeOffset 0x%x (rc=%r)\n", __FUNCTION__, CellsName, NodeOffset, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Returns information about the interrupts of a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [out]     InterruptArray     - Buffer of size NumberOfInterrupts that will contain the list of interrupt information
  @param  [in, out] NumberOfInterrupts - On input contains size of InterruptArray, on output number of required entries.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfInterrupts is less than required entries
  @retval EFI_INVALID_PARAMETER - NumberOfInterrupts pointer is NULL
  @retval EFI_INVALID_PARAMETER - InterruptArray is NULL when *NumberOfInterrupts is not 0
  @retval EFI_NOT_FOUND         - No interrupts
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetInterrupts (
  IN INT32                               NodeOffset,
  OUT NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptArray OPTIONAL,
  IN OUT UINT32                          *NumberOfInterrupts
  )
{
  EFI_STATUS    Status;
  VOID          *DeviceTree;
  UINT32        InterruptCells;
  CONST UINT32  *IntProperty;
  CONST VOID    *IntNames;
  UINT32        PropertySize;
  UINT32        IntPropertyEntries;
  UINT32        IntIndex;
  UINT32        NameSize;
  INT32         NameOffset;
  INT32         ParentNodeOffset;
  UINT32        CellIndex;
  UINT32        NumCells;
  BOOLEAN       ExtendedInterrupts;

  if ((NumberOfInterrupts == NULL) ||
      ((*NumberOfInterrupts != 0) && (InterruptArray == NULL)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, "interrupts-extended", (CONST VOID **)&IntProperty, &PropertySize);
  if (!EFI_ERROR (Status)) {
    ExtendedInterrupts = TRUE;
    NumCells           = PropertySize/sizeof (UINT32);

    CellIndex          = 0;
    IntPropertyEntries = 0;
    while (CellIndex < NumCells) {
      Status = GetPhandleCells (Fdt32ToCpu (IntProperty[CellIndex++]), "#interrupt-cells", &InterruptCells);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error getting #interrupt-cells count for interrupt controller (rc=%r)\n", __FUNCTION__, Status));
        return Status;
      }

      ASSERT (InterruptCells > 0);
      if (InterruptCells == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Didn't get a valid #interrupt-cells count for interrupt controller (rc=%r)\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((DEBUG_VERBOSE, "%a: Parent has %u interrupt cells\n", __FUNCTION__, InterruptCells));
      ASSERT (CellIndex + InterruptCells <= NumCells);
      IntPropertyEntries++;
      CellIndex += InterruptCells;
    }

    ASSERT (CellIndex == NumCells);
  } else {
    // Didn't find extended interrupts, so look for normal ones
    ExtendedInterrupts = FALSE;

    Status = DeviceTreeGetNodeProperty (NodeOffset, "interrupts", (CONST VOID **)&IntProperty, &PropertySize);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    NumCells = PropertySize/sizeof (UINT32);
    Status   = GetInterruptParentOffset (DeviceTree, NodeOffset, &ParentNodeOffset);

    if (!EFI_ERROR (Status)) {
      Status = DeviceTreeGetNodePropertyValue32 (ParentNodeOffset, "#interrupt-cells", &InterruptCells);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error getting #interrupt-cells count for interrupt controller 0x%x (rc=%r)\n", __FUNCTION__, ParentNodeOffset, Status));
        return Status;
      }
    } else {
      // Maintain legacy behavior of assuming it's 3 for all interrupts where we haven't determined that it shouldn't be
      DEBUG ((DEBUG_WARN, "%a: Error determining interrupt controller (possible incorrect DeviceTree). Using legacy #interrupt-cells of 3\n", __FUNCTION__));
      InterruptCells = 3;
    }

    ASSERT ((PropertySize % InterruptCells) == 0);
    IntPropertyEntries = PropertySize / (InterruptCells * sizeof (UINT32));
  }

  if (*NumberOfInterrupts < IntPropertyEntries) {
    *NumberOfInterrupts = IntPropertyEntries;
    return EFI_BUFFER_TOO_SMALL;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, "interrupt-names", &IntNames, &NameSize);
  if (Status == EFI_NOT_FOUND) {
    NameSize = 0;
  } else if (EFI_ERROR (Status)) {
    return Status;
  }

  NameOffset = 0;

  CellIndex = 0;
  for (IntIndex = 0; IntIndex < IntPropertyEntries; IntIndex++) {
    ASSERT (CellIndex < NumCells);
    if (ExtendedInterrupts) {
      Status = GetPhandleCells (Fdt32ToCpu (IntProperty[CellIndex++]), "#interrupt-cells", &InterruptCells);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error getting #interrupt-cells count for interrupt controller (rc=%r)\n", __FUNCTION__, Status));
        return Status;
      }

      ASSERT (InterruptCells > 0);
      if (InterruptCells == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Didn't get a valid #interrupt-cells count for interrupt controller (rc=%r)\n", __FUNCTION__, Status));
        return EFI_DEVICE_ERROR;
      }
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "compatible", (CONST VOID **)&InterruptArray[IntIndex].ControllerCompatible, NULL);
    if (EFI_ERROR (Status)) {
      InterruptArray[IntIndex].ControllerCompatible = NULL;
    }

    Status     = ParseInterruptCells (&IntProperty[CellIndex], InterruptCells, &InterruptArray[IntIndex]);
    CellIndex += InterruptCells;
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to parse %u Interrupt Cells for interrupt index %u\n", __FUNCTION__, Status, InterruptCells, IntIndex));
      return Status;
    }

    if (NameOffset < NameSize) {
      InterruptArray[IntIndex].Name = IntNames + NameOffset;
      NameOffset                   += AsciiStrSize (InterruptArray[IntIndex].Name);
    } else {
      InterruptArray[IntIndex].Name = NULL;
    }

    DEBUG ((DEBUG_INFO, "%a: Parent interrupt controller \"%a\"\n", __FUNCTION__, InterruptArray[IntIndex].ControllerCompatible));
  }

  ASSERT (CellIndex == NumCells);

  *NumberOfInterrupts = IntPropertyEntries;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ParseAddressCells (
  IN CONST UINT32           *Property,
  IN UINT32                 NumCells,
  OUT EFI_PHYSICAL_ADDRESS  *AddrHigh,
  OUT EFI_PHYSICAL_ADDRESS  *AddrLow
  )
{
  UINT32  CellOffset;

  DEBUG ((DEBUG_VERBOSE, "%a: Property = 0x%p, NumCells = %u, AddrHigh = 0x%p, AddrLow = 0x%p\n", __FUNCTION__, Property, NumCells, AddrHigh, AddrLow));
  NV_ASSERT_RETURN ((Property != NULL) || (NumCells == 0), return EFI_INVALID_PARAMETER, "%a: Property is NULL when NumCells is %u\n", __FUNCTION__, NumCells);
  NV_ASSERT_RETURN ((AddrHigh != NULL) || (NumCells <= 2), return EFI_INVALID_PARAMETER, "%a: NumCells (%u) > 2 but AddrHigh is NULL\n", __FUNCTION__, NumCells);
  NV_ASSERT_RETURN ((AddrLow != NULL) || (NumCells == 0), return EFI_INVALID_PARAMETER, "%a: NumCells is %u but AddrLow is NULL\n", __FUNCTION__, NumCells);

  if (AddrHigh != NULL) {
    *AddrHigh = 0;
  }

  if (AddrLow != NULL) {
    *AddrLow = 0;
  }

  CellOffset = 0;
  switch (NumCells) {
    default:
      NV_ASSERT_RETURN (NumCells <= 4, return EFI_UNSUPPORTED, "%a: NumCells more than 4 aren't currently supported, but found %u\n", __FUNCTION__, NumCells);
      break;

    case 4:
      *AddrHigh |= (((UINT64)Fdt32ToCpu (Property[CellOffset])) << 32);
      CellOffset++;
    case 3:
      *AddrHigh |= Fdt32ToCpu (Property[CellOffset]);
      CellOffset++;
    case 2:
      *AddrLow |= (((UINT64)Fdt32ToCpu (Property[CellOffset])) << 32);
      CellOffset++;
    case 1:
      *AddrLow |= Fdt32ToCpu (Property[CellOffset]);
      CellOffset++;
    case 0:
      break;
  }

  return EFI_SUCCESS;
}

#define PARENT_PHANDLE_CELLS  1

/**
  Returns information about the interrupt map of a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [out]     InterruptMapArray  - Buffer of size NumberOfMaps that will contain the list of interrupt map information
  @param  [in, out] NumberOfMaps       - On input contains size of InterruptMapArray, on output number of required entries.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfMaps is less than required entries
  @retval EFI_INVALID_PARAMETER - NumberOfMaps pointer is NULL
  @retval EFI_INVALID_PARAMETER - InterruptMapArray is NULL when *NumberOfMaps is not 0
  @retval EFI_NOT_FOUND         - No interrupt maps
  @retval EFI_UNSUPPORTED       - Found unsupported number of cells
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetInterruptMap (
  IN INT32                                   NodeOffset,
  OUT NVIDIA_DEVICE_TREE_INTERRUPT_MAP_DATA  *InterruptMapArray OPTIONAL,
  IN OUT UINT32                              *NumberOfMaps
  )
{
  EFI_STATUS    Status;
  VOID          *DeviceTree;
  CONST UINT32  *MapProperty;
  UINT32        PropertySize;
  UINT32        MapIndex;
  UINT32        CellIndex;
  UINT32        NumCells;
  INT32         ParentNodeOffset;
  INT32         ParentPhandle;
  UINT32        ChildAddressCells;
  UINT32        ChildInterruptCells;
  UINT32        ParentAddressCells;
  UINT32        ParentInterruptCells;
  UINT32        EntryCells;
  INT32         ChildAddressOffset;
  INT32         ChildInterruptOffset;
  INT32         ParentPhandleOffset;
  INT32         ParentAddressOffset;
  INT32         ParentInterruptOffset;
  BOOLEAN       TryZeroAddressCells;

  NV_ASSERT_RETURN (NumberOfMaps != NULL, return EFI_INVALID_PARAMETER, "%a: NumberOfMaps is not allowed to be NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN ((InterruptMapArray != NULL) || (*NumberOfMaps == 0), return EFI_INVALID_PARAMETER, "%a: InterruptMapArray can only be NULL if NumberOfMaps is zero\n", __FUNCTION__);

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get DeviceTreePointer\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, "interrupt-map", (CONST VOID **)&MapProperty, &PropertySize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get interrupt-map property of NodeOffset 0x%x\n", __FUNCTION__, Status, NodeOffset));
    return Status;
  }

  NumCells = PropertySize/sizeof (UINT32);

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "#address-cells", &ChildAddressCells);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get #address-cells for NodeOffset 0x%x\n", __FUNCTION__, Status, NodeOffset));
    return Status;
  }

  ChildAddressOffset = 0;

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "#interrupt-cells", &ChildInterruptCells);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a: Didn't find #interrupt-cells in the node containing #interrupt-cells, which is a DTB bug. Assuming a default of 1\n", __FUNCTION__));
    ChildInterruptCells = 1;
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get #interrupt-cells for NodeOffset 0x%x\n", __FUNCTION__, Status, NodeOffset));
    return Status;
  }

  ChildInterruptOffset = ChildAddressOffset + ChildAddressCells;
  ParentPhandleOffset  = ChildInterruptOffset + ChildInterruptCells;
  DEBUG ((DEBUG_VERBOSE, "%a: ChildAddressCells = %u, ChildInterruptCells = %u, ChildInterruptOffset = %d, ParentPhandleOffset = %d\n", __FUNCTION__, ChildAddressCells, ChildInterruptCells, ChildInterruptOffset, ParentPhandleOffset));

  // Loop through each entry, parsing it
  TryZeroAddressCells = FALSE;
ParseInterruptMapEntries:
  if (TryZeroAddressCells) {
    DEBUG ((DEBUG_ERROR, "%a: DTB might have missing required #address-cells field. Trying to work around it by using zero for the value\n", __FUNCTION__));
  }

  for (MapIndex = 0, CellIndex = 0; CellIndex < NumCells; MapIndex++, CellIndex += EntryCells) {
    DEBUG ((DEBUG_VERBOSE, "%a: MapIndex = %u, CellIndex = %u, NumCells = %u, TryZeroAddressCells = %a\n", __FUNCTION__, MapIndex, CellIndex, NumCells, TryZeroAddressCells ? "true" : "false"));
    NV_ASSERT_RETURN (CellIndex + ParentPhandleOffset < NumCells, return EFI_DEVICE_ERROR, "%a: Cell parsing bug - parent phandle offset exceeds map property size for Node Offset 0x%x, MapIndex %u\n", __FUNCTION__, NodeOffset, MapIndex);
    ParentPhandle = Fdt32ToCpu (MapProperty[CellIndex + ParentPhandleOffset]);
    Status        = DeviceTreeGetNodeByPHandle (ParentPhandle, &ParentNodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get node offset of phandle 0x%x\n", __FUNCTION__, Status, ParentPhandle));
      break;
    }

    Status = DeviceTreeGetNodePropertyValue32 (ParentNodeOffset, "#address-cells", &ParentAddressCells);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get #address-cells for NodeOffset 0x%x\n", __FUNCTION__, Status, ParentNodeOffset));
      break;
    }

    if ((ParentAddressCells == DEFAULT_ADDRESS_CELLS_VALUE) && TryZeroAddressCells) {
      ParentAddressCells = 0;
    }

    Status = DeviceTreeGetNodePropertyValue32 (ParentNodeOffset, "#interrupt-cells", &ParentInterruptCells);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get #interrupt-cells for (Interrupt Parent) NodeOffset 0x%x\n", __FUNCTION__, Status, ParentNodeOffset));
      break;
    }

    ParentAddressOffset   = ParentPhandleOffset + PARENT_PHANDLE_CELLS;
    ParentInterruptOffset = ParentAddressOffset + ParentAddressCells;
    EntryCells            = ParentInterruptOffset + ParentInterruptCells;
    DEBUG ((DEBUG_VERBOSE, "%a: ParentAddressOffset = %d, ParentInterruptOffset = %d, EntryCells = %u\n", __FUNCTION__, ParentAddressOffset, ParentInterruptOffset, EntryCells));

    // Sanity check the number of cells
    if ((CellIndex + EntryCells > NumCells) &&
        (!TryZeroAddressCells))
    {
      TryZeroAddressCells = TRUE;
      goto ParseInterruptMapEntries;
    }

    NV_ASSERT_RETURN (CellIndex + EntryCells <= NumCells, return EFI_DEVICE_ERROR, "%a: Cell size bug in parsing interrupt-map of node offset 0x%x\n", __FUNCTION__, NodeOffset);

    if (MapIndex < *NumberOfMaps) {
      NVIDIA_DEVICE_TREE_INTERRUPT_MAP_DATA  *Map = &InterruptMapArray[MapIndex];
      DEBUG ((DEBUG_VERBOSE, "%a: MapIndex = %u, *NumberOfMaps = %u\n", __FUNCTION__, MapIndex, *NumberOfMaps));

      Status = ParseAddressCells (&MapProperty[CellIndex + ChildAddressOffset], ChildAddressCells, &Map->ChildAddressHigh, &Map->ChildAddressLow);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to parse %u ChildAddressCells\n", __FUNCTION__, ChildAddressCells));
        break;
      }

      Status = ParseInterruptCells (&MapProperty[CellIndex + ChildInterruptOffset], ChildInterruptCells, &Map->ChildInterrupt);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to parse %u ChildInterruptCells\n", __FUNCTION__, ChildInterruptCells));
        break;
      }

      Map->InterruptParentPhandle = ParentPhandle;

      Status = ParseAddressCells (&MapProperty[CellIndex + ParentAddressOffset], ParentAddressCells, &Map->ParentAddressHigh, &Map->ParentAddressLow);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to parse %u ParentAddressCells\n", __FUNCTION__, ParentAddressCells));
        break;
      }

      Status = ParseInterruptCells (&MapProperty[CellIndex + ParentInterruptOffset], ParentInterruptCells, &Map->ParentInterrupt);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to parse %u ParentInterruptCells\n", __FUNCTION__, ParentInterruptCells));
        break;
      }
    }
  }

  // Older DTB had a bug where if parent address cells was missing it was treated as zero rather than the default value, so try that to see if it fixes the error
  if (EFI_ERROR (Status) && !TryZeroAddressCells) {
    TryZeroAddressCells = TRUE;
    goto ParseInterruptMapEntries;
  }

  if (*NumberOfMaps < MapIndex) {
    Status = EFI_BUFFER_TOO_SMALL;
  } else {
    Status = EFI_SUCCESS;
  }

  *NumberOfMaps = MapIndex;
  return Status;
}

STATIC
EFI_STATUS
ParseControllerCells (
  IN CONST UINT32                         *Property,
  IN UINT32                               NumCells,
  OUT NVIDIA_DEVICE_TREE_CONTROLLER_DATA  *Controller
  )
{
  DEBUG ((DEBUG_VERBOSE, "%a: Property = 0x%p, NumCells = %u, Controller = 0x%p\n", __FUNCTION__, Property, NumCells, Controller));
  NV_ASSERT_RETURN ((Property != NULL) || (NumCells == 0), return EFI_INVALID_PARAMETER, "%a: Property is NULL when NumCells is %u\n", __FUNCTION__, NumCells);
  NV_ASSERT_RETURN ((Controller != NULL) || (NumCells == 0), return EFI_INVALID_PARAMETER, "%a: NumCells is %u but Controller is NULL\n", __FUNCTION__, NumCells);

  switch (NumCells) {
    default:
      NV_ASSERT_RETURN (NumCells <= 1, return EFI_UNSUPPORTED, "%a: NumCells more than 1 aren't currently supported, but found %u\n", __FUNCTION__, NumCells);
      break;

    case 1:
      Controller->Base = Fdt32ToCpu (Property[0]);
      break;

    case 0:
      Controller->Base = DEVICE_ID_INVALID;
      break;
  }

  return EFI_SUCCESS;
}

#define MAP_RID_BASE_CELLS  1
#define MAP_LENGTH_CELLS    1

/**
  Returns information about the msi or iommu map of a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [in]      MapName            - "msi-map" or "iommu-map"
  @param  [out]     MapArray           - Buffer of size NumberOfMaps that will contain the list of map information
  @param  [in, out] NumberOfMaps       - On input contains size of the MapArray, on output number of required entries.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfMaps is less than required entries
  @retval EFI_INVALID_PARAMETER - NumberOfMaps pointer is NULL
  @retval EFI_INVALID_PARAMETER - MapArray is NULL when *NumberOfMaps is not 0
  @retval EFI_NOT_FOUND         - No maps found
  @retval EFI_UNSUPPORTED       - Found unsupported number of cells
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetMsiIommuMap (
  IN INT32                                   NodeOffset,
  IN CONST CHAR8                             *MapName,
  OUT NVIDIA_DEVICE_TREE_MSI_IOMMU_MAP_DATA  *MapArray OPTIONAL,
  IN OUT UINT32                              *NumberOfMaps
  )
{
  EFI_STATUS    Status;
  VOID          *DeviceTree;
  CONST UINT32  *MapProperty;
  UINT32        PropertySize;
  UINT32        MapIndex;
  UINT32        CellIndex;
  UINT32        NumCells;
  UINT32        RidBaseOffset;
  UINT32        ControllerPhandleOffset;
  UINT32        BaseOffset;
  UINT32        LengthOffset;
  UINT32        EntryCells;
  INT32         ControllerPhandle;
  UINT32        ControllerCells;
  CONST CHAR8   *CellsName;

  NV_ASSERT_RETURN (NumberOfMaps != NULL, return EFI_INVALID_PARAMETER, "%a: NumberOfMaps is not allowed to be NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN ((MapArray != NULL) || (*NumberOfMaps == 0), return EFI_INVALID_PARAMETER, "%a: MapArray can only be NULL if NumberOfMaps is zero\n", __FUNCTION__);
  NV_ASSERT_RETURN (MapName != NULL, return EFI_INVALID_PARAMETER, "%a: MapName is not allowed to be NULL\n", __FUNCTION__);

  if (AsciiStrCmp (MapName, "msi-map") == 0) {
    CellsName = "#msi-cells";
  } else if (AsciiStrCmp (MapName, "iommu-map") == 0) {
    CellsName = "#iommu-cells";
  } else {
    DEBUG ((DEBUG_ERROR, "%a: MapName must be \"msi-map\" or \"iommu-map\", but found \"%a\"\n", __FUNCTION__, MapName));
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get DeviceTreePointer\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, MapName, (CONST VOID **)&MapProperty, &PropertySize);
  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get %a property of NodeOffset 0x%x\n", __FUNCTION__, Status, MapName, NodeOffset));
    }

    return Status;
  }

  NumCells = PropertySize/sizeof (UINT32);

  RidBaseOffset           = 0;
  ControllerPhandleOffset = RidBaseOffset + MAP_RID_BASE_CELLS;
  BaseOffset              = ControllerPhandleOffset + PARENT_PHANDLE_CELLS;

  for (MapIndex = 0, CellIndex = 0; (CellIndex + ControllerPhandleOffset) < NumCells; MapIndex++, CellIndex += EntryCells) {
    DEBUG ((DEBUG_VERBOSE, "%a: MapIndex = %u, CellIndex = %u, NumCells = %u\n", __FUNCTION__, MapIndex, CellIndex, NumCells));
    NV_ASSERT_RETURN (CellIndex + ControllerPhandleOffset < NumCells, return EFI_DEVICE_ERROR, "%a: Cell parsing bug - controller phandle offset exceeds map property size for Node Offset 0x%x, MapIndex %u\n", __FUNCTION__, NodeOffset, MapIndex);
    ControllerPhandle = Fdt32ToCpu (MapProperty[CellIndex + ControllerPhandleOffset]);
    if (ControllerPhandle == NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
      DEBUG ((DEBUG_ERROR, "%a: Found invalid controller phandle 0x%x\n", __FUNCTION__, ControllerPhandle));
      return EFI_DEVICE_ERROR;
    }

    Status = GetPhandleCells (ControllerPhandle, CellsName, &ControllerCells);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get %a\n", __FUNCTION__, Status, CellsName));
      return Status;
    }

    LengthOffset = BaseOffset + ControllerCells;
    EntryCells   = LengthOffset + MAP_LENGTH_CELLS;
    DEBUG ((DEBUG_VERBOSE, "%a: LengthOffset = %u, EntryCells = %u\n", __FUNCTION__, LengthOffset, EntryCells));

    // Sanity check the number of cells
    NV_ASSERT_RETURN (CellIndex + EntryCells <= NumCells, return EFI_DEVICE_ERROR, "%a: Cell size bug in parsing msi-map of node offset 0x%x\n", __FUNCTION__, NodeOffset);

    if (MapIndex < *NumberOfMaps) {
      NVIDIA_DEVICE_TREE_MSI_IOMMU_MAP_DATA  *Map = &MapArray[MapIndex];
      DEBUG ((DEBUG_VERBOSE, "%a: MapIndex = %u, *NumberOfMaps = %u\n", __FUNCTION__, MapIndex, *NumberOfMaps));

      Map->RidBase            = Fdt32ToCpu (MapProperty[CellIndex + RidBaseOffset]);
      Map->Controller.Phandle = ControllerPhandle;
      Status                  = ParseControllerCells (&MapProperty[CellIndex + BaseOffset], ControllerCells, &Map->Controller);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to parse %u ControllerCells\n", __FUNCTION__, ControllerCells));
        return Status;
      }

      Map->Length = Fdt32ToCpu (MapProperty[CellIndex + LengthOffset]);
    }
  }

  if (*NumberOfMaps < MapIndex) {
    Status = EFI_BUFFER_TOO_SMALL;
  } else {
    Status = EFI_SUCCESS;
  }

  *NumberOfMaps = MapIndex;
  return Status;
}

/**
  Returns information about the msi parent a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [out]     Array              - Buffer of size NumberOfParents that will contain the list of msi parent information
  @param  [in, out] NumberOfParents    - On input contains size of the Array, on output number of required entries.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfParents is less than required entries
  @retval EFI_INVALID_PARAMETER - NumberOfParents pointer is NULL
  @retval EFI_INVALID_PARAMETER - Array is NULL when *NumberOfParents is not 0
  @retval EFI_NOT_FOUND         - No parents found
  @retval EFI_UNSUPPORTED       - Found unsupported number of cells
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetMsiParent (
  IN INT32                                NodeOffset,
  OUT NVIDIA_DEVICE_TREE_CONTROLLER_DATA  *Array OPTIONAL,
  IN OUT UINT32                           *NumberOfParents
  )
{
  EFI_STATUS    Status;
  VOID          *DeviceTree;
  CONST UINT32  *Property;
  UINT32        PropertySize;
  UINT32        ParentIndex;
  UINT32        CellIndex;
  UINT32        NumCells;
  UINT32        EntryCells;
  INT32         ControllerPhandle;
  UINT32        ControllerCells;

  NV_ASSERT_RETURN (NumberOfParents != NULL, return EFI_INVALID_PARAMETER, "%a: NumberOfParents is not allowed to be NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN ((Array != NULL) || (*NumberOfParents == 0), return EFI_INVALID_PARAMETER, "%a: Array can only be NULL if NumberOfParents is zero\n", __FUNCTION__);

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get DeviceTreePointer\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, "msi-parent", (CONST VOID **)&Property, &PropertySize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get \"msi-parent\" property of NodeOffset 0x%x\n", __FUNCTION__, Status, NodeOffset));
    return Status;
  }

  NumCells = PropertySize/sizeof (UINT32);

  for (ParentIndex = 0, CellIndex = 0; CellIndex < NumCells; ParentIndex++, CellIndex += EntryCells) {
    DEBUG ((DEBUG_VERBOSE, "%a: ParentIndex = %u, CellIndex = %u, NumCells = %u\n", __FUNCTION__, ParentIndex, CellIndex, NumCells));
    NV_ASSERT_RETURN (CellIndex < NumCells, return EFI_DEVICE_ERROR, "%a: Cell parsing bug - controller phandle offset exceeds msi-parent property size for Node Offset 0x%x, ParentIndex %u\n", __FUNCTION__, NodeOffset, ParentIndex);
    ControllerPhandle = Fdt32ToCpu (Property[CellIndex]);
    if (ControllerPhandle == NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
      DEBUG ((DEBUG_ERROR, "%a: Found invalid controller phandle 0x%x\n", __FUNCTION__, ControllerPhandle));
      return EFI_DEVICE_ERROR;
    }

    Status = GetPhandleCells (ControllerPhandle, "#msi-cells", &ControllerCells);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get \"#msi-cells\"\n", __FUNCTION__, Status));
      return Status;
    }

    EntryCells = PARENT_PHANDLE_CELLS + ControllerCells;
    DEBUG ((DEBUG_VERBOSE, "%a: EntryCells = %u\n", __FUNCTION__, EntryCells));

    // Sanity check the number of cells
    NV_ASSERT_RETURN (CellIndex + EntryCells <= NumCells, return EFI_DEVICE_ERROR, "%a: Cell size bug in parsing msi-parent of node offset 0x%x\n", __FUNCTION__, NodeOffset);

    if (ParentIndex < *NumberOfParents) {
      NVIDIA_DEVICE_TREE_CONTROLLER_DATA  *Parent = &Array[ParentIndex];
      DEBUG ((DEBUG_VERBOSE, "%a: ParentIndex = %u, *NumberOfParents = %u\n", __FUNCTION__, ParentIndex, *NumberOfParents));

      Parent->Phandle = ControllerPhandle;
      Status          = ParseControllerCells (&Property[CellIndex + PARENT_PHANDLE_CELLS], ControllerCells, Parent);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to parse %u ControllerCells\n", __FUNCTION__, ControllerCells));
        return Status;
      }
    }
  }

  if (*NumberOfParents < ParentIndex) {
    Status = EFI_BUFFER_TOO_SMALL;
  } else {
    Status = EFI_SUCCESS;
  }

  *NumberOfParents = ParentIndex;
  return Status;
}

STATIC
EFI_STATUS
ParseIommuCells (
  IN CONST UINT32                     *Property,
  IN UINT32                           Cells,
  OUT NVIDIA_DEVICE_TREE_IOMMUS_DATA  *Data
  )
{
  UINT64  Length;

  DEBUG ((DEBUG_VERBOSE, "%a: Property = 0x%p, Cells = %u, Data = 0x%p\n", __FUNCTION__, Property, Cells, Data));
  NV_ASSERT_RETURN (Property != NULL, return EFI_INVALID_PARAMETER, "%a: Property was NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (Data != NULL, return EFI_INVALID_PARAMETER, "%a: Data was NULL\n", __FUNCTION__);

  Data->MasterDeviceId  = DEVICE_ID_INVALID;
  Data->DmaWindowStart  = 0;
  Data->DmaWindowLength = 0;

  switch (Cells) {
    default:
      DEBUG ((DEBUG_ERROR, "%a: Don't know how to parse iommus that have %u cells\n", __FUNCTION__, Cells));
      return EFI_UNSUPPORTED;

    case 4:
      Data->DmaWindowStart = Fdt32ToCpu (Property[1]);
      CopyMem (&Length, &Property[2], sizeof (UINT64));
      Data->DmaWindowLength = Fdt64ToCpu (Length);

    case 1:
      Data->MasterDeviceId = Fdt32ToCpu (Property[0]);

    case 0:
      break;
  }

  DEBUG ((DEBUG_INFO, "%a: MasterDeviceId = 0x%x\n", __FUNCTION__, Data->MasterDeviceId));
  DEBUG ((DEBUG_INFO, "%a: DmaWindowStart = 0x%x\n", __FUNCTION__, Data->DmaWindowStart));
  DEBUG ((DEBUG_INFO, "%a: DmaWindowLength = 0x%lx\n", __FUNCTION__, Data->DmaWindowLength));
  return EFI_SUCCESS;
}

/**
  Returns information about the iommus of a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [out]     Array              - Buffer of size NumberOfIommus that will contain the list of iommu information
  @param  [in, out] NumberOfIommus     - On input contains size of the Array, on output number of required entries.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfIommus is less than required entries
  @retval EFI_INVALID_PARAMETER - NumberOfIommus pointer is NULL
  @retval EFI_INVALID_PARAMETER - Array is NULL when *NumberOfIommus is not 0
  @retval EFI_NOT_FOUND         - No iommus found
  @retval EFI_UNSUPPORTED       - Found unsupported number of cells
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetIommus (
  IN INT32                            NodeOffset,
  OUT NVIDIA_DEVICE_TREE_IOMMUS_DATA  *Array OPTIONAL,
  IN OUT UINT32                       *NumberOfIommus
  )
{
  EFI_STATUS    Status;
  VOID          *DeviceTree;
  CONST UINT32  *Property;
  UINT32        PropertySize;
  UINT32        IommusIndex;
  UINT32        CellIndex;
  UINT32        NumCells;
  UINT32        EntryCells;
  INT32         IommuPhandle;
  UINT32        IommuCells;

  NV_ASSERT_RETURN (NumberOfIommus != NULL, return EFI_INVALID_PARAMETER, "%a: NumberOfIommus is not allowed to be NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN ((Array != NULL) || (*NumberOfIommus == 0), return EFI_INVALID_PARAMETER, "%a: Array can only be NULL if NumberOfIommus is zero\n", __FUNCTION__);

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get DeviceTreePointer\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, "iommus", (CONST VOID **)&Property, &PropertySize);
  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get \"iommus\" property of NodeOffset 0x%x\n", __FUNCTION__, Status, NodeOffset));
    }

    return Status;
  }

  NumCells = PropertySize/sizeof (UINT32);

  for (IommusIndex = 0, CellIndex = 0; CellIndex < NumCells; IommusIndex++, CellIndex += EntryCells) {
    DEBUG ((DEBUG_VERBOSE, "%a: IommusIndex = %u, CellIndex = %u, NumCells = %u\n", __FUNCTION__, IommusIndex, CellIndex, NumCells));
    NV_ASSERT_RETURN (CellIndex < NumCells, return EFI_DEVICE_ERROR, "%a: Cell parsing bug - iommu phandle offset exceeds iommus property size for Node Offset 0x%x, IommusIndex %u\n", __FUNCTION__, NodeOffset, IommusIndex);
    IommuPhandle = Fdt32ToCpu (Property[CellIndex]);
    if (IommuPhandle == NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
      DEBUG ((DEBUG_ERROR, "%a: Found invalid iommu phandle 0x%x\n", __FUNCTION__, IommuPhandle));
      return EFI_DEVICE_ERROR;
    }

    Status = GetPhandleCells (IommuPhandle, "#iommu-cells", &IommuCells);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get \"#iommu-cells\"\n", __FUNCTION__, Status));
      return Status;
    }

    EntryCells = PARENT_PHANDLE_CELLS + IommuCells;
    DEBUG ((DEBUG_VERBOSE, "%a: EntryCells = %u\n", __FUNCTION__, EntryCells));

    // Sanity check the number of cells
    NV_ASSERT_RETURN (CellIndex + EntryCells <= NumCells, return EFI_DEVICE_ERROR, "%a: Cell size bug in parsing iommu of node offset 0x%x\n", __FUNCTION__, NodeOffset);

    if (IommusIndex < *NumberOfIommus) {
      NVIDIA_DEVICE_TREE_IOMMUS_DATA  *Iommu = &Array[IommusIndex];
      DEBUG ((DEBUG_VERBOSE, "%a: IommusIndex = %u, *NumberOfIommus = %u\n", __FUNCTION__, IommusIndex, *NumberOfIommus));

      Iommu->IommuPhandle = IommuPhandle;
      Status              = ParseIommuCells (&Property[CellIndex + PARENT_PHANDLE_CELLS], IommuCells, Iommu);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to parse %u IommuCells\n", __FUNCTION__, IommuCells));
        return Status;
      }
    }
  }

  if (*NumberOfIommus < IommusIndex) {
    Status = EFI_BUFFER_TOO_SMALL;
  } else {
    Status = EFI_SUCCESS;
  }

  *NumberOfIommus = IommusIndex;
  return Status;
}

STATIC CONST NVIDIA_DEVICE_TREE_CACHE_FIELD_STRINGS  ICacheFieldStrings = {
  "i-cache-size",
  "i-cache-sets",
  "i-cache-block-size",
  "i-cache-line-size"
};

STATIC CONST NVIDIA_DEVICE_TREE_CACHE_FIELD_STRINGS  DCacheFieldStrings = {
  "d-cache-size",
  "d-cache-sets",
  "d-cache-block-size",
  "d-cache-line-size"
};

STATIC CONST NVIDIA_DEVICE_TREE_CACHE_FIELD_STRINGS  UnifiedCacheFieldStrings = {
  "cache-size",
  "cache-sets",
  "cache-block-size",
  "cache-line-size"
};

/**
  Returns information about the cache of a given device tree node

  @param  [in]      NodeOffset       - Node offset of the device
  @param  [in, out] CacheData        - Buffer for the cache data. Type field specifies
                                       the type of cache data to populate from the Node

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - CacheData pointer is NULL
  @retval EFI_NOT_FOUND         - No cache data of Type found in the node
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetCacheData (
  IN INT32                              NodeOffset,
  IN OUT NVIDIA_DEVICE_TREE_CACHE_DATA  *CacheData
  )
{
  EFI_STATUS                                    Status;
  CONST NVIDIA_DEVICE_TREE_CACHE_FIELD_STRINGS  *FieldStrings;
  CONST CHAR8                                   *PropertyString;
  CHAR8                                         *NodePath;

  if (CacheData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Unified caches have a "cache-unified" property, split caches don't. Verify that the node matches the requested Type
  Status = DeviceTreeGetNodeProperty (NodeOffset, "cache-unified", NULL, NULL);
  if ((Status == EFI_NOT_FOUND) && (CacheData->Type == CACHE_TYPE_UNIFIED)) {
    // Requested Unified cache info, but it wasn't found in this node
    // Older device-tree doesn't mark L3 as cache-unified, so don't error out if it's missing
    DEBUG ((DEBUG_ERROR, "%a: Warning - trying to get unified cache data from a cache node that isn't marked as such.\nThe \"cache-unified\" property might be missing in the DTB\n", __FUNCTION__));
  } else if ((Status == EFI_SUCCESS) && (CacheData->Type != CACHE_TYPE_UNIFIED)) {
    // Found Unified cache info, but didn't request it for this node
    return EFI_NOT_FOUND;
  } else if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    return Status;
  }

  // Determine how to look up the fields
  switch (CacheData->Type) {
    case CACHE_TYPE_ICACHE:
      FieldStrings = &ICacheFieldStrings;
      break;

    case CACHE_TYPE_DCACHE:
      FieldStrings = &DCacheFieldStrings;
      break;

    case CACHE_TYPE_UNIFIED:
      FieldStrings = &UnifiedCacheFieldStrings;
      break;

    default:
      DEBUG ((DEBUG_ERROR, "%a: Trying to look up data for unknown CacheType %d\n", __FUNCTION__, CacheData->Type));
      return EFI_UNSUPPORTED;
  }

  // Get the ID
  Status = DeviceTreeGetNodePHandle (NodeOffset, &CacheData->CacheId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got error (%r) getting PHandle for NodeOffset 0x%x\n", __FUNCTION__, Status, NodeOffset));
    return Status;
  }

  // Get the Level
  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "cache-level", &CacheData->CacheLevel);
  if (Status == EFI_NOT_FOUND) {
    Status = DeviceTreeGetNodeProperty (
               NodeOffset,
               "device_type",
               (CONST VOID **)&PropertyString,
               NULL
               );
    if (!EFI_ERROR (Status)) {
      // Cpu node cache doesn't have "cache-level", but should be 1
      if (AsciiStrCmp ("cpu", PropertyString) == 0) {
        CacheData->CacheLevel = 1;
      } else if (AsciiStrCmp ("cache", PropertyString) == 0) {
        // Support older DTB that doesn't have the cache-level field but instead has device_type = "cache" and compatible = "l2-cache" or "l3-cache"
        Status = DeviceTreeGetNodeProperty (
                   NodeOffset,
                   "compatible",
                   (CONST VOID **)&PropertyString,
                   NULL
                   );
        if (!EFI_ERROR (Status)) {
          if (AsciiStrCmp ("l2-cache", PropertyString) == 0) {
            CacheData->CacheLevel = 2;
          } else if (AsciiStrCmp ("l3-cache", PropertyString) == 0) {
            CacheData->CacheLevel = 3;
          } else {
            DEBUG ((DEBUG_ERROR, "%a: Cache node has unknown \"compatible\" string \"%a\"\n", __FUNCTION__, PropertyString));
            return EFI_DEVICE_ERROR;
          }
        } else if (Status != EFI_NOT_FOUND) {
          DEBUG ((DEBUG_ERROR, "%a: Got %r trying to check the \"compatible\" property of the cache node at offset 0x%x\n", __FUNCTION__, Status, NodeOffset));
          return Status;
        }
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Got unknown \"device_type\" = \"%a\" for cache node at offset 0x%x\n", __FUNCTION__, PropertyString, NodeOffset));
        return EFI_DEVICE_ERROR;
      }
    }

    if (Status == EFI_NOT_FOUND) {
      // Older DTB is missing "cache-level" and either "device_type" or "compatible", so try to infer level from the path
      Status = DeviceTreeGetNodePath (NodeOffset, &NodePath, NULL);
      if (EFI_ERROR (Status) || (NodePath == NULL)) {
        DEBUG ((DEBUG_ERROR, "%a: The \"cache-level\" property for the cache node at offset 0x%x wasn't found, and got %r trying to get the NodePath to infer it\n", __FUNCTION__, NodeOffset, Status));
        return Status;
      }

      if (AsciiStrStr (NodePath, "l2c") != NULL) {
        CacheData->CacheLevel = 2;
      } else if (AsciiStrStr (NodePath, "l3c") != NULL) {
        CacheData->CacheLevel = 3;
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Unable to determine cache level based on the node path \"%a\"\n", __FUNCTION__, NodePath));
        FreePool (NodePath);
        return EFI_DEVICE_ERROR;
      }

      FreePool (NodePath);
    } else if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to infer the cache level of node offset 0x%x\n", __FUNCTION__, Status, NodeOffset));
      return Status;
    }
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got error (%r) searching for \"cache-level\" property for NodeOffset 0x%x\n", __FUNCTION__, Status, NodeOffset));
    return Status;
  }

  // Get the Size, Sets, BlockSize, and LineSize
  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, FieldStrings->SizeStr, &CacheData->CacheSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got error (%r) searching for %a property for NodeOffset 0x%x\n", __FUNCTION__, Status, FieldStrings->SizeStr, NodeOffset));
    return Status;
  }

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, FieldStrings->SetsStr, &CacheData->CacheSets);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got error (%r) searching for %a property for NodeOffset 0x%x\n", __FUNCTION__, Status, FieldStrings->SetsStr, NodeOffset));
    return Status;
  }

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, FieldStrings->BlockSizeStr, &CacheData->CacheBlockSize);
  if (EFI_ERROR (Status)) {
    // Get the value from hardware instead
    CacheData->CacheBlockSize = DeviceTreeGetCacheBlockSizeBytesFromHW ();
  }

  // Only required if different from CacheBlockSize
  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, FieldStrings->LineSizeStr, &CacheData->CacheLineSize);
  if (Status == EFI_NOT_FOUND) {
    CacheData->CacheLineSize = CacheData->CacheBlockSize;
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got error (%r) searching for %a property for NodeOffset 0x%x\n", __FUNCTION__, Status, FieldStrings->LineSizeStr, NodeOffset));
    return Status;
  }

  // Get the "next-level-cache", or older "l2-cache", if present
  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "next-level-cache", &CacheData->NextLevelCache);
  if (Status == EFI_NOT_FOUND) {
    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "l2-cache", &CacheData->NextLevelCache);
    if (Status == EFI_NOT_FOUND) {
      CacheData->NextLevelCache = 0;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
DeviceTreeFindRegisterByName (
  IN CONST CHAR8                             *RegisterName,
  IN CONST NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray,
  IN UINT32                                  NumberOfRegisters,
  OUT UINT32                                 *RegisterIndex
  )
{
  UINTN  Index;

  NV_ASSERT_RETURN ((RegisterName != NULL) && (RegisterArray != NULL) && (RegisterIndex != NULL), return EFI_INVALID_PARAMETER, "%a: bad parameter\n", __FUNCTION__);

  for (Index = 0; Index < NumberOfRegisters; Index++, RegisterArray++) {
    if (AsciiStrCmp (RegisterName, RegisterArray->Name) == 0) {
      DEBUG ((DEBUG_INFO, "%a: index %u reg %a base 0x%llx size 0x%llx\n", __FUNCTION__, Index, RegisterName, RegisterArray->BaseAddress, RegisterArray->Size));

      *RegisterIndex = Index;
      return EFI_SUCCESS;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: reg %a not found\n", __FUNCTION__, RegisterName));

  return EFI_NOT_FOUND;
}

/**
  Updates information about the registers of a given device tree node.
  Note: Name fields in the RegisterArray may not be valid upon return
  since they point to strings in the DTB.

  @param[in]  NodeOffset        - NodeHandle
  @param[in]  RegisterArray     - Buffer of size NumberOfRegisters that contains the list of register information
  @param[in]  NumberOfRegisters - Contains size of RegisterArray

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - RegisterArray is NULL or NumberOfRegisters is 0
  @retval EFI_NOT_FOUND         - No registers
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeSetRegisters (
  IN INT32                                   NodeOffset,
  IN CONST NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray,
  IN UINT32                                  NumberOfRegisters
  )
{
  EFI_STATUS   Status;
  VOID         *DeviceTree;
  INT32        ParentOffset;
  UINT64       AddressCells;
  UINT64       SizeCells;
  VOID         *RegProperty;
  VOID         *RegNames;
  UINT32       PropertySize;
  CONST CHAR8  *Name;
  UINT32       NameSize;
  INT32        NameOffset;
  UINTN        EntrySize;
  UINTN        RegionIndex;
  UINT64       AddressBase;
  UINT64       RegionSize;
  UINTN        NameCount;
  UINTN        RegNamesSize;
  BOOLEAN      NullNameFound = FALSE;

  if ((RegisterArray == NULL) || (NumberOfRegisters == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetParent (DeviceTree, NodeOffset, &ParentOffset);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceTreeGetNodePropertyValue64 (ParentOffset, "#address-cells", &AddressCells);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = DeviceTreeGetNodePropertyValue64 (ParentOffset, "#size-cells", &SizeCells);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if ((AddressCells > 2) || (AddressCells == 0) || (SizeCells > 2) || (SizeCells == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: Bad cell values, %llu, %llu\r\n", __FUNCTION__, AddressCells, SizeCells));
    return EFI_DEVICE_ERROR;
  }

  EntrySize    = sizeof (UINT32) * (AddressCells + SizeCells);
  PropertySize = NumberOfRegisters * EntrySize;
  RegProperty  = (VOID *)AllocateZeroPool (PropertySize);

  NameCount    = 0;
  RegNamesSize = 0;
  for (RegionIndex = 0; RegionIndex < NumberOfRegisters; RegionIndex++) {
    AddressBase = RegisterArray[RegionIndex].BaseAddress;
    RegionSize  = RegisterArray[RegionIndex].Size;

    DEBUG ((DEBUG_INFO, "%a: %u - 0x%llx: 0x%llx %a\n", __FUNCTION__, RegionIndex, AddressBase, RegionSize, RegisterArray[RegionIndex].Name));

    if (AddressCells == 2) {
      AddressBase = CpuToFdt64 (AddressBase);
    } else {
      AddressBase = CpuToFdt32 (AddressBase);
    }

    if (SizeCells == 2) {
      RegionSize = CpuToFdt64 (RegionSize);
    } else {
      RegionSize = CpuToFdt32 (RegionSize);
    }

    CopyMem (RegProperty + EntrySize * RegionIndex, (VOID *)&AddressBase, AddressCells * sizeof (UINT32));
    CopyMem (RegProperty + EntrySize * RegionIndex + (AddressCells * sizeof (UINT32)), (VOID *)&RegionSize, SizeCells * sizeof (UINT32));

    if ((RegisterArray[RegionIndex].Name != NULL) && !NullNameFound) {
      NameCount++;
      RegNamesSize += AsciiStrSize (RegisterArray[RegionIndex].Name);
    } else {
      NullNameFound = TRUE;
      DEBUG ((DEBUG_INFO, "%a: register %u name skipped,\n", __FUNCTION__, RegionIndex));
    }
  }

  DEBUG ((DEBUG_INFO, "%a: size=%u node %d %a\n", __FUNCTION__, PropertySize, NodeOffset, FdtGetName (DeviceTree, NodeOffset, NULL)));

  if (NameCount == 0) {
    DEBUG ((DEBUG_INFO, "%a: no names found for %u registers\n", __FUNCTION__, NumberOfRegisters));
    goto WriteRegProperty;
  }

  if (NameCount != NumberOfRegisters) {
    DEBUG ((DEBUG_INFO, "%a: name/register count mismatch %u/%u\n", __FUNCTION__, NameCount, NumberOfRegisters));
  }

  // must process reg-names before updating reg property since Names point into DTB
  RegNames   = (VOID *)AllocateZeroPool (RegNamesSize);
  NameOffset = 0;
  for (RegionIndex = 0; RegionIndex < NumberOfRegisters; RegionIndex++) {
    Name = RegisterArray[RegionIndex].Name;
    if (Name == NULL) {
      break;
    }

    NameSize = AsciiStrSize (Name);
    ASSERT (NameOffset + NameSize <= RegNamesSize);
    DEBUG ((DEBUG_INFO, "%a: name %u size=%u '%a'\n", __FUNCTION__, RegionIndex, NameSize, Name));

    CopyMem (RegNames + NameOffset, Name, NameSize);
    NameOffset += NameSize;
  }

  DEBUG ((DEBUG_INFO, "%a: names size=%u node %d %a\n", __FUNCTION__, RegNamesSize, NodeOffset, FdtGetName (DeviceTree, NodeOffset, NULL)));

  Status = DeviceTreeSetNodeProperty (NodeOffset, "reg-names", RegNames, RegNamesSize);
  FreePool (RegNames);
  if (EFI_ERROR (Status)) {
    return Status;
  }

WriteRegProperty:
  Status = DeviceTreeSetNodeProperty (NodeOffset, "reg", RegProperty, PropertySize);
  FreePool (RegProperty);

  return Status;
}
