/** @file

Stub implementation of a virtual NOR flash device.

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

  @retval EFI_SUCCESS           The Attributes were copied successfully
  @retval EFI_INVALID_PARAMETER Found a NULL parameter

**/
EFI_STATUS
EFIAPI
VirtualNorFlashGetAttributes (
  NVIDIA_NOR_FLASH_PROTOCOL  *This,
  NOR_FLASH_ATTRIBUTES       *Attributes
  )
{
  VIRTUAL_NOR_FLASH_DEVICE  *Device;

  if ((This == NULL) ||
      (Attributes == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Device = NOR_FLASH_DEVICE_FROM_NOR_FLASH_PROTOCOL (This);

  CopyMem (Attributes, &Device->Attributes, sizeof (NOR_FLASH_ATTRIBUTES));
  return EFI_SUCCESS;
}

/**
  Read Size bytes from Offset into Buffer.

  @param  This       A pointer to the calling protocol
  @param  Offset     The starting byte offset to read from
  @param  Size       Number of bytes to read into Buffer
  @param  Buffer     A pointer to the destination buffer for the data. The
                     caller is responsible for either having implicit
                     or explicit ownership of the buffer.

  @retval EFI_SUCCESS           The data was read correctly from the device.
  @retval EFI_INVALID_PARAMETER The parameters don't allow for a valid read.

**/
EFI_STATUS
EFIAPI
VirtualNorFlashRead (
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

  CopyMem (Buffer, Device->Memory + Offset, Size);
  return EFI_SUCCESS;
}

/**
  Write Size bytes from Buffer into Flash at Offset.

  @param  This       A pointer to the calling protocol
  @param  Offset     The starting byte offset to write to
  @param  Size       Number of bytes to write from Buffer
  @param  Buffer     A pointer to the source buffer for the data

  @retval EFI_SUCCESS           The data was written correctly to the virtual flash
  @retval EFI_INVALID_PARAMETER The parameters don't allow for a valid write

**/
EFI_STATUS
EFIAPI
VirtualNorFlashWrite (
  IN NVIDIA_NOR_FLASH_PROTOCOL  *This,
  IN UINT32                     Offset,
  IN UINT32                     Size,
  IN VOID                       *Buffer
  )
{
  VIRTUAL_NOR_FLASH_DEVICE  *Device;
  UINT8                     *Src;
  UINT8                     *Dst;
  INTN                      Index;

  if ((This == NULL) ||
      (Buffer == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Device = NOR_FLASH_DEVICE_FROM_NOR_FLASH_PROTOCOL (This);

  if (Device->Attributes.MemoryDensity < Offset + Size) {
    return EFI_INVALID_PARAMETER;
  }

  Dst = (UINT8 *)(Device->Memory + Offset);
  Src = (UINT8 *)Buffer;
  for (Index = 0; Index < Size; Index++) {
    Dst[Index] &= Src[Index];
  }

  return EFI_SUCCESS;
}

/**
  Erase NumLba blocks of flash starting at block Lba

  @param  This       A pointer to the calling protocol
  @param  Lba        The first block number to erase
  @param  NumLba     Number of blocks to erase

  @retval EFI_SUCCESS           The flash was erased to 0xFF
  @retval EFI_INVALID_PARAMETER The parameters don't allow for a valid erase

**/
EFI_STATUS
EFIAPI
VirtualNorFlashErase (
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

  SetMem (Device->Memory + Offset, Size, 0xFF);
  return EFI_SUCCESS;
}

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
  Device->Protocol.GetAttributes   = VirtualNorFlashGetAttributes;
  Device->Protocol.Read            = VirtualNorFlashRead;
  Device->Protocol.Write           = VirtualNorFlashWrite;
  Device->Protocol.Erase           = VirtualNorFlashErase;
  *Protocol                        = &Device->Protocol;
  return EFI_SUCCESS;
}

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
