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
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>

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
  EFI_STATUS   Status;
  CONST CHAR8  *CompatibleInfo[2];
  UINT32       NodeCount;
  INT32        NodeOffset;
  VOID         *DeviceTree;
  UINT32       Index;

  Status = GetDeviceTreePointer (&DeviceTree, NULL);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if ((CompatibleString == NULL) || (NumberOfNodes == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((*NumberOfNodes != 0) && (NodeHandleArray == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  CompatibleInfo[0] = CompatibleString;
  CompatibleInfo[1] = NULL;

  Status = DeviceTreeGetCompatibleNodeCount (CompatibleInfo, &NodeCount);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NodeOffset = -1;
  Index      = 0;
  while (DeviceTreeGetNextCompatibleNode (CompatibleInfo, &NodeOffset) != EFI_NOT_FOUND) {
    if (Index >= *NumberOfNodes) {
      break;
    }

    Status = GetDeviceTreeHandle (DeviceTree, NodeOffset, &NodeHandleArray[Index]);
    Index++;
    if (EFI_ERROR (Status)) {
      break;
    }
  }

  if (*NumberOfNodes < NodeCount) {
    Status = EFI_BUFFER_TOO_SMALL;
  }

  *NumberOfNodes = NodeCount;

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
  INT32       NodeOffset;

  if ((KernelStart == NULL) || (KernelDtbStart == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = DeviceTreeGetNodeByPath (
             "/chosen",
             &NodeOffset
             );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceTreeGetNodePropertyValue64 (NodeOffset, "kernel-start", KernelStart);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceTreeGetNodePropertyValue64 (NodeOffset, "kernel-dtb-start", KernelDtbStart);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
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

  Status = GetDeviceTreeNode (Handle, &DeviceTree, &NodeOffset);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return DeviceTreeGetRegisters (NodeOffset, RegisterArray, NumberOfRegisters);
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
  EFI_STATUS  Status;
  VOID        *DeviceTree;
  INT32       NodeOffset;

  Status = GetDeviceTreeNode (Handle, &DeviceTree, &NodeOffset);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return DeviceTreeGetInterrupts (NodeOffset, InterruptArray, NumberOfInterrupts);
}
