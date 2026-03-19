/** @file
  Provides services to display completion progress of a firmware update on a
  text console, centered below the boot logo.

  SPDX-FileCopyrightText: Copyright (c) 2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2016, Microsoft Corporation. All rights reserved.<BR>
  Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

//
// Control Style.  Set to 100 so it is reset on first call.
//
STATIC UINTN  mPreviousProgress = 100;

//
// Text foreground color of progress bar
//
STATIC UINTN  mProgressBarForegroundColor;

//
// Cursor position and bar geometry, computed once per progress session
//
STATIC UINTN  mProgressRow = 0;
STATIC UINTN  mProgressCol = 0;
STATIC UINTN  mBarBlocks   = 50;

#define PROGRESS_TEXT_WIDTH    25
#define BAR_PERCENT_OF_SCREEN  40
#define MIN_BAR_BLOCKS         20

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
  UINTN  Index;
  UINTN  CurrentAttribute;

  //
  // Check range
  //
  if (Completion > 100) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check to see if this Completion percentage has already been displayed
  //
  if (Completion == mPreviousProgress) {
    return EFI_SUCCESS;
  }

  //
  // Do special init on first call of each progress session
  //
  if (mPreviousProgress == 100) {
    //
    // Query console geometry and compute bar width / position so that
    // the progress bar is horizontally centered and placed at 80% vertical.
    //
    {
      UINTN  MaxColumns = 0;
      UINTN  MaxRows    = 0;

      gST->ConOut->QueryMode (
                     gST->ConOut,
                     (UINTN)gST->ConOut->Mode->Mode,
                     &MaxColumns,
                     &MaxRows
                     );
      mBarBlocks = (MaxColumns * BAR_PERCENT_OF_SCREEN) / 100;
      if (mBarBlocks < MIN_BAR_BLOCKS) {
        mBarBlocks = MIN_BAR_BLOCKS;
      }

      {
        UINTN  TotalWidth = PROGRESS_TEXT_WIDTH + mBarBlocks;

        mProgressCol = (MaxColumns > TotalWidth) ? (MaxColumns - TotalWidth) / 2 : 0;
      }
      mProgressRow = (MaxRows * 4) / 5;
    }

    //
    // Convert pixel color to text foreground color
    //
    if (Color == NULL) {
      mProgressBarForegroundColor = EFI_WHITE;
    } else {
      mProgressBarForegroundColor = EFI_BLACK;
      if (Color->Pixel.Blue >= 0x40) {
        mProgressBarForegroundColor |= EFI_BLUE;
      }

      if (Color->Pixel.Green >= 0x40) {
        mProgressBarForegroundColor |= EFI_GREEN;
      }

      if (Color->Pixel.Red >= 0x40) {
        mProgressBarForegroundColor |= EFI_RED;
      }

      if ((Color->Pixel.Blue >= 0xC0) || (Color->Pixel.Green >= 0xC0) || (Color->Pixel.Red >= 0xC0)) {
        mProgressBarForegroundColor |= EFI_BRIGHT;
      }

      if (mProgressBarForegroundColor == EFI_BLACK) {
        mProgressBarForegroundColor = EFI_WHITE;
      }
    }

    //
    // Clear previous
    //
    mPreviousProgress = 0;
  }

  //
  // Can not update progress bar if Completion is less than previous
  //
  if (Completion < mPreviousProgress) {
    DEBUG ((DEBUG_WARN, "WARNING: Completion (%d) should not be less than Previous (%d)!!!\n", Completion, mPreviousProgress));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Save current text color
  //
  CurrentAttribute = (UINTN)gST->ConOut->Mode->Attribute;

  //
  // Position cursor at the computed row/column instead of using \r
  //
  gST->ConOut->SetCursorPosition (gST->ConOut, mProgressCol, mProgressRow);

  //
  // Print progress percentage
  //
  Print (L"Update Progress - %3d%% ", Completion);

  //
  // Set progress bar color
  //
  gST->ConOut->SetAttribute (
                 gST->ConOut,
                 EFI_TEXT_ATTR (mProgressBarForegroundColor, EFI_BLACK)
                 );

  //
  // Print completed portion of progress bar (scaled to mBarBlocks)
  //
  for (Index = 0; Index < (Completion * mBarBlocks) / 100; Index++) {
    Print (L"%c", BLOCKELEMENT_FULL_BLOCK);
  }

  //
  // Restore text color
  //
  gST->ConOut->SetAttribute (gST->ConOut, CurrentAttribute);

  //
  // Print remaining portion of progress bar
  //
  for ( ; Index < mBarBlocks; Index++) {
    Print (L"%c", BLOCKELEMENT_LIGHT_SHADE);
  }

  mPreviousProgress = Completion;

  return EFI_SUCCESS;
}
