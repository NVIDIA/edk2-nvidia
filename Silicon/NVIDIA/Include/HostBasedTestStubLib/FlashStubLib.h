/** @file

Flash stub definitions.

Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _FLASH_STUB_LIB_H_
#define _FLASH_STUB_LIB_H_

#include <Uefi.h>
#include <Protocol/BlockIo.h>

/**
  Initialize the Flash Stub.

  @param  Buffer                Pointer to the starting address for the flash
                                stub's memory.
  @param  BufferSize            BufferSize of the flash stub.
  @param  BlockSize             BlockSize of the flash stub.
  @param  IoAlign               IoAlign value for the BlockIo interface's media.
  @param  BlockIo               Pointer to the initialized BlockIo interface
                                returned back to the user.

  @retval EFI_SUCCESS           Initialization succeeded.
  @retval EFI_OUT_OF_RESOURCES  Internal memory allocation failed.
  @retval EFI_BAD_BUFFER_SIZE   BufferSize or BlockSize was 0,
                                or if BufferSize was not a multiple of BlockSize
  @retval EFI_INVALID_PARAMETER Buffer or BlockIo was NULL
**/
EFI_STATUS
EFIAPI
FlashStubInitialize (
  IN  VOID                   *Buffer,
  IN  UINTN                  BufferSize,
  IN  UINT32                 BlockSize,
  IN  UINT32                 IoAlign,
  OUT EFI_BLOCK_IO_PROTOCOL  **BlockIo
);

/**
  Clean up the space used by the flash stub if necessary.

  @param  BlockIo     BlockIo protocol of the flash stub.

  @retval EFI_SUCCESS Clean up was successful.
**/
EFI_STATUS
EFIAPI
FlashStubDestroy (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo
);

/**
  Initialize the Flash Stub.

  @param  Buffer                Pointer to the starting address for the flash
                                stub's memory.
  @param  BufferSize            BufferSize of the flash stub.
  @param  BlockSize             BlockSize of the flash stub.
  @param  IoAlign               IoAlign value for the BlockIo interface's media.
  @param  BlockIo               Pointer to the initialized BlockIo interface
                                returned back to the user.

  @retval EFI_SUCCESS           Initialization succeeded.
  @retval EFI_OUT_OF_RESOURCES  Internal memory allocation failed.
  @retval EFI_BAD_BUFFER_SIZE   BufferSize or BlockSize was 0,
                                or if BufferSize was not a multiple of BlockSize
  @retval EFI_INVALID_PARAMETER Buffer or BlockIo was NULL
**/
EFI_STATUS
EFIAPI
FaultyFlashStubInitialize (
  IN  VOID                   *Buffer,
  IN  UINTN                  BufferSize,
  IN  UINT32                 BlockSize,
  IN  UINT32                 IoAlign,
  OUT EFI_BLOCK_IO_PROTOCOL  **BlockIo
);

/**
  Clean up the space used by the flash stub if necessary.

  @param  BlockIo     BlockIo protocol of the flash stub.

  @retval EFI_SUCCESS Clean up was successful.
**/
EFI_STATUS
EFIAPI
FaultyFlashStubDestroy (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo
);

#endif
