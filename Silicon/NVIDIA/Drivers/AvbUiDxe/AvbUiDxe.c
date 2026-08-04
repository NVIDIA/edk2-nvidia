/** @file
  AVB UI DXE Driver - Displays Android Verified Boot warning screens.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/AvbUi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BmpBlobLib.h>
#include <Library/BootLogoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>
#include <Library/ImageScaleLib.h>

//
// AVB UI display timeout in seconds
//
#define AVB_UI_TIMEOUT_SECONDS       5
#define AVB_UI_MICROSECONDS_PER_SEC  1000000

/**
  Display an image scaled to full screen.
  Uses single Blt for the entire frame to minimize tearing.

  The artwork lives in the BMP partition (bmp.blob) rather than in the
  firmware image, so it can be refreshed without reflashing UEFI.

  @param[in] BlobImageType   Blob image type to look up in bmp.blob.
  @param[in] TimeoutSeconds  Timeout in seconds (0 = no wait).

  @retval EFI_SUCCESS        Image displayed successfully.
  @retval Other              Error occurred.
**/
STATIC
EFI_STATUS
AvbUiDisplayImage (
  IN UINT32  BlobImageType,
  IN UINTN   TimeoutSeconds
  )
{
  EFI_STATUS                     Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *Gop;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *SourceImage;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *ScaledImage;
  UINTN                          SourceWidth;
  UINTN                          SourceHeight;
  UINTN                          ScreenWidth;
  UINTN                          ScreenHeight;
  UINTN                          ElapsedSeconds;
  EFI_INPUT_KEY                  Key;

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

  Status = BmpBlobGetPartitionImage (
             BlobImageType,
             ScreenWidth,
             ScreenHeight,
             &SourceImage,
             &SourceWidth,
             &SourceHeight
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to load image %u: %r\n", __FUNCTION__, BlobImageType, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Image %ux%u\n", __FUNCTION__, SourceWidth, SourceHeight));

  //
  // Scale image to full screen in memory first
  //
  Status = ImageScale (
             SourceImage,
             SourceWidth,
             SourceHeight,
             ScreenWidth,
             ScreenHeight,
             &ScaledImage
             );

  FreePool (SourceImage);

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
  EFI_STATUS  Status        = EFI_SUCCESS;
  UINT32      BlobImageType = MAX_UINT32;
  UINTN       Timeout       = 0;
  BOOLEAN     ShowImage     = TRUE;

  DEBUG ((DEBUG_INFO, "%a: State = %d\n", __FUNCTION__, State));

  switch (State) {
    case AVB_UI_STATE_GREEN:
      DEBUG ((DEBUG_INFO, "%a: Green state - no UI needed\n", __FUNCTION__));
      ShowImage = FALSE;
      break;

    case AVB_UI_STATE_ORANGE:
      DEBUG ((DEBUG_INFO, "%a: Orange state - device unlocked\n", __FUNCTION__));
      BlobImageType = NVIDIA_BMP_IMAGE_VERITY_ORANGE;
      Timeout       = AVB_UI_TIMEOUT_SECONDS;
      break;

    case AVB_UI_STATE_RED:
      DEBUG ((DEBUG_ERROR, "%a: Red state - verification failed\n", __FUNCTION__));
      BlobImageType = NVIDIA_BMP_IMAGE_VERITY_RED_STOP;
      Timeout       = 0;  // Halt
      break;

    case AVB_UI_STATE_RED_EIO:
      DEBUG ((DEBUG_ERROR, "%a: Red EIO state - I/O error\n", __FUNCTION__));
      BlobImageType = NVIDIA_BMP_IMAGE_VERITY_RED_EIO;
      Timeout       = 0;  // Halt
      break;

    default:
      DEBUG ((DEBUG_WARN, "%a: Unknown state %d\n", __FUNCTION__, State));
      ShowImage = FALSE;
      break;
  }

  if (ShowImage) {
    Status = AvbUiDisplayImage (BlobImageType, Timeout);
  }

  //
  // PlatformBootManagerLib defers BootLogoEnableLogo() for Android builds
  // (PcdBootAndroidImage), expecting AvbUiDxe to draw the platform boot
  // logo once verified-boot UI handling is complete. Clear the framebuffer
  // first so the boot logo isn't overlaid on top of a lingering AVB splash.
  // Skip both clear and logo for red / red_eio so the diagnostic image
  // stays on screen while the device halts.
  //
  if ((State != AVB_UI_STATE_RED) && (State != AVB_UI_STATE_RED_EIO)) {
    if (gST->ConOut != NULL) {
      gST->ConOut->ClearScreen (gST->ConOut);
    }

    BootLogoEnableLogo ();
  }

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
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "%a: Initializing AVB UI DXE Driver\n", __FUNCTION__));

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
