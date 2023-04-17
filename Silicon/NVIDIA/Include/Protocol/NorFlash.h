/** @file
  NVIDIA Nor Flash Protocol

  Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_NOR_FLASH_PROTOCOL_H__
#define __NVIDIA_NOR_FLASH_PROTOCOL_H__

#include <Pi/PiFirmwareVolume.h>

typedef struct {
  UINT64    MemoryDensity;
  UINT32    BlockSize;
} NOR_FLASH_ATTRIBUTES;

//
// Define for forward reference.
//
typedef struct _NVIDIA_NOR_FLASH_PROTOCOL NVIDIA_NOR_FLASH_PROTOCOL;

/**
  Get NOR Flash Attributes.

  @param[in]  This                  Instance to protocol
  @param[out] Attributes            Pointer to flash attributes

  @retval EFI_SUCCESS               Operation successful.
  @retval others                    Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *NOR_FLASH_GET_ATTRIBUTES)(
  IN  NVIDIA_NOR_FLASH_PROTOCOL *This,
  OUT NOR_FLASH_ATTRIBUTES      *Attributes
  );

/**
  Read data from NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Offset to read from
  @param[in] Size                  Number of bytes to be read
  @param[in] Buffer                Address to read data into

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *NOR_FLASH_READ)(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Offset,
  IN UINT32                    Size,
  IN VOID                      *Buffer
  );

/**
  Write data to NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Offset to write to
  @param[in] Size                  Number of bytes to write
  @param[in] Buffer                Address to write data from

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *NOR_FLASH_WRITE)(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Offset,
  IN UINT32                    Size,
  IN VOID                      *Buffer
  );

/**
  Erase data from NOR Flash.

  @param[in] This                  Instance to protocol
  @param[in] Lba                   Logical block to start erasing from
  @param[in] NumLba                Number of block to be erased

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *NOR_FLASH_ERASE)(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Lba,
  IN UINT32                    NumLba
  );

/// NVIDIA_NOR_FLASH_PROTOCOL protocol structure.
struct _NVIDIA_NOR_FLASH_PROTOCOL {
  EFI_FVB_ATTRIBUTES_2        FvbAttributes;
  NOR_FLASH_GET_ATTRIBUTES    GetAttributes;
  NOR_FLASH_READ              Read;
  NOR_FLASH_WRITE             Write;
  NOR_FLASH_ERASE             Erase;
};

extern EFI_GUID  gNVIDIANorFlashProtocolGuid;

#endif
