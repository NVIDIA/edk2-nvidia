/** @file
  Logo DXE Driver, install Edkii Platform Logo protocol.

  SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2016 - 2017, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/HiiImageEx.h>
#include <Protocol/HiiPackageList.h>
#include <Protocol/PlatformLogo.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PerformanceLib.h>

#include "LogoPrivate.h"

EFI_HII_IMAGE_EX_PROTOCOL  *mHiiImageEx;
EFI_HII_HANDLE             mHiiHandle;
EFI_IMAGE_INPUT            mLogoImage = { 0 };

/**
  Calculates the absolute value of an INT64 number.

  @param[in]  Value    The INT64 value to get the absolute value of.

  @return     The absolute value of the input.
**/
STATIC
UINT64
EFIAPI
Abs (
  IN INT64  Value
  )
{
  return (Value < 0) ? (UINT64)(-Value) : (UINT64)Value;
}

/**
  Scales an image to a new width and height.

  @param[in]  Image          Pointer to the source image to scale
  @param[in]  ImageWidth     Width of the image in pixels
  @param[in]  ImageHeight    Height of the image in pixels
  @param[in]  ScaledWidth    Width of the scaled image in pixels
  @param[in]  ScaledHeight   Height of the scaled image in pixels
  @param[out] ScaledImage    Pointer to receive the scaled image

  @retval EFI_SUCCESS           Image was scaled successfully
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate memory for scaled image
  @retval EFI_INVALID_PARAMETER OriginalImage or ScaledImage is NULL
**/
EFI_STATUS
ScaleImage (
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Image,
  IN  UINT64                         ImageWidth,
  IN  UINT64                         ImageHeight,
  IN  UINT64                         ScaledWidth,
  IN  UINT64                         ScaledHeight,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL  **ScaledImage
  )
{
  EFI_STATUS  Status;
  UINT64      PixelCount;
  UINT64      DstX;
  UINT64      DstY;
  UINT64      SrcY;
  UINT64      SrcX;
  // Variables for box/area resampling
  double                         SrcY0f;
  double                         SrcY1f;
  UINT64                         SrcY0;
  UINT64                         SrcY1;
  double                         SrcX0f;
  double                         SrcX1f;
  UINT64                         SrcX0;
  UINT64                         SrcX1;
  UINT32                         SumRed;
  UINT32                         SumGreen;
  UINT32                         SumBlue;
  UINT32                         SumReserved;
  UINT64                         Count;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  P;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Result;

  PERF_FUNCTION_BEGIN ();
  if ((Image == NULL) || (ScaledImage == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  if ((ImageWidth == 0) || (ImageHeight == 0) || (ScaledWidth == 0) || (ScaledHeight == 0)) {
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  PixelCount = ScaledWidth * ScaledHeight;

  *ScaledImage = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)AllocatePool (PixelCount * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  if (*ScaledImage == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  if ((ScaledHeight == ImageHeight) && (ScaledWidth == ImageWidth)) {
    CopyMem (*ScaledImage, Image, PixelCount * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    Status = EFI_SUCCESS;
    goto Done;
  }

  // Box (area) resampling scaling
  if ((ScaledWidth >= ImageWidth) || (ScaledHeight >= ImageHeight)) {
    // For upscaling, fall back to nearest-neighbor
    for (DstY = 0; DstY < ScaledHeight; ++DstY) {
      SrcY = (DstY * ImageHeight) / ScaledHeight;
      for (DstX = 0; DstX < ScaledWidth; ++DstX) {
        SrcX                                      = (DstX * ImageWidth) / ScaledWidth;
        (*ScaledImage)[DstY * ScaledWidth + DstX] = Image[SrcY * ImageWidth + SrcX];
      }
    }
  } else {
    // For downscaling, use box/area resampling
    for (DstY = 0; DstY < ScaledHeight; ++DstY) {
      // Calculate the source box for this destination row
      SrcY0f = ((double)DstY * (double)ImageHeight) / (double)ScaledHeight;
      SrcY1f = ((double)(DstY + 1) * (double)ImageHeight) / (double)ScaledHeight;
      SrcY0  = (UINT64)SrcY0f;
      SrcY1  = (UINT64)SrcY1f;
      if (SrcY1 > ImageHeight) {
        SrcY1 = ImageHeight;
      }

      for (DstX = 0; DstX < ScaledWidth; ++DstX) {
        SrcX0f = ((double)DstX * (double)ImageWidth) / (double)ScaledWidth;
        SrcX1f = ((double)(DstX + 1) * (double)ImageWidth) / (double)ScaledWidth;
        SrcX0  = (UINT64)SrcX0f;
        SrcX1  = (UINT64)SrcX1f;
        if (SrcX1 > ImageWidth) {
          SrcX1 = ImageWidth;
        }

        SumRed      = 0;
        SumGreen    = 0;
        SumBlue     = 0;
        SumReserved = 0;
        Count       = 0;
        for (UINT64 y = SrcY0; y < SrcY1; ++y) {
          for (UINT64 x = SrcX0; x < SrcX1; ++x) {
            P            = Image[y * ImageWidth + x];
            SumRed      += P.Red;
            SumGreen    += P.Green;
            SumBlue     += P.Blue;
            SumReserved += P.Reserved;
            ++Count;
          }
        }

        ZeroMem (&Result, sizeof (Result));
        if (Count > 0) {
          Result.Red      = (UINT8)(SumRed / Count);
          Result.Green    = (UINT8)(SumGreen / Count);
          Result.Blue     = (UINT8)(SumBlue / Count);
          Result.Reserved = (UINT8)(SumReserved / Count);
        }

        (*ScaledImage)[DstY * ScaledWidth + DstX] = Result;
      }
    }
  }

  Status = EFI_SUCCESS;

Done:
  PERF_FUNCTION_END ();
  return Status;
}

/**
  Load a platform logo image and return its data and attributes.

  @param This              The pointer to this protocol instance.
  @param Instance          The visible image instance is found.
  @param Image             Points to the image.
  @param Attribute         The display attributes of the image returned.
  @param OffsetX           The X offset of the image regarding the Attribute.
  @param OffsetY           The Y offset of the image regarding the Attribute.

  @retval EFI_SUCCESS      The image was fetched successfully.
  @retval EFI_NOT_FOUND    The specified image could not be found.
**/
EFI_STATUS
EFIAPI
GetImage (
  IN     EDKII_PLATFORM_LOGO_PROTOCOL        *This,
  IN OUT UINT32                              *Instance,
  OUT EFI_IMAGE_INPUT                        *Image,
  OUT EDKII_PLATFORM_LOGO_DISPLAY_ATTRIBUTE  *Attribute,
  OUT INTN                                   *OffsetX,
  OUT INTN                                   *OffsetY
  )
{
  EFI_STATUS                     Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *GraphicsOutput;
  UINTN                          CurrentLogo;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *ScaledImage;
  UINT64                         ScreenHeight;
  UINT64                         ScreenWidth;
  UINT64                         AdjustedScreenHeight;
  UINT64                         TargetHeight;
  UINT64                         TargetWidth;
  UINT64                         LogoScreenRatio;
  UINT64                         LogoScreenCenterY;
  EFI_IMAGE_INPUT                CurrentImage;
  EFI_IMAGE_INPUT                SelectedImage;

  if ((This == NULL) || (Instance == NULL) || (Image == NULL) ||
      (Attribute == NULL) || (OffsetX == NULL) || (OffsetY == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (*Instance != 0) {
    return EFI_NOT_FOUND;
  }

  (*Instance)++;

  if (mLogoImage.Bitmap != NULL) {
    CopyMem (Image, &mLogoImage, sizeof (EFI_IMAGE_INPUT));
    return EFI_SUCCESS;
  }

  Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **)&GraphicsOutput);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get graphics output protocol\n", __func__));
    return Status;
  }

  ScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
  ScreenWidth  = GraphicsOutput->Mode->Info->HorizontalResolution;

  LogoScreenCenterY = PcdGet16 (PcdLogoCenterY);
  LogoScreenRatio   = PcdGet16 (PcdLogoScreenRatio);
  if (LogoScreenRatio > 1000) {
    DEBUG ((DEBUG_ERROR, "%a: LogoScreenRatio is greater than 1000\n", __func__));
    LogoScreenRatio = 1000;
  }

  if (LogoScreenCenterY > 1000) {
    DEBUG ((DEBUG_ERROR, "%a: LogoScreenCenterY is greater than 1000\n", __func__));
    LogoScreenCenterY = 1000;
  }

  // Adjusted screen height is used for logo selection only and adjusts to the
  // screen size to account for the fact that the logo might not be centered on
  // the screen. The available space is reduced by amount it is shifted from the
  // center of the screen.
  if (LogoScreenCenterY <= 500) {
    AdjustedScreenHeight = (2 * (ScreenHeight * LogoScreenCenterY / 1000));
  } else {
    AdjustedScreenHeight = (2 * (ScreenHeight * (1000 - LogoScreenCenterY) / 1000));
  }

  if (LogoScreenRatio != 0) {
    TargetHeight = (ScreenHeight * LogoScreenRatio) / 1000;
    TargetWidth  = (ScreenWidth * LogoScreenRatio) / 1000;
  } else {
    TargetHeight = AdjustedScreenHeight;
    TargetWidth  = ScreenWidth;
  }

  ZeroMem (&SelectedImage, sizeof (EFI_IMAGE_INPUT));

  for (CurrentLogo = 0; CurrentLogo < mLogoImageIdCount; CurrentLogo++) {
    Status = mHiiImageEx->GetImageEx (
                            mHiiImageEx,
                            mHiiHandle,
                            mLogoImageId[CurrentLogo],
                            &CurrentImage
                            );
    if (EFI_ERROR (Status)) {
      if (Status == EFI_NOT_FOUND) {
        break;
      }

      DEBUG ((DEBUG_ERROR, "%a: Failed to get logo image: %r\n", __func__, Status));
      return Status;
    }

    DEBUG ((DEBUG_ERROR, "%a: Found logo %ux%u\r\n", __func__, CurrentImage.Width, CurrentImage.Height));
    if (LogoScreenRatio == 0) {
      // If larger that display or this is smaller then previous image skip
      if ((CurrentImage.Height > TargetHeight) ||
          (CurrentImage.Width > TargetWidth) ||
          (CurrentImage.Height < SelectedImage.Height) ||
          (CurrentImage.Width < SelectedImage.Width))
      {
        continue;
      }
    } else if (SelectedImage.Height != 0) {
      // Skip the image if previous image is closer to the target height.
      if (Abs (CurrentImage.Height - TargetHeight) > Abs (SelectedImage.Height - TargetHeight)) {
        continue;
      }
    }

    CopyMem (&SelectedImage, &CurrentImage, sizeof (EFI_IMAGE_INPUT));
  }

  if (SelectedImage.Bitmap == NULL) {
    DEBUG ((DEBUG_VERBOSE, "%a: No suitable logo found\n", __func__));
    return EFI_NOT_FOUND;
  }

  mLogoImage.Flags = 0;
  if (LogoScreenRatio != 0) {
    // Calculate the height and width of the selected logo
    TargetHeight = (ScreenHeight * LogoScreenRatio) / 1000;
    TargetWidth  = (SelectedImage.Width * TargetHeight) / SelectedImage.Height;

    // If width would exceed screen, scale to fit width
    if (TargetWidth > ScreenWidth) {
      TargetWidth  = ScreenWidth;
      TargetHeight = (SelectedImage.Height * TargetWidth) / SelectedImage.Width;
    }

    Status = ScaleImage (
               SelectedImage.Bitmap,
               SelectedImage.Width,
               SelectedImage.Height,
               TargetWidth,
               TargetHeight,
               &ScaledImage
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to scale image: %r\n", __func__, Status));
      return Status;
    }

    mLogoImage.Bitmap = ScaledImage;
    mLogoImage.Width  = TargetWidth;
    mLogoImage.Height = TargetHeight;
  } else {
    CopyMem (&mLogoImage, &SelectedImage, sizeof (EFI_IMAGE_INPUT));
  }

  CopyMem (Image, &mLogoImage, sizeof (EFI_IMAGE_INPUT));

  // Center horizontally
  *Attribute = EdkiiPlatformLogoDisplayAttributeCenterTop;
  *OffsetX   = 0;
  // Calculate Y offset to center image at specified percentage down the screen
  *OffsetY = (INTN)((ScreenHeight * LogoScreenCenterY / 1000) - (Image->Height / 2));

  DEBUG ((DEBUG_VERBOSE, "%a: Image dimensions: %dx%d\n", __func__, Image->Width, Image->Height));
  DEBUG ((DEBUG_VERBOSE, "%a: Placing image at offset: X=%d Y=%d\n", __func__, *OffsetX, *OffsetY));

  return EFI_SUCCESS;
}

EDKII_PLATFORM_LOGO_PROTOCOL  mPlatformLogo = {
  GetImage
};

/**
  Entrypoint of this module.

  This function is the entrypoint of this module. It installs the Edkii
  Platform Logo protocol.

  @param  ImageHandle       The firmware allocated handle for the EFI image.
  @param  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.

**/
EFI_STATUS
EFIAPI
InitializeLogo (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                   Status;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;
  EFI_HII_DATABASE_PROTOCOL    *HiiDatabase;

  (VOID)SystemTable; // Unused

  Status = gBS->LocateProtocol (
                  &gEfiHiiDatabaseProtocolGuid,
                  NULL,
                  (VOID **)&HiiDatabase
                  );
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  Status = gBS->LocateProtocol (
                  &gEfiHiiImageExProtocolGuid,
                  NULL,
                  (VOID **)&mHiiImageEx
                  );
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  //
  // Retrieve HII package list from ImageHandle
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
    DEBUG ((DEBUG_ERROR, "HII Image Package with logo not found in PE/COFF resource section\n"));
    return Status;
  }

  //
  // Publish HII package list to HII Database.
  //
  Status = HiiDatabase->NewPackageList (
                          HiiDatabase,
                          PackageList,
                          NULL,
                          &mHiiHandle
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create HII package list\n"));
    return Status;
  }

  return gBS->InstallMultipleProtocolInterfaces (
                &ImageHandle,
                &gEdkiiPlatformLogoProtocolGuid,
                &mPlatformLogo,
                NULL
                );
}
