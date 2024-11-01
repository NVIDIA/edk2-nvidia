/** @file
  PRM Module Static Data

  SPDX-FileCopyrightText: Copyright (c) 2024-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PRM_RAS_MODULE_H_
#define PRM_RAS_MODULE_H_

#include <Base.h>

//
// PRM Handler GUIDs
//
// {ad16d36e-1933-480e-9b52-d17de5b4e632}
#define NVIDIA_RAS_PRM_HANDLER_GUID  {0xad16d36e, 0x1933, 0x480e, {0x9b, 0x52, 0xd1, 0x7d, 0xe5, 0xb4, 0xe6, 0x32}}

#define PRM_SPI_ACCESS_DATA_SIZE       (64 * 1024)
#define PRM_MM_RAS_PARTITION_OFFSET    0x00
#define PRM_MM_CMET_PARTITION_OFFSET   0x01
#define PRM_MM_EARLY_BOOT_VARS_OFFSET  0x02

#pragma pack(1)

typedef struct {
  UINT64    Offset : 56;
  UINT64    Select : 8;
} PARTITION_COMMAND;

typedef struct {
  UINT64               PartitionSize;
  PARTITION_COMMAND    PartitionCommand;
  UINT32               DataSize;
  UINT8                CperData[PRM_SPI_ACCESS_DATA_SIZE];
} PRM_RAS_MODULE_STATIC_DATA_CONTEXT_BUFFER;

#pragma pack()

#endif
