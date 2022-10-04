/** @file

  MM FW partition protocol communication

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2010 - 2019, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "FwPartitionMmDxe.h"

EFI_STATUS
EFIAPI
MmInitCommBuffer (
  OUT     VOID   **DataPtr OPTIONAL,
  IN      UINTN  DataSize,
  IN      UINTN  Function
  )
{
  EFI_MM_COMMUNICATE_HEADER  *MmCommHeader;
  FW_PARTITION_COMM_HEADER   *FwImageCommHeader;

  if (DataSize + OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) +
      FW_PARTITION_COMM_HEADER_SIZE > FW_PARTITION_COMM_BUFFER_SIZE)
  {
    return EFI_INVALID_PARAMETER;
  }

  MmCommHeader = (EFI_MM_COMMUNICATE_HEADER *)mMmCommBuffer;
  CopyGuid (&MmCommHeader->HeaderGuid, &gNVIDIAFwPartitionProtocolGuid);
  MmCommHeader->MessageLength = DataSize + FW_PARTITION_COMM_HEADER_SIZE;

  FwImageCommHeader               = (FW_PARTITION_COMM_HEADER *)MmCommHeader->Data;
  FwImageCommHeader->Function     = Function;
  FwImageCommHeader->ReturnStatus = EFI_PROTOCOL_ERROR;
  if (DataPtr != NULL) {
    *DataPtr = FwImageCommHeader->Data;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MmSendCommBuffer (
  IN      UINTN  DataSize
  )
{
  EFI_STATUS                 Status;
  UINTN                      CommSize;
  EFI_MM_COMMUNICATE_HEADER  *CommHeader;
  FW_PARTITION_COMM_HEADER   *FwImageCommHeader;

  CommSize = DataSize + OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) +
             FW_PARTITION_COMM_HEADER_SIZE;

  DEBUG ((DEBUG_INFO, "%a: doing communicate\n", __FUNCTION__));
  Status = mMmCommProtocol->Communicate (
                              mMmCommProtocol,
                              mMmCommBufferPhysical,
                              mMmCommBuffer,
                              &CommSize
                              );
  DEBUG ((
    DEBUG_INFO,
    "%a: communicate returned: %r\n",
    __FUNCTION__,
    Status
    ));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CommHeader        = (EFI_MM_COMMUNICATE_HEADER *)mMmCommBuffer;
  FwImageCommHeader = (FW_PARTITION_COMM_HEADER *)CommHeader->Data;
  return FwImageCommHeader->ReturnStatus;
}

EFI_STATUS
EFIAPI
MmSendInitialize  (
  UINTN    ActiveBootChain,
  BOOLEAN  OverwriteActiveFwPartition
  )
{
  FW_PARTITION_COMM_INITIALIZE  *InitPayload;
  UINTN                         PayloadSize;
  EFI_STATUS                    Status;

  PayloadSize = sizeof (FW_PARTITION_COMM_INITIALIZE);
  Status      = MmInitCommBuffer (
                  (VOID **)&InitPayload,
                  PayloadSize,
                  FW_PARTITION_COMM_FUNCTION_INITIALIZE
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (InitPayload != NULL);

  ZeroMem (InitPayload, sizeof (*InitPayload));
  InitPayload->ActiveBootChain            = ActiveBootChain;
  InitPayload->OverwriteActiveFwPartition = OverwriteActiveFwPartition;

  Status = MmSendCommBuffer (PayloadSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error initializing MM: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}

EFI_STATUS
EFIAPI
MmSendGetPartitions  (
  IN  UINTN                           MaxCount,
  OUT FW_PARTITION_MM_PARTITION_INFO  *PartitionInfoBuffer,
  OUT UINTN                           *Count,
  OUT UINTN                           *BrBctEraseBlockSize
  )
{
  FW_PARTITION_COMM_GET_PARTITIONS  *GetPartitionsPayload;
  UINTN                             PayloadSize;
  EFI_STATUS                        Status;

  DEBUG ((DEBUG_INFO, "%a: entry\n", __FUNCTION__));

  PayloadSize = (MaxCount * sizeof (FW_PARTITION_MM_PARTITION_INFO)) +
                OFFSET_OF (FW_PARTITION_COMM_GET_PARTITIONS, Partitions);

  Status = MmInitCommBuffer (
             (VOID **)&GetPartitionsPayload,
             PayloadSize,
             FW_PARTITION_COMM_FUNCTION_GET_PARTITIONS
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (GetPartitionsPayload != NULL);

  ZeroMem (GetPartitionsPayload, sizeof (*GetPartitionsPayload));
  GetPartitionsPayload->MaxCount = MaxCount;

  Status = MmSendCommBuffer (PayloadSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting MM image names: %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  ASSERT (GetPartitionsPayload->Count <= MaxCount);

  *BrBctEraseBlockSize = GetPartitionsPayload->BrBctEraseBlockSize;
  *Count               = GetPartitionsPayload->Count;
  CopyMem (
    PartitionInfoBuffer,
    GetPartitionsPayload->Partitions,
    GetPartitionsPayload->Count * sizeof (GetPartitionsPayload->Partitions[0])
    );

  return Status;
}

EFI_STATUS
EFIAPI
MmSendReadData (
  IN  CONST CHAR16  *Name,
  IN  UINT64        Offset,
  IN  UINTN         Bytes,
  IN  VOID          *Buffer
  )
{
  EFI_STATUS                   Status;
  FW_PARTITION_COMM_READ_DATA  *ReadDataPayload;
  UINTN                        PayloadSize;

  PayloadSize = OFFSET_OF (FW_PARTITION_COMM_READ_DATA, Data) + Bytes;
  Status      = MmInitCommBuffer (
                  (VOID **)&ReadDataPayload,
                  PayloadSize,
                  FW_PARTITION_COMM_FUNCTION_READ_DATA
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (ReadDataPayload != NULL);

  ZeroMem (ReadDataPayload, sizeof (*ReadDataPayload));
  Status = StrnCpyS (
             ReadDataPayload->Name,
             sizeof (ReadDataPayload->Name),
             Name,
             StrLen (Name)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ReadDataPayload->Offset = Offset;
  ReadDataPayload->Bytes  = Bytes;

  Status = MmSendCommBuffer (PayloadSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: read of %s Offset=%llu Bytes=%u failed: %r\n",
      __FUNCTION__,
      Name,
      Offset,
      Bytes,
      Status
      ));
    return Status;
  }

  CopyMem (Buffer, ReadDataPayload->Data, Bytes);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MmSendWriteData (
  IN  CONST CHAR16  *Name,
  IN  UINT64        Offset,
  IN  UINTN         Bytes,
  IN  CONST VOID    *Buffer
  )
{
  EFI_STATUS                    Status;
  FW_PARTITION_COMM_WRITE_DATA  *WriteDataPayload;
  UINTN                         PayloadSize;

  PayloadSize = OFFSET_OF (FW_PARTITION_COMM_WRITE_DATA, Data) + Bytes;
  Status      = MmInitCommBuffer (
                  (VOID **)&WriteDataPayload,
                  PayloadSize,
                  FW_PARTITION_COMM_FUNCTION_WRITE_DATA
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (WriteDataPayload != NULL);

  ZeroMem (WriteDataPayload, sizeof (*WriteDataPayload));
  Status = StrnCpyS (
             WriteDataPayload->Name,
             sizeof (WriteDataPayload->Name),
             Name,
             StrLen (Name)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  WriteDataPayload->Offset = Offset;
  WriteDataPayload->Bytes  = Bytes;
  CopyMem (WriteDataPayload->Data, Buffer, Bytes);

  Status = MmSendCommBuffer (PayloadSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: write of %s Offset=%llu Bytes=%u failed: %r\n",
      __FUNCTION__,
      Name,
      Offset,
      Bytes,
      Status
      ));
  }

  return Status;
}
