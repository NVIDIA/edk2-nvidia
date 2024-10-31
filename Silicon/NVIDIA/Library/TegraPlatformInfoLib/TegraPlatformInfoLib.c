/** @file

  Tegra Platform Info Library.

  SPDX-FileCopyrightText: Copyright (c) 2018-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include "TegraPlatformInfoLibPrivate.h"
#include "Uefi/UefiBaseType.h"

#define MAX_REV_SIZE    (5)
#define MAX_OPT_SUBREV  (4)
#define MAX_MINORREV    (16)

STATIC CHAR8  MinorRevEncoding[MAX_MINORREV][MAX_OPT_SUBREV][MAX_REV_SIZE] = {
  { " ",   " ",    " ",    " "    },
  { "A01", "A01P", "A01Q", "A01R" },
  { "A02", "A02P", "A02Q", "A02R" },
  { "A03", "A03P", "A03Q", "A03R" },
  { "B01", "B01P", "B01Q", "B01R" },
  { "B02", "B02P", "B02Q", "B02R" },
  { "B03", "B03P", "B03Q", "B03R" },
  { "C01", "C01P", "C01Q", "C01R" },
  { "C02", "C02P", "C02Q", "C02R" },
  { "C03", "C03P", "C03Q", "C03R" },
  { "D01", "D01P", "D01Q", "D01R" },
  { "D02", "D02P", "D02Q", "D02R" },
  { "D03", "D03P", "D03Q", "D03R" },
  { " ",   " ",    " ",    " "    },
  { " ",   " ",    " ",    " "    },
  { " ",   " ",    " ",    " "    }
};

STATIC
EFI_STATUS
TegraReadSocId (
  UINTN  SocParam,
  INT32  *SocId
  )
{
  if (SocId == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *SocId = ArmCallSmc1 (SMCCC_ARCH_SOC_ID, &SocParam, NULL, NULL);
  if (*SocId < 0) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

TEGRA_PLATFORM_TYPE
TegraGetPlatform (
  VOID
  )
{
  UINT32  Hidrev;
  UINT32  PlatType;
  UINT64  MiscRegBaseAddr = FixedPcdGet64 (PcdMiscRegBaseAddress);

  if (MiscRegBaseAddr == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read HIDREV register\n", __FUNCTION__));
    return MAX_UINT32;
  }

  Hidrev   = MmioRead32 (MiscRegBaseAddr + HIDREV_OFFSET);
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
  INT32  SocId;

  if (EFI_ERROR (TegraReadSocId (SMCCC_ARCH_SOC_ID_GET_SOC_VERSION, &SocId))) {
    return MAX_UINT32;
  }

  return ((SocId >> SOC_ID_VERSION_MAJORVER_SHIFT) & SOC_ID_VERSION_MAJORVER_MASK);
}

CHAR8 *
TegraGetMinorVersion (
  VOID
  )
{
  INT32   SocId;
  UINT8   MinorRev;
  UINT8   OptSubRev;
  UINT32  ChipId;

  MinorRev  = 0;
  OptSubRev = 0;

  if (EFI_ERROR (TegraReadSocId (SMCCC_ARCH_SOC_ID_GET_SOC_REVISION, &SocId))) {
    goto ExitTegraGetMinorVersion;
  }

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case TH500_CHIP_ID:
      // Minor Rev and Opt Subrev are swapped for TH500
      MinorRev  = ((SocId >> SOC_ID_REVISION_OPT_SUBREV_SHIFT) & SOC_ID_REVISION_OPT_SUBREV_MASK);
      OptSubRev = ((SocId >> SOC_ID_REVISION_MINORVER_SHIFT) & SOC_ID_REVISION_MINORVER_MASK);
      break;

    default:
      MinorRev  = ((SocId >> SOC_ID_REVISION_MINORVER_SHIFT) & SOC_ID_REVISION_MINORVER_MASK);
      OptSubRev = ((SocId >> SOC_ID_REVISION_OPT_SUBREV_SHIFT) & SOC_ID_REVISION_OPT_SUBREV_MASK);
      break;
  }

  if ((MinorRev >= MAX_MINORREV) ||
      (OptSubRev >= MAX_OPT_SUBREV))
  {
    MinorRev  = 0;
    OptSubRev = 0;
  }

ExitTegraGetMinorVersion:
  return MinorRevEncoding[MinorRev][OptSubRev];
}
