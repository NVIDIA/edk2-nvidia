/** @file

  DiskIo Protocol stubs for host based tests

  This library builds a mocked EFI_DISK_IO_PROTOCOL.

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DISKIO_STUB_PROTOCOL_H__
#define __DISKIO_STUB_PROTOCOL_H__

#include <Uefi.h>
#include <Protocol/DiskIo.h>
#include <Library/BaseMemoryLib.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

/**
  Set the return values for the stub implementation of DiskIo.ReadDisk.

  @param[In]  ExpectedOffset  Expected value of Offset
  @param[In]  ReadBuffer    Will be copied into Buffer.
  @param[In]  DiskIoReturn  Will be returned.
 */
VOID
MockDiskIoReadDisk (
  UINT64      ExpectedOffset,
  VOID        *ReadBuffer,
  EFI_STATUS  ReadStatus
  );

/**
  Create a new Mock DiskIo.

  Return a mocked DiskIo protocol.

  @retval A new DiskIo
 */
EFI_DISK_IO_PROTOCOL *
MockDiskIoCreate (
  VOID
  );

/**
  Destroy a Mock DiskIo.

  @param[In]  DiskIo   The DiskIo to destroy.
 **/
VOID
MockDiskIoDestroy (
  EFI_DISK_IO_PROTOCOL  *DiskIo
  );

#endif
