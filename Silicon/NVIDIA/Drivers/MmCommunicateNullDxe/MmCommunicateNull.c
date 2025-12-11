/** @file
  MM Communicate driver for non-MM platforms.

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiDxe.h>
#include <Library/DebugLib.h>

/**
  The Entry Point for MM Communication

  @param  ImageHandle    The firmware allocated handle for the EFI image.
  @param  SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS    The entry point is executed successfully.
  @retval Other          Some error occurred when executing this entry point.

**/
EFI_STATUS
EFIAPI
MmCommunicateNullInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "%a: return unsupported\n", __FUNCTION__));

  // return error so gNVIDIAStandaloneMmUnavailableGuid is installed by lib deconstructor
  return EFI_UNSUPPORTED;
}
