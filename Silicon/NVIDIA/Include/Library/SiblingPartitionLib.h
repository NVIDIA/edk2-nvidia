/** @file

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SIBLING_PARTITION_H_
#define __SIBLING_PARTITION_H_

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HandleParsingLib.h>

#include <Protocol/PartitionInfo.h>

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

#endif /* __SIBLING_PARTITION_H_ */
