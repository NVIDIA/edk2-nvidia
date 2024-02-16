/*
 * Threading.h
 *
 *  Created on: Jul 17, 2017
 *      Author: mrabeda
 *
 *  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
 *  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 */

#ifndef MDEMODULEPKG_THREADINGDXE_THREADING_H_
#define MDEMODULEPKG_THREADINGDXE_THREADING_H_

//
// Return ID of core/CPU on which this function is called.
// Return information whether current core is a BSP
//
EFI_STATUS
EFIAPI
ThreadingIdentifyCpu (
  OUT UINTN    *CpuId,
  OUT BOOLEAN  *IsBsp
  );

//
// Create a new thread, enqueue it for execution. If there is an idle CPU,
// run this thread on that CPU.
//
EFI_STATUS
EFIAPI
ThreadingSpawnThread (
  IN  EFI_THREADING_PROCEDURE  ThreadProcedure,
  IN  VOID                     *ThreadArgument,
  IN  EFI_THREADING_PROCEDURE  OnThreadExit,
  IN  VOID                     *OnThreadExitArgument,
  IN  UINTN                    ThreadTimeout,
  OUT EFI_THREAD               *ThreadObj
  );

//
// Blocking wait for thread to finish execution
//
EFI_STATUS
EFIAPI
ThreadingWaitForThread (
  IN  EFI_THREAD  Thread
  );

//
// Cleanup thread data and cache the structure for future use
//
EFI_STATUS
EFIAPI
ThreadingCleanupThread (
  IN  EFI_THREAD  Thread
  );

//
// Get information on how many CPUs are available in the system
//
EFI_STATUS
EFIAPI
ThreadingGetCpuCount (
  IN  UINTN  *CpuCount,
  IN  UINTN  *EnabledCpuCount
  );

//
// Abort thread if running. Terminate thread structure.
//
EFI_STATUS
EFIAPI
ThreadingAbortThread (
  IN  EFI_THREAD  Thread
  );

#endif /* MDEMODULEPKG_THREADINGDXE_THREADING_H_ */
