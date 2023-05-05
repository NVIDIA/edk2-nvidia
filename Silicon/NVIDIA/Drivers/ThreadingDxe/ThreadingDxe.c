/*
 * Threading.c
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

#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SynchronizationLib.h>

#include <Protocol/MpService.h>
#include <Protocol/Threading.h>

#include "ThreadingDxe.h"

//
// Enum defining state in which CPU currently is
// - IDLE - doing nothing
// - BUSY - CPU is currently executing an assigned thread
// - BSP - CPU is a BSP and should not be executing threads
//
typedef enum _THREADING_CPU_STATE {
  THREADING_CPU_IDLE,
  THREADING_CPU_BUSY,
  THREADING_CPU_BSP
} THREADING_CPU_STATE;

typedef enum _THREAD_STATE {
  THREADING_THREAD_SPAWNED,
  THREADING_THREAD_READY,
  THREADING_THREAD_RUNNING,
  THREADING_THREAD_FINISHED
} THREAD_STATE;

typedef struct _INTERNAL_EFI_THREAD {
  LIST_ENTRY                 Entry;
  THREAD_STATE               State;
  EFI_THREADING_PROCEDURE    Procedure;
  VOID                       *ProcedureArgument;
  EFI_EVENT                  FinishedEvent;
  EFI_THREADING_PROCEDURE    OnThreadExit;
  VOID                       *OnThreadExitArgument;
  UINTN                      Timeout;
  UINTN                      CpuId;
} INTERNAL_EFI_THREAD;

typedef struct _THREADING_CPU_INFO {
  UINTN                  CpuId;
  UINT64                 ApicId;
  INTERNAL_EFI_THREAD    *CurrentThread;
  THREADING_CPU_STATE    State;
  BOOLEAN                Initialized;
} THREADING_CPU_INFO;

typedef struct _THREADING_DATA {
  UINTN                 CpuCount;
  UINTN                 EnabledCpuCount;
  THREADING_CPU_INFO    *CpuInfo;
  SPIN_LOCK             ThreadsQueuedLock;
  LIST_ENTRY            ThreadsQueued;
} THREADING_DATA;

EFI_MP_SERVICES_PROTOCOL  *mMultiProc      = NULL;
THREADING_DATA            mThreadingData   = { 0 };
EFI_HANDLE                mThreadingHandle = NULL;
UINTN                     mBspCpuId        = 0;

EFI_THREADING_PROTOCOL  gThreading = {
  ThreadingIdentifyCpu,
  ThreadingSpawnThread,
  ThreadingWaitForThread,
  ThreadingCleanupThread,
  ThreadingGetCpuCount,
  ThreadingAbortThread
};

//
// Forward declarations
//
EFI_STATUS
ThreadingRunThread (
  INTERNAL_EFI_THREAD  *Thread,
  UINTN                CpuId
  );

//
// Verify whether specific CPU is currently busy running tasks.
// This information is based on saved CPU status
//
EFI_STATUS
ThreadingIsCpuBusy (
  UINTN  CpuId
  )
{
  if (CpuId >= mThreadingData.CpuCount) {
    return EFI_INVALID_PARAMETER;
  }

  switch (mThreadingData.CpuInfo[CpuId].State) {
    case THREADING_CPU_BSP:
      return EFI_UNSUPPORTED;
    case THREADING_CPU_BUSY:
      return EFI_ACCESS_DENIED;
    case THREADING_CPU_IDLE:
      return EFI_SUCCESS;
    default:
      return EFI_UNSUPPORTED;
  }
}

//
// Iterate through CPU list to find first idle CPU
//
UINTN
ThreadingFindFreeCpu (
  )
{
  UINTN  i;

  for (i = 0; i < mThreadingData.CpuCount; i++) {
    if ((ThreadingIsCpuBusy (i) == EFI_SUCCESS) && (mThreadingData.CpuInfo[i].Initialized == TRUE)) {
      return i;
    }
  }

  return MAX_UINTN;
}

//
// Generic function to be called on thread finishing its execution.
// Calls ThreadOnExit procedure, then if other threads are queued, runs
// first one from the queue on the same CPU that finished thread execution.
//
VOID
ThreadingGenericOnThreadExit (
  EFI_EVENT  Event,
  VOID       *Arg
  )
{
  INTERNAL_EFI_THREAD  *Thread;
  INTERNAL_EFI_THREAD  *NewThread;
  UINTN                CpuId;
  BOOLEAN              IsBsp;

  Thread = (INTERNAL_EFI_THREAD *)Arg;

  gBS->CloseEvent (Event);
  Thread->FinishedEvent = NULL;

  ThreadingIdentifyCpu (&CpuId, &IsBsp);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Generic OnThreadExit\n", CpuId, (UINT64)Thread, Thread->CpuId));

  if (Thread->OnThreadExit) {
    DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Calling user OnThreadExit\n", CpuId, (UINT64)Thread, Thread->CpuId));
    Thread->OnThreadExit (Thread->OnThreadExitArgument);
  }

  Thread->State = THREADING_THREAD_FINISHED;

  AcquireSpinLock (&mThreadingData.ThreadsQueuedLock);
  if (!IsListEmpty (&mThreadingData.ThreadsQueued)) {
    NewThread = (INTERNAL_EFI_THREAD *)GetFirstNode (&mThreadingData.ThreadsQueued);
    RemoveEntryList ((LIST_ENTRY *)NewThread);
  } else {
    NewThread = NULL;
  }

  ReleaseSpinLock (&mThreadingData.ThreadsQueuedLock);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Thread completed\n", CpuId, (UINT64)Thread, Thread->CpuId));

  gBS->CloseEvent (Event);

  mThreadingData.CpuInfo[Thread->CpuId].State = THREADING_CPU_IDLE;

  if (NewThread != NULL) {
    DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Found threads enqueued for execution. Starting\n", CpuId, (UINT64)Thread, Thread->CpuId));
    ThreadingRunThread (NewThread, Thread->CpuId);
  }

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Generic OnThreadExit exit\n", CpuId, (UINT64)Thread, Thread->CpuId));
}

//
// Generic function to be called on thread execution start.
// Updates thread structure and calls thread procedure
//
VOID
ThreadingGenericProcedure (
  VOID  *Arg
  )
{
  INTERNAL_EFI_THREAD  *Thread;
  UINTN                CpuId;
  BOOLEAN              IsBsp;

  Thread = (INTERNAL_EFI_THREAD *)Arg;

  ThreadingIdentifyCpu (&CpuId, &IsBsp);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX] Generic procedure start\n", CpuId, (UINT64)Thread));

  while (Thread->State != THREADING_THREAD_READY) {
    CpuPause ();
  }

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Calling user procedure\n", CpuId, (UINT64)Thread, Thread->CpuId));

  Thread->State = THREADING_THREAD_RUNNING;
  Thread->Procedure (Thread->ProcedureArgument);
  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX] User procedure done\n", CpuId, (UINT64)Thread));
}

//
// Obtain total count of CPUs (total & enabled)
//
EFI_STATUS
EFIAPI
ThreadingGetCpuCount (
  UINTN  *CpuCount,
  UINTN  *EnabledCpuCount
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  DEBUG ((DEBUG_VERBOSE, "[T] Getting CPU count\n"));

  if ((CpuCount == NULL) || (EnabledCpuCount == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (mThreadingData.CpuCount == 0) {
    Status = mMultiProc->GetNumberOfProcessors (
                           mMultiProc,
                           CpuCount,
                           EnabledCpuCount
                           );
    ASSERT_EFI_ERROR (Status);
  } else {
    *CpuCount        = mThreadingData.CpuCount;
    *EnabledCpuCount = mThreadingData.EnabledCpuCount;
  }

  DEBUG ((DEBUG_VERBOSE, "[T] GetCpuCount status = %r\n", Status));
  DEBUG ((DEBUG_VERBOSE, "[T] CPUs = %d, Enabled CPUs = %d\n", *CpuCount, *EnabledCpuCount));

  return Status;
}

//
// Return information regarding CPU ID and whether current
// CPU is a BSP.
//
EFI_STATUS
EFIAPI
ThreadingIdentifyCpu (
  OUT UINTN    *CpuId,
  OUT BOOLEAN  *IsBsp
  )
{
  EFI_STATUS  Status;

  Status = mMultiProc->WhoAmI (mMultiProc, CpuId);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *IsBsp = (*CpuId == mBspCpuId);
  return EFI_SUCCESS;
}

//
// Start a thread on specific CPU
//
EFI_STATUS
ThreadingRunThread (
  INTERNAL_EFI_THREAD  *Thread,
  UINTN                CpuId
  )
{
  EFI_STATUS  Status;
  UINTN       MyCpuId;
  BOOLEAN     IsBsp;

  ThreadingIdentifyCpu (&MyCpuId, &IsBsp);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] RunThread start\n", MyCpuId, (UINT64)Thread, CpuId));

  if ((Thread == NULL) || (MyCpuId == CpuId)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Reserve CPU
  //
  mThreadingData.CpuInfo[CpuId].State = THREADING_CPU_BUSY;

  //
  // Create OnThreadExit event
  //
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  ThreadingGenericOnThreadExit,
                  (VOID *)Thread,
                  &Thread->FinishedEvent
                  );

  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] OnThreadExit event created\n", MyCpuId, (UINT64)Thread, CpuId));

  //
  // Start thread on selected CPU. It will stop waiting for thread to move
  // to READY state.
  //
  Status = mMultiProc->StartupThisAP (
                         mMultiProc,
                         ThreadingGenericProcedure,
                         CpuId,
                         Thread->FinishedEvent,
                         Thread->Timeout,
                         Thread,
                         NULL
                         );

  if (EFI_ERROR (Status)) {
    ThreadingCleanupThread ((EFI_THREAD *)Thread);
    goto ON_ERROR;
  }

  //
  // Add thread to running queue
  //
  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] AP started\n", MyCpuId, (UINT64)Thread, CpuId));
  Thread->CpuId                               = CpuId;
  mThreadingData.CpuInfo[CpuId].CurrentThread = Thread;
  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Thread ready\n", MyCpuId, (UINT64)Thread, CpuId));

  //
  // Move thread state to READY to begin proper thread execution
  //
  Thread->State = THREADING_THREAD_READY;

  return EFI_SUCCESS;

ON_ERROR:
  //
  // Free up CPU in case something goes wrong
  //
  mThreadingData.CpuInfo[CpuId].State = THREADING_CPU_IDLE;
  return Status;
}

//
// Protocol functions
//

//
// Create a new thread, enqueue it for execution. If there is an idle CPU,
// run this thread on that CPU.
//
EFI_STATUS
ThreadingSpawnThread (
  IN  EFI_THREADING_PROCEDURE  ThreadProcedure,
  IN  VOID                     *ThreadArgument,
  IN  EFI_THREADING_PROCEDURE  OnThreadExit,
  IN  VOID                     *OnThreadExitArgument,
  IN  UINTN                    ThreadTimeout,
  OUT EFI_THREAD               *ThreadObj
  )
{
  INTERNAL_EFI_THREAD  *Thread;
  UINTN                CpuId = 0;
  UINTN                MyCpuId;
  BOOLEAN              IsBsp;

  if (ThreadProcedure == 0) {
    return EFI_INVALID_PARAMETER;
  }

  ThreadingIdentifyCpu (&MyCpuId, &IsBsp);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d] SpawnThread start\n", MyCpuId));

  //
  // Create thread object
  //
  Thread = AllocateZeroPool (sizeof (INTERNAL_EFI_THREAD));
  if (Thread == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX] Thread object allocated\n", MyCpuId, (UINT64)Thread));

  Thread->Procedure            = ThreadProcedure;
  Thread->ProcedureArgument    = ThreadArgument;
  Thread->OnThreadExit         = OnThreadExit;
  Thread->OnThreadExitArgument = OnThreadExitArgument;
  Thread->FinishedEvent        = 0;
  Thread->Timeout              = ThreadTimeout;
  Thread->State                = THREADING_THREAD_SPAWNED;
  InitializeListHead (&Thread->Entry);

  *ThreadObj = Thread;

  CpuId = ThreadingFindFreeCpu ();

  if (CpuId == MAX_UINTN) {
    //
    // No free CPU now, enqueue it for later execution.
    //
    DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX] No free CPU. Caching\n", MyCpuId, (UINT64)Thread));
    AcquireSpinLock (&mThreadingData.ThreadsQueuedLock);
    InsertTailList (&mThreadingData.ThreadsQueued, &Thread->Entry);
    ReleaseSpinLock (&mThreadingData.ThreadsQueuedLock);
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Free CPU found. Attempting to run thread\n", MyCpuId, (UINT64)Thread, CpuId));

  return ThreadingRunThread (Thread, CpuId);
}

//
// Blocking wait for thread to finish execution
//
EFI_STATUS
EFIAPI
ThreadingWaitForThread (
  IN  EFI_THREAD  Thread
  )
{
  INTERNAL_EFI_THREAD  *IThread;
  UINTN                CpuId;
  BOOLEAN              IsBsp;
  EFI_TPL              OldTpl;

  if (Thread == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  IThread = (INTERNAL_EFI_THREAD *)Thread;

  ThreadingIdentifyCpu (&CpuId, &IsBsp);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Waiting for thread to finish\n", CpuId, (UINT64)Thread, IThread->CpuId));

  switch (IThread->State) {
    case THREADING_THREAD_SPAWNED:
    case THREADING_THREAD_READY:
    case THREADING_THREAD_RUNNING:
      break;
    case THREADING_THREAD_FINISHED:
      DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Instant finish detected\n", CpuId, (UINT64)Thread, IThread->CpuId));
      return EFI_SUCCESS;
    default:
      return EFI_NOT_STARTED;
  }

  //
  // If CPU called this function is a BSP, run background processes as well
  //
  while (IThread->State != THREADING_THREAD_FINISHED) {
    if (IsBsp) {
      OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
      gBS->RestoreTPL (OldTpl);
    } else {
      CpuPause ();
    }
  }

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX, CPU %d] Thread finished\n", CpuId, (UINT64)Thread, IThread->CpuId));

  return EFI_SUCCESS;
}

//
// Cleanup thread data and cache the structure for future use
//
EFI_STATUS
EFIAPI
ThreadingCleanupThread (
  IN  EFI_THREAD  Thread
  )
{
  INTERNAL_EFI_THREAD  *IThread;
  UINTN                CpuId;
  BOOLEAN              IsBsp;

  ThreadingIdentifyCpu (&CpuId, &IsBsp);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX] Cleaning up thread\n", CpuId, (UINT64)Thread));

  if (Thread == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  IThread = (INTERNAL_EFI_THREAD *)Thread;

  if ((IThread->State == THREADING_THREAD_RUNNING) || (IThread->State == THREADING_THREAD_READY)) {
    return EFI_ALREADY_STARTED;
  }

  AcquireSpinLock (&mThreadingData.ThreadsQueuedLock);
  if (IThread->State == THREADING_THREAD_SPAWNED) {
    RemoveEntryList (&IThread->Entry);
    gBS->CloseEvent (IThread->FinishedEvent);
  }

  ReleaseSpinLock (&mThreadingData.ThreadsQueuedLock);

  FreePool (IThread);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %d][THREAD %lX] Thread cleaned\n", CpuId, (UINT64)IThread));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ThreadingAbortThread (
  IN  EFI_THREAD  Thread
  )
{
  INTERNAL_EFI_THREAD  *IThread;

  if (Thread == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  IThread = (INTERNAL_EFI_THREAD *)Thread;

  //
  // Case 1: Running
  // Case 2: Spawned
  //
  switch (IThread->State) {
    case THREADING_THREAD_READY:
    case THREADING_THREAD_RUNNING:
      //
      // Terminate running thread. Switch to new thread.
      //
      DEBUG ((DEBUG_ERROR, "[T] AbortThread: Stopping AP...\n"));
      ThreadingWaitForThread (Thread);
      DEBUG ((DEBUG_ERROR, "[T] AbortThread: Notifying finished event...\n"));
      break;
    default:
      break;
  }

  return EFI_SUCCESS;
}

//
// Further initialization code and threading protocol installation upon
// MP Services protocol installation
//
EFI_STATUS
ThreadingInitCores (
  )
{
  EFI_STATUS  Status;
  UINTN       i /*, j*/;

  DEBUG ((DEBUG_VERBOSE, "[T][INIT] Commencing second init stage\n"));

  //
  // Obtain processor count
  //
  Status = ThreadingGetCpuCount (
             &mThreadingData.CpuCount,
             &mThreadingData.EnabledCpuCount
             );
  ASSERT_EFI_ERROR (Status);

  //
  // If system is single-core (BSP only), using this library has no point
  //
  if ((mThreadingData.CpuCount == 1) || (mThreadingData.EnabledCpuCount == 1)) {
    return EFI_UNSUPPORTED;
  }

  //
  // Create CPU info table
  //
  mThreadingData.CpuInfo = AllocatePool (mThreadingData.CpuCount * sizeof (THREADING_CPU_INFO));
  if (mThreadingData.CpuInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = mMultiProc->WhoAmI (mMultiProc, &mBspCpuId);
  ASSERT_EFI_ERROR (Status);

  for (i = 0; i < mThreadingData.CpuCount; i++) {
    mThreadingData.CpuInfo[i].CpuId         = i;
    mThreadingData.CpuInfo[i].CurrentThread = NULL;
    if (i == mBspCpuId) {
      mThreadingData.CpuInfo[i].State = THREADING_CPU_BSP;
    } else {
      mThreadingData.CpuInfo[i].State = THREADING_CPU_IDLE;
    }

    mThreadingData.CpuInfo[i].Initialized = TRUE;
  }

  //
  // Initialize thread queues (for control reasons)
  //
  InitializeListHead (&mThreadingData.ThreadsQueued);
  InitializeSpinLock (&mThreadingData.ThreadsQueuedLock);

  DEBUG ((DEBUG_VERBOSE, "[T][INIT] CPU data initialized\n"));

  //
  // Initialize threading protocol
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mThreadingHandle,
                  &gEfiThreadingProtocolGuid,
                  &gThreading,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  DEBUG ((DEBUG_VERBOSE, "[T][INIT] Threading protocol is now installed\n"));

  return EFI_SUCCESS;
}

//
// Initialize this threading library
// This boils down to locating MP Services protocol and initializing internal
// data for spawning and running threads.
//
EFI_STATUS
ThreadingDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  VOID        *TimerInit;

  //
  // If already initialized, exit.
  //
  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gEfiThreadingProtocolGuid);

  DEBUG ((DEBUG_VERBOSE, "[T][INIT] ThreadingLib entry point\n"));

  //
  // Test if MP Services protocol is installed
  //
  Status = gBS->LocateProtocol (&gEfiMpServiceProtocolGuid, NULL, (VOID **)&mMultiProc);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Test if Timer protocol is installed
  //
  Status = gBS->LocateProtocol (&gEfiTimerArchProtocolGuid, NULL, &TimerInit);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_VERBOSE, "[T][INIT] Found both MP & Timer. OK!\n"));

  //
  // Initialize APs
  //
  Status = ThreadingInitCores ();

  return Status;
}

EFI_STATUS
EFIAPI
ThreadingDriverUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;

  //
  // Uninstall protocol
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  mThreadingHandle,
                  &gEfiThreadingProtocolGuid,
                  &gThreading
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Release memory resources
  //
  FreePool (mThreadingData.CpuInfo);

  return EFI_SUCCESS;
}
