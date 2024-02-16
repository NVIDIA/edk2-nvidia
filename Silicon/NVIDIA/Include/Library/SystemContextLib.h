/** @file

  SystemContextLib - Library for system context operations

  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SYSTEM_CONTEXT_LIB_H__
#define SYSTEM_CONTEXT_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/DebugSupport.h>

/**
  Gets the current system context

  @param[in,out] SystemContext  SystemContext structure to fill out
**/
VOID
EFIAPI
GetSystemContext (
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  );

/**
  Swaps the system context

  In addition to the system registers this changes the LR so it returns to the
  previous context caller, however the other context should swap back so this
  will overall function like a regular function call.

  @param[in,out] CurrentSystemContext SystemContext structure to update with
                                      current context
  @param[in]     NewSystemContext     SystemContext to be restored, everything
                                      will be restored except for PC

**/
EFI_STATUS
EFIAPI
SwapSystemContext (
  IN OUT EFI_SYSTEM_CONTEXT  CurrentSystemContext,
  IN     EFI_SYSTEM_CONTEXT  NewSystemContext
  );

#endif
