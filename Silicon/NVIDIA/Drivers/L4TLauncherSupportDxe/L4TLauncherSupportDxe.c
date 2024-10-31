/** @file

  L4T Launcher Support DXE driver.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright 1999 - 2021 Intel Corporation. <BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Base.h"
#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformBootOrderLib.h>
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
  if (HeaderSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *HeaderSize =  SIZE_8KB;

  return EFI_SUCCESS;
}

/**
  Gets the boot mode type and associated metadata.

  @param[out] BootModeInfo  Boot mode info for the platform

  @retval EFI_SUCCESS             The header size was successfully read
  @retval EFI_INVALID_PARAMETER   BootModeInfo is NULL
  @retval EFI_NOT_FOUND           Boot mode and metadata is not found
**/
EFI_STATUS
EFIAPI
GetBootModeInfo (
  TEGRA_BOOT_MODE_METADATA  *BootModeInfo
  )
{
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  if (BootModeInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    return EFI_NOT_FOUND;
  }

  BootModeInfo->BootType = PlatformResourceInfo->BootType;
  if (BootModeInfo->BootType == TegrablBootRcm) {
    BootModeInfo->RcmBootOsInfo.Base = PcdGet64 (PcdRcmKernelBase);
    BootModeInfo->RcmBootOsInfo.Size = PcdGet64 (PcdRcmKernelSize);
  }

  return EFI_SUCCESS;
}

L4T_LAUNCHER_SUPPORT_PROTOCOL  mL4TLauncherSupport = {
  GetRootfsStatusReg,
  SetRootfsStatusReg,
  GetBootDeviceClass,
  GetBootComponentHeaderSize,
  ApplyTegraDeviceTreeOverlay,
  GetBootModeInfo
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
