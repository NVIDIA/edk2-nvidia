/** @file

  MM driver to write Sequential records to Flash.
  This file handles the storage portions.

  Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SequentialRecordPrivate.h"
#include <Library/GptLib.h>
#include <Library/BaseLib.h>
#include <Library/Crc8Lib.h>

#define ERASE_BYTE                (0xFF)
#define ACTIVE_PAGE_MAGIC         (0xFE)
#define RETIRED_PAGE_MAGIC        (0xFC)
#define GPT_PARTITION_BLOCK_SIZE  (512)
#define SOCKET_0_NOR_FLASH        (0)
#define MIN_PARTITION_BLOCKS      (2)
#define SEQ_BLOCK_SIZE            (65536)

STATIC NOR_FLASH_ATTRIBUTES  NorFlashAttributes;
STATIC UINT32                SupportedPartitions[] = {
  TEGRABL_RAS_ERROR_LOGS,
  TEGRABL_EARLY_BOOT_VARS
};

/**
 * Get Partition's First Block number
 *
 * @param[in]   Partition      Partition info.
 *
 * @retval      Start Block Number in Partition
 */
STATIC
UINT32
GetPartitionStartBlock (
  PARTITION_INFO  *Partition
  )
{
  return (Partition->PartitionByteOffset/ SEQ_BLOCK_SIZE);
}

/**
 * Get Number of Blocks in Partition
 *
 * @param[in]   Partition      Partition info.
 *
 * @retval      Number of Blocks in Partition
 */
STATIC
UINT32
GetPartitionNumBlocks (
  PARTITION_INFO  *Partition
  )
{
  return (Partition->PartitionSize / SEQ_BLOCK_SIZE);
}

/**
 * Get Partition's Last Block number.
 *
 * @param[in]   Partition      Partition info.
 *
 * @retval      Last Block Number in Partition
 */
STATIC
UINT32
GetPartitionLastBlock (
  PARTITION_INFO  *Partition
  )
{
  UINT32  LastBlockNum;
  UINT32  NumBlocks;

  NumBlocks    = Partition->PartitionSize / SEQ_BLOCK_SIZE;
  LastBlockNum = GetPartitionStartBlock (Partition) + NumBlocks - 1;
  return LastBlockNum;
}

/**
 * Check if the region of the SPI-NOR has the Erase Byte.
 *
 * @param[in]   NorFlashProtocol   NorFlash Protocol.
 * @param[in]   CurOffset          Offset to check.
 * @param[in]   RecSize            Size of record.
 *
 * @retval       TRUE         Valid variable at offset (header and checksum).
 *               FALSE        Not a valid variable.
 */
STATIC
BOOLEAN
IsSpiNorRegionErased (
  IN  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  IN  UINT32                     CurOffset,
  IN  UINT32                     RecSize
  )
{
  BOOLEAN     IsErased;
  UINT8       *Buf;
  UINTN       Index;
  EFI_STATUS  Status;

  IsErased = TRUE;
  Buf      = AllocateZeroPool (RecSize);
  if (Buf == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate %u bytes\n",
      __FUNCTION__,
      RecSize
      ));
    return FALSE;
  }

  Status = NorFlashProtocol->Read (
                               NorFlashProtocol,
                               CurOffset,
                               RecSize,
                               Buf
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to read at Offset %u Size %u\n",
      __FUNCTION__,
      CurOffset,
      RecSize
      ));
    IsErased = FALSE;
    goto ExitIsSpiNorRegionErased;
  }

  for (Index = 0; Index < RecSize; Index++) {
    if (Buf[Index] != ERASE_BYTE) {
      IsErased = FALSE;
      DEBUG ((
        DEBUG_ERROR,
        "%a: Unexpected Byte 0x%x (%u)\n",
        __FUNCTION__,
        Buf[Index],
        Index
        ));
      break;
    }
  }

ExitIsSpiNorRegionErased:
  FreePool (Buf);
  return IsErased;
}

/**
 * Check if a variable record is valid by checking the header
 * and recomputing the payload checksum.
 *
 * @param[in]    CurOffset          Read Offset to check.
 * @param[in]    NorFlashProtocol   NorFlash Protocol.
 * @param[out]   RecSize            Size of record (if valid).
 *
 * @retval       TRUE         Valid variable at offset (header and checksum).
 *               FALSE        Not a valid variable.
 */
STATIC
BOOLEAN
IsValidRecord (
  IN  UINT32                     CurOffset,
  IN  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  OUT UINT32                     *RecSize
  )
{
  BOOLEAN     IsValid;
  EFI_STATUS  Status;
  DATA_HDR    DataHdr;
  UINT8       ComputedCrc8;
  VOID        *Buf;
  UINT32      BufSize;
  VOID        *CrcBuf;

  Buf     = NULL;
  IsValid = TRUE;
  Status  = NorFlashProtocol->Read (
                                NorFlashProtocol,
                                CurOffset,
                                sizeof (DataHdr),
                                &DataHdr
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to read Block at %u header %r\n",
      __FUNCTION__,
      CurOffset,
      Status
      ));
    goto ExitIsValidRecord;
  }

  if (DataHdr.Flags == ACTIVE_PAGE_MAGIC) {
    Buf = AllocateZeroPool (DataHdr.SizeBytes);
    if (Buf == NULL) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to allocate %u bytes\n",
        __FUNCTION__,
        DataHdr.SizeBytes
        ));
      IsValid = FALSE;
      goto ExitIsValidRecord;
    }

    BufSize = DataHdr.SizeBytes;
    Status  = NorFlashProtocol->Read (
                                  NorFlashProtocol,
                                  CurOffset,
                                  BufSize,
                                  Buf
                                  );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a:Failed to read Block at %u header %r\n",
        __FUNCTION__,
        CurOffset,
        Status
        ));
      IsValid = FALSE;
      goto ExitIsValidRecord;
    }

    CrcBuf       = ((UINT8 *)Buf + OFFSET_OF (DATA_HDR, SizeBytes));
    ComputedCrc8 = CalculateCrc8 (
                     CrcBuf,
                     (BufSize - OFFSET_OF (DATA_HDR, SizeBytes)),
                     0,
                     TYPE_CRC8_MAXIM
                     );
    if (DataHdr.Crc8 == ComputedCrc8) {
      IsValid  = TRUE;
      *RecSize = DataHdr.SizeBytes;
    } else {
      DEBUG ((
        DEBUG_ERROR,
        "%a:Failed Crc Expected %u Got %u\n",
        __FUNCTION__,
        DataHdr.Crc8,
        ComputedCrc8
        ));
      IsValid = FALSE;
    }
  } else {
    DEBUG ((
      DEBUG_INFO,
      "%a:Invalid Header Expected 0x%x Got 0x%x\n",
      __FUNCTION__,
      ACTIVE_PAGE_MAGIC,
      DataHdr.Flags
      ));
    IsValid = FALSE;
  }

ExitIsValidRecord:
  if (Buf != NULL) {
    FreePool (Buf);
  }

  return IsValid;
}

/**
 * Get next block to write to.
 *
 * @param[in]   Partition      Partition info.
 * @param[in]   ActiveBlock    Current Active Block.
 *
 * @retval      Next Block Number to write to.
 *
 */
STATIC
UINT32
GetNextWriteBlock (
  IN  PARTITION_INFO  *Partition,
  IN  UINT32          ActiveBlock
  )
{
  UINT32  StartBlock;
  UINT32  LastBlock;
  UINT32  NextBlock;

  StartBlock = GetPartitionStartBlock (Partition);
  LastBlock  = GetPartitionLastBlock (Partition);

  /*
   * If we're currently on the LastBlock of the partition, wrap
   * around to the first, else move to the next sequential block.
   */
  if (ActiveBlock == LastBlock) {
    NextBlock = StartBlock;
  } else {
    NextBlock = ActiveBlock + 1;
  }

  return NextBlock;
}

/**
 * Erase New Block being used to write to
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
EraseNewBlock (
  IN  PARTITION_INFO             *Partition,
  IN  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  IN  UINT32                     SocketNum,
  IN  UINT32                     ActiveBlockNum
  )
{
  EFI_STATUS  Status;
  UINT32      EraseBlocks;
  UINT32      EraseBlockNum;

  EraseBlocks   = SEQ_BLOCK_SIZE / NorFlashAttributes.BlockSize;
  EraseBlockNum = (ActiveBlockNum * SEQ_BLOCK_SIZE) / NorFlashAttributes.BlockSize;

  DEBUG ((
    DEBUG_INFO,
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
 * Retire the current Active Block and return the new block to use..
 *
 * @param[in]       Partition          Partition info.
 * @param[in]       NorFlashProtocol   NorFlash Protocol.
 * @param[in]       SocketNum          Specify which SPI-NOR to write to.
 * @param[in]       RetireBlockNum     The block to be retired.
 *
 * @retval       EFI_SUCCESS   Block retired.
 *               OTHER         NOR FLASH Transaction error.
 */
STATIC
EFI_STATUS
RetireBlock (
  IN     PARTITION_INFO             *Partition,
  IN     NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  IN     UINT32                     SocketNum,
  IN     UINT32                     RetireBlockNum
  )
{
  EFI_STATUS  Status;
  DATA_HDR    DataHdr;
  UINT32      ReadOffset;

  DEBUG ((
    DEBUG_INFO,
    "%a: Retiring Block %u\n",
    __FUNCTION__,
    RetireBlockNum
    ));
  ReadOffset = RetireBlockNum * SEQ_BLOCK_SIZE;
  Status     = NorFlashProtocol->Read (
                                   NorFlashProtocol,
                                   ReadOffset,
                                   sizeof (DataHdr),
                                   &DataHdr
                                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Failed to read Block%u header %r\n",
      RetireBlockNum,
      Status
      ));
    goto RetireBlockExit;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Readback first record Flags %x Size %x \n",
    __FUNCTION__,
    DataHdr.Flags,
    DataHdr.SizeBytes
    ));
  DataHdr.Flags = RETIRED_PAGE_MAGIC;
  Status        = NorFlashProtocol->Write (
                                      NorFlashProtocol,
                                      ReadOffset,
                                      sizeof (DataHdr),
                                      &DataHdr
                                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Failed to Write Block%u Retire header %r\n",
      RetireBlockNum,
      Status
      ));
    goto RetireBlockExit;
  }

RetireBlockExit:
  return Status;
}

/**
 * Get the Active Block for the Partition being written to.
 *
 * @param[in]    Partition      Partition info.
 * @param[in]    SocketNum      Specify which SPI-NOR to write to.
 * @param[out]   ActiveBlockNum Active Block being used to read/write.
 *
 * @retval       EFI_SUCCESS   Found active page to write to.
 *               EFI_NOT_FOUND No Active Page found.
 */
STATIC
EFI_STATUS
GetActiveBlock (
  IN  PARTITION_INFO             *Partition,
  IN  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  IN  UINTN                      SocketNum,
  OUT UINT32                     *ActiveBlockNum
  )
{
  EFI_STATUS  Status;
  UINTN       NumBlocks;
  UINTN       StartBlock;
  UINTN       EndBlock;
  UINTN       BlockIndex;
  DATA_HDR    DataHdr;
  UINT32      ReadOffset;

  NumBlocks  = GetPartitionNumBlocks (Partition);
  StartBlock = GetPartitionStartBlock (Partition);
  EndBlock   = StartBlock + NumBlocks;

  for (BlockIndex = StartBlock; BlockIndex < EndBlock; BlockIndex++) {
    ReadOffset = (BlockIndex * SEQ_BLOCK_SIZE);
    DEBUG ((
      DEBUG_INFO,
      "%a: Block %u Offset %u\n",
      __FUNCTION__,
      BlockIndex,
      ReadOffset
      ));
    Status = NorFlashProtocol->Read (
                                 NorFlashProtocol,
                                 ReadOffset,
                                 sizeof (DataHdr),
                                 &DataHdr
                                 );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to read Block%u header %r\n",
        BlockIndex,
        Status
        ));
      goto ActiveBlockExit;
    }

    if (DataHdr.Flags == ACTIVE_PAGE_MAGIC) {
      Status          = EFI_SUCCESS;
      *ActiveBlockNum = BlockIndex;
      DEBUG ((
        DEBUG_INFO,
        "%a: Return ActiveBLock %u\n",
        __FUNCTION__,
        *ActiveBlockNum
        ));
      break;
    }
  }

  /* There are no blocks with the active page flags in the header flags */
  if (BlockIndex == EndBlock) {
    DEBUG ((DEBUG_INFO, "Failed to find active block Default to StartBlok\n"));
    Status = EFI_NOT_FOUND;
  }

ActiveBlockExit:
  return Status;
}

/**
 * Get the Offset in the passed in block to read from.
 *
 * @param[in]    Partition         Partition info.
 * @param[in]    NorFlashProtocol  NorFlash Protocol.
 * @param[in]    SocketNum         Specify which SPI-NOR to write to.
 * @param[in]    BlockNum          SPI-NOR Absolute Block to check.
 * @param[out]   ReadLastOffset    Offset to read record from.
 * @param[out]   RecSize           Total Size of the Record at the offset.
 *                                 (Including the header).
 *
 * @retval       EFI_SUCCESS   Found active page to write to.
 *               Other Error   if NOR flash transaction fail.
 */
STATIC
EFI_STATUS
GetReadLastOffset (
  IN  PARTITION_INFO             *Partition,
  IN  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  IN  UINTN                      SocketNum,
  IN  UINTN                      BlockNum,
  OUT UINT32                     *ReadLastOffset,
  OUT UINT32                     *RecSize
  )
{
  UINT32  StartOffset;
  UINT32  EndOffset;
  UINT32  CurOffset;
  UINT32  CurSize;
  UINT32  PrevOffset;
  UINT32  PrevSize;

  DEBUG ((DEBUG_INFO, "%a: Read from Block %u\n", __FUNCTION__, BlockNum));
  StartOffset = BlockNum * SEQ_BLOCK_SIZE;
  EndOffset   = StartOffset + SEQ_BLOCK_SIZE;
  CurOffset   = StartOffset;
  PrevOffset  = CurOffset;
  CurSize     = 0;
  PrevSize    = CurSize;
  while (CurOffset < EndOffset) {
    if (IsValidRecord (CurOffset, NorFlashProtocol, &CurSize) == TRUE) {
      PrevOffset =  CurOffset;
      PrevSize   =  CurSize;
      CurOffset += CurSize;
    } else {
      DEBUG ((
        DEBUG_INFO,
        "%a: Header isn't valid %u\n",
        __FUNCTION__,
        CurOffset
        ));
      break;
    }
  }

  *ReadLastOffset = PrevOffset;
  *RecSize        = PrevSize;
  DEBUG ((
    DEBUG_INFO,
    "%a: ReadLast Record %u, Sz %u\n",
    __FUNCTION__,
    *ReadLastOffset,
    *RecSize
    ));
  return EFI_SUCCESS;
}

/**
 * Get the Offset in the active block to write the next record to.
 *
 * @param[in]    Partition         Partition info.
 * @param[in]    NorFlashProtocol  NorFlash Protocol.
 * @param[in]    SocketNum         Specify which SPI-NOR to write to.
 * @param[out]   WriteNextOffset   Block Offset to write to.
 *
 * @retval       EFI_SUCCESS   Found active page to write to.
 *               Other Error   if NOR flash transaction fail.
 */
STATIC
EFI_STATUS
GetWriteNextOffset (
  IN  PARTITION_INFO             *Partition,
  IN  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  IN  UINTN                      SocketNum,
  OUT UINT32                     *WriteNextOffset
  )
{
  EFI_STATUS  Status;
  UINT32      ReadLast;
  UINT32      ReadLastSize;
  UINT32      ActiveBlockNum;

  Status = GetActiveBlock (
             Partition,
             NorFlashProtocol,
             SocketNum,
             &ActiveBlockNum
             );
  if (EFI_ERROR (Status)) {
    /* If an active block isn't found (EFI_NOT_FOUND), this could be the first
     * record being written to the partition.
     */
    if (Status == EFI_NOT_FOUND) {
      ActiveBlockNum   = GetPartitionStartBlock (Partition);
      *WriteNextOffset = ActiveBlockNum * SEQ_BLOCK_SIZE;
      DEBUG ((
        DEBUG_INFO,
        "No Active block found default to the first block %u %r\n",
        ActiveBlockNum,
        Status
        ));
      Status = EFI_SUCCESS;
    } else {
      DEBUG ((DEBUG_ERROR, "Failed to find active block %r", Status));
      return Status;
    }
  } else {
    /* If we've got an active block being written to find the last valid read
     * record and set the write offset to be an additional sizeBytes of the last
     * read record.
     */
    Status = GetReadLastOffset (
               Partition,
               NorFlashProtocol,
               SocketNum,
               ActiveBlockNum,
               &ReadLast,
               &ReadLastSize
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get Read Offset %r\n",
        __FUNCTION__,
        Status
        ));
      goto ExitGetNextWriteOffset;
    }

    *WriteNextOffset = ReadLast + ReadLastSize;
  }

  DEBUG ((DEBUG_INFO, "WriteOffset %u\n", *WriteNextOffset));
ExitGetNextWriteOffset:
  return Status;
}

/**
 * Read the last valid record from the partition.
 *  Read the last written record. Find the last valid record with a
 *  valid header (magic/checksum) and return that to the client.
 *
 * @param[in]   This          Pointer to Sequential Record Proto.
 * @param[in]   SocketNum     Specify which SPI-NOR to write to.
 * @param[out]  Buf           Buffer to read into..
 * @param[in]   BufSize       Size of read buffer.
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
ReadLastRecord (
  IN  NVIDIA_SEQ_RECORD_PROTOCOL  *This,
  IN  UINTN                       SocketNum,
  OUT VOID                        *Buf,
  IN  UINTN                       BufSize
  )
{
  EFI_STATUS                 Status;
  UINT32                     ReadLastHdrOffset;
  UINT32                     ReadLastRecOffset;
  UINT32                     ReadLastRecSize;
  UINT32                     ActiveBlock;
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;

  if (BufSize < sizeof (DATA_HDR)) {
    DEBUG ((DEBUG_ERROR, "%a: Buffer too small\n", __FUNCTION__));
    Status = EFI_INVALID_PARAMETER;
    goto ExitReadLastRecord;
  }

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

  Status = GetActiveBlock (
             &This->PartitionInfo,
             NorFlashProtocol,
             SocketNum,
             &ActiveBlock
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get ActiveBlock %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitReadLastRecord;
  }

  Status = GetReadLastOffset (
             &This->PartitionInfo,
             NorFlashProtocol,
             SocketNum,
             ActiveBlock,
             &ReadLastHdrOffset,
             &ReadLastRecSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Last Read Offset %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitReadLastRecord;
  }

  /* If provided BufferSize is less than the record being read. */
  if ((BufSize <  (ReadLastRecSize - sizeof (DATA_HDR)))) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: BufSize = %u not big enough RecSize %u\n",
      __FUNCTION__,
      BufSize,
      ReadLastRecSize
      ));
    Status = EFI_BUFFER_TOO_SMALL;
    goto ExitReadLastRecord;
  }

  ReadLastRecOffset = ReadLastHdrOffset + sizeof (DATA_HDR);
  Status            = NorFlashProtocol->Read (
                                          NorFlashProtocol,
                                          ReadLastRecOffset,
                                          BufSize,
                                          Buf
                                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to read Block at %u header %r\n",
      __FUNCTION__,
      ReadLastRecOffset,
      Status
      ));
    goto ExitReadLastRecord;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Read Record at %u Socket %u\n",
    __FUNCTION__,
    ReadLastRecOffset,
    SocketNum
    ));
ExitReadLastRecord:
  return Status;
}

/**
 * Write the next record to the Partition.
 *  This function locates the last valid record and writes the next record
 *  right after it OR the start of the first block if this is the very first
 *  record.
 *  Erase the block first if we are writing the first record.
 *  When writing write the record paylod (which comes from the client) first
 *  then the Header (containing the checksum/size/flags) that this driver
 *  adds.
 *  After writing the record fully, retire the previous block (if we switched
 *  blocks), which means we mark the very first record with the
 *  RETIRED_PAGE_MAGIC in the header.
 *
 *
 * @param[in]    This          Pointer to Sequential Record Proto.
 * @param[in]    SocketNum     Specify which SPI-NOR to write to.
 * @param[in]    InBuf         Input Write buffer.
 * @param[in]    BufSize       Size of the write buffer.
 *
 * @retval       EFI_SUCCESS   Read back last active record.
 *               Other         NOR Flash Transaction fail.
 */
STATIC
EFI_STATUS
EFIAPI
WriteNextRecord (
  IN  NVIDIA_SEQ_RECORD_PROTOCOL  *This,
  IN  UINTN                       SocketNum,
  IN  VOID                        *InBuf,
  IN  UINTN                       BufSize
  )
{
  EFI_STATUS                 Status;
  UINT32                     WriteHeaderOffset;
  UINT32                     ActiveBlock;
  UINT32                     WriteBlock;
  UINT32                     ActiveBlockEnd;
  DATA_HDR                   *DataHdr;
  UINT32                     RecSize;
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;
  VOID                       *Buf;
  VOID                       *RecBuf;
  VOID                       *CrcBuf;

  Buf = NULL;

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

  RecSize = BufSize + sizeof (DataHdr);
  Status  = GetWriteNextOffset (
              &This->PartitionInfo,
              NorFlashProtocol,
              SocketNum,
              &WriteHeaderOffset
              );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Writeoffset %r \n",
      __FUNCTION__,
      Status
      ));
    goto ExitWriteNextRecord;
  }

  Status = GetActiveBlock (
             &This->PartitionInfo,
             NorFlashProtocol,
             SocketNum,
             &ActiveBlock
             );
  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      /* If an active block isn't found and this is the beginning of the
       * first block.Set the active block to the start block.
       */
      ActiveBlock = GetPartitionStartBlock (&This->PartitionInfo);
    } else {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get ActiveBlock %r\n",
        __FUNCTION__,
        Status
        ));
      goto ExitWriteNextRecord;
    }
  }

  DEBUG ((
    DEBUG_INFO,
    "%a:%d ActiveBlock %u WriteOffet %u\n",
    __FUNCTION__,
    __LINE__,
    ActiveBlock,
    WriteHeaderOffset
    ));

  ActiveBlockEnd = (ActiveBlock + 1) * SEQ_BLOCK_SIZE;

  /**
   * Check if we need to switch to a new block, Either we are going over
   * the current block OR
   * If we're writing to a region that isn't erased (which is not the start of
   * a new block).
   **/
  if (((WriteHeaderOffset + RecSize) > ActiveBlockEnd) ||
      ((WriteHeaderOffset != (ActiveBlock * SEQ_BLOCK_SIZE)) &&
       (IsSpiNorRegionErased (
          NorFlashProtocol,
          WriteHeaderOffset,
          RecSize
          ) == FALSE)))
  {
    DEBUG ((
      DEBUG_ERROR,
      "Current Block %u(%u) is full OR INVALID Move to new Block\n",
      ActiveBlock,
      WriteHeaderOffset
      ));
    WriteBlock        = GetNextWriteBlock (&This->PartitionInfo, ActiveBlock);
    WriteHeaderOffset = WriteBlock * SEQ_BLOCK_SIZE;
  } else {
    WriteBlock = ActiveBlock;
  }

  if ((WriteBlock != ActiveBlock) ||
      (WriteHeaderOffset == (WriteBlock * SEQ_BLOCK_SIZE)))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Erasing new Block %u\n",
      __FUNCTION__,
      WriteBlock
      ));
    EraseNewBlock (
      &This->PartitionInfo,
      NorFlashProtocol,
      SocketNum,
      WriteBlock
      );
  }

  Buf = AllocateZeroPool (BufSize + sizeof (DATA_HDR));
  if (Buf == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to allocate Buf \n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitWriteNextRecord;
  }

  DataHdr = (DATA_HDR *)Buf;
  RecBuf  = ((UINT8 *)Buf + sizeof (DATA_HDR));
  CopyMem (RecBuf, InBuf, BufSize);

  SetMem (DataHdr, sizeof (DATA_HDR), ERASE_BYTE);
  DataHdr->Flags     = ACTIVE_PAGE_MAGIC;
  DataHdr->SizeBytes = RecSize;

  /* Compute CRC8 inclusive of the Size Field.*/
  CrcBuf        = ((UINT8 *)Buf + OFFSET_OF (DATA_HDR, SizeBytes));
  DataHdr->Crc8 = CalculateCrc8 (
                    CrcBuf,
                    (RecSize - OFFSET_OF (DATA_HDR, SizeBytes)),
                    0,
                    TYPE_CRC8_MAXIM
                    );
  DEBUG ((
    DEBUG_INFO,
    "%a: Allocated Buf %p RecBuf %p CrcBuf %p CrcBufSize %u\n",
    __FUNCTION__,
    Buf,
    RecBuf,
    CrcBuf,
    (RecSize - OFFSET_OF (DATA_HDR, SizeBytes))
    ));

  Status = NorFlashProtocol->Write (
                               NorFlashProtocol,
                               WriteHeaderOffset,
                               RecSize,
                               Buf
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to WriteRecord %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitWriteNextRecord;
  }

  DEBUG ((
    DEBUG_INFO,
    "Computed CRC %u. TotaLen %u RecLen %u WriteHeader to %u\n",
    DataHdr->Crc8,
    DataHdr->SizeBytes,
    BufSize,
    WriteHeaderOffset
    ));

  /* If we switched blocks, then retire the old active Block */
  if (WriteBlock != ActiveBlock) {
    RetireBlock (
      &This->PartitionInfo,
      NorFlashProtocol,
      SocketNum,
      ActiveBlock
      );
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Wrote Record Size %u at %u Socket %u\n",
    __FUNCTION__,
    BufSize,
    SocketNum,
    WriteHeaderOffset
    ));
ExitWriteNextRecord:
  if (Buf != NULL) {
    FreePool (Buf);
  }

  return Status;
}

/**
 *  Erase the partition at a given Socket.
 *  Protocol function to erase the partition. The decision to erase a
 *  partition is usually done outside StMM and whilst this is usually
 *  a rare occurence, keep this function in case we need to erase
 *  the partition via StMM.
 *
 * @param[in]    This          Pointer to Sequential Record Proto.
 * @param[in]    SocketNum     Specify which SPI-NOR to write to.
 *
 * @retval       EFI_SUCCESS   Read back last active record.
 *               Other         Return value from NorFlash Erase function call.
 */
STATIC
EFI_STATUS
EFIAPI
ErasePartition (
  IN  NVIDIA_SEQ_RECORD_PROTOCOL  *This,
  IN  UINTN                       SocketNum
  )
{
  EFI_STATUS                 Status;
  UINT32                     EraseBlocks;
  UINT32                     EraseBlockNum;
  PARTITION_INFO             *Partition;
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;

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

  Partition     = &This->PartitionInfo;
  EraseBlocks   = Partition->PartitionSize / NorFlashAttributes.BlockSize;
  EraseBlockNum = Partition->PartitionByteOffset / NorFlashAttributes.BlockSize;

  DEBUG ((
    DEBUG_ERROR,
    "%a Erasing at %u %u blocks \n",
    __FUNCTION__,
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
  if (((Partition->PartitionSize % SEQ_BLOCK_SIZE) != 0) ||
      ((Partition->PartitionByteOffset % SEQ_BLOCK_SIZE) != 0))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Partition not valid.Size %lu Offset %lu Block %d",
      __FUNCTION__,
      Partition->PartitionSize,
      Partition->PartitionByteOffset,
      SEQ_BLOCK_SIZE
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitValidatePartitionInfo;
  }

  if ((Partition->PartitionSize / SEQ_BLOCK_SIZE) <
      MIN_PARTITION_BLOCKS)
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Partition size %lu invalid."
      "Must be atleast 2 64KB blocks",
      __FUNCTION__,
      Partition->PartitionSize
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
SequentialStorageInit (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS                  Status;
  NVIDIA_NOR_FLASH_PROTOCOL   *NorFlashProtocolArr[MAX_SOCKETS];
  NVIDIA_NOR_FLASH_PROTOCOL   *NorFlashProtocol;
  EFI_HANDLE                  SeqStoreHandle;
  UINTN                       Index;
  NVIDIA_SEQ_RECORD_PROTOCOL  *SeqProtocol;

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
    goto ExitInitDataFlash;
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
    goto ExitInitDataFlash;
  }

  for (Index = 0; Index < ARRAY_SIZE (SupportedPartitions); Index++) {
    SeqProtocol = AllocateZeroPool (sizeof (NVIDIA_SEQ_RECORD_PROTOCOL));

    Status = GetPartitionData (
               SupportedPartitions[Index],
               &SeqProtocol->PartitionInfo
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a : Failed to find Partition info for Partition%u %r\n",
        __FUNCTION__,
        SupportedPartitions[Index],
        Status
        ));
      continue;
    }

    Status = ValidatePartitionInfo (&SeqProtocol->PartitionInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: %u Partition info is not valid %r\n",
        __FUNCTION__,
        SupportedPartitions[Index],
        Status
        ));
      continue;
    }

    SeqProtocol->ReadLast       = ReadLastRecord;
    SeqProtocol->WriteNext      = WriteNextRecord;
    SeqProtocol->ErasePartition = ErasePartition;
    CopyMem (
      SeqProtocol->NorFlashProtocol,
      NorFlashProtocolArr,
      (sizeof (NVIDIA_NOR_FLASH_PROTOCOL *) * MAX_SOCKETS)
      );
    SeqStoreHandle = NULL;
    Status         = gMmst->MmInstallProtocolInterface (
                              &SeqStoreHandle,
                              &gNVIDIASequentialStorageGuid,
                              EFI_NATIVE_INTERFACE,
                              SeqProtocol
                              );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to install FVP protocol Index %d %p Status %r\r\n",
        __FUNCTION__,
        Index,
        SeqStoreHandle,
        Status
        ));
      goto ExitInitDataFlash;
    }
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: Density %lu Logical BlockSize %d \n",
    __FUNCTION__,
    NorFlashAttributes.MemoryDensity,
    SEQ_BLOCK_SIZE
    ));

ExitInitDataFlash:
  return EFI_SUCCESS;
}
