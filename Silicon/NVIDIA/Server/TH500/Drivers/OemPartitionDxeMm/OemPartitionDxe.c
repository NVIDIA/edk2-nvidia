/** @file
  Oem partition access DXE Sample wrapper.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "OemPartitionDxe.h"

EFI_MM_COMMUNICATION2_PROTOCOL  *mMmCommunication2 = NULL;
VOID                            *mMmCommBuffer     = NULL;
OEM_PARTITION_PROTOCOL          mOemPartitionProtocol;

/**
  Initialize the communicate buffer using DataSize and Function.

  @param[out]      DataPtr          Points to the data in the communicate buffer.
  @param[in]       DataSize         The data size to send to MM.
  @param[in]       Function         The function number to initialize the communicate header.

  @return Communicate buffer. It's caller's responsibility to release by calling FreePool().
**/
STATIC
VOID *
InitCommunicateBuffer (
  OUT     VOID   **DataPtr OPTIONAL,
  IN      UINTN  DataSize,
  IN      UINTN  Function
  )
{
  EFI_MM_COMMUNICATE_HEADER         *MmCommunicateHeader;
  OEM_PARTITION_COMMUNICATE_HEADER  *MmFunctionHeader;
  VOID                              *Buffer;

  Buffer = NULL;

  // Allocate the buffer for MM communication
  mMmCommBuffer = AllocateRuntimePool (
                    DataSize + OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) +
                    sizeof (OEM_PARTITION_COMMUNICATE_HEADER)
                    );
  if (mMmCommBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Buffer allocation for MM comm. failed\n", __FUNCTION__));
    return Buffer;
  }

  Buffer = mMmCommBuffer;
  ASSERT (Buffer != NULL);

  // Initialize EFI_MM_COMMUNICATE_HEADER structure
  MmCommunicateHeader = (EFI_MM_COMMUNICATE_HEADER *)mMmCommBuffer;
  CopyGuid (&MmCommunicateHeader->HeaderGuid, &gNVIDIAOemPartitionGuid);
  MmCommunicateHeader->MessageLength = DataSize + sizeof (OEM_PARTITION_COMMUNICATE_HEADER);

  MmFunctionHeader = (OEM_PARTITION_COMMUNICATE_HEADER *)MmCommunicateHeader->Data;
  ZeroMem (MmFunctionHeader, DataSize + sizeof (OEM_PARTITION_COMMUNICATE_HEADER));
  MmFunctionHeader->Function = Function;
  if (DataPtr != NULL) {
    *DataPtr = MmFunctionHeader + 1;
  }

  return Buffer;
}

/**
  Send the data in communicate buffer to MM.

  @param[in]   Buffer                 Points to the data in the communicate buffer.
  @param[in]   DataSize               The data size to send to MM.

  @retval      EFI_SUCCESS            Success is returned from the function in MM.
  @retval      Others                 Failure is returned from the function in MM.

**/
STATIC
EFI_STATUS
SendCommunicateBuffer (
  IN      VOID   *Buffer,
  IN      UINTN  DataSize
  )
{
  EFI_STATUS                        Status;
  UINTN                             CommSize;
  EFI_MM_COMMUNICATE_HEADER         *MmCommunicateHeader;
  OEM_PARTITION_COMMUNICATE_HEADER  *MmFunctionHeader;

  //
  // Locates EFI MM Communication 2 protocol.
  //
  if (mMmCommunication2 == NULL) {
    Status = gBS->LocateProtocol (&gEfiMmCommunication2ProtocolGuid, NULL, (VOID **)&mMmCommunication2);
    return Status;
  }

  CommSize = DataSize + OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data) + sizeof (OEM_PARTITION_COMMUNICATE_HEADER);
  if (mMmCommunication2 != NULL) {
    Status = mMmCommunication2->Communicate (mMmCommunication2, Buffer, Buffer, &CommSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Mm communicate failed!", __FUNCTION__));
      return Status;
    }
  }

  MmCommunicateHeader = (EFI_MM_COMMUNICATE_HEADER *)Buffer;
  MmFunctionHeader    = (OEM_PARTITION_COMMUNICATE_HEADER *)MmCommunicateHeader->Data;
  return MmFunctionHeader->ReturnStatus;
}

/**
  Read data from Oem partition.

  @param[out] Data                 Address to read data into
  @param[in]  Offset               Offset to read from
  @param[in]  Size                 Number of bytes to be read

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionRead (
  OUT VOID   *Data,
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS                      Status;
  VOID                            *Buffer;
  OEM_PARTITION_COMMUNICATE_READ  *ReadOemPartition;

  Buffer = NULL;
  Buffer = InitCommunicateBuffer (
             (VOID **)&ReadOemPartition,
             sizeof (*ReadOemPartition),
             OEM_PARTITION_FUNC_READ
             );
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ReadOemPartition->Offset = Offset;
  ReadOemPartition->Length = Length;

  Status = SendCommunicateBuffer (Buffer, sizeof (*ReadOemPartition));
  if (!EFI_ERROR (Status)) {
    CopyMem (Data, ReadOemPartition->Data, Length);
  }

  if (Buffer != NULL) {
    FreePool (Buffer);
    Buffer = NULL;
  }

  return Status;
}

/**
  Write data to Oem partition.

  @param[in] Data                  Address to write data from
  @param[in] Offset                Offset to read from
  @param[in] Size                  Number of bytes to write

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionWrite (
  IN VOID    *Data,
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS                       Status;
  VOID                             *Buffer;
  OEM_PARTITION_COMMUNICATE_WRITE  *WriteOemPartition;

  Buffer = NULL;
  Buffer = InitCommunicateBuffer (
             (VOID **)&WriteOemPartition,
             sizeof (*WriteOemPartition),
             OEM_PARTITION_FUNC_WRITE
             );
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  WriteOemPartition->Offset = Offset;
  WriteOemPartition->Length = Length;
  if (Data != NULL) {
    CopyMem (WriteOemPartition->Data, Data, Length);
  }

  Status = SendCommunicateBuffer (Buffer, sizeof (*WriteOemPartition));

  if (Buffer != NULL) {
    FreePool (Buffer);
    Buffer = NULL;
  }

  return Status;
}

/**
  Erase data block from Oem partition.

  @param[in] Offset                Offset to be erased
  @param[in] Length                Number of bytes to be erased

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionErase (
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS                       Status;
  VOID                             *Buffer;
  OEM_PARTITION_COMMUNICATE_ERASE  *EraseOemPartition;

  Buffer = NULL;
  Buffer = InitCommunicateBuffer (
             (VOID **)&EraseOemPartition,
             sizeof (*EraseOemPartition),
             OEM_PARTITION_FUNC_ERASE
             );
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  EraseOemPartition->Offset = Offset;
  EraseOemPartition->Length = Length;

  Status = SendCommunicateBuffer (Buffer, sizeof (*EraseOemPartition));

  if (Buffer != NULL) {
    FreePool (Buffer);
    Buffer = NULL;
  }

  return Status;
}

/**
   data erased check from Oem partition.

  @param[in] Offset                Offset to be checked
  @param[in] Length                Number of bytes to be checked

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionIsErased (
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS                       Status;
  VOID                             *Buffer;
  OEM_PARTITION_COMMUNICATE_ERASE  *EraseOemPartition;

  Buffer = NULL;
  Buffer = InitCommunicateBuffer (
             (VOID **)&EraseOemPartition,
             sizeof (*EraseOemPartition),
             OEM_PARTITION_FUNC_IS_ERASED
             );
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  EraseOemPartition->Offset = Offset;
  EraseOemPartition->Length = Length;

  Status = SendCommunicateBuffer (Buffer, sizeof (*EraseOemPartition));

  if (Buffer != NULL) {
    FreePool (Buffer);
    Buffer = NULL;
  }

  return Status;
}

/**
  Get Oem partition info.

  @param[out] PartitionBaseAddress Oem partition offset in SPI NOR.
  @param[out] PartitionSize      Size in bytes the partition.
  @param[out] BlockSize            The size in bytes of each block.
  @param[out] NumBlocks            Number of blocks in partition.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionInfo (
  UINT32  *PartitionBaseAddress,
  UINT32  *PartitionSize,
  UINT32  *BlockSize,
  UINT32  *NumBlocks
  )
{
  EFI_STATUS                          Status;
  VOID                                *Buffer;
  OEM_PARTITION_COMMUNICATE_GET_INFO  *GetInfo;

  Buffer = NULL;
  Buffer = InitCommunicateBuffer (
             (VOID **)&GetInfo,
             sizeof (*GetInfo),
             OEM_PARTITION_FUNC_GET_INFO
             );
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = SendCommunicateBuffer (Buffer, sizeof (*GetInfo));
  if (!EFI_ERROR (Status)) {
    *PartitionBaseAddress = GetInfo->PartitionBaseAddress;
    *PartitionSize        = GetInfo->PartitionSize;
    *BlockSize            = GetInfo->BlockSize;
    *NumBlocks            = GetInfo->NumBlocks;
  }

  if (Buffer != NULL) {
    FreePool (Buffer);
    Buffer = NULL;
  }

  return Status;
}

/**
  Oem partition Dxe entry point.

  @param ImageHandle     The image handle.
  @param SystemTable     The system table.

  @retval EFI_SUCCESS    The entry point is executed successfully.
  @return  other         Contain some other errors.

**/
EFI_STATUS
EFIAPI
OemPartitionEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Handle = NULL;

  ZeroMem (&mOemPartitionProtocol, sizeof (mOemPartitionProtocol));
  //
  // Locates EFI MM Communication 2 protocol.
  //
  Status = gBS->LocateProtocol (&gEfiMmCommunication2ProtocolGuid, NULL, (VOID **)&mMmCommunication2);
  ASSERT_EFI_ERROR (Status);

  mOemPartitionProtocol.Info     = OemPartitionInfo;
  mOemPartitionProtocol.Read     = OemPartitionRead;
  mOemPartitionProtocol.Write    = OemPartitionWrite;
  mOemPartitionProtocol.Erase    = OemPartitionErase;
  mOemPartitionProtocol.IsErased = OemPartitionIsErased;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gNVIDIAOemPartitionProtocolGuid,
                  &mOemPartitionProtocol,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
