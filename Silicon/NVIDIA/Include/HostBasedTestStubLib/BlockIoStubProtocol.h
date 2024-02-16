/** @file

  BlockIo Protocol stubs for host based tests

  This library builds a mocked EFI_BLOCK_IO_PROTOCOL.

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BLOCKIO_STUB_PROTOCOL_H__
#define __BLOCKIO_STUB_PROTOCOL_H__

#include <Uefi.h>
#include <Protocol/BlockIo.h>

/**
  Create a new Mock BlockIo.

  Return a mocked BlockIo protocol.

  @retval A new BlockIo
 */
EFI_BLOCK_IO_PROTOCOL *
MockBlockIoCreate (
  EFI_BLOCK_IO_MEDIA  *Media
  );

/**
  Destroy a Mock BlockIo.

  @param[In]  BlockIo   The BlockIo to destroy.
 **/
VOID
MockBlockIoDestroy (
  EFI_BLOCK_IO_PROTOCOL  *BlockIo
  );

#endif
