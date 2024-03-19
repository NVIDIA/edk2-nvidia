/** @file

  Null Boot Chain Information Library

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/NVIDIADebugLib.h>

EFI_STATUS
EFIAPI
GetBootChainPartitionName (
  IN  CONST CHAR16  *BasePartitionName,
  IN  UINTN         BootChain,
  OUT CHAR16        *BootChainPartitionName
  )
{
  if ((BasePartitionName == NULL) || (BootChainPartitionName == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return StrCpyS (BootChainPartitionName, MAX_PARTITION_NAME_LEN, BasePartitionName);
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
  if ((GeneralPartitionName == NULL) || (ActivePartitionName == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return GetBootChainPartitionName (GeneralPartitionName, 0, ActivePartitionName);
}

EFI_STATUS
EFIAPI
GetPartitionBaseNameAndBootChain (
  IN  CONST CHAR16  *PartitionName,
  OUT CHAR16        *BaseName,
  OUT UINTN         *BootChain
  )
{
  if ((PartitionName == NULL) || (BaseName == NULL) || (BootChain == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *BootChain = 0;
  return StrCpyS (BaseName, MAX_PARTITION_NAME_LEN, PartitionName);
}

EFI_STATUS
EFIAPI
GetPartitionBaseNameAndBootChainAny (
  IN  CONST CHAR16  *PartitionName,
  OUT CHAR16        *BaseName,
  OUT UINTN         *BootChain
  )
{
  if ((PartitionName == NULL) || (BaseName == NULL) || (BootChain == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  StrCpyS (BaseName, MAX_PARTITION_NAME_LEN, PartitionName);
  *BootChain = 0;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GetBootChainPartitionNameAny (
  IN  CONST CHAR16  *BasePartitionName,
  IN  UINTN         BootChain,
  OUT CHAR16        *BootChainPartitionName
  )
{
  if ((BasePartitionName == NULL) || (BootChainPartitionName == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  StrCpyS (BootChainPartitionName, MAX_PARTITION_NAME_LEN, BasePartitionName);

  return EFI_SUCCESS;
}

UINT32
EFIAPI
GetBootChainForGpt (
  VOID
  )
{
  return 0;
}
