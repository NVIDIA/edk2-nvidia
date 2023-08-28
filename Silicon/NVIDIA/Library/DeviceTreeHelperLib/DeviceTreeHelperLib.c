/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>

STATIC VOID   *LocalDeviceTree    = NULL;
STATIC UINTN  LocalDeviceTreeSize = 0;

/**
  Set the base address and size of the device tree

  This is to support the use cases when the HOB list is not populated.

  @param  DeviceTree        - Pointer to base Address of the device tree.
  @param  DeviceTreeSize    - Pointer to size of the device tree.

**/
VOID
SetDeviceTreePointer (
  IN  VOID   *DeviceTree,
  IN  UINTN  DeviceTreeSize
  )
{
  LocalDeviceTree     = DeviceTree;
  LocalDeviceTreeSize = DeviceTreeSize;
}

/**
  Return the base address and size of the device tree

  @param  DeviceTree        - Pointer to base Address of the device tree.
  @param  DeviceTreeSize    - Pointer to size of the device tree.

  @return EFI_SUCCESS       - Operation successful
  @return others            - Failed to get device tree base or tree size

**/
STATIC
EFI_STATUS
GetDeviceTreePointer (
  OUT VOID   **DeviceTree,
  OUT UINTN  *DeviceTreeSize
  )
{
  if ((LocalDeviceTree == NULL) || (LocalDeviceTreeSize == 0)) {
    return DtPlatformLoadDtb (DeviceTree, DeviceTreeSize);
  }

  *DeviceTree     = LocalDeviceTree;
  *DeviceTreeSize = LocalDeviceTreeSize;
  return EFI_SUCCESS;
}

/**
  Returns the enabled nodes that match the compatible string

  @param  CompatibleString - String to located devices for
  @param  NodeHandleArray  - Buffer of size NumberOfNodes that will contain the list of supported nodes
  @param  NumberOfNodes    - On input contains size of NodeOffsetArray, on output number of matching nodes.

  @retval EFI_SUCCESS           - Nodes located
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfNodes is less than required nodes
  @retval EFI_INVALID_PARAMETER - CompatibleString is NULL
  @retval EFI_INVALID_PARAMETER - NumberOfNodes pointer is NULL
  @retval EFI_INVALID_PARAMETER - NodeOffsetArray is NULL when *NumberOfNodes is not 0
  @retval EFI_NOT_FOUND         - No matching nodes
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetMatchingEnabledDeviceTreeNodes (
  IN CONST CHAR8  *CompatibleString,
  OUT UINT32      *NodeHandleArray OPTIONAL,
  IN OUT UINT32   *NumberOfNodes
  )
{
  UINT32      OriginalSize;
  UINT32      DeviceCount;
  EFI_STATUS  Status;
  VOID        *DeviceTree;
  UINTN       DeviceTreeSize;
  INT32       Offset;
  CONST VOID  *Property;

  if ((CompatibleString == NULL) ||
      (NumberOfNodes == NULL)    ||
      ((*NumberOfNodes != 0) && (NodeHandleArray == NULL)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, &DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to load DTB (%r)\r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  DeviceCount = 0;
  Offset      = fdt_node_offset_by_compatible (DeviceTree, -1, CompatibleString);
  while (Offset != -FDT_ERR_NOTFOUND) {
    Property = fdt_getprop (
                 DeviceTree,
                 Offset,
                 "status",
                 NULL
                 );
    if ((Property == NULL) ||
        (AsciiStrCmp (Property, "okay") == 0))
    {
      if (DeviceCount < *NumberOfNodes) {
        NodeHandleArray[DeviceCount] = (UINT32)Offset;
      }

      DeviceCount++;
    }

    Offset = fdt_node_offset_by_compatible (DeviceTree, Offset, CompatibleString);
  }

  OriginalSize   = *NumberOfNodes;
  *NumberOfNodes = DeviceCount;
  if (DeviceCount == 0) {
    Status = EFI_NOT_FOUND;
  } else if (DeviceCount > OriginalSize) {
    Status = EFI_BUFFER_TOO_SMALL;
  } else {
    Status = EFI_SUCCESS;
  }

  return Status;
}

/**
  Return kernel and kernel DTB address.

  Look for the /chosen/kernel-start and /chosen/kernel-dtb-start properties. If
  they are set, return them.  These may be set if a kernel was loaded for us.

  @param  KernelStart     - Output the kernel's base address
  @param  KernelDtbStart  - Output the kernel DTB's base address

  @retval EFI_SUCCESS           - Nodes located
  @retval EFI_INVALID_PARAMETER - KernelStart is NULL
  @retval EFI_INVALID_PARAMETER - KernelDtbStart is NULL
  @retval EFI_NOT_FOUND         - No matching nodes
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetKernelAddress (
  OUT UINT64  *KernelStart,
  OUT UINT64  *KernelDtbStart
  )
{
  EFI_STATUS  Status;
  VOID        *DeviceTree;
  UINTN       DeviceTreeSize;
  INT32       Offset;

  if ((KernelStart == NULL) || (KernelDtbStart == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, &DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Offset = fdt_path_offset (DeviceTree, "/chosen");
  if (Offset == -FDT_ERR_NOTFOUND) {
    return EFI_NOT_FOUND;
  }

  Status = GetNodeFieldByName64 (DeviceTree, Offset, "kernel-start", KernelStart);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GetNodeFieldByName64 (DeviceTree, Offset, "kernel-dtb-start", KernelDtbStart);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Returns the specific device tree node information

  @param  NodeHandle      - NodeHandle
  @param  DeviceTreeBase  - Base Address of the device tree.
  @param  NodeOffset      - Offset from DeviceTreeBase to the specified node.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - NodeHandle is in invalid
  @retval EFI_INVALID_PARAMETER - DeviceTreeBase pointer is NULL
  @retval EFI_INVALID_PARAMETER - NodeOffset is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetDeviceTreeNode (
  IN UINT32  Handle,
  OUT VOID   **DeviceTreeBase,
  OUT INT32  *NodeOffset
  )
{
  EFI_STATUS  Status;
  VOID        *DeviceTree;
  UINTN       DeviceTreeSize;

  if ((DeviceTreeBase == NULL) ||
      (NodeOffset == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, &DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if (Handle > DeviceTreeSize) {
    return EFI_INVALID_PARAMETER;
  }

  *DeviceTreeBase = DeviceTree;
  *NodeOffset     = (INT32)Handle;
  return EFI_SUCCESS;
}

/**
  Returns the handle for a specific  node

  @param  DeviceTreeBase  - Base Address of the device tree.
  @param  NodeOffset      - Offset from DeviceTreeBase to the specified node.
  @param  NodeHandle      - NodeHandle

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - NodeHandle is in NULL
  @retval EFI_INVALID_PARAMETER - DeviceTreeBase pointer is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetDeviceTreeHandle (
  IN  VOID    *DeviceTreeBase,
  IN  INT32   NodeOffset,
  OUT UINT32  *Handle
  )
{
  EFI_STATUS  Status;
  VOID        *DeviceTree;
  UINTN       DeviceTreeSize;

  if ((DeviceTreeBase == NULL) ||
      (Handle == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, &DeviceTreeSize);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if (DeviceTree != DeviceTreeBase) {
    return EFI_INVALID_PARAMETER;
  }

  if (NodeOffset > DeviceTreeSize) {
    return EFI_INVALID_PARAMETER;
  }

  *Handle = (UINT32)NodeOffset;
  return EFI_SUCCESS;
}

/**
  Returns information about the registers of a given device tree node

  @param  NodeHandle        - NodeHandle
  @param  RegisterArray     - Buffer of size NumberOfRegisters that will contain the list of register information
  @param  NumberOfRegisters - On input contains size of RegisterArray, on output number of required registers.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfRegisters is less than required registers
  @retval EFI_INVALID_PARAMETER - Handle is invalid
  @retval EFI_INVALID_PARAMETER - NumberOfRegisters pointer is NULL
  @retval EFI_INVALID_PARAMETER - RegisterArray is NULL when *NumberOfRegisters is not 0
  @retval EFI_NOT_FOUND         - No registers
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetDeviceTreeRegisters (
  IN UINT32                             Handle,
  OUT NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray OPTIONAL,
  IN OUT UINT32                         *NumberOfRegisters
  )
{
  EFI_STATUS  Status;
  VOID        *DeviceTree;
  INT32       NodeOffset;
  INT32       AddressCells;
  INT32       SizeCells;
  CONST VOID  *RegProperty;
  CONST VOID  *RegNames;
  INT32       PropertySize;
  INT32       NameSize;
  INT32       NameOffset;
  UINTN       EntrySize;
  UINTN       NumberOfRegRegions;
  UINTN       RegionIndex;

  if ((NumberOfRegisters == NULL) ||
      ((*NumberOfRegisters != 0) && (RegisterArray == NULL)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreeNode (Handle, &DeviceTree, &NodeOffset);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  AddressCells = fdt_address_cells (DeviceTree, fdt_parent_offset (DeviceTree, NodeOffset));
  SizeCells    = fdt_size_cells (DeviceTree, fdt_parent_offset (DeviceTree, NodeOffset));

  if ((AddressCells > 2) ||
      (AddressCells == 0) ||
      (SizeCells > 2) ||
      (SizeCells == 0))
  {
    DEBUG ((DEBUG_ERROR, "%a: Bad cell values, %d, %d\r\n", __FUNCTION__, AddressCells, SizeCells));
    return EFI_DEVICE_ERROR;
  }

  RegProperty = fdt_getprop (
                  DeviceTree,
                  NodeOffset,
                  "reg",
                  &PropertySize
                  );
  if (RegProperty == NULL) {
    return EFI_NOT_FOUND;
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

  RegNames = fdt_getprop (
               DeviceTree,
               NodeOffset,
               "reg-names",
               &NameSize
               );
  if (RegNames == NULL) {
    NameSize = 0;
  }

  NameOffset = 0;

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
  Gets the value of a 32-bit field within the specified node

  @param  [in]  DeviceTreeBase  - Base Address of the device tree.
  @param  [in]  NodeOffset      - Offset from DeviceTreeBase to the specified node.
  @param  [in]  Name            - Name of the field to look up
  @param  [out] Value           - The resulting value of the field

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - DeviceTreeBase pointer is NULL
  @retval EFI_INVALID_PARAMETER - NodeOffset is 0
  @retval EFI_INVALID_PARAMETER - Name pointer is NULL
  @retval EFI_NOT_FOUND         - Name wasn't found in the specified node

**/
EFI_STATUS
EFIAPI
GetNodeFieldByName32 (
  IN CONST VOID   *DeviceTree,
  IN INT32        NodeOffset,
  IN CONST CHAR8  *Name,
  OUT UINT32      *Value
  )
{
  CONST UINT32  *Field;
  INT32         FieldSize;

  if ((DeviceTree == NULL) || (NodeOffset == 0) || (Name == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Field = (CONST UINT32 *)fdt_getprop (
                            DeviceTree,
                            NodeOffset,
                            Name,
                            &FieldSize
                            );

  if ((Field != NULL) && (FieldSize == sizeof (UINT32))) {
    *Value = SwapBytes32 (*Field);
    return EFI_SUCCESS;
  } else {
    return EFI_NOT_FOUND;
  }
}

/**
  Gets the value of a 64-bit field within the specified node

  @param  [in]  DeviceTreeBase  - Base Address of the device tree.
  @param  [in]  NodeOffset      - Offset from DeviceTreeBase to the specified node.
  @param  [in]  Name            - Name of the field to look up
  @param  [out] Value           - The resulting value of the field

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_INVALID_PARAMETER - DeviceTreeBase pointer is NULL
  @retval EFI_INVALID_PARAMETER - NodeOffset is 0
  @retval EFI_INVALID_PARAMETER - Name pointer is NULL
  @retval EFI_NOT_FOUND         - Name wasn't found in the specified node

**/
EFI_STATUS
EFIAPI
GetNodeFieldByName64 (
  IN CONST VOID   *DeviceTree,
  IN INT32        NodeOffset,
  IN CONST CHAR8  *Name,
  OUT UINT64      *Value
  )
{
  CONST VOID  *Field;
  INT32       FieldSize;

  if ((DeviceTree == NULL) || (NodeOffset == 0) || (Name == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Field = fdt_getprop (DeviceTree, NodeOffset, Name, &FieldSize);
  if ((Field == NULL) || (FieldSize != sizeof (UINT64))) {
    return EFI_NOT_FOUND;
  }

  if ((Field != NULL) && (FieldSize == sizeof (UINT64))) {
    *Value = fdt64_to_cpu (ReadUnaligned64 ((CONST UINT64 *)Field));
    return EFI_SUCCESS;
  } else {
    return EFI_NOT_FOUND;
  }
}

/**
  Gets the offset of the interrupt-parent of the specified node

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
  UINT32      ParentPhandle;
  CONST VOID  *InterruptControllerProperty;

  if ((DeviceTree == NULL) || (NodeOffset == 0) || (ParentNodeOffset == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetNodeFieldByName32 (DeviceTree, NodeOffset, "interrupt-parent", &ParentPhandle);

  if (!EFI_ERROR (Status)) {
    *ParentNodeOffset = fdt_node_offset_by_phandle (DeviceTree, ParentPhandle);
  } else if (Status == EFI_NOT_FOUND) {
    *ParentNodeOffset = fdt_parent_offset (DeviceTree, NodeOffset);
  } else {
    return Status;
  }

  // Verify that this node is an interrupt-controller, and if not, recursively look for its parent
  InterruptControllerProperty = fdt_get_property_namelen (DeviceTree, *ParentNodeOffset, "interrupt-controller", strlen ("interrupt-controller"), NULL);
  if (InterruptControllerProperty != NULL) {
    return EFI_SUCCESS;
  } else {
    return GetInterruptParentOffset (DeviceTree, *ParentNodeOffset, ParentNodeOffset);
  }
}

/**
  Returns information about the interrupts of a given device tree node

  @param  [in]      NodeHandle         - NodeHandle
  @param  [out]     InterruptArray     - Buffer of size NumberOfInterrupts that will contain the list of interrupt information
  @param  [in, out] NumberOfInterrupts - On input contains size of InterruptArray, on output number of required registers.

  @retval EFI_SUCCESS           - Operation successful
  @retval EFI_BUFFER_TOO_SMALL  - NumberOfInterrupts is less than required registers
  @retval EFI_INVALID_PARAMETER - Handle is invalid
  @retval EFI_INVALID_PARAMETER - NumberOfInterrupts pointer is NULL
  @retval EFI_INVALID_PARAMETER - InterruptArray is NULL when *NumberOfInterrupts is not 0
  @retval EFI_NOT_FOUND         - No interrupts
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
GetDeviceTreeInterrupts (
  IN UINT32                              Handle,
  OUT NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptArray OPTIONAL,
  IN OUT UINT32                          *NumberOfInterrupts
  )
{
  EFI_STATUS    Status;
  VOID          *DeviceTree;
  INT32         NodeOffset;
  UINT32        InterruptCells;
  CONST UINT32  *IntProperty;
  CONST VOID    *IntNames;
  INT32         PropertySize;
  UINT32        IntPropertyEntries;
  UINT32        IntIndex;
  INT32         NameSize;
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

  Status = GetDeviceTreeNode (Handle, &DeviceTree, &NodeOffset);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Spec says to use "interrupts-extended" if both it and "interrupts" exist
  IntProperty = (CONST UINT32 *)fdt_getprop (
                                  DeviceTree,
                                  NodeOffset,
                                  "interrupts-extended",
                                  &PropertySize
                                  );

  if (IntProperty != NULL) {
    ExtendedInterrupts = TRUE;
    NumCells           = PropertySize/sizeof (UINT32);

    CellIndex          = 0;
    IntPropertyEntries = 0;
    while (CellIndex < NumCells) {
      ParentNodeOffset = fdt_node_offset_by_phandle (DeviceTree, SwapBytes32 (IntProperty[CellIndex++]));
      Status           = GetNodeFieldByName32 (DeviceTree, ParentNodeOffset, "#interrupt-cells", &InterruptCells);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error getting #interrupt-cells count for interrupt controller \"%a\" (rc=%r)\n", __FUNCTION__, fdt_getprop (DeviceTree, ParentNodeOffset, "compatible", NULL), Status));
        return Status;
      }

      ASSERT (InterruptCells > 0);
      if (InterruptCells == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Didn't get a valid #interrupt-cells count for interrupt controller \"%a\" (rc=%r)\n", __FUNCTION__, fdt_getprop (DeviceTree, ParentNodeOffset, "compatible", NULL), Status));
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

    IntProperty = (CONST UINT32 *)fdt_getprop (
                                    DeviceTree,
                                    NodeOffset,
                                    "interrupts",
                                    &PropertySize
                                    );

    if (IntProperty == NULL) {
      return EFI_NOT_FOUND;
    }

    NumCells = PropertySize/sizeof (UINT32);
    Status   = GetInterruptParentOffset (DeviceTree, NodeOffset, &ParentNodeOffset);

    if (!EFI_ERROR (Status)) {
      Status = GetNodeFieldByName32 (DeviceTree, ParentNodeOffset, "#interrupt-cells", &InterruptCells);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error getting #interrupt-cells count for interrupt controller \"%a\" (rc=%r)\n", __FUNCTION__, fdt_getprop (DeviceTree, ParentNodeOffset, "compatible", NULL), Status));
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

  IntNames = fdt_getprop (
               DeviceTree,
               NodeOffset,
               "interrupt-names",
               &NameSize
               );
  if (IntNames == NULL) {
    NameSize = 0;
  }

  NameOffset = 0;

  CellIndex = 0;
  for (IntIndex = 0; IntIndex < IntPropertyEntries; IntIndex++) {
    ASSERT (CellIndex < NumCells);
    if (ExtendedInterrupts) {
      ParentNodeOffset = fdt_node_offset_by_phandle (DeviceTree, SwapBytes32 (IntProperty[CellIndex++]));
      Status           = GetNodeFieldByName32 (DeviceTree, ParentNodeOffset, "#interrupt-cells", &InterruptCells);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error getting #interrupt-cells count for interrupt controller \"%a\" (rc=%r)\n", __FUNCTION__, fdt_getprop (DeviceTree, ParentNodeOffset, "compatible", NULL), Status));
        return Status;
      }

      ASSERT (InterruptCells > 0);
      if (InterruptCells == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Didn't get a valid #interrupt-cells count for interrupt controller \"%a\" (rc=%r)\n", __FUNCTION__, fdt_getprop (DeviceTree, ParentNodeOffset, "compatible", NULL), Status));
        return EFI_DEVICE_ERROR;
      }
    }

    InterruptArray[IntIndex].ControllerCompatible = fdt_getprop (DeviceTree, ParentNodeOffset, "compatible", NULL);
    if (InterruptCells >= 3) {
      DEBUG ((DEBUG_INFO, "%a: IntProperty[%u] - Type = %u\n", __FUNCTION__, CellIndex, SwapBytes32 (IntProperty[CellIndex])));
      InterruptArray[IntIndex].Type = SwapBytes32 (IntProperty[CellIndex++]);
    }

    DEBUG ((DEBUG_INFO, "%a: IntProperty[%u] - Interrupt = %u\n", __FUNCTION__, CellIndex, SwapBytes32 (IntProperty[CellIndex])));
    InterruptArray[IntIndex].Interrupt = SwapBytes32 (IntProperty[CellIndex++]);
    if (InterruptCells >= 2) {
      DEBUG ((DEBUG_INFO, "%a: IntProperty[%u] - Flag = %u\n", __FUNCTION__, CellIndex, SwapBytes32 (IntProperty[CellIndex])));
      InterruptArray[IntIndex].Flag = SwapBytes32 (IntProperty[CellIndex++]);
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
