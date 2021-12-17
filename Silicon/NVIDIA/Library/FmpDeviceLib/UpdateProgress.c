/** @file
  Update Image Progress support from DxeCapsuleLibFmp

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2016 - 2019, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

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

  DEBUG((DEBUG_INFO, "Update Progress - %d%%\n", Completion));

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
      DEBUG ((DEBUG_VERBOSE, "Arm watchdog timer %d seconds\n", Seconds));
      gBS->SetWatchdogTimer (Seconds, 0x0000, 0, NULL);
    }
  }

  Status = DisplayUpdateProgress (Completion, Color);

  return Status;
}
