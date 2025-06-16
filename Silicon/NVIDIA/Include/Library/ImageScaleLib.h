/** @file
  Image Scale Library - provides image scaling functions.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef IMAGE_SCALE_LIB_H_
#define IMAGE_SCALE_LIB_H_

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>

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
  );

#endif // IMAGE_SCALE_LIB_H_
