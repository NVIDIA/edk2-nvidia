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
EFI_STATUS
TegraReadSocId (
  UINTN  SocParam,
  INT32  *SocId
  )
{
  UINT32  ChipId;

  if (SocId == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *SocId = ArmCallSmc1 (SMCCC_ARCH_SOC_ID, &SocParam, NULL, NULL);
  if (*SocId < 0) {
    return EFI_DEVICE_ERROR;
  } else {
    if (SocParam == SMCCC_ARCH_SOC_ID_GET_SOC_VERSION) {
      ChipId = TegraGetChipID ();
      if (ChipId == T194_CHIP_ID) {
        if (*SocId == T194_SOC_ID_VERSION_ALT) {
          *SocId = T194_SOC_ID_VERSION_FIX;
        }
      }
    }

    return EFI_SUCCESS;
  }
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
  INT32            SocId;
  UINT8            MinorRev;
  UINTN            Index;
  UINT32           ChipId;
  ChipMinorRevTbl  *MinorRevTbl = NULL;
  CHAR8            *MinorRevStr = " ";
  UINTN            NumEncodings = 0;

  if (EFI_ERROR (TegraReadSocId (SMCCC_ARCH_SOC_ID_GET_SOC_REVISION, &SocId))) {
    goto ExitTegraGetMinorVersion;
  }

  MinorRev = ((SocId >> SOC_ID_REVISION_MINORVER_SHIFT) & SOC_ID_REVISION_MINORVER_MASK);

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
    if (MinorRev == MinorRevTbl[Index].Val) {
      MinorRevStr = MinorRevTbl[Index].Rev;
      break;
    }
  }

ExitTegraGetMinorVersion:
  return MinorRevStr;
}
