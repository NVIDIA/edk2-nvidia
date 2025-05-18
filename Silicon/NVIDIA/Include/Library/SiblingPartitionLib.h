/** @file

  SPDX-FileCopyrightText: Copyright (c) 2024-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SIBLING_PARTITION_H_
#define __SIBLING_PARTITION_H_

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HandleParsingLib.h>
#include <Library/BootChainInfoLib.h>

#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>

/**
  Locate sibling partition's handle

  @param[in]   Handle                 Partition handle whose sibling is needed
  @param[in]   SiblingPartitionName   Name of sibling partition

**/
EFI_HANDLE
EFIAPI
GetSiblingPartitionHandle (
  IN EFI_HANDLE  ControllerHandle,
  IN CHAR16      *SiblingPartitionName
  );

/**
  Find given partition name

  @param[in]   PrivatePartitionName   Name of partition need to be found
  @param[out]  PartitionName          Name of found partition
  @param[in]   SiblingPartitionMap    Map of sibling partition
  @param[in]   NumberOfEntries        Entries of SiblingPartitionMap

**/
EFI_STATUS
EFIAPI
AndroidBootLocateSiblingPartition (
  IN  CHAR16  *PrivatePartitionName,
  OUT CHAR16  *PartitionName,
  IN  CHAR16  *KernelPartitionToSiblingPartitionMap[][2],
  IN  UINTN   NumberOfEntries
  );

/**
  Find given partition

  @param[in]   Handle                 Partition handle whose sibling is needed
  @param[in]   PartitionName          Name of sibling partition
  @param[out]  Partition              given partition

**/
EFI_STATUS
EFIAPI
AndroidBootReadSiblingPartition (
  IN  EFI_HANDLE  PrivateControllerHandle,
  IN  CHAR16      *PartitionName,
  OUT VOID        **Partition
  );

/**
  Find given name and partition

  @param[in]   PrivatePartitionName   Name of partition need to be found
  @param[in]   Handle                 Partition handle whose sibling is needed
  @param[in]   SiblingPartitionMap    Map of sibling partition
  @param[in]   NumberOfEntries        Entries of SiblingPartitionMap
  @param[out]  Partition              given partition

**/
EFI_STATUS
EFIAPI
AndroidBootLocateAndReadSiblingPartition (
  IN  CHAR16      *PrivatePartitionName,
  IN  EFI_HANDLE  PrivateControllerHandle,
  IN  CHAR16      *KernelPartitionToSiblingPartitionMap[][2],
  IN  UINTN       NumberOfEntries,
  OUT VOID        **Partition
  );

#endif /* __SIBLING_PARTITION_H_ */
