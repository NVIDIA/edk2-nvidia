/** @file

  SystemFiberLib - Library for system fiber operations

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SYSTEM_FIBER_LIB_H__
#define SYSTEM_FIBER_LIB_H__

#include <Uefi/UefiBaseType.h>

typedef VOID (*SYSTEM_FIBER_ENTRY_POINT)(
  VOID  *Context
  );
typedef VOID *SYSTEM_FIBER;

/**
  Creates a new fiber.
  Fiber will not be started until ResumeSystemFiber is called.

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
  );

/**
  Destroys a fiber
  If the fiber is currently running, it will yield before being destroyed

  @param[in] Fiber  Fiber object

  @return EFI_SUCCESS if the fiber was destroyed successfully
  @return EFI_INVALID_PARAMETER if the fiber is NULL
*/
EFI_STATUS
EFIAPI
DestroySystemFiber (
  IN SYSTEM_FIBER  Fiber
  );

/**
  Resumes a fiber
  Will not return until the fiber yields.

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
  );

/**
  Yields the current fiber.
  This function will return to the caller of ResumeSystemFiber.

  @param[in] Fiber  Fiber object

  @return EFI_SUCCESS if the fiber was yielded successfully
  @return EFI_INVALID_PARAMETER if the fiber is NULL
  @return EFI_NOT_STARTED if the fiber is not running
*/
EFI_STATUS
EFIAPI
YieldSystemFiber (
  IN SYSTEM_FIBER  Fiber
  );

#endif
