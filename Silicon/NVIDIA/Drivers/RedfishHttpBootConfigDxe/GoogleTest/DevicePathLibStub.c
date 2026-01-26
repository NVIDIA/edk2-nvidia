/** @file
  Stub implementation of DevicePathLib functions for unit tests.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Protocol/DevicePath.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

//
// Stub implementations of DevicePathLib functions
//

/**
  Sets the length of a device path node.

  @param[in] Node   Device path node
  @param[in] Length Length to set

  @retval Length value set

**/
UINT16
EFIAPI
SetDevicePathNodeLength (
  IN OUT VOID   *Node,
  IN     UINTN  Length
  )
{
  ((EFI_DEVICE_PATH_PROTOCOL *)Node)->Length[0] = (UINT8)(Length);
  ((EFI_DEVICE_PATH_PROTOCOL *)Node)->Length[1] = (UINT8)((Length) >> 8);
  return (UINT16)Length;
}

/**
  Returns the length of a device path node.

  @param[in] Node   Device path node

  @retval Length of the node

**/
UINTN
EFIAPI
DevicePathNodeLength (
  IN CONST VOID  *Node
  )
{
  return ((EFI_DEVICE_PATH_PROTOCOL *)Node)->Length[0] |
         (((EFI_DEVICE_PATH_PROTOCOL *)Node)->Length[1] << 8);
}

//
// Global variables to control stub behavior
//
static EFI_DEVICE_PATH_PROTOCOL  *gStubDevicePath                     = NULL;
static EFI_DEVICE_PATH_PROTOCOL  *gStubAppendDevicePathNodeResult     = NULL;
static BOOLEAN                   gStubAppendDevicePathNodeResultIsSet = FALSE;
static UINTN                     gStubAppendCallCount                 = 0;
static UINTN                     gStubAppendFailOnCall                = 0;

/**
  Set the device path that DevicePathFromHandle should return.

  @param[in]  DevicePath  Device path to return, or NULL to return NULL

**/
VOID
SetStubDevicePath (
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  gStubDevicePath = DevicePath;
}

/**
  Set the result that AppendDevicePathNode should return.

  @param[in]  DevicePath  Device path to return, or NULL to simulate allocation failure

**/
VOID
SetStubAppendDevicePathNodeResult (
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  gStubAppendDevicePathNodeResult      = DevicePath;
  gStubAppendDevicePathNodeResultIsSet = TRUE;
}

/**
  Clear the configured result for AppendDevicePathNode.
  After this, AppendDevicePathNode will use its default behavior.

**/
VOID
ClearStubAppendDevicePathNodeResult (
  VOID
  )
{
  gStubAppendDevicePathNodeResult      = NULL;
  gStubAppendDevicePathNodeResultIsSet = FALSE;
  gStubAppendCallCount                 = 0;
  gStubAppendFailOnCall                = 0;
}

/**
  Configure AppendDevicePathNode to fail on a specific call number.

  @param[in]  CallNumber  Call number to fail on (1-based), or 0 to disable

**/
VOID
SetStubAppendDevicePathNodeFailOnCall (
  UINTN  CallNumber
  )
{
  gStubAppendCallCount  = 0;
  gStubAppendFailOnCall = CallNumber;
}

/**
  Stub implementation of DevicePathFromHandle.

  Returns the device path set by SetStubDevicePath().

  @param[in]  Handle  Handle (ignored in stub)

  @retval Device path set by SetStubDevicePath()

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
DevicePathFromHandle (
  IN EFI_HANDLE  Handle
  )
{
  return gStubDevicePath;
}

/**
  Stub implementation of AppendDevicePathNode.

  Returns the device path set by SetStubAppendDevicePathNodeResult().
  If not set, allocates a simple device path for testing.

  @param[in]  DevicePath  Base device path (can be NULL)
  @param[in]  DevicePathNode  Node to append

  @retval Device path set by SetStubAppendDevicePathNodeResult(), or a test device path

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
AppendDevicePathNode (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath      OPTIONAL,
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePathNode  OPTIONAL
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *Result;

  // Track call count and fail on specific call if configured
  gStubAppendCallCount++;
  if ((gStubAppendFailOnCall > 0) && (gStubAppendCallCount == gStubAppendFailOnCall)) {
    return NULL;
  }

  // If a specific result is configured, return it (even if NULL)
  if (gStubAppendDevicePathNodeResultIsSet) {
    return gStubAppendDevicePathNodeResult;
  }

  // Otherwise, allocate a simple test device path
  // Include space for the node plus an end node
  UINTN  NodeSize  = DevicePathNodeLength (DevicePathNode);
  UINTN  TotalSize = NodeSize + sizeof (EFI_DEVICE_PATH_PROTOCOL);

  Result = AllocateZeroPool (TotalSize);
  if (Result == NULL) {
    return NULL;
  }

  // Copy the node
  CopyMem (Result, DevicePathNode, NodeSize);

  // Add end node
  EFI_DEVICE_PATH_PROTOCOL  *EndNode = (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)Result + NodeSize);

  EndNode->Type    = END_DEVICE_PATH_TYPE;
  EndNode->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
  SetDevicePathNodeLength (EndNode, sizeof (EFI_DEVICE_PATH_PROTOCOL));

  return Result;
}
