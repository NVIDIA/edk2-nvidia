/** @file

  Platform Resource Lib stubs for host based tests

  Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Library/MemoryAllocationLib.h>

#include <HostBasedTestStubLib/PlatformResourceStubLib.h>

typedef struct MOCK_PARTITION_INFO_ENTRY {
  UINTN                               CpuBlAddress;
  UINT32                              PartitionIndex;
  UINT16                              DeviceInstance;
  UINT64                              PartitionStartByte;
  UINT64                              PartitionSizeBytes;
  EFI_STATUS                          ReturnStatus;
  struct MOCK_PARTITION_INFO_ENTRY    *Next;
} MOCK_PARTITION_INFO_ENTRY;

BOOLEAN  mBootChainIsInvalid[2] = { FALSE, FALSE };

MOCK_PARTITION_INFO_ENTRY  *mPartitionInfoList = NULL;

EFI_STATUS
EFIAPI
GetPartitionInfoStMm (
  IN  UINTN   CpuBlAddress,
  IN  UINT32  PartitionIndex,
  OUT UINT16  *DeviceInstance,
  OUT UINT64  *PartitionStartByte,
  OUT UINT64  *PartitionSizeBytes
  )
{
  MOCK_PARTITION_INFO_ENTRY  *PartitionInfo = mPartitionInfoList;

  while (PartitionInfo) {
    if ((PartitionInfo->CpuBlAddress == CpuBlAddress) &&
        (PartitionInfo->PartitionIndex == PartitionIndex))
    {
      if (!EFI_ERROR (PartitionInfo->ReturnStatus)) {
        *DeviceInstance     = PartitionInfo->DeviceInstance;
        *PartitionStartByte = PartitionInfo->PartitionStartByte;
        *PartitionSizeBytes = PartitionInfo->PartitionSizeBytes;
      }

      return PartitionInfo->ReturnStatus;
    }

    PartitionInfo = PartitionInfo->Next;
  }

  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
EFIAPI
MockGetPartitionInfoStMm (
  IN UINTN       CpuBlAddress,
  IN UINT32      PartitionIndex,
  IN UINT16      DeviceInstance,
  IN UINT64      PartitionStartByte,
  IN UINT64      PartitionSizeBytes,
  IN EFI_STATUS  ReturnStatus
  )
{
  MOCK_PARTITION_INFO_ENTRY  *PartitionInfo;

  PartitionInfo = mPartitionInfoList;
  while (PartitionInfo) {
    if ((PartitionInfo->CpuBlAddress == CpuBlAddress) &&
        (PartitionInfo->PartitionIndex == PartitionIndex))
    {
      break;
    }

    PartitionInfo = PartitionInfo->Next;
  }

  if (PartitionInfo == NULL) {
    PartitionInfo = AllocateZeroPool (sizeof (MOCK_PARTITION_INFO_ENTRY));
    if (PartitionInfo == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    PartitionInfo->Next = mPartitionInfoList;
    mPartitionInfoList  = PartitionInfo;

    PartitionInfo->CpuBlAddress   = CpuBlAddress;
    PartitionInfo->PartitionIndex = PartitionIndex;
  }

  PartitionInfo->DeviceInstance     = DeviceInstance;
  PartitionInfo->PartitionStartByte = PartitionStartByte;
  PartitionInfo->PartitionSizeBytes = PartitionSizeBytes;
  PartitionInfo->ReturnStatus       = ReturnStatus;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GetActiveBootChain (
  OUT UINT32  *BootChain
  )
{
  EFI_STATUS  Status;
  UINT32      RequestedBootChain;

  Status             = (EFI_STATUS)mock ();
  RequestedBootChain = (UINT32)mock ();

  if (!EFI_ERROR (Status)) {
    *BootChain = RequestedBootChain;
  }

  return Status;
}

VOID
MockGetActiveBootChain (
  IN  UINT32      ReturnBootChain,
  IN  EFI_STATUS  ReturnStatus
  )
{
  will_return (GetActiveBootChain, ReturnStatus);
  will_return (GetActiveBootChain, ReturnBootChain);
}

EFI_STATUS
EFIAPI
ValidateActiveBootChain (
  VOID
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SetNextBootChain (
  IN  UINT32  BootChain
  )
{
  UINT32      OtherBootChain;
  EFI_STATUS  ReturnStatus;

  ReturnStatus = (EFI_STATUS)mock ();

  if (!EFI_ERROR (ReturnStatus)) {
    OtherBootChain = BootChain ^ 1;

    mBootChainIsInvalid[BootChain]      = FALSE;
    mBootChainIsInvalid[OtherBootChain] = TRUE;
  }

  return ReturnStatus;
}

VOID
MockSetNextBootChain (
  IN  EFI_STATUS  ReturnStatus
  )
{
  will_return (SetNextBootChain, ReturnStatus);
}

VOID
PlatformResourcesStubLibInit (
  VOID
  )
{
}

VOID
PlatformResourcesStubLibDeinit (
  VOID
  )
{
  MOCK_PARTITION_INFO_ENTRY  *PartitionInfo;
  MOCK_PARTITION_INFO_ENTRY  *PartitionInfoNext;

  mBootChainIsInvalid[0] = FALSE;
  mBootChainIsInvalid[1] = FALSE;

  PartitionInfo = mPartitionInfoList;
  while (PartitionInfo) {
    PartitionInfoNext = PartitionInfo->Next;
    FreePool (PartitionInfo);
    PartitionInfo = PartitionInfoNext;
  }

  mPartitionInfoList = NULL;
}
