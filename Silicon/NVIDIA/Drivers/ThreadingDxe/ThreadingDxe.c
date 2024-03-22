/*
 * Threading.c
 *
 *  Created on: Jul 17, 2017
 *      Author: mrabeda
 *
 *  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
 *  Copyright (c) 2023-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <Library/TimerLib.h>

#include <Protocol/MpService.h>
#include <Protocol/Threading.h>

#include "ThreadingDxe.h"
#include "Base.h"
#include "Uefi/UefiBaseType.h"

#define THREADING_CPU_RETRY_COUNT  10

//
// Enum defining state in which CPU currently is
// - IDLE - doing nothing
// - BUSY - CPU is currently executing an assigned thread
// - BSP - CPU is a BSP and should not be executing threads
// - TIMEOUT - CPU failed to enable but is idle
//
typedef enum _THREADING_CPU_STATE {
  THREADING_CPU_IDLE,
  THREADING_CPU_BUSY,
  THREADING_CPU_BSP,
  THREADING_CPU_TIMEOUT,
} THREADING_CPU_STATE;

typedef enum _THREAD_STATE {
  THREADING_THREAD_SPAWNED,
  THREADING_THREAD_STARTING,
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
// Iterate through CPU list to find first idle CPU
//
UINTN
ThreadingFindFreeCpu (
  )
{
  UINTN  Cpu;
  UINTN  FirstTimedoutCpu = MAX_UINTN;

  for (Cpu = 0; Cpu < mThreadingData.CpuCount; Cpu++) {
    if (mThreadingData.CpuInfo[Cpu].Initialized == TRUE) {
      if (mThreadingData.CpuInfo[Cpu].State == THREADING_CPU_IDLE) {
        return Cpu;
      } else if (mThreadingData.CpuInfo[Cpu].State == THREADING_CPU_TIMEOUT) {
        if (FirstTimedoutCpu == MAX_UINTN) {
          FirstTimedoutCpu = Cpu;
        }
      }
    }
  }

  return FirstTimedoutCpu;
}

/**
  Executes the next thread in the threading queue on a free CPU.

  This function searches for a free CPU using the ThreadingFindFreeCpu() function.
  If a free CPU is found, it acquires the ThreadsQueuedLock spin lock and checks if there are any threads queued for execution.
  If there are threads in the queue, it removes the first thread from the queue and releases the spin lock.
  If there are no threads in the queue, it sets the Thread variable to NULL and releases the spin lock.

  If a thread is found in the queue, it prints a debug message indicating the CPU and thread ID, and then calls the ThreadingRunThread() function to run the thread on the CPU.
  If ThreadingRunThread() returns EFI_NOT_READY, it means the CPU was not ready to execute the thread, so the thread is reinserted into the queue and the function continues searching for another free CPU.
  If ThreadingRunThread() returns an error status, the function breaks out of the loop.

  @retval None
**/
VOID
ThreadingQueueNextThread (
  VOID
  )
{
  EFI_STATUS           Status;
  INTERNAL_EFI_THREAD  *Thread;
  UINTN                CpuId = 0;

  while ((CpuId = ThreadingFindFreeCpu ()) != MAX_UINTN) {
    AcquireSpinLock (&mThreadingData.ThreadsQueuedLock);
    if (!IsListEmpty (&mThreadingData.ThreadsQueued)) {
      Thread = (INTERNAL_EFI_THREAD *)GetFirstNode (&mThreadingData.ThreadsQueued);
      RemoveEntryList ((LIST_ENTRY *)Thread);
    } else {
      Thread = NULL;
    }

    ReleaseSpinLock (&mThreadingData.ThreadsQueuedLock);

    if (Thread == NULL) {
      break;
    }

    DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] Found threads enqueued for execution. Starting\n", CpuId, (UINT64)Thread, Thread->CpuId));
    Status = ThreadingRunThread (Thread, CpuId);
    if (Status == EFI_NOT_READY) {
      AcquireSpinLock (&mThreadingData.ThreadsQueuedLock);
      InsertTailList (&mThreadingData.ThreadsQueued, &Thread->Entry);
      ReleaseSpinLock (&mThreadingData.ThreadsQueuedLock);
    } else if (EFI_ERROR (Status)) {
      break;
    }
  }
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
  UINTN                CpuId;
  BOOLEAN              IsBsp;

  Thread = (INTERNAL_EFI_THREAD *)Arg;

  gBS->CloseEvent (Event);
  Thread->FinishedEvent = NULL;

  ThreadingIdentifyCpu (&CpuId, &IsBsp);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] Generic OnThreadExit\n", CpuId, (UINT64)Thread, Thread->CpuId));

  if (Thread->OnThreadExit) {
    DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] Calling user OnThreadExit\n", CpuId, (UINT64)Thread, Thread->CpuId));
    Thread->OnThreadExit (Thread->OnThreadExitArgument);
  }

  Thread->State = THREADING_THREAD_FINISHED;

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] Thread completed\n", CpuId, (UINT64)Thread, Thread->CpuId));

  gBS->CloseEvent (Event);

  mThreadingData.CpuInfo[Thread->CpuId].State = THREADING_CPU_IDLE;

  ThreadingQueueNextThread ();

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] Generic OnThreadExit exit\n", CpuId, (UINT64)Thread, Thread->CpuId));
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

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX] Generic procedure start\n", CpuId, (UINT64)Thread));

  while (Thread->State != THREADING_THREAD_READY) {
    CpuPause ();
  }

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] Calling user procedure\n", CpuId, (UINT64)Thread, Thread->CpuId));

  Thread->State = THREADING_THREAD_RUNNING;
  Thread->Procedure (Thread->ProcedureArgument);
  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX] User procedure done\n", CpuId, (UINT64)Thread));
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
  DEBUG ((DEBUG_VERBOSE, "[T] CPUs = %u, Enabled CPUs = %u\n", *CpuCount, *EnabledCpuCount));

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
  UINTN       RetryCount;

  ThreadingIdentifyCpu (&MyCpuId, &IsBsp);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] RunThread start\n", MyCpuId, (UINT64)Thread, CpuId));

  if ((Thread == NULL) || (MyCpuId == CpuId)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Reserve CPU
  //
  mThreadingData.CpuInfo[CpuId].State = THREADING_CPU_BUSY;
  Thread->State                       = THREADING_THREAD_STARTING;

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

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] OnThreadExit event created\n", MyCpuId, (UINT64)Thread, CpuId));

  //
  // Start thread on selected CPU. It will stop waiting for thread to move
  // to READY state.
  //
  for (RetryCount = 0; RetryCount < THREADING_CPU_RETRY_COUNT; RetryCount++) {
    Status = mMultiProc->StartupThisAP (
                           mMultiProc,
                           ThreadingGenericProcedure,
                           CpuId,
                           Thread->FinishedEvent,
                           Thread->Timeout,
                           Thread,
                           NULL
                           );
    if (Status == EFI_NOT_READY) {
      DEBUG ((DEBUG_INFO, "[T][CPU %u][THREAD %lX, CPU %u] Failed to start AP, retrying\n", MyCpuId, (UINT64)Thread, CpuId));
      MicroSecondDelay (10);
    } else {
      break;
    }
  }

  if (Status == EFI_NOT_READY) {
    // Thread will be requeued.
    Thread->State = THREADING_THREAD_SPAWNED;
    gBS->CloseEvent (Thread->FinishedEvent);
    goto ON_ERROR;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[T][CPU %u][THREAD %lX, CPU %u] Failed to start AP\n", MyCpuId, (UINT64)Thread, CpuId));
    ThreadingCleanupThread ((EFI_THREAD *)Thread);
    goto ON_ERROR;
  }

  //
  // Add thread to running queue
  //
  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] AP started\n", MyCpuId, (UINT64)Thread, CpuId));
  Thread->CpuId                               = CpuId;
  mThreadingData.CpuInfo[CpuId].CurrentThread = Thread;
  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] Thread ready\n", MyCpuId, (UINT64)Thread, CpuId));

  //
  // Move thread state to READY to begin proper thread execution
  //
  Thread->State = THREADING_THREAD_READY;

  return EFI_SUCCESS;

ON_ERROR:
  //
  // Free up CPU in case something goes wrong
  //
  if (Status == EFI_NOT_READY) {
    mThreadingData.CpuInfo[CpuId].State = THREADING_CPU_TIMEOUT;
  } else {
    mThreadingData.CpuInfo[CpuId].State = THREADING_CPU_IDLE;
  }

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
  UINTN                MyCpuId;
  BOOLEAN              IsBsp;

  if (ThreadProcedure == 0) {
    return EFI_INVALID_PARAMETER;
  }

  ThreadingIdentifyCpu (&MyCpuId, &IsBsp);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u] SpawnThread start\n", MyCpuId));

  //
  // Create thread object
  //
  Thread = AllocateZeroPool (sizeof (INTERNAL_EFI_THREAD));
  if (Thread == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX] Thread object allocated\n", MyCpuId, (UINT64)Thread));

  Thread->Procedure            = ThreadProcedure;
  Thread->ProcedureArgument    = ThreadArgument;
  Thread->OnThreadExit         = OnThreadExit;
  Thread->OnThreadExitArgument = OnThreadExitArgument;
  Thread->FinishedEvent        = 0;
  Thread->Timeout              = ThreadTimeout;
  Thread->State                = THREADING_THREAD_SPAWNED;
  InitializeListHead (&Thread->Entry);

  *ThreadObj = Thread;

  //
  // enqueue it for later execution.
  //
  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX] No free CPU. Caching\n", MyCpuId, (UINT64)Thread));
  AcquireSpinLock (&mThreadingData.ThreadsQueuedLock);
  InsertTailList (&mThreadingData.ThreadsQueued, &Thread->Entry);
  ReleaseSpinLock (&mThreadingData.ThreadsQueuedLock);
  // Start any pending threads if CPUs are free
  ThreadingQueueNextThread ();
  return EFI_SUCCESS;
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

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] Waiting for thread to finish\n", CpuId, (UINT64)Thread, IThread->CpuId));

  switch (IThread->State) {
    case THREADING_THREAD_SPAWNED:
    case THREADING_THREAD_STARTING:
    case THREADING_THREAD_READY:
    case THREADING_THREAD_RUNNING:
      break;
    case THREADING_THREAD_FINISHED:
      DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] Instant finish detected\n", CpuId, (UINT64)Thread, IThread->CpuId));
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

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX, CPU %u] Thread finished\n", CpuId, (UINT64)Thread, IThread->CpuId));

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

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX] Cleaning up thread\n", CpuId, (UINT64)Thread));

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
  } else if (IThread->State == THREADING_THREAD_STARTING) {
    gBS->CloseEvent (IThread->FinishedEvent);
  }

  ReleaseSpinLock (&mThreadingData.ThreadsQueuedLock);

  FreePool (IThread);

  DEBUG ((DEBUG_VERBOSE, "[T][CPU %u][THREAD %lX] Thread cleaned\n", CpuId, (UINT64)IThread));
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
