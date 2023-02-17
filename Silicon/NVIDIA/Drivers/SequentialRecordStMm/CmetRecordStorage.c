/** @file

  MM driver to write Sequential records to Flash.
  This file handles the storage portions.

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SequentialRecordPrivate.h"

#define ERASE_BYTE            (0xFF)
#define SOCKET_0_NOR_FLASH    (0)
#define MIN_PARTITION_BLOCKS  (2)
#define NUM_CMET_RECORDS      (2)

STATIC NOR_FLASH_ATTRIBUTES  NorFlashAttributes;
STATIC UINT32                CmetBlockSize = SIZE_64KB;

/**
 * Erase CMET record.
 *
 * @param[in]       Partition          Partition info.
 * @param[in]       NorFlashProtocol   NorFlash Protocol.
 * @param[in]       SocketNum          Specify which SPI-NOR to write to.
 * @param[in]       ActiveBlockNum     Active block to be erased.
 *
 * @retval       EFI_SUCCESS   Block Erased.
 *               OTHER         NOR FLASH Transaction error.
 */
STATIC
EFI_STATUS
EraseRecord (
  IN UINT32                     RecordOffset,
  IN NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol
  )
{
  EFI_STATUS  Status;
  UINT32      EraseBlocks;
  UINT32      EraseBlockNum;

  EraseBlocks   = CmetBlockSize / NorFlashAttributes.BlockSize;
  EraseBlockNum = (RecordOffset) / NorFlashAttributes.BlockSize;

  DEBUG ((
    DEBUG_ERROR,
    "%a:%d Erasing at %u %u blocks \n",
    __FUNCTION__,
    __LINE__,
    EraseBlockNum,
    EraseBlocks
    ));
  Status = NorFlashProtocol->Erase (
                               NorFlashProtocol,
                               EraseBlockNum,
                               EraseBlocks
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to erase LBA %u %r\n", EraseBlockNum, Status));
  }

  return Status;
}

/**
 * CmetReadRecord.
 *  Read the CMET record, the caller specifices if the record to be read
 *  is the primary record or not.
 *
 * @param[in]   This          Pointer to CMET Record Proto.
 * @param[in]   SocketNum     Specify which SPI-NOR to write to.
 * @param[out]  Buf           Buffer to read into.
 * @param[in]   BufSize       Size of read buffer.
 * @param[in]   PrimaryRecord 1 means read the primary record
 *                            0 read the secondary record.
 *
 * @retval       EFI_SUCCESS               Read back last active record.
 *               EFI_INVALID_PARAMETER     Invalid buffer size
 *               EFI_NOT_FOUND             No valid records found.
 *               EFI_DEVICE_ERROR          Can't find the NOR Flash device.
 *               Other                     NOR Flash Transaction fail.
 */
STATIC
EFI_STATUS
EFIAPI
CmetReadRecord (
  IN  NVIDIA_CMET_RECORD_PROTOCOL  *This,
  IN  UINTN                        SocketNum,
  OUT VOID                         *Buf,
  IN  UINTN                        BufSize,
  IN  UINTN                        PrimaryRecord
  )
{
  EFI_STATUS                 Status;
  UINT32                     CmetRecordReadOffset;
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;

  if (BufSize > CmetBlockSize) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Can only read one CMET record (max %u bytes)\n",
      __FUNCTION__,
      CmetBlockSize
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitCmetReadRecord;
  }

  if (SocketNum >= MAX_SOCKETS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Invalid SocketNumber %u \n",
      __FUNCTION__,
      SocketNum
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitCmetReadRecord;
  }

  NorFlashProtocol = This->NorFlashProtocol[SocketNum];
  if (NorFlashProtocol == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get NorFlashProtocol for %u\n",
      __FUNCTION__,
      SocketNum
      ));
    Status = EFI_DEVICE_ERROR;
    goto ExitCmetReadRecord;
  }

  CmetRecordReadOffset = This->PartitionInfo.PartitionByteOffset;
  if (PrimaryRecord == 0) {
    CmetRecordReadOffset += CmetBlockSize;
  }

  Status = NorFlashProtocol->Read (
                               NorFlashProtocol,
                               CmetRecordReadOffset,
                               BufSize,
                               Buf
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to read Block at %u header %r\n",
      __FUNCTION__,
      CmetRecordReadOffset,
      Status
      ));
    goto ExitCmetReadRecord;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Read Record at %u Socket %u\n",
    __FUNCTION__,
    CmetRecordReadOffset,
    SocketNum
    ));
ExitCmetReadRecord:
  return Status;
}

/**
 * Write CMET Record.
 * For CMET records, this function just writes the first 32k block
 *
 *
 * @param[in]    This          Pointer to CMET Record Proto.
 * @param[in]    SocketNum     Specify which SPI-NOR to write to.
 * @param[in]    InBuf         Input Write buffer.
 * @param[in]    BufSize       Size of the write buffer.
 * @param[in]    Erase         1 Erase the record before writing.
 *                             0 Don't Erase the record before writing
 *
 * @retval       EFI_SUCCESS            Write Record succeeded.
 *               EFI_INVALID_PARAMETER  Invalid Socket num.
 *               EFI_DEVICE_ERROR       Failed to find Nor Flash Protocol for
 *                                      socket provided.
 *               Other                  NOR Flash Transaction fail.
 */
STATIC
EFI_STATUS
EFIAPI
CmetWriteRecord (
  IN  NVIDIA_CMET_RECORD_PROTOCOL  *This,
  IN  UINTN                        SocketNum,
  IN  VOID                         *InBuf,
  IN  UINTN                        BufSize,
  IN  UINTN                        Erase
  )
{
  EFI_STATUS                 Status;
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;
  UINT32                     CmetOffset;
  UINTN                      RecordIdx;

  if (SocketNum >= MAX_SOCKETS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Invalid SocketNumber %u \n",
      __FUNCTION__,
      SocketNum
      ));
    return EFI_INVALID_PARAMETER;
  }

  NorFlashProtocol = This->NorFlashProtocol[SocketNum];
  if (NorFlashProtocol == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get NorFlashProtocol for %u\n",
      __FUNCTION__,
      SocketNum
      ));
    return EFI_DEVICE_ERROR;
  }

  CmetOffset = This->PartitionInfo.PartitionByteOffset;

  for (RecordIdx = 0; RecordIdx < NUM_CMET_RECORDS; RecordIdx++) {
    if (Erase == 1) {
      Status = EraseRecord (CmetOffset, NorFlashProtocol);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a:Failed to Erase Record %u %r\n",
          __FUNCTION__,
          RecordIdx,
          Status
          ));
        goto ExitCmetWriteRecord;
      }
    }

    Status = NorFlashProtocol->Write (
                                 NorFlashProtocol,
                                 CmetOffset,
                                 BufSize,
                                 InBuf
                                 );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to Write Cmet %u Record %r\n",
        __FUNCTION__,
        RecordIdx,
        Status
        ));
      goto ExitCmetWriteRecord;
    }

    CmetOffset += CmetBlockSize;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Wrote Records Size %u at %u Socket %u \n",
    __FUNCTION__,
    BufSize,
    CmetOffset,
    SocketNum
    ));
ExitCmetWriteRecord:
  return Status;
}

/**
 * Validate the partition size/offset that we've gotten, the size/offset has to
 * be erase block aligned and the size has to be atleast 2 erase blocks
 * for wear leveling.
 *
 *
 * @param[in]  PartitionInfo   Partition information to check.(offset/size).
 *
 * @retval     EFI_SUCCESS            The Partition info is valid and can
 *                                    be written.
 *             EFI_INVALID_PARAMETER  The Partition size/offset isn't valid.
 **/
EFI_STATUS
EFIAPI
ValidatePartitionInfo (
  IN PARTITION_INFO  *Partition
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;

  if ((NorFlashAttributes.BlockSize > CmetBlockSize) ||
      ((CmetBlockSize % NorFlashAttributes.BlockSize) != 0))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid CmetBlockSize %u EraseSize %u\n",
      __FUNCTION__,
      CmetBlockSize,
      NorFlashAttributes.BlockSize
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitValidatePartitionInfo;
  }

  if ((Partition->PartitionSize / CmetBlockSize) <
      MIN_PARTITION_BLOCKS)
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Partition size %lu invalid."
      "Must be atleast 2 %u blocks",
      __FUNCTION__,
      Partition->PartitionSize,
      CmetBlockSize
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitValidatePartitionInfo;
  }

ExitValidatePartitionInfo:
  return Status;
}

/**
 * Initialize the Storage portions of the driver
 *
 * @param[in]    ImageHandle              Image Handle of this file.
 * @param[in]    MmSystemTable            Pointer to the MM System table.
 *
 * @retval       EFI_SUCCESS               Read back last active record.
 *               EFI_DEVICE_ERROR          Can't find the NOR Flash Device.
 *               Other                     NOR Flash Transaction fail.
 */
EFI_STATUS
EFIAPI
CmetStorageInit (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS                   Status;
  NVIDIA_NOR_FLASH_PROTOCOL    *NorFlashProtocolArr[MAX_SOCKETS];
  NVIDIA_NOR_FLASH_PROTOCOL    *NorFlashProtocol;
  EFI_HANDLE                   CmetHandle;
  NVIDIA_CMET_RECORD_PROTOCOL  *CmetProtocol;
  UINTN                        Index;

  for (Index = 0; Index < MAX_SOCKETS; Index++) {
    NorFlashProtocol = GetSocketNorFlashProtocol (Index);
    if (NorFlashProtocol == NULL) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get NorFlashProtocol for Socket  %u\n",
        __FUNCTION__,
        Index
        ));
    }

    NorFlashProtocolArr[Index] = NorFlashProtocol;
  }

  NorFlashProtocol = NorFlashProtocolArr[SOCKET_0_NOR_FLASH];
  if (NorFlashProtocol == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Socket 0 NorFlash is not present\n",
      __FUNCTION__
      ));
    goto ExitCmetStorageInit;
  }

  /* The assumption is that all SPI-NORs have the same attributes */
  Status = NorFlashProtocol->GetAttributes (
                               NorFlashProtocol,
                               &NorFlashAttributes
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get NOR Flash attributes (%r)\r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitCmetStorageInit;
  }

  CmetProtocol = AllocateZeroPool (sizeof (NVIDIA_CMET_RECORD_PROTOCOL));
  Status       = GetPartitionData (
                   TEGRABL_CMET,
                   &CmetProtocol->PartitionInfo
                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to find Cmet Partition Info %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitCmetStorageInit;
  }

  Status = ValidatePartitionInfo (&CmetProtocol->PartitionInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Cmet Partition info is not valid %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitCmetStorageInit;
  }

  CmetProtocol->ReadRecord  = CmetReadRecord;
  CmetProtocol->WriteRecord = CmetWriteRecord;

  CopyMem (
    CmetProtocol->NorFlashProtocol,
    NorFlashProtocolArr,
    (sizeof (NVIDIA_NOR_FLASH_PROTOCOL *) * MAX_SOCKETS)
    );
  CmetHandle = NULL;
  Status     = gMmst->MmInstallProtocolInterface (
                        &CmetHandle,
                        &gNVIDIACmetStorageGuid,
                        EFI_NATIVE_INTERFACE,
                        CmetProtocol
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install CMET protocol Index %d %p Status %r\r\n",
      __FUNCTION__,
      Index,
      CmetHandle,
      Status
      ));
    goto ExitCmetStorageInit;
  }

ExitCmetStorageInit:
  return EFI_SUCCESS;
}
