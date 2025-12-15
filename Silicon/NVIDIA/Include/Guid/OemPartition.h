/** @file
  GUID is for Oem partition MM communication.

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __OEM_PARTITION_GUID_H__
#define __OEM_PARTITION_GUID_H__

extern EFI_GUID  gNVIDIAOemPartitionGuid;

#define OEM_PARTITION_FUNC_GET_INFO   1
#define OEM_PARTITION_FUNC_READ       2
#define OEM_PARTITION_FUNC_WRITE      3
#define OEM_PARTITION_FUNC_ERASE      4
#define OEM_PARTITION_FUNC_IS_ERASED  5

typedef struct {
  UINTN         Function;
  EFI_STATUS    ReturnStatus;
} OEM_PARTITION_COMMUNICATE_HEADER;

typedef struct {
  UINT32    PartitionBaseAddress;
  UINT32    PartitionSize;
  UINT32    BlockSize;
  UINT32    NumBlocks;
} OEM_PARTITION_COMMUNICATE_GET_INFO;

typedef struct {
  UINT32    Offset;
  UINT32    Length;
  UINT8     Data[0];
} OEM_PARTITION_COMMUNICATE_READ;

typedef struct {
  UINT32    Offset;
  UINT32    Length;
  UINT8     Data[0];
} OEM_PARTITION_COMMUNICATE_WRITE;

typedef struct {
  UINT32    Offset;
  UINT32    Length;
} OEM_PARTITION_COMMUNICATE_ERASE;

typedef struct {
  UINT32    Offset;
  UINT32    Length;
} OEM_PARTITION_COMMUNICATE_IS_ERASED;

typedef union {
  OEM_PARTITION_COMMUNICATE_GET_INFO     Info;
  OEM_PARTITION_COMMUNICATE_ERASE        Erase;
  OEM_PARTITION_COMMUNICATE_IS_ERASED    IsErased;
} OEM_PARTITION_COMMUNICATE_BUFFER;

#endif
