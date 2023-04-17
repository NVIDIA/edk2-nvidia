/** @file

Stub implementation of a flash device that reports device errors.

Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <HostBasedTestStubLib/NorFlashStubLib.h>

#include "NorFlashStubLibPrivate.h"

/**
  Get the Attributes of the SPINOR

  @param  This       A pointer to the calling protocol
  @param  Attributes A pointer to the Attributes buffer for the data. The
                     caller is responsible for either having implicit
                     or explicit ownership of the buffer.

  @retval EFI_DEVICE_ERROR      Error getting attributes
  @retval EFI_INVALID_PARAMETER Found a NULL parameter

**/
EFI_STATUS
EFIAPI
FaultyNorFlashGetAttributes (
  IN  NVIDIA_NOR_FLASH_PROTOCOL  *This,
  OUT NOR_FLASH_ATTRIBUTES       *Attributes
  )
{
  if ((This == NULL) ||
      (Attributes == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_DEVICE_ERROR;
}

/**
  Read Size bytes from Offset into Buffer.

  @param  This       A pointer to the calling protocol
  @param  Offset     The starting byte offset to read from
  @param  Size       Number of bytes to read into Buffer
  @param  Buffer     A pointer to the destination buffer for the data. The
                     caller is responsible for either having implicit
                     or explicit ownership of the buffer.

  @retval EFI_DEVICE_ERROR      Error reading device
  @retval EFI_INVALID_PARAMETER The parameters don't allow for a valid read.

**/
EFI_STATUS
EFIAPI
FaultyNorFlashRead (
  IN  NVIDIA_NOR_FLASH_PROTOCOL  *This,
  IN  UINT32                     Offset,
  IN  UINT32                     Size,
  OUT VOID                       *Buffer
  )
{
  VIRTUAL_NOR_FLASH_DEVICE  *Device;

  if ((This == NULL) ||
      (Buffer == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Device = NOR_FLASH_DEVICE_FROM_NOR_FLASH_PROTOCOL (This);

  if (Device->Attributes.MemoryDensity < Offset + Size) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_DEVICE_ERROR;
}

/**
  Write Size bytes from Buffer into Flash at Offset.

  @param  This       A pointer to the calling protocol
  @param  Offset     The starting byte offset to write to
  @param  Size       Number of bytes to write from Buffer
  @param  Buffer     A pointer to the source buffer for the data

  @retval EFI_DEVICE_ERROR      Error writing device
  @retval EFI_INVALID_PARAMETER The parameters don't allow for a valid write

**/
EFI_STATUS
EFIAPI
FaultyNorFlashWrite (
  IN NVIDIA_NOR_FLASH_PROTOCOL  *This,
  IN UINT32                     Offset,
  IN UINT32                     Size,
  IN VOID                       *Buffer
  )
{
  VIRTUAL_NOR_FLASH_DEVICE  *Device;

  if ((This == NULL) ||
      (Buffer == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Device = NOR_FLASH_DEVICE_FROM_NOR_FLASH_PROTOCOL (This);

  if (Device->Attributes.MemoryDensity < Offset + Size) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_DEVICE_ERROR;
}

/**
  Erase NumLba blocks of flash starting at block Lba

  @param  This       A pointer to the calling protocol
  @param  Lba        The first block number to erase
  @param  NumLba     Number of blocks to erase

  @retval EFI_DEVICE_ERROR      Error erasing device
  @retval EFI_INVALID_PARAMETER The parameters don't allow for a valid erase

**/
EFI_STATUS
EFIAPI
FaultyNorFlashErase (
  IN NVIDIA_NOR_FLASH_PROTOCOL  *This,
  IN UINT32                     Lba,
  IN UINT32                     NumLba
  )
{
  VIRTUAL_NOR_FLASH_DEVICE  *Device;
  UINT32                    Offset;
  UINT32                    Size;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Device = NOR_FLASH_DEVICE_FROM_NOR_FLASH_PROTOCOL (This);

  Offset = Lba * Device->Attributes.BlockSize;
  Size   = NumLba * Device->Attributes.BlockSize;

  if (Device->Attributes.MemoryDensity < Offset + Size) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_DEVICE_ERROR;
}

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
  )
{
  if ((Memory == NULL) ||
      (Protocol == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  VIRTUAL_NOR_FLASH_DEVICE  *Device;

  Device = AllocatePool (sizeof (VIRTUAL_NOR_FLASH_DEVICE));
  if (Device == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Device->Signature                = VIRTUAL_NOR_FLASH_SIGNATURE;
  Device->Memory                   = Memory;
  Device->Attributes.MemoryDensity = Size;
  Device->Attributes.BlockSize     = BlockSize;
  Device->Protocol.FvbAttributes   = 0;
  Device->Protocol.GetAttributes   = FaultyNorFlashGetAttributes;
  Device->Protocol.Read            = FaultyNorFlashRead;
  Device->Protocol.Write           = FaultyNorFlashWrite;
  Device->Protocol.Erase           = FaultyNorFlashErase;
  *Protocol                        = &Device->Protocol;
  return EFI_SUCCESS;
}

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
  )
{
  VIRTUAL_NOR_FLASH_DEVICE  *Device;

  if (Protocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Device = NOR_FLASH_DEVICE_FROM_NOR_FLASH_PROTOCOL (Protocol);

  if (Device != NULL) {
    FreePool (Device);
    Device = NULL;
  }

  return EFI_SUCCESS;
}
