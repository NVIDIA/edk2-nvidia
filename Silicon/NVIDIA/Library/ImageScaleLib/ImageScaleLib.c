/** @file
  Image Scale Library implementation.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ImageScaleLib.h>

/**
  Scales an image to a new width and height.

  Uses box/area resampling for downscaling (better quality) and
  nearest-neighbor for upscaling (fast).

  @param[in]  Image          Pointer to the source image bitmap
  @param[in]  ImageWidth     Width of the source image in pixels
  @param[in]  ImageHeight    Height of the source image in pixels
  @param[in]  ScaledWidth    Width of the scaled image in pixels
  @param[in]  ScaledHeight   Height of the scaled image in pixels
  @param[out] ScaledImage    Pointer to receive the scaled image (caller must free)

  @retval EFI_SUCCESS           Image was scaled successfully
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate memory for scaled image
  @retval EFI_INVALID_PARAMETER Image or ScaledImage is NULL, or dimensions are 0
**/
EFI_STATUS
EFIAPI
ImageScale (
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Image,
  IN  UINTN                          ImageWidth,
  IN  UINTN                          ImageHeight,
  IN  UINTN                          ScaledWidth,
  IN  UINTN                          ScaledHeight,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL  **ScaledImage
  )
{
  UINTN  PixelCount;
  UINTN  DstX;
  UINTN  DstY;
  UINTN  SrcY;
  UINTN  SrcX;
  // Variables for box/area resampling
  UINTN                          SrcY0;
  UINTN                          SrcY1;
  UINTN                          SrcX0;
  UINTN                          SrcX1;
  UINT32                         SumRed;
  UINT32                         SumGreen;
  UINT32                         SumBlue;
  UINT32                         SumReserved;
  UINTN                          Count;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  P;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Result;

  if ((Image == NULL) || (ScaledImage == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((ImageWidth == 0) || (ImageHeight == 0) || (ScaledWidth == 0) || (ScaledHeight == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  PixelCount = ScaledWidth * ScaledHeight;

  *ScaledImage = AllocatePool (PixelCount * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  if (*ScaledImage == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // No scaling needed - just copy
  //
  if ((ScaledHeight == ImageHeight) && (ScaledWidth == ImageWidth)) {
    CopyMem (*ScaledImage, Image, PixelCount * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    return EFI_SUCCESS;
  }

  //
  // Upscaling or mixed - use nearest-neighbor (fast)
  //
  if ((ScaledWidth >= ImageWidth) || (ScaledHeight >= ImageHeight)) {
    for (DstY = 0; DstY < ScaledHeight; ++DstY) {
      SrcY = (DstY * ImageHeight) / ScaledHeight;
      for (DstX = 0; DstX < ScaledWidth; ++DstX) {
        SrcX                                      = (DstX * ImageWidth) / ScaledWidth;
        (*ScaledImage)[DstY * ScaledWidth + DstX] = Image[SrcY * ImageWidth + SrcX];
      }
    }
  } else {
    //
    // Downscaling - use box/area resampling (better quality)
    //
    for (DstY = 0; DstY < ScaledHeight; ++DstY) {
      SrcY0 = (DstY * ImageHeight) / ScaledHeight;
      SrcY1 = ((DstY + 1) * ImageHeight) / ScaledHeight;
      if (SrcY1 > ImageHeight) {
        SrcY1 = ImageHeight;
      }

      for (DstX = 0; DstX < ScaledWidth; ++DstX) {
        SrcX0 = (DstX * ImageWidth) / ScaledWidth;
        SrcX1 = ((DstX + 1) * ImageWidth) / ScaledWidth;
        if (SrcX1 > ImageWidth) {
          SrcX1 = ImageWidth;
        }

        SumRed      = 0;
        SumGreen    = 0;
        SumBlue     = 0;
        SumReserved = 0;
        Count       = 0;
        for (SrcY = SrcY0; SrcY < SrcY1; ++SrcY) {
          for (SrcX = SrcX0; SrcX < SrcX1; ++SrcX) {
            P            = Image[SrcY * ImageWidth + SrcX];
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

  return EFI_SUCCESS;
}
