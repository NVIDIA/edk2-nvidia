/** @file

  UI menu startup lib

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>

EFI_STATUS
EFIAPI
UiMenuStartupLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EfiSignalEventReadyToBoot ();

  return EFI_SUCCESS;
}
