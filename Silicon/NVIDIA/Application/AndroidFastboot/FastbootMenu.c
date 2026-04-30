/** @file
  Android Fastboot Bootloader Menu implementation.

  See FastbootMenu.h for the API description.

  The menu is descriptor-driven:

    FASTBOOT_MENU_DESCRIPTOR ties together header, body help text, and
    an array of FASTBOOT_MENU_ENTRY. Each entry carries:

    - row label (MsEntry) and optional status-line flash (MsOnSelect),
    - OnSelect(Arg) dispatched on commit,
    - optional NextMenu pointing at a nested confirm menu (or another layer).

    When an entry is selected the renderer ignores MsEntry idle color and
    uses a fixed highlight color (EFI_GREEN).

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "AndroidFastbootApp.h"
#include "FastbootMenu.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/AndroidBcbLib.h>
#include <Library/AvbLib.h>
#include <Library/BootConfigProtocolLib.h>
#include <Protocol/BootConfigUpdateProtocol.h>

//
// Menu configuration.
//
#define BOOTLOADER_MENU_TIMEOUT_SEC  0       // 0 disables auto-continue
#define MENU_STATUS_MAX_CHARS        128

//
// Right-padding string used when redrawing menu lines without
// ClearScreen, so leftover characters from a longer previous line are
// overwritten. Long enough for typical 80-column consoles.
//
#define MENU_LINE_PAD  L"                                        "

//
// Cursor row at which the menu body starts redrawing.
//
#define MENU_REDRAW_ROW  2

//
// Rows wiped on confirm-screen entry to cover the main menu without
// using ClearScreen. Sized just above the main menu's actual height.
//
#define CONFIRM_OVERLAY_ROWS  18

//
// Default color palette. Anything in the menu tables left as
// FASTBOOT_MENU_COLOR_DEFAULT (0) gets these per-slot fallbacks.
//
#define MENU_HEADER_DEFAULT_COLOR  EFI_LIGHTRED
#define MENU_ENTRY_DEFAULT_COLOR   EFI_WHITE
#define MENU_SELECTED_COLOR        EFI_GREEN
#define MENU_HELP_COLOR            EFI_LIGHTGRAY
#define MENU_STATUS_DEFAULT_COLOR  EFI_LIGHTCYAN
#define MENU_BANNER_COLOR          EFI_LIGHTGRAY

//
// RCM: SCRATCH0 at 0xc392000 (DT scratch bank 0xc390000 + tegra234 offset 0x2000). Avoid MMIO to PMC 0xc360000
// from this app; program scratch like kernel forced-recovery, then EfiResetCold.
//
#define TEGRA_PMC_SCRATCH_BLOCK_BASE  0x0C390000UL
#define TEGRA_PMC_SCRATCH0_OFFSET     0x2000U
#define PMC_SCRATCH0_BOOTMODE_MASK32  (((UINT32)1 << 31) | ((UINT32)1 << 30) | ((UINT32)1 << 1))
#define PMC_SCRATCH0_MODE_RCM32       ((UINT32)1 << 1)

#define TEGRA_PMC_SCRATCH0_REG_ADDR \
  (TEGRA_PMC_SCRATCH_BLOCK_BASE + TEGRA_PMC_SCRATCH0_OFFSET)

STATIC CONST CHAR16  mDefaultMainHelp[] =
  L"\r\n  [UP/DOWN: select  ENTER: choose  SPACE: exit]" MENU_LINE_PAD L"\r\n";

STATIC CONST CHAR16  mDefaultNestedMenuHelp[] =
  L"\r\n  [UP/DOWN: select  ENTER: choose  ESC: cancel]" MENU_LINE_PAD L"\r\n";

STATIC EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *mTextOut;
STATIC EFI_EVENT                        mIdleTimer;
STATIC UINTN                            mSelected;
STATIC BOOLEAN                          mMenuDirty;
STATIC BOOLEAN                          mStatusDirty;
STATIC CHAR16                           mStatusLine[MENU_STATUS_MAX_CHARS];
STATIC UINTN                            mStatusColor               = MENU_STATUS_DEFAULT_COLOR;
STATIC BOOLEAN                          mShouldContinueAndroidBoot = FALSE;

STATIC EFI_STATUS
AddSafeModeToBootConfig (
  VOID
  );

STATIC BOOLEAN
DoLockTransition (
  IN BOOLEAN  WantLocked
  );

STATIC BOOLEAN
RunNestedMenu (
  IN CONST FASTBOOT_MENU_DESCRIPTOR  *MenuDesc
  );

STATIC VOID
PaintNestedMenuChrome (
  IN CONST FASTBOOT_MENU_DESCRIPTOR  *MenuDesc
  );

STATIC BOOLEAN EFIAPI
MenuActionContinue (
  VOID  *Arg
  );

STATIC BOOLEAN EFIAPI
MenuActionRecovery (
  VOID  *Arg
  );

STATIC BOOLEAN EFIAPI
MenuActionSafeMode (
  VOID  *Arg
  );

STATIC BOOLEAN EFIAPI
MenuActionReboot (
  VOID  *Arg
  );

STATIC BOOLEAN EFIAPI
MenuActionForcedRecovery (
  VOID  *Arg
  );

STATIC BOOLEAN EFIAPI
UnlockBootloaderOnUserConfirm (
  VOID  *Arg
  );

STATIC BOOLEAN EFIAPI
LockBootloaderOnUserConfirm (
  VOID  *Arg
  );

STATIC BOOLEAN EFIAPI
FactoryResetOnUserConfirm (
  VOID  *Arg
  );

STATIC BOOLEAN EFIAPI
HostChooserOnConfirm (
  VOID  *Arg
  );

STATIC BOOLEAN EFIAPI
HostChooserOnCancel (
  VOID  *Arg
  );

STATIC VOID
ProgramPmcScratch0ForcedRecovery (
  VOID
  );

/**
  Append androidboot.mode=safe to the bootconfig accumulator (shared with AndroidBootDxe).
**/
STATIC
EFI_STATUS
AddSafeModeToBootConfig (
  VOID
  )
{
  EFI_STATUS                         Status;
  NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  *BootConfigProtocol;

  Status = GetBootConfigUpdateProtocol (&BootConfigProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetBootConfigUpdateProtocol failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = BootConfigProtocol->UpdateBootConfigs (BootConfigProtocol, "mode", "safe");
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: UpdateBootConfigs(mode,safe) failed: %r\n", __FUNCTION__, Status));
  } else {
    DEBUG ((DEBUG_INFO, "%a: appended androidboot.mode=safe to bootconfig\n", __FUNCTION__));
  }

  return Status;
}

STATIC
BOOLEAN
DoLockTransition (
  IN BOOLEAN  WantLocked
  )
{
  EFI_STATUS  Status;
  BOOLEAN     IsLocked;

  Status = AvbReadDeviceLockedState (&IsLocked);
  if (EFI_ERROR (Status)) {
    FastbootMenuSetStatus (L"Fastboot: lock state unknown (%r)", Status);
    return FALSE;
  }

  if (IsLocked == WantLocked) {
    FastbootMenuSetStatus (
      WantLocked ? L"Fastboot: bootloader is already locked"
                 : L"Fastboot: bootloader is already unlocked"
      );
    return FALSE;
  }

  Status = WantLocked ? FastbootLockBootloader () : FastbootUnlockBootloader ();
  if (EFI_ERROR (Status)) {
    if (Status == EFI_ACCESS_DENIED) {
      FastbootMenuSetStatus (L"Fastboot: unlock not allowed (enable OEM unlock first)");
    } else {
      FastbootMenuSetStatus (
        WantLocked ? L"Fastboot: lock failed (%r)" : L"Fastboot: unlock failed (%r)",
        Status
        );
    }

    return FALSE;
  }

  FastbootMenuSetStatus (
    WantLocked ? L"Fastboot: bootloader locked"
               : L"Fastboot: bootloader unlocked"
    );
  return FALSE;
}

STATIC
BOOLEAN EFIAPI
MenuActionContinue (
  VOID  *Arg
  )
{
  (VOID)Arg;
  mShouldContinueAndroidBoot = TRUE;
  return TRUE;
}

STATIC
BOOLEAN EFIAPI
MenuActionRecovery (
  VOID  *Arg
  )
{
  (VOID)Arg;
  SetCmdToMiscPartition (NULL, MISC_CMD_TYPE_RECOVERY);
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
  return TRUE;
}

STATIC
BOOLEAN EFIAPI
MenuActionSafeMode (
  VOID  *Arg
  )
{
  EFI_STATUS  Status;

  (VOID)Arg;
  Status = AddSafeModeToBootConfig ();
  if (EFI_ERROR (Status)) {
    FastbootMenuSetStatus (L"Fastboot: failed to enable safe mode (%r)", Status);
    return FALSE;
  }

  mShouldContinueAndroidBoot = TRUE;
  return TRUE;
}

STATIC
BOOLEAN EFIAPI
MenuActionReboot (
  VOID  *Arg
  )
{
  (VOID)Arg;
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
  return TRUE;
}

STATIC
VOID
ProgramPmcScratch0ForcedRecovery (
  VOID
  )
{
  UINT32  Scratch0;

  Scratch0  = MmioRead32 (TEGRA_PMC_SCRATCH0_REG_ADDR);
  Scratch0 &= ~PMC_SCRATCH0_BOOTMODE_MASK32;
  Scratch0 |= PMC_SCRATCH0_MODE_RCM32;
  MmioWrite32 (
    TEGRA_PMC_SCRATCH0_REG_ADDR,
    Scratch0
    );
  MemoryFence ();
}

//
// Program SCRATCH0 like tegra forced-recovery; cold reset via RT (avoid NS MMIO to PMC @ 0xc360000).
//
STATIC
BOOLEAN EFIAPI
MenuActionForcedRecovery (
  VOID  *Arg
  )
{
  (VOID)Arg;

  DEBUG ((DEBUG_INFO, "%a: SCRATCH0 RCM + EfiResetCold\n", __FUNCTION__));
  ProgramPmcScratch0ForcedRecovery ();
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
  CpuDeadLoop ();
  return TRUE;
}

STATIC
BOOLEAN EFIAPI
UnlockBootloaderOnUserConfirm (
  VOID  *Arg
  )
{
  (VOID)Arg;
  return DoLockTransition (FALSE);
}

STATIC
BOOLEAN EFIAPI
LockBootloaderOnUserConfirm (
  VOID  *Arg
  )
{
  (VOID)Arg;
  return DoLockTransition (TRUE);
}

STATIC
BOOLEAN EFIAPI
FactoryResetOnUserConfirm (
  VOID  *Arg
  )
{
  EFI_STATUS  Status;

  (VOID)Arg;
  Status = FastbootFactoryReset ();
  if (EFI_ERROR (Status)) {
    if (Status == EFI_ACCESS_DENIED) {
      FastbootMenuSetStatus (L"Fastboot: factory reset blocked by fac_rst_protection");
    } else {
      FastbootMenuSetStatus (L"Fastboot: factory reset failed (%r)", Status);
    }
  } else {
    FastbootMenuSetStatus (L"Fastboot: factory reset done");
  }

  return FALSE;
}

STATIC
BOOLEAN EFIAPI
HostChooserOnConfirm (
  VOID  *Arg
  )
{
  (VOID)Arg;
  return TRUE;
}

STATIC
BOOLEAN EFIAPI
HostChooserOnCancel (
  VOID  *Arg
  )
{
  (VOID)Arg;
  return FALSE;
}

STATIC CONST FASTBOOT_MENU_ENTRY  mHostChooserEntries[] = {
  {
    { EFI_WHITE,                   L"Confirm" },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL       },
    HostChooserOnConfirm,
    NULL,
    NULL
  },
  {
    { EFI_WHITE,                   L"Cancel"  },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL       },
    HostChooserOnCancel,
    NULL,
    NULL
  }
};

STATIC CONST FASTBOOT_MENU_ENTRY  mUnlockConfirmEntries[] = {
  {
    { EFI_WHITE,                   L"Confirm" },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL       },
    UnlockBootloaderOnUserConfirm,
    NULL,
    NULL
  },
  {
    { EFI_WHITE,                   L"Cancel"  },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL       },
    NULL,
    NULL,
    NULL
  }
};

STATIC CONST FASTBOOT_MENU_DESCRIPTOR  gUnlockConfirmNextMenu = {
  {
    TRUE,
    { EFI_LIGHTRED, L"!!! READ THE FOLLOWING !!!" }
  },
  L"Unlocking the bootloader allows custom OS\r\n"
  L"  software to be installed on this device.\r\n"
  L"  All user data will be wiped (factory data\r\n"
  L"  reset) to prevent unauthorized access.",
  NULL,
  mUnlockConfirmEntries,
  ARRAY_SIZE (mUnlockConfirmEntries),
  L"Fastboot: unlock cancelled",
  TRUE
};

STATIC CONST FASTBOOT_MENU_ENTRY  mLockConfirmEntries[] = {
  {
    { EFI_WHITE,                   L"Confirm" },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL       },
    LockBootloaderOnUserConfirm,
    NULL,
    NULL
  },
  {
    { EFI_WHITE,                   L"Cancel"  },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL       },
    NULL,
    NULL,
    NULL
  }
};

STATIC CONST FASTBOOT_MENU_DESCRIPTOR  gLockConfirmNextMenu = {
  {
    TRUE,
    { EFI_LIGHTRED, L"!!! READ THE FOLLOWING !!!" }
  },
  L"Locking the bootloader will wipe all user\r\n"
  L"  data on this device (factory data reset)\r\n"
  L"  to prevent unauthorized access to your\r\n"
  L"  personal data.",
  NULL,
  mLockConfirmEntries,
  ARRAY_SIZE (mLockConfirmEntries),
  L"Fastboot: lock cancelled",
  TRUE
};

STATIC CONST FASTBOOT_MENU_ENTRY  mFactoryConfirmEntries[] = {
  {
    { EFI_WHITE,                   L"Confirm" },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL       },
    FactoryResetOnUserConfirm,
    NULL,
    NULL
  },
  {
    { EFI_WHITE,                   L"Cancel"  },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL       },
    NULL,
    NULL,
    NULL
  }
};

STATIC CONST FASTBOOT_MENU_DESCRIPTOR  gFactoryConfirmNextMenu = {
  {
    TRUE,
    { EFI_LIGHTRED, L"!!! READ THE FOLLOWING !!!" }
  },
  L"This will erase all user data on this\r\n"
  L"  device. This action cannot be undone.",
  NULL,
  mFactoryConfirmEntries,
  ARRAY_SIZE (mFactoryConfirmEntries),
  L"Fastboot: factory reset cancelled",
  TRUE
};

//
// Main bootloader menu: each row exposes OnSelect and optional NextMenu.
//
STATIC CONST FASTBOOT_MENU_ENTRY  gBootloaderMenuEntries[] = {
  {
    { EFI_WHITE,                   L"Continue"                           },
    { FASTBOOT_MENU_COLOR_DEFAULT, L"Continue booting ..."               },
    MenuActionContinue,
    NULL,
    NULL
  },
  {
    { EFI_WHITE,                   L"Boot recovery kernel"               },
    { FASTBOOT_MENU_COLOR_DEFAULT, L"Booting recovery kernel ..."        },
    MenuActionRecovery,
    NULL,
    NULL
  },
  {
    { EFI_WHITE,                   L"Boot safe mode"                     },
    { FASTBOOT_MENU_COLOR_DEFAULT, L"Booting into safe mode ..."         },
    MenuActionSafeMode,
    NULL,
    NULL
  },
  {
    { EFI_WHITE,                   L"Reboot"                             },
    { FASTBOOT_MENU_COLOR_DEFAULT, L"Rebooting ..."                      },
    MenuActionReboot,
    NULL,
    NULL
  },
  {
    { EFI_WHITE,                   L"Forced Recovery"                    },
    { FASTBOOT_MENU_COLOR_DEFAULT, L"Booting into USB recovery mode ..." },
    MenuActionForcedRecovery,
    NULL,
    NULL
  },
  {
    { EFI_WHITE,                   L"Unlock Bootloader"                  },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL                                  },
    NULL,
    NULL,
    &gUnlockConfirmNextMenu
  },
  {
    { EFI_WHITE,                   L"Lock Bootloader"                    },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL                                  },
    NULL,
    NULL,
    &gLockConfirmNextMenu
  },
  {
    { EFI_WHITE,                   L"Factory Data Reset"                 },
    { FASTBOOT_MENU_COLOR_DEFAULT, NULL                                  },
    NULL,
    NULL,
    &gFactoryConfirmNextMenu
  }
};

STATIC CONST FASTBOOT_MENU_DESCRIPTOR  gBootloaderMainMenu = {
  {
    TRUE,
    { MENU_HEADER_DEFAULT_COLOR, L"Android Bootloader Menu" }
  },
  NULL,
  mDefaultMainHelp,
  gBootloaderMenuEntries,
  ARRAY_SIZE (gBootloaderMenuEntries),
  NULL,
  FALSE
};

/**
  Map FASTBOOT_MENU_COLOR_DEFAULT (0) to Fallback; otherwise return Color.
**/
STATIC
UINTN
ResolveColor (
  IN UINTN  Color,
  IN UINTN  Fallback
  )
{
  return (Color == FASTBOOT_MENU_COLOR_DEFAULT) ? Fallback : Color;
}

/**
  Internal: set the console foreground color. Background is always
  EFI_BLACK because that's what the rest of the console has been
  using and changing it would create odd-looking strips when only
  the menu re-renders.
**/
STATIC
VOID
SetTextColor (
  IN UINTN  Foreground
  )
{
  if (mTextOut == NULL) {
    return;
  }

  mTextOut->SetAttribute (mTextOut, EFI_TEXT_ATTR (Foreground, EFI_BLACK));
}

/**
  Internal: revert to the menu's "neutral" body color. Called between
  colored sections so a partial redraw cannot leak its color into
  whatever the next OutputString draws.
**/
STATIC
VOID
ResetTextColor (
  VOID
  )
{
  SetTextColor (EFI_LIGHTGRAY);
}

/**
  Internal: render the menu header (title + separator). No-op when
  the header is marked invalid or has no text.
**/
STATIC
VOID
DrawHeader (
  VOID
  )
{
  UINTN  Color;

  if (mTextOut == NULL) {
    return;
  }

  if (!gBootloaderMainMenu.Header.Valid || (gBootloaderMainMenu.Header.Ms.Data == NULL)) {
    //
    // Even when the header is suppressed, push two blank padded lines
    // so leftover characters from a previous (header-having) draw get
    // overwritten without flashing the screen via ClearScreen.
    //
    mTextOut->OutputString (mTextOut, L"\r\n  " MENU_LINE_PAD L"\r\n");
    mTextOut->OutputString (mTextOut, L"  " MENU_LINE_PAD L"\r\n\r\n");
    return;
  }

  Color = ResolveColor (
            gBootloaderMainMenu.Header.Ms.Color,
            MENU_HEADER_DEFAULT_COLOR
            );

  SetTextColor (Color);
  mTextOut->OutputString (mTextOut, L"\r\n  ");
  mTextOut->OutputString (mTextOut, (CHAR16 *)gBootloaderMainMenu.Header.Ms.Data);
  mTextOut->OutputString (mTextOut, MENU_LINE_PAD L"\r\n");
  mTextOut->OutputString (
              mTextOut,
              L"  ----------------------------------------" MENU_LINE_PAD L"\r\n\r\n"
              );
  ResetTextColor ();
}

/**
  Internal: render one menu column — one line per FASTBOOT_MENU_ENTRY.
  The selected row uses MENU_SELECTED_COLOR and a "> " marker.
**/
STATIC
VOID
DrawMenuEntryLines (
  IN CONST FASTBOOT_MENU_ENTRY  *Entries,
  IN UINTN                      EntryCount,
  IN UINTN                      SelectedIndex
  )
{
  UINTN                       Idx;
  UINTN                       Color;
  CONST FASTBOOT_MENU_STRING  *Ms;
  CONST CHAR16                *Prefix;

  if (mTextOut == NULL) {
    return;
  }

  for (Idx = 0; Idx < EntryCount; Idx++) {
    Ms = &Entries[Idx].MsEntry;
    if (Ms->Data == NULL) {
      continue;
    }

    if (Idx == SelectedIndex) {
      Color  = MENU_SELECTED_COLOR;
      Prefix = L"  > ";
    } else {
      Color  = ResolveColor (Ms->Color, MENU_ENTRY_DEFAULT_COLOR);
      Prefix = L"    ";
    }

    SetTextColor (Color);
    mTextOut->OutputString (mTextOut, (CHAR16 *)Prefix);
    mTextOut->OutputString (mTextOut, (CHAR16 *)Ms->Data);
    mTextOut->OutputString (mTextOut, MENU_LINE_PAD L"\r\n");
  }

  ResetTextColor ();
}

STATIC
VOID
PaintNestedMenuChrome (
  IN CONST FASTBOOT_MENU_DESCRIPTOR  *MenuDesc
  )
{
  UINTN                 Idx;
  UINTN                 Color;
  FASTBOOT_MENU_HEADER  Hdr;

  if ((mTextOut == NULL) || (MenuDesc == NULL)) {
    return;
  }

  mTextOut->SetCursorPosition (mTextOut, 0, MENU_REDRAW_ROW);
  for (Idx = 0; Idx < CONFIRM_OVERLAY_ROWS; Idx++) {
    mTextOut->OutputString (mTextOut, MENU_LINE_PAD MENU_LINE_PAD L"\r\n");
  }

  mTextOut->SetCursorPosition (mTextOut, 0, MENU_REDRAW_ROW);

  Hdr = MenuDesc->Header;
  if (!Hdr.Valid || (Hdr.Ms.Data == NULL)) {
    mTextOut->OutputString (mTextOut, L"\r\n  " MENU_LINE_PAD L"\r\n\r\n");
  } else {
    Color = ResolveColor (Hdr.Ms.Color, MENU_HEADER_DEFAULT_COLOR);
    SetTextColor (Color);
    mTextOut->OutputString (mTextOut, L"\r\n  ");
    mTextOut->OutputString (mTextOut, (CHAR16 *)Hdr.Ms.Data);
    mTextOut->OutputString (mTextOut, MENU_LINE_PAD L"\r\n");
    mTextOut->OutputString (
                mTextOut,
                L"  ----------------------------------------" MENU_LINE_PAD L"\r\n\r\n"
                );
    ResetTextColor ();
  }

  if ((MenuDesc->Body != NULL) && (MenuDesc->Body[0] != CHAR_NULL)) {
    SetTextColor (EFI_WHITE);
    mTextOut->OutputString (mTextOut, L"  ");
    mTextOut->OutputString (mTextOut, (CHAR16 *)MenuDesc->Body);
    mTextOut->OutputString (mTextOut, L"\r\n\r\n");
    ResetTextColor ();
  }
}

/**
  Clear the nested menu region starting at MENU_REDRAW_ROW through
  @p MenuBottomRow, optionally flashing the short WORKING banner first.
**/
STATIC
VOID
ClearNestedMenuInPlace (
  IN INT32    MenuBottomRow,
  IN BOOLEAN  ShowWorkingBrief
  )
{
  if (mTextOut == NULL) {
    return;
  }

  mTextOut->SetCursorPosition (mTextOut, 0, MENU_REDRAW_ROW);

  if (ShowWorkingBrief) {
    SetTextColor (EFI_LIGHTRED);
    mTextOut->OutputString (
                mTextOut,
                L"\r\n  *** WORKING -- DO NOT POWER OFF ***" MENU_LINE_PAD L"\r\n"
                );
    SetTextColor (EFI_WHITE);
    mTextOut->OutputString (
                mTextOut,
                L"  This may take several seconds. Please wait..." MENU_LINE_PAD L"\r\n"
                );
    ResetTextColor ();
  }

  while ((INT32)mTextOut->Mode->CursorRow <= MenuBottomRow) {
    mTextOut->OutputString (mTextOut, MENU_LINE_PAD MENU_LINE_PAD L"\r\n");
  }
}

/**
  Run a blocking nested-menu overlay (@p MenuDesc): header/body chrome,
  Confirm/Cancel-style rows, optional OnSelect per row.

  Default selection is last row (typically Cancel). ENTER on a row with
  NULL OnSelect is treated as dismiss (cancel). ESC uses CancelledStatusHint.

  @retval TRUE    Last invoked OnSelect returned TRUE (e.g. host confirm).
  @retval FALSE   Otherwise.
**/
STATIC
BOOLEAN
RunNestedMenu (
  IN CONST FASTBOOT_MENU_DESCRIPTOR  *MenuDesc
  )
{
  EFI_STATUS                      Status;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *TextIn;
  EFI_INPUT_KEY                   Key;
  UINTN                           Selected;
  UINTN                           WaitIdx;
  BOOLEAN                         Done;
  BOOLEAN                         RetVal;
  BOOLEAN                         GotReturn;
  BOOLEAN                         EntriesDirty;
  INT32                           EntriesRow;
  INT32                           MenuBottomRow;
  CONST CHAR16                    *Help;

  if ((mTextOut == NULL) || (MenuDesc == NULL) || (MenuDesc->Entries == NULL) ||
      (MenuDesc->EntryCount == 0))
  {
    return FALSE;
  }

  TextIn = NULL;
  Status = gBS->LocateProtocol (&gEfiSimpleTextInProtocolGuid, NULL, (VOID **)&TextIn);
  if (EFI_ERROR (Status) || (TextIn == NULL)) {
    return FALSE;
  }

  ASSERT (MenuDesc->EntryCount >= 2);

  Selected      = MenuDesc->EntryCount - 1;
  Done          = FALSE;
  RetVal        = FALSE;
  GotReturn     = FALSE;
  EntriesDirty  = TRUE;
  MenuBottomRow = 0;

  PaintNestedMenuChrome (MenuDesc);

  EntriesRow = mTextOut->Mode->CursorRow;
  Help       = (MenuDesc->HelpLegend != NULL) ? MenuDesc->HelpLegend : mDefaultNestedMenuHelp;

  while (!Done) {
    if (EntriesDirty) {
      mTextOut->SetCursorPosition (mTextOut, 0, EntriesRow);

      DrawMenuEntryLines (MenuDesc->Entries, MenuDesc->EntryCount, Selected);

      SetTextColor (MENU_HELP_COLOR);
      mTextOut->OutputString (mTextOut, (CHAR16 *)Help);
      ResetTextColor ();

      EntriesDirty = FALSE;
    }

    Status = gBS->WaitForEvent (1, &TextIn->WaitForKey, &WaitIdx);
    if (EFI_ERROR (Status)) {
      MenuBottomRow = (INT32)mTextOut->Mode->CursorRow;
      ClearNestedMenuInPlace (MenuBottomRow, FALSE);
      GotReturn = TRUE;
      RetVal    = FALSE;
      Done      = TRUE;
      break;
    }

    Status = TextIn->ReadKeyStroke (TextIn, &Key);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (Key.ScanCode == SCAN_UP) {
      if (MenuDesc->EntryCount > 1) {
        Selected     = (Selected + MenuDesc->EntryCount - 1) % MenuDesc->EntryCount;
        EntriesDirty = TRUE;
      }

      continue;
    }

    if (Key.ScanCode == SCAN_DOWN) {
      if (MenuDesc->EntryCount > 1) {
        Selected     = (Selected + 1) % MenuDesc->EntryCount;
        EntriesDirty = TRUE;
      }

      continue;
    }

    if (Key.ScanCode == SCAN_ESC) {
      if (MenuDesc->CancelledStatusHint != NULL) {
        FastbootMenuSetStatus (MenuDesc->CancelledStatusHint);
      }

      MenuBottomRow = (INT32)mTextOut->Mode->CursorRow;
      ClearNestedMenuInPlace (MenuBottomRow, FALSE);
      GotReturn = TRUE;
      RetVal    = FALSE;
      Done      = TRUE;
      continue;
    }

    if ((Key.UnicodeChar == CHAR_CARRIAGE_RETURN) || (Key.UnicodeChar == L'\n')) {
      MenuBottomRow = (INT32)mTextOut->Mode->CursorRow;

      if (MenuDesc->Entries[Selected].OnSelect != NULL) {
        //
        // Working banner only for the primary confirm row (index 0) so
        // host chooser "Cancel" does not flash it.
        //
        ClearNestedMenuInPlace (
          MenuBottomRow,
          MenuDesc->ShowWorkingOnConfirm && (Selected == 0)
          );

        RetVal = MenuDesc->Entries[Selected].OnSelect (
                                               MenuDesc->Entries[Selected].Arg
                                               );
        GotReturn = TRUE;
      } else {
        if (MenuDesc->CancelledStatusHint != NULL) {
          FastbootMenuSetStatus (MenuDesc->CancelledStatusHint);
        }

        ClearNestedMenuInPlace (MenuBottomRow, FALSE);
        RetVal    = FALSE;
        GotReturn = TRUE;
      }

      Done = TRUE;
      continue;
    }
  }

  if (!GotReturn) {
    RetVal = FALSE;
  }

  mMenuDirty   = TRUE;
  mStatusDirty = TRUE;

  return RetVal;
}

/**
  Internal: render the main menu body.
**/
STATIC
VOID
DrawBody (
  VOID
  )
{
  DrawMenuEntryLines (
    gBootloaderMainMenu.Entries,
    gBootloaderMainMenu.EntryCount,
    mSelected
    );
}

/**
  Internal: render the help/legend line below the body. Always uses
  MENU_HELP_COLOR (a dim color) so the legend reads as secondary
  information rather than competing with the entries.
**/
STATIC
VOID
DrawHelp (
  VOID
  )
{
  if (mTextOut == NULL) {
    return;
  }

  SetTextColor (MENU_HELP_COLOR);
  if (gBootloaderMainMenu.HelpLegend != NULL) {
    mTextOut->OutputString (
                mTextOut,
                (CHAR16 *)gBootloaderMainMenu.HelpLegend
                );
  } else {
    mTextOut->OutputString (mTextOut, (CHAR16 *)mDefaultMainHelp);
  }

  ResetTextColor ();
}

/**
  Internal: paint the menu starting at the configured top row. Uses
  padding and SetCursorPosition rather than ClearScreen to avoid the
  screen flicker that a full clear produces when the user navigates
  the menu.
**/
STATIC
VOID
DrawMenu (
  VOID
  )
{
  if (mTextOut == NULL) {
    return;
  }

  mTextOut->SetCursorPosition (mTextOut, 0, MENU_REDRAW_ROW);

  DrawHeader ();
  DrawBody ();
  DrawHelp ();
}

/**
  Internal: paint the status line directly below the menu body, in
  the color set by the most recent FastbootMenuSetStatus / on-select
  flash.
**/
STATIC
VOID
DrawStatusFooter (
  VOID
  )
{
  if (mTextOut == NULL) {
    return;
  }

  if (mStatusLine[0] != CHAR_NULL) {
    SetTextColor (ResolveColor (mStatusColor, MENU_STATUS_DEFAULT_COLOR));
    mTextOut->OutputString (mTextOut, L"\r\n  ");
    mTextOut->OutputString (mTextOut, (CHAR16 *)mStatusLine);
    mTextOut->OutputString (mTextOut, MENU_LINE_PAD L"\r\n");
    ResetTextColor ();
  }
}

/**
  Internal: load a Unicode string into the status line with an
  explicit color. Used by the printf-style FastbootMenuSetStatus and
  the on-select flash path so both paths share the same dirty-flag /
  color bookkeeping.
**/
STATIC
VOID
SetStatusInternal (
  IN UINTN         Color,
  IN CONST CHAR16  *Fmt,
  IN VA_LIST       Marker
  )
{
  UnicodeVSPrint (mStatusLine, sizeof (mStatusLine), Fmt, Marker);
  mStatusColor = Color;
  mStatusDirty = TRUE;
  mMenuDirty   = TRUE;
}

/**
  Internal: show the on-select flash message for @p Option in the
  status footer and force an immediate full repaint, so that even
  actions that reset the platform have a chance to surface the
  confirmation text before control transfers away. The on-select
  message is printed first; the action runs second.
**/
STATIC
VOID
FlashOnSelectFromEntry (
  IN CONST FASTBOOT_MENU_ENTRY  *Entry
  )
{
  CONST FASTBOOT_MENU_STRING  *Ms;

  if (Entry == NULL) {
    return;
  }

  Ms = &Entry->MsOnSelect;
  if ((Ms->Data == NULL) || (Ms->Data[0] == CHAR_NULL)) {
    return;
  }

  UnicodeSPrint (mStatusLine, sizeof (mStatusLine), L"%s", Ms->Data);
  mStatusColor = ResolveColor (Ms->Color, MENU_STATUS_DEFAULT_COLOR);
  mStatusDirty = TRUE;
  mMenuDirty   = TRUE;

  DrawMenu ();
  DrawStatusFooter ();
  mMenuDirty   = FALSE;
  mStatusDirty = FALSE;
}

/**
  Dispatch one menu row: optional nested NextMenu overlay, optional footer
  flash, then OnSelect.

  @retval TRUE    Request application exit.
  @retval FALSE   Stay in Fastboot app.
**/
STATIC
BOOLEAN
InvokeMenuEntry (
  IN CONST FASTBOOT_MENU_ENTRY  *Entry
  )
{
  if (Entry == NULL) {
    return FALSE;
  }

  DEBUG ((DEBUG_INFO, "%a: menu row select\n", __FUNCTION__));

  if (Entry->NextMenu != NULL) {
    return RunNestedMenu (Entry->NextMenu);
  }

  FlashOnSelectFromEntry (Entry);

  if (Entry->OnSelect != NULL) {
    return Entry->OnSelect (Entry->Arg);
  }

  return FALSE;
}

STATIC
BOOLEAN
InvokeBootMenuIndex (
  IN UINTN  Index
  )
{
  if (Index >= gBootloaderMainMenu.EntryCount) {
    return FALSE;
  }

  return InvokeMenuEntry (&gBootloaderMainMenu.Entries[Index]);
}

BOOLEAN
FastbootMenuConfirmAction (
  IN CONST CHAR16  *Title,
  IN CONST CHAR16  *Body
  )
{
  FASTBOOT_MENU_DESCRIPTOR  HostChooserMenu;

  ZeroMem (&HostChooserMenu, sizeof (HostChooserMenu));
  HostChooserMenu.Header.Valid         = TRUE;
  HostChooserMenu.Header.Ms.Color      = EFI_LIGHTRED;
  HostChooserMenu.Header.Ms.Data       = Title;
  HostChooserMenu.Body                 = Body;
  HostChooserMenu.HelpLegend           = NULL;
  HostChooserMenu.Entries              = mHostChooserEntries;
  HostChooserMenu.EntryCount           = ARRAY_SIZE (mHostChooserEntries);
  HostChooserMenu.CancelledStatusHint  = NULL;
  HostChooserMenu.ShowWorkingOnConfirm = TRUE;

  return RunNestedMenu (&HostChooserMenu);
}

STATIC
BOOLEAN
LockUnlockConfirmFromHost (
  IN BOOLEAN  WantLocked
  )
{
  CONST FASTBOOT_MENU_DESCRIPTOR  *Desc;

  Desc = WantLocked ? &gLockConfirmNextMenu : &gUnlockConfirmNextMenu;

  if (  !Desc->Header.Valid || (Desc->Header.Ms.Data == NULL)
     || (Desc->Body == NULL))
  {
    return FALSE;
  }

  return FastbootMenuConfirmAction (Desc->Header.Ms.Data, Desc->Body);
}

BOOLEAN
FastbootMenuLockConfirmFromHost (
  VOID
  )
{
  return LockUnlockConfirmFromHost (TRUE);
}

BOOLEAN
FastbootMenuUnlockConfirmFromHost (
  VOID
  )
{
  return LockUnlockConfirmFromHost (FALSE);
}

VOID
FastbootMenuInit (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *TextOut,
  IN EFI_EVENT                        IdleTimerEvent
  )
{
  mTextOut                   = TextOut;
  mIdleTimer                 = IdleTimerEvent;
  mSelected                  = 0;
  mMenuDirty                 = TRUE;
  mStatusDirty               = TRUE;
  mShouldContinueAndroidBoot = FALSE;
  mStatusColor               = MENU_STATUS_DEFAULT_COLOR;
  ZeroMem (mStatusLine, sizeof (mStatusLine));

  if (mTextOut != NULL) {
    mTextOut->ClearScreen (mTextOut);
    SetTextColor (MENU_BANNER_COLOR);
    mTextOut->OutputString (
                mTextOut,
                L"Android Fastboot mode - version " ANDROID_FASTBOOT_VERSION ". "
                                                                             L"SPACE exits this app; ENTER selects a menu item.\r\n"
                );
    ResetTextColor ();
  }
}

VOID
FastbootMenuSetStatus (
  IN CONST CHAR16  *Fmt,
  ...
  )
{
  VA_LIST  Marker;

  VA_START (Marker, Fmt);
  SetStatusInternal (MENU_STATUS_DEFAULT_COLOR, Fmt, Marker);
  VA_END (Marker);
}

VOID
FastbootMenuRedrawIfDirty (
  VOID
  )
{
  if (!mMenuDirty && !mStatusDirty) {
    return;
  }

  DrawMenu ();
  DrawStatusFooter ();

  mMenuDirty   = FALSE;
  mStatusDirty = FALSE;

  FastbootMenuArmIdleTimer ();
}

VOID
FastbootMenuArmIdleTimer (
  VOID
  )
{
  if (mIdleTimer == NULL) {
    return;
  }

  if (BOOTLOADER_MENU_TIMEOUT_SEC == 0) {
    gBS->SetTimer (mIdleTimer, TimerCancel, 0);
    return;
  }

  gBS->SetTimer (mIdleTimer, TimerRelative, BOOTLOADER_MENU_TIMEOUT_SEC * 10000000ULL);
}

BOOLEAN
FastbootMenuShouldContinueAndroidBoot (
  VOID
  )
{
  return mShouldContinueAndroidBoot;
}

FASTBOOT_MENU_KEY_RESULT
FastbootMenuHandleKey (
  IN EFI_INPUT_KEY  *Key
  )
{
  BOOLEAN  ExitOnEnter;

  if (Key == NULL) {
    return FastbootMenuKeyResultIgnored;
  }

  FastbootMenuArmIdleTimer ();

  if (Key->ScanCode == SCAN_UP) {
    mSelected = (mSelected == 0)
                 ? (gBootloaderMainMenu.EntryCount - 1) : (mSelected - 1);
    mMenuDirty = TRUE;
    return FastbootMenuKeyResultHandled;
  }

  if (Key->ScanCode == SCAN_DOWN) {
    mSelected  = (mSelected + 1) % gBootloaderMainMenu.EntryCount;
    mMenuDirty = TRUE;
    return FastbootMenuKeyResultHandled;
  }

  if ((Key->UnicodeChar == CHAR_CARRIAGE_RETURN) || (Key->UnicodeChar == L'\n')) {
    ExitOnEnter = InvokeBootMenuIndex (mSelected);
    if (ExitOnEnter) {
      return FastbootMenuKeyResultExitApp;
    }

    mMenuDirty = TRUE;
    return FastbootMenuKeyResultHandled;
  }

  if ((Key->UnicodeChar >= L'1') && (Key->UnicodeChar <= L'9')) {
    UINTN  Picked;

    Picked = (UINTN)(Key->UnicodeChar - L'1');
    if (Picked < gBootloaderMainMenu.EntryCount) {
      mSelected   = Picked;
      ExitOnEnter = InvokeBootMenuIndex (mSelected);
      if (ExitOnEnter) {
        return FastbootMenuKeyResultExitApp;
      }

      mMenuDirty = TRUE;
      return FastbootMenuKeyResultHandled;
    }
  }

  return FastbootMenuKeyResultIgnored;
}

BOOLEAN
FastbootMenuOnIdleTimeout (
  VOID
  )
{
  if (InvokeBootMenuIndex (0)) {
    return TRUE;
  }

  mMenuDirty = TRUE;
  return FALSE;
}
