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

CHAR16 *T234PartitionNameId[MAX_BOOT_CHAIN_INFO_MAPPING] = {
  L"A_",
  L"B_",
};

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
  UINTN      ChipID;
  UINT32     BootChain;
  CHAR16     *Identifier;

  Status = GetActiveBootChain (&BootChain);
  if (EFI_ERROR (Status)) {
    UnicodeSPrint (ActivePartitionName, sizeof (CHAR16) * MAX_PARTITION_NAME_LEN, L"%s", GeneralPartitionName);
    return Status;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T234_CHIP_ID:
      Identifier = T234PartitionNameId[BootChain];
      UnicodeSPrint (ActivePartitionName, sizeof (CHAR16) * MAX_PARTITION_NAME_LEN, L"%s%s", Identifier, GeneralPartitionName);
      break;
    default:
      UnicodeSPrint (ActivePartitionName, sizeof (CHAR16) * MAX_PARTITION_NAME_LEN, L"%s", GeneralPartitionName);
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}
