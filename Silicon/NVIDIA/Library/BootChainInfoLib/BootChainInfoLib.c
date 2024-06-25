/** @file

  Boot Chain Information Library

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/PrintLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PcdLib.h>
#include <Library/NVIDIADebugLib.h>

#define MAX_BOOT_CHAIN_INFO_MAPPING  2
#define PARTITION_PREFIX_LENGTH      2
#define PARTITION_SUFFIX_LENGTH      2

CHAR16  *SuffixPartitionNameId[MAX_BOOT_CHAIN_INFO_MAPPING] = {
  L"_a",
  L"_b",
};

CHAR16  *T234PartitionNameId[MAX_BOOT_CHAIN_INFO_MAPPING] = {
  L"A_",
  L"B_",
};

CHAR16  *T194PartitionNameId[MAX_BOOT_CHAIN_INFO_MAPPING] = {
  L"",
  L"_b",
};

EFI_STATUS
EFIAPI
GetBootChainPartitionName (
  IN  CONST CHAR16  *BasePartitionName,
  IN  UINTN         BootChain,
  OUT CHAR16        *BootChainPartitionName
  )
{
  UINTN   ChipID;
  CHAR16  *Identifier;

  if ((BasePartitionName == NULL) || (BootChainPartitionName == NULL) ||
      (BootChain >= MAX_BOOT_CHAIN_INFO_MAPPING))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (TegraGetPlatform () != TEGRA_PLATFORM_SILICON) {
    return StrCpyS (
             BootChainPartitionName,
             MAX_PARTITION_NAME_LEN,
             BasePartitionName
             );
  }

  if (PcdGetBool (PcdPartitionNamesHaveSuffixes)) {
    Identifier = SuffixPartitionNameId[BootChain];
    UnicodeSPrint (
      BootChainPartitionName,
      sizeof (CHAR16) * MAX_PARTITION_NAME_LEN,
      L"%s%s",
      BasePartitionName,
      Identifier
      );

    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID ();
  switch (ChipID) {
    case T234_CHIP_ID:
    case T264_CHIP_ID:
      Identifier = T234PartitionNameId[BootChain];
      UnicodeSPrint (
        BootChainPartitionName,
        sizeof (CHAR16) * MAX_PARTITION_NAME_LEN,
        L"%s%s",
        Identifier,
        BasePartitionName
        );
      break;
    case T194_CHIP_ID:
      Identifier = T194PartitionNameId[BootChain];
      UnicodeSPrint (
        BootChainPartitionName,
        sizeof (CHAR16) * MAX_PARTITION_NAME_LEN,
        L"%s%s",
        BasePartitionName,
        Identifier
        );
      break;
    default:
      return EFI_UNSUPPORTED;
      break;
  }

  return EFI_SUCCESS;
}

/**
  Retrieve Active Boot Chain Partition Name

**/
EFI_STATUS
EFIAPI
GetActivePartitionName (
  IN  CONST CHAR16  *GeneralPartitionName,
  OUT CHAR16        *ActivePartitionName
  )
{
  UINT32  BootChain;
  VOID    *Hob;

  if ((GeneralPartitionName == NULL) || (ActivePartitionName == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    BootChain = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ActiveBootChain;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting active boot chain\n",
      __FUNCTION__
      ));
    return StrCpyS (
             ActivePartitionName,
             MAX_PARTITION_NAME_LEN,
             GeneralPartitionName
             );
  }

  return GetBootChainPartitionName (
           GeneralPartitionName,
           BootChain,
           ActivePartitionName
           );
}

EFI_STATUS
EFIAPI
GetPartitionBaseNameAndBootChainAny (
  IN  CONST CHAR16  *PartitionName,
  OUT CHAR16        *BaseName,
  OUT UINTN         *BootChain
  )
{
  UINTN         Index;
  CONST CHAR16  *SuffixStart;
  UINTN         NameLength;

  if ((PartitionName == NULL) || (BaseName == NULL) || (BootChain == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // first check for t234-style name with prefix
  for (Index = 0; Index < MAX_BOOT_CHAIN_INFO_MAPPING; Index++) {
    if (StrnCmp (PartitionName, T234PartitionNameId[Index], PARTITION_PREFIX_LENGTH) == 0) {
      StrCpyS (BaseName, MAX_PARTITION_NAME_LEN, PartitionName + PARTITION_PREFIX_LENGTH);
      *BootChain = Index;
      return EFI_SUCCESS;
    }
  }

  // check for name with suffix
  NameLength  = StrLen (PartitionName);
  SuffixStart = PartitionName + NameLength - PARTITION_SUFFIX_LENGTH;
  for (Index = 0; Index < MAX_BOOT_CHAIN_INFO_MAPPING; Index++) {
    if (StrnCmp (SuffixStart, SuffixPartitionNameId[Index], PARTITION_SUFFIX_LENGTH) == 0) {
      StrnCpyS (
        BaseName,
        MAX_PARTITION_NAME_LEN,
        PartitionName,
        NameLength - PARTITION_SUFFIX_LENGTH
        );
      *BootChain = Index;
      return EFI_SUCCESS;
    }
  }

  // check for t194-style name with suffix
  {
    CONST CHAR16  *BSuffix;
    UINTN         BSuffixLength;
    CONST CHAR16  *SuffixStart;
    UINTN         NameLength;

    // check if boot chain B suffix is present, if not, it's boot chain A
    BSuffix       = T194PartitionNameId[1];
    BSuffixLength = StrLen (BSuffix);
    NameLength    = StrLen (PartitionName);
    if (NameLength > BSuffixLength) {
      SuffixStart = PartitionName + NameLength - BSuffixLength;
      if (StrnCmp (SuffixStart, BSuffix, BSuffixLength) == 0) {
        StrnCpyS (
          BaseName,
          MAX_PARTITION_NAME_LEN,
          PartitionName,
          NameLength - BSuffixLength
          );
        *BootChain = 1;
        return EFI_SUCCESS;
      }
    }
  }

  // default is partition name is base name
  StrCpyS (BaseName, MAX_PARTITION_NAME_LEN, PartitionName);
  *BootChain = BOOT_CHAIN_A;
  return EFI_SUCCESS;
}

UINT32
EFIAPI
GetBootChainForGpt (
  VOID
  )
{
  VOID  *Hob;

  if (!PcdGetBool (PcdGptIsPerBootChain)) {
    return BOOT_CHAIN_A;
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  NV_ASSERT_RETURN (
    ((Hob != NULL) && (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO))),
    return BOOT_CHAIN_A,
    "%a: Error getting boot chain\n",
    __FUNCTION__
    );

  return ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ActiveBootChain;
}
