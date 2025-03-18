/** @file

  Sequential record protocol/header definitions

  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SEQUENTIAL_RECORD_HDR_H
#define __SEQUENTIAL_RECORD_HDR_H

#include <Protocol/NorFlash.h>

#define MAX_SOCKETS  (4)

typedef struct _NVIDIA_SEQ_RECORD_PROTOCOL   NVIDIA_SEQ_RECORD_PROTOCOL;
typedef struct _NVIDIA_CMET_RECORD_PROTOCOL  NVIDIA_CMET_RECORD_PROTOCOL;

typedef struct {
  UINT8     Flags;
  UINT8     Reserved[2];
  UINT8     Crc8;
  UINT32    SizeBytes;
} DATA_HDR;

typedef struct {
  UINT64    PartitionByteOffset;
  UINT64    PartitionSize;
  UINT32    PartitionIndex;
} PARTITION_INFO;

typedef
EFI_STATUS
(EFIAPI *SEQ_REC_WRITE_NEXT)(
  IN  NVIDIA_SEQ_RECORD_PROTOCOL *This,
  IN  UINTN                      SocketNum,
  IN  VOID                       *Buf,
  IN  UINTN                      BufSize
  );

typedef
EFI_STATUS
(EFIAPI *SEQ_REC_READ_LAST)(
  IN  NVIDIA_SEQ_RECORD_PROTOCOL *This,
  IN  UINTN                      SocketNum,
  IN  VOID                       *Buf,
  IN  UINTN                      BufSize
  );

typedef
EFI_STATUS
(EFIAPI *SEQ_REC_ERASE_PARTITION)(
  IN  NVIDIA_SEQ_RECORD_PROTOCOL *This,
  IN  UINTN                      SocketNum
  );

typedef
EFI_STATUS
(EFIAPI *CMET_REC_WRITE)(
  IN  NVIDIA_CMET_RECORD_PROTOCOL *This,
  IN  UINTN                       SocketNum,
  IN  VOID                        *Buf,
  IN  UINTN                       BufSize,
  IN  UINTN                       Erase
  );

typedef
EFI_STATUS
(EFIAPI *CMET_REC_READ)(
  IN  NVIDIA_CMET_RECORD_PROTOCOL *This,
  IN  UINTN                       SocketNum,
  IN  VOID                        *Buf,
  IN  UINTN                       BufSize,
  IN  UINTN                       PrimaryRecord
  );

typedef
EFI_STATUS
(EFIAPI *SEQ_REC_READ_NTH_RECORD_FROM_END)(
  IN  NVIDIA_SEQ_RECORD_PROTOCOL *This,
  IN  UINTN                      SocketNum,
  IN  UINT32                     NthFromEnd,
  IN  VOID                       *Buf,
  IN  UINTN                      BufSize
  );

struct _NVIDIA_SEQ_RECORD_PROTOCOL {
  SEQ_REC_READ_LAST                   ReadLast;
  SEQ_REC_WRITE_NEXT                  WriteNext;
  SEQ_REC_ERASE_PARTITION             ErasePartition;
  SEQ_REC_READ_NTH_RECORD_FROM_END    ReadNthRecordFromEnd;
  PARTITION_INFO                      PartitionInfo;
  NVIDIA_NOR_FLASH_PROTOCOL           *NorFlashProtocol[MAX_SOCKETS];
};

struct _NVIDIA_CMET_RECORD_PROTOCOL {
  CMET_REC_READ                ReadRecord;
  CMET_REC_WRITE               WriteRecord;
  PARTITION_INFO               PartitionInfo;
  NVIDIA_NOR_FLASH_PROTOCOL    *NorFlashProtocol[MAX_SOCKETS];
};

#endif //  __SEQUENTIAL_RECORD_HDR_H
