/** @file
  Host-based tests for GptLib.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/GoogleTestLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include <Library/GptLib.h>
}

using namespace testing;

#define TEST_CHAR16(x)  (reinterpret_cast<CONST CHAR16 *>(u##x))

namespace {
constexpr UINT32  kDefaultEntryCount = 4;
constexpr UINT64  kDevice64Mb        = 64ULL * 1024ULL * 1024ULL;
constexpr UINT64  kDevice32Mb        = 32ULL * 1024ULL * 1024ULL;

VOID
UpdateHeaderCrc (
  EFI_PARTITION_TABLE_HEADER  *Header
  )
{
  Header->Header.CRC32 = 0;
  Header->Header.CRC32 = CalculateCrc32 ((UINT8 *)Header, Header->Header.HeaderSize);
}

EFI_PARTITION_TABLE_HEADER
MakeHeader (
  UINT32  EntryCount = kDefaultEntryCount,
  UINT32  EntrySize  = sizeof (EFI_PARTITION_ENTRY)
  )
{
  EFI_PARTITION_TABLE_HEADER  Header;

  ZeroMem (&Header, sizeof (Header));
  Header.Header.Signature         = EFI_PTAB_HEADER_ID;
  Header.Header.HeaderSize        = sizeof (EFI_PARTITION_TABLE_HEADER);
  Header.MyLBA                    = 1;
  Header.AlternateLBA             = 0x1FFFF;
  Header.FirstUsableLBA           = 34;
  Header.LastUsableLBA            = 1000;
  Header.PartitionEntryLBA        = 2;
  Header.NumberOfPartitionEntries = EntryCount;
  Header.SizeOfPartitionEntry     = EntrySize;
  UpdateHeaderCrc (&Header);
  return Header;
}

VOID
SetPartitionName (
  EFI_PARTITION_ENTRY  *Entry,
  CONST CHAR16         *Name
  )
{
  UINTN  Index;

  ZeroMem (Entry->PartitionName, sizeof (Entry->PartitionName));
  for (Index = 0;
       (Index < (sizeof (Entry->PartitionName) / sizeof (Entry->PartitionName[0])) - 1) &&
       (Name[Index] != L'\0');
       Index++)
  {
    Entry->PartitionName[Index] = Name[Index];
  }
}

EFI_PARTITION_ENTRY *
PartitionAt (
  VOID   *PartitionTable,
  UINTN  EntrySize,
  UINTN  Index
  )
{
  return (EFI_PARTITION_ENTRY *)((UINT8 *)PartitionTable + (EntrySize * Index));
}

VOID
AddPartition (
  EFI_PARTITION_TABLE_HEADER  *Header,
  VOID                        *PartitionTable,
  UINTN                       Index,
  CONST CHAR16                *Name,
  EFI_LBA                     StartingLba,
  EFI_LBA                     EndingLba
  )
{
  EFI_PARTITION_ENTRY  *Partition;

  Partition = PartitionAt (PartitionTable, Header->SizeOfPartitionEntry, Index);
  SetPartitionName (Partition, Name);
  Partition->StartingLBA = StartingLba;
  Partition->EndingLBA   = EndingLba;
}

VOID
FinalizePartitionTableCrc (
  EFI_PARTITION_TABLE_HEADER  *Header,
  VOID                        *PartitionTable
  )
{
  Header->PartitionEntryArrayCRC32 = CalculateCrc32 (
                                       PartitionTable,
                                       GptPartitionTableSizeInBytes (Header)
                                       );
  UpdateHeaderCrc (Header);
}
} // namespace

TEST (GptLibTest, ValidateHeaderAcceptsValidHeader) {
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();

  EXPECT_EQ (EFI_SUCCESS, GptValidateHeader (&Header));
}

TEST (GptLibTest, ValidateHeaderRejectsInvalidHeaderSize) {
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();

  Header.Header.HeaderSize = sizeof (EFI_PARTITION_TABLE_HEADER) - 1;
  UpdateHeaderCrc (&Header);

  EXPECT_EQ (EFI_VOLUME_CORRUPTED, GptValidateHeader (&Header));
}

TEST (GptLibTest, ValidateHeaderRejectsCrcMismatch) {
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();

  Header.Header.CRC32 ^= 1;

  EXPECT_EQ (EFI_VOLUME_CORRUPTED, GptValidateHeader (&Header));
}

TEST (GptLibTest, ValidateHeaderRejectsInvalidSignature) {
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();

  Header.Header.Signature = 0;
  UpdateHeaderCrc (&Header);

  EXPECT_EQ (EFI_VOLUME_CORRUPTED, GptValidateHeader (&Header));
}

TEST (GptLibTest, ValidateHeaderRejectsSmallPartitionEntry) {
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader (kDefaultEntryCount, sizeof (EFI_PARTITION_ENTRY) - 1);

  EXPECT_EQ (EFI_VOLUME_CORRUPTED, GptValidateHeader (&Header));
}

TEST (GptLibTest, ValidateHeaderRejectsNonPowerOfTwoPartitionEntry) {
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader (kDefaultEntryCount, 192);

  EXPECT_EQ (EFI_VOLUME_CORRUPTED, GptValidateHeader (&Header));
}

TEST (GptLibTest, ValidateHeaderAcceptsLargerPowerOfTwoPartitionEntry) {
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader (kDefaultEntryCount, 256);

  EXPECT_EQ (EFI_SUCCESS, GptValidateHeader (&Header));
}

TEST (GptLibTest, PartitionTableSizeUsesHeaderStride) {
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader (3, 256);

  EXPECT_EQ (768U, GptPartitionTableSizeInBytes (&Header));
}

TEST (GptLibTest, ValidatePartitionTableRejectsCrcMismatch) {
  UINT8                       PartitionTable[sizeof (EFI_PARTITION_ENTRY) * kDefaultEntryCount];
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();

  ZeroMem (PartitionTable, sizeof (PartitionTable));
  FinalizePartitionTableCrc (&Header, PartitionTable);
  Header.PartitionEntryArrayCRC32 ^= 1;

  EXPECT_EQ (EFI_CRC_ERROR, GptValidatePartitionTable (&Header, PartitionTable));
}

TEST (GptLibTest, ValidatePartitionTableSkipsEmptyEntries) {
  UINT8                       PartitionTable[sizeof (EFI_PARTITION_ENTRY) * kDefaultEntryCount];
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();
  EFI_PARTITION_ENTRY         *UnusedPartition;

  ZeroMem (PartitionTable, sizeof (PartitionTable));
  UnusedPartition              = PartitionAt (PartitionTable, Header.SizeOfPartitionEntry, 1);
  UnusedPartition->StartingLBA = 1;
  UnusedPartition->EndingLBA   = 2;
  FinalizePartitionTableCrc (&Header, PartitionTable);

  EXPECT_EQ (EFI_SUCCESS, GptValidatePartitionTable (&Header, PartitionTable));
}

TEST (GptLibTest, ValidatePartitionTableRejectsOutOfRangeLba) {
  UINT8                       PartitionTable[sizeof (EFI_PARTITION_ENTRY) * kDefaultEntryCount];
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();

  ZeroMem (PartitionTable, sizeof (PartitionTable));
  AddPartition (&Header, PartitionTable, 0, TEST_CHAR16 ("bad"), Header.FirstUsableLBA - 1, Header.FirstUsableLBA);
  FinalizePartitionTableCrc (&Header, PartitionTable);

  EXPECT_EQ (EFI_VOLUME_CORRUPTED, GptValidatePartitionTable (&Header, PartitionTable));
}

TEST (GptLibTest, ValidatePartitionTableRejectsEndingBeforeStarting) {
  UINT8                       PartitionTable[sizeof (EFI_PARTITION_ENTRY) * kDefaultEntryCount];
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();

  ZeroMem (PartitionTable, sizeof (PartitionTable));
  AddPartition (&Header, PartitionTable, 0, TEST_CHAR16 ("bad"), 100, 99);
  FinalizePartitionTableCrc (&Header, PartitionTable);

  EXPECT_EQ (EFI_VOLUME_CORRUPTED, GptValidatePartitionTable (&Header, PartitionTable));
}

TEST (GptLibTest, ValidatePartitionTableUsesHeaderStride) {
  UINT8                       PartitionTable[256 * 2];
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader (2, 256);

  ZeroMem (PartitionTable, sizeof (PartitionTable));
  AddPartition (&Header, PartitionTable, 0, TEST_CHAR16 ("first"), 100, 101);
  AddPartition (&Header, PartitionTable, 1, TEST_CHAR16 ("second"), Header.LastUsableLBA + 1, Header.LastUsableLBA + 2);
  FinalizePartitionTableCrc (&Header, PartitionTable);

  EXPECT_EQ (EFI_VOLUME_CORRUPTED, GptValidatePartitionTable (&Header, PartitionTable));
}

TEST (GptLibTest, ValidatePartitionTableHandlesFullPartitionNameWithoutOverread) {
  UINT8                       PartitionTable[sizeof (EFI_PARTITION_ENTRY) * kDefaultEntryCount];
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();
  EFI_PARTITION_ENTRY         *Partition;
  UINTN                       Index;

  ZeroMem (PartitionTable, sizeof (PartitionTable));
  Partition = PartitionAt (PartitionTable, Header.SizeOfPartitionEntry, 0);
  for (Index = 0; Index < sizeof (Partition->PartitionName) / sizeof (Partition->PartitionName[0]); Index++) {
    Partition->PartitionName[Index] = L'A';
  }

  Partition->StartingLBA = 100;
  Partition->EndingLBA   = 101;
  FinalizePartitionTableCrc (&Header, PartitionTable);

  EXPECT_EQ (EFI_SUCCESS, GptValidatePartitionTable (&Header, PartitionTable));
}

TEST (GptLibTest, FindPartitionByNameFindsFirstAndLastEntries) {
  UINT8                       PartitionTable[256 * 3];
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader (3, 256);

  ZeroMem (PartitionTable, sizeof (PartitionTable));
  AddPartition (&Header, PartitionTable, 0, TEST_CHAR16 ("first"), 100, 110);
  AddPartition (&Header, PartitionTable, 2, TEST_CHAR16 ("last"), 300, 310);

  EXPECT_EQ (PartitionAt (PartitionTable, Header.SizeOfPartitionEntry, 0), GptFindPartitionByName (&Header, PartitionTable, TEST_CHAR16 ("first")));
  EXPECT_EQ (PartitionAt (PartitionTable, Header.SizeOfPartitionEntry, 2), GptFindPartitionByName (&Header, PartitionTable, TEST_CHAR16 ("last")));
}

TEST (GptLibTest, FindPartitionByNameReturnsNullWhenAbsent) {
  UINT8                       PartitionTable[sizeof (EFI_PARTITION_ENTRY) * kDefaultEntryCount];
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();

  ZeroMem (PartitionTable, sizeof (PartitionTable));
  AddPartition (&Header, PartitionTable, 0, TEST_CHAR16 ("present"), 100, 110);

  EXPECT_EQ (nullptr, GptFindPartitionByName (&Header, PartitionTable, TEST_CHAR16 ("absent")));
}

TEST (GptLibTest, PartitionSizeInBlocksCoversNormalSingleAndInvalidRanges) {
  EFI_PARTITION_ENTRY  Partition;

  ZeroMem (&Partition, sizeof (Partition));
  Partition.StartingLBA = 100;
  Partition.EndingLBA   = 199;
  EXPECT_EQ (100U, GptPartitionSizeInBlocks (&Partition));

  Partition.StartingLBA = 77;
  Partition.EndingLBA   = 77;
  EXPECT_EQ (1U, GptPartitionSizeInBlocks (&Partition));

  Partition.StartingLBA = 78;
  Partition.EndingLBA   = 77;
  EXPECT_EQ (0U, GptPartitionSizeInBlocks (&Partition));
}

TEST (GptLibTest, PartitionTableLbaHandlesNormalWrapAndBoundaryValues) {
  EFI_PARTITION_TABLE_HEADER  Header = MakeHeader ();

  Header.PartitionEntryLBA = 10;
  EXPECT_EQ (10U, GptPartitionTableLba (&Header, kDevice64Mb));

  Header.PartitionEntryLBA = (kDevice64Mb / NVIDIA_GPT_BLOCK_SIZE) + 7;
  EXPECT_EQ (7U, GptPartitionTableLba (&Header, kDevice64Mb));

  Header.PartitionEntryLBA = kDevice64Mb / NVIDIA_GPT_BLOCK_SIZE;
  EXPECT_EQ (kDevice64Mb / NVIDIA_GPT_BLOCK_SIZE, GptPartitionTableLba (&Header, kDevice64Mb));
}

TEST (GptLibTest, HeaderOffsetMatchesBootChainLayout) {
  EXPECT_EQ (0x3FFFE00U, GptGetHeaderOffset (0, kDevice64Mb, 512));
  EXPECT_EQ (0x3FC4000U, GptGetHeaderOffset (1, kDevice64Mb, 512));
  EXPECT_EQ (0x3F84000U, GptGetHeaderOffset (1, kDevice64Mb, SIZE_128KB));
  EXPECT_EQ (0x1FFFE00U, GptGetHeaderOffset (0, kDevice32Mb, 512));
}

TEST (GptLibTest, GptDataOffsetAndSizeMatchHeaderLayout) {
  EXPECT_EQ (GptGetHeaderOffset (0, kDevice64Mb, 512) - NVIDIA_GPT_PARTITION_TABLE_SIZE, GptGetGptDataOffset (0, kDevice64Mb, 512));
  EXPECT_EQ (GptGetHeaderOffset (1, kDevice64Mb, 512) - NVIDIA_GPT_PARTITION_TABLE_SIZE, GptGetGptDataOffset (1, kDevice64Mb, 512));
  EXPECT_EQ (NVIDIA_GPT_PARTITION_TABLE_SIZE + NVIDIA_GPT_BLOCK_SIZE, GptGetGptDataSize ());
}

TEST (GptLibTest, HeaderOffsetRejectsInvalidBootChain) {
  EXPECT_ANY_THROW (GptGetHeaderOffset (2, kDevice64Mb, 512));
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
