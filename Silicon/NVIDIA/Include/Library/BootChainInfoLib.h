/** @file

  Boot Chain Information Library

  Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BOOTCHAININFOLIB_H__
#define __BOOTCHAININFOLIB_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define MAX_PARTITION_NAME_LEN      36
#define BOOT_CHAIN_A                0
#define BOOT_CHAIN_B                1

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
  IN  CONST CHAR16 *GeneralPartitionName,
  OUT CHAR16       *ActivePartitionName
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
  IN  CONST CHAR16      *BasePartitionName,
  IN  UINTN             BootChain,
  OUT CHAR16            *BootChainPartitionName
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
  IN  CONST CHAR16      *PartitionName,
  OUT CHAR16            *BaseName,
  OUT UINTN             *BootChain
  );

#endif
