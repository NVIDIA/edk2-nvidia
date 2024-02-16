/*
 * Threading.h
 *
 *  Created on: Jul 25, 2017
 *      Author: mrabeda
 *
 *  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 */

#ifndef THREADING_H_
#define THREADING_H_

//
// Threading Protocol GUID value
//
#define EFI_THREADING_PROTOCOL_GUID \
    { \
      0x99fc2222, 0x5c4d, 0x472b, { 0x84, 0xf9, 0x9f, 0xda, 0xf8, 0xe9, 0x9c, 0x03 } \
    }

//
// Forward reference
//
typedef struct _EFI_THREADING_PROTOCOL EFI_THREADING_PROTOCOL;

typedef
VOID
(EFIAPI *EFI_THREADING_PROCEDURE)(
  IN VOID   *Argument
  );

typedef VOID *EFI_THREAD;

typedef
EFI_STATUS
(EFIAPI *EFI_THREADING_IDENTIFY_CPU)(
  OUT UINTN     *CpuId,
  OUT BOOLEAN   *IsBsp
  );

typedef
EFI_STATUS
(EFIAPI *EFI_THREADING_SPAWN_THREAD)(
  IN  EFI_THREADING_PROCEDURE  ThreadProcedure,
  IN  VOID                     *ThreadArgument,
  IN  EFI_THREADING_PROCEDURE  OnThreadExit,
  IN  VOID                     *OnThreadExitArgument,
  IN  UINTN                    ThreadTimeout,
  OUT EFI_THREAD               *ThreadObj
  );

typedef
EFI_STATUS
(EFIAPI *EFI_THREADING_WAIT_FOR_THREAD)(
  IN  EFI_THREAD      Thread
  );

typedef
EFI_STATUS
(EFIAPI *EFI_THREADING_CLEANUP_THREAD)(
  IN  EFI_THREAD      Thread
  );

typedef
EFI_STATUS
(EFIAPI *EFI_THREADING_GET_CPU_COUNT)(
  IN  UINTN           *CpuCount,
  IN  UINTN           *EnabledCpuCount
  );

typedef
EFI_STATUS
(EFIAPI *EFI_THREADING_ABORT_THREAD)(
  IN  EFI_THREAD      Thread
  );

struct _EFI_THREADING_PROTOCOL {
  EFI_THREADING_IDENTIFY_CPU       IdentifyCpu;
  EFI_THREADING_SPAWN_THREAD       SpawnThread;
  EFI_THREADING_WAIT_FOR_THREAD    WaitForThread;
  EFI_THREADING_CLEANUP_THREAD     CleanupThread;
  EFI_THREADING_GET_CPU_COUNT      GetCpuCount;
  EFI_THREADING_ABORT_THREAD       AbortThread;
};

extern EFI_GUID  gEfiThreadingProtocolGuid;

#endif /* MDEMODULEPKG_INCLUDE_PROTOCOL_THREADING_H_ */
