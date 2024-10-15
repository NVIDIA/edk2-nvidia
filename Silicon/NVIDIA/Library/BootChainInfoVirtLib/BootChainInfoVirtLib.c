/** @file

  Virt Boot Chain Information Library

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <libfdt.h>

#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/NVIDIADebugLib.h>

#define INVALID_BOOTCHAIN_INDEX  MAX_UINT32
#define PARTITION_SUFFIX_LEN     StrLen (PartitionNameSuffix[0])

STATIC CHAR16  *PartitionNameSuffix[BOOT_CHAIN_COUNT] = {
  L"_a",
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
  CHAR16  *Identifier;

  if ((BasePartitionName == NULL) || (BootChainPartitionName == NULL) ||
      (BootChain >= BOOT_CHAIN_COUNT))
  {
    return EFI_INVALID_PARAMETER;
  }

  Identifier = PartitionNameSuffix[BootChain];
  UnicodeSPrint (
    BootChainPartitionName,
    sizeof (CHAR16) * MAX_PARTITION_NAME_LEN,
    L"%s%s",
    BasePartitionName,
    Identifier
    );

  return EFI_SUCCESS;
}

STATIC
UINT32
EFIAPI
GetBootChainFromDtb (
  VOID
  )
{
  EFI_STATUS    Status;
  INT32         NodeOffset;
  VOID          *UefiDtb;
  CONST UINT32  *BootChainIdx;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &UefiDtb);
  if (EFI_ERROR (Status)) {
    return INVALID_BOOTCHAIN_INDEX;
  }

  NodeOffset = fdt_path_offset (UefiDtb, "/chosen/update-info");
  if (NodeOffset < 0) {
    return INVALID_BOOTCHAIN_INDEX;
  }

  BootChainIdx = fdt_getprop (UefiDtb, NodeOffset, "active-boot-chain", NULL);

  return BootChainIdx == NULL ? INVALID_BOOTCHAIN_INDEX : fdt32_to_cpu (*BootChainIdx);
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
  UINTN  BootChain;

  if ((GeneralPartitionName == NULL) || (ActivePartitionName == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  BootChain = GetBootChainFromDtb ();
  if (BootChain >= BOOT_CHAIN_COUNT) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Index of boot chain %u exceeded the limit of %u\n",
      __FUNCTION__,
      BootChain,
      BOOT_CHAIN_COUNT - 1U
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

STATIC
EFI_STATUS
EFIAPI
GetPartitionBaseNameAndBootChain (
  IN  CONST CHAR16  *PartitionName,
  OUT CHAR16        *BaseName,
  OUT UINTN         *BootChain
  )
{
  CONST CHAR16  *SuffixStart;
  UINTN         NameLength;
  UINTN         Index;

  if ((PartitionName == NULL) || (BaseName == NULL) || (BootChain == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  NameLength  = StrLen (PartitionName);
  SuffixStart = PartitionName + NameLength - PARTITION_SUFFIX_LEN;
  if (NameLength < PARTITION_SUFFIX_LEN) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Can not get base partition name from %s\n",
      __FUNCTION__,
      PartitionName
      ));
    return EFI_INVALID_PARAMETER;
  }

  *BootChain = BOOT_CHAIN_A;
  for (Index = 0; Index < BOOT_CHAIN_COUNT; Index++) {
    if (StrnCmp (SuffixStart, PartitionNameSuffix[Index], PARTITION_SUFFIX_LEN) == 0) {
      *BootChain  = Index;
      NameLength -= PARTITION_SUFFIX_LEN;
      break;
    }
  }

  StrnCpyS (BaseName, MAX_PARTITION_NAME_LEN, PartitionName, NameLength);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GetPartitionBaseNameAndBootChainAny (
  IN  CONST CHAR16  *PartitionName,
  OUT CHAR16        *BaseName,
  OUT UINTN         *BootChain
  )
{
  return GetPartitionBaseNameAndBootChain (PartitionName, BaseName, BootChain);
}

UINT32
EFIAPI
GetBootChainForGpt (
  VOID
  )
{
  UINT32  BootChain;

  BootChain = GetBootChainFromDtb ();
  if (BootChain >= BOOT_CHAIN_COUNT) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Index of boot chain %u exceeded the limit of %u\n",
      __FUNCTION__,
      BootChain,
      BOOT_CHAIN_COUNT - 1U
      ));
    BootChain = BOOT_CHAIN_A;
  }

  return BootChain;
}
