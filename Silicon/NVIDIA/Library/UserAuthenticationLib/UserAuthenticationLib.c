/** @file

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/SecurityManagementLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/UserAuthentication.h>

EFI_GUID  mUiAppFileGuid = {
  0x462CAA21, 0x7614, 0x4503, { 0x83, 0x6E, 0x8A, 0xB6, 0xF4, 0x66, 0x23, 0x31 }
};

/**
  Check user password before loading setup menu

  @param[in]    AuthenticationStatus
                           This is the authentication status returned from the security
                           measurement services for the input file.
  @param[in]    File       This is a pointer to the device path of the file that is
                           being dispatched. This will optionally be used for logging.
  @param[in]    FileBuffer File buffer matches the input file device path.
  @param[in]    FileSize   Size of File buffer matches the input file device path.
  @param[in]    BootPolicy A boot policy that was used to call LoadImage() UEFI service.

  @retval EFI_SUCCESS            The file specified by DevicePath and non-NULL
                                 FileBuffer did authenticate, and the platform policy dictates
                                 that the DXE Foundation may use the file.
  @retval EFI_SUCCESS            The device path specified by NULL device path DevicePath
                                 and non-NULL FileBuffer did authenticate, and the platform
                                 policy dictates that the DXE Foundation may execute the image in
                                 FileBuffer.
  @retval EFI_SECURITY_VIOLATION The file specified by File did not authenticate, and
                                 the platform policy dictates that File should be placed
                                 in the untrusted state. The image has been added to the file
                                 execution table.
  @retval EFI_ACCESS_DENIED      The file specified by File and FileBuffer did not
                                 authenticate, and the platform policy dictates that the DXE
                                 Foundation may not use File. The image has
                                 been added to the file execution table.

**/
EFI_STATUS
EFIAPI
UserAuthenticationHandler (
  IN  UINT32                          AuthenticationStatus,
  IN  CONST EFI_DEVICE_PATH_PROTOCOL  *File  OPTIONAL,
  IN  VOID                            *FileBuffer,
  IN  UINTN                           FileSize,
  IN  BOOLEAN                         BootPolicy
  )
{
  EFI_STATUS                         Status;
  EFI_DEVICE_PATH_PROTOCOL           *Node;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  *FvFile;
  NVIDIA_USER_AUTH_PROTOCOL          *UserAuthProtocol;

  if (File == NULL) {
    return EFI_SUCCESS;
  }

  //
  // If UiApp.efi (Setup Menu) is being loaded
  //
  Node = (EFI_DEVICE_PATH_PROTOCOL *)File;
  if ((DevicePathType (Node) == MEDIA_DEVICE_PATH) && (DevicePathSubType (Node) == MEDIA_PIWG_FW_VOL_DP)) {
    Node = NextDevicePathNode (Node);
    if ((DevicePathType (Node) == MEDIA_DEVICE_PATH) && (DevicePathSubType (Node) == MEDIA_PIWG_FW_FILE_DP)) {
      FvFile = (MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)Node;
      if (IsDevicePathEnd (NextDevicePathNode (Node))) {
        if (CompareGuid (&FvFile->FvFileName, &mUiAppFileGuid) == TRUE) {
          //
          // Prompt for password if user password is required
          //
          Status = gBS->LocateProtocol (
                          &gNVIDIAUserAuthenticationProtocolGuid,
                          NULL,
                          (VOID **)&UserAuthProtocol
                          );
          if (!EFI_ERROR (Status) && (UserAuthProtocol != NULL)) {
            return UserAuthProtocol->CheckPassword (UserAuthProtocol);
          }
        }
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Register security handler to check user password

  @param  ImageHandle  ImageHandle of the loaded driver.
  @param  SystemTable  Pointer to the EFI System Table.

  @retval  EFI_SUCCESS            Register successfully.
  @retval  EFI_OUT_OF_RESOURCES   No enough memory to register this handler.
**/
EFI_STATUS
EFIAPI
UserAuthenticationLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return RegisterSecurity2Handler (
           UserAuthenticationHandler,
           EFI_AUTH_OPERATION_VERIFY_IMAGE | EFI_AUTH_OPERATION_IMAGE_REQUIRED
           );
}
