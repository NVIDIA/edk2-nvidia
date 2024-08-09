/** @file
  FW Partition Protocol

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FW_PARTITION_PROTOCOL_H__
#define __FW_PARTITION_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define NVIDIA_FW_PARTITION_PROTOCOL_GUID  {0x52771b87, 0x204a, 0x4d7b, \
    {0xab, 0x5c, 0xbe, 0xf8, 0x70, 0x1e, 0x84, 0x16}}

#define FW_PARTITION_NAME_LENGTH                                \
  (sizeof (((EFI_PARTITION_ENTRY *) 0)->PartitionName) /        \
   sizeof (((EFI_PARTITION_ENTRY *) 0)->PartitionName[0]))

// pseudo-partition to update meta-data of inactive partitions
#define FW_PARTITION_UPDATE_INACTIVE_PARTITIONS  L"update_inactive_partitions"

typedef struct _NVIDIA_FW_PARTITION_PROTOCOL NVIDIA_FW_PARTITION_PROTOCOL;

// partition attributes structure
typedef struct {
  UINTN     Bytes;
  UINT32    BlockSize;
} FW_PARTITION_ATTRIBUTES;

/**
  Read data from partition.

  @param[in]  This                 Instance to protocol
  @param[in]  Offset               Offset to read from
  @param[in]  Bytes                Number of bytes to read, must be a multiple
                                   of FW_PARTITION_ATTRIBUTES.BlockSize.
  @param[out] Buffer               Address to read data into

  @retval EFI_SUCCESS              Operation successful
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *FW_PARTITION_READ)(
  IN  NVIDIA_FW_PARTITION_PROTOCOL      *This,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  OUT VOID                              *Buffer
  );

/**
  Write data to partition.

  @param[in] This                  Instance to protocol
  @param[in] Offset                Offset to write
  @param[in] Bytes                 Number of bytes to write
  @param[in] Buffer                Address of write data

  @retval EFI_SUCCESS              Operation successful
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *FW_PARTITION_WRITE)(
  IN  NVIDIA_FW_PARTITION_PROTOCOL      *This,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer
  );

/**
  Get partition attributes.

  @param[in]  This                 Instance to protocol
  @param[out] Attributes           Address to store partition attributes

  @retval EFI_SUCCESS              Operation successful
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *FW_PARTITION_GET_ATTRIBUTES)(
  IN  NVIDIA_FW_PARTITION_PROTOCOL      *This,
  OUT FW_PARTITION_ATTRIBUTES           *Attributes
  );

// protocol structure
struct _NVIDIA_FW_PARTITION_PROTOCOL {
  CONST CHAR16                   *PartitionName;
  FW_PARTITION_GET_ATTRIBUTES    GetAttributes;
  FW_PARTITION_READ              Read;
  FW_PARTITION_READ              PrmRead;
  FW_PARTITION_WRITE             Write;
};

extern EFI_GUID  gNVIDIAFwPartitionProtocolGuid;

#endif
