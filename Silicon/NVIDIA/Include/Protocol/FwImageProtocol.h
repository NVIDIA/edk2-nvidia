/** @file
  FW Image Protocol

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FW_IMAGE_PROTOCOL_H__
#define __FW_IMAGE_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define NVIDIA_FW_IMAGE_PROTOCOL_GUID  {0xd836a4a8, 0xdb25, 0x44a7, \
  {0x9a, 0x3c, 0x9d, 0xb3, 0xd1, 0xb0, 0x23, 0x04}}

#define FW_IMAGE_MAX_IMAGES  50
#define FW_IMAGE_NAME_LENGTH                                    \
  (sizeof (((EFI_PARTITION_ENTRY *) 0)->PartitionName) /        \
   sizeof (((EFI_PARTITION_ENTRY *) 0)->PartitionName[0]))

// Flags for Read() and/or Write()
#define FW_IMAGE_RW_FLAG_NONE                 0x00000000
#define FW_IMAGE_RW_FLAG_READ_INACTIVE_IMAGE  0x00000001
#define FW_IMAGE_RW_FLAG_FORCE_PARTITION_A    0x00000002
#define FW_IMAGE_RW_FLAG_FORCE_PARTITION_B    0x00000004

typedef struct _NVIDIA_FW_IMAGE_PROTOCOL NVIDIA_FW_IMAGE_PROTOCOL;

// image attributes structure
typedef struct {
  UINTN     ReadBytes;
  UINTN     WriteBytes;
  UINT32    BlockSize;
} FW_IMAGE_ATTRIBUTES;

/**
  Read data from image.  Reads from active partition unless Flags are set:
    FW_IMAGE_RW_FLAG_READ_INACTIVE_IMAGE:   reads from inactive partition
    FW_IMAGE_RW_FLAG_FORCE_PARTITION_A:     reads from A partition
    FW_IMAGE_RW_FLAG_FORCE_PARTITION_B:     reads from B partition

  @param[in]  This              Instance to protocol
  @param[in]  Offset            Offset to read from
  @param[in]  Bytes             Number of bytes to read, must be a multiple
                                of FW_IMAGE_ATTRIBUTES.BlockSize.
  @param[out] Buffer            Address to read data into
  @param[in]  Flags             Flags for read operation

  @retval EFI_SUCCESS              Operation successful
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *FW_IMAGE_READ_IMAGE)(
  IN  NVIDIA_FW_IMAGE_PROTOCOL          *This,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  OUT VOID                              *Buffer,
  IN  UINTN                             Flags
  );

/**
  Write data to image.  Writes to inactive partition unless flags are set:
    FW_IMAGE_RW_FLAG_FORCE_PARTITION_A:     writes to A partition
    FW_IMAGE_RW_FLAG_FORCE_PARTITION_B:     writes to B partition

  Note: writes to an image's active partition are not allowed unless
        PcdOverwriteActiveFwPartition is TRUE

  @param[in]  This              Instance to protocol
  @param[in]  Offset            Offset to write
  @param[in]  Bytes             Number of bytes to write
  @param[in]  Buffer            Address of write data
  @param[in]  Flags             Flags for write operation

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *FW_IMAGE_WRITE_IMAGE)(
  IN  NVIDIA_FW_IMAGE_PROTOCOL          *This,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer,
  IN  UINTN                             Flags
  );

/**
  Get image attributes.

  @param[in]  This                 Instance to protocol
  @param[out] Attributes           Address to store image attributes

  @retval EFI_SUCCESS              Operation successful
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *FW_IMAGE_GET_ATTRIBUTES)(
  IN  NVIDIA_FW_IMAGE_PROTOCOL          *This,
  IN  FW_IMAGE_ATTRIBUTES               *Attributes
  );

// protocol structure
struct _NVIDIA_FW_IMAGE_PROTOCOL {
  CONST CHAR16               *ImageName;
  FW_IMAGE_READ_IMAGE        Read;
  FW_IMAGE_WRITE_IMAGE       Write;
  FW_IMAGE_GET_ATTRIBUTES    GetAttributes;
};

extern EFI_GUID  gNVIDIAFwImageProtocolGuid;

#endif
