/** @file

  GPT - GUID Partition Table Library Public Interface
        This implementation of GPT uses just the secondary GPT table.

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __GPTLIB_H__
#define __GPTLIB_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

/**
  Validate GPT header structure

  @param[in]    Header                  Pointer to GPT header structure

  @retval       EFI_SUCCESS             Header is valid
  @retval       EFI_VOLUME_CORRUPTED    Header is invalid
**/
EFI_STATUS
EFIAPI
GptValidateHeader (
  IN EFI_PARTITION_TABLE_HEADER *Header
  );

/**
  Get the partition table size for all entries in bytes

  @param[in]    Header          Pointer to GPT header structure

  @retval       UINTN           Size of the partition table in bytes
**/
UINTN
EFIAPI
GptPartitionTableSizeInBytes (
  IN CONST EFI_PARTITION_TABLE_HEADER *Header
  );

/**
  Validate the partition table CRC

  @param[in]    Header          Pointer to GPT header structure
  @param[in]    PartitionTable  Pointer to GPT partition table first entry

  @retval       EFI_SUCCESS     Partition table is valid
  @retval       EFI_CRC_ERROR   Partition table has invalid CRC
**/
EFI_STATUS
EFIAPI
GptValidatePartitionTable (
  IN CONST EFI_PARTITION_TABLE_HEADER *Header,
  IN VOID                             *PartitionTable
  );

/**
  Find a partition table entry by its partition name field

  @param[in]    Header          Pointer to GPT header structure
  @param[in]    PartitionTable  Pointer to GPT partition table first entry
  @param[in]    Name            Pointer to the name string to find

  @retval       NULL            Partition not found with that name
  @retval       Other           Pointer to the matching partition table entry
**/
CONST EFI_PARTITION_ENTRY *
EFIAPI
GptFindPartitionByName (
  IN CONST EFI_PARTITION_TABLE_HEADER *Header,
  IN CONST VOID                       *PartitionTable,
  IN CONST CHAR16                     *Name
  );

/**
  Return the size of a partition in blocks

  @param[in]    Partition       Pointer to GPT partition table entry

  @retval       UINT64          Size of the partition in blocks
**/
UINT64
EFIAPI
GptPartitionSizeInBlocks(
  CONST EFI_PARTITION_ENTRY *Partition;
  );

#endif
