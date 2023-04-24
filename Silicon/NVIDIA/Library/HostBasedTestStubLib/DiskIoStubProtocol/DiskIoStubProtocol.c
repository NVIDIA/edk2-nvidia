/** @file

  DiskIo Protocol stubs for host based tests

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/MemoryAllocationLib.h>

#include <HostBasedTestStubLib/DiskIoStubProtocol.h>

/**
  Stubbed implementation of DiskIo.ReadDisk.

  This will be used to implement the DiskIo protocol.  This function's
  behavior and returns can be set via MockDiskIoReadDisk().
 */
EFI_STATUS
EFIAPI
DiskIoStubReadDisk (
  IN EFI_DISK_IO_PROTOCOL  *This,
  IN UINT32                MediaId,
  IN UINT64                Offset,
  IN UINTN                 BufferSize,
  OUT VOID                 *Buffer
  )
{
  VOID        *Data;
  EFI_STATUS  Status;

  check_expected (Offset);

  Data   = (VOID *)mock ();
  Status = (EFI_STATUS)mock ();

  CopyMem (Buffer, Data, BufferSize);

  return Status;
}

/**
  Set the return values for the stub implementation of DiskIo.ReadDisk.

  @param[In]  ExpectedOffset  Expected value of Offset
  @param[In]  ReadBuffer      Will be copied into Buffer.
  @param[In]  ReadStatus      Will be returned.
 */
VOID
MockDiskIoReadDisk (
  UINT64      ExpectedOffset,
  VOID        *ReadBuffer,
  EFI_STATUS  ReadStatus
  )
{
  expect_value (DiskIoStubReadDisk, Offset, ExpectedOffset);
  will_return (DiskIoStubReadDisk, ReadBuffer);
  will_return (DiskIoStubReadDisk, ReadStatus);
}

/**
  Create a new Mock DiskIo.

  Return a mocked DiskIo protocol.

  @retval A new DiskIo
 **/
EFI_DISK_IO_PROTOCOL *
MockDiskIoCreate (
  VOID
  )
{
  EFI_DISK_IO_PROTOCOL  *DiskIo;

  DiskIo           = AllocateZeroPool (sizeof (EFI_DISK_IO_PROTOCOL));
  DiskIo->ReadDisk = DiskIoStubReadDisk;

  return DiskIo;
}

/**
  Destroy a Mock DiskIo.

  @param[In]  DiskIo   The DiskIo to destroy.
 **/
VOID
MockDiskIoDestroy (
  EFI_DISK_IO_PROTOCOL  *DiskIo
  )
{
  FreePool (DiskIo);
}
