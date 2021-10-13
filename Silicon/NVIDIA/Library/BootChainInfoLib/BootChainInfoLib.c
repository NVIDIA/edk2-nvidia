/** @file

  Boot Chain Information Library

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <Library/BootChainInfoLib.h>
#include <Library/PrintLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/BaseLib.h>

#define MAX_BOOT_CHAIN_INFO_MAPPING 2
#define PARTITION_PREFIX_LENGTH     2

CHAR16 *T234PartitionNameId[MAX_BOOT_CHAIN_INFO_MAPPING] = {
  L"A_",
  L"B_",
};

CHAR16 *T194PartitionNameId[MAX_BOOT_CHAIN_INFO_MAPPING] = {
  L"",
  L"_b",
};

EFI_STATUS
EFIAPI
GetBootChainPartitionName (
  IN  CONST CHAR16      *BasePartitionName,
  IN  UINTN             BootChain,
  OUT CHAR16            *BootChainPartitionName
  )
{
  UINTN      ChipID;
  CHAR16     *Identifier;

  if ((BasePartitionName == NULL) || (BootChainPartitionName == NULL) ||
      (BootChain >= MAX_BOOT_CHAIN_INFO_MAPPING)) {
    return EFI_INVALID_PARAMETER;
  }

  ChipID = TegraGetChipID ();
  switch (ChipID) {
    case T234_CHIP_ID:
      Identifier = T234PartitionNameId[BootChain];
      UnicodeSPrint (BootChainPartitionName,
                     sizeof (CHAR16) * MAX_PARTITION_NAME_LEN,
                     L"%s%s",
                     Identifier,
                     BasePartitionName);
      break;
    case T194_CHIP_ID:
      Identifier = T194PartitionNameId[BootChain];
      UnicodeSPrint (BootChainPartitionName,
                     sizeof (CHAR16) * MAX_PARTITION_NAME_LEN,
                     L"%s%s",
                     BasePartitionName,
                     Identifier);
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
  IN  CONST CHAR16 *GeneralPartitionName,
  OUT CHAR16       *ActivePartitionName
)
{
  EFI_STATUS Status;
  UINT32     BootChain;

  if ((GeneralPartitionName == NULL) || (ActivePartitionName == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetActiveBootChain (&BootChain);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return GetBootChainPartitionName (GeneralPartitionName,
                                    BootChain,
                                    ActivePartitionName);
}

EFI_STATUS
EFIAPI
GetPartitionBaseNameAndBootChain (
  IN  CONST CHAR16      *PartitionName,
  OUT CHAR16            *BaseName,
  OUT UINTN             *BootChain
  )
{
  UINTN         ChipID;
  UINTN         Index;

  if ((PartitionName == NULL) || (BaseName == NULL) || (BootChain == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ChipID = TegraGetChipID ();
  switch (ChipID) {
    case T234_CHIP_ID: {
      for (Index = 0; Index < MAX_BOOT_CHAIN_INFO_MAPPING; Index++) {
        if (StrnCmp (PartitionName, T234PartitionNameId[Index], PARTITION_PREFIX_LENGTH) == 0) {
          StrCpyS (BaseName, MAX_PARTITION_NAME_LEN, PartitionName + PARTITION_PREFIX_LENGTH);
          *BootChain = Index;
          return EFI_SUCCESS;
        }
      }
    }
    break;
    case T194_CHIP_ID: {
      CONST CHAR16  *BSuffix;
      UINTN         BSuffixLength;
      CONST CHAR16  *SuffixStart;
      UINTN         NameLength;

      // check if boot chain B suffix is present, if not, it's boot chain A
      BSuffix = T194PartitionNameId[1];
      BSuffixLength = StrLen (BSuffix);
      NameLength = StrLen (PartitionName);
      if (NameLength > BSuffixLength) {
        SuffixStart = PartitionName + NameLength - BSuffixLength;
        if (StrnCmp (SuffixStart, BSuffix, BSuffixLength) == 0) {
          StrnCpyS (BaseName, MAX_PARTITION_NAME_LEN, PartitionName,
                    NameLength - BSuffixLength);
          *BootChain = 1;
          return EFI_SUCCESS;
        }
      }
      StrCpyS (BaseName, MAX_PARTITION_NAME_LEN, PartitionName);
      *BootChain = 0;
      return EFI_SUCCESS;
    }
    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_NOT_FOUND;
}
