/** @file

  GPT - GUID Partition Table Library
        This implementation of GPT uses just the secondary GPT table.

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/GptLib.h>
#include <Library/BaseLib.h>

EFI_STATUS
EFIAPI
GptValidateHeader (
  IN EFI_PARTITION_TABLE_HEADER *Header
  )
{
  UINT32                        OriginalCrc;
  UINT32                        Crc;

  // save off original crc and set to 0 for calculation, then put it back
  OriginalCrc = Header->Header.CRC32;
  Header->Header.CRC32 = 0;
  Crc = CalculateCrc32 ((UINT8 *) Header, Header->Header.HeaderSize);
  Header->Header.CRC32 = OriginalCrc;

  // perform validation checks
  if ((Header->Header.Signature != EFI_PTAB_HEADER_ID) ||
      (OriginalCrc != Crc) ||
      (Header->SizeOfPartitionEntry < sizeof (EFI_PARTITION_ENTRY)) ||
      // Ensure NumberOfPartitionEntries*SizeOfPartitionEntry doesn't overflow.
      (Header->NumberOfPartitionEntries >
        DivU64x32 (MAX_UINTN, Header->SizeOfPartitionEntry))
      ) {
    return EFI_VOLUME_CORRUPTED;
  }

  return EFI_SUCCESS;
}

UINTN
EFIAPI
GptPartitionTableSizeInBytes (
  IN CONST EFI_PARTITION_TABLE_HEADER *Header
  )
{
  return Header->NumberOfPartitionEntries * Header->SizeOfPartitionEntry;
}

EFI_STATUS
EFIAPI
GptValidatePartitionTable (
  IN CONST EFI_PARTITION_TABLE_HEADER *Header,
  IN VOID                             *PartitionTable
  )
{
  UINT32        Crc;

  Crc = CalculateCrc32 (PartitionTable,
                        GptPartitionTableSizeInBytes (Header));
  if (Header->PartitionEntryArrayCRC32 != Crc) {
    return EFI_CRC_ERROR;
  }

  return EFI_SUCCESS;
}

CONST EFI_PARTITION_ENTRY *
EFIAPI
GptFindPartitionByName (
  IN CONST EFI_PARTITION_TABLE_HEADER *Header,
  IN CONST VOID                       *PartitionTable,
  IN CONST CHAR16                     *Name
  )
{
  CONST EFI_PARTITION_ENTRY  *Partition;
  UINTN                      Index;

  for (Index = 0; Index < Header->NumberOfPartitionEntries; Index++) {
    Partition = (CONST EFI_PARTITION_ENTRY *) ((CONST UINT8 *) PartitionTable +
                  (Index * Header->SizeOfPartitionEntry));
    if (StrnCmp (Partition->PartitionName,
                 Name,
                 sizeof (Partition->PartitionName)/sizeof (CHAR16)) == 0) {
      return Partition;
    }
  }

  return NULL;
}

UINT64
EFIAPI
GptPartitionSizeInBlocks(
  CONST EFI_PARTITION_ENTRY *Partition
  )
{
  return Partition->EndingLBA - Partition->StartingLBA + 1;
}
