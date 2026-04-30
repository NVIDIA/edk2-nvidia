/** @file
  Android Fastboot Bootloader Menu.

  On-screen, keyboard-driven menu shown by the Android Fastboot UEFI
  application. The menu owns its own state (selection, status line, idle
  timer arming), draws itself onto the supplied console, and dispatches
  the user's selection via Android BCB / runtime-services resets.

  The menu module is intentionally decoupled from the Fastboot transport
  / command handling logic in AndroidFastbootApp.c so that UI changes do
  not bleed into the protocol code.

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef FASTBOOT_MENU_H_
#define FASTBOOT_MENU_H_

#include <Uefi.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/SimpleTextIn.h>

//
// Color sentinel meaning "use the menu module's default foreground color"
// (currently EFI_LIGHTGRAY for the body, EFI_LIGHTCYAN for the status
// line, etc.). EFI_BLACK is unusable as a foreground color, so 0 is
// safe to repurpose as "unset" with explicit per-slot fallback in
// the static tables.
//
#define FASTBOOT_MENU_COLOR_DEFAULT  ((UINTN)0)

/**
  A colored Unicode string used by the menu (header text, entry text,
  on-select flash text, status text, ...). The Color field is a UEFI
  Simple Text Output foreground attribute (EFI_BLUE, EFI_GREEN,
  EFI_LIGHTRED, EFI_YELLOW, ...) so the renderer can drive
  `SetAttribute (EFI_TEXT_ATTR (Color, BG))` directly without an
  intermediate translation layer.

  @param Color   EFI foreground attribute. FASTBOOT_MENU_COLOR_DEFAULT
                 (0) means "use the menu's default for this slot".
  @param Data    NUL-terminated wide string. NULL is treated as
                 "skip this slot" (do not render anything).
**/
typedef struct {
  UINTN           Color;
  CONST CHAR16    *Data;
} FASTBOOT_MENU_STRING;

/**
  Optional, colored menu header rendered above the entry body.

  If @c Valid is FALSE the menu body starts directly at the redraw
  row with no title and no separator.

  @param Valid   TRUE to render the header, FALSE to skip it.
  @param Ms      The header text and color.
**/
typedef struct {
  BOOLEAN                 Valid;
  FASTBOOT_MENU_STRING    Ms;
} FASTBOOT_MENU_HEADER;

typedef struct _FASTBOOT_MENU_DESCRIPTOR FASTBOOT_MENU_DESCRIPTOR;

/**
  Invoked when the user commits the focused menu row (on-select).

  @retval TRUE    Request Fastboot application exit (legacy DoMenuAction TRUE).
  @retval FALSE    Keep running (stay in nested menu or main menu).

**/
typedef BOOLEAN (EFIAPI *FASTBOOT_MENU_ON_SELECT)(
  IN VOID *Arg OPTIONAL
  );

typedef struct {
  FASTBOOT_MENU_STRING              MsEntry;
  FASTBOOT_MENU_STRING              MsOnSelect;
  FASTBOOT_MENU_ON_SELECT           OnSelect;
  VOID                              *Arg;
  CONST FASTBOOT_MENU_DESCRIPTOR    *NextMenu;
} FASTBOOT_MENU_ENTRY;

struct _FASTBOOT_MENU_DESCRIPTOR {
  FASTBOOT_MENU_HEADER         Header;
  CONST CHAR16                 *Body;
  CONST CHAR16                 *HelpLegend;
  CONST FASTBOOT_MENU_ENTRY    *Entries;
  UINTN                        EntryCount;
  CONST CHAR16                 *CancelledStatusHint;
  BOOLEAN                      ShowWorkingOnConfirm;
};

/**
  Result of FastbootMenuHandleKey().
**/
typedef enum {
  ///
  /// The key was not consumed by the menu (e.g. SPACE quit). The caller
  /// is free to interpret it.
  ///
  FastbootMenuKeyResultIgnored = 0,
  ///
  /// The menu consumed the key (navigation or non-exiting action). The
  /// caller should continue its main loop.
  ///
  FastbootMenuKeyResultHandled,
  ///
  /// The menu performed an action that requested the application to
  /// exit (e.g. pre-reset action that did not actually reset).
  ///
  FastbootMenuKeyResultExitApp
} FASTBOOT_MENU_KEY_RESULT;

/**
  Initialise the bootloader menu module.

  Stores references to the console and idle-timer event used by the
  menu, resets all internal state (selection, status line, dirty flags,
  await-continue flag) and prepares for the first redraw. The first
  call to FastbootMenuRedrawIfDirty() will paint the menu.

  @param[in] TextOut          Simple Text Output protocol used for drawing.
  @param[in] IdleTimerEvent   EFI_EVENT (timer) used for the idle timeout.
                              May be NULL to disable idle timeout entirely.
**/
VOID
FastbootMenuInit (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *TextOut,
  IN EFI_EVENT                        IdleTimerEvent
  );

/**
  Set the status line shown below the menu.

  Accepts a printf-style Unicode format string. Marks the menu as dirty
  so the next FastbootMenuRedrawIfDirty() will repaint.

  @param[in] Fmt   Unicode format string.
  @param[in] ...   Arguments matching @p Fmt.
**/
VOID
FastbootMenuSetStatus (
  IN CONST CHAR16  *Fmt,
  ...
  );

/**
  Repaint the menu if any state changed since the last paint.

  Also (re)arms the idle timer when a redraw actually happens.
**/
VOID
FastbootMenuRedrawIfDirty (
  VOID
  );

/**
  (Re)arm the idle timer.

  The main loop should call this on any user/transport activity so the
  idle "Continue" action does not fire while the user is interacting.
  Has no effect when the menu is in await-continue mode.
**/
VOID
FastbootMenuArmIdleTimer (
  VOID
  );

/**
  Query whether the most recent menu action requested that the
  caller continue the platform's normal Android boot sequence
  (Android kernel via AndroidBootDxe), rather than letting the
  EDK2 BDS dispatch its post-app fallback (e.g. surfacing
  "Enter Setup" / BootManagerMenu when this app exits with
  EFI_SUCCESS — see MdeModulePkg/Universal/BdsDxe/BdsEntry.c
  BootBootOptions()).

  Set when the user picks "Continue" or "Boot safe mode" from
  the menu. Cleared by FastbootMenuInit().

  @retval TRUE    The caller should explicitly invoke the next
                  CATEGORY_BOOT/ACTIVE/non-HIDDEN BootOrder entry
                  (skipping the Fastboot app itself) before
                  returning to BDS.
  @retval FALSE   The caller should just return to BDS as normal.
**/
BOOLEAN
FastbootMenuShouldContinueAndroidBoot (
  VOID
  );

/**
  Handle a single key press from EFI_SIMPLE_TEXT_INPUT_PROTOCOL.

  @param[in] Key   The key that was read from ReadKeyStroke.

  @return One of FASTBOOT_MENU_KEY_RESULT, see that enum for details.
**/
FASTBOOT_MENU_KEY_RESULT
FastbootMenuHandleKey (
  IN EFI_INPUT_KEY  *Key
  );

/**
  Render a nested confirmation overlay and block on
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL until the user picks one or presses
  ESC. The choices are data-driven (same FASTBOOT_MENU_ENTRY shape as
  the main menu): the first row is "Confirm", the second "Cancel";
  default selection is Cancel — the user must move to Confirm before
  ENTER. Used by lock / unlock / factory reset and host-driven
  `fastboot flashing lock|unlock`.

  Entry, exit, and UP/DOWN navigation all use in-place padded
  redraws (no ClearScreen) so there is no flicker. On Confirm a
  short "WORKING -- DO NOT POWER OFF" banner is drawn before
  return so the screen is not blank during the 10+ s user-data
  wipe; the caller's next FastbootMenuSetStatus + redraw overwrites
  it with the main menu and the final status.

  @param[in] Title   Single-line RED title.
  @param[in] Body    Multi-line warning text (caller embeds \r\n  for
                     each subsequent line so it stays indented).

  @retval TRUE   User confirmed (Confirm + ENTER).
  @retval FALSE  User cancelled (Cancel + ENTER, ESC, or no
                 EFI_SIMPLE_TEXT_INPUT_PROTOCOL was available).
**/
BOOLEAN
FastbootMenuConfirmAction (
  IN CONST CHAR16  *Title,
  IN CONST CHAR16  *Body
  );

/**
  Run the same Lock confirm overlay as the on-device menu (host `flashing lock`).

  @retval TRUE   User chose Confirm.

  @retval FALSE  Cancel, ESC, or invalid menu data.
**/
BOOLEAN
FastbootMenuLockConfirmFromHost (
  VOID
  );

/**
  Run the same Unlock confirm overlay as the on-device menu (host `flashing unlock`).

  @retval TRUE   User chose Confirm.

  @retval FALSE  Cancel, ESC, or invalid menu data.
**/
BOOLEAN
FastbootMenuUnlockConfirmFromHost (
  VOID
  );

/**
  Handle the menu idle-timer expiry.

  Fires the default "Continue" action.

  @retval TRUE    The application should exit (action was performed and
                  control will not return; in practice this is always TRUE
                  because Continue resets the system).
  @retval FALSE   The application should continue running and redraw the
                  menu.
**/
BOOLEAN
FastbootMenuOnIdleTimeout (
  VOID
  );

#endif /* FASTBOOT_MENU_H_ */
