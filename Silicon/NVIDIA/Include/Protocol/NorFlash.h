/** @file
  NVIDIA Nor Flash Protocol

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __NVIDIA_NOR_FLASH_PROTOCOL_H__
#define __NVIDIA_NOR_FLASH_PROTOCOL_H__


#include <Pi/PiFirmwareVolume.h>


#define NVIDIA_NOR_FLASH_PROTOCOL_GUID \
  { \
  0x39a68587, 0x8252, 0x4e57, { 0x8a, 0x92, 0x86, 0x70, 0x03, 0x68, 0x58, 0x13 } \
  }

#define SPANSION_MANUFACTURER_ID      0x1
#define SPANSION_SPI_NOR_INTERFACE_ID 0x2
#define SPANSION_FLASH_DENSITY_512    0x20
#define SPANSION_FLASH_DENSITY_256    0x19

//
// NOR Flash attributes
//
#define NOR_FLASH_NAME_SIZE           32


typedef struct {
  CHAR8                            FlashName[NOR_FLASH_NAME_SIZE];
  UINT8                            ManufacturerId;
  UINT8                            MemoryInterfaceType;
  UINT8                            MemoryDensity;
  UINT32                           SectorSize;
  UINT32                           NumSectors;
  UINT32                           BlockSize;
  UINT32                           PageSize;
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
(EFIAPI * NOR_FLASH_GET_ATTRIBUTES)(
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
(EFIAPI * NOR_FLASH_READ)(
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
(EFIAPI * NOR_FLASH_WRITE)(
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
(EFIAPI * NOR_FLASH_ERASE)(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Lba,
  IN UINT32                    NumLba
);

/// NVIDIA_NOR_FLASH_PROTOCOL protocol structure.
struct _NVIDIA_NOR_FLASH_PROTOCOL {
  EFI_FVB_ATTRIBUTES_2     FvbAttributes;
  NOR_FLASH_GET_ATTRIBUTES GetAttributes;
  NOR_FLASH_READ           Read;
  NOR_FLASH_WRITE          Write;
  NOR_FLASH_ERASE          Erase;
};

extern EFI_GUID gNVIDIANorFlashProtocolGuid;

#endif
