/** @file

  FW Partition Device Library

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/DebugLib.h>
#include <Library/FwPartitionDeviceLib.h>
#include <Library/GptLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Uefi/UefiBaseType.h>

#define FW_PARTITION_PSEUDO_DEVICE_SIGNATURE  SIGNATURE_32 ('F','W','P','P')

typedef struct {
  UINT32                      Signature;

  FW_PARTITION_DEVICE_INFO    DeviceInfo;
  FW_PARTITION_DEVICE_INFO    *MmDeviceInfo;
} FW_PARTITION_PSEUDO_DEVICE_INFO;

CONST CHAR16 **
EFIAPI
FwImageGetList (
  IN  UINTN  ChipId,
  OUT UINTN  *ImageCount
  );

STATIC FW_PARTITION_PRIVATE_DATA  *mPrivate                   = NULL;
STATIC UINTN                      mNumFwPartitions            = 0;
STATIC UINTN                      mMaxFwPartitions            = 0;
STATIC UINT32                     mActiveBootChain            = MAX_UINT32;
STATIC BOOLEAN                    mOverwriteActiveFwPartition = FALSE;
STATIC CONST CHAR16               **mFwImageList              = NULL;
STATIC UINTN                      mFwImageCount               = 0;
STATIC UINTN                      mChipId                     = MAX_UINTN;
STATIC UINT32                     mGptBootChain               = MAX_UINT32;

// non-A/B partition names
STATIC CONST CHAR16  *NonABPartitionNames[] = {
  L"BCT",
  L"BCT-boot-chain_backup",
  L"mb2-applet",
  FW_PARTITION_UPDATE_INACTIVE_PARTITIONS,
  NULL
};

STATIC CONST CHAR16  *mPseudoPartitionNames[] = {
  FW_PARTITION_UPDATE_INACTIVE_PARTITIONS,
  NULL
};

/**
  Check if given Name is in List.

  @param[in]  Name                      Name to look for
  @param[in]  List                      Null-terminated list to search

  @retval BOOLEAN                       TRUE if Name is in List

**/
STATIC
BOOLEAN
EFIAPI
NameIsInList (
  CONST CHAR16  *Name,
  CONST CHAR16  **List
  )
{
  while (*List != NULL) {
    if (StrCmp (Name, *List) == 0) {
      return TRUE;
    }

    List++;
  }

  return FALSE;
}

/**
  Check if partition is in image list.

  @param[in]  PartitionName             Name of partition to check.

  @retval BOOLEAN                       TRUE if partition is in image list.

**/
STATIC
BOOLEAN
EFIAPI
FwPartitionIsInImageList (
  CONST CHAR16  *PartitionName
  )
{
  CHAR16      ImageName[MAX_PARTITION_NAME_LEN];
  UINTN       BootChain;
  EFI_STATUS  Status;

  Status = GetPartitionBaseNameAndBootChainAny (PartitionName, ImageName, &BootChain);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get base name for %s: %r\n", PartitionName, Status));
    return FALSE;
  }

  if (NameIsInList (ImageName, mFwImageList)) {
    return TRUE;
  }

  return NameIsInList (PartitionName, mPseudoPartitionNames);
}

/**
  Check if partition is part of the active FW boot chain

  @param[in]  Name                  Partition name

  @retval BOOLEAN                   TRUE if partition is active

**/
STATIC
BOOLEAN
EFIAPI
FwPartitionIsActive (
  IN  CONST CHAR16  *Name
  )
{
  CHAR16      BaseName[MAX_PARTITION_NAME_LEN];
  UINTN       BootChain;
  EFI_STATUS  Status;

  if (NameIsInList (Name, NonABPartitionNames)) {
    return FALSE;
  }

  Status = GetPartitionBaseNameAndBootChainAny (Name, BaseName, &BootChain);
  if (EFI_ERROR (Status)) {
    return TRUE;
  }

  return (BootChain == mActiveBootChain);
}

// NVIDIA_FW_PARTITION_PROTOCOL.GetAttributes()
EFI_STATUS
EFIAPI
FwPartitionGetAttributes (
  IN  NVIDIA_FW_PARTITION_PROTOCOL  *This,
  OUT FW_PARTITION_ATTRIBUTES       *Attributes
  )
{
  FW_PARTITION_PRIVATE_DATA  *Private;
  FW_PARTITION_INFO          *PartitionInfo;

  if ((This == NULL) || (Attributes == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              FW_PARTITION_PRIVATE_DATA,
              Protocol,
              FW_PARTITION_PRIVATE_DATA_SIGNATURE
              );
  PartitionInfo = &Private->PartitionInfo;

  Attributes->Bytes     = PartitionInfo->Bytes;
  Attributes->BlockSize = Private->DeviceInfo->BlockSize;

  return EFI_SUCCESS;
}

// NVIDIA_FW_PARTITION_PROTOCOL.Read()
EFI_STATUS
EFIAPI
FwPartitionRead (
  IN  NVIDIA_FW_PARTITION_PROTOCOL  *This,
  IN  UINT64                        Offset,
  IN  UINTN                         Bytes,
  OUT VOID                          *Buffer
  )
{
  FW_PARTITION_PRIVATE_DATA  *Private;
  FW_PARTITION_INFO          *PartitionInfo;
  FW_PARTITION_DEVICE_INFO   *DeviceInfo;
  EFI_STATUS                 Status;

  if ((This == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              FW_PARTITION_PRIVATE_DATA,
              Protocol,
              FW_PARTITION_PRIVATE_DATA_SIGNATURE
              );
  PartitionInfo = &Private->PartitionInfo;
  DeviceInfo    = Private->DeviceInfo;

  Status = FwPartitionCheckOffsetAndBytes (PartitionInfo->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s read offset=%llu, bytes=%u error: %r\n",
      __FUNCTION__,
      PartitionInfo->Name,
      Offset,
      Bytes,
      Status
      ));
    return Status;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Starting %s read: Offset=%llu, Bytes=%u, Buffer=0x%p\n",
    __FUNCTION__,
    PartitionInfo->Name,
    Offset,
    Bytes,
    Buffer
    ));

  Status = DeviceInfo->DeviceRead (
                         DeviceInfo,
                         Offset + PartitionInfo->Offset,
                         Bytes,
                         Buffer
                         );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: read of %s, Offset=%llu, Bytes=%u failed: %r\n",
      __FUNCTION__,
      PartitionInfo->Name,
      Offset,
      Bytes,
      Status
      ));
  }

  return Status;
}

// NVIDIA_FW_PARTITION_PROTOCOL.Write()
EFI_STATUS
EFIAPI
FwPartitionWrite (
  IN  NVIDIA_FW_PARTITION_PROTOCOL  *This,
  IN  UINT64                        Offset,
  IN  UINTN                         Bytes,
  IN  CONST VOID                    *Buffer
  )
{
  FW_PARTITION_PRIVATE_DATA  *Private;
  FW_PARTITION_INFO          *PartitionInfo;
  FW_PARTITION_DEVICE_INFO   *DeviceInfo;
  EFI_STATUS                 Status;

  if ((This == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              FW_PARTITION_PRIVATE_DATA,
              Protocol,
              FW_PARTITION_PRIVATE_DATA_SIGNATURE
              );
  PartitionInfo = &Private->PartitionInfo;
  DeviceInfo    = Private->DeviceInfo;

  Status = FwPartitionCheckOffsetAndBytes (PartitionInfo->Bytes, Offset, Bytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s write offset=%llu, bytes=%u error: %r\n",
      __FUNCTION__,
      PartitionInfo->Name,
      Offset,
      Bytes,
      Status
      ));
    return Status;
  }

  if (PartitionInfo->IsActivePartition && !mOverwriteActiveFwPartition) {
    DEBUG ((
      DEBUG_ERROR,
      "Overwriting active %s partition not allowed\n",
      PartitionInfo->Name
      ));
    return EFI_WRITE_PROTECTED;
  } else if (PartitionInfo->IsActivePartition) {
    DEBUG ((
      DEBUG_INFO,
      "Overwriting active %s partition\n",
      PartitionInfo->Name
      ));
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Starting %s write Offset=%llu, Bytes=%u, Buffer=0x%p\n",
    __FUNCTION__,
    PartitionInfo->Name,
    Offset,
    Bytes,
    Buffer
    ));

  Status = DeviceInfo->DeviceWrite (
                         DeviceInfo,
                         Offset + PartitionInfo->Offset,
                         Bytes,
                         Buffer
                         );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: write of %s, Offset=%llu, Bytes=%u failed: %r\n",
      __FUNCTION__,
      PartitionInfo->Name,
      Offset,
      Bytes,
      Status
      ));
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
FwPartitionWriteToUpdateInactivePartitions (
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes,
  IN  CONST VOID                *Buffer
  )
{
  EFI_STATUS                       Status;
  UINTN                            GptTableSize;
  EFI_PARTITION_TABLE_HEADER       *GptHeader;
  EFI_PARTITION_ENTRY              *PartitionTable;
  UINTN                            BlockSize;
  CONST EFI_PARTITION_ENTRY        *Partition;
  UINTN                            Index;
  CHAR16                           BaseName[MAX_PARTITION_NAME_LEN];
  UINTN                            PartitionBootChain;
  UINT32                           InactiveBootChain;
  CONST CHAR16                     *Name;
  FW_PARTITION_PRIVATE_DATA        *Private;
  FW_PARTITION_INFO                *PartitionInfo;
  CONST VOID                       *AlignedBuffer;
  FW_PARTITION_PSEUDO_DEVICE_INFO  *PseudoDeviceInfo;

  if ((DeviceInfo == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  PseudoDeviceInfo = CR (
                       DeviceInfo,
                       FW_PARTITION_PSEUDO_DEVICE_INFO,
                       DeviceInfo,
                       FW_PARTITION_PSEUDO_DEVICE_SIGNATURE
                       );

  AlignedBuffer = AllocateRuntimeCopyPool (Bytes, Buffer);
  NV_ASSERT_RETURN ((AlignedBuffer != NULL), return EFI_OUT_OF_RESOURCES, "%a: alloc failed\n", __FUNCTION__);

  GptTableSize      = NVIDIA_GPT_PARTITION_TABLE_SIZE;
  GptHeader         = (EFI_PARTITION_TABLE_HEADER  *)((UINT8 *)AlignedBuffer + GptTableSize);
  PartitionTable    = (EFI_PARTITION_ENTRY *)AlignedBuffer;
  InactiveBootChain = OTHER_BOOT_CHAIN (mActiveBootChain);
  BlockSize         = NVIDIA_GPT_BLOCK_SIZE;

  DEBUG ((DEBUG_INFO, "%a: Starting update Offset=%llu, Bytes=%u, Buffer=0x%p\n", __FUNCTION__, Offset, Bytes, AlignedBuffer));

  Status = GptValidateHeader (GptHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid GPT header: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = GptValidatePartitionTable (GptHeader, PartitionTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid PartitionTable: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Partition = PartitionTable;
  for (Index = 0; Index < GptHeader->NumberOfPartitionEntries; Index++, Partition++) {
    Name = Partition->PartitionName;

    if (StrLen (Name) == 0) {
      continue;
    }

    Status = GetPartitionBaseNameAndBootChainAny (Name, BaseName, &PartitionBootChain);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get boot chain for %s: %r\n", __FUNCTION__, Name, Status));
      continue;
    }

    if (PartitionBootChain != InactiveBootChain) {
      DEBUG ((DEBUG_INFO, "%a: skipping %s, chain=%u\n", __FUNCTION__, Name, PartitionBootChain));
      continue;
    }

    DEBUG ((DEBUG_INFO, "%a: updating %s\n", __FUNCTION__, Name));

    Private = FwPartitionFindByName (Name);
    if (Private == NULL) {
      DEBUG ((DEBUG_INFO, "%a: Partition %s not found\n", __FUNCTION__, Name));
      continue;
    }

    PartitionInfo = &Private->PartitionInfo;

    DEBUG ((
      DEBUG_INFO,
      "%a: Updating %s Offset 0x%llx->0x%llx Bytes 0x%llx->0x%llx\n",
      __FUNCTION__,
      Name,
      PartitionInfo->Offset,
      Partition->StartingLBA * BlockSize,
      PartitionInfo->Bytes,
      GptPartitionSizeInBlocks (Partition) * BlockSize
      ));

    if (PseudoDeviceInfo->MmDeviceInfo == NULL) {
      PartitionInfo->Offset = Partition->StartingLBA * BlockSize;
    } else {
      DEBUG ((DEBUG_INFO, "%a: no %s offset update for MM\n", __FUNCTION__, Name));
    }

    PartitionInfo->Bytes = GptPartitionSizeInBlocks (Partition) * BlockSize;
  }

  if (PseudoDeviceInfo->MmDeviceInfo != NULL) {
    Status = PseudoDeviceInfo->MmDeviceInfo->DeviceWrite (
                                               PseudoDeviceInfo->MmDeviceInfo,
                                               Offset,
                                               Bytes,
                                               Buffer
                                               );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: write pass-thru failed: %r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: Finished update\n", __FUNCTION__));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FwPartitionAdd (
  IN  CONST CHAR16              *Name,
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    Offset,
  IN  UINTN                     Bytes
  )
{
  FW_PARTITION_PRIVATE_DATA  *Private;
  FW_PARTITION_INFO          *PartitionInfo;

  if (!FwPartitionIsInImageList (Name)) {
    DEBUG ((DEBUG_INFO, "%a: %s not in image list\n", __FUNCTION__, Name));
    return EFI_SUCCESS;
  }

  if (mNumFwPartitions >= mMaxFwPartitions) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Can't add partition %s, reached MaxFwPartitions=%u\n",
      __FUNCTION__,
      Name,
      mMaxFwPartitions
      ));
    return EFI_OUT_OF_RESOURCES;
  }

  if (FwPartitionFindByName (Name) != NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Partition %s already added\n",
      __FUNCTION__,
      Name
      ));
    return EFI_UNSUPPORTED;
  }

  Private       = &mPrivate[mNumFwPartitions];
  PartitionInfo = &Private->PartitionInfo;

  Private->Signature = FW_PARTITION_PRIVATE_DATA_SIGNATURE;

  StrnCpyS (PartitionInfo->Name, FW_PARTITION_NAME_LENGTH, Name, StrLen (Name));
  PartitionInfo->Offset            = Offset;
  PartitionInfo->Bytes             = Bytes;
  PartitionInfo->IsActivePartition = FwPartitionIsActive (Name);

  Private->DeviceInfo = DeviceInfo;

  Private->Protocol.PartitionName = Private->PartitionInfo.Name;
  Private->Protocol.Read          = FwPartitionRead;
  Private->Protocol.Write         = FwPartitionWrite;
  Private->Protocol.GetAttributes = FwPartitionGetAttributes;

  mNumFwPartitions++;

  DEBUG ((
    DEBUG_INFO,
    "Added partition %s, Offset=%llu, Bytes=%u\n",
    PartitionInfo->Name,
    PartitionInfo->Offset,
    PartitionInfo->Bytes
    ));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FwPartitionAddPseudoPartition (
  IN  FW_PARTITION_DEVICE_INFO  *MmDeviceInfo   OPTIONAL
  )
{
  EFI_STATUS                       Status;
  FW_PARTITION_DEVICE_INFO         *DeviceInfo;
  FW_PARTITION_PSEUDO_DEVICE_INFO  *PseudoDeviceInfo;
  CHAR16                           *Name;

  Name             = FW_PARTITION_UPDATE_INACTIVE_PARTITIONS;
  PseudoDeviceInfo = (FW_PARTITION_PSEUDO_DEVICE_INFO *)AllocateRuntimeZeroPool (sizeof (*PseudoDeviceInfo));
  NV_ASSERT_RETURN ((PseudoDeviceInfo != NULL), return EFI_OUT_OF_RESOURCES, "%a: PDI alloc failed\n", __FUNCTION__);

  PseudoDeviceInfo->Signature    = FW_PARTITION_PSEUDO_DEVICE_SIGNATURE;
  PseudoDeviceInfo->MmDeviceInfo = MmDeviceInfo;
  DeviceInfo                     = &PseudoDeviceInfo->DeviceInfo;
  DeviceInfo->DeviceName         = L"pseudo-partition";
  DeviceInfo->DeviceWrite        = FwPartitionWriteToUpdateInactivePartitions;
  DeviceInfo->DeviceRead         = NULL;
  DeviceInfo->BlockSize          = 1;

  Status = FwPartitionAdd (
             Name,
             DeviceInfo,
             0,
             GptGetGptDataSize ()
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: add failed: %r\n", __FUNCTION__, Status));
  }

  return Status;
}

EFI_STATUS
EFIAPI
FwPartitionAddFromDeviceGpt (
  IN  FW_PARTITION_DEVICE_INFO  *DeviceInfo,
  IN  UINT64                    DeviceSizeInBytes
  )
{
  EFI_STATUS                  Status;
  EFI_PARTITION_TABLE_HEADER  *GptHeader;
  EFI_PARTITION_ENTRY         *PartitionTable;
  UINT64                      PartitionTableOffset;
  UINTN                       PartitionCount;
  UINTN                       BlockSize;
  UINTN                       GptHeaderOffset;
  UINTN                       GptDataOffset;
  UINT32                      BootChain;
  CHAR16                      GptPartitionName[MAX_PARTITION_NAME_LEN];

  BlockSize       = NVIDIA_GPT_BLOCK_SIZE;
  PartitionCount  = mNumFwPartitions;
  PartitionTable  = NULL;
  GptHeaderOffset = GptGetHeaderOffset (mGptBootChain, DeviceSizeInBytes, DeviceInfo->BlockSize);

  // read and validate GPT header
  GptHeader = (EFI_PARTITION_TABLE_HEADER *)AllocatePool (BlockSize);
  if (GptHeader == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  DEBUG ((
    DEBUG_INFO,
    "Reading secondary GPT header DeviceSizeInBytes=%llu, BlockSize=%u Offset=%u\n",
    DeviceSizeInBytes,
    BlockSize,
    GptHeaderOffset
    ));

  Status = DeviceInfo->DeviceRead (
                         DeviceInfo,
                         GptHeaderOffset,
                         BlockSize,
                         GptHeader
                         );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Secondary GPT header read failed on %s: %r\n",
      DeviceInfo->DeviceName,
      Status
      ));
    goto Done;
  }

  Status = GptValidateHeader (GptHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Invalid secondary GPT header on %s: %r\n",
      DeviceInfo->DeviceName,
      Status
      ));
    goto Done;
  }

  // read the GPT partition table
  PartitionTable = AllocateZeroPool (GptPartitionTableSizeInBytes (GptHeader));
  if (PartitionTable == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  PartitionTableOffset = GptPartitionTableLba (GptHeader, DeviceSizeInBytes) *
                         BlockSize;

  DEBUG ((
    DEBUG_INFO,
    "Reading partition table on %s, Offset=%llu, entries=%u, size=%u\n",
    DeviceInfo->DeviceName,
    PartitionTableOffset,
    GptHeader->NumberOfPartitionEntries,
    GptPartitionTableSizeInBytes (GptHeader)
    ));

  Status = DeviceInfo->DeviceRead (
                         DeviceInfo,
                         PartitionTableOffset,
                         GptPartitionTableSizeInBytes (GptHeader),
                         PartitionTable
                         );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to read partition table: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  // add all the partitions from the table
  Status = FwPartitionAddFromPartitionTable (
             GptHeader,
             PartitionTable,
             DeviceInfo
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to create partitions from table: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  PartitionCount = mNumFwPartitions - PartitionCount;
  DEBUG ((
    DEBUG_INFO,
    "%a: Found %u FW partitions on %s\n",
    __FUNCTION__,
    PartitionCount,
    DeviceInfo->DeviceName
    ));

  if (PartitionCount == 0) {
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  if (mChipId == T194_CHIP_ID) {
    goto Done;
  }

  // only add GPT update support for boot device
  if (FwPartitionFindByName (L"BCT") == NULL) {
    goto Done;
  }

  // add partitions for GPT updates
  for (BootChain = 0; BootChain < BOOT_CHAIN_COUNT; BootChain++) {
    Status = GetBootChainPartitionNameAny (L"GPT", BootChain, GptPartitionName);
    NV_ASSERT_EFI_ERROR_RETURN (Status, goto Done);

    GptDataOffset = GptGetGptDataOffset (BootChain, DeviceSizeInBytes, DeviceInfo->BlockSize);

    Status = FwPartitionAdd (
               GptPartitionName,
               DeviceInfo,
               GptDataOffset,
               GptGetGptDataSize ()
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error adding %s partition: %r\n", __FUNCTION__, GptPartitionName, Status));
      goto Done;
    }
  }

  // add pseudo-partition to update inactive partition meta-data
  Status = FwPartitionAddPseudoPartition (NULL);

Done:
  if (PartitionTable != NULL) {
    FreePool (PartitionTable);
  }

  if (GptHeader != NULL) {
    FreePool (GptHeader);
  }

  return Status;
}

EFI_STATUS
EFIAPI
FwPartitionAddFromPartitionTable (
  IN  CONST EFI_PARTITION_TABLE_HEADER  *GptHeader,
  IN  EFI_PARTITION_ENTRY               *PartitionTable,
  IN  FW_PARTITION_DEVICE_INFO          *DeviceInfo
  )
{
  UINTN                      BlockSize;
  CONST EFI_PARTITION_ENTRY  *Partition;
  EFI_STATUS                 Status;
  UINTN                      Index;
  CHAR16                     BaseName[MAX_PARTITION_NAME_LEN];
  UINTN                      PartitionBootChain;

  BlockSize = NVIDIA_GPT_BLOCK_SIZE;

  Status = GptValidatePartitionTable (GptHeader, PartitionTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid PartitionTable: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  // initialize a private struct for each partition in the table
  Partition = PartitionTable;
  for (Index = 0; Index < GptHeader->NumberOfPartitionEntries; Index++, Partition++) {
    if (StrLen (Partition->PartitionName) > 0) {
      Status = GetPartitionBaseNameAndBootChainAny (Partition->PartitionName, BaseName, &PartitionBootChain);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to get base name for %s: %r\n", Partition->PartitionName, Status));
        continue;
      }

      Status = FwPartitionAdd (
                 Partition->PartitionName,
                 DeviceInfo,
                 Partition->StartingLBA * BlockSize,
                 GptPartitionSizeInBlocks (Partition) * BlockSize
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Error adding %s partition: %r\n",
          __FUNCTION__,
          Partition->PartitionName,
          Status
          ));
        goto Done;
      }
    }
  }

Done:
  return Status;
}

VOID
EFIAPI
FwPartitionAddressChangeHandler (
  IN  FW_PARTITION_ADDRESS_CONVERT  ConvertFunction
  )
{
  FW_PARTITION_PRIVATE_DATA  *Private;
  UINTN                      Index;

  Private = mPrivate;
  for (Index = 0; Index < mNumFwPartitions; Index++, Private++) {
    ConvertFunction ((VOID **)&Private->DeviceInfo);
    ConvertFunction ((VOID **)&Private->Protocol.PartitionName);
    ConvertFunction ((VOID **)&Private->Protocol.Read);
    ConvertFunction ((VOID **)&Private->Protocol.Write);
    ConvertFunction ((VOID **)&Private->Protocol.GetAttributes);
  }

  ConvertFunction ((VOID **)&mPrivate);
}

EFI_STATUS
EFIAPI
FwPartitionCheckOffsetAndBytes (
  IN  UINT64  MaxOffset,
  IN  UINT64  Offset,
  IN  UINTN   Bytes
  )
{
  // Check offset and bytes separately to avoid overflow
  if ((Offset > MaxOffset) ||
      (Bytes > MaxOffset) ||
      (Offset + Bytes > MaxOffset))
  {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

FW_PARTITION_PRIVATE_DATA *
EFIAPI
FwPartitionFindByName (
  IN  CONST CHAR16  *Name
  )
{
  FW_PARTITION_PRIVATE_DATA  *Private;
  UINTN                      Index;

  Private = mPrivate;
  for (Index = 0; Index < mNumFwPartitions; Index++, Private++) {
    if (StrCmp (Private->PartitionInfo.Name, Name) == 0) {
      return Private;
    }
  }

  return NULL;
}

UINTN
EFIAPI
FwPartitionGetCount (
  VOID
  )
{
  return mNumFwPartitions;
}

FW_PARTITION_PRIVATE_DATA *
EFIAPI
FwPartitionGetPrivateArray (
  VOID
  )
{
  return mPrivate;
}

VOID
EFIAPI
FwPartitionDeviceLibDeinit (
  VOID
  )
{
  if (mPrivate != NULL) {
    FreePool (mPrivate);
    mPrivate = NULL;
  }

  mNumFwPartitions            = 0;
  mMaxFwPartitions            = 0;
  mActiveBootChain            = MAX_UINT32;
  mOverwriteActiveFwPartition = FALSE;
}

EFI_STATUS
EFIAPI
FwPartitionDeviceLibInit (
  IN  UINT32   ActiveBootChain,
  IN  UINTN    MaxFwPartitions,
  IN  BOOLEAN  OverwriteActiveFwPartition,
  IN  UINTN    ChipId,
  IN  UINT32   GptBootChain
  )
{
  mActiveBootChain            = ActiveBootChain;
  mMaxFwPartitions            = MaxFwPartitions;
  mOverwriteActiveFwPartition = OverwriteActiveFwPartition;
  mChipId                     = ChipId;
  mGptBootChain               = GptBootChain;

  mPrivate = (FW_PARTITION_PRIVATE_DATA *)AllocateRuntimeZeroPool (
                                            mMaxFwPartitions * sizeof (FW_PARTITION_PRIVATE_DATA)
                                            );
  if (mPrivate == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: mPrivate allocation failed, MaxFwPartitions=%u\n",
      __FUNCTION__,
      MaxFwPartitions
      ));
    return EFI_OUT_OF_RESOURCES;
  }

  mFwImageList = FwImageGetList (ChipId, &mFwImageCount);

  return EFI_SUCCESS;
}
