/** @file

  SystemFiberLib - Private header for system fiber operations

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SYSTEM_FIBER_LIB_PRIVATE_H__
#define __SYSTEM_FIBER_LIB_PRIVATE_H__

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/SystemContextLib.h>
#include <Library/SystemFiberLib.h>
#include <Protocol/DebugSupport.h>

#define MIN_STACK_SIZE  SIZE_4KB

// Calculate the maximum size needed
#define MAX_SYSTEM_CONTEXT_SIZE \
  MAX(sizeof(EFI_SYSTEM_CONTEXT_IPF), \
      MAX(sizeof(EFI_SYSTEM_CONTEXT_AARCH64), \
          MAX(sizeof(EFI_SYSTEM_CONTEXT_X64), \
              MAX(sizeof(EFI_SYSTEM_CONTEXT_RISCV64), \
                  MAX(sizeof(EFI_SYSTEM_CONTEXT_LOONGARCH64), \
                      MAX(sizeof(EFI_SYSTEM_CONTEXT_IA32), \
                          MAX(sizeof(EFI_SYSTEM_CONTEXT_ARM), \
                              sizeof(EFI_SYSTEM_CONTEXT_EBC))))))))

typedef struct {
  SYSTEM_FIBER_ENTRY_POINT    EntryPoint;
  VOID                        *Context;
  UINT8                       *Stack;
  UINTN                       StackSize;
  EFI_SYSTEM_CONTEXT          SystemContext;
  EFI_SYSTEM_CONTEXT          ParentSystemContext;
  BOOLEAN                     IsRunning;
  BOOLEAN                     IsDestroyed;
} SYSTEM_FIBER_CONTEXT;

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
  );

#endif
