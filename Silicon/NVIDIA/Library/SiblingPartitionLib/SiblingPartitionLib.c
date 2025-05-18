/** @file
  SiblingPartitionLib

  SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/SiblingPartitionLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_HANDLE
EFIAPI
GetSiblingPartitionHandle (
  IN EFI_HANDLE  ControllerHandle,
  IN CHAR16      *SiblingPartitionName
  )
{
  EFI_STATUS                   Status;
  EFI_HANDLE                   *ParentHandles = NULL;
  UINTN                        ParentCount;
  UINTN                        ParentIndex;
  EFI_HANDLE                   *ChildHandles = NULL;
  UINTN                        ChildCount;
  UINTN                        ChildIndex;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo = NULL;
  EFI_HANDLE                   SiblingHandle  = NULL;

  Status = PARSE_HANDLE_DATABASE_PARENTS (ControllerHandle, &ParentCount, &ParentHandles);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find parents - %r\r\n", __FUNCTION__, Status));
    return NULL;
  }

  for (ParentIndex = 0; ParentIndex < ParentCount; ParentIndex++) {
    Status = ParseHandleDatabaseForChildControllers (ParentHandles[ParentIndex], &ChildCount, &ChildHandles);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find child controllers - %r\r\n", __FUNCTION__, Status));
      goto Exit;
    }

    for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex++) {
      Status = gBS->HandleProtocol (ChildHandles[ChildIndex], &gEfiPartitionInfoProtocolGuid, (VOID **)&PartitionInfo);
      if (!EFI_ERROR (Status)) {
        if (PartitionInfo->Type != PARTITION_TYPE_GPT) {
          goto Exit;
        }

        if (PartitionInfo->Info.Gpt.StartingLBA > PartitionInfo->Info.Gpt.EndingLBA) {
          goto Exit;
        }

        if (StrCmp (PartitionInfo->Info.Gpt.PartitionName, SiblingPartitionName) == 0) {
          SiblingHandle = ChildHandles[ChildIndex];
          goto Exit;
        }
      }
    }
  }

Exit:
  if (ParentHandles != NULL) {
    FreePool (ParentHandles);
    ParentHandles = NULL;
  }

  if (ChildHandles != NULL) {
    FreePool (ChildHandles);
    ChildHandles = NULL;
  }

  return SiblingHandle;
}

EFI_STATUS
EFIAPI
AndroidBootLocateSiblingPartition (
  IN  CHAR16  *PrivatePartitionName,
  OUT CHAR16  *PartitionName,
  IN  CHAR16  *KernelPartitionToSiblingPartitionMap[][2],
  IN  UINTN   NumberOfEntries
  )
{
  UINTN  Count;

  for (Count = 0;
       Count < NumberOfEntries;
       Count++)
  {
    if (StrCmp (PrivatePartitionName, KernelPartitionToSiblingPartitionMap[Count][0]) == 0) {
      StrCpyS (PartitionName, MAX_PARTITION_NAME_LEN, KernelPartitionToSiblingPartitionMap[Count][1]);
      return EFI_SUCCESS;
    }
  }

  DEBUG ((DEBUG_ERROR, "%a Partition not found after scanning Count = %u\r\n", __FUNCTION__, Count));
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
AndroidBootReadSiblingPartition (
  IN  EFI_HANDLE  PrivateControllerHandle,
  IN  CHAR16      *PartitionName,
  OUT VOID        **Partition
  )
{
  EFI_STATUS             Status = EFI_SUCCESS;
  EFI_HANDLE             PartitionHandle;
  EFI_BLOCK_IO_PROTOCOL  *BlockIo;
  UINT64                 Size;

  if ((*Partition != NULL) || (PrivateControllerHandle == NULL) || (PartitionName == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameters.\r\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  PartitionHandle = GetSiblingPartitionHandle (
                      PrivateControllerHandle,
                      PartitionName
                      );
  if (PartitionHandle == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to obtain sibling partition for %s\r\n", __FUNCTION__, PartitionName));
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo
                  );
  if (EFI_ERROR (Status) || (BlockIo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate BlockIo protocol\r\n", __FUNCTION__));
    goto Exit;
  }

  Size = MultU64x32 (BlockIo->Media->LastBlock+1, BlockIo->Media->BlockSize);

  *Partition = AllocatePool (Size);
  if (*Partition == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for %a\r\n", __FUNCTION__, PartitionName));
    goto Exit;
  }

  Status = BlockIo->ReadBlocks (
                      BlockIo,
                      BlockIo->Media->MediaId,
                      0,
                      Size,
                      *Partition
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read blocks into memory\r\n", __FUNCTION__));
    goto Exit;
  }

Exit:
  if ((*Partition != NULL) && EFI_ERROR (Status)) {
    FreePool (*Partition);
    *Partition = NULL;
  }

  return Status;
}

EFI_STATUS
EFIAPI
AndroidBootLocateAndReadSiblingPartition (
  IN  CHAR16      *PrivatePartitionName,
  IN  EFI_HANDLE  PrivateControllerHandle,
  IN  CHAR16      *KernelPartitionToSiblingPartitionMap[][2],
  IN  UINTN       NumberOfEntries,
  OUT VOID        **Partition
  )
{
  CHAR16      PartitionName[MAX_PARTITION_NAME_LEN];
  EFI_STATUS  Status;

  Status = AndroidBootLocateSiblingPartition (
             PrivatePartitionName,
             PartitionName,
             KernelPartitionToSiblingPartitionMap,
             NumberOfEntries
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AndroidBootReadSiblingPartition (
             PrivateControllerHandle,
             PartitionName,
             Partition
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}
