/** @file

  L4T Launcher Support DXE driver.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright 1999 - 2021 Intel Corporation. <BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Base.h"
#include <Library/BaseLib.h>
#include <Library/PlatformBootOrderLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraDeviceTreeOverlayLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/L4TLauncherSupportProtocol.h>

/**
  Gets the size of the boot component headers for the platform

  @param[out] HeaderSize  The size of the boot component headers

  @retval EFI_SUCCESS             The header size was successfully read
  @retval EFI_INVALID_PARAMETER   HeaderSize is NULL
  @retval EFI_UNSUPPORTED         The header size is not supported
**/
EFI_STATUS
EFIAPI
GetBootComponentHeaderSize (
  OUT UINTN  *HeaderSize
  )
{
  UINTN  ChipID;

  if (HeaderSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ChipID = TegraGetChipID ();
  if (ChipID == T194_CHIP_ID) {
    *HeaderSize = SIZE_4KB;
  } else {
    *HeaderSize =  SIZE_8KB;
  }

  return EFI_SUCCESS;
}

L4T_LAUNCHER_SUPPORT_PROTOCOL  mL4TLauncherSupport = {
  GetRootfsStatusReg,
  SetRootfsStatusReg,
  GetBootDeviceClass,
  GetBootComponentHeaderSize,
  ApplyTegraDeviceTreeOverlay
};

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  @param[in]  ImageHandle       The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.

**/
EFI_STATUS
EFIAPI
L4TLauncherSupportDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return gBS->InstallMultipleProtocolInterfaces (
                &ImageHandle,
                &gNVIDIAL4TLauncherSupportProtocol,
                &mL4TLauncherSupport,
                NULL
                );
}
