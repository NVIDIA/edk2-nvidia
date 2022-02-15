/** @file

  Tegra Platform Info Library.

  Copyright (c) 2018-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include "TegraPlatformInfoLibPrivate.h"

STATIC
UINT32
TegraReadHidrevReg (
  VOID
  )
{
  UINT64 MiscRegBaseAddr = FixedPcdGet64(PcdMiscRegBaseAddress);
  if (MiscRegBaseAddr == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read HIDREV register\n", __FUNCTION__));
    return MAX_UINT32;
  }

  return (MmioRead32(MiscRegBaseAddr + HIDREV_OFFSET));
}

TEGRA_PLATFORM_TYPE
TegraGetPlatform (
  VOID
  )
{
  UINT32 Hidrev = TegraReadHidrevReg();
  UINT32 PlatType;
  PlatType = ((Hidrev >> HIDREV_PRE_SI_PLAT_SHIFT) & HIDREV_PRE_SI_PLAT_MASK);
  if (PlatType >= TEGRA_PLATFORM_UNKNOWN) {
    return TEGRA_PLATFORM_UNKNOWN;
  } else {
    return PlatType;
  }
}

UINT32
TegraGetMajorVersion (
  VOID
  )
{
  UINT32 Hidrev = TegraReadHidrevReg();

  return ((Hidrev >> HIDREV_MAJORVER_SHIFT) & HIDREV_MAJORVER_MASK);
}
