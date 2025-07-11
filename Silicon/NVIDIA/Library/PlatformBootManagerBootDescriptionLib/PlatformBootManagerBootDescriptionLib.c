/** @file
  Implementation for PlatformBootManagerBootDescriptionLib library class interfaces.

  SPDX-FileCopyrightText: Copyright (c) 2020-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/AndroidBcbLib.h>
#include <NVIDIAConfiguration.h>

#define PLATFORM_BOOT_MANAGER_BOOT_DESCRIPTION_GUID \
  { \
  0x0796b119, 0x3799, 0x4e6f, {0xb7, 0x36, 0xa4, 0x20, 0xda, 0x35, 0xcf, 0x5a} \
  }

EFI_GUID        mPlatformBootManagerDeviceDescriptionGuid = PLATFORM_BOOT_MANAGER_BOOT_DESCRIPTION_GUID;
EFI_HII_HANDLE  mHiiHandle                                = NULL;

/**
  Return the platform provided boot option description for the controller.

  @param Handle                Controller handle.
  @param DefaultDescription    Default boot description provided by core.

  @return  The callee allocated description string
           or NULL if the handler wants to use DefaultDescription.
**/
CHAR16 *
EFIAPI
PlatformLoadFileBootDescriptionHandler (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *DefaultDescription
  )
{
  EFI_STATUS                Status;
  VOID                      *Interface;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *CurrentDevicePath;
  CHAR16                    *DescriptionString;
  UINT32                    DescriptionStringLength;
  CHAR16                    *Desc;
  UINTN                     DataSize;
  UINT32                    BootMode;
  MiscCmdType               MiscCmd;
  BOOLEAN                   RecoveryBoot;
  BOOLEAN                   FlashMediaBoot;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiLoadFileProtocolGuid,
                  &Interface
                  );
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  DevicePath = DevicePathFromHandle (Handle);
  if (DevicePath == NULL) {
    return NULL;
  }

  RecoveryBoot = FALSE;
  DataSize     = sizeof (BootMode);
  Status       = gRT->GetVariable (L4T_BOOTMODE_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, NULL, &DataSize, &BootMode);
  if (!EFI_ERROR (Status) && (BootMode == NVIDIA_L4T_BOOTMODE_RECOVERY)) {
    RecoveryBoot = TRUE;
  }

  Status = GetCmdFromMiscPartition (NULL, &MiscCmd, TRUE);
  if (!EFI_ERROR (Status) && ((MiscCmd == MISC_CMD_TYPE_RECOVERY) || (MiscCmd == MISC_CMD_TYPE_FASTBOOT_USERSPACE))) {
    RecoveryBoot = TRUE;
  }

  FlashMediaBoot    = FALSE;
  DescriptionString = NULL;
  Desc              = NULL;

  CurrentDevicePath = DevicePath;
  while (IsDevicePathEnd (CurrentDevicePath) == FALSE) {
    if (CurrentDevicePath->Type == MEDIA_DEVICE_PATH) {
      FlashMediaBoot = TRUE;
      break;
    }

    CurrentDevicePath = NextDevicePathNode (CurrentDevicePath);
  }

  if (FlashMediaBoot == TRUE) {
    CurrentDevicePath = DevicePath;
    while (IsDevicePathEnd (CurrentDevicePath) == FALSE) {
      if (CurrentDevicePath->Type == MESSAGING_DEVICE_PATH) {
        if (CurrentDevicePath->SubType == MSG_EMMC_DP) {
          if (RecoveryBoot) {
            DescriptionString = HiiGetString (mHiiHandle, STRING_TOKEN (STR_LOAD_FILE_EMMC_RECOVERY_BOOT_DESCRIPTION), NULL);
          } else {
            DescriptionString = HiiGetString (mHiiHandle, STRING_TOKEN (STR_LOAD_FILE_EMMC_KERNEL_BOOT_DESCRIPTION), NULL);
          }

          break;
        } else if (CurrentDevicePath->SubType == MSG_SD_DP) {
          if (RecoveryBoot) {
            DescriptionString = HiiGetString (mHiiHandle, STRING_TOKEN (STR_LOAD_FILE_SD_RECOVERY_BOOT_DESCRIPTION), NULL);
          } else {
            DescriptionString = HiiGetString (mHiiHandle, STRING_TOKEN (STR_LOAD_FILE_SD_KERNEL_BOOT_DESCRIPTION), NULL);
          }

          break;
        } else if (CurrentDevicePath->SubType == MSG_UFS_DP) {
          if (RecoveryBoot) {
            DescriptionString = HiiGetString (mHiiHandle, STRING_TOKEN (STR_LOAD_FILE_UFS_RECOVERY_BOOT_DESCRIPTION), NULL);
          } else {
            DescriptionString = HiiGetString (mHiiHandle, STRING_TOKEN (STR_LOAD_FILE_UFS_KERNEL_BOOT_DESCRIPTION), NULL);
          }
        } else if (CurrentDevicePath->SubType == MSG_USB_DP) {
          if (RecoveryBoot) {
            DescriptionString = HiiGetString (mHiiHandle, STRING_TOKEN (STR_LOAD_FILE_USB_RECOVERY_BOOT_DESCRIPTION), NULL);
          } else {
            DescriptionString = HiiGetString (mHiiHandle, STRING_TOKEN (STR_LOAD_FILE_USB_KERNEL_BOOT_DESCRIPTION), NULL);
          }
        }
      } else if (CurrentDevicePath->Type == HARDWARE_DEVICE_PATH) {
        if (CurrentDevicePath->SubType == HW_VENDOR_DP) {
          VENDOR_DEVICE_PATH  *VendorPath = (VENDOR_DEVICE_PATH *)CurrentDevicePath;
          if (CompareGuid (&VendorPath->Guid, &gNVIDIARcmKernelGuid)) {
            DescriptionString = HiiGetString (mHiiHandle, STRING_TOKEN (STR_LOAD_FILE_RCM_BOOT_DESCRIPTION), NULL);
            break;
          } else if (CompareGuid (&VendorPath->Guid, &gEfiPersistentVirtualDiskGuid)) {
            DescriptionString = HiiGetString (mHiiHandle, STRING_TOKEN (STR_LOAD_FILE_VIRTUAL_STORAGE_KERNEL_BOOT_DESCRIPTION), NULL);
            break;
          }
        }
      }

      CurrentDevicePath = NextDevicePathNode (CurrentDevicePath);
    }
  }

  if (DescriptionString != NULL) {
    DescriptionStringLength = (StrLen (DescriptionString) + 1) * sizeof (CHAR16);
    Status                  = gBS->AllocatePool (
                                     EfiBootServicesData,
                                     DescriptionStringLength,
                                     (VOID **)&Desc
                                     );
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
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  mHiiHandle = HiiAddPackages (
                 &mPlatformBootManagerDeviceDescriptionGuid,
                 ImageHandle,
                 PlatformBootManagerBootDescriptionLibStrings,
                 NULL
                 );
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
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
