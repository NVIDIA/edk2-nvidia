/** @file
  Provides services to display completion progress of a firmware update on
  a graphical console that supports the Graphics Output Protocol. The bar
  is drawn pixel-precise via GOP Blt; the percentage caption is rendered
  with HII Font below the bar. On high-resolution panels the library first
  tries a dedicated NVIDIA Sans package for the update caption without
  replacing default system glyph tables. If HII Font is unavailable, it
  falls back to text-console Print.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2016, Microsoft Corporation. All rights reserved.<BR>
  Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/GraphicsOutput.h>
#include <Protocol/HiiFont.h>
#include <Protocol/FwUpdateSplashProtocol.h>
#include <Uefi/UefiInternalFormRepresentation.h>

//
// Control Style.  Set to 100 so it is reset on first call.
//
STATIC UINTN  mPreviousProgress = 100;

//
// Text-mode caption position, computed once per progress session. The
// caption fallback sits one text row beneath mProgressRow (the text row
// whose pixel range hosts the GOP bar), centered horizontally on screen.
//
STATIC UINTN                  mProgressRow       = 0;
STATIC UINTN                  mTextCol           = 0;
STATIC UINTN                  mCaptionPixelLeft  = 0;
STATIC UINTN                  mCaptionPixelTop   = 0;
STATIC BOOLEAN                mUseHiiFontCaption = FALSE;
STATIC EFI_HII_FONT_PROTOCOL  *mHiiFont          = NULL;
#define CAPTION_FONT_NAME_MAX_CHARS  64U
typedef struct {
  EFI_FONT_DISPLAY_INFO    Info;
  CHAR16                   ExtraFontNameChars[CAPTION_FONT_NAME_MAX_CHARS - 1U];
} CAPTION_FONT_DISPLAY_INFO;

STATIC CAPTION_FONT_DISPLAY_INFO  mCaptionFontInfo;
STATIC EFI_HII_HANDLE             mNvidiaSansCaptionHiiHandle = NULL;
STATIC BOOLEAN                    mNvidiaSansCaptionTried     = FALSE;

//
// Caption prefix used for layout. For Android OTA builds the static
// "Updating SHIELD:" label is baked into the splash BMP, and this
// prefix is kept only to compute where the dynamic "%3u%%" suffix
// should be rendered.
//
STATIC CONST CHAR16  *mCaption   = L"Update Progress - ";
STATIC UINTN         mCaptionLen = 0;

//
// Displayed-progress lower bound for the current session. Re-evaluated at
// each session start by reading NVIDIA_FW_UPDATE_SPLASH_PROTOCOL::ProgressBase
// published by whichever driver staged the firmware update (e.g.
// AndroidFmpInitDxe); 0 means no remap (raw FMP progress is rendered as
// 0..100). Only Android runtime path consumes this value.
//
STATIC UINTN  mProgressBase = 0;

//
// GOP handle (lazy-cached) and pixel-precise bar geometry, refreshed at
// the start of every progress session. The lib bails out with
// EFI_NOT_READY if no GOP is available so the caller doesn't try to
// drive a non-existent display.
//
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL   *mGop = NULL;
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mBarFillPixel;
STATIC UINTN                          mBarPixelLeft   = 0;
STATIC UINTN                          mBarPixelTop    = 0;
STATIC UINTN                          mBarPixelWidth  = 0;
STATIC UINTN                          mBarPixelHeight = 0;

//
// Dim gray track behind the unfilled portion of the bar, matching the
// look of EFI_DARKGRAY in text mode.
//
STATIC CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mBarTrackPixel = {
  0x33, 0x33, 0x33, 0x00
};

//
// Colors sampled from the android_ota splash art in bmp.blob so the dynamic
// OTA progress UI matches it (yellow-green bar, brighter percentage text).
//
#define ANDROID_OTA_PROGRESS_GREEN_R  118U
#define ANDROID_OTA_PROGRESS_GREEN_G  185U
#define ANDROID_OTA_PROGRESS_GREEN_B  0U
#define ANDROID_OTA_PERCENT_TEXT_R    230U
#define ANDROID_OTA_PERCENT_TEXT_G    230U
#define ANDROID_OTA_PERCENT_TEXT_B    230U

//
// Trailing render after the caption: "%3d%% " (3 digits + '%' + space).
//
#define CAPTION_TAIL_WIDTH  5

//
// Bar geometry:
//   - BAR_PERCENT_OF_SCREEN: horizontal extent as a % of screen width.
//                            The reference spans x=245..778 on a
//                            1024-pixel-wide frame (about 52%).
//   - BAR_TOP_PERMILLE:      vertical anchor (top edge) in pixel space.
//                            The reference bar starts around y=483 on a
//                            576-pixel-high frame (about 83.9%).
//   - BAR_HEIGHT_PERMILLE:   bar thickness as 1/1000 of screen height.
//                            Tuned to the splash mockup (~1% of screen
//                            tall ≈ 10-11 px on 1080p).
//   - MIN_BAR_PIXEL_HEIGHT:  floor for short panels so the bar stays
//                            visible even on 480p framebuffers.
//
#define BAR_PERCENT_OF_SCREEN  52
#define BAR_TOP_PERMILLE       839
#define BAR_HEIGHT_PERMILLE    10
#define MIN_BAR_PIXEL_HEIGHT   4

//
// Caption size is anchored to the NVIDIA Sans glyph cell height (28px).
// For 4K panels, use dedicated 48x56 alpha glyphs so the displayed text
// keeps the same geometry but avoids magnifying a smaller source bitmap.
//
#define CAPTION_BASE_SCREEN_HEIGHT    1080U
#define CAPTION_MIN_FONT_SIZE         12U
#define CAPTION_4K_SCREEN_HEIGHT      2160U
#define ANDROID_OTA_4K_PERCENT_SCALE  2U
#define ANDROID_OTA_REFERENCE_WIDTH   1024U
#define ANDROID_OTA_REFERENCE_HEIGHT  576U
#define ANDROID_OTA_PERCENT_LEFT      556U
#define ANDROID_OTA_PERCENT_TOP       498U
//
// Pixel-space gap between bar and caption for >1080p panels.
// 1080p path intentionally keeps legacy text-row based spacing.
//
#define CAPTION_BAR_GAP_PERMILLE_HIGHRES  8U
#define CAPTION_BAR_GAP_MIN_HIGHRES       8U

#define NVIDIA_SANS_CAPTION_FONT_FAMILY             L"nvidia-sans-caption"
#define NVIDIA_SANS_CAPTION_SOURCE_GLYPH_WIDTH      24U
#define NVIDIA_SANS_CAPTION_SOURCE_GLYPH_HEIGHT     28U
#define NVIDIA_SANS_CAPTION_SOURCE_BITMAP_BYTES     84U
#define NVIDIA_SANS_CAPTION_SOURCE_ALPHA_BYTES      (NVIDIA_SANS_CAPTION_SOURCE_GLYPH_WIDTH * NVIDIA_SANS_CAPTION_SOURCE_GLYPH_HEIGHT)
#define NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_WIDTH   48U
#define NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_HEIGHT  56U
#define NVIDIA_SANS_CAPTION_4K_SOURCE_ALPHA_BYTES   (NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_WIDTH * NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_HEIGHT)
#define NVIDIA_SANS_CAPTION_4K_GLYPH_ADVANCE_X      34U
#define NVIDIA_SANS_CAPTION_GLYPH_WIDTH             NVIDIA_SANS_CAPTION_SOURCE_GLYPH_WIDTH
#define NVIDIA_SANS_CAPTION_GLYPH_HEIGHT            NVIDIA_SANS_CAPTION_SOURCE_GLYPH_HEIGHT
#define NVIDIA_SANS_CAPTION_GLYPH_ADVANCE_X         17U
#define NVIDIA_SANS_CAPTION_BITMAP_BYTES            NVIDIA_SANS_CAPTION_SOURCE_BITMAP_BYTES

STATIC EFI_GUID  mNvidiaSansCaptionFontPackageListGuid = {
  0x1147f8cb, 0x8e79, 0x4d9d, { 0x9a, 0x44, 0x7f, 0xee, 0x28, 0xa5, 0xc7, 0x31 }
};

typedef struct {
  CHAR16    Char;
  UINT8     Bitmap[NVIDIA_SANS_CAPTION_SOURCE_BITMAP_BYTES];
} NVIDIA_SANS_CAPTION_GLYPH;

typedef struct {
  CHAR16    Char;
  UINT8     Alpha[NVIDIA_SANS_CAPTION_SOURCE_ALPHA_BYTES];
} NVIDIA_SANS_CAPTION_ALPHA_GLYPH;

typedef struct {
  CHAR16    Char;
  UINT8     Alpha[NVIDIA_SANS_CAPTION_4K_SOURCE_ALPHA_BYTES];
} NVIDIA_SANS_CAPTION_4K_ALPHA_GLYPH;

#include "DisplayUpdateProgressLibGraphicsLegacyGlyphs.inc"

#include "DisplayUpdateProgressLibGraphicsAlphaGlyphs.inc"
#include "DisplayUpdateProgressLibGraphicsAlphaGlyphs4k.inc"

STATIC
EFI_STATUS
EnsureNvidiaSansCaptionFontPackage (
  VOID
  )
{
  EFI_HII_FONT_PACKAGE_HDR          *FontPackage;
  EFI_HII_GIBT_SKIP2_BLOCK          *SkipBlock;
  EFI_HII_GIBT_GLYPH_DEFAULT_BLOCK  *GlyphBlock;
  EFI_GLYPH_GIBT_END_BLOCK          *EndBlock;
  UINT8                             *PackageBuffer;
  UINT8                             *BlockPtr;
  UINTN                             HeaderSize;
  UINTN                             FontPackageSize;
  UINTN                             TotalPackageSize;
  UINTN                             GlyphBlocksSize;
  UINTN                             GlyphIndex;
  UINT16                            CurrentChar;
  UINT16                            SkipCount;
  CONST NVIDIA_SANS_CAPTION_GLYPH   *GlyphTable;
  UINTN                             GlyphCount;

  if (mNvidiaSansCaptionHiiHandle != NULL) {
    return EFI_SUCCESS;
  }

  if (mNvidiaSansCaptionTried) {
    return EFI_NOT_FOUND;
  }

  GlyphTable = mNvidiaSansCaptionGlyphsMd;
  GlyphCount = NVIDIA_SANS_CAPTION_GLYPH_COUNT_MD;

  mNvidiaSansCaptionTried = TRUE;
  HeaderSize              = sizeof (EFI_HII_FONT_PACKAGE_HDR) - sizeof (CHAR16) + StrSize (NVIDIA_SANS_CAPTION_FONT_FAMILY);
  GlyphBlocksSize         = 0;
  CurrentChar             = 1;

  for (GlyphIndex = 0; GlyphIndex < GlyphCount; GlyphIndex++) {
    if (GlyphTable[GlyphIndex].Char > CurrentChar) {
      GlyphBlocksSize += sizeof (EFI_HII_GIBT_SKIP2_BLOCK);
      CurrentChar      = GlyphTable[GlyphIndex].Char;
    }

    GlyphBlocksSize += sizeof (EFI_HII_GIBT_GLYPH_DEFAULT_BLOCK) - sizeof (((EFI_HII_GIBT_GLYPH_DEFAULT_BLOCK *)0)->BitmapData) + NVIDIA_SANS_CAPTION_BITMAP_BYTES;
    CurrentChar++;
  }

  GlyphBlocksSize += sizeof (EFI_GLYPH_GIBT_END_BLOCK);
  FontPackageSize  = HeaderSize + GlyphBlocksSize;
  TotalPackageSize = sizeof (UINT32) + FontPackageSize;

  PackageBuffer = AllocateZeroPool (TotalPackageSize);
  if (PackageBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  WriteUnaligned32 ((UINT32 *)PackageBuffer, (UINT32)TotalPackageSize);
  FontPackage                   = (EFI_HII_FONT_PACKAGE_HDR *)(PackageBuffer + sizeof (UINT32));
  FontPackage->Header.Length    = (UINT32)FontPackageSize;
  FontPackage->Header.Type      = EFI_HII_PACKAGE_FONTS;
  FontPackage->HdrSize          = (UINT32)HeaderSize;
  FontPackage->GlyphBlockOffset = (UINT32)HeaderSize;
  FontPackage->Cell.Width       = NVIDIA_SANS_CAPTION_GLYPH_WIDTH;
  FontPackage->Cell.Height      = NVIDIA_SANS_CAPTION_GLYPH_HEIGHT;
  FontPackage->Cell.OffsetX     = 0;
  FontPackage->Cell.OffsetY     = 0;
  FontPackage->Cell.AdvanceX    = NVIDIA_SANS_CAPTION_GLYPH_ADVANCE_X;
  FontPackage->FontStyle        = EFI_HII_FONT_STYLE_NORMAL;
  StrCpyS (
    FontPackage->FontFamily,
    StrSize (NVIDIA_SANS_CAPTION_FONT_FAMILY) / sizeof (CHAR16),
    NVIDIA_SANS_CAPTION_FONT_FAMILY
    );

  BlockPtr    = ((UINT8 *)FontPackage) + HeaderSize;
  CurrentChar = 1;

  for (GlyphIndex = 0; GlyphIndex < GlyphCount; GlyphIndex++) {
    if (GlyphTable[GlyphIndex].Char > CurrentChar) {
      SkipCount                   = (UINT16)(GlyphTable[GlyphIndex].Char - CurrentChar);
      SkipBlock                   = (EFI_HII_GIBT_SKIP2_BLOCK *)BlockPtr;
      SkipBlock->Header.BlockType = EFI_HII_GIBT_SKIP2;
      SkipBlock->SkipCount        = SkipCount;
      BlockPtr                   += sizeof (EFI_HII_GIBT_SKIP2_BLOCK);
      CurrentChar                += SkipCount;
    }

    GlyphBlock                   = (EFI_HII_GIBT_GLYPH_DEFAULT_BLOCK *)BlockPtr;
    GlyphBlock->Header.BlockType = EFI_HII_GIBT_GLYPH_DEFAULT;
    CopyMem (
      ((UINT8 *)GlyphBlock) + sizeof (EFI_HII_GLYPH_BLOCK),
      GlyphTable[GlyphIndex].Bitmap,
      NVIDIA_SANS_CAPTION_BITMAP_BYTES
      );
    BlockPtr += sizeof (EFI_HII_GIBT_GLYPH_DEFAULT_BLOCK) - sizeof (GlyphBlock->BitmapData) + NVIDIA_SANS_CAPTION_BITMAP_BYTES;
    CurrentChar++;
  }

  EndBlock                   = (EFI_GLYPH_GIBT_END_BLOCK *)BlockPtr;
  EndBlock->Header.BlockType = EFI_HII_GIBT_END;

  mNvidiaSansCaptionHiiHandle = HiiAddPackages (
                                  &mNvidiaSansCaptionFontPackageListGuid,
                                  NULL,
                                  (UINT32 *)PackageBuffer,
                                  NULL
                                  );

  FreePool (PackageBuffer);
  if (mNvidiaSansCaptionHiiHandle == NULL) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  Pick a caption font size that stays on integer glyph steps.
  Integer steps keep alpha/bitmap sampling deterministic and avoid
  additional blur in the OTA percentage caption.
**/
STATIC
UINT16
ChooseCaptionFontSize (
  IN UINTN  ScreenHeight
  )
{
  UINTN  Scale;

  Scale = 1;
  //
  // Android OTA caption should keep 1x glyph sizing below 4K so the dynamic
  // percentage visually matches the baked "Updating SHIELD:" text in 1080p
  // assets. Only 4K panels use the larger step.
  //
  if (FixedPcdGetBool (PcdBootAndroidImage)) {
    if (ScreenHeight >= CAPTION_4K_SCREEN_HEIGHT) {
      Scale = ANDROID_OTA_4K_PERCENT_SCALE;
    }
  } else {
    if (ScreenHeight > CAPTION_BASE_SCREEN_HEIGHT) {
      Scale = 2;
    }
  }

  //
  // Keep integer scaling from the native 32px glyph cell. Android OTA uses
  // a larger 4K step for clearer percentage readout at TV viewing distance.
  //
  return (UINT16)(NVIDIA_SANS_CAPTION_SOURCE_GLYPH_HEIGHT * Scale);
}

STATIC
VOID
ConfigureCaptionFontInfo (
  IN UINT16   FontSize,
  IN BOOLEAN  PreferNvidiaSans
  )
{
  EFI_STATUS             Status;
  EFI_FONT_HANDLE        FontHandle;
  EFI_FONT_DISPLAY_INFO  *StringInfoOut;
  UINTN                  FontNameBufferBytes;
  BOOLEAN                ExactNativeSize;

  ZeroMem (&mCaptionFontInfo, sizeof (mCaptionFontInfo));
  mCaptionFontInfo.Info.ForegroundColor.Blue     = ANDROID_OTA_PERCENT_TEXT_B;
  mCaptionFontInfo.Info.ForegroundColor.Green    = ANDROID_OTA_PERCENT_TEXT_G;
  mCaptionFontInfo.Info.ForegroundColor.Red      = ANDROID_OTA_PERCENT_TEXT_R;
  mCaptionFontInfo.Info.ForegroundColor.Reserved = 0x00;

  if (PreferNvidiaSans && (mHiiFont != NULL) && !EFI_ERROR (EnsureNvidiaSansCaptionFontPackage ())) {
    FontNameBufferBytes = sizeof (mCaptionFontInfo.Info.FontInfo.FontName) + sizeof (mCaptionFontInfo.ExtraFontNameChars);
    UnicodeSPrint (
      mCaptionFontInfo.Info.FontInfo.FontName,
      FontNameBufferBytes,
      NVIDIA_SANS_CAPTION_FONT_FAMILY
      );
    ExactNativeSize = (FontSize == NVIDIA_SANS_CAPTION_SOURCE_GLYPH_HEIGHT);
    //
    // Prefer the packaged native cell when possible. Only enable RESIZE
    // for intentional integer multiples (e.g. 64 on 4K).
    //
    mCaptionFontInfo.Info.FontInfoMask = EFI_FONT_INFO_RESTYLE;
    if (!ExactNativeSize) {
      mCaptionFontInfo.Info.FontInfoMask |= EFI_FONT_INFO_RESIZE;
    }

    mCaptionFontInfo.Info.FontInfo.FontStyle = EFI_HII_FONT_STYLE_NORMAL;
    mCaptionFontInfo.Info.FontInfo.FontSize  = FontSize;
    FontHandle                               = NULL;
    StringInfoOut                            = NULL;
    Status                                   = mHiiFont->GetFontInfo (
                                                           mHiiFont,
                                                           &FontHandle,
                                                           &mCaptionFontInfo.Info,
                                                           &StringInfoOut,
                                                           L"Updating SHIELD: 100%"
                                                           );
    if (!EFI_ERROR (Status)) {
      if (StringInfoOut != NULL) {
        FreePool (StringInfoOut);
      }

      return;
    }

    if (StringInfoOut != NULL) {
      FreePool (StringInfoOut);
    }
  }

  //
  // Native-font fallback path: keep requested size, but avoid forcing style
  // bits that some firmware font engines reject at large sizes.
  //
  mCaptionFontInfo.Info.FontInfoMask         = EFI_FONT_INFO_ANY_FONT | EFI_FONT_INFO_RESIZE;
  mCaptionFontInfo.Info.FontInfo.FontStyle   = EFI_HII_FONT_STYLE_NORMAL;
  mCaptionFontInfo.Info.FontInfo.FontSize    = FontSize;
  mCaptionFontInfo.Info.FontInfo.FontName[0] = L'\0';
}

/**
  Build the full layout caption text ("%s%3u%% ").

  This helper is used for width/position calculations.

  @param[in]  DisplayProgress  Progress value to append.
  @param[out] CaptionText      Allocated NUL-terminated caption string.

  @retval EFI_SUCCESS           CaptionText created.
  @retval EFI_OUT_OF_RESOURCES  Allocation failed.
**/
STATIC
EFI_STATUS
BuildFullLayoutCaptionText (
  IN  UINTN   DisplayProgress,
  OUT CHAR16  **CaptionText
  )
{
  UINTN  BufferSize;

  if (CaptionText == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *CaptionText = NULL;
  BufferSize   = StrSize (mCaption) + (10 * sizeof (CHAR16));
  *CaptionText = AllocateZeroPool (BufferSize);
  if (*CaptionText == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  UnicodeSPrint (
    *CaptionText,
    BufferSize,
    L"%s%3u%% ",
    mCaption,
    (UINT32)DisplayProgress
    );
  return EFI_SUCCESS;
}

/**
  Build the rendered dynamic progress text.

  Android OTA builds render only the trailing percentage because the
  "Updating SHIELD:" label is baked into the bitmap.
**/
STATIC
EFI_STATUS
BuildProgressCaptionText (
  IN  UINTN   DisplayProgress,
  OUT CHAR16  **CaptionText
  )
{
  UINTN  BufferSize;

  if (CaptionText == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *CaptionText = NULL;
  if (FixedPcdGetBool (PcdBootAndroidImage)) {
    BufferSize = 16 * sizeof (CHAR16);
  } else {
    BufferSize = StrSize (mCaption) + (10 * sizeof (CHAR16));
  }

  *CaptionText = AllocateZeroPool (BufferSize);
  if (*CaptionText == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (FixedPcdGetBool (PcdBootAndroidImage)) {
    UnicodeSPrint (
      *CaptionText,
      BufferSize,
      L" %u%%",
      (UINT32)DisplayProgress
      );
  } else {
    UnicodeSPrint (
      *CaptionText,
      BufferSize,
      L"%s%3u%% ",
      mCaption,
      (UINT32)DisplayProgress
      );
  }

  return EFI_SUCCESS;
}

/**
  Measure caption pixel extents using HII font rendering.

  @param[in]  CaptionText   Caption to measure.
  @param[out] CaptionWidth  Measured width in pixels.
  @param[out] CaptionHeight Measured height in pixels.

  @retval EFI_SUCCESS           Size measured.
  @retval EFI_INVALID_PARAMETER Input invalid.
  @retval Others               Underlying HII font error.
**/
STATIC
EFI_STATUS
MeasureCaptionPixelSize (
  IN  CONST CHAR16  *CaptionText,
  OUT UINTN         *CaptionWidth,
  OUT UINTN         *CaptionHeight
  )
{
  EFI_STATUS        Status;
  EFI_IMAGE_OUTPUT  *Blt;
  EFI_HII_ROW_INFO  *RowInfoArray;
  UINTN             RowInfoArraySize;

  if ((CaptionText == NULL) || (CaptionWidth == NULL) || (CaptionHeight == NULL) || (mHiiFont == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Blt              = NULL;
  RowInfoArray     = NULL;
  RowInfoArraySize = 0;
  *CaptionWidth    = 0;
  *CaptionHeight   = 0;

  Status = mHiiFont->StringToImage (
                       mHiiFont,
                       EFI_HII_IGNORE_IF_NO_GLYPH | EFI_HII_IGNORE_LINE_BREAK,
                       (EFI_STRING)CaptionText,
                       &mCaptionFontInfo.Info,
                       &Blt,
                       0,
                       0,
                       &RowInfoArray,
                       &RowInfoArraySize,
                       NULL
                       );
  if (!EFI_ERROR (Status)) {
    if (RowInfoArraySize > 0) {
      *CaptionWidth  = RowInfoArray[0].LineWidth;
      *CaptionHeight = RowInfoArray[0].LineHeight;
    } else if (Blt != NULL) {
      *CaptionWidth  = Blt->Width;
      *CaptionHeight = Blt->Height;
    }
  }

  if (RowInfoArray != NULL) {
    FreePool (RowInfoArray);
  }

  if (Blt != NULL) {
    if (Blt->Image.Bitmap != NULL) {
      FreePool (Blt->Image.Bitmap);
    }

    FreePool (Blt);
  }

  return Status;
}

/**
  Render caption directly to the GOP framebuffer using HII Font.

  @param[in] DisplayProgress Current displayed progress percentage.

  @retval EFI_SUCCESS  Caption rendered.
  @retval Others       Render failed.
**/
STATIC
EFI_STATUS
RenderCaptionWithHiiFont (
  IN UINTN  DisplayProgress
  )
{
  EFI_STATUS                     Status;
  EFI_IMAGE_OUTPUT               *Blt;
  EFI_HII_ROW_INFO               *RowInfoArray;
  UINTN                          RowInfoArraySize;
  CHAR16                         *CaptionText;
  UINTN                          ScreenWidth;
  UINTN                          ScreenHeight;
  UINTN                          RenderWidth;
  UINTN                          RenderHeight;
  UINTN                          DesiredHeight;
  UINTN                          Upscale;
  UINTN                          SourceWidth;
  UINTN                          SourceHeight;
  UINTN                          SourceStride;
  UINTN                          ClipWidth;
  UINTN                          ClipHeight;
  UINTN                          SrcY;
  UINTN                          SrcX;
  UINTN                          DstY;
  UINTN                          DstX;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *SourceBitmap;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *ScaledBitmap;

  if ((mHiiFont == NULL) || (mGop == NULL) || (mGop->Mode == NULL) || (mGop->Mode->Info == NULL)) {
    return EFI_NOT_READY;
  }

  Blt              = NULL;
  RowInfoArray     = NULL;
  RowInfoArraySize = 0;
  CaptionText      = NULL;
  ScaledBitmap     = NULL;
  Status           = BuildProgressCaptionText (DisplayProgress, &CaptionText);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = mHiiFont->StringToImage (
                       mHiiFont,
                       EFI_HII_IGNORE_IF_NO_GLYPH |
                       EFI_HII_IGNORE_LINE_BREAK |
                       EFI_HII_OUT_FLAG_CLIP,
                       (EFI_STRING)CaptionText,
                       &mCaptionFontInfo.Info,
                       &Blt,
                       0,
                       0,
                       &RowInfoArray,
                       &RowInfoArraySize,
                       NULL
                       );

  if (!EFI_ERROR (Status) && (Blt != NULL)) {
    ScreenWidth  = mGop->Mode->Info->HorizontalResolution;
    ScreenHeight = mGop->Mode->Info->VerticalResolution;

    RenderWidth  = Blt->Width;
    RenderHeight = Blt->Height;
    if (RowInfoArraySize > 0) {
      if (RowInfoArray[0].LineWidth > 0) {
        RenderWidth = RowInfoArray[0].LineWidth;
      }

      if (RowInfoArray[0].LineHeight > 0) {
        RenderHeight = RowInfoArray[0].LineHeight;
      }
    }

    SourceBitmap = Blt->Image.Bitmap;
    SourceWidth  = RenderWidth;
    SourceHeight = RenderHeight;
    SourceStride = Blt->Width;
    Upscale      = 1;
    if (FixedPcdGetBool (PcdBootAndroidImage)) {
      DesiredHeight = ChooseCaptionFontSize (ScreenHeight);
      if (DesiredHeight < CAPTION_MIN_FONT_SIZE) {
        DesiredHeight = CAPTION_MIN_FONT_SIZE;
      }

      if ((SourceHeight > 0) && (SourceHeight < DesiredHeight)) {
        Upscale = (DesiredHeight + SourceHeight - 1) / SourceHeight;
        if (Upscale > 8) {
          Upscale = 8;
        }
      }
    }

    if ((Upscale > 1) &&
        (SourceBitmap != NULL) &&
        (SourceWidth > 0) &&
        (SourceHeight > 0))
    {
      UINTN  ScaledWidth;
      UINTN  ScaledHeight;

      ScaledWidth  = SourceWidth * Upscale;
      ScaledHeight = SourceHeight * Upscale;
      ScaledBitmap = AllocateZeroPool (ScaledWidth * ScaledHeight * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
      if (ScaledBitmap == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
      } else {
        for (SrcY = 0; SrcY < SourceHeight; SrcY++) {
          for (SrcX = 0; SrcX < SourceWidth; SrcX++) {
            EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Pixel;

            Pixel = SourceBitmap[(SrcY * SourceStride) + SrcX];
            for (DstY = 0; DstY < Upscale; DstY++) {
              for (DstX = 0; DstX < Upscale; DstX++) {
                ScaledBitmap[((SrcY * Upscale + DstY) * ScaledWidth) + (SrcX * Upscale + DstX)] = Pixel;
              }
            }
          }
        }

        SourceBitmap = ScaledBitmap;
        SourceWidth  = ScaledWidth;
        SourceHeight = ScaledHeight;
        SourceStride = ScaledWidth;
      }
    }

    if ((mCaptionPixelLeft >= ScreenWidth) || (mCaptionPixelTop >= ScreenHeight)) {
      Status = EFI_NOT_READY;
    } else {
      ClipWidth  = ScreenWidth - mCaptionPixelLeft;
      ClipHeight = ScreenHeight - mCaptionPixelTop;
      if (ClipWidth > SourceWidth) {
        ClipWidth = SourceWidth;
      }

      if (ClipHeight > SourceHeight) {
        ClipHeight = SourceHeight;
      }

      if ((ClipWidth == 0) ||
          (ClipHeight == 0) ||
          (SourceBitmap == NULL) ||
          (SourceStride == 0))
      {
        Status = EFI_NOT_READY;
      } else {
        Status = mGop->Blt (
                         mGop,
                         SourceBitmap,
                         EfiBltBufferToVideo,
                         0,
                         0,
                         mCaptionPixelLeft,
                         mCaptionPixelTop,
                         ClipWidth,
                         ClipHeight,
                         SourceStride * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                         );
      }
    }
  }

  if (RowInfoArray != NULL) {
    FreePool (RowInfoArray);
  }

  if (Blt != NULL) {
    if (Blt->Image.Bitmap != NULL) {
      FreePool (Blt->Image.Bitmap);
    }

    FreePool (Blt);
  }

  if (ScaledBitmap != NULL) {
    FreePool (ScaledBitmap);
  }

  FreePool (CaptionText);

  return Status;
}

STATIC
VOID
GetNvidiaSansCaptionAlphaGlyphTable (
  OUT CONST NVIDIA_SANS_CAPTION_ALPHA_GLYPH  **GlyphTable,
  OUT UINTN                                  *GlyphCount
  )
{
  if ((GlyphTable == NULL) || (GlyphCount == NULL)) {
    return;
  }

  *GlyphTable = mNvidiaSansCaptionAlphaGlyphsMd;
  *GlyphCount = NVIDIA_SANS_CAPTION_ALPHA_GLYPH_COUNT_MD;
}

STATIC
VOID
GetNvidiaSansCaption4kAlphaGlyphTable (
  OUT CONST NVIDIA_SANS_CAPTION_4K_ALPHA_GLYPH  **GlyphTable,
  OUT UINTN                                     *GlyphCount
  )
{
  if ((GlyphTable == NULL) || (GlyphCount == NULL)) {
    return;
  }

  *GlyphTable = mNvidiaSansCaption4kAlphaGlyphsMd;
  *GlyphCount = NVIDIA_SANS_CAPTION_4K_ALPHA_GLYPH_COUNT_MD;
}

STATIC
CONST NVIDIA_SANS_CAPTION_ALPHA_GLYPH *
FindNvidiaSansCaptionAlphaGlyph (
  IN CHAR16  Char
  )
{
  CONST NVIDIA_SANS_CAPTION_ALPHA_GLYPH  *GlyphTable;
  UINTN                                  GlyphCount;
  UINTN                                  GlyphIndex;

  GlyphTable = NULL;
  GlyphCount = 0;
  GetNvidiaSansCaptionAlphaGlyphTable (&GlyphTable, &GlyphCount);
  if ((GlyphTable == NULL) || (GlyphCount == 0)) {
    return NULL;
  }

  for (GlyphIndex = 0; GlyphIndex < GlyphCount; GlyphIndex++) {
    if (GlyphTable[GlyphIndex].Char == Char) {
      return &GlyphTable[GlyphIndex];
    }
  }

  return NULL;
}

STATIC
CONST NVIDIA_SANS_CAPTION_4K_ALPHA_GLYPH *
FindNvidiaSansCaption4kAlphaGlyph (
  IN CHAR16  Char
  )
{
  CONST NVIDIA_SANS_CAPTION_4K_ALPHA_GLYPH  *GlyphTable;
  UINTN                                     GlyphCount;
  UINTN                                     GlyphIndex;

  GlyphTable = NULL;
  GlyphCount = 0;
  GetNvidiaSansCaption4kAlphaGlyphTable (&GlyphTable, &GlyphCount);
  if ((GlyphTable == NULL) || (GlyphCount == 0)) {
    return NULL;
  }

  for (GlyphIndex = 0; GlyphIndex < GlyphCount; GlyphIndex++) {
    if (GlyphTable[GlyphIndex].Char == Char) {
      return &GlyphTable[GlyphIndex];
    }
  }

  return NULL;
}

STATIC
UINT8
NvidiaSansCaptionGlyphAlphaValue (
  IN CONST NVIDIA_SANS_CAPTION_ALPHA_GLYPH  *Glyph,
  IN UINTN                                  Row,
  IN UINTN                                  Col
  )
{
  if ((Glyph == NULL) ||
      (Row >= NVIDIA_SANS_CAPTION_SOURCE_GLYPH_HEIGHT) ||
      (Col >= NVIDIA_SANS_CAPTION_SOURCE_GLYPH_WIDTH))
  {
    return 0;
  }

  return Glyph->Alpha[(Row * NVIDIA_SANS_CAPTION_SOURCE_GLYPH_WIDTH) + Col];
}

STATIC
UINT8
NvidiaSansCaptionScaledAlphaValue (
  IN CONST NVIDIA_SANS_CAPTION_ALPHA_GLYPH  *Glyph,
  IN UINTN                                  Row,
  IN UINTN                                  Col,
  IN UINTN                                  ScaleY,
  IN UINTN                                  ScaleX,
  IN UINTN                                  Scale
  )
{
  UINTN  FracX;
  UINTN  FracY;
  UINTN  A00;
  UINTN  A10;
  UINTN  A01;
  UINTN  A11;
  UINTN  Top;
  UINTN  Bottom;

  if ((Glyph == NULL) || (Scale == 0)) {
    return 0;
  }

  if (Scale <= 1U) {
    return NvidiaSansCaptionGlyphAlphaValue (Glyph, Row, Col);
  }

  FracX = ((2U * ScaleX + 1U) * 256U) / (2U * Scale);
  FracY = ((2U * ScaleY + 1U) * 256U) / (2U * Scale);

  A00 = NvidiaSansCaptionGlyphAlphaValue (Glyph, Row, Col);
  A10 = (Col + 1U < NVIDIA_SANS_CAPTION_SOURCE_GLYPH_WIDTH) ? NvidiaSansCaptionGlyphAlphaValue (Glyph, Row, Col + 1U) : 0U;
  A01 = (Row + 1U < NVIDIA_SANS_CAPTION_SOURCE_GLYPH_HEIGHT) ? NvidiaSansCaptionGlyphAlphaValue (Glyph, Row + 1U, Col) : 0U;
  A11 = ((Row + 1U < NVIDIA_SANS_CAPTION_SOURCE_GLYPH_HEIGHT) &&
         (Col + 1U < NVIDIA_SANS_CAPTION_SOURCE_GLYPH_WIDTH))
          ? NvidiaSansCaptionGlyphAlphaValue (Glyph, Row + 1U, Col + 1U)
          : 0U;

  Top    = ((A00 * (256U - FracX)) + (A10 * FracX) + 128U) >> 8;
  Bottom = ((A01 * (256U - FracX)) + (A11 * FracX) + 128U) >> 8;
  return (UINT8)(((Top * (256U - FracY)) + (Bottom * FracY) + 128U) >> 8);
}

STATIC
UINT8
NvidiaSansCaption4kGlyphAlphaValue (
  IN CONST NVIDIA_SANS_CAPTION_4K_ALPHA_GLYPH  *Glyph,
  IN UINTN                                     Row,
  IN UINTN                                     Col
  )
{
  if ((Glyph == NULL) ||
      (Row >= NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_HEIGHT) ||
      (Col >= NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_WIDTH))
  {
    return 0;
  }

  return Glyph->Alpha[(Row * NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_WIDTH) + Col];
}

STATIC
UINT8
NvidiaSansCaption4kScaledAlphaValue (
  IN CONST NVIDIA_SANS_CAPTION_4K_ALPHA_GLYPH  *Glyph,
  IN UINTN                                     Row,
  IN UINTN                                     Col,
  IN UINTN                                     ScaleY,
  IN UINTN                                     ScaleX,
  IN UINTN                                     Scale
  )
{
  UINTN  FracX;
  UINTN  FracY;
  UINTN  A00;
  UINTN  A10;
  UINTN  A01;
  UINTN  A11;
  UINTN  Top;
  UINTN  Bottom;

  if ((Glyph == NULL) || (Scale == 0)) {
    return 0;
  }

  if (Scale <= 1U) {
    return NvidiaSansCaption4kGlyphAlphaValue (Glyph, Row, Col);
  }

  FracX = ((2U * ScaleX + 1U) * 256U) / (2U * Scale);
  FracY = ((2U * ScaleY + 1U) * 256U) / (2U * Scale);

  A00 = NvidiaSansCaption4kGlyphAlphaValue (Glyph, Row, Col);
  A10 = (Col + 1U < NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_WIDTH) ? NvidiaSansCaption4kGlyphAlphaValue (Glyph, Row, Col + 1U) : 0U;
  A01 = (Row + 1U < NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_HEIGHT) ? NvidiaSansCaption4kGlyphAlphaValue (Glyph, Row + 1U, Col) : 0U;
  A11 = ((Row + 1U < NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_HEIGHT) &&
         (Col + 1U < NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_WIDTH))
          ? NvidiaSansCaption4kGlyphAlphaValue (Glyph, Row + 1U, Col + 1U)
          : 0U;

  Top    = ((A00 * (256U - FracX)) + (A10 * FracX) + 128U) >> 8;
  Bottom = ((A01 * (256U - FracX)) + (A11 * FracX) + 128U) >> 8;
  return (UINT8)(((Top * (256U - FracY)) + (Bottom * FracY) + 128U) >> 8);
}

STATIC
VOID
BlendCaptionPixel (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *DstPixel,
  IN     CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *SrcPixel,
  IN     UINT8                                Alpha
  )
{
  UINTN  InvAlpha;

  if ((DstPixel == NULL) || (SrcPixel == NULL)) {
    return;
  }

  if (Alpha >= 0xFF) {
    *DstPixel = *SrcPixel;
    return;
  }

  if (Alpha == 0) {
    return;
  }

  InvAlpha           = 0xFF - Alpha;
  DstPixel->Blue     = (UINT8)((DstPixel->Blue * InvAlpha + SrcPixel->Blue * Alpha + 127U) / 255U);
  DstPixel->Green    = (UINT8)((DstPixel->Green * InvAlpha + SrcPixel->Green * Alpha + 127U) / 255U);
  DstPixel->Red      = (UINT8)((DstPixel->Red * InvAlpha + SrcPixel->Red * Alpha + 127U) / 255U);
  DstPixel->Reserved = 0;
}

STATIC
EFI_STATUS
RenderCaptionWithBuiltinGlyphs (
  IN UINTN  DisplayProgress
  )
{
  EFI_STATUS                                Status;
  CHAR16                                    *CaptionText;
  UINTN                                     ScreenWidth;
  UINTN                                     ScreenHeight;
  UINTN                                     FontSize;
  UINTN                                     Scale;
  UINTN                                     GlyphSourceWidth;
  UINTN                                     GlyphSourceHeight;
  UINTN                                     GlyphAdvanceX;
  UINTN                                     CaptionChars;
  UINTN                                     RenderWidth;
  UINTN                                     ClearWidth;
  UINTN                                     RenderHeight;
  UINTN                                     ClipWidth;
  UINTN                                     ClipHeight;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL             *RenderBuffer;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL             TextPixel;
  UINTN                                     CharIndex;
  UINTN                                     SrcY;
  UINTN                                     SrcX;
  UINTN                                     ScaleY;
  UINTN                                     ScaleX;
  UINTN                                     GlyphStartX;
  UINTN                                     DstX;
  UINTN                                     DstY;
  CONST NVIDIA_SANS_CAPTION_4K_ALPHA_GLYPH  *AlphaGlyph4k;
  CONST NVIDIA_SANS_CAPTION_ALPHA_GLYPH     *AlphaGlyph;
  UINTN                                     BufferSize;
  UINT8                                     Alpha;
  BOOLEAN                                   Use4kAlphaGlyphs;

  if ((mGop == NULL) || (mGop->Mode == NULL) || (mGop->Mode->Info == NULL)) {
    return EFI_NOT_READY;
  }

  CaptionText = NULL;
  Status      = BuildProgressCaptionText (DisplayProgress, &CaptionText);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ScreenWidth  = mGop->Mode->Info->HorizontalResolution;
  ScreenHeight = mGop->Mode->Info->VerticalResolution;
  if ((mCaptionPixelLeft >= ScreenWidth) || (mCaptionPixelTop >= ScreenHeight)) {
    FreePool (CaptionText);
    return EFI_NOT_READY;
  }

  FontSize         = ChooseCaptionFontSize (ScreenHeight);
  Use4kAlphaGlyphs = (ScreenHeight >= CAPTION_4K_SCREEN_HEIGHT);
  if (Use4kAlphaGlyphs) {
    GlyphSourceWidth  = NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_WIDTH;
    GlyphSourceHeight = NVIDIA_SANS_CAPTION_4K_SOURCE_GLYPH_HEIGHT;
    GlyphAdvanceX     = NVIDIA_SANS_CAPTION_4K_GLYPH_ADVANCE_X;
  } else {
    GlyphSourceWidth  = NVIDIA_SANS_CAPTION_SOURCE_GLYPH_WIDTH;
    GlyphSourceHeight = NVIDIA_SANS_CAPTION_SOURCE_GLYPH_HEIGHT;
    GlyphAdvanceX     = NVIDIA_SANS_CAPTION_GLYPH_ADVANCE_X;
  }

  Scale = FontSize / GlyphSourceHeight;
  if (Scale == 0) {
    Scale = 1;
  }

  CaptionChars = StrLen (CaptionText);
  if (CaptionChars == 0) {
    RenderWidth = 0;
  } else {
    //
    // Advance controls inter-glyph spacing, but the trailing glyph can extend
    // beyond that advance (notably '%' in NVIDIA Sans). Include one full glyph
    // cell at the tail so rightmost pixels are never clipped.
    //
    RenderWidth = (((CaptionChars - 1) * GlyphAdvanceX) + GlyphSourceWidth) * Scale;
  }

  //
  // Always refresh the full dynamic percentage slot (" 100%") so transitions
  // like 81 -> 82 never leave stale strokes behind.
  //
  ClearWidth = (((CAPTION_TAIL_WIDTH - 1) * GlyphAdvanceX) + GlyphSourceWidth) * Scale;
  if (ClearWidth < RenderWidth) {
    ClearWidth = RenderWidth;
  }

  RenderHeight = GlyphSourceHeight * Scale;
  if ((RenderWidth == 0) || (RenderHeight == 0)) {
    FreePool (CaptionText);
    return EFI_INVALID_PARAMETER;
  }

  ClipWidth  = ScreenWidth - mCaptionPixelLeft;
  ClipHeight = ScreenHeight - mCaptionPixelTop;
  if (ClipWidth > ClearWidth) {
    ClipWidth = ClearWidth;
  }

  if (ClipHeight > RenderHeight) {
    ClipHeight = RenderHeight;
  }

  if ((ClipWidth == 0) || (ClipHeight == 0)) {
    FreePool (CaptionText);
    return EFI_NOT_READY;
  }

  BufferSize   = ClipWidth * ClipHeight * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
  RenderBuffer = AllocateZeroPool (BufferSize);
  if (RenderBuffer == NULL) {
    FreePool (CaptionText);
    return EFI_OUT_OF_RESOURCES;
  }

  TextPixel.Blue     = ANDROID_OTA_PERCENT_TEXT_B;
  TextPixel.Green    = ANDROID_OTA_PERCENT_TEXT_G;
  TextPixel.Red      = ANDROID_OTA_PERCENT_TEXT_R;
  TextPixel.Reserved = 0;

  for (CharIndex = 0; CharIndex < CaptionChars; CharIndex++) {
    //
    // Prefer anti-aliased coverage masks generated from NVIDIA Sans TTF.
    // On 4K panels, use the dedicated 48x56 alpha set to avoid magnifying
    // a smaller source glyph. No bitmap fallback path is used.
    //
    AlphaGlyph4k = NULL;
    AlphaGlyph   = NULL;
    if (Use4kAlphaGlyphs) {
      AlphaGlyph4k = FindNvidiaSansCaption4kAlphaGlyph (CaptionText[CharIndex]);
      if (AlphaGlyph4k == NULL) {
        continue;
      }
    } else {
      AlphaGlyph = FindNvidiaSansCaptionAlphaGlyph (CaptionText[CharIndex]);
      if (AlphaGlyph == NULL) {
        continue;
      }
    }

    GlyphStartX = CharIndex * (GlyphAdvanceX * Scale);
    for (SrcY = 0; SrcY < GlyphSourceHeight; SrcY++) {
      for (SrcX = 0; SrcX < GlyphSourceWidth; SrcX++) {
        for (ScaleY = 0; ScaleY < Scale; ScaleY++) {
          DstY = (SrcY * Scale) + ScaleY;
          if (DstY >= ClipHeight) {
            continue;
          }

          for (ScaleX = 0; ScaleX < Scale; ScaleX++) {
            DstX = GlyphStartX + (SrcX * Scale) + ScaleX;
            if (DstX >= ClipWidth) {
              continue;
            }

            if (Use4kAlphaGlyphs) {
              Alpha = NvidiaSansCaption4kScaledAlphaValue (AlphaGlyph4k, SrcY, SrcX, ScaleY, ScaleX, Scale);
              if (Alpha == 0U) {
                continue;
              }
            } else if (AlphaGlyph != NULL) {
              Alpha = NvidiaSansCaptionScaledAlphaValue (AlphaGlyph, SrcY, SrcX, ScaleY, ScaleX, Scale);
              if (Alpha == 0U) {
                continue;
              }
            }

            BlendCaptionPixel (&RenderBuffer[(DstY * ClipWidth) + DstX], &TextPixel, Alpha);
          }
        }
      }
    }
  }

  Status = mGop->Blt (
                   mGop,
                   RenderBuffer,
                   EfiBltBufferToVideo,
                   0,
                   0,
                   mCaptionPixelLeft,
                   mCaptionPixelTop,
                   ClipWidth,
                   ClipHeight,
                   ClipWidth * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                   );

  FreePool (RenderBuffer);
  FreePool (CaptionText);
  return Status;
}

/**
  Function indicates the current completion progress of a firmware update.
  Platform may override with its own specific function.

  @param[in] Completion  A value between 0 and 100 indicating the current
                         completion progress of a firmware update.  This
                         value must the the same or higher than previous
                         calls to this service.  The first call of 0 or a
                         value of 0 after reaching a value of 100 resets
                         the progress indicator to 0.
  @param[in] Color       Color of the progress indicator.  Only used when
                         Completion is 0 to set the color of the progress
                         indicator.  If Color is NULL, then the default color
                         is used.

  @retval EFI_SUCCESS            Progress displayed successfully.
  @retval EFI_INVALID_PARAMETER  Completion is not in range 0..100.
  @retval EFI_INVALID_PARAMETER  Completion is less than Completion value from
                                 a previous call to this service.
  @retval EFI_NOT_READY          The device used to indicate progress is not
                                 available.
**/
EFI_STATUS
EFIAPI
DisplayUpdateProgress (
  IN UINTN                                Completion,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  *Color       OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINTN       DisplayProgress;
  UINTN       FilledPx;

  //
  // Check range
  //
  if (Completion > 100) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Locate GOP once (lazy cache). Without GOP the caller has no usable
  // framebuffer for progress display, so bail with EFI_NOT_READY just
  // like the stock MdeModulePkg Graphics implementation.
  //
  if (mGop == NULL) {
    Status = gBS->LocateProtocol (
                    &gEfiGraphicsOutputProtocolGuid,
                    NULL,
                    (VOID **)&mGop
                    );
    if (EFI_ERROR (Status)) {
      mGop = NULL;
      return EFI_NOT_READY;
    }
  }

  if ((mGop->Mode == NULL) || (mGop->Mode->Info == NULL)) {
    return EFI_NOT_READY;
  }

  //
  // Do special init on first call of each progress session
  //
  if (mPreviousProgress == 100) {
    //
    // Restore caption-prefix layout baseline for the new progress session.
    //
    if (FixedPcdGetBool (PcdBootAndroidImage)) {
      mCaption = L"Updating SHIELD: ";
      //
      // If a driver staging a firmware update published
      // gNVIDIAFwUpdateSplashProtocolGuid, pull the session-specific
      // ProgressBase (e.g. 80) off it. The caption prefix is intentionally
      // fixed because the static label is baked into android_ota*.bmp.
      //
      {
        NVIDIA_FW_UPDATE_SPLASH_PROTOCOL  *Splash;

        mProgressBase = 0;

        if (!EFI_ERROR (
               gBS->LocateProtocol (
                      &gNVIDIAFwUpdateSplashProtocolGuid,
                      NULL,
                      (VOID **)&Splash
                      )
               ))
        {
          if (Splash->ProgressBase < 100) {
            mProgressBase = Splash->ProgressBase;
          }
        }
      }
    } else {
      mCaption      = L"Update Progress - ";
      mProgressBase = 0;
    }

    mCaptionLen = StrLen (mCaption);

    //
    // Stash a pixel color for the GOP fill. A NULL Color falls back to the
    // android_ota_4k.bmp yellow-green; FMP normally supplies PcdFmpDeviceProgressColor.
    //
    if (Color == NULL) {
      mBarFillPixel.Blue     = ANDROID_OTA_PROGRESS_GREEN_B;
      mBarFillPixel.Green    = ANDROID_OTA_PROGRESS_GREEN_G;
      mBarFillPixel.Red      = ANDROID_OTA_PROGRESS_GREEN_R;
      mBarFillPixel.Reserved = 0x00;
    } else {
      mBarFillPixel = Color->Pixel;
    }

    //
    // Query the text console for its current row/column grid. The
    // caption fallback is rendered via Print on the first text row below
    // the pixel-positioned bar.
    //
    {
      UINTN   MaxColumns = 0;
      UINTN   MaxRows    = 0;
      UINTN   ScreenW    = mGop->Mode->Info->HorizontalResolution;
      UINTN   ScreenH    = mGop->Mode->Info->VerticalResolution;
      UINTN   CellH;
      UINT16  FontSize;

      gST->ConOut->QueryMode (
                     gST->ConOut,
                     (UINTN)gST->ConOut->Mode->Mode,
                     &MaxColumns,
                     &MaxRows
                     );

      if (MaxRows == 0) {
        return EFI_NOT_READY;
      }

      CellH = ScreenH / MaxRows;

      {
        UINTN  TextWidth = mCaptionLen + CAPTION_TAIL_WIDTH;
        UINTN  TextStartCol;

        TextStartCol = (MaxColumns > TextWidth) ? (MaxColumns - TextWidth) / 2 : 0;
        if (FixedPcdGetBool (PcdBootAndroidImage)) {
          //
          // Static "Updating SHIELD:" is in the image; only draw "%3u%% ".
          //
          mTextCol = TextStartCol + mCaptionLen;
        } else {
          mTextCol = TextStartCol;
        }
      }

      mBarPixelWidth  = (ScreenW * BAR_PERCENT_OF_SCREEN) / 100;
      mBarPixelHeight = (ScreenH * BAR_HEIGHT_PERMILLE) / 1000;
      if (mBarPixelHeight < MIN_BAR_PIXEL_HEIGHT) {
        mBarPixelHeight = MIN_BAR_PIXEL_HEIGHT;
      }

      //
      // Cap to one text-cell row so the bar can never bleed into the
      // caption row's pixel range.
      //
      if (mBarPixelHeight > CellH) {
        mBarPixelHeight = CellH;
      }

      mBarPixelLeft = (ScreenW > mBarPixelWidth) ? (ScreenW - mBarPixelWidth) / 2 : 0;
      mBarPixelTop  = (ScreenH * BAR_TOP_PERMILLE) / 1000;
      mProgressRow  = mBarPixelTop / CellH;

      //
      // Default caption path: text-console Print().
      //
      mUseHiiFontCaption = FALSE;
      mHiiFont           = NULL;
      mCaptionPixelLeft  = mBarPixelLeft;
      mCaptionPixelTop   = mBarPixelTop + CellH;
      if (FixedPcdGetBool (PcdBootAndroidImage)) {
        mCaptionPixelLeft = (ScreenW * ANDROID_OTA_PERCENT_LEFT) / ANDROID_OTA_REFERENCE_WIDTH;
        mCaptionPixelTop  = (ScreenH * ANDROID_OTA_PERCENT_TOP) / ANDROID_OTA_REFERENCE_HEIGHT;
      } else if (ScreenH > CAPTION_BASE_SCREEN_HEIGHT) {
        UINTN  CaptionGapPx;

        CaptionGapPx = (ScreenH * CAPTION_BAR_GAP_PERMILLE_HIGHRES) / 1000;
        if (CaptionGapPx < CAPTION_BAR_GAP_MIN_HIGHRES) {
          CaptionGapPx = CAPTION_BAR_GAP_MIN_HIGHRES;
        }

        mCaptionPixelTop = mBarPixelTop + mBarPixelHeight + CaptionGapPx;
      }

      //
      // Preferred caption path: HII font with native/integer-scaled NVIDIA Sans.
      //
      Status = gBS->LocateProtocol (&gEfiHiiFontProtocolGuid, NULL, (VOID **)&mHiiFont);
      if (!EFI_ERROR (Status)) {
        CHAR16  *CaptionText;
        UINTN   CaptionPixelWidth;
        UINTN   CaptionPixelHeight;

        FontSize = ChooseCaptionFontSize (ScreenH);
        if (FontSize < CAPTION_MIN_FONT_SIZE) {
          FontSize = CAPTION_MIN_FONT_SIZE;
        }

        //
        // Android OTA tries native platform font first for smoother glyph
        // edges, while other platforms keep NVIDIA Sans preference.
        //
        ConfigureCaptionFontInfo (FontSize, !FixedPcdGetBool (PcdBootAndroidImage));
        CaptionText        = NULL;
        CaptionPixelWidth  = 0;
        CaptionPixelHeight = 0;
        Status             = BuildFullLayoutCaptionText (100, &CaptionText);
        if (!EFI_ERROR (Status)) {
          Status = MeasureCaptionPixelSize (CaptionText, &CaptionPixelWidth, &CaptionPixelHeight);
          if (!EFI_ERROR (Status) && (CaptionPixelWidth > 0)) {
            if (!FixedPcdGetBool (PcdBootAndroidImage)) {
              mCaptionPixelLeft = (ScreenW > CaptionPixelWidth) ? (ScreenW - CaptionPixelWidth) / 2 : 0;
            }

            if ((CaptionPixelHeight > 0) && ((mCaptionPixelTop + CaptionPixelHeight) > ScreenH)) {
              mCaptionPixelTop = (ScreenH > CaptionPixelHeight) ? (ScreenH - CaptionPixelHeight) : 0;
            }

            mUseHiiFontCaption = TRUE;
          } else {
            if (ScreenH > CAPTION_BASE_SCREEN_HEIGHT) {
              //
              // If measurement fails on high-res panels, keep HII enabled and
              // use a conservative width estimate for centering instead of
              // falling back immediately to tiny text-console glyphs on 4K.
              //
              CaptionPixelWidth = (StrLen (mCaption) + CAPTION_TAIL_WIDTH) * FontSize;
              if (CaptionPixelWidth > ScreenW) {
                CaptionPixelWidth = ScreenW;
              }

              if (!FixedPcdGetBool (PcdBootAndroidImage)) {
                mCaptionPixelLeft = (ScreenW > CaptionPixelWidth) ? (ScreenW - CaptionPixelWidth) / 2 : 0;
              }

              mUseHiiFontCaption = TRUE;
            }
          }
        }

        if (CaptionText != NULL) {
          FreePool (CaptionText);
        }
      }
    }

    //
    // Clear previous
    //
    mPreviousProgress = 0;
  }

  //
  // On Android image builds, remap raw FMP completion (0..100) into the
  // displayed window mProgressBase..100 if the firmware-update splash
  // protocol set one; otherwise render Completion as-is. Then dedup and
  // monotonic-check against the previously displayed value.
  //
  if (FixedPcdGetBool (PcdBootAndroidImage)) {
    DisplayProgress = mProgressBase + ((100 - mProgressBase) * Completion) / 100;
  } else {
    DisplayProgress = Completion;
  }

  if (DisplayProgress == mPreviousProgress) {
    return EFI_SUCCESS;
  }

  if (DisplayProgress < mPreviousProgress) {
    DEBUG ((DEBUG_WARN, "WARNING: Completion (%d -> %d) should not be less than Previous (%d)!!!\n", Completion, DisplayProgress, mPreviousProgress));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Draw the bar via GOP: filled segment in the FMP-supplied color,
  // then the dim gray track for the remainder. Two Blt fills per tick
  // keep the bar flicker-free as the percentage advances.
  //
  FilledPx = (DisplayProgress * mBarPixelWidth) / 100;

  if (FilledPx > 0) {
    mGop->Blt (
            mGop,
            &mBarFillPixel,
            EfiBltVideoFill,
            0,
            0,
            mBarPixelLeft,
            mBarPixelTop,
            FilledPx,
            mBarPixelHeight,
            0
            );
  }

  if (FilledPx < mBarPixelWidth) {
    mGop->Blt (
            mGop,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)&mBarTrackPixel,
            EfiBltVideoFill,
            0,
            0,
            mBarPixelLeft + FilledPx,
            mBarPixelTop,
            mBarPixelWidth - FilledPx,
            mBarPixelHeight,
            0
            );
  }

  //
  // Caption rendering:
  //  - Android OTA: single-path NVIDIA Sans glyph renderer generated from TTF.
  //  - Others keep HII font, then legacy text-console fallback.
  //
  Status = EFI_UNSUPPORTED;
  if (FixedPcdGetBool (PcdBootAndroidImage)) {
    Status = RenderCaptionWithBuiltinGlyphs (DisplayProgress);
  } else if (mUseHiiFontCaption) {
    Status = RenderCaptionWithHiiFont (DisplayProgress);
    if (EFI_ERROR (Status)) {
      mUseHiiFontCaption = FALSE;
    }
  }

  if (EFI_ERROR (Status)) {
    if (FixedPcdGetBool (PcdBootAndroidImage)) {
      DEBUG ((DEBUG_WARN, "DisplayUpdateProgress: caption render failed (%r)\n", Status));
    } else {
      UINTN  CurrentAttribute;

      CurrentAttribute = (UINTN)gST->ConOut->Mode->Attribute;
      gST->ConOut->SetAttribute (gST->ConOut, CurrentAttribute);
      gST->ConOut->SetCursorPosition (gST->ConOut, mTextCol, mProgressRow + 1);
      Print (L"%s%3d%% ", mCaption, DisplayProgress);
    }
  }

  mPreviousProgress = DisplayProgress;

  return EFI_SUCCESS;
}
