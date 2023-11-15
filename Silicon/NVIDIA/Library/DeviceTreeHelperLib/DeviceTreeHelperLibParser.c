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
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/FdtLib.h>
#include <Library/MemoryAllocationLib.h>
#include "DeviceTreeHelperLibPrivate.h"

#define DEVICE_TREE_MAX_PROPERTY_LENGTH  32

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
STATIC
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
  ASSERT ((PropertySize % EntrySize) == 0);
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
@param[in]      RangeName       - Name of the ranges property ("ranges", "dma-ranges", etc)
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
  UINT64      AddressCells;
  UINT64      SizeCells;
  CONST VOID  *RangeProperty;
  CONST VOID  *RangeNames;
  UINT32      PropertySize;
  UINT32      NameSize;
  INT32       NameOffset;
  UINTN       EntrySize;
  UINTN       NumberOfRangeRegions;
  UINTN       RegionIndex;
  CHAR8       NamePropertyString[DEVICE_TREE_MAX_PROPERTY_LENGTH];

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

  Status = DeviceTreeGetNodeProperty (NodeOffset, RangeName, &RangeProperty, &PropertySize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  EntrySize = sizeof (UINT32) * (AddressCells + AddressCells + SizeCells);
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

    CopyMem (
      (VOID *)&ChildAddressBase,
      RangeProperty + EntrySize * RegionIndex,
      AddressCells * sizeof (UINT32)
      );
    CopyMem (
      (VOID *)&ParentAddressBase,
      RangeProperty + EntrySize * RegionIndex + (AddressCells * sizeof (UINT32)),
      AddressCells * sizeof (UINT32)
      );
    CopyMem ((VOID *)&RegionSize, RangeProperty + EntrySize * RegionIndex + (2 * (AddressCells * sizeof (UINT32))), SizeCells * sizeof (UINT32));
    if (AddressCells == 2) {
      ChildAddressBase  = Fdt64ToCpu (ChildAddressBase);
      ParentAddressBase = Fdt64ToCpu (ParentAddressBase);
    } else {
      ChildAddressBase  = Fdt32ToCpu (ChildAddressBase);
      ParentAddressBase = Fdt32ToCpu (ParentAddressBase);
    }

    if (SizeCells == 2) {
      RegionSize = Fdt64ToCpu (RegionSize);
    } else {
      RegionSize = Fdt32ToCpu (RegionSize);
    }

    RangesArray[RegionIndex].ChildAddress  = ChildAddressBase;
    RangesArray[RegionIndex].ParentAddress = ParentAddressBase;
    RangesArray[RegionIndex].Size          = RegionSize;
    RangesArray[RegionIndex].Name          = NULL;

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

/**
  Returns information about the interrupts of a given device tree node

  @param  [in]      NodeOffset         - Node offset of the device
  @param  [out]     InterruptArray     - Buffer of size NumberOfInterrupts that will contain the list of interrupt information
  @param  [in, out] NumberOfInterrupts - On input contains size of InterruptArray, on output number of required registers.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfInterrupts is less than required registers
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
  UINT64        InterruptCells;
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
      Status = DeviceTreeGetNodeByPHandle (Fdt32ToCpu (IntProperty[CellIndex++]), &ParentNodeOffset);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = DeviceTreeGetNodePropertyValue64 (ParentNodeOffset, "#interrupt-cells", &InterruptCells);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error getting #interrupt-cells count for interrupt controller 0x%x (rc=%r)\n", __FUNCTION__, ParentNodeOffset, Status));
        return Status;
      }

      ASSERT (InterruptCells > 0);
      if (InterruptCells == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Didn't get a valid #interrupt-cells count for interrupt controller 0x%x (rc=%r)\n", __FUNCTION__, ParentNodeOffset, Status));
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((DEBUG_VERBOSE, "%a: Parent has %llu interrupt cells\n", __FUNCTION__, InterruptCells));
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
      Status = DeviceTreeGetNodePropertyValue64 (ParentNodeOffset, "#interrupt-cells", &InterruptCells);
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
      Status = DeviceTreeGetNodeByPHandle (Fdt32ToCpu (IntProperty[CellIndex++]), &ParentNodeOffset);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = DeviceTreeGetNodePropertyValue64 (ParentNodeOffset, "#interrupt-cells", &InterruptCells);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error getting #interrupt-cells count for interrupt controller 0x%x (rc=%r)\n", __FUNCTION__, ParentNodeOffset, Status));
        return Status;
      }

      ASSERT (InterruptCells > 0);
      if (InterruptCells == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Didn't get a valid #interrupt-cells count for interrupt controller 0x%x (rc=%r)\n", __FUNCTION__, ParentNodeOffset, Status));
        return EFI_DEVICE_ERROR;
      }
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "compatible", (CONST VOID **)&InterruptArray[IntIndex].ControllerCompatible, NULL);
    if (EFI_ERROR (Status)) {
      InterruptArray[IntIndex].ControllerCompatible = NULL;
    }

    if (InterruptCells >= 3) {
      DEBUG ((DEBUG_INFO, "%a: IntProperty[%u] - Type = %u\n", __FUNCTION__, CellIndex, Fdt32ToCpu (IntProperty[CellIndex])));
      InterruptArray[IntIndex].Type = Fdt32ToCpu (IntProperty[CellIndex++]);
    }

    DEBUG ((DEBUG_INFO, "%a: IntProperty[%u] - Interrupt = %u\n", __FUNCTION__, CellIndex, Fdt32ToCpu (IntProperty[CellIndex])));
    InterruptArray[IntIndex].Interrupt = Fdt32ToCpu (IntProperty[CellIndex++]);
    if (InterruptCells >= 2) {
      DEBUG ((DEBUG_INFO, "%a: IntProperty[%u] - Flag = %u\n", __FUNCTION__, CellIndex, Fdt32ToCpu (IntProperty[CellIndex])));
      InterruptArray[IntIndex].Flag = Fdt32ToCpu (IntProperty[CellIndex++]);
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
