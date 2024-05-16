/** @file

  FwVariableLibNull - Null Version of the Firmware variable support library

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>

/**
  Delete all Firmware Variables
  Stub version that returns success only.

  @retval         EFI_SUCCESS          All variables deleted

**/
EFI_STATUS
EFIAPI
FwVariableDeleteAll (
  )
{
  return EFI_SUCCESS;
}
