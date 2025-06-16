/** @file
  AVB UI Implementation - Displays Android Verified Boot warning images.

  This module calls the AVB UI Protocol to display warning screens.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/AvbUi.h>

#include "Library/AvbLib.h"

/**
  Show AVB UI based on the boot state.

  This function locates the AVB UI Protocol and calls it to display
  the appropriate warning or error screen.

  @param[in] BootState  The AVB boot state.

  @retval EFI_SUCCESS           UI displayed successfully or no UI needed.
  @retval EFI_NOT_FOUND         AVB UI Protocol not found.
  @retval Other                 Error occurred.
**/
EFI_STATUS
AvbShowUi (
  IN AVB_BOOT_STATE  BootState
  )
{
  EFI_STATUS              Status;
  NVIDIA_AVB_UI_PROTOCOL  *AvbUi;
  AVB_UI_STATE            UiState;

  DEBUG ((DEBUG_INFO, "%a: Boot state = %d\n", __func__, BootState));

  //
  // Locate AVB UI Protocol
  //
  Status = gBS->LocateProtocol (
                  &gNVIDIAAvbUiProtocolGuid,
                  NULL,
                  (VOID **)&AvbUi
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: AVB UI Protocol not found: %r\n", __func__, Status));
    DEBUG ((DEBUG_WARN, "%a: Continuing without UI display\n", __func__));
    return EFI_SUCCESS;  // Continue boot without UI
  }

  //
  // Map AVB boot state to UI state
  //
  switch (BootState) {
    case VERIFIED_BOOT_GREEN_STATE:
      UiState = AVB_UI_STATE_GREEN;
      break;
    case VERIFIED_BOOT_YELLOW_STATE:
      UiState = AVB_UI_STATE_YELLOW;
      break;
    case VERIFIED_BOOT_ORANGE_STATE:
      UiState = AVB_UI_STATE_ORANGE;
      break;
    case VERIFIED_BOOT_RED_STATE:
      UiState = AVB_UI_STATE_RED;
      break;
    case VERIFIED_BOOT_RED_STATE_EIO:
      UiState = AVB_UI_STATE_RED_EIO;
      break;
    default:
      DEBUG ((DEBUG_WARN, "%a: Unknown boot state %d\n", __func__, BootState));
      return EFI_SUCCESS;
  }

  //
  // Call AVB UI Protocol
  //
  Status = AvbUi->Show (AvbUi, UiState);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to show UI: %r\n", __func__, Status));
  }

  return Status;
}
