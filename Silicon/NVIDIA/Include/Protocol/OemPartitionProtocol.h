/** @file
  NVIDIA Oem Partition Sample Protocol header

  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _OEM_PARTITION_PROTOCOL_H_
#define _OEM_PARTITION_PROTOCOL_H_

/**
  Get Oem partition info.

  @param[out] PartitionBaseAddress Oem partition offset in SPI NOR.
  @param[out] PartitionSize        Size in bytes of the partition.
  @param[out] BlockSize            The size in bytes of each block.
  @param[out] NumBlocks            Number of blocks in partition.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *OEM_PARTITION_INFO)(
  UINT32  *PartitionBaseAddress,
  UINT32  *PartitionSize,
  UINT32  *BlockSize,
  UINT32  *NumBlocks
  );

/**
  Read data from Oem partition.

  @param[out] Data                 Address to read data into
  @param[in]  Offset               Offset to read from
  @param[in]  Size                 Number of bytes to be read

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *OEM_PARTITION_READ)(
  OUT VOID   *Data,
  IN UINT32  Offset,
  IN UINT32  Length
  );

/**
  Write data to Oem partition.

  @param[in] Data                  Address to write data from
  @param[in] Offset                Offset to read from
  @param[in] Size                  Number of bytes to write

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *OEM_PARTITION_WRITE)(
  IN VOID    *Data,
  IN UINT32  Offset,
  IN UINT32  Length
  );

/**
  Erase data block from Oem partition.

  @param[in] Offset                Offset to be erased
  @param[in] Length                Number of bytes to be erased

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *OEM_PARTITION_ERASE)(
  IN UINT32  Offset,
  IN UINT32  Length
  );

/**
  Data erased check from Oem partition.

  @param[in] Offset                Offset to be checked
  @param[in] Length                Number of bytes to be checked

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *OEM_PARTITION_IS_ERASED)(
  IN UINT32  Offset,
  IN UINT32  Length
  );

typedef struct {
  OEM_PARTITION_INFO         Info;
  OEM_PARTITION_READ         Read;
  OEM_PARTITION_WRITE        Write;
  OEM_PARTITION_ERASE        Erase;
  OEM_PARTITION_IS_ERASED    IsErased;
} OEM_PARTITION_PROTOCOL;

#endif
