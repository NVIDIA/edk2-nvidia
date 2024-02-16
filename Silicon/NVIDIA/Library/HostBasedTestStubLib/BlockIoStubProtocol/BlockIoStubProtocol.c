/** @file

  BlockIo Protocol stubs for host based tests

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/MemoryAllocationLib.h>

#include <HostBasedTestStubLib/BlockIoStubProtocol.h>

/**
  Create a new Mock BlockIo.

  Return a mocked BlockIo protocol.

  @retval A new BlockIo
 **/
EFI_BLOCK_IO_PROTOCOL *
MockBlockIoCreate (
  EFI_BLOCK_IO_MEDIA  *Media
  )
{
  EFI_BLOCK_IO_PROTOCOL  *BlockIo;

  BlockIo = AllocateZeroPool (sizeof (EFI_BLOCK_IO_PROTOCOL));

  BlockIo->Media = Media;

  return BlockIo;
}

/**
  Destroy a Mock BlockIo.

  @param[In]  BlockIo   The BlockIo to destroy.
 **/
VOID
MockBlockIoDestroy (
  EFI_BLOCK_IO_PROTOCOL  *BlockIo
  )
{
  FreePool (BlockIo);
}
