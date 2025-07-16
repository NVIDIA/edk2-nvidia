/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/NVIDIADebugLib.h>
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
  NV_ASSERT_RETURN ((DeviceTreeSize != 0) || (DeviceTree == NULL), return , "%a: size=0\n", __FUNCTION__);
  NV_ASSERT_RETURN (
    (LocalDeviceTree == NULL) || (LocalDeviceTree == DeviceTree) || (DeviceTree == NULL),
    return ,
    "%a: set would switch tree from 0x%p to 0x%p\n",
    __FUNCTION__,
    LocalDeviceTree,
    DeviceTree
    );

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
  if ((LocalDeviceTree == NULL)) {
    Status = DtPlatformLoadDtb (&LocalDeviceTree, &LocalDeviceTreeSize);
  }

  if (!EFI_ERROR (Status)) {
    NV_ASSERT_RETURN (LocalDeviceTreeSize != 0, return EFI_DEVICE_ERROR, "%a: size=0\n", __FUNCTION__);

    *DeviceTree = LocalDeviceTree;
    if (DeviceTreeSize != NULL) {
      *DeviceTreeSize = LocalDeviceTreeSize;
    }
  }

  return Status;
}
