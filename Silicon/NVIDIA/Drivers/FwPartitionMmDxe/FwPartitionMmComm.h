/** @file

  MM FW partition protocol communication

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2010 - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef __FW_PARTITION_MM_COMM_H__
#define __FW_PARTITION_MM_COMM_H__

#define FW_PARTITION_COMM_BUFFER_SIZE (65 * 1024)
#define FW_PARTITION_COMM_HEADER_SIZE (OFFSET_OF (FW_PARTITION_COMM_HEADER, Data))

//
// FW partition protocol MM communications function codes
// Each function's payload structure type is the same label without _FUNCTION_
//
#define FW_PARTITION_COMM_FUNCTION_NOOP             0
#define FW_PARTITION_COMM_FUNCTION_INITIALIZE       1
#define FW_PARTITION_COMM_FUNCTION_GET_PARTITIONS   2
#define FW_PARTITION_COMM_FUNCTION_READ_DATA        3
#define FW_PARTITION_COMM_FUNCTION_WRITE_DATA       4

typedef struct {
  UINTN       Function;
  EFI_STATUS  ReturnStatus;
  UINT8       Data[1];
} FW_PARTITION_COMM_HEADER;

typedef struct {
  CHAR16                            Name[FW_PARTITION_NAME_LENGTH];
  UINTN                             Bytes;
} FW_PARTITION_MM_PARTITION_INFO;

//
// MM communication function payloads
//
typedef struct {
  // request fields
  UINTN                             ActiveBootChain;
  BOOLEAN                           OverwriteActiveFwPartition;

} FW_PARTITION_COMM_INITIALIZE;

typedef struct {
  // request fields
  UINTN                             MaxCount;
  // reply fields
  UINTN                             BrBctEraseBlockSize;
  UINTN                             Count;
  FW_PARTITION_MM_PARTITION_INFO    Partitions[1];

} FW_PARTITION_COMM_GET_PARTITIONS;

typedef struct {
  // request fields
  CHAR16        Name[FW_PARTITION_NAME_LENGTH];
  UINT64        Offset;
  UINTN         Bytes;
  // reply fields
  UINT8         Data[1];
} FW_PARTITION_COMM_READ_DATA;

typedef struct {
  // request fields
  CHAR16        Name[FW_PARTITION_NAME_LENGTH];
  UINT64        Offset;
  UINTN         Bytes;
  UINT8         Data[1];
} FW_PARTITION_COMM_WRITE_DATA;

EFI_STATUS
EFIAPI
MmInitCommBuffer (
  OUT     VOID                  **DataPtr OPTIONAL,
  IN      UINTN                 DataSize,
  IN      UINTN                 Function
  );

EFI_STATUS
EFIAPI
MmSendCommBuffer (
  IN      UINTN                 DataSize
  );

EFI_STATUS
EFIAPI
MmSendInitialize  (
  UINTN     ActiveBootChain,
  BOOLEAN   OverwriteActiveFwPartitions
  );

EFI_STATUS
EFIAPI
MmSendGetPartitions  (
  IN  UINTN                             MaxCount,
  OUT FW_PARTITION_MM_PARTITION_INFO    *PartitionInfoBuffer,
  OUT UINTN                             *Count,
  OUT UINTN                             *BrBctEraseBlockSize
  );

EFI_STATUS
EFIAPI
MmSendReadData (
  IN  CONST CHAR16                      *Name,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  IN VOID                               *Buffer
  );

EFI_STATUS
EFIAPI
MmSendWriteData (
  IN  CONST CHAR16                      *Name,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes,
  IN  CONST VOID                        *Buffer
  );

extern EFI_MM_COMMUNICATION2_PROTOCOL   *mMmCommProtocol;
extern VOID                             *mMmCommBuffer;
extern VOID                             *mMmCommBufferPhysical;

#endif
