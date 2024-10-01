/** @file

  FW partition standalone MM

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/MmServicesTableLib.h>

#include "FwPartitionMmDxe.h"

EFI_STATUS
EFIAPI
FwPartitionNorFlashStmmInitialize (
  UINTN    ActiveBootChain,
  BOOLEAN  OverwriteActiveFwPartition,
  UINTN    ChipId
  );

EFI_STATUS
EFIAPI
FwPartitionMmHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  FW_PARTITION_COMM_HEADER  *FwImageCommHeader;
  UINTN                     PayloadSize;
  EFI_STATUS                Status;

  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_INVALID_PARAMETER));
    return EFI_SUCCESS;
  }

  if (*CommBufferSize < sizeof (FW_PARTITION_COMM_HEADER)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_BUFFER_TOO_SMALL));
    return EFI_SUCCESS;
  }

  FwImageCommHeader = (FW_PARTITION_COMM_HEADER *)CommBuffer;

  PayloadSize = *CommBufferSize - FW_PARTITION_COMM_HEADER_SIZE;

  DEBUG ((DEBUG_INFO, "%a: Func=%u\n", __FUNCTION__, FwImageCommHeader->Function));

  switch (FwImageCommHeader->Function) {
    case FW_PARTITION_COMM_FUNCTION_INITIALIZE:
    {
      FW_PARTITION_COMM_INITIALIZE  *InitPayload;
      if (PayloadSize < sizeof (FW_PARTITION_COMM_INITIALIZE)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Command [%d], payload buffer : %r!\n",
          __FUNCTION__,
          FwImageCommHeader->Function,
          EFI_INVALID_PARAMETER
          ));
        FwImageCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      InitPayload = (FW_PARTITION_COMM_INITIALIZE *)FwImageCommHeader->Data;
      Status      = FwPartitionNorFlashStmmInitialize (
                      InitPayload->ActiveBootChain,
                      InitPayload->OverwriteActiveFwPartition,
                      InitPayload->ChipId
                      );

      FwImageCommHeader->ReturnStatus = Status;
      break;
    }

    case FW_PARTITION_COMM_FUNCTION_GET_PARTITIONS:
    {
      FW_PARTITION_COMM_GET_PARTITIONS  *ImagesPayload;
      UINTN                             Index;
      UINTN                             NumImages;
      FW_PARTITION_PRIVATE_DATA         *Partition;
      UINT32                            EraseBlockSize;

      NumImages     = FwPartitionGetCount ();
      ImagesPayload = (FW_PARTITION_COMM_GET_PARTITIONS *)FwImageCommHeader->Data;
      if (PayloadSize != OFFSET_OF (FW_PARTITION_COMM_GET_PARTITIONS, Partitions) +
          (ImagesPayload->MaxCount * sizeof (ImagesPayload->Partitions[0])))
      {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Command [%d], payload buffer : %r!\n",
          __FUNCTION__,
          FwImageCommHeader->Function,
          EFI_INVALID_PARAMETER
          ));
        FwImageCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      if (NumImages > ImagesPayload->MaxCount) {
        FwImageCommHeader->ReturnStatus = EFI_BUFFER_TOO_SMALL;
        break;
      }

      EraseBlockSize = 0;
      Partition      = FwPartitionGetPrivateArray ();
      for (Index = 0; Index < NumImages; Index++, Partition++) {
        FW_PARTITION_MM_PARTITION_INFO  *ImageInfo = &ImagesPayload->Partitions[Index];

        CopyMem (
          ImageInfo->Name,
          Partition->PartitionInfo.Name,
          StrSize (Partition->PartitionInfo.Name)
          );
        ImageInfo->Bytes = Partition->PartitionInfo.Bytes;

        if (StrCmp (ImageInfo->Name, L"BCT") == 0) {
          EraseBlockSize = Partition->DeviceInfo->BlockSize;
        }
      }

      ImagesPayload->Count               = NumImages;
      ImagesPayload->BrBctEraseBlockSize = EraseBlockSize;
      FwImageCommHeader->ReturnStatus    = EFI_SUCCESS;
      break;
    }

    case FW_PARTITION_COMM_FUNCTION_READ_DATA:
    {
      FW_PARTITION_COMM_READ_DATA  *ReadDataPayload;
      FW_PARTITION_PRIVATE_DATA    *Partition;
      FW_PARTITION_DEVICE_INFO     *DeviceInfo;
      if (PayloadSize < sizeof (FW_PARTITION_COMM_READ_DATA)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Command [%d], payload buffer : %r!\n",
          __FUNCTION__,
          FwImageCommHeader->Function,
          EFI_INVALID_PARAMETER
          ));
        FwImageCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      ReadDataPayload = (FW_PARTITION_COMM_READ_DATA *)FwImageCommHeader->Data;
      if (PayloadSize != OFFSET_OF (FW_PARTITION_COMM_READ_DATA, Data) + ReadDataPayload->Bytes) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Command [%d], payload buffer : %r!\n",
          __FUNCTION__,
          FwImageCommHeader->Function,
          EFI_INVALID_PARAMETER
          ));
        FwImageCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      DEBUG ((
        DEBUG_INFO,
        "%a: reading %s offset=%u bytes=%u\n",
        __FUNCTION__,
        ReadDataPayload->Name,
        ReadDataPayload->Offset,
        ReadDataPayload->Bytes
        ));

      Partition = FwPartitionFindByName (ReadDataPayload->Name);
      if (Partition == NULL) {
        FwImageCommHeader->ReturnStatus = EFI_NOT_FOUND;
        break;
      }

      DeviceInfo = Partition->DeviceInfo;
      Status     = DeviceInfo->DeviceRead (
                                 Partition->PartitionInfo.Name,
                                 DeviceInfo,
                                 Partition->PartitionInfo.Offset + ReadDataPayload->Offset,
                                 ReadDataPayload->Bytes,
                                 ReadDataPayload->Data
                                 );

      FwImageCommHeader->ReturnStatus = Status;
      break;
    }

    case FW_PARTITION_COMM_FUNCTION_WRITE_DATA:
    {
      FW_PARTITION_COMM_WRITE_DATA  *WriteDataPayload;
      FW_PARTITION_PRIVATE_DATA     *Partition;
      FW_PARTITION_DEVICE_INFO      *DeviceInfo;
      if (PayloadSize < sizeof (FW_PARTITION_COMM_WRITE_DATA)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Command [%d], payload buffer : %r!\n",
          __FUNCTION__,
          FwImageCommHeader->Function,
          EFI_INVALID_PARAMETER
          ));
        FwImageCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      WriteDataPayload = (FW_PARTITION_COMM_WRITE_DATA *)FwImageCommHeader->Data;
      if (PayloadSize != OFFSET_OF (FW_PARTITION_COMM_WRITE_DATA, Data) + WriteDataPayload->Bytes) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Command [%d], payload buffer : %r!\n",
          __FUNCTION__,
          FwImageCommHeader->Function,
          EFI_INVALID_PARAMETER
          ));
        FwImageCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
        break;
      }

      DEBUG ((
        DEBUG_INFO,
        "%a: writing  %s offset=%u bytes=%u\n",
        __FUNCTION__,
        WriteDataPayload->Name,
        WriteDataPayload->Offset,
        WriteDataPayload->Bytes
        ));

      Partition = FwPartitionFindByName (WriteDataPayload->Name);
      if (Partition == NULL) {
        FwImageCommHeader->ReturnStatus = EFI_NOT_FOUND;
        break;
      }

      DeviceInfo = Partition->DeviceInfo;
      Status     = DeviceInfo->DeviceWrite (
                                 Partition->PartitionInfo.Name,
                                 DeviceInfo,
                                 Partition->PartitionInfo.Offset + WriteDataPayload->Offset,
                                 WriteDataPayload->Bytes,
                                 WriteDataPayload->Data
                                 );

      FwImageCommHeader->ReturnStatus = Status;
      break;
    }

    default:
      FwImageCommHeader->ReturnStatus = EFI_INVALID_PARAMETER;
      break;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Func=%u ReturnStatus=%u\n",
    __FUNCTION__,
    FwImageCommHeader->Function,
    FwImageCommHeader->ReturnStatus
    ));

  return EFI_SUCCESS;
}

/**
  Initialize the FW partition standalone MM driver

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

**/
EFI_STATUS
EFIAPI
FwPartitionStandaloneMmInitialize (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_HANDLE  Handle;
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "%a: Entry\n", __FUNCTION__));

  Handle = NULL;
  Status = gMmst->MmiHandlerRegister (
                    FwPartitionMmHandler,
                    &gNVIDIAFwPartitionProtocolGuid,
                    &Handle
                    );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
