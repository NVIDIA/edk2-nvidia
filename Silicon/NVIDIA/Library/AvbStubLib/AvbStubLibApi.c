/** @file
  EDK2 API for LibAvb

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#include <Library/BaseLib.h>
#include <Library/HandleParsingLib.h>

#include <Library/AvbLib.h>

EFI_STATUS
AvbVerifyBoot (
  IN BOOLEAN     IsRecovery,
  IN EFI_HANDLE  ControllerHandle,
  OUT CHAR8      **AvbCmdline
  )
{
  if (AvbCmdline != NULL) {
    *AvbCmdline = NULL;
  }

  return EFI_SUCCESS;
}
