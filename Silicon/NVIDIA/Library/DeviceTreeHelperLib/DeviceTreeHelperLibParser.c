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
      Status = DeviceTreeGetNodeByPHandle (Fdt32ToCpu (IntProperty[CellIndex++]), &ParentNodeOffset);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = DeviceTreeGetNodePropertyValue32 (ParentNodeOffset, "#interrupt-cells", &InterruptCells);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error getting #interrupt-cells count for interrupt controller 0x%x (rc=%r)\n", __FUNCTION__, ParentNodeOffset, Status));
        return Status;
      }

      ASSERT (InterruptCells > 0);
      if (InterruptCells == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Didn't get a valid #interrupt-cells count for interrupt controller 0x%x (rc=%r)\n", __FUNCTION__, ParentNodeOffset, Status));
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
      Status = DeviceTreeGetNodeByPHandle (Fdt32ToCpu (IntProperty[CellIndex++]), &ParentNodeOffset);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = DeviceTreeGetNodePropertyValue32 (ParentNodeOffset, "#interrupt-cells", &InterruptCells);
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
