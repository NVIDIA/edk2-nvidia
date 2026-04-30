/** @file

  BootConfig Protocol library

  SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BOOTCONFIG_PROTOCOL_LIB_H_
#define __BOOTCONFIG_PROTOCOL_LIB_H_

#include <Protocol/BootConfigUpdateProtocol.h>

#define MAX_ANDROID_BOOT_DTBO_IDX  10

EFI_STATUS
EFIAPI
GetBootConfigUpdateProtocol (
  OUT NVIDIA_BOOTCONFIG_UPDATE_PROTOCOL  **BootConfigProtocol
  );

EFI_STATUS
EFIAPI
BootConfigAddSerialNumber (
  CONST CHAR8  *NewValue OPTIONAL,
  CHAR8        *OutStrSn,
  UINT32       OutStrSnLen
  );

EFI_STATUS
EFIAPI
BootConfigAddSlotSuffix (
  VOID
  );

EFI_STATUS
EFIAPI
BootConfigSetDtboIdx (
  CONST CHAR8  *NewValue
  );

EFI_STATUS
EFIAPI
BootConfigAddDtboIdx (
  VOID
  );

EFI_STATUS
EFIAPI
BootConfigPrepareBootTimeArgs (
  VOID
  );

/**
  Test whether the shared bootconfig accumulator already contains an
  androidboot.<Key>=<ExpectedValue> entry, with proper line-boundary
  matching so e.g. "mode=safe" does not match "mode=safety".

  The accumulator is read from the singleton BootConfigUpdateProtocol
  instance (same one used by BootConfigAdd*); callers do not need to
  hold a protocol pointer themselves.

  @param[in] Key            The androidboot key to look for (e.g. "mode").
  @param[in] ExpectedValue  The value to look for (e.g. "safe").

  @retval TRUE   The exact androidboot.<Key>=<ExpectedValue> entry is
                 present in the accumulator.
  @retval FALSE  Otherwise (or the accumulator/protocol is unavailable).
**/
BOOLEAN
EFIAPI
BootConfigHasAndroidbootValue (
  IN CONST CHAR8  *Key,
  IN CONST CHAR8  *ExpectedValue
  );

#endif /* __BOOTCONFIG_PROTOCOL_LIB_H_ */
