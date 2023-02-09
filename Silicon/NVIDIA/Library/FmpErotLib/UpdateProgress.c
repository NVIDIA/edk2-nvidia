/** @file
  Update Image Progress support from DxeCapsuleLibFmp

  Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2016 - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DisplayUpdateProgressLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/FirmwareManagementProgress.h>

extern EDKII_FIRMWARE_MANAGEMENT_PROGRESS_PROTOCOL  mFmpProgress;

/**
  Function indicate the current completion progress of the firmware
  update. Platform may override with own specific progress function.

  @param[in]  Completion  A value between 1 and 100 indicating the current
                          completion progress of the firmware update

  @retval EFI_SUCESS             The capsule update progress was updated.
  @retval EFI_INVALID_PARAMETER  Completion is greater than 100%.

**/
EFI_STATUS
EFIAPI
UpdateImageProgress (
  IN UINTN  Completion
  )
{
  EFI_STATUS                           Status;
  UINTN                                Seconds;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  *Color;

  DEBUG ((DEBUG_INFO, "Update Progress - %llu%%\n", Completion));

  if (Completion > 100) {
    return EFI_INVALID_PARAMETER;
  }

  Seconds = mFmpProgress.WatchdogSeconds;
  Color   = &mFmpProgress.ProgressBarForegroundColor;

  //
  // Cancel the watchdog timer
  //
  gBS->SetWatchdogTimer (0, 0x0000, 0, NULL);

  if (Completion != 100) {
    //
    // Arm the watchdog timer from PCD setting
    //
    if (Seconds != 0) {
      DEBUG ((DEBUG_VERBOSE, "Arm watchdog timer %llu seconds\n", Seconds));
      gBS->SetWatchdogTimer (Seconds, 0x0000, 0, NULL);
    }
  }

  Status = DisplayUpdateProgress (Completion, Color);

  return Status;
}
