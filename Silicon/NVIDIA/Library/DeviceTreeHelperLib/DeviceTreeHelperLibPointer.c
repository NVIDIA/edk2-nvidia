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

STATIC VOID   *LocalDeviceTree    = NULL;
STATIC UINTN  LocalDeviceTreeSize = 0;

/**
  Set the base address and size of the device tree

  This is to support the use cases when the HOB list is not populated.

  @param  DeviceTree        - Pointer to base Address of the device tree.
  @param  DeviceTreeSize    - Pointer to size of the device tree.

**/
VOID
EFIAPI
SetDeviceTreePointer (
  IN  VOID   *DeviceTree,
  IN  UINTN  DeviceTreeSize
  )
{
  LocalDeviceTree     = DeviceTree;
  LocalDeviceTreeSize = DeviceTreeSize;
}

/**
  Get the base address and size of the device tree

  @param[out]  DeviceTree        - Pointer to base Address of the device tree.
  @param[out]  DeviceTreeSize    - Pointer to size of the device tree.

  @retval EFI_SUCCESS           - DeviceTree pointer located
  @retval EFI_INVALID_PARAMETER - DeviceTree is NULL
  @retval EFI_NOT_FOUND         - DeviceTree is not found
**/
EFI_STATUS
EFIAPI
GetDeviceTreePointer (
  OUT VOID   **DeviceTree,
  OUT UINTN  *DeviceTreeSize OPTIONAL
  )
{
  EFI_STATUS  Status;

  if (DeviceTree == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  if ((LocalDeviceTree == NULL) || (LocalDeviceTreeSize == 0)) {
    Status = DtPlatformLoadDtb (&LocalDeviceTree, &LocalDeviceTreeSize);
  }

  if (!EFI_ERROR (Status)) {
    *DeviceTree = LocalDeviceTree;
    if (DeviceTreeSize != NULL) {
      *DeviceTreeSize = LocalDeviceTreeSize;
    }
  }

  return Status;
}
