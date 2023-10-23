/** @file

  Boot Chain Information Library

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BOOTCHAININFOLIB_H__
#define __BOOTCHAININFOLIB_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define MAX_PARTITION_NAME_LEN  36
#define BOOT_CHAIN_A            0
#define BOOT_CHAIN_B            1
#define BOOT_CHAIN_COUNT        2
#define OTHER_BOOT_CHAIN(BootChain)  ((BootChain) ^ 1)

/**
  Retrieve Active Boot Chain Partition Name

  @param[in]  GeneralPartitionName  Pointer to general partition name string
  @param[out] ActivePartitionName   Pointer to buffer of MAX_PARTITION_NAME_LEN
                                    CHAR16 characters to contain the
                                    active boot chain partition name

  @retval EFI_SUCCESS               Operation successful.
  @retval others                    Error occurred.

**/
EFI_STATUS
EFIAPI
GetActivePartitionName (
  IN  CONST CHAR16  *GeneralPartitionName,
  OUT CHAR16        *ActivePartitionName
  );

/**
  Get boot chain partition name from base partition name and boot chain index.

  @param[in]  BasePartitionName         Pointer to base partition name string
  @param[in]  BootChain                 Boot chain (0=a, 1=b)
  @param[out] BootChainPartitionName    Pointer to buffer of MAX_PARTITION_NAME_LEN
                                        CHAR16 characters to contain the
                                        boot chain partition name

  @retval EFI_SUCCESS                   Operation successful.
  @retval others                        Error occurred.

**/
EFI_STATUS
EFIAPI
GetBootChainPartitionName (
  IN  CONST CHAR16  *BasePartitionName,
  IN  UINTN         BootChain,
  OUT CHAR16        *BootChainPartitionName
  );

/**
  Get base name and boot chain index from partition name

  @param[in]  PartitionName         Pointer to partition name string
  @param[out] BaseName              Pointer to buffer of MAX_PARTITION_NAME_LEN
                                    CHAR16 characters to contain the
                                    base partition name
  @param[out] BootChain             Pointer to boot chain (0=a, 1=b)

  @retval EFI_SUCCESS               Operation successful.
  @retval others                    Error occurred.
**/
EFI_STATUS
EFIAPI
GetPartitionBaseNameAndBootChain (
  IN  CONST CHAR16  *PartitionName,
  OUT CHAR16        *BaseName,
  OUT UINTN         *BootChain
  );

/**
  Get base name and boot chain index from partition name without knowing
  platform.

  @param[in]  PartitionName         Pointer to partition name string
  @param[out] BaseName              Pointer to buffer of MAX_PARTITION_NAME_LEN
                                    CHAR16 characters to contain the
                                    base partition name
  @param[out] BootChain             Pointer to boot chain (0=a, 1=b)

  @retval EFI_SUCCESS               Operation successful.
  @retval others                    Error occurred.
**/
EFI_STATUS
EFIAPI
GetPartitionBaseNameAndBootChainAny (
  IN  CONST CHAR16  *PartitionName,
  OUT CHAR16        *BaseName,
  OUT UINTN         *BootChain
  );

/**
  Retrieve Boot Chain Partition Name without knowing platform.
  Returns T234-style names.

  @param[in]  BasePartitionName        Pointer to base partition name string
  @param[in]  BootChain                Boot chain (0=a, 1=b)
  @param[out] BootChainPartitionName   Pointer to buffer of MAX_PARTITION_NAME_LEN
                                       CHAR16 characters to contain the
                                       boot chain partition name

  @retval EFI_SUCCESS               Operation successful.
  @retval others                    Error occurred.

**/
EFI_STATUS
EFIAPI
GetBootChainPartitionNameAny (
  IN  CONST CHAR16  *BasePartitionName,
  IN  UINTN         BootChain,
  OUT CHAR16        *BootChainPartitionName
  );

/**
  Get boot chain to use for locating GPT.

  @retval UINT32            Boot chain (0=a, 1=b).

**/
UINT32
EFIAPI
GetBootChainForGpt (
  VOID
  );

#endif
