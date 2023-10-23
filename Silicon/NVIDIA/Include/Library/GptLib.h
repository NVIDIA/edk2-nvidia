/** @file

  GPT - GUID Partition Table Library Public Interface
        This implementation of GPT uses just the secondary GPT table.

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __GPTLIB_H__
#define __GPTLIB_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define NVIDIA_GPT_BLOCK_SIZE   512
#define NVIDIA_GPT_MAX_ENTRIES  128
#define NVIDIA_GPT_ALIGN_MIN    SIZE_64KB

#define NVIDIA_GPT_PARTITION_TABLE_SIZE  (NVIDIA_GPT_MAX_ENTRIES * sizeof (EFI_PARTITION_ENTRY))

/**
  Validate GPT header structure

  @param[in]    Header                  Pointer to GPT header structure

  @retval       EFI_SUCCESS             Header is valid
  @retval       EFI_VOLUME_CORRUPTED    Header is invalid
**/
EFI_STATUS
EFIAPI
GptValidateHeader (
  IN EFI_PARTITION_TABLE_HEADER  *Header
  );

/**
  Get the partition table starting LBA

  @param[in]    Header          Pointer to GPT header structure
  @param[in]    DeviceBytes     Size of the device in bytes

  @retval       EFI_LBA         LBA of the partition table
**/
EFI_LBA
EFIAPI
GptPartitionTableLba (
  IN EFI_PARTITION_TABLE_HEADER  *Header,
  IN UINT64                      DeviceBytes
  );

/**
  Get the partition table size for all entries in bytes

  @param[in]    Header          Pointer to GPT header structure

  @retval       UINTN           Size of the partition table in bytes
**/
UINTN
EFIAPI
GptPartitionTableSizeInBytes (
  IN CONST EFI_PARTITION_TABLE_HEADER  *Header
  );

/**
  Validate the partition table CRC

  @param[in]    Header                  Pointer to GPT header structure
  @param[in]    PartitionTable          Pointer to GPT partition table first entry

  @retval       EFI_SUCCESS             Partition table is valid
  @retval       EFI_CRC_ERROR           Partition table has invalid CRC
  @retval       EFI_VOLUME_CORRUPTED    Partition table entry had invalid LBA range
**/
EFI_STATUS
EFIAPI
GptValidatePartitionTable (
  IN CONST EFI_PARTITION_TABLE_HEADER  *Header,
  IN VOID                              *PartitionTable
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
  IN CONST EFI_PARTITION_TABLE_HEADER  *Header,
  IN CONST VOID                        *PartitionTable,
  IN CONST CHAR16                      *Name
  );

/**
  Return the size of a partition in blocks

  @param[in]    Partition       Pointer to GPT partition table entry

  @retval       UINT64          Size of the partition in blocks
**/
UINT64
EFIAPI
GptPartitionSizeInBlocks (
  CONST EFI_PARTITION_ENTRY  *Partition
  );

/**
  Return the offset of the GPT header in bytes.

  @param[in]    BootChain       Active boot chain (0=A, 1=B).
  @param[in]    DeviceSize      Size of flash device in bytes.
  @param[in]    DeviceBlockSize Block size of flash device in bytes.

  @retval       UINTN           Offset of the GPT in bytes.
**/
UINTN
EFIAPI
GptGetHeaderOffset (
  UINT32  BootChain,
  UINTN   DeviceSize,
  UINT32  DeviceBlockSize
  );

/**
  Return the offset of the GPT data in bytes.

  @param[in]    BootChain       Active boot chain (0=A, 1=B).
  @param[in]    DeviceSize      Size of flash device in bytes.
  @param[in]    DeviceBlockSize Block size of flash device in bytes.

  @retval       UINTN           Offset of the GPT data in bytes.

**/
UINTN
EFIAPI
GptGetGptDataOffset (
  UINT32  BootChain,
  UINTN   DeviceSize,
  UINT32  DeviceBlockSize
  );

/**
  Return the size of the GPT data in bytes.

  @retval       UINTN           Size of the GPT data in bytes.

**/
UINTN
EFIAPI
GptGetGptDataSize (
  VOID
  );

#endif
