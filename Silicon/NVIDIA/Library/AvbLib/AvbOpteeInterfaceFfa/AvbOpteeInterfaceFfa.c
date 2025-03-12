/** @file
  EDK2 API for AvbOpteeInterfaceFfa

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#include <Library/NVIDIADebugLib.h>
#include <Library/OpteeNvLib.h>

/**
  Init the optee interface for AVB

  @retval EFI_SUCCESS  Init Optee interface successfully.
**/
EFI_STATUS
AvbOpteeInterfaceInit (
  VOID
  )
{
  // Not implemented til HV FF-A supported
  // Tracked in Bug 5300536
  DEBUG ((DEBUG_ERROR, "%a: Optee FF-A drv not implemented yet, ignoring...\n", __func__));
  return EFI_SUCCESS;
}

/**
  FFA implementation
  Invoke an AVB TA cmd request

  @param[inout] AvbTaArg  OPTEE_INVOKE_FUNCTION_ARG for AVB TA cmd

  @retval EFI_SUCCESS     The operation completed successfully.

**/
EFI_STATUS
AvbOpteeInvoke (
  IN OUT OPTEE_INVOKE_FUNCTION_ARG  *InvokeFunctionArg
  )
{
  // Not implemented til HV FF-A supported
  // Tracked in Bug 5300536
  DEBUG ((DEBUG_ERROR, "%a: Optee FF-A drv not implemented yet, ignoring...\n", __func__));
  return EFI_SUCCESS;
}
