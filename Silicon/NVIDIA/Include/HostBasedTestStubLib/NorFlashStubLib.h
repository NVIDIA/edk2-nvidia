/** @file

NOR Flash stub definitions.

Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NOR_FLASH_STUB_LIB_H_
#define _NOR_FLASH_STUB_LIB_H_

#include <Uefi.h>
#include <Protocol/NorFlash.h>

/**
  Create a virtual NOR flash device and return its protocol

  @param  Memory     A pointer to the memory to use for the virtual flash
  @param  Size       The size of the virtual flash memory
  @param  BlockSize  The size of the virtual flash device's blocks
  @param  Protocol   Where to write the pointer to the device's protocol

  @retval EFI_SUCCESS           The flash device was created
  @retval EFI_OUT_OF_RESOURCES  The device couldn't be allocated
  @retval EFI_INVALID_PARAMETER A Null parameter was found

**/
EFI_STATUS
EFIAPI
VirtualNorFlashInitialize (
  IN  UINT8                      *Memory,
  IN  UINT32                     Size,
  IN  UINT32                     BlockSize,
  OUT NVIDIA_NOR_FLASH_PROTOCOL  **Protocol
  );

/**
  Clean up the space used by the virtual NOR flash stub if necessary

  @param  Protocol              Protocol for the stub device to free

  @retval EFI_SUCCESS           Clean up was successful
  @retval EFI_INVALID_PARAMETER The protocol wasn't for a valid
                                virtual NOR flash device
**/
EFI_STATUS
EFIAPI
VirtualNorFlashStubDestroy (
  IN NVIDIA_NOR_FLASH_PROTOCOL  *Protocol
  );

/**
  Create a faulty NOR flash device and return its protocol

  @param  Memory     A pointer to the memory to use for the faulty flash
  @param  Size       The size of the faulty flash memory
  @param  BlockSize  The size of the faulty flash device's blocks
  @param  Protocol   Where to write the pointer to the device's protocol

  @retval EFI_SUCCESS           The flash device was created
  @retval EFI_OUT_OF_RESOURCES  The device couldn't be allocated
  @retval EFI_INVALID_PARAMETER A Null parameter was found

**/
EFI_STATUS
EFIAPI
FaultyNorFlashInitialize (
  IN  UINT8                      *Memory,
  IN  UINT32                     Size,
  IN  UINT32                     BlockSize,
  OUT NVIDIA_NOR_FLASH_PROTOCOL  **Protocol
  );

/**
  Clean up the space used by the faulty NOR flash stub if necessary

  @param  Protocol              Protocol for the stub device to free

  @retval EFI_SUCCESS           Clean up was successful
  @retval EFI_INVALID_PARAMETER The protocol wasn't for a valid
                                faulty NOR flash device
**/
EFI_STATUS
EFIAPI
FaultyNorFlashStubDestroy (
  IN NVIDIA_NOR_FLASH_PROTOCOL  *Protocol
  );

#endif
