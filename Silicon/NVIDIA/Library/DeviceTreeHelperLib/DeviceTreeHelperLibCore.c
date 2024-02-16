/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#define DEVICE_TREE_MAX_NAME_LENGTH  32

/**
  Get the next node with at least one compatible property.

  The Device tree is traversed in a depth-first search, starting from Node.
  The input Node is skipped.
  The status property is checked and if present needs to be "okay"

  @param [in]  CompatibleInfo   Pointer to an array of compatible strings.
                                Array is terminated with a NULL entry.
  @param [in, out]  NodeOffset  At entry: Node offset to start the search.
                                          This first node is skipped.
                                          Write (-1) to search the whole tree.
                                At exit:  If success, contains the offset of
                                          the next node in the branch
                                          being compatible.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetNextCompatibleNode (
  IN      CONST CHAR8  **CompatibleInfo,
  IN OUT        INT32  *NodeOffset
  )
{
  EFI_STATUS   Status;
  INT32        SearchNodeOffset;
  VOID         *DeviceTree;
  UINT32       CompatiblityIndex;
  UINT32       StringIndex;
  CONST CHAR8  *StatusString;

  if ((CompatibleInfo == NULL) || (NodeOffset == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  SearchNodeOffset = FdtNextNode (DeviceTree, *NodeOffset, NULL);

  while (SearchNodeOffset >= 0) {
    Status = EFI_NOT_FOUND;
    for (CompatiblityIndex = 0; CompatibleInfo[CompatiblityIndex] != NULL; CompatiblityIndex++) {
      Status = DeviceTreeLocateStringIndex (
                 SearchNodeOffset,
                 "compatible",
                 CompatibleInfo[CompatiblityIndex],
                 &StringIndex
                 );
      // Exit if not found. This covers both the case of EFI_SUCCESS and other errors
      if (Status != EFI_NOT_FOUND) {
        break;
      }
    }

    // No mapping is valid and means that the compatible entry is not present
    if (Status == EFI_NO_MAPPING) {
      Status = EFI_NOT_FOUND;
    }

    if (!EFI_ERROR (Status)) {
      // Compatible node found, check for status node, if defined needs to be "okay"
      Status = DeviceTreeGetNodeProperty (
                 SearchNodeOffset,
                 "status",
                 (CONST VOID **)&StatusString,
                 NULL
                 );
      if (Status == EFI_NOT_FOUND) {
        // Treat missing node as valid
        Status = EFI_SUCCESS;
        break;
      } else if (!EFI_ERROR (Status)) {
        if (AsciiStrCmp (StatusString, "okay") == 0) {
          break;
        } else {
          Status = EFI_NOT_FOUND;
        }
      }
    }

    SearchNodeOffset = FdtNextNode (DeviceTree, SearchNodeOffset, NULL);
  }

  if (!EFI_ERROR (Status)) {
    *NodeOffset = SearchNodeOffset;
  }

  return Status;
}

/**
  Get the count of node with at least one compatible property.

  The status property is checked and if present needs to be "okay"

  @param [in]  CompatibleInfo   Pointer to an array of compatible strings.
                                Array is terminated with a NULL entry.
  @param [out] NodeCount        Number of matching nodes

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetCompatibleNodeCount (
  IN  CONST CHAR8  **CompatibleInfo,
  OUT UINT32       *NodeCount
  )
{
  EFI_STATUS  Status;
  INT32       NodeOffset;

  if (NodeCount == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *NodeCount = 0;
  NodeOffset = -1;

  do {
    Status = DeviceTreeGetNextCompatibleNode (CompatibleInfo, &NodeOffset);
    if (!EFI_ERROR (Status)) {
      (*NodeCount)++;
    }
  } while (!EFI_ERROR (Status));

  if (Status == EFI_NOT_FOUND) {
    if (*NodeCount == 0) {
      Status = EFI_NOT_FOUND;
    } else {
      Status = EFI_SUCCESS;
    }
  }

  return Status;
}

/**
  Get the next node with matching device_type node

  Use of the property is deprecated, and it should be included only on
  cpu and memory nodes for compatibility with IEEE 1275–derived devicetrees.

  The Device tree is traversed in a depth-first search, starting from Node.
  The input Node is skipped.
  The status property is checked and if present needs to be "okay"

  @param [in]  DeviceType       Device type to match.
  @param [in, out]  NodeOffset  At entry: Node offset to start the search.
                                          This first node is skipped.
                                          Write (-1) to search the whole tree.
                                At exit:  If success, contains the offset of
                                          the next node in the branch
                                          being compatible.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
STATIC
EFI_STATUS
EFIAPI
DeviceTreeGetNextDeviceTypeNode (
  IN      CONST CHAR8  *DeviceType,
  IN OUT        INT32  *NodeOffset
  )
{
  EFI_STATUS   Status;
  INT32        SearchNodeOffset;
  VOID         *DeviceTree;
  CONST CHAR8  *PropertyString;
  CONST CHAR8  *StatusString;

  if ((DeviceType == NULL) || (NodeOffset == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  SearchNodeOffset = FdtNextNode (DeviceTree, *NodeOffset, NULL);

  while (SearchNodeOffset >= 0) {
    Status = DeviceTreeGetNodeProperty (
               SearchNodeOffset,
               "device_type",
               (CONST VOID **)&PropertyString,
               NULL
               );
    if (!EFI_ERROR (Status)) {
      if (AsciiStrCmp (DeviceType, PropertyString) == 0) {
        // Compatible node found, check for status node, if defined needs to be "okay"
        Status = DeviceTreeGetNodeProperty (
                   SearchNodeOffset,
                   "status",
                   (CONST VOID **)&StatusString,
                   NULL
                   );
        if (Status == EFI_NOT_FOUND) {
          // Treat missing node as valid
          Status = EFI_SUCCESS;
          break;
        } else if (!EFI_ERROR (Status)) {
          if (AsciiStrCmp (StatusString, "okay") == 0) {
            break;
          } else {
            Status = EFI_NOT_FOUND;
          }
        }
      }
    }

    SearchNodeOffset = FdtNextNode (DeviceTree, SearchNodeOffset, NULL);
  }

  if (!EFI_ERROR (Status)) {
    *NodeOffset = SearchNodeOffset;
  }

  return Status;
}

/**
  Get the next node of device_type = "cpu".

  The status property is checked and if present needs to be "okay"

  @param [in, out]  NodeOffset  At entry: Node offset to start the search.
                                          This first node is skipped.
                                          Write (-1) to search the whole tree.
                                At exit:  If success, contains the offset of
                                          the next node in the branch
                                          being compatible.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetNextCpuNode (
  IN OUT        INT32  *NodeOffset
  )
{
  return DeviceTreeGetNextDeviceTypeNode ("cpu", NodeOffset);
}

/**
  Get the count of nodes with of device_type = "cpu"

  The status property is checked and if present needs to be "okay"

  @param [out] NodeCount        Number of matching nodes

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetCpuNodeCount (
  OUT UINT32  *NodeCount
  )
{
  EFI_STATUS  Status;
  INT32       NodeOffset;

  if (NodeCount == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *NodeCount = 0;
  NodeOffset = -1;

  do {
    Status = DeviceTreeGetNextCpuNode (&NodeOffset);
    if (!EFI_ERROR (Status)) {
      (*NodeCount)++;
    }
  } while (!EFI_ERROR (Status));

  if (Status == EFI_NOT_FOUND) {
    if (*NodeCount == 0) {
      Status = EFI_NOT_FOUND;
    } else {
      Status = EFI_SUCCESS;
    }
  }

  return Status;
}

/**
  Get the next node of device_type = "memory".

  The Device tree is traversed in a depth-first search, starting from Node.
  The input Node is skipped.
  The status property is checked and if present needs to be "okay"

  @param [in, out]  NodeOffset  At entry: Node offset to start the search.
                                          This first node is skipped.
                                          Write (-1) to search the whole tree.
                                At exit:  If success, contains the offset of
                                          the next node in the branch
                                          being compatible.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetNextMemoryNode (
  IN OUT        INT32  *NodeOffset
  )
{
  return DeviceTreeGetNextDeviceTypeNode ("memory", NodeOffset);
}

/**
  Get the count of nodes with of device_type = "memory"

  The status property is checked and if present needs to be "okay"

  @param [out] NodeCount        Number of matching nodes

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching node found.
**/
EFI_STATUS
EFIAPI
DeviceTreeGetMemoryNodeCount (
  OUT UINT32  *NodeCount
  )
{
  EFI_STATUS  Status;
  INT32       NodeOffset;

  if (NodeCount == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *NodeCount = 0;
  NodeOffset = -1;

  do {
    Status = DeviceTreeGetNextMemoryNode (&NodeOffset);
    if (!EFI_ERROR (Status)) {
      (*NodeCount)++;
    }
  } while (!EFI_ERROR (Status));

  if (Status == EFI_NOT_FOUND) {
    if (*NodeCount == 0) {
      Status = EFI_NOT_FOUND;
    } else {
      Status = EFI_SUCCESS;
    }
  }

  return Status;
}

/**
  Returns the node with the specified phandle

  @param[in]  NodePHandle - DTB PHandle to search for
  @param[out] NodeOffset  - Node offset of the matching node

  @retval EFI_SUCCESS           - Nodes located
  @retval EFI_NOT_FOUND         - Node not found
  @retval EFI_INVALID_PARAMETER - NodeOffset is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodeByPHandle (
  IN UINT32  NodePHandle,
  OUT INT32  *NodeOffset
  )
{
  EFI_STATUS  Status;
  INT32       SearchNodeOffset;
  VOID        *DeviceTree;
  UINT32      SearchNodePHandle;

  if (NodeOffset == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  SearchNodeOffset = FdtNextNode (DeviceTree, -1, NULL);

  while (SearchNodeOffset >= 0) {
    Status = DeviceTreeGetNodePHandle (SearchNodeOffset, &SearchNodePHandle);
    if (!EFI_ERROR (Status)) {
      if (SearchNodePHandle == NodePHandle) {
        break;
      } else {
        Status = EFI_NOT_FOUND;
      }
    }

    SearchNodeOffset = FdtNextNode (DeviceTree, SearchNodeOffset, NULL);
  }

  *NodeOffset = SearchNodeOffset;
  return Status;
}

/**
  Returns the specified node's phandle

  @param[in]  NodeOffset  - Node offset
  @param[out] NodePHandle - DTB PHandle of the node

  @retval EFI_SUCCESS           - PHandle returned
  @retval EFI_NOT_FOUND         - Node does not have a phandle
  @retval EFI_INVALID_PARAMETER - NodePHandle is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodePHandle (
  IN  INT32   NodeOffset,
  OUT UINT32  *NodePHandle
  )
{
  EFI_STATUS  Status;

  if (NodePHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "phandle", NodePHandle);
  if (EFI_ERROR (Status)) {
    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "linux,phandle", NodePHandle);
  }

  return Status;
}

/**
  Returns the node with the specified path

  @param[in]  NodePath    - Path to the node
  @param[out] NodeOffset  - Node offset of the matching node

  @retval EFI_SUCCESS           - Nodes located
  @retval EFI_NOT_FOUND         - Node not found
  @retval EFI_INVALID_PARAMETER - NodePath is NULL
  @retval EFI_INVALID_PARAMETER - NodeOffset is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodeByPath (
  IN CONST CHAR8  *NodePath,
  OUT INT32       *NodeOffset
  )
{
  EFI_STATUS   Status;
  VOID         *DeviceTree;
  INT32        AliasOffset;
  CONST CHAR8  *AliasPath;
  CONST CHAR8  *CurrentPath;
  INT32        CurrentOffset;
  CONST CHAR8  *NodeEnd;
  CHAR8        AliasName[DEVICE_TREE_MAX_NAME_LENGTH];

  if ((NodePath == NULL) || (NodeOffset == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  CurrentOffset = 0;
  CurrentPath   = NodePath;

  // If the path doesn't start with / it is an alias
  if (*CurrentPath != '/') {
    // Get the alias node offset
    Status = DeviceTreeGetNodeByPath ("/aliases", &AliasOffset);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    NodeEnd = AsciiStrStr (CurrentPath, "/");
    if (NodeEnd == NULL) {
      NodeEnd = CurrentPath + AsciiStrLen (CurrentPath);
    }

    AsciiStrnCpyS (AliasName, DEVICE_TREE_MAX_NAME_LENGTH, CurrentPath, NodeEnd-CurrentPath);

    Status = DeviceTreeGetNodeProperty (AliasOffset, AliasName, (CONST VOID **)&AliasPath, NULL);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = DeviceTreeGetNodeByPath (AliasPath, &CurrentOffset);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    CurrentPath = NodeEnd;
  }

  while (*CurrentPath != '\0') {
    if (*CurrentPath == '/') {
      CurrentPath++;
      continue;
    }

    NodeEnd = AsciiStrStr (CurrentPath, "/");
    if (NodeEnd == NULL) {
      NodeEnd = CurrentPath + AsciiStrLen (CurrentPath);
    }

    CurrentOffset = FdtSubnodeOffsetNameLen (DeviceTree, CurrentOffset, CurrentPath, NodeEnd - CurrentPath);
    if (CurrentOffset < 0) {
      return EFI_NOT_FOUND;
    }

    CurrentPath = NodeEnd;
  }

  *NodeOffset = CurrentOffset;
  return EFI_SUCCESS;
}

/**
  Returns the specified node's path

  @param[in]      NodeOffset    - Node offset
  @param[out]     NodePath      - A pointer to a buffer allocated by this function with
                                  AllocatePool() to store the path.If this function
                                  returns EFI_SUCCESS, it stores the path the caller
                                  wants to get. The caller should release the string
                                  buffer with FreePool() after the path is not used any more.
  @param[out]     NodePathSize  - On output size of the path string.

  @retval EFI_SUCCESS           - Path returned
  @retval EFI_INVALID_PARAMETER - NodePath is NULL
  @retval EFI_OUT_OF_RESOURCES  - There are not enough resources to allocate the return buffer NodePath.
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodePath (
  IN  INT32   NodeOffset,
  OUT CHAR8   **NodePath,
  OUT UINT32  *NodePathSize OPTIONAL
  )
{
  EFI_STATUS   Status;
  VOID         *DeviceTree;
  CONST CHAR8  *Name;
  INT32        Index;
  INT32        LocalNodeArray[GET_NODE_HIERARCHY_DEPTH_GUESS];
  INT32        *NodeArray;
  UINT32       NodeDepth;
  UINT32       SizeNeeded;
  CHAR8        *NewNodePath;
  INT32        NodeLength;

  if (NodePath == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  NewNodePath = NULL;

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  NodeDepth = 0;
  NodeArray = LocalNodeArray;
  Status    = GetNodeHierarchyInfo (DeviceTree, NodeOffset, NodeArray, GET_NODE_HIERARCHY_DEPTH_GUESS, &NodeDepth);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    NodeArray = (INT32 *)AllocatePool (sizeof (INT32) * NodeDepth);
    if (NodeArray == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    Status = GetNodeHierarchyInfo (DeviceTree, NodeOffset, NodeArray, NodeDepth, &NodeDepth);
  }

  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  SizeNeeded = 1;   // NUL termination
  for (Index = 0; Index < NodeDepth; Index++) {
    Name = FdtGetName (DeviceTree, NodeArray[Index], &NodeLength);
    if ((Name == NULL) || (NodeLength <= 0)) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    // "/" and string length
    SizeNeeded += 1 + NodeLength;
  }

  NewNodePath = (CHAR8 *)AllocateZeroPool (SizeNeeded);
  if (NewNodePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  for (Index = 0; Index < NodeDepth; Index++) {
    Status = AsciiStrCatS (NewNodePath, SizeNeeded, "/");
    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    Name = FdtGetName (DeviceTree, NodeArray[Index], &NodeLength);
    if (Name == NULL) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    Status = AsciiStrCatS (NewNodePath, SizeNeeded, Name);
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  *NodePath = NewNodePath;
  if (NodePathSize != NULL) {
    *NodePathSize = SizeNeeded;
  }

Exit:
  if (NodeArray != LocalNodeArray) {
    FreePool (NodeArray);
  }

  if (EFI_ERROR (Status)) {
    if (NewNodePath != NULL) {
      FreePool (NewNodePath);
    }
  }

  return Status;
}

/**
  Returns the specified property data

  @param[in]      NodeOffset    - Node offset
  @param[in]      Property      - Property name
  @param[out]     PropertyData  - Data of the property
  @param[in,out]  PropertySize  - Size of the property node.

  @retval EFI_SUCCESS           - Property returned
  @retval EFI_NOT_FOUND         - Property is not present in node
  @retval EFI_INVALID_PARAMETER - Property is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodeProperty (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *Property,
  OUT CONST VOID   **PropertyData OPTIONAL,
  OUT UINT32       *PropertySize OPTIONAL
  )
{
  EFI_STATUS          Status;
  VOID                *DeviceTree;
  CONST FDT_PROPERTY  *PropertyValue;
  INT32               InternalPropertySize;

  if (Property == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  PropertyValue = FdtGetProperty (DeviceTree, NodeOffset, Property, &InternalPropertySize);
  if (PropertyValue == NULL) {
    return EFI_NOT_FOUND;
  }

  if (PropertyData != NULL) {
    *PropertyData = PropertyValue->Data;
  }

  if (InternalPropertySize < 0) {
    return EFI_DEVICE_ERROR;
  }

  if (PropertySize != NULL) {
    *PropertySize = InternalPropertySize;
  }

  return EFI_SUCCESS;
}

/**
  Returns the uint64 value of the property

  Uses the size of the acutual property node to read the data and converts
  endian to system type from stored big endian

  @param[in]      NodeOffset    - Node offset
  @param[in]      Property      - Property name
  @param[out]     PropertyValue - Value of the property

  @retval EFI_SUCCESS           - Property returned
  @retval EFI_NOT_FOUND         - Property is not present in node
  @retval EFI_BAD_BUFFER_SIZE   - Property did not have a size that could be
                                  converted to UINT64
  @retval EFI_INVALID_PARAMETER - Property is NULL
  @retval EFI_INVALID_PARAMETER - PropertyValue is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodePropertyValue64 (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *Property,
  OUT UINT64       *PropertyValue
  )
{
  EFI_STATUS  Status;
  CONST VOID  *PropertyData;
  UINT32      PropertySize;

  if ((Property == NULL) || (PropertyValue == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = DeviceTreeGetNodeProperty (NodeOffset, Property, &PropertyData, &PropertySize);
  if (!EFI_ERROR (Status)) {
    if (PropertySize == sizeof (UINT32)) {
      *PropertyValue = Fdt32ToCpu (ReadUnaligned32 ((UINT32 *)PropertyData));
    } else if (PropertySize == sizeof (UINT64)) {
      *PropertyValue = Fdt64ToCpu (ReadUnaligned64 ((UINT64 *)PropertyData));
    } else {
      Status = EFI_BAD_BUFFER_SIZE;
    }
  }

  return Status;
}

/**
  Returns the uint32 value of the property

  Uses the size of the acutual property node to read the data and converts
  endian to system type from stored big endian

  @param[in]      NodeOffset    - Node offset
  @param[in]      Property      - Property name
  @param[out]     PropertyValue - Value of the property

  @retval EFI_SUCCESS           - Property returned
  @retval EFI_NOT_FOUND         - Property is not present in node
  @retval EFI_BAD_BUFFER_SIZE   - Property did not have a size that could be
                                  converted to UINT32
  @retval EFI_NO_MAPPING        - Value was stored as a 64 bit in DTB but is
                                  greated than MAX_UINT32

  @retval EFI_INVALID_PARAMETER - Property is NULL
  @retval EFI_INVALID_PARAMETER - PropertyValue is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeGetNodePropertyValue32 (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *Property,
  OUT UINT32       *PropertyValue
  )
{
  EFI_STATUS  Status;
  UINT64      LargeValue;

  if ((Property == NULL) || (PropertyValue == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = DeviceTreeGetNodePropertyValue64 (NodeOffset, Property, &LargeValue);

  if (!EFI_ERROR (Status)) {
    if (LargeValue > MAX_UINT32) {
      Status = EFI_NO_MAPPING;
    } else {
      *PropertyValue = (UINT32)LargeValue;
    }
  }

  return Status;
}

/**
  Locates the matching string in a string list property.

  @param[in]      NodeOffset    - Node offset
  @param[in]      Property      - Property name
  @param[in]      String        - String to match
  @param[out]     Index         - Index of the string to match

  @retval EFI_SUCCESS           - Property returned
  @retval EFI_NO_MAPPING        - Property is not present in node
  @retval EFI_NOT_FOUND         - String is not found in the property string list
  @retval EFI_INVALID_PARAMETER - Property is NULL
  @retval EFI_INVALID_PARAMETER - String is NULL
  @retval EFI_INVALID_PARAMETER - Index is NULL
  @retval EFI_DEVICE_ERROR      - Other Errors

**/
EFI_STATUS
EFIAPI
DeviceTreeLocateStringIndex (
  IN INT32        NodeOffset,
  IN CONST CHAR8  *Property,
  IN CONST CHAR8  *String,
  OUT UINT32      *Index
  )
{
  EFI_STATUS   Status;
  CONST VOID   *PropertyData;
  UINT32       PropertySize;
  CONST CHAR8  *StringSearch;
  UINT32       MatchSize;

  if ((Property == NULL) || (String == NULL) || (Index == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  MatchSize = AsciiStrSize (String);

  *Index = 0;
  Status = DeviceTreeGetNodeProperty (NodeOffset, Property, &PropertyData, &PropertySize);
  if (Status == EFI_NOT_FOUND) {
    Status = EFI_NO_MAPPING;
  } else if (!EFI_ERROR (Status)) {
    Status       = EFI_NOT_FOUND;
    StringSearch = (CONST CHAR8 *)PropertyData;
    while ((CONST VOID *)(StringSearch + MatchSize) <= (PropertyData + PropertySize)) {
      if (AsciiStrCmp (String, StringSearch) == 0) {
        Status = EFI_SUCCESS;
        break;
      }

      (*Index)++;
      StringSearch += AsciiStrSize (StringSearch);
    }
  }

  return Status;
}
