/** @file

FlashStubLib private definitions.

Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _FLASH_STUB_LIB_PRIVATE_H_
#define _FLASH_STUB_LIB_PRIVATE_H_

#include <Uefi.h>
#include <Protocol/BlockIo.h>

typedef struct {
  UINT32                  Signature;
  EFI_BLOCK_IO_PROTOCOL   BlockIo;
  EFI_BLOCK_IO_MEDIA      Media;
  UINT64                  StartingAddr;
  UINT64                  Size;
} FLASH_TEST_PRIVATE;

#define DATA_BUFFER_BLOCK_NUM   (64)

#define FLASH_TEST_PRIVATE_SIGNATURE    SIGNATURE_32 ('F', 'S', 'H', 'T')

#define FLASH_TEST_PRIVATE_FROM_BLOCK_IO(a)   CR (a, FLASH_TEST_PRIVATE, BlockIo, FLASH_TEST_PRIVATE_SIGNATURE)

#endif
