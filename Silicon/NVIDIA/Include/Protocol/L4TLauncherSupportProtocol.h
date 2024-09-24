/** @file
  NVIDIA L4T Launcher Support Protocol

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef L4T_LAUNCHER_SUPPORT_PROTOCOL_H__
#define L4T_LAUNCHER_SUPPORT_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define L4T_LAUNCHER_SUPPORT_PROTOCOL_GUID \
  { \
  0xb1f8c13e, 0x5fd8, 0x424f, { 0x97, 0x33, 0x84, 0xc6, 0x97, 0x1c, 0xdb, 0xa2 } \
  }

//
// Define for forward reference.
//
typedef struct _L4T_LAUNCHER_SUPPORT_PROTOCOL L4T_LAUNCHER_SUPPORT_PROTOCOL;

/**
  Get Rootfs Status (SR_RF) Register Value

  @param[out] RegisterValue  The value of the Rootfs Status Register

  @retval EFI_SUCCESS             The Rootfs Status Register value was successfully read
  @retval EFI_INVALID_PARAMETER   RegisterValue is NULL
  @retval EFI_UNSUPPORTED         The Rootfs Status Register is not supported
**/
typedef
EFI_STATUS
(EFIAPI *GET_ROOTFS_STATUS_REG)(
  OUT UINT32  *RegisterValue
  );

/**
  Set Rootfs Status (SR_RF) Register

  @param[in] RegisterValue  The value to write to the Rootfs Status Register

  @retval EFI_SUCCESS             The Rootfs Status Register value was successfully written
  @retval EFI_UNSUPPORTED         The Rootfs Status Register is not supported
**/
typedef
EFI_STATUS
(EFIAPI *SET_ROOTFS_STATUS_REG)(
  IN UINT32  RegisterValue
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
typedef
EFI_STATUS
(EFIAPI *GET_BOOT_DEVICE_CLASS)(
  IN EFI_DEVICE_PATH_PROTOCOL  *FilePath,
  OUT CONST CHAR8              **DeviceClass
  );

/**
  Gets the size of the boot component headers for the platform

  @param[out] HeaderSize  The size of the boot component headers

  @retval EFI_SUCCESS             The header size was successfully read
  @retval EFI_INVALID_PARAMETER   HeaderSize is NULL
  @retval EFI_UNSUPPORTED         The header size is not supported
**/
typedef
EFI_STATUS
(EFIAPI *GET_BOOT_COMPONENT_HEADER_SIZE)(
  OUT UINTN  *HeaderSize
  );

/**
  Applies the device tree overlay to the base device tree if the board and
  module match the overlay.

  @param[in] FdtBase     The base device tree
  @param[in] FdtOverlay  The device tree overlay
  @param[in] ModuleStr   The module string to match

  @retval EFI_SUCCESS             The overlay was successfully applied
  @retval EFI_INVALID_PARAMETER   FdtBase is NULL
  @retval EFI_INVALID_PARAMETER   FdtOverlay is NULL
  @retval EFI_INVALID_PARAMETER   ModuleStr is NULL
  @retval EFI_DEVICE_ERROR       The overlay could not be applied
*/
typedef
EFI_STATUS
(EFIAPI *APPLY_TEGRA_DEVICE_TREE_OVERLAY)(
  VOID   *FdtBase,
  VOID   *FdtOverlay,
  CHAR8  *ModuleStr
  );

struct _L4T_LAUNCHER_SUPPORT_PROTOCOL {
  GET_ROOTFS_STATUS_REG              GetRootfsStatusReg;
  SET_ROOTFS_STATUS_REG              SetRootfsStatusReg;
  GET_BOOT_DEVICE_CLASS              GetBootDeviceClass;
  GET_BOOT_COMPONENT_HEADER_SIZE     GetBootComponentHeaderSize;
  APPLY_TEGRA_DEVICE_TREE_OVERLAY    ApplyTegraDeviceTreeOverlay;
};

extern EFI_GUID  gNVIDIAL4TLauncherSupportProtocol;

#endif
