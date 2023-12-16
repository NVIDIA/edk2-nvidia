/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _PLATFORM_BOOT_ORDER_LIB_H_
#define _PLATFORM_BOOT_ORDER_LIB_H_

VOID
EFIAPI
SetBootOrder (
  VOID
  );

VOID
PrintCurrentBootOrder (
  IN CONST UINTN  DebugPrintLevel
  );

/**
  Gets the class name of the specified device

  Gets the name of the type of device that is passed in

  @param[in]  FilePath            DevicePath of the device.
  @param[out] DeviceClass         Pointer to a string that describes the device type.

  @retval EFI_SUCCESS             Class name returned.
  @retval EFI_NOT_FOUND           Device type not found.
  @retval EFI_INVALID_PARAMETER   FilePath is NULL.
  @retval EFI_INVALID_PARAMETER   DeviceClass is NULL.
**/
EFI_STATUS
EFIAPI
GetBootDeviceClass (
  IN EFI_DEVICE_PATH_PROTOCOL  *FilePath,
  OUT CONST CHAR8              **DeviceClass
  );

#endif
