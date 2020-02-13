/** @file
  Implementation for PlatformBootManagerBootDescriptionLib library class interfaces.

  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>

#define PLATFORM_BOOT_MANAGER_BOOT_DESCRIPTION_GUID \
  { \
  0x0796b119, 0x3799, 0x4e6f, {0xb7, 0x36, 0xa4, 0x20, 0xda, 0x35, 0xcf, 0x5a} \
  }

EFI_GUID mPlatformBootManagerDeviceDescriptionGuid = PLATFORM_BOOT_MANAGER_BOOT_DESCRIPTION_GUID;
EFI_HII_HANDLE mHiiHandle                          = NULL;

/**
  Return the platform provided boot option description for the controller.

  @param Handle                Controller handle.
  @param DefaultDescription    Default boot description provided by core.

  @return  The callee allocated description string
           or NULL if the handler wants to use DefaultDescription.
**/
CHAR16 *
EFIAPI PlatformLoadFileBootDescriptionHandler (
  IN EFI_HANDLE                  Handle,
  IN CONST CHAR16                *DefaultDescription
  )
{
  EFI_STATUS               Status;
  VOID                     *Interface;
  EFI_DEVICE_PATH_PROTOCOL *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL *CurrentDevicePath;
  CHAR16                   *DescriptionString;
  UINT32                   DescriptionStringLength;
  CHAR16                   *Desc;

  Status = gBS->HandleProtocol (Handle,
                                &gEfiLoadFileProtocolGuid,
                                &Interface);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  DevicePath = DevicePathFromHandle (Handle);
  if (DevicePath == NULL) {
    return NULL;
  }

  DescriptionString = NULL;
  Desc = NULL;
  CurrentDevicePath = DevicePath;
  while (IsDevicePathEnd (CurrentDevicePath) == FALSE) {
    if (CurrentDevicePath->SubType == MSG_EMMC_DP) {
      DescriptionString = HiiGetString(mHiiHandle, STRING_TOKEN(STR_LOAD_FILE_EMMC_BOOT_DESCRIPTION), NULL);
      break;
    } else if (CurrentDevicePath->SubType == HW_VENDOR_DP) {
      VENDOR_DEVICE_PATH *VendorPath = (VENDOR_DEVICE_PATH *)CurrentDevicePath;
      if (CompareGuid (&VendorPath->Guid, &gNVIDIARamloadKernelGuid)) {
        DescriptionString = HiiGetString(mHiiHandle, STRING_TOKEN(STR_LOAD_FILE_RAMLOADED_BOOT_DESCRIPTION), NULL);
        break;
      }
    }
    CurrentDevicePath = NextDevicePathNode (CurrentDevicePath);
  }

  if (DescriptionString != NULL) {
    DescriptionStringLength = (StrLen (DescriptionString) + 1) * sizeof (CHAR16);
    Status = gBS->AllocatePool (EfiBootServicesData,
                                DescriptionStringLength,
                                (VOID **)&Desc);
    if (EFI_ERROR (Status)) {
      return NULL;
    }
    gBS->CopyMem (Desc, DescriptionString, DescriptionStringLength);
  }

  return Desc;
}

/**

  Initialize Boot Manager Platform Description.

  @param ImageHandle     The image handle.
  @param SystemTable     The system table.

  @retval  EFI_SUCEESS  Install Boot manager menu success.
  @retval  Other        Return error status.

**/
EFI_STATUS
EFIAPI
BootManagerBootDescriptionLibConstructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  mHiiHandle = HiiAddPackages (&mPlatformBootManagerDeviceDescriptionGuid,
                               ImageHandle,
                               PlatformBootManagerBootDescriptionLibStrings,
                               NULL);
  if (mHiiHandle == NULL) {
    return EFI_UNSUPPORTED;
  }

  return EfiBootManagerRegisterBootDescriptionHandler (PlatformLoadFileBootDescriptionHandler);
}

/**
  Destructor for the library.

  @param[in]  ImageHandle       Handle that identifies the image to be unloaded.
  @param[in]  SystemTable       System Table

  @retval EFI_SUCCESS           The image has been unloaded.
**/
EFI_STATUS
EFIAPI
BootManagerBootDescriptionLibDestructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
