/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/FdtLib.h>
#include <Library/MemoryAllocationLib.h>
#include "DeviceTreeHelperLibPrivate.h"

/**
  Gets the node hierarchy for a given node

  Returns an array of all the parent node offsets of the node.

  @param[in]  DeviceTree          - Pointer to base Address of the device tree.
  @param[in]  NodeOffset          - Offset of the node to get information on.
  @param[out] OffsetArray         - Pointer to an array that will be returned
                                    that will contain all of the offsets of the
                                    nodes above the input node.
                                    There will normally be NodeDepth entries
                                    in list and will specified node.
                                    OffsetArray[0] will indicate the node at
                                    depth 1
                                    If NodeDepth is != 0 on input and
                                    OffsetEntries is < NodeDepth, entries will
                                    start at depth (NodeDepth-OffsetArrayEntries).
  @param[in]  OffsetArrayEntries  - Size of the OffsetArray in entries.
  @param[out] NodeDepth           - Depth of the specified node.
                                    If non-zero on input is a hint for the depth
                                    of the node.

  @retval EFI_SUCCESS           - Node hierarchy is returned.
  @retval EFI_INVALID_PARAMETER - DeviceTree is NULL.
  @retval EFI_INVALID_PARAMETER - Node Offset is < 0.
  @retval EFI_INVALID_PARAMETER - OffsetArray is NULL if OffsetArrayEntries != 0.
  @retval EFI_INVALID_PARAMETER - NodeDepth is NULL.
  @retval EFI_BUFFER_TOO_SMALL  - OffsetArray was too small to hold offsets of
                                  all parents.
  @retval EFI_NOT_FOUND         - Node is not found.

**/
EFI_STATUS
EFIAPI
GetNodeHierarchyInfo (
  IN  CONST VOID  *DeviceTree,
  IN  INT32       NodeOffset,
  OUT INT32       *OffsetArray OPTIONAL,
  IN  UINT32      OffsetArrayEntries,
  IN OUT UINT32   *NodeDepth
  )
{
  EFI_STATUS  Status;
  INT32       Depth;
  INT32       CurrentOffset;
  UINT32      BaseNodeOffset;

  if ((DeviceTree == NULL) ||
      (NodeOffset < 0) ||
      ((OffsetArray == NULL) && (OffsetArray != 0)) ||
      (NodeDepth == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  BaseNodeOffset = 1;
  if (*NodeDepth > OffsetArrayEntries) {
    BaseNodeOffset = *NodeDepth-OffsetArrayEntries-1;
  }

  CurrentOffset = 0;
  Depth         = 0;
  Status        = EFI_NOT_FOUND;
  do {
    // Log the current node into the array, the most recent node at a depth is
    // the parent of the node.
    if ((Depth >= BaseNodeOffset) &&
        ((Depth - BaseNodeOffset) < OffsetArrayEntries))
    {
      OffsetArray[Depth-BaseNodeOffset] = CurrentOffset;
    }

    if (CurrentOffset == NodeOffset) {
      Status = EFI_SUCCESS;
      break;
    }

    CurrentOffset = FdtNextNode (DeviceTree, CurrentOffset, &Depth);
    if (CurrentOffset < 0) {
      break;
    }
  } while (CurrentOffset <= NodeOffset);

  if (!EFI_ERROR (Status)) {
    if ((Depth - BaseNodeOffset) >= OffsetArrayEntries) {
      Status = EFI_BUFFER_TOO_SMALL;
    }

    *NodeDepth = Depth;
  }

  return Status;
}
