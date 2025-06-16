/** @file
  AVB UI DXE Driver - Displays Android Verified Boot warning screens.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/HiiImageEx.h>
#include <Protocol/HiiPackageList.h>
#include <Protocol/AvbUi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>
#include <Library/ImageScaleLib.h>

#include "AvbUiPrivate.h"

//
// AVB UI display timeout in seconds
//
#define AVB_UI_TIMEOUT_SECONDS       5
#define AVB_UI_MICROSECONDS_PER_SEC  1000000

//
// Module globals
//
STATIC EFI_HII_IMAGE_EX_PROTOCOL  *mHiiImageEx;
STATIC EFI_HII_HANDLE             mHiiHandle;

/**
  Display an image scaled to full screen.
  Uses single Blt for the entire frame to minimize tearing.

  @param[in] ImageId         HII Image ID to display.
  @param[in] TimeoutSeconds  Timeout in seconds (0 = no wait).

  @retval EFI_SUCCESS        Image displayed successfully.
  @retval Other              Error occurred.
**/
STATIC
EFI_STATUS
AvbUiDisplayImage (
  IN EFI_IMAGE_ID  ImageId,
  IN UINTN         TimeoutSeconds
  )
{
  EFI_STATUS                     Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *Gop;
  EFI_IMAGE_INPUT                Image;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *ScaledImage;
  UINTN                          ScreenWidth;
  UINTN                          ScreenHeight;
  UINTN                          ElapsedSeconds;
  EFI_INPUT_KEY                  Key;

  //
  // Get image from HII
  //
  Status = mHiiImageEx->GetImageEx (
                          mHiiImageEx,
                          mHiiHandle,
                          ImageId,
                          &Image
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get image %d: %r\n", __FUNCTION__, ImageId, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Image %ux%u\n", __FUNCTION__, Image.Width, Image.Height));

  //
  // Locate GOP
  //
  Status = gBS->LocateProtocol (
                  &gEfiGraphicsOutputProtocolGuid,
                  NULL,
                  (VOID **)&Gop
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate GOP: %r\n", __FUNCTION__, Status));
    return Status;
  }

  ScreenWidth  = Gop->Mode->Info->HorizontalResolution;
  ScreenHeight = Gop->Mode->Info->VerticalResolution;

  DEBUG ((DEBUG_INFO, "%a: Screen %ux%u\n", __FUNCTION__, ScreenWidth, ScreenHeight));

  //
  // Scale image to full screen in memory first
  //
  Status = ImageScale (
             Image.Bitmap,
             (UINTN)Image.Width,
             (UINTN)Image.Height,
             ScreenWidth,
             ScreenHeight,
             &ScaledImage
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to scale image: %r\n", __FUNCTION__, Status));
    return Status;
  }

  //
  // Single Blt for entire screen - GOP driver should handle sync
  //
  Status = Gop->Blt (
                  Gop,
                  ScaledImage,
                  EfiBltBufferToVideo,
                  0,
                  0,      // Source X, Y
                  0,
                  0,      // Destination X, Y
                  ScreenWidth,
                  ScreenHeight,
                  ScreenWidth * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                  );

  FreePool (ScaledImage);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to blit image: %r\n", __FUNCTION__, Status));
    return Status;
  }

  //
  // Wait for timeout or key press
  //
  if (TimeoutSeconds > 0) {
    for (ElapsedSeconds = 0; ElapsedSeconds < TimeoutSeconds; ElapsedSeconds++) {
      if (gST->ConIn != NULL) {
        Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
        if (!EFI_ERROR (Status)) {
          DEBUG ((DEBUG_INFO, "%a: Key pressed, continuing\n", __FUNCTION__));
          break;
        }
      }

      MicroSecondDelay (AVB_UI_MICROSECONDS_PER_SEC);
    }
  } else {
    //
    // No timeout - just display and return
    //
    DEBUG ((DEBUG_ERROR, "%a: AVB verification failed\n", __FUNCTION__));
  }

  return EFI_SUCCESS;
}

/**
  Show AVB UI based on the boot state.

  @param[in] This   Pointer to this protocol instance.
  @param[in] State  The AVB boot state to display.

  @retval EFI_SUCCESS   UI displayed successfully.
  @retval Other         Error occurred.
**/
STATIC
EFI_STATUS
EFIAPI
AvbUiShow (
  IN NVIDIA_AVB_UI_PROTOCOL  *This,
  IN AVB_UI_STATE            State
  )
{
  EFI_STATUS    Status;
  EFI_IMAGE_ID  ImageId;
  UINTN         Timeout;

  DEBUG ((DEBUG_INFO, "%a: State = %d\n", __FUNCTION__, State));

  switch (State) {
    case AVB_UI_STATE_GREEN:
      DEBUG ((DEBUG_INFO, "%a: Green state - no UI needed\n", __FUNCTION__));
      return EFI_SUCCESS;

    case AVB_UI_STATE_ORANGE:
      DEBUG ((DEBUG_INFO, "%a: Orange state - device unlocked\n", __FUNCTION__));
      ImageId = mAvbUiImageId[AVB_UI_IMAGE_ORANGE];
      Timeout = AVB_UI_TIMEOUT_SECONDS;
      break;

    case AVB_UI_STATE_YELLOW:
      DEBUG ((DEBUG_INFO, "%a: Yellow state - unverified key\n", __FUNCTION__));
      ImageId = mAvbUiImageId[AVB_UI_IMAGE_YELLOW];
      Timeout = AVB_UI_TIMEOUT_SECONDS;
      break;

    case AVB_UI_STATE_RED:
      DEBUG ((DEBUG_ERROR, "%a: Red state - verification failed\n", __FUNCTION__));
      ImageId = mAvbUiImageId[AVB_UI_IMAGE_RED_STOP];
      Timeout = 0;  // Halt
      break;

    case AVB_UI_STATE_RED_EIO:
      DEBUG ((DEBUG_ERROR, "%a: Red EIO state - I/O error\n", __FUNCTION__));
      ImageId = mAvbUiImageId[AVB_UI_IMAGE_RED_EIO];
      Timeout = 0;  // Halt
      break;

    default:
      DEBUG ((DEBUG_WARN, "%a: Unknown state %d\n", __FUNCTION__, State));
      return EFI_SUCCESS;
  }

  Status = AvbUiDisplayImage (ImageId, Timeout);

  return Status;
}

//
// Protocol instance
//
STATIC NVIDIA_AVB_UI_PROTOCOL  mAvbUiProtocol = {
  AvbUiShow
};

/**
  Entry point of AVB UI DXE Driver.

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS     The entry point executed successfully.
  @retval Other           Error occurred.
**/
EFI_STATUS
EFIAPI
AvbUiDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                   Status;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;
  EFI_HII_DATABASE_PROTOCOL    *HiiDatabase;

  DEBUG ((DEBUG_INFO, "%a: Initializing AVB UI DXE Driver\n", __FUNCTION__));

  //
  // Locate HII Database Protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiHiiDatabaseProtocolGuid,
                  NULL,
                  (VOID **)&HiiDatabase
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate HII Database: %r\n", __FUNCTION__, Status));
    return Status;
  }

  //
  // Locate HII Image Ex Protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiHiiImageExProtocolGuid,
                  NULL,
                  (VOID **)&mHiiImageEx
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate HII Image Ex: %r\n", __FUNCTION__, Status));
    return Status;
  }

  //
  // Retrieve HII package list from PE/COFF resource section
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **)&PackageList,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: HII Package not found in PE/COFF resource: %r\n", __FUNCTION__, Status));
    return Status;
  }

  //
  // Publish HII package list to HII Database
  //
  Status = HiiDatabase->NewPackageList (
                          HiiDatabase,
                          PackageList,
                          NULL,
                          &mHiiHandle
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create HII package list: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: HII package list registered\n", __FUNCTION__));

  //
  // Install AVB UI Protocol
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gNVIDIAAvbUiProtocolGuid,
                  &mAvbUiProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install protocol: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: AVB UI Protocol installed\n", __FUNCTION__));
  return EFI_SUCCESS;
}
