/** @file
  NVIDIA Nor Flash Protocol

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __NVIDIA_NOR_FLASH_PROTOCOL_H__
#define __NVIDIA_NOR_FLASH_PROTOCOL_H__


#include <Pi/PiFirmwareVolume.h>


#define NVIDIA_NOR_FLASH_PROTOCOL_GUID \
  { \
  0x9545a4b9, 0x0e8a, 0x43db, { 0xbe, 0x00, 0xed, 0xc0, 0x6f, 0xe0, 0x81, 0xf7 } \
  }

typedef struct {
  UINT64                           UniformMemoryDensity;
  UINT32                           UniformBlockSize;
  UINT64                           HybridMemoryDensity;
  UINT32                           HybridBlockSize;
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
  @param[in] Hybrid                Use hybrid region

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI * NOR_FLASH_ERASE)(
  IN NVIDIA_NOR_FLASH_PROTOCOL *This,
  IN UINT32                    Lba,
  IN UINT32                    NumLba,
  IN BOOLEAN                   Hybrid
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
