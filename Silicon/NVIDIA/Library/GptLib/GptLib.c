/** @file

  GPT - GUID Partition Table Library
        This implementation of GPT uses just the secondary GPT table.

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/GptLib.h>
#include <Library/BaseLib.h>
#include <Library/NVIDIADebugLib.h>

EFI_STATUS
EFIAPI
GptValidateHeader (
  IN EFI_PARTITION_TABLE_HEADER  *Header
  )
{
  UINT32  OriginalCrc;
  UINT32  Crc;

  if (Header->Header.HeaderSize != sizeof (EFI_PARTITION_TABLE_HEADER)) {
    return EFI_VOLUME_CORRUPTED;
  }

  // save off original crc and set to 0 for calculation, then put it back
  OriginalCrc          = Header->Header.CRC32;
  Header->Header.CRC32 = 0;
  Crc                  = CalculateCrc32 ((UINT8 *)Header, Header->Header.HeaderSize);
  Header->Header.CRC32 = OriginalCrc;

  // perform validation checks
  if ((Header->Header.Signature != EFI_PTAB_HEADER_ID) ||
      (OriginalCrc != Crc) ||
      (Header->SizeOfPartitionEntry < sizeof (EFI_PARTITION_ENTRY)) ||
      // Ensure NumberOfPartitionEntries*SizeOfPartitionEntry doesn't overflow.
      (Header->NumberOfPartitionEntries >
       DivU64x32 (MAX_UINTN, Header->SizeOfPartitionEntry))
      )
  {
    return EFI_VOLUME_CORRUPTED;
  }

  return EFI_SUCCESS;
}

EFI_LBA
EFIAPI
GptPartitionTableLba (
  IN EFI_PARTITION_TABLE_HEADER  *Header,
  IN UINT64                      DeviceBytes
  )
{
  UINT64   DeviceSizeInBlocks;
  EFI_LBA  PartitionTableLba;

  DeviceSizeInBlocks = DeviceBytes / NVIDIA_GPT_BLOCK_SIZE;
  PartitionTableLba  = Header->PartitionEntryLBA;

  // secondary GPT on boot flash has PartitionEntryLBA value beyond end of device
  if (PartitionTableLba > DeviceSizeInBlocks) {
    PartitionTableLba -= DeviceSizeInBlocks;
  }

  return PartitionTableLba;
}

UINTN
EFIAPI
GptPartitionTableSizeInBytes (
  IN CONST EFI_PARTITION_TABLE_HEADER  *Header
  )
{
  return Header->NumberOfPartitionEntries * Header->SizeOfPartitionEntry;
}

EFI_STATUS
EFIAPI
GptValidatePartitionTable (
  IN CONST EFI_PARTITION_TABLE_HEADER  *Header,
  IN VOID                              *PartitionTable
  )
{
  UINT32                     Crc;
  UINTN                      Index;
  CONST EFI_PARTITION_ENTRY  *Partition;
  EFI_LBA                    FirstBlock;
  EFI_LBA                    LastBlock;

  Crc = CalculateCrc32 (
          PartitionTable,
          GptPartitionTableSizeInBytes (Header)
          );
  if (Header->PartitionEntryArrayCRC32 != Crc) {
    return EFI_CRC_ERROR;
  }

  FirstBlock = Header->FirstUsableLBA;
  LastBlock  = Header->LastUsableLBA;
  Partition  = (CONST EFI_PARTITION_ENTRY *)PartitionTable;
  for (Index = 0; Index < Header->NumberOfPartitionEntries; Index++, Partition++) {
    // skip unused partitions
    if (StrLen (Partition->PartitionName) == 0) {
      continue;
    }

    if ((Partition->StartingLBA > LastBlock) ||
        (Partition->EndingLBA > LastBlock) ||
        (Partition->StartingLBA < FirstBlock) ||
        (Partition->EndingLBA < FirstBlock))
    {
      return EFI_VOLUME_CORRUPTED;
    }
  }

  return EFI_SUCCESS;
}

CONST EFI_PARTITION_ENTRY *
EFIAPI
GptFindPartitionByName (
  IN CONST EFI_PARTITION_TABLE_HEADER  *Header,
  IN CONST VOID                        *PartitionTable,
  IN CONST CHAR16                      *Name
  )
{
  CONST EFI_PARTITION_ENTRY  *Partition;
  UINTN                      Index;

  for (Index = 0; Index < Header->NumberOfPartitionEntries; Index++) {
    Partition = (CONST EFI_PARTITION_ENTRY *)((CONST UINT8 *)PartitionTable +
                                              (Index * Header->SizeOfPartitionEntry));
    if (StrnCmp (
          Partition->PartitionName,
          Name,
          sizeof (Partition->PartitionName)/sizeof (CHAR16)
          ) == 0)
    {
      return Partition;
    }
  }

  return NULL;
}

UINT64
EFIAPI
GptPartitionSizeInBlocks (
  CONST EFI_PARTITION_ENTRY  *Partition
  )
{
  return Partition->EndingLBA - Partition->StartingLBA + 1;
}

UINTN
EFIAPI
GptGetHeaderOffset (
  UINT32  BootChain,
  UINTN   DeviceSize,
  UINT32  DeviceBlockSize
  )
{
  UINTN   GptHeaderOffset;
  UINTN   SecondaryGptStart;
  UINTN   GptTableSize;
  UINT32  PartitionAlign;

  PartitionAlign = MAX (DeviceBlockSize, NVIDIA_GPT_ALIGN_MIN);

  switch (BootChain) {
    case 0:
      GptHeaderOffset = DeviceSize - NVIDIA_GPT_BLOCK_SIZE;
      break;
    case 1:
      SecondaryGptStart = DeviceSize - (4 * PartitionAlign);
      GptTableSize      = NVIDIA_GPT_PARTITION_TABLE_SIZE;
      GptHeaderOffset   = SecondaryGptStart + GptTableSize;
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a: Invalid BootChain=%u\n", __FUNCTION__, BootChain));
      ASSERT (FALSE);
      GptHeaderOffset = 0;
      break;
  }

  DEBUG ((DEBUG_INFO, "%a: 0x%x/0x%x/0x%x %u Offset=0x%x\n", __FUNCTION__, DeviceSize, PartitionAlign, DeviceBlockSize, BootChain, GptHeaderOffset));

  return GptHeaderOffset;
}

UINTN
EFIAPI
GptGetGptDataOffset (
  UINT32  BootChain,
  UINTN   DeviceSize,
  UINT32  DeviceBlockSize
  )
{
  UINTN  GptHeaderOffset;
  UINTN  GptDataOffset;

  GptHeaderOffset = GptGetHeaderOffset (BootChain, DeviceSize, DeviceBlockSize);
  GptDataOffset   = GptHeaderOffset - NVIDIA_GPT_PARTITION_TABLE_SIZE;

  DEBUG ((DEBUG_INFO, "%a: %u hdr=0x%x data=0x%x\n", __FUNCTION__, BootChain, GptHeaderOffset, GptDataOffset));

  return GptDataOffset;
}

UINTN
EFIAPI
GptGetGptDataSize (
  VOID
  )
{
  return NVIDIA_GPT_PARTITION_TABLE_SIZE + NVIDIA_GPT_BLOCK_SIZE;
}
