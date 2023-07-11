/** @file
  NVIDIA Oem Partition Sample Driver Internal header

  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _OEM_PARTITION_MM_INTERNAL_H_
#define _OEM_PARTITION_MM_INTERNAL_H_

#include <PiMm.h>
#include <Protocol/NorFlash.h>

#define SOCKET_0_NOR_FLASH  (0)
#define ERASE_BYTE          (0xFF)

typedef struct {
  EFI_HANDLE                   Handle;                // Handle for Oem partition protocol
  NVIDIA_NOR_FLASH_PROTOCOL    *NorFlashProtocol;     // Protocol for writing the SPINOR
  NOR_FLASH_ATTRIBUTES         NorAttributes;         // Attributes of the SPINOR
  UINT32                       PartitionBaseAddress;
  UINT32                       PartitionSize;
  UINT32                       BlockSize;
  UINT32                       NumBlocks;
} OEM_PARTITION_PRIVATE_INFO;

#endif
