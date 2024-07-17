/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef DEVICE_TREE_HELPER_LIB_PRIVATE_H__
#define DEVICE_TREE_HELPER_LIB_PRIVATE_H__
#include <PiDxe.h>

#define GET_NODE_HIERARCHY_DEPTH_GUESS  32

// DTB v0.4 spec says to use the defaults if the parameter is missing
#define DEFAULT_ADDRESS_CELLS_VALUE  2
#define DEFAULT_SIZE_CELLS_VALUE     1

/**
  Gets the node hierarchy for a given node

  Returns an array of all the parent node offsets of the node.

  @param[in]  DeviceTree          - Pointer to base Address of the device tree.
  @param[in]  NodeOffset          - Offset of the node to get information on.
  @param[out] OffsetArray         - Pointer to an array that will be returned
                                    that will contain all of the offsets of the
                                    nodes above the input node.
                                    There will normally be NodeDepth entries
                                    in list and will contain the specified node.
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
  );

/**
  Returns the cache block size in bytes based on the hardware

  @retval BlockSize - Block size in bytes

**/
UINT32
DeviceTreeGetCacheBlockSizeBytesFromHW (
  VOID
  );

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
  );

#endif //DEVICE_TREE_HELPER_LIB_PRIVATE_H__
