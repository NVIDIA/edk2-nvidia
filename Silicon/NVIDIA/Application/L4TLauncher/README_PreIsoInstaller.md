# PreIsoInstaller — QSPI Capsule Update for ISO Install Flow

## Overview

PreIsoInstaller adds firmware capsule update support to L4TLauncher for
ISO-based installation media on NVIDIA Jetson platforms. When the boot
device is detected as ISO installation media, PreIsoInstaller compares the
capsule version against both A/B firmware slots and, if either slot needs an
update, stages a capsule file and triggers a warm reset for the firmware to
apply it.

## Boot Flow

```
L4TLauncher
  |
  +-- HandleIsoBootMedia()
        |
        +-- IsIsoIdFileValid()    -- cheap: check EFI\ISO_ID matches SHA256(EFI\version + "L4T")
        |                            (returns early if check fails)
        +-- IsIso9660BootMedia()  -- expensive: check ISO9660 PVD "CD001" at sector 16
        |                            (returns early if check fails)
        +-- Clear rootfs status register
        |
        +-- RunPreIsoInstaller()
        |     +-- PreIsoLogInit (open/create EFI\PreIsoInstaller.log)
        |     +-- Validate BootChain and read firmware versions
        |     |      (SystemFwVersions variable or ESRT fallback)
        |     +-- Read capsule version from EFI\version
        |     +-- Compare capsule version against current and non-current slots
        |     +-- Boot-loop guard (max 5 staging attempts via PreIsoCapsuleStaged variable)
        |     +-- Read board info from CVM EEPROM
        |     +-- Ensure TegraPlatformSpec / TegraPlatformCompatSpec variables
        |     +-- Select capsule file based on board ID, SKU, FAB
        |     +-- Prompt user for confirmation (30s timeout, auto-skip if no input)
        |     +-- Copy capsule to EFI\UpdateCapsule\, set OsIndications, warm reset
        |     +-- PreIsoLogClose
        |
        +-- LoadAndStartShim()    -- load EFI\BOOT\shimaa64.efi (fatal if missing)
```

## ISO Detection

Two methods detect ISO installation media (both must pass):

1. **ISO_ID file** (evaluated first — cheap): Reads `EFI\ISO_ID` from the
   ESP and compares its content (64 hex characters) against
   `SHA256(trimmed EFI\version content + "L4T")` using `CompareMem`.
   Trailing CR, LF, and space characters are ignored in the
   version file; the ISO_ID value is read until CR, LF, or space, and
   uppercase `A`-`F` are normalized to lowercase before comparison. If this
   check fails, `IsIso9660BootMedia` is never called, avoiding the expensive
   BlockIo enumeration on normal (non-ISO) boots.

2. **ISO9660 PVD** (evaluated second — expensive): Reads the Primary Volume
   Descriptor at byte offset 32768 (sector 16) from the parent disk or
   partition 1 and checks for the `CD001` magic signature with volume
   descriptor type `0x01`. This call enumerates all BlockIo handles via
   `LocateHandleBuffer` and walks child controllers, so it is gated behind
   the ISO_ID check.

## User Confirmation Prompt

When a capsule update is needed (and this is the first staging attempt),
a prompt is displayed on the console:

```
  *** NVIDIA QSPI Firmware Update Required ***

  A newer firmware version has been detected.
  New FW version  : <branch>.<major>.<minor>
  Current version : <branch>.<major>.<minor>

  WARNING: Skipping the firmware update may cause the
  subsequent ISO installation to fail.

  Do you want to update the firmware?
  Press [Y] to proceed, [N] to skip.
  Auto-skipping in 30 seconds...

  Both firmware slots (A and B) will be updated.
  This requires 2 automatic reboots. Do not power off
  the system until the update is complete.
```

Version format `0xAABBCCDD` is displayed as `BB.CC.DD` in decimal;
`0xAA` is always 0 (e.g., `0x00270000` -> `39.00.00`).

- **[Y]**: Proceeds with capsule update (stages capsule, sets OsIndications,
  warm reset). On success, `ResetSystem` does not return.
- **[N]**: Skips capsule update and continues to shim boot.
- **Timeout (30s)**: Automatically skips the update.

On subsequent staging attempts (StagedFlag > 0, i.e., after a reboot from a
previous capsule update for the other A/B slot), the prompt is skipped and
the update proceeds automatically.

## Version Comparison

The capsule version is parsed from `EFI\version` as a UINT32 hex value. The
installed firmware versions come from the `SystemFwVersions` NVRAM variable
when present; if that variable cannot be read, the ESRT system firmware
version is used for both slots.

`BootChain` selects which firmware slot is current:

| BootChain | Current slot | Non-current slot |
|-----------|--------------|------------------|
| 0         | Slot A       | Slot B           |
| 1         | Slot B       | Slot A           |

Any other `BootChain` value is invalid and aborts the PreIsoInstaller flow.
An update is required if the capsule version is newer than either the current
or non-current slot.
The comparison has two early-exit cases:

1. If the current slot version is earlier than 35.5.0, the firmware is too old
   to support the ISO capsule layout change. PreIsoInstaller prints an error
   message and returns `EFI_ABORTED`, causing L4TLauncher to halt instead of
   continuing the ISO installation.
2. If the ISO version is earlier than the non-current slot version,
   PreIsoInstaller skips the capsule update and continues the ISO installation.

If no update is needed, `PreIsoCapsuleStaged` is deleted and shim boot continues.

## Platform Spec Variables

PreIsoInstaller ensures the following NVRAM variables exist. It creates them
from Tegra-format EEPROM data and the root DTB `compatible` property when they
are missing. Existing variables are not overwritten:

- **TegraPlatformSpec**: `<BoardId>-<FAB>-<SKU>-<Rev>.0-1-2-<BoardName>-`
- **TegraPlatformCompatSpec**: `<BoardId>-<CompatFAB>-<SKU>--1--<BoardName>-`

These variables are consumed by downstream boot components for platform
identification.

When PreIsoInstaller synthesizes these variables, the base board name and
compatible FAB are resolved from EEPROM data as follows:

| Board ID | SKU | BoardName | CompatFAB |
|----------|-----|-----------|-----------|
| 3701 | 4, 5 | jetson-agx-orin-devkit | 300 |
| 3701 | 8 | jetson-agx-orin-devkit-industrial | 300 |
| 3701 | 0 | jetson-agx-orin-devkit | 000 for T*, E*, or numeric FAB < 300; otherwise 300 |
| 3701 | other | jetson-agx-orin-devkit | 300 |
| 3767 | any | jetson-orin-nano-devkit | 000 |
| 3834 | 0 | jetson-agx-thor-t4000 | 000 |
| 3834 | 8 | jetson-agx-thor-devkit | 401 for EB9+, TS5+, RC2+, or numeric FAB > 400; otherwise 000 |
| 3834 | other | jetson-agx-thor-devkit | 000 |
| other | any | jetson-unknown | 000 |

### DT compatible variant board names

Variants that cannot be distinguished from EEPROM SKU/FAB alone are detected
from the root DTB `compatible` property when PreIsoInstaller synthesizes
`TegraPlatformSpec` and `TegraPlatformCompatSpec`.

The `DTB compatible match` column shows the raw token in the DTB compatible
property. The generated `TegraPlatformCompatSpec` stores the full BoardName
shown in the table, and capsule selection matches that generated board name.

| Board ID | DTB compatible match | BoardName                          | CompatFAB |
|----------|----------------------|------------------------------------|-----------|
| 3701     | `super`              | jetson-agx-orin-devkit-super       | EEPROM rule |
| 3767     | `nanoe8gb` + `super` | jetson-orin-nanoe8gb-devkit-super  | 000 |
| 3767     | `nanoe8gb`           | jetson-orin-nanoe8gb-devkit        | 000 |
| 3767     | `super`              | jetson-orin-nano-devkit-super      | 000 |

For Orin Nano, NanoE8GB has base and Super variants.

## Capsule File Selection

The capsule file is selected based on board ID, SKU, FAB from the CVM EEPROM,
and the `TegraPlatformCompatSpec` board name. That variable can be an existing
platform variable or one synthesized by PreIsoInstaller from EEPROM plus the
root DTB `compatible` property:

| Board ID | SKU     | TegraPlatformCompatSpec board name      | Capsule File                          |
|----------|---------|-----------------------------------------|---------------------------------------|
| 3701     | any     | contains `super`                        | TEGRA_BL_3701_agx_super.Cap           |
| 3701     | 4, 5    | otherwise                               | TEGRA_BL_3701_agx.Cap                 |
| 3701     | 8       | otherwise                               | TEGRA_BL_3701_agx_ind.Cap             |
| 3701     | 0       | otherwise (FAB == 300)                  | TEGRA_BL_3701_agx.Cap                 |
| 3701     | 0       | otherwise (FAB != 300)                  | TEGRA_BL_3701_000.Cap                 |
| 3701     | other   | otherwise                               | TEGRA_BL_3701_agx.Cap (default)       |
| 3767     | any     | contains `jetson-orin-nanoe8gb-devkit` and `super` | TEGRA_BL_3767_nanoe8gb_super.Cap      |
| 3767     | any     | contains `jetson-orin-nanoe8gb-devkit` (no `super`) | TEGRA_BL_3767_nanoe8gb.Cap            |
| 3767     | any     | contains `super` (no `jetson-orin-nanoe8gb-devkit`) | TEGRA_BL_3767_super.Cap               |
| 3767     | any     | otherwise / missing / read failure      | TEGRA_BL_3767.Cap                     |
| 3834     | any     | any                                     | TEGRA_BL_3834_agx.Cap                 |
| other    | any     | any                                     | TEGRA_BL_3701_agx.Cap (default)       |
| non-Tegra EEPROM | any | any                                 | TEGRA_BL_3701_agx.Cap (default)       |

Capsule selection uses substring matches against the
`TegraPlatformCompatSpec` NVRAM variable: the full board-name prefix
`jetson-orin-nanoe8gb-devkit` (including its `-super` form) and `super`
(matches any `-super` board name). When `TegraPlatformCompatSpec` cannot be
read, capsule selection falls back to the non-variant default for that board
(e.g., `TEGRA_BL_3701_agx.Cap` for AGX Orin SKU 4, `TEGRA_BL_3767.Cap` for
Orin Nano).

For boards with alphabetic FAB strings (e.g., `T00`, `EB9`), FAB is treated
as 0 for capsule selection purposes. Capsule files are copied from `EFI\` to
`EFI\UpdateCapsule\`; the destination directory is created if needed.

## Boot-Loop Guard

A `PreIsoCapsuleStaged` NVRAM variable (UINT8) tracks how many times a
capsule has been staged. Normal A/B slot updates require 2 staging cycles.
If the counter reaches 5 without the firmware version being bumped (indicating
the capsule is failing to apply), the update is aborted with `EFI_ABORTED`,
the `OsIndications` capsule delivery flag is cleared, and the staged counter
is deleted. `L4TLauncher` prints `Iso boot loop detected, halting` and
halts the system (`CpuDeadLoop`) on `EFI_ABORTED`.

Failed staging attempts (e.g., `PerformCapsuleUpdate` errors) still count
toward the limit — the counter is not decremented on failure. This ensures
a consistently-failing capsule (corrupt file, full filesystem) eventually
triggers the guard rather than retrying indefinitely. The counter is cleared
on the success path when the firmware version has been bumped.

## Logging

All diagnostic output is written to `EFI\PreIsoInstaller.log` on the ESP
(capped at 10 MB, truncated when exceeded). Key messages are also printed
to the UEFI console (`ConOut`) via `PreIsoLogPrint`; log-only messages
use `PreIsoLogWrite`.

The log file is opened by `PreIsoLogInit` at the start of
`RunPreIsoInstaller`. Detection-phase messages from `IsIsoIdFileValid`
and `IsIso9660BootMedia` (called from `HandleIsoBootMedia` before
`RunPreIsoInstaller`) go through `PreIsoLogWrite`, which is a no-op
until `PreIsoLogInit` opens the log file. As a result, detection-phase
`PreIsoLogWrite` calls are silently dropped; only `ErrorPrint` output
(e.g., from `IsIso9660BootMedia`) reaches the console. Log writes are
buffered; a single `FileHandleFlush` is performed in `PreIsoLogClose`
before the file handle is closed.

## Files

| File | Description |
|------|-------------|
| `L4TPreIsoInstaller.h` | Public API, constants, board definitions |
| `L4TPreIsoInstaller.c` | Implementation |
| `L4TLauncher.c` | Integration point (calls `HandleIsoBootMedia`) |
