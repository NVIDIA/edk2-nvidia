/** @file

  Tegra Platform Info Library.

  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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

UINT32
TegraGetChipID (
  VOID
  )
{
  UINT32 Hidrev = TegraReadHidrevReg();
  if (Hidrev == MAX_UINT32) {
    return MAX_UINT32;
  } else {
    return ((Hidrev >> HIDREV_CHIPID_SHIFT) & HIDREV_CHIPID_MASK);
  }
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
