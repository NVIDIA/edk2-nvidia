/** @file

  SystemFiberLib - Library for system fiber operations

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SystemFiberLibPrivate.h"

#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

/**
  Free internal memory resources

  @param[in] FiberContext  Fiber context
 */
STATIC
VOID
EFIAPI
FreeInternalResources (
  IN SYSTEM_FIBER_CONTEXT  *FiberContext
  )
{
  if (FiberContext->Stack != NULL) {
    FreePages (FiberContext->Stack, EFI_SIZE_TO_PAGES (FiberContext->StackSize));
    FiberContext->Stack = NULL;
  }

  if (FiberContext->SystemContext.SystemContextAArch64 != NULL) {
    FreePool (FiberContext->SystemContext.SystemContextAArch64);
    FiberContext->SystemContext.SystemContextAArch64 = NULL;
  }

  if (FiberContext->ParentSystemContext.SystemContextAArch64 != NULL) {
    FreePool (FiberContext->ParentSystemContext.SystemContextAArch64);
    FiberContext->ParentSystemContext.SystemContextAArch64 = NULL;
  }
}

/**
  System Fiber start function
  This function is called when the fiber is started
  and will call the entry point of the fiber

  @param[in] Fiber  Fiber object
*/
STATIC
VOID
EFIAPI
SystemFiberStart (
  IN SYSTEM_FIBER  Fiber
  )
{
  SYSTEM_FIBER_CONTEXT  *FiberContext;

  FiberContext = (SYSTEM_FIBER_CONTEXT *)Fiber;

  FiberContext->EntryPoint (FiberContext->Context);
  DestroySystemFiber (Fiber);

  // Should never get here
  ASSERT (FALSE);
  CpuDeadLoop ();
}

/**
  Creates a new fiber

  @param[in] EntryPoint  Entry point of the fiber
  @param[in] Context     Context of the fiber
  @param[in] StackSize   Stack size of the fiber
  @param[out] Fiber     Fiber object

  @return EFI_SUCCESS if the fiber was created successfully
  @return EFI_OUT_OF_RESOURCES if the fiber could not be created
  @return EFI_INVALID_PARAMETER if the entry point is NULL
*/
EFI_STATUS
EFIAPI
CreateSystemFiber (
  IN SYSTEM_FIBER_ENTRY_POINT  EntryPoint,
  IN VOID                      *Context,
  IN UINTN                     StackSize,
  OUT SYSTEM_FIBER             *Fiber
  )
{
  EFI_STATUS            Status;
  SYSTEM_FIBER_CONTEXT  *FiberContext;

  FiberContext = NULL;

  if (Fiber == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto ExitHandler;
  }

  if (EntryPoint == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto ExitHandler;
  }

  if (StackSize < MIN_STACK_SIZE) {
    Status = EFI_INVALID_PARAMETER;
    goto ExitHandler;
  }

  FiberContext = (SYSTEM_FIBER_CONTEXT *)AllocateZeroPool (sizeof (SYSTEM_FIBER_CONTEXT));
  if (FiberContext == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitHandler;
  }

  FiberContext->EntryPoint = EntryPoint;
  FiberContext->Context    = Context;
  FiberContext->StackSize  = StackSize;
  FiberContext->Stack      = AllocatePages (EFI_SIZE_TO_PAGES (StackSize));
  if (FiberContext->Stack == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: AllocatePages Stack failed\n", __func__));
    goto ExitHandler;
  }

  // Allocate memory for any system context, using AArch64 as placeholder
  FiberContext->SystemContext.SystemContextAArch64 = (EFI_SYSTEM_CONTEXT_AARCH64 *)AllocateZeroPool (MAX_SYSTEM_CONTEXT_SIZE);
  if (FiberContext->SystemContext.SystemContextAArch64 == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: AllocateZeroPool SystemContext failed\n", __func__));
    goto ExitHandler;
  }

  // Architecture specific initialization
  Status = InitializeSystemContext (&FiberContext->SystemContext, SystemFiberStart, FiberContext, FiberContext->Stack, StackSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InitializeSystemContext failed\n", __func__));
    goto ExitHandler;
  }

  // Allocate memory for the parent system context
  FiberContext->ParentSystemContext.SystemContextAArch64 = (EFI_SYSTEM_CONTEXT_AARCH64 *)AllocateZeroPool (MAX_SYSTEM_CONTEXT_SIZE);
  if (FiberContext->ParentSystemContext.SystemContextAArch64 == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: AllocateZeroPool ParentSystemContext failed\n", __func__));
    goto ExitHandler;
  }

  FiberContext->IsRunning = FALSE;

  *Fiber = FiberContext;

ExitHandler:
  if (EFI_ERROR (Status)) {
    if (FiberContext != NULL) {
      FreeInternalResources (FiberContext);
      FreePool (FiberContext);
    }
  }

  return Status;
}

/**
  Destroys a fiber
  If the fiber is currently running, it will be yield before being destroyed

  @param[in] Fiber  Fiber object

  @return EFI_SUCCESS if the fiber was destroyed successfully
  @return EFI_INVALID_PARAMETER if the fiber is NULL
*/
EFI_STATUS
EFIAPI
DestroySystemFiber (
  IN SYSTEM_FIBER  Fiber
  )
{
  EFI_STATUS            Status;
  SYSTEM_FIBER_CONTEXT  *FiberContext;

  if (Fiber == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FiberContext = (SYSTEM_FIBER_CONTEXT *)Fiber;

  // If the fiber is running, yield it before destroying it
  FiberContext->IsDestroyed = TRUE;
  if (FiberContext->IsRunning) {
    // This will end up in ResumeSystemFiber call in other context and will never return
    Status = YieldSystemFiber (Fiber);
    // Should never get here
    ASSERT (FALSE);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  FreeInternalResources (FiberContext);
  FreePool (FiberContext);

  return EFI_SUCCESS;
}

/**
  Resumes a fiber

  @param[in] Fiber  Fiber object

  @return EFI_SUCCESS if the fiber was resumed successfully
  @return EFI_INVALID_PARAMETER if the fiber is NULL
  @return EFI_ALREADY_STARTED if the fiber is already running
  @return EFI_ABORTED if the fiber is marked as destroyed
*/
EFI_STATUS
EFIAPI
ResumeSystemFiber (
  IN SYSTEM_FIBER  Fiber
  )
{
  EFI_STATUS            Status;
  SYSTEM_FIBER_CONTEXT  *FiberContext;

  if (Fiber == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FiberContext = (SYSTEM_FIBER_CONTEXT *)Fiber;

  if (FiberContext->IsRunning) {
    return EFI_ALREADY_STARTED;
  }

  if (FiberContext->IsDestroyed) {
    return EFI_ABORTED;
  }

  FiberContext->IsRunning = TRUE;
  Status                  = SwapSystemContext (FiberContext->ParentSystemContext, FiberContext->SystemContext);
  // Free resources if the fiber is marked as destroyed, happens when the fiber exits it main function
  // Have to do this here as opposed to the DestroySystemFiber function for running fibers
  // as we need to yield the fiber before destroying it
  if (FiberContext->IsDestroyed) {
    FreeInternalResources (FiberContext);
  }

  return Status;
}

/**
  Yields the current fiber

  @param[in] Fiber  Fiber object

  @return EFI_SUCCESS if the fiber was yielded successfully
  @return EFI_INVALID_PARAMETER if the fiber is NULL
  @return EFI_NOT_STARTED if the fiber is not running
*/
EFI_STATUS
EFIAPI
YieldSystemFiber (
  IN SYSTEM_FIBER  Fiber
  )
{
  EFI_STATUS            Status;
  SYSTEM_FIBER_CONTEXT  *FiberContext;

  if (Fiber == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FiberContext = (SYSTEM_FIBER_CONTEXT *)Fiber;

  if (!FiberContext->IsRunning) {
    return EFI_NOT_STARTED;
  }

  FiberContext->IsRunning = FALSE;
  Status                  = SwapSystemContext (FiberContext->SystemContext, FiberContext->ParentSystemContext);
  return Status;
}
