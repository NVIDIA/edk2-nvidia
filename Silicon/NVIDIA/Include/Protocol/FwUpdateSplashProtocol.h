/** @file

  NVIDIA firmware-update splash protocol.

  Produced by any driver that wants to participate in the boot-time
  firmware update UX: it bundles a splash bitmap to draw before the
  capsule install begins and a session-specific lower bound for the
  FMP progress bar that follows. Consumed by PlatformBootManagerLib
  (to render the splash) and DisplayUpdateProgressLib (to remap the
  FMP completion into ProgressBase..100). Not tied to OTA: any caller
  that publishes the protocol gets the same treatment.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_FW_UPDATE_SPLASH_PROTOCOL_H__
#define __NVIDIA_FW_UPDATE_SPLASH_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define NVIDIA_FW_UPDATE_SPLASH_PROTOCOL_GUID    \
  {0x8e2a6f17, 0xb5d3, 0x4c9e, {0x91, 0x4a, 0x73, 0x6b, 0x2c, 0x9f, 0x8d, 0xa4}}

typedef struct _NVIDIA_FW_UPDATE_SPLASH_PROTOCOL NVIDIA_FW_UPDATE_SPLASH_PROTOCOL;

/**
  Render the firmware-update splash bitmap to the framebuffer.

  Best-effort: missing HII / GOP / image data yields EFI_SUCCESS without
  rendering, since capsule install must proceed regardless of UI.

  @param[in] This  Pointer to this protocol instance.

  @retval EFI_SUCCESS  Splash shown (or silently skipped).
  @retval Other        Underlying HII / GOP failure (logged).
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_FW_UPDATE_SPLASH_SHOW)(
  IN NVIDIA_FW_UPDATE_SPLASH_PROTOCOL  *This
  );

struct _NVIDIA_FW_UPDATE_SPLASH_PROTOCOL {
  NVIDIA_FW_UPDATE_SPLASH_SHOW    Show;
  //
  // Displayed-progress lower bound (0..99) for the FMP capsule install
  // that follows the splash. When this protocol is published, the FMP
  // progress lib is expected to render the firmware install over the
  // ProgressBase..100 range so the user perceives it as the final
  // slice of the overall update flow. The producer owns this value;
  // out-of-range entries are treated as 0 (no remap).
  //
  UINTN           ProgressBase;
  //
  // Caption rendered on the row beneath the progress bar (e.g.
  // L"Updating SHIELD: "). DisplayUpdateProgressLib appends the
  // current percentage to this string and centers the result on
  // screen. The producer owns the storage; NULL falls back to the
  // lib's built-in default ("Update Progress - ").
  //
  CONST CHAR16    *Caption;
};

extern EFI_GUID  gNVIDIAFwUpdateSplashProtocolGuid;

#endif
