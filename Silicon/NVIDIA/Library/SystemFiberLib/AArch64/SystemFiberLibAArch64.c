/** @file

  SystemFiberLibPrivate - AArch64 functions for system fiber operations

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SystemFiberLibPrivate.h"
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

/**
  Initialze SystemContext */
EFI_STATUS
EFIAPI
InitializeSystemContext (
  IN OUT EFI_SYSTEM_CONTEXT  *SystemContext,
  SYSTEM_FIBER_ENTRY_POINT   EntryPoint,
  VOID                       *Context,
  UINT8                      *Stack,
  UINTN                      StackSize
  )
{
  EFI_SYSTEM_CONTEXT          CurrentSystemContext;
  EFI_SYSTEM_CONTEXT_AARCH64  CurrentSystemContextAArch64;

  CurrentSystemContext.SystemContextAArch64 = &CurrentSystemContextAArch64;

  ZeroMem (SystemContext->SystemContextAArch64, sizeof (EFI_SYSTEM_CONTEXT_AARCH64));

  GetSystemContext (CurrentSystemContext);
  SystemContext->SystemContextAArch64->ELR  = CurrentSystemContextAArch64.ELR;
  SystemContext->SystemContextAArch64->SPSR = CurrentSystemContextAArch64.SPSR;
  SystemContext->SystemContextAArch64->FPSR = CurrentSystemContextAArch64.FPSR;
  SystemContext->SystemContextAArch64->ESR  = CurrentSystemContextAArch64.ESR;
  SystemContext->SystemContextAArch64->FAR  = CurrentSystemContextAArch64.FAR;

  SystemContext->SystemContextAArch64->LR = (UINT64)EntryPoint;
  SystemContext->SystemContextAArch64->SP = (UINT64)Stack + StackSize;
  SystemContext->SystemContextAArch64->X0 = (UINT64)Context;

  return EFI_SUCCESS;
}
