/** @file
  SiblingPartitionLib

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
        if (PartitionInfo->Info.Gpt.StartingLBA > PartitionInfo->Info.Gpt.EndingLBA) {
          goto Exit;
        }

        if (PartitionInfo->Type != PARTITION_TYPE_GPT) {
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
