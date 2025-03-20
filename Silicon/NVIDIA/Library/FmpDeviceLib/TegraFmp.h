/** @file

  Tegra Firmware Management Protocol support

  SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __TEGRA_FMP_H__
#define __TEGRA_FMP_H__

#include <Uefi/UefiBaseType.h>
#include <Library/FmpParamLib.h>
#include <Library/FmpVersionLib.h>
#include <Library/FmpDeviceLib.h>
#include <Protocol/FirmwareManagement.h>

/**
  Check if a given capsule image is suitable to perform a FW update.

  @param[in]  Image                 Pointer to the image
  @param[in]  ImageSize             Number of bytes in image
  @param[out] ImageUpdatable        Pointer to store the image updateable result
  @param[out] LastAttemptStatus     Pointer to store the last attempt status

  @retval EFI_SUCCESS               The image is suitable.
  @retval Others                    An error occurred.  If not NULL,
                                    ImageUpdateable and LastAttemptStatus
                                    contain additional error information.

**/
EFI_STATUS
EFIAPI
FmpTegraCheckImage (
  IN  CONST VOID  *Image,
  IN  UINTN       ImageSize,
  OUT UINT32      *ImageUpdatable,
  OUT UINT32      *LastAttemptStatus
  );

/**
  Set the given capsule image into the system FW partitions.

  @param[in]  Image             Pointer to the image
  @param[in]  ImageSize         Number of bytes in image
  @param[in]  VendorCode        Pointer to optional vendor-specific update
                                policy code in capsule
  @param[in]  Progress          Progress function pointer
  @param[in]  CapsuleFwVersion  New FW version from capsule FMP payload header
  @param[out] AbortReason       If not NULL, pointer to null-terminated abort
                                reason string which must be freed by caller
                                using FreePool().
  @param[out] LastAttemptStatus Pointer to store the last attempt status

  @retval EFI_SUCCESS           The FW was updated successfully.
  @retval Others                An error occurred.  If not NULL,
                                LastAttemptStatus contains additional error
                                information.

**/
EFI_STATUS
EFIAPI
FmpTegraSetImage (
  IN  CONST VOID *Image,
  IN  UINTN ImageSize,
  IN  CONST VOID *VendorCode, OPTIONAL
  IN  EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  Progress, OPTIONAL
  IN  UINT32                                         CapsuleFwVersion,
  OUT CHAR16                                         **AbortReason,
  OUT UINT32                                         *LastAttemptStatus
  );

/**
  Register FmpDxe installer function.

  @param[in]  Function              Installer function pointer.

  @retval EFI_SUCCESS               The installer was registered successfully.
  @retval EFI_UNSUPPORTED           The FMP library is already initialized, FmpDxe
                                    should continue without installer callback.
**/
EFI_STATUS
EFIAPI
FmpTegraRegisterInstaller (
  IN FMP_DEVICE_LIB_REGISTER_FMP_INSTALLER  Function
  );

/**
  Register FmpDxe uninstaller function.

  @param[in]  Function              Uninstaller function pointer.

  @retval EFI_SUCCESS               The uninstaller was registered successfully.
  @retval EFI_UNSUPPORTED           The FMP library is already initialized, FmpDxe
                                    should continue without uninstaller callback.
**/
EFI_STATUS
EFIAPI
FmpTegraRegisterUninstaller (
  IN FMP_DEVICE_LIB_REGISTER_FMP_UNINSTALLER  Function
  );

#endif
