/** @file

  Tegra Platform Info Library.

  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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

#define MAX_REV_SIZE  (5)

typedef struct {
  UINT8    Val;
  CHAR8    Rev[MAX_REV_SIZE];
} ChipMinorRevTbl;

STATIC ChipMinorRevTbl  MinorRevEncoding[] = {
  { 1,  "A01" },
  { 2,  "A02" },
  { 3,  "A03" },
  { 5,  "B01" },
  { 6,  "B02" },
  { 7,  "B03" },
  { 9,  "C01" },
  { 10, "C02" },
  { 11, "C03" },
  { 13, "D01" },
  { 14, "D02" },
  { 15, "D03" }
};

STATIC
UINT32
TegraReadHidrevReg (
  VOID
  )
{
  UINT64  MiscRegBaseAddr = FixedPcdGet64 (PcdMiscRegBaseAddress);

  if (MiscRegBaseAddr == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read HIDREV register\n", __FUNCTION__));
    return MAX_UINT32;
  }

  return (MmioRead32 (MiscRegBaseAddr + HIDREV_OFFSET));
}

TEGRA_PLATFORM_TYPE
TegraGetPlatform (
  VOID
  )
{
  UINT32  Hidrev = TegraReadHidrevReg ();
  UINT32  PlatType;

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
  UINT32  Hidrev = TegraReadHidrevReg ();

  return ((Hidrev >> HIDREV_MAJORVER_SHIFT) & HIDREV_MAJORVER_MASK);
}

CHAR8 *
TegraGetMinorVersion (
  VOID
  )
{
  UINT32           HidRev = TegraReadHidrevReg ();
  UINT8            MinorRev;
  UINTN            Index;
  UINT32           ChipId;
  ChipMinorRevTbl  *MinorRevTbl = NULL;
  CHAR8            *MinorRevStr = NULL;
  UINTN            NumEncodings = 0;

  MinorRev = ((HidRev >> HIDREV_MINORREV_SHIFT) & HIDREV_MINORREV_MASK);

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case T194_CHIP_ID:
      MinorRevTbl = NULL;
      break;
    default:
      MinorRevTbl  = MinorRevEncoding;
      NumEncodings = ARRAY_SIZE (MinorRevEncoding);
      break;
  }

  if (MinorRevTbl == NULL) {
    goto ExitTegraGetMinorVersion;
  }

  for (Index = 0; Index <= NumEncodings; Index++) {
    if (MinorRev == MinorRevTbl->Val) {
      MinorRevStr = MinorRevTbl->Rev;
      break;
    }
  }

ExitTegraGetMinorVersion:
  return MinorRevStr;
}
