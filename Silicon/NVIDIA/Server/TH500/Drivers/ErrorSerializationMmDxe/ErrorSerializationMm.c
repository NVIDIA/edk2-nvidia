/** @file
  NVIDIA ERST Driver

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/MmServicesTableLib.h>  // gMmst
#include <Library/IoLib.h>               // MMIO calls
#include <Library/BaseMemoryLib.h>       // CopyMem
#include <Library/MemoryAllocationLib.h> // AllocatePool
#include <Library/NVIDIADebugLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h> // STMM_COMM_BUFFERS
#include <Library/HobLib.h>

#include <Guid/Cper.h> // From MdePkg

#include <Protocol/NorFlash.h>

#include <TH500/TH500Definitions.h> // For TH500_SW_IO6_BASE
#include <LicSwIo.h>                // For INTR_CTRL_SW_IO_N_INTR_*
#include <Include/Server/Erst.h>
#include "ErrorSerializationMm.h"
#include "ErrorSerializationMemory.h"

#ifdef EDKII_UNIT_TEST_FRAMEWORK_ENABLED
  #undef DEBUG_CODE
#define DEBUG_CODE(X)
  #undef DEBUG_ERROR
#define DEBUG_ERROR  DEBUG_INFO
  #undef DEBUG_WARN
#define DEBUG_WARN  DEBUG_INFO
#else
  #include <Library/TimerLib.h>
STATIC UINT64  WriteRecordTime __attribute__ ((unused)) = 0;
STATIC UINT64  SpiTime         __attribute__ ((unused)) = 0;
#endif

/* ERST Flash format overview

  This code assumes that the bits in the flash erase to 1, and can only be modified
  to 0 or a whole block erased back to 1.

  A portion of the flash is reserved for ERST. This portion is divided by
  this driver into ERST blocks, which correspond to one or more erasible blocks on
  the flash.

  Within an ERST block, CPER records are stored back to back starting at the beginning
  of the block. The CPER header includes an identifier, the record length, and some
  space for persistence information, so no additional header/metadata needs to be stored
  in the flash. The next entry will always start immediately after the current entry
  or at the start of the next ERST block.

  Due to the inability to modify a bit in a record from 0->1, modifications to anything
  other than the peristence information Status field will look like writing a new copy
  of the CPER and deleting the old copy.

  For Fault Tolerance reasons, rather that simply tracking Used/Free, this driver tracks
  several additional States to allow for error recovery to be possible in the event
  that an error happened during a write operation of the flash.

  The Status field in the persistence information can have these possible values:
  - FREE:     nothing has been written here yet
  - INCOMING: a CPER write has been started but not yet completed
  - VALID:    the CPER has been completely written and is valid
  - OUTGOING: the CPER is being rewritten elsewhere, and this copy will be deleted
  - DELETED:  the CPER length is valid, but the CPER contents are no longer active
  - INVALID:  the CPER is in an invalid state so the rest of the block is in an unknown
              state and the block should be cleaned up to resolve this

  When a CPER is written, it is always written after all the previous CPERs in the block,
  regardless of their State.
  When a CPER is cleared, its Status is written to DELETED, but the rest of it stays intact
  on the flash.

  The write sequence is:
  - Find the FREE space within a block
  - Write the Status of the FREE space to INCOMING
  - Write the CPER to that space
  - If the new CPER is "replacing" an existing CPER (ie. they have the same RecordID),
    write the Status of the existing CPER to OUTGOING
  - Write the Status new CPER to VALID
  - Write the Status of the existing CPER to DELETED, if it exists

  At initialization time (and when out-of-sync errors are detected by the driver), the driver
  will attempt to clean up INCOMING, INVALID, and OUTGOING Status it sees, before allowing
  any user-generated operatios to happen.

  This has several implications:
  - An empty block will have FREE as the first Status in the block
  - A non-empty block will have one or more back-to-back VALID or OUTGOING Statuses
  - At most one OUTGOING Status can be seen, which will be cleaned up at Init time
  - At most one INCOMING Status can be seen, which will be the last non-FREE Status
    in its block, and which will be cleaned up at Init time.
  - Any INVALID Status was previously an INCOMING status, so will similarly be the
    last non-FREE Status in its block
  - The RecordID of INCOMING may not be valid, but that of OUTGOING and VALID is
  - The RecordID of FREE should be all 1s
  - The RecordID of INVALID is irrelevant and possibly wrong, so should be ignored

  Any given RecordID can be associated with some combination of the following Status,
  depending on where in the write sequence an error happened:
  (with no existing CPER for the ID):
  - NONE
  - INCOMING
  - VALID
  (with an existing, non-INVALID CPER for the ID):
    Existing     New
    --------     ---
  - VALID        NONE
  - VALID        INCOMING
  - OUTGOING     INCOMING
  - OUTGOING     VALID
  - DELETED      VALID

  During Init, if an OUTGOING Status is seen and a VALID Status for the same RecordID
  is seen, the OUTGOING will be marked as DELETED. But if no VALID is seen and an
  INCOMING Status is seen for that RecordID, it is possible that the record was being
  moved, and if possible the driver will continue the move of OUTGOING to INCOMING.

  If an OUTGOING Status is seen but no corresponding INCOMING is seen, the OUTGOING
  will be moved to restore it to VALID Status.

  If an INCOMING Status is seen but no corresponding OUTGOING is seen, it is impossible
  to determine how much of the INCOMING CPER is missing, and it will be marked as
  INVALID.

  At Init time the driver will read all the blocks and cache information about the
  records there. During read and clear operations, it will keep track of when the tracking
  information is out of sync with the flash, and will attempt to re-init itself when
  it detects an out of sync problem.

  When space is required for writing a new record (or moving an existing one), the code
  will look first for a block that doesn't contain any valid records, and erase it if
  it exists. If not available, the code will consolidate valid records into a reserved
  free block in an attempt to consolidate the free space into a block that can be erased.

  Most of the time CPER records are under 256 bytes, but they can get up to around 3k

*/

ERST_PRIVATE_INFO                       mErrorSerialization;
STATIC ERROR_SERIALIZATION_MM_PROTOCOL  ErrorSerializationProtocol;
UINT8                                   *mShadowFlash = NULL;

STATIC
BOOLEAN
IsErasedBuffer (
  IN UINT8  *Buffer,
  IN UINTN  BufferSize,
  IN UINT8  Expected
  )
{
  BOOLEAN  IsEmpty;
  UINT8    *Ptr;
  UINTN    Index;

  Ptr     = Buffer;
  IsEmpty = TRUE;
  for (Index = 0; Index < BufferSize; Index += 1) {
    if (*Ptr++ != Expected) {
      IsEmpty = FALSE;
      break;
    }
  }

  return IsEmpty;
}

EFI_STATUS
EFIAPI
ErstInitShadowFlash (
  )
{
  EFI_STATUS  Status;
  UINT64      StartTime __attribute__ ((unused));

  mShadowFlash = AllocatePool (mErrorSerialization.PartitionSize);
  if (mShadowFlash == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Error allocating 0x%x bytes of memory to cache the Flash contents. Will run witohut a cache\n", __FUNCTION__, mErrorSerialization.PartitionSize));
    Status = EFI_OUT_OF_RESOURCES;
  } else {
    DEBUG_CODE (
      StartTime = GetTimeInNanoSecond (GetPerformanceCounter ());
      );
    Status = mErrorSerialization.NorFlashProtocol->Read (
                                                     mErrorSerialization.NorFlashProtocol,
                                                     mErrorSerialization.NorErstOffset,
                                                     mErrorSerialization.PartitionSize,
                                                     mShadowFlash
                                                     );
    DEBUG_CODE (
    {
      UINT64 EndTime;
      UINT64 ElapsedTime;
      EndTime = GetTimeInNanoSecond (GetPerformanceCounter ());
      if (EndTime > StartTime) {
        ElapsedTime = EndTime-StartTime;
      } else {
        ElapsedTime = (MAX_UINT64 - StartTime) + EndTime;
      }

      DEBUG ((DEBUG_ERROR, "%a: Initing the cache of the Flash contents took %llu ns\n", __FUNCTION__, ElapsedTime));
    }
      );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to initialize the cache of the Flash contents (rc=%r). Will try to run witohut a cache\n", __FUNCTION__, Status));
      FreePool (mShadowFlash);
      mShadowFlash = NULL;
    }
  }

  return Status;
}

// Read data from the SPINOR
EFI_STATUS
EFIAPI
ErstReadSpiNor (
  OUT VOID   *Data,
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS  Status;
  UINT64      StartTime __attribute__ ((unused));

  if (Offset + Length > mErrorSerialization.PartitionSize) {
    Status = EFI_INVALID_PARAMETER;
    goto ReturnStatus;
  }

  if (mShadowFlash) {
    CopyMem (Data, &mShadowFlash[Offset], Length);
    Status = EFI_SUCCESS;
  } else {
    DEBUG_CODE (
      StartTime = GetTimeInNanoSecond (GetPerformanceCounter ());
      );
    Status = mErrorSerialization.NorFlashProtocol->Read (
                                                     mErrorSerialization.NorFlashProtocol,
                                                     Offset + mErrorSerialization.NorErstOffset,
                                                     Length,
                                                     Data
                                                     );

    DEBUG_CODE (
    {
      UINT64 EndTime;
      UINT64 ElapsedTime;
      EndTime = GetTimeInNanoSecond (GetPerformanceCounter ());
      if (EndTime > StartTime) {
        ElapsedTime = EndTime-StartTime;
      } else {
        ElapsedTime = (MAX_UINT64 - StartTime) + EndTime;
      }

      SpiTime += ElapsedTime;
    }
      );
  }

ReturnStatus:
  return Status;
}

// Write data to the SPINOR
EFI_STATUS
EFIAPI
ErstWriteSpiNor (
  IN VOID    *Data,
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS  Status;
  UINT64      StartTime __attribute__ ((unused));

  if (Offset + Length > mErrorSerialization.PartitionSize) {
    Status = EFI_INVALID_PARAMETER;
    goto ReturnStatus;
  }

  DEBUG_CODE (
    StartTime = GetTimeInNanoSecond (GetPerformanceCounter ());
    );

  if (mShadowFlash) {
    CopyMem (&mShadowFlash[Offset], Data, Length);
  }

  Status = mErrorSerialization.NorFlashProtocol->Write (
                                                   mErrorSerialization.NorFlashProtocol,
                                                   Offset + mErrorSerialization.NorErstOffset,
                                                   Length,
                                                   Data
                                                   );

  DEBUG_CODE (
  {
    UINT64  EndTime;
    UINT64  ElapsedTime;

    EndTime = GetTimeInNanoSecond (GetPerformanceCounter ());
    if (EndTime > StartTime) {
      ElapsedTime = EndTime-StartTime;
    } else {
      ElapsedTime = (MAX_UINT64 - StartTime) + EndTime;
    }

    SpiTime += ElapsedTime;
  }
    );

ReturnStatus:
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NorFlashWrite returned Status 0x%x\n", __FUNCTION__, Status));
  }

  return Status;
}

// Erase a block of SPINOR
EFI_STATUS
EFIAPI
ErstEraseSpiNor (
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS  Status;
  UINT32      Lba;
  UINT32      NumLba;
  UINT64      StartTime __attribute__ ((unused));

  if ((Offset%mErrorSerialization.NorAttributes.BlockSize != 0) ||
      (Length%mErrorSerialization.NorAttributes.BlockSize != 0) ||
      (Offset + Length + mErrorSerialization.NorErstOffset > mErrorSerialization.NorAttributes.MemoryDensity))
  {
    DEBUG ((DEBUG_ERROR, "%a: Offset or Length invalid\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Lba    = (Offset + mErrorSerialization.NorErstOffset)/mErrorSerialization.NorAttributes.BlockSize;
  NumLba = Length/mErrorSerialization.NorAttributes.BlockSize;

  DEBUG_CODE (
    StartTime = GetTimeInNanoSecond (GetPerformanceCounter ());
    );

  Status = mErrorSerialization.NorFlashProtocol->Erase (
                                                   mErrorSerialization.NorFlashProtocol,
                                                   Lba,
                                                   NumLba
                                                   );

  DEBUG_CODE (
  {
    UINT64 EndTime;
    UINT64 ElapsedTime;
    EndTime = GetTimeInNanoSecond (GetPerformanceCounter ());
    if (EndTime > StartTime) {
      ElapsedTime = EndTime-StartTime;
    } else {
      ElapsedTime = (MAX_UINT64 - StartTime) + EndTime;
    }

    SpiTime += ElapsedTime;

    UINT8 *Data            = ErstAllocatePoolBlock (mErrorSerialization.BlockSize);
    VOID *SavedShadowFlash = mShadowFlash;
    mShadowFlash           = NULL;
    ErstReadSpiNor (Data, Offset, Length); // Want this to access actual spinor, not cache
    mShadowFlash = SavedShadowFlash;
    if (!IsErasedBuffer (Data, Length, 0xFF)) {
      DEBUG ((DEBUG_ERROR, "%a: Spinor block isn't erased after Erase operation!\n", __FUNCTION__));
    } else {
      DEBUG ((DEBUG_INFO, "%a: Erased block successfully!\n", __FUNCTION__));
    }

    ErstFreePoolBlock (Data);
  }
    );

  return Status;
}

// Locates Status field in the CPER and writes it, and updates the INCOMING/OUTGOING tracking
EFI_STATUS
EFIAPI
ErstWriteCperStatus (
  IN UINT8           *CperStatus,
  IN ERST_CPER_INFO  *CperInfo
  )
{
  EFI_STATUS  Status;

  if ((*CperStatus == ERST_RECORD_STATUS_INCOMING) &&
      (mErrorSerialization.IncomingCperInfo != NULL) &&
      (mErrorSerialization.IncomingCperInfo->RecordOffset != CperInfo->RecordOffset))
  {
    DEBUG ((DEBUG_ERROR, "%a: Trying to set Record Status to INCOMING when a different INCOMING already exists\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ReturnStatus;
  }

  if ((*CperStatus == ERST_RECORD_STATUS_OUTGOING) &&
      (mErrorSerialization.OutgoingCperInfo != NULL) &&
      (mErrorSerialization.OutgoingCperInfo->RecordOffset != CperInfo->RecordOffset))
  {
    DEBUG ((DEBUG_ERROR, "%a: Trying to set Record Status to OUTGOING when a different OUTGOING already exists\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ReturnStatus;
  }

  Status = ErstWriteSpiNor (
             CperStatus,
             CperInfo->RecordOffset +
             OFFSET_OF (EFI_COMMON_ERROR_RECORD_HEADER, PersistenceInfo) +
             OFFSET_OF (CPER_ERST_PERSISTENCE_INFO, Status),
             1
             );
  if (EFI_ERROR (Status)) {
    goto ReturnStatus;
  }

  // Update Incoming/Outgoing tracking
  switch (*CperStatus) {
    case ERST_RECORD_STATUS_INCOMING:
      mErrorSerialization.IncomingCperInfo = CperInfo;
      break;

    case ERST_RECORD_STATUS_OUTGOING:
      mErrorSerialization.OutgoingCperInfo = CperInfo;
      break;

    default:
      if ((mErrorSerialization.IncomingCperInfo != NULL) &&
          (mErrorSerialization.IncomingCperInfo->RecordOffset == CperInfo->RecordOffset))
      {
        mErrorSerialization.IncomingCperInfo = NULL;
      }

      if ((mErrorSerialization.OutgoingCperInfo != NULL) &&
          (mErrorSerialization.OutgoingCperInfo->RecordOffset == CperInfo->RecordOffset))
      {
        mErrorSerialization.OutgoingCperInfo = NULL;
      }

      break;
  }

ReturnStatus:
  return Status;
}

// Finds the CperInfo for the RecordID if the ID is VALID
ERST_CPER_INFO *
ErstFindRecord (
  IN UINT64  RecordID
  )
{
  ERST_CPER_INFO  *Record;
  UINTN           RecordIndex;

  for (RecordIndex = 0; RecordIndex < mErrorSerialization.RecordCount; RecordIndex++) {
    Record = &mErrorSerialization.CperInfo[RecordIndex];
    if ((Record->RecordId == RecordID) &&
        (Record != mErrorSerialization.IncomingCperInfo) &&
        (Record != mErrorSerialization.OutgoingCperInfo))
    {
      DEBUG ((
        DEBUG_INFO,
        "%a: Index %u (0x%p) Has ID 0x%llx at offset 0x%x\n",
        __FUNCTION__,
        RecordIndex,
        Record,
        Record->RecordId,
        Record->RecordOffset
        ));
      return Record;
    }
  }

  return NULL;
}

// Erases the block in the SPINOR
EFI_STATUS
EFIAPI
ErstEraseBlock (
  IN ERST_BLOCK_INFO  *BlockInfo
  )
{
  EFI_STATUS  Status;

  Status = ErstEraseSpiNor (BlockInfo->Base, mErrorSerialization.BlockSize);
  if (!EFI_ERROR (Status)) {
    BlockInfo->UsedSize     = 0;
    BlockInfo->WastedSize   = 0;
    BlockInfo->ValidEntries = 0;
  }

  return Status;
}

// Sanity/Correctness checks the CPER header fields
EFI_STATUS
EFIAPI
ErstValidateCperHeader (
  IN EFI_COMMON_ERROR_RECORD_HEADER  *Cper
  )
{
  CPER_ERST_PERSISTENCE_INFO  *CperPI;

  if ((Cper->SignatureStart != EFI_ERROR_RECORD_SIGNATURE_START) ||
      (Cper->Revision != EFI_ERROR_RECORD_REVISION) ||
      (Cper->SignatureEnd != EFI_ERROR_RECORD_SIGNATURE_END))
  {
    DEBUG ((DEBUG_ERROR, "%a: Cper Signature/Revision validation failed\n", __FUNCTION__));
    DEBUG ((DEBUG_INFO, "%a: Cper SignatureStart = 0x%x expected 0x%x\n", __FUNCTION__, Cper->SignatureStart, EFI_ERROR_RECORD_SIGNATURE_START));
    DEBUG ((DEBUG_INFO, "%a: Cper Revision = 0x%x expected 0x%x\n", __FUNCTION__, Cper->Revision, EFI_ERROR_RECORD_REVISION));
    DEBUG ((DEBUG_INFO, "%a: Cper SignatureEnd = 0x%x expected 0x%x\n", __FUNCTION__, Cper->SignatureEnd, EFI_ERROR_RECORD_SIGNATURE_END));
    return EFI_INCOMPATIBLE_VERSION;
  }

  if ((Cper->RecordID == ERST_FIRST_RECORD_ID) || (Cper->RecordID == ERST_INVALID_RECORD_ID)) {
    DEBUG ((DEBUG_ERROR, "%a: RecordId validation failed\n", __FUNCTION__));
    return EFI_COMPROMISED_DATA;
  }

  CperPI = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  if ((CperPI->Signature != ERST_RECORD_SIGNATURE) ||
      (CperPI->Major != ERST_RECORD_VERSION_MAJOR) ||
      (CperPI->Minor != ERST_RECORD_VERSION_MINOR))
  {
    DEBUG ((DEBUG_ERROR, "%a: PersistenceInfo Signature/Revision validation failed\n", __FUNCTION__));
    DEBUG ((DEBUG_INFO, "%a: PersistenceInfo Signature = 0x%x expected 0x%x\n", __FUNCTION__, CperPI->Signature, ERST_RECORD_SIGNATURE));
    DEBUG ((DEBUG_INFO, "%a: PersistenceInfo Major = 0x%x expected 0x%x\n", __FUNCTION__, CperPI->Major, ERST_RECORD_VERSION_MAJOR));
    DEBUG ((DEBUG_INFO, "%a: PersistenceInfo Minor = 0x%x expected 0x%x\n", __FUNCTION__, CperPI->Minor, ERST_RECORD_VERSION_MINOR));
    return EFI_INCOMPATIBLE_VERSION;
  }

  if ((CperPI->Status != ERST_RECORD_STATUS_DELETED) &&
      (CperPI->Status != ERST_RECORD_STATUS_FREE) &&
      (CperPI->Status != ERST_RECORD_STATUS_INCOMING) &&
      (CperPI->Status != ERST_RECORD_STATUS_INVALID) &&
      (CperPI->Status != ERST_RECORD_STATUS_OUTGOING) &&
      (CperPI->Status != ERST_RECORD_STATUS_VALID))
  {
    DEBUG ((DEBUG_ERROR, "%a: Status value 0x%x isn't a known status value\n", __FUNCTION__, CperPI->Status));

    DEBUG ((DEBUG_ERROR, "%a: CPER->SignatureStart = 0x%08x Revision      = 0x%04x     SigantureEnd   = 0x%08x\n", __FUNCTION__, Cper->SignatureStart, Cper->Revision, Cper->SignatureEnd));
    DEBUG ((DEBUG_ERROR, "%a: CPER->SectionCount   = 0x%04x     ErrorSeverity = 0x%08x ValidationBits = 0x%08x\n", __FUNCTION__, Cper->SectionCount, Cper->ErrorSeverity, Cper->ValidationBits));
    DEBUG ((DEBUG_ERROR, "%a: CPER->RecordLength   = 0x%08x TimeStamp(Sec)= 0x%02x       RecordID       = 0x%016llx\n", __FUNCTION__, Cper->RecordLength, Cper->TimeStamp.Seconds, Cper->RecordID));
    DEBUG ((
      DEBUG_ERROR,
      "%a: CPER->Header1 = 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n",
      __FUNCTION__,
      ((UINT64 *)(Cper))[0],
      ((UINT64 *)(Cper))[1],
      ((UINT64 *)(Cper))[2],
      ((UINT64 *)(Cper))[3],
      ((UINT64 *)(Cper))[4],
      ((UINT64 *)(Cper))[5],
      ((UINT64 *)(Cper))[6],
      ((UINT64 *)(Cper))[7]
      ));
    DEBUG ((
      DEBUG_ERROR,
      "%a: CPER->Header2 = 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n",
      __FUNCTION__,
      ((UINT64 *)(Cper))[8],
      ((UINT64 *)(Cper))[9],
      ((UINT64 *)(Cper))[10],
      ((UINT64 *)(Cper))[11],
      ((UINT64 *)(Cper))[12],
      ((UINT64 *)(Cper))[13],
      ((UINT64 *)(Cper))[14],
      ((UINT64 *)(Cper))[15]
      ));
    DEBUG ((
      DEBUG_ERROR,
      "%a: CPER->Data = 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n",
      __FUNCTION__,
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[0],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[1],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[2],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[3],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[4],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[5],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[6],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[7]
      ));
    DEBUG ((
      DEBUG_ERROR,
      "%a: CPER->Data = 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n",
      __FUNCTION__,
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[8],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[9],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[10],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[11],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[12],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[13],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[14],
      ((UINT64 *)((UINT64)Cper + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[15]
      ));

    return EFI_COMPROMISED_DATA;
  }

  return EFI_SUCCESS;
}

// Sanity checks the ID and Length against the header, and then Validates the Header
EFI_STATUS
EFIAPI
ErstValidateRecord (
  IN EFI_COMMON_ERROR_RECORD_HEADER  *Cper,
  IN UINT64                          RecordID,
  IN UINT64                          RecordLength
  )
{
  if ((RecordID == ERST_FIRST_RECORD_ID) || (RecordID == ERST_INVALID_RECORD_ID)) {
    DEBUG ((DEBUG_ERROR, "%a: RecordId invalid\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if ((RecordID != Cper->RecordID) ||
      (RecordLength != Cper->RecordLength))
  {
    DEBUG ((DEBUG_ERROR, "%a: RecordId or RecordLength doesn't match tracking data\n", __FUNCTION__));
    DEBUG ((DEBUG_INFO, "%a: RecordId 0x%lx has ID 0x%x in Flash\n", __FUNCTION__, RecordID, Cper->RecordID));
    DEBUG ((DEBUG_INFO, "%a: RecordLength 0x%lx is Length 0x%x in Flash\n", __FUNCTION__, RecordLength, Cper->RecordLength));
    return EFI_COMPROMISED_DATA;
  }

  return ErstValidateCperHeader (Cper);
}

// Copies all valid records to another block, so that this block can be erased when needed
EFI_STATUS
EFIAPI
ErstReclaimBlock (
  ERST_BLOCK_INFO  *BlockInfo
  )
{
  EFI_STATUS      Status;
  UINT16          CperInfoIndex = 0;
  ERST_CPER_INFO  *CperInfo;
  UINT32          BlockEnd = BlockInfo->Base + mErrorSerialization.BlockSize;

  // Mark block as being reclaimed
  if (BlockInfo->ValidEntries > 0) {
    BlockInfo->ValidEntries = -BlockInfo->ValidEntries;
  }

  // Make sure there's no OUTGOING before we try to move other records around
  // This happens when RelocateOutgoing requires reclaiming its own block to make space
  if (mErrorSerialization.OutgoingCperInfo != NULL) {
    Status = ErstRelocateOutgoing ();
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  while (BlockInfo->ValidEntries < 0) {
    // Continue searching the CperInfo list for a Cper in the block
    do {
      CperInfo = &mErrorSerialization.CperInfo[CperInfoIndex];
      if ((CperInfo->RecordOffset >= BlockInfo->Base) &&
          (CperInfo->RecordOffset < BlockEnd))
      {
        break;
      } else {
        CperInfoIndex++;
        // Note: Should be imposible without data corruption or code bug
        if (CperInfoIndex >= mErrorSerialization.RecordCount) {
          DEBUG ((DEBUG_ERROR, "%a: Error locating all the Cpers in the Block\n", __FUNCTION__));
          return EFI_NOT_FOUND;
        }
      }
    } while (CperInfoIndex < mErrorSerialization.RecordCount);

    Status = ErstRelocateRecord (CperInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  // All valid entries have been relocated. Block can be erased
  Status = ErstEraseBlock (BlockInfo);

  return Status;
}

// Finds the requested-sized space for a new record, starting with the most-resent block
EFI_STATUS
EFIAPI
ErstFindFreeSpace (
  IN UINT64                  RecordLength,
  OUT UINT32                 *NewOffset,
  IN                BOOLEAN  DummyOp
  )
{
  EFI_STATUS       Status;
  UINT32           FreeOffset         = 0;
  UINT32           BlockIndex         = 0;
  UINT32           AdjustedBlockIndex = mErrorSerialization.MostRecentBlock;
  ERST_BLOCK_INFO  *BlockInfo;
  ERST_BLOCK_INFO  *FreeBlockInfo       = NULL;
  ERST_BLOCK_INFO  *WastedBlockInfo     = NULL;
  UINT32           FreeBlockCount       = 0;
  UINT32           ReclaimingBlockCount = 0;

  // Find a used block with enough free space if possible
  do {
    AdjustedBlockIndex = (BlockIndex + mErrorSerialization.MostRecentBlock)%mErrorSerialization.NumBlocks;
    BlockInfo          = &mErrorSerialization.BlockInfo[AdjustedBlockIndex];
    DEBUG ((DEBUG_VERBOSE, "%a: Block %u has UsedSize 0x%x, WastedSize 0x%x\n", __FUNCTION__, AdjustedBlockIndex, BlockInfo->UsedSize, BlockInfo->WastedSize));
    if ((BlockInfo->ValidEntries > 0) &&
        (BlockInfo->UsedSize + RecordLength <= mErrorSerialization.BlockSize))
    {
      FreeOffset = BlockInfo->UsedSize + BlockInfo->Base;
      Status     = EFI_SUCCESS;
      goto ReturnStatus;
    } else if (BlockInfo->ValidEntries == 0) {
      if ((BlockInfo->UsedSize == 0) && (BlockInfo->WastedSize == 0)) {
        // Entire block is free and ready to be written
        if (FreeBlockInfo == NULL) {
          FreeBlockInfo = BlockInfo;
        }

        FreeBlockCount++;
      } else {
        // Block has no valid entries so can easily be erased
        WastedBlockInfo = BlockInfo;
      }
    } else if (BlockInfo->ValidEntries >= 0 ) {
      if (WastedBlockInfo && ((WastedBlockInfo->UsedSize - WastedBlockInfo->WastedSize) < (BlockInfo->UsedSize - BlockInfo->WastedSize))) {
        // The current block has more waste than the previously wasted block, so set it as the wasted block
        WastedBlockInfo = BlockInfo;
      } else if (BlockInfo->UsedSize - BlockInfo->WastedSize + RecordLength <= mErrorSerialization.BlockSize) {
        // The current block is the first block found with usable waste
        WastedBlockInfo = BlockInfo;
      }

      // else there's no guarantee reclaiming the block will create enough space, so don't try to
    } else if (BlockInfo->ValidEntries < 0) {
      ReclaimingBlockCount++;
    }

    BlockIndex++;
  } while (BlockIndex < mErrorSerialization.NumBlocks);

  // Start a free block. Always maintain a free block after reclaims are done
  if (((FreeBlockCount+ReclaimingBlockCount) > 1) &&
      (FreeBlockInfo != NULL))
  {
    FreeOffset = FreeBlockInfo->Base;
    BlockInfo  = FreeBlockInfo;
    Status     = EFI_SUCCESS;
    goto ReturnStatus;
  } else if ((WastedBlockInfo != NULL) &&
             (mErrorSerialization.OutgoingCperInfo == NULL) && !DummyOp)
  {
    // Only have one or less free block, so reclaim the most-wasted block and then try again
    Status = ErstReclaimBlock (WastedBlockInfo);
    if (EFI_ERROR (Status)) {
      goto ReturnStatus;
    }

    return ErstFindFreeSpace (RecordLength, NewOffset, DummyOp);
  } else {
    // No free or wasted blocks
    DEBUG ((DEBUG_ERROR, "%a: No free or wasted blocks found, trying to find space for 0x%llx bytes\n", __FUNCTION__, RecordLength));
    Status = EFI_OUT_OF_RESOURCES;
  }

ReturnStatus:
  if (!EFI_ERROR (Status)) {
    *NewOffset = FreeOffset;
  }

  return Status;
}

// Finds the BlockInfo for the block that the Record is part of
ERST_BLOCK_INFO *
ErstGetBlockOfRecord (
  IN ERST_CPER_INFO  *Record
  )
{
  UINT32           BlockIndex;
  ERST_BLOCK_INFO  *BlockInfo;

  for (BlockIndex = 0; BlockIndex < mErrorSerialization.NumBlocks; BlockIndex++) {
    BlockInfo = &mErrorSerialization.BlockInfo[BlockIndex];
    if ((Record->RecordOffset >= BlockInfo->Base) &&
        (Record->RecordOffset < (BlockInfo->Base + mErrorSerialization.BlockSize)))
    {
      return BlockInfo;
    }
  }

  return NULL;
}

// Finds free space and allocates it from its block
EFI_STATUS
EFIAPI
ErstPrepareNewRecord (
  IN UINT64              RecordId,
  IN UINT64              RecordLength,
  IN OUT ERST_CPER_INFO  *Record,
  IN BOOLEAN             DummyOp
  )
{
  EFI_STATUS       Status;
  ERST_BLOCK_INFO  *BlockInfo;

  if (Record == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Record parameter was NULL\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Record->RecordId     = RecordId;
  Record->RecordLength = RecordLength;

  Status = ErstFindFreeSpace (RecordLength, &Record->RecordOffset, DummyOp);

  if (!EFI_ERROR (Status)) {
    BlockInfo = ErstGetBlockOfRecord (Record);
    if (BlockInfo != NULL) {
      mErrorSerialization.UnsyncedSpinorChanges++;
      BlockInfo->UsedSize += RecordLength;
      BlockInfo->ValidEntries++;
    } else {
      // GCOVR_EXCL_START - Unable to force ErstFindFreeSpace to return an invalid RecordOffset
      DEBUG ((DEBUG_ERROR, "%a: Block Info for Record not found\n", __FUNCTION__));
      Status = EFI_NOT_FOUND;
      // GCOVR_EXCL_STOP
    }
  }

  return Status;
}

// Undoes the allocation for a record that wasn't written due to an error
EFI_STATUS
EFIAPI
ErstUndoAllocateRecord (
  ERST_CPER_INFO  *Record
  )
{
  EFI_STATUS       Status;
  ERST_BLOCK_INFO  *BlockInfo;

  if (Record == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  BlockInfo = ErstGetBlockOfRecord (Record);
  if (BlockInfo != NULL) {
    BlockInfo->UsedSize -= Record->RecordLength;
    BlockInfo->ValidEntries--;
    mErrorSerialization.UnsyncedSpinorChanges--;
    Status = EFI_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Block Info for Record not found\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
  }

  return Status;
}

// Frees up the block space used by a record (Note: often is followed by ErstDeallocateRecord)
EFI_STATUS
EFIAPI
ErstFreeRecord (
  ERST_CPER_INFO  *Record
  )
{
  EFI_STATUS       Status = EFI_SUCCESS;
  ERST_BLOCK_INFO  *BlockInfo;

  if (Record == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Can't free a NULL record\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: Freeing RecordId %llx\n", __FUNCTION__, Record->RecordId));
  BlockInfo = ErstGetBlockOfRecord (Record);
  if (BlockInfo != NULL) {
    BlockInfo->WastedSize += Record->RecordLength;
    if (BlockInfo->ValidEntries > 0) {
      BlockInfo->ValidEntries--;
    } else if (BlockInfo->ValidEntries < 0) {
      BlockInfo->ValidEntries++;
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Block Info for Record not found\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
  }

  return Status;
}

// Read out the existing record and then write it into a different block
EFI_STATUS
EFIAPI
ErstRelocateRecord (
  ERST_CPER_INFO  *CperInfo
  )
{
  EFI_STATUS                      Status;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  ERST_CPER_INFO                  NewRecord;

  DEBUG ((DEBUG_INFO, "%a: record 0x%p ID 0x%x\n", __FUNCTION__, CperInfo, CperInfo->RecordId));

  Cper = ErstAllocatePoolRecord (CperInfo->RecordLength);
  if (Cper == NULL) {
    // GCOVR_EXCL_START - won't test allocation errors
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate temp space for relocated record\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ReturnStatus;
    // GCOVR_EXCL_STOP
  }

  // Read (and validate) the old record
  Status = ErstReadSpiNor (Cper, CperInfo->RecordOffset, CperInfo->RecordLength);
  if (!EFI_ERROR (Status)) {
    Status = ErstValidateRecord (Cper, CperInfo->RecordId, CperInfo->RecordLength);
    if (EFI_ERROR (Status)) {
      goto ReturnStatus;
    }
  }

  // Write it in a new location
  Status = ErstPrepareNewRecord (CperInfo->RecordId, CperInfo->RecordLength, &NewRecord, FALSE);
  if (EFI_ERROR (Status)) {
    goto ReturnStatus;
  }

  Status = ErstWriteRecord (Cper, CperInfo, &NewRecord, FALSE);
  if (!EFI_ERROR (Status)) {
    // The prepared record has been written, so we're in sync
    mErrorSerialization.UnsyncedSpinorChanges--;
  } else {
    ErstUndoAllocateRecord (&NewRecord);
  }

ReturnStatus:
  if (Cper != NULL) {
    ErstFreePoolRecord (Cper);
    Cper = NULL;
  }

  return Status;
}

// Returns the Index into the array of BlockInfo corresponding to the Block the Record is in
UINT16
ErstGetBlockIndexOfRecord (
  IN ERST_CPER_INFO  *Record
  )
{
  UINT32           BlockIndex;
  ERST_BLOCK_INFO  *BlockInfo;

  for (BlockIndex = 0; BlockIndex < mErrorSerialization.NumBlocks; BlockIndex++) {
    BlockInfo = &mErrorSerialization.BlockInfo[BlockIndex];
    if ((Record->RecordOffset >= BlockInfo->Base) &&
        (Record->RecordOffset < (BlockInfo->Base + mErrorSerialization.BlockSize)))
    {
      return BlockIndex;
    }
  }

  DEBUG ((DEBUG_ERROR, "%a: Record not found\n", __FUNCTION__));
  return 0;
}

// Removes the record from the list of valid records
EFI_STATUS
EFIAPI
ErstDeallocateRecord (
  IN ERST_CPER_INFO  *Record
  )
{
  if (Record == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Note: we have to move the whole list to fill the hole, rather than just move the last record into the
  // hole, since the Linux driver assumes that records will never be reordered relative to each other.
  mErrorSerialization.RecordCount--;
  if (Record != &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount]) {
    DEBUG ((DEBUG_VERBOSE, "%a: Moving 0x%llx bytes (0x%p - 0x%p)\n", __FUNCTION__, (VOID *)&mErrorSerialization.CperInfo[mErrorSerialization.RecordCount] - (VOID *)Record, &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount], Record));
    CopyMem (Record, Record+1, (VOID *)&mErrorSerialization.CperInfo[mErrorSerialization.RecordCount] - (VOID *)Record);
  }

  SetMem (&mErrorSerialization.CperInfo[mErrorSerialization.RecordCount], sizeof (ERST_CPER_INFO), 0x0);

  // Also need to shift incoming/outgoing if they were after the deleted record
  if (mErrorSerialization.IncomingCperInfo > Record) {
    mErrorSerialization.IncomingCperInfo--;
  } else if (mErrorSerialization.IncomingCperInfo == Record) {
    mErrorSerialization.IncomingCperInfo = NULL;
  }

  if (mErrorSerialization.OutgoingCperInfo > Record) {
    mErrorSerialization.OutgoingCperInfo--;
  } else if (mErrorSerialization.OutgoingCperInfo == Record) {
    mErrorSerialization.OutgoingCperInfo = NULL;
  }

  mErrorSerialization.UnsyncedSpinorChanges--;
  return EFI_SUCCESS;
}

// Mark the record as DELETED in the SPINOR and deallocate it from tracking
EFI_STATUS
EFIAPI
ErstClearRecord (
  IN ERST_CPER_INFO  *Record
  )
{
  EFI_STATUS  Status;
  UINT8       DeletedStatus = ERST_RECORD_STATUS_DELETED;

  if ((Record < &mErrorSerialization.CperInfo[0]) ||
      (Record > &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount-1]))
  {
    DEBUG ((DEBUG_ERROR, "%a: Record pointer out of bounds\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  Status = ErstWriteCperStatus (&DeletedStatus, Record);
  if (!EFI_ERROR (Status)) {
    mErrorSerialization.UnsyncedSpinorChanges++; // Wrote SPINOR
    Status = ErstFreeRecord (Record);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = ErstDeallocateRecord (Record);
  }

  return Status;
}

// Adds tracking data for a new record to the array of Valid records
EFI_STATUS
EFIAPI
ErstAllocateNewRecord (
  IN ERST_CPER_INFO   *NewRecord,
  OUT ERST_CPER_INFO  **AllocatedRecord
  )
{
  if ((NewRecord >= &mErrorSerialization.CperInfo[0]) &&
      (NewRecord < &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount]))
  {
    mErrorSerialization.UnsyncedSpinorChanges++;
    if (AllocatedRecord) {
      *AllocatedRecord = NULL;
    }

    return EFI_SUCCESS;
  }

  if (mErrorSerialization.RecordCount < mErrorSerialization.MaxRecords) {
    CopyMem (&mErrorSerialization.CperInfo[mErrorSerialization.RecordCount], NewRecord, sizeof (ERST_CPER_INFO));
    if (AllocatedRecord) {
      *AllocatedRecord = &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount];
    }

    mErrorSerialization.RecordCount++;
    mErrorSerialization.UnsyncedSpinorChanges++;
    return EFI_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Max record count reached\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }
}

// Write the provided Record into the SPI-NOR by creating a new copy and
// deleting the old copy, if any. In certain circumstances it would work
// to overwrite the current record, but that's very data dependent and not
// implemented here.
EFI_STATUS
EFIAPI
ErstWriteRecord (
  IN EFI_COMMON_ERROR_RECORD_HEADER  *Cper,
  IN ERST_CPER_INFO                  *CurrentRecord, // Optional, if replacing
  IN ERST_CPER_INFO                  *NewRecord,     // Required
  IN BOOLEAN                         DummyOp
  )
{
  EFI_STATUS                  Status;
  ERST_CPER_INFO              *AllocatedRecord;
  CPER_ERST_PERSISTENCE_INFO  *CperPI        = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
  UINT8                       OutgoingStatus = ERST_RECORD_STATUS_OUTGOING;
  UINT8                       DeletedStatus  = ERST_RECORD_STATUS_DELETED;
  UINT64                      StartTime __attribute__ ((unused));

  DEBUG_CODE (
    StartTime = GetTimeInNanoSecond (GetPerformanceCounter ());
    );

  if (NewRecord == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: NewRecord parm was NULL\n", __FUNCTION__));
    Status = EFI_INVALID_PARAMETER;
    goto ReturnStatus;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: Record=0x%p, ID=0x%llx, Len=0x%x, Offset=0x%x\n", __FUNCTION__, NewRecord, NewRecord->RecordId, NewRecord->RecordLength, NewRecord->RecordOffset));
  // Make sure we're not creating a second OUTGOING with the upcoming write
  if ((mErrorSerialization.OutgoingCperInfo != NULL) &&
      (mErrorSerialization.OutgoingCperInfo != CurrentRecord))
  {
    DEBUG ((DEBUG_ERROR, "%a: Unable write record because there's already an OUTGOING record\n", __FUNCTION__));
    if (CurrentRecord != NULL) {
      DEBUG ((DEBUG_INFO, "%a:  Current=0x%p, ID=0x%llx, Len=0x%llx, Offset=0x%x\n", __FUNCTION__, CurrentRecord, CurrentRecord->RecordId, CurrentRecord->RecordLength, CurrentRecord->RecordOffset));
    }

    DEBUG ((DEBUG_INFO, "%a: Outgoing=0x%p, ID=0x%llx, Len=0x%llx, Offset=0x%x\n", __FUNCTION__, mErrorSerialization.OutgoingCperInfo, mErrorSerialization.OutgoingCperInfo->RecordId, mErrorSerialization.OutgoingCperInfo->RecordLength, mErrorSerialization.OutgoingCperInfo->RecordOffset));
    Status = EFI_UNSUPPORTED;
    goto ReturnStatus;
  }

  CperPI->Signature = ERST_RECORD_SIGNATURE;
  CperPI->Major     = ERST_RECORD_VERSION_MAJOR;
  CperPI->Minor     = ERST_RECORD_VERSION_MINOR;
  CperPI->Status    = ERST_RECORD_STATUS_INCOMING;

  Status = ErstValidateCperHeader (Cper);
  if (EFI_ERROR (Status)) {
    goto ReturnStatus;
  }

  // Either allocate space for a new record, or eventually replace current record with new record
  if (CurrentRecord == NULL) {
    Status = ErstAllocateNewRecord (NewRecord, &AllocatedRecord);
    if (EFI_ERROR (Status)) {
      goto ReturnStatus;
    }
  } else {
    mErrorSerialization.UnsyncedSpinorChanges++;
    AllocatedRecord = NULL;
  }

  // Making SPINOR changes from this point on, so skip to the end for Dummy Write
  if (DummyOp) {
    // Free up the dummy allocation, since it's not actually written or staying present
    ErstDeallocateRecord (AllocatedRecord);
    goto ReturnStatus;
  }

  Status = ErstWriteCperStatus (&CperPI->Status, NewRecord);
  if (EFI_ERROR (Status)) {
    // If the first SPINOR write fails, undo tracking updates and stay in sync
    ErstDeallocateRecord (AllocatedRecord);
    goto ReturnStatus;
  }

  mErrorSerialization.UnsyncedSpinorChanges++; // Started SPINOR write sequence

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Writing ID 0x%x to offset 0x%x with length 0x%x\n",
    __FUNCTION__,
    NewRecord->RecordId,
    NewRecord->RecordOffset,
    NewRecord->RecordLength
    ));
  Status = ErstWriteSpiNor (Cper, NewRecord->RecordOffset, NewRecord->RecordLength);
  if (EFI_ERROR (Status)) {
    // GCOVR_EXCL_START - can't test Flash failure here, since faulty flash causes above Status write to fail
    goto ReturnStatus;
    // GCOVR_EXCL_STOP
  }

  if (CurrentRecord) {
    DEBUG ((DEBUG_VERBOSE, "%a:  RC: 0x%x Writing Outgoing Current=0x%p, ID=0x%llx, Len=0x%llx, Offset=0x%x\n", __FUNCTION__, mErrorSerialization.RecordCount, CurrentRecord, CurrentRecord->RecordId, CurrentRecord->RecordLength, CurrentRecord->RecordOffset));
    Status = ErstWriteCperStatus (&OutgoingStatus, CurrentRecord);
    if (EFI_ERROR (Status)) {
      // GCOVR_EXCL_START - can't test Flash failure here, since faulty flash causes above Status write to fail
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }
  }

  CperPI->Status = ERST_RECORD_STATUS_VALID;
  Status         = ErstWriteCperStatus (&CperPI->Status, NewRecord);
  if (EFI_ERROR (Status)) {
    // GCOVR_EXCL_START - can't test Flash failure here, since faulty flash causes above Status write to fail
    goto ReturnStatus;
    // GCOVR_EXCL_STOP
  }

  if (CurrentRecord) {
    DEBUG ((DEBUG_VERBOSE, "%a: RC: 0x%x Replacing Current=0x%p, ID=0x%llx, Len=0x%llx, Offset=0x%x\n", __FUNCTION__, mErrorSerialization.RecordCount, CurrentRecord, CurrentRecord->RecordId, CurrentRecord->RecordLength, CurrentRecord->RecordOffset));
    Status = ErstWriteCperStatus (&DeletedStatus, CurrentRecord);
    if (EFI_ERROR (Status)) {
      goto ReturnStatus;
    }

    mErrorSerialization.UnsyncedSpinorChanges++; // Wrote SPINOR

    Status = ErstFreeRecord (CurrentRecord);
    if (EFI_ERROR (Status)) {
      goto ReturnStatus;
    }

    if ((NewRecord >= &mErrorSerialization.CperInfo[0]) &&
        (NewRecord < &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount]))
    {
      Status = ErstDeallocateRecord (CurrentRecord);
    } else {
      // Instead of deallocating CurrentRecord, we reuse the CurrentRecord allocation for NewRecord
      CopyMem (CurrentRecord, NewRecord, sizeof (ERST_CPER_INFO));
      CurrentRecord = NULL;

      mErrorSerialization.UnsyncedSpinorChanges--;
    }
  }

  mErrorSerialization.UnsyncedSpinorChanges--; // Completed SPINOR write sequence

  mErrorSerialization.MostRecentBlock = ErstGetBlockIndexOfRecord (NewRecord);

ReturnStatus:
  if ((!EFI_ERROR (Status)) && !DummyOp) {
    // Now that all writes have completed successfully, we're in sync again
    mErrorSerialization.UnsyncedSpinorChanges--; // Allocated record was written
  }

  DEBUG_CODE (
  {
    UINT64 EndTime;
    UINT64 ElapsedTime;
    EndTime = GetTimeInNanoSecond (GetPerformanceCounter ());
    if (EndTime > StartTime) {
      ElapsedTime = EndTime-StartTime;
    } else {
      ElapsedTime = (MAX_UINT64 - StartTime) + EndTime;
    }

    WriteRecordTime = ElapsedTime;
  }
    );

  return Status;
}

// Read the specified record into the specified buffer location
EFI_STATUS
EFIAPI
ErstReadRecord (
  IN UINT64                          RecordID,
  IN EFI_COMMON_ERROR_RECORD_HEADER  *Cper,
  IN UINT64                          MaxLength
  )
{
  ERST_CPER_INFO  *Record;
  UINT32          Status;

  Record = ErstFindRecord (RecordID);
  if (Record == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Record not found\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  } else if (Record->RecordLength > MaxLength) {
    DEBUG ((DEBUG_ERROR, "%a: Record doesn't fit at offset\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  } else {
    Status = ErstReadSpiNor (Cper, Record->RecordOffset, Record->RecordLength);
    if (!EFI_ERROR (Status)) {
      Status = ErstValidateCperHeader (Cper);
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Spinor read failed with Status=%u\n", __FUNCTION__, Status));
    }
  }

  return Status;
}

// Returns the next valid record ID after the given one, if any
UINT64
ErstGetNextRecordID (
  IN UINT64  RecordID
  )
{
  INTN  RecordIndex = 0;

  if (mErrorSerialization.RecordCount == 0) {
    return ERST_INVALID_RECORD_ID;
  }

  while ((RecordIndex < (mErrorSerialization.RecordCount-1)) &&
         (mErrorSerialization.CperInfo[RecordIndex].RecordId != RecordID))
  {
    RecordIndex++;
  }

  if (RecordIndex < (mErrorSerialization.RecordCount-1)) {
    return mErrorSerialization.CperInfo[RecordIndex+1].RecordId;
  } else {
    return mErrorSerialization.CperInfo[0].RecordId;
  }
}

// Clear the interrupt status bit that is used as a busy indicator to the OS
VOID
ErstClearBusy (
  VOID
  )
{
  // JDS TODO - do we need a memory sync call here to ensure memory is up to date when register bit flips?

  if (mErrorSerialization.ErstLicSwIoBase != 0) {
    MmioWrite32 (mErrorSerialization.ErstLicSwIoBase + INTR_CTLR_SW_IO_N_INTR_STATUS_CLR_0_OFFSET, 1);
  }
}

UINT32
ErstEfiStatusToAcpiStatus (
  EFI_STATUS  EfiStatus
  )
{
  // GCOVR_EXCL_START - Not going to test all the EFI return codes
  switch (EfiStatus) {
    case EFI_SUCCESS:
      return EFI_ACPI_6_4_ERST_STATUS_SUCCESS;
      break;
    case EFI_OUT_OF_RESOURCES:
    case EFI_VOLUME_FULL:
    case EFI_BUFFER_TOO_SMALL:
      return EFI_ACPI_6_4_ERST_STATUS_NOT_ENOUGH_SPACE;
      break;
    case EFI_NO_MEDIA:
    case EFI_NO_RESPONSE:
    case EFI_ACCESS_DENIED:
    case EFI_WRITE_PROTECTED:
    case EFI_NO_MAPPING:
    case EFI_NOT_READY:
    case EFI_TIMEOUT:
      return EFI_ACPI_6_4_ERST_STATUS_HARDWARE_NOT_AVAILABLE;
      break;
    case EFI_LOAD_ERROR:
    case EFI_INVALID_PARAMETER:
    case EFI_UNSUPPORTED:
    case EFI_BAD_BUFFER_SIZE:
    case EFI_VOLUME_CORRUPTED:
    case EFI_DEVICE_ERROR:
    case EFI_INCOMPATIBLE_VERSION:
    case EFI_MEDIA_CHANGED:
    case EFI_NOT_STARTED:
    case EFI_ALREADY_STARTED:
    case EFI_ABORTED:
    case EFI_ICMP_ERROR:
    case EFI_TFTP_ERROR:
    case EFI_PROTOCOL_ERROR:
    case EFI_SECURITY_VIOLATION:
    case EFI_CRC_ERROR:
    case EFI_INVALID_LANGUAGE:
    case EFI_COMPROMISED_DATA:
    case EFI_HTTP_ERROR:
      return EFI_ACPI_6_4_ERST_STATUS_FAILED;
      break;
    case EFI_NOT_FOUND:
    case EFI_END_OF_MEDIA:
    case EFI_END_OF_FILE:
      return EFI_ACPI_6_4_ERST_STATUS_RECORD_NOT_FOUND;
      break;
    default:
      return EFI_ACPI_6_4_ERST_STATUS_FAILED;
      break;
  }

  // GCOVR_EXCL_STOP
}

/**
 * MMI handler for ERST service. This assumes that the shared ERST memory contains a valid request from the OS.
 *
 * @params[in]   DispatchHandle   Handle of the registered MMI..
 * @params[in]   RegisterContext  Context Info from MMI root handler.
 * @params[out]  CommBuffer       Comm Buffer sent by the client.
 * @params[out]  CommBufferSize   Comm Buffer Size sent from the client.
 *
 * @retval       EFI_SUCCESS     Always return Success to the MMI root handler
 *                               The error from the service will be in-band of
 *                               the service call.
 **/
EFI_STATUS
EFIAPI
ErrorSerializationEventHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  ERST_COMM_STRUCT                *ERSTComm;
  UINT32                          AcpiStatus;
  EFI_STATUS                      EfiStatus;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  EFI_COMMON_ERROR_RECORD_HEADER  *NewCper;
  UINT64                          OSRecordLength;
  UINT64                          OSRecordOffset;
  UINT64                          OSRecordID;
  UINT64                          MaxLength;
  ERST_CPER_INFO                  *Record;
  ERST_CPER_INFO                  NewRecord;
  BOOLEAN                         DummyOp;
  UINT64                          StartTime __attribute__ ((unused));

  DEBUG_CODE (
    StartTime = GetTimeInNanoSecond (GetPerformanceCounter ());
    );

  DEBUG ((DEBUG_INFO, "%a: ERST Handler Entered\n", __FUNCTION__));

  // Note: must be initialized before any gotos
  ERSTComm = (ERST_COMM_STRUCT *)mErrorSerialization.BufferInfo.ErstBase;
  NewCper  = NULL;
  DummyOp  = FALSE;

  // Default status, which will be interpreted at the end
  AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_SUCCESS;
  EfiStatus  = mErrorSerialization.InitStatus;

  if (EFI_ERROR (EfiStatus)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to handle ERST request due to initialization status failed (%u) %r\n", __FUNCTION__, EfiStatus, EfiStatus));
    goto ReturnStatus;
  }

  if ((ERSTComm == NULL) ||
      (mErrorSerialization.UnsyncedSpinorChanges != 0) ||
      (mErrorSerialization.IncomingCperInfo != NULL) ||
      (mErrorSerialization.OutgoingCperInfo != NULL))
  {
    EfiStatus = ErrorSerializationReInit ();
    if (EFI_ERROR (EfiStatus)) {
      DEBUG ((DEBUG_ERROR, "%a: ErrorSerialization driver is out of sync with the SPINOR and failed recovery attempt!\n", __FUNCTION__));
      goto ReturnStatus;
    }

    ERSTComm = (ERST_COMM_STRUCT *)mErrorSerialization.BufferInfo.ErstBase;
    if (ERSTComm == NULL) {
      // GCOVR_EXCL_START - shouldn't be possible for ReInit to succeed but have no ErstBase
      EfiStatus = EFI_NO_MAPPING;
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }
  }

  DEBUG ((DEBUG_INFO, "%a: ERSTComm is at 0x%p, operation is 0x%x, Read/Clear ID is 0x%llx, Status Invalid is %u\n", __FUNCTION__, ERSTComm, ERSTComm->Operation, ERSTComm->RecordID, ERSTComm->Status & ERST_STATUS_INVALID_MASK));
  // Save off the inputs from OS before validating them, in case malicious code tries to change them after validation
  OSRecordOffset = ERSTComm->RecordOffset;
  OSRecordID     = ERSTComm->RecordID;

  /* Parse the requested action */
  switch (ERSTComm->Operation) {
    case ERST_OPERATION_DUMMY_WRITE:
      DummyOp = TRUE;
    case ERST_OPERATION_WRITE:
      /* Write the record at RecordOffset into the storage as RecordID */
      if (OSRecordOffset > (mErrorSerialization.BufferInfo.ErrorLogInfo.Length - sizeof (EFI_COMMON_ERROR_RECORD_HEADER))) {
        DEBUG ((DEBUG_WARN, "%a: RecordOffset overflows ErrorLogBuffer\n", __FUNCTION__));
        AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_FAILED;
        break;
      }

      Cper = (EFI_COMMON_ERROR_RECORD_HEADER *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset);
      DEBUG_CODE (
    {
      DEBUG ((
        DEBUG_INFO,
        "%a: PhysicalBase = 0x%p OsRecordOffset = 0x%p Cper = 0x%p\n",
        __FUNCTION__,
        (void *)mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase,
        (void *)OSRecordOffset,
        Cper
        ));
      DEBUG ((DEBUG_INFO, "%a: CPER->SignatureStart = 0x%08x Revision      = 0x%04x     SigantureEnd   = 0x%08x\n", __FUNCTION__, Cper->SignatureStart, Cper->Revision, Cper->SignatureEnd));
      DEBUG ((DEBUG_INFO, "%a: CPER->SectionCount   = 0x%04x     ErrorSeverity = 0x%08x ValidationBits = 0x%08x\n", __FUNCTION__, Cper->SectionCount, Cper->ErrorSeverity, Cper->ValidationBits));
      DEBUG ((DEBUG_INFO, "%a: CPER->RecordLength   = 0x%08x TimeStamp(Sec)= 0x%02x       RecordID       = 0x%016llx\n", __FUNCTION__, Cper->RecordLength, Cper->TimeStamp.Seconds, Cper->RecordID));
      DEBUG ((
        DEBUG_INFO,
        "%a: CPER->Header1 = 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n",
        __FUNCTION__,
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[0],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[1],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[2],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[3],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[4],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[5],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[6],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[7]
        ));
      DEBUG ((
        DEBUG_INFO,
        "%a: CPER->Header2 = 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n",
        __FUNCTION__,
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[8],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[9],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[10],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[11],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[12],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[13],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[14],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset))[15]
        ));
      DEBUG ((
        DEBUG_INFO,
        "%a: CPER->Data = 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n",
        __FUNCTION__,
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[0],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[1],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[2],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[3],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[4],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[5],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[6],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[7]
        ));
      DEBUG ((
        DEBUG_INFO,
        "%a: CPER->Data = 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n",
        __FUNCTION__,
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[8],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[9],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[10],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[11],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[12],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[13],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[14],
        ((UINT64 *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset + sizeof (EFI_COMMON_ERROR_RECORD_HEADER)))[15]
        ));
    }
        );
      // Save off the length and ID before validating them
      OSRecordLength = Cper->RecordLength;
      OSRecordID     = Cper->RecordID;

      if (OSRecordOffset + OSRecordLength > mErrorSerialization.BufferInfo.ErrorLogInfo.Length) {
        DEBUG ((DEBUG_WARN, "%a: RecordOffset (0x%lx) + RecordLength (0x%lx) overflows ErrorLogBuffer Length (0x%x)\n", __FUNCTION__, OSRecordOffset, OSRecordLength, mErrorSerialization.BufferInfo.ErrorLogInfo.Length));
        AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_FAILED;
        break;
      }

      NewCper = ErstAllocatePoolRecord (OSRecordLength);
      if (NewCper == NULL) {
        // GCOVR_EXCL_START - won't test allocation errors
        DEBUG ((DEBUG_WARN, "%a: Couldn't allocate space for Cper tracking\n", __FUNCTION__));
        AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_NOT_ENOUGH_SPACE;
        break;
        // GCOVR_EXCL_STOP
      }

      CopyMem (NewCper, Cper, OSRecordLength);

      Record    = ErstFindRecord (OSRecordID);
      EfiStatus = ErstPrepareNewRecord (OSRecordID, OSRecordLength, &NewRecord, DummyOp);
      if (EFI_ERROR (EfiStatus)) {
        DEBUG ((DEBUG_WARN, "%a: Couldn't prepare a new record\n", __FUNCTION__));
        break;
      }

      EfiStatus = ErstWriteRecord (NewCper, Record, &NewRecord, DummyOp);
      if ((EFI_ERROR (EfiStatus)) ||
          (DummyOp == TRUE))
      {
        ErstUndoAllocateRecord (&NewRecord);
      } else {
        // We've committed the record to the block, so we're in sync by keeping the block
        mErrorSerialization.UnsyncedSpinorChanges--;
      }

      // Only update the ERSTComm ID if we actually wrote a new record and need to point to it
      if (!EFI_ERROR (EfiStatus) &&
          !DummyOp &&
          (ERSTComm->RecordID == ERST_INVALID_RECORD_ID))
      {
        ERSTComm->RecordID = OSRecordID;
      }

      break;

    case ERST_OPERATION_READ:
      if (mErrorSerialization.RecordCount == 0) {
        AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_RECORD_STORE_EMPTY;
        DEBUG ((DEBUG_WARN, "%a: Record Store Empty\n", __FUNCTION__));
        break;
      }

      /* Determine where to put the error record */
      if (OSRecordOffset > (mErrorSerialization.BufferInfo.ErrorLogInfo.Length - sizeof (EFI_COMMON_ERROR_RECORD_HEADER))) {
        DEBUG ((DEBUG_WARN, "%a: RecordOffset overflows ErrorLogBuffer\n", __FUNCTION__));
        AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_NOT_ENOUGH_SPACE;
        break;
      }

      Cper      = (EFI_COMMON_ERROR_RECORD_HEADER *)(mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase + OSRecordOffset);
      MaxLength = mErrorSerialization.BufferInfo.ErrorLogInfo.Length - OSRecordOffset;

      if (OSRecordID == ERST_FIRST_RECORD_ID) {
        // Reading record ID 0 means reading the first valid record
        OSRecordID = mErrorSerialization.CperInfo[0].RecordId;
      }

      EfiStatus = ErstReadRecord (OSRecordID, Cper, MaxLength);
      if (!EFI_ERROR (EfiStatus)) {
        // If read success, update RecordID to next valid
        ERSTComm->RecordID = ErstGetNextRecordID (OSRecordID);
      } else if (EfiStatus == EFI_NOT_FOUND) {
        DEBUG ((DEBUG_WARN, "%a: RecordId not found\n", __FUNCTION__));
        // Set RecordID to a valid value if requested one not found
        ERSTComm->RecordID = mErrorSerialization.CperInfo[0].RecordId;
      }

      break;

    case ERST_OPERATION_CLEAR:
      if (OSRecordID == ERST_FIRST_RECORD_ID) {
        AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_FAILED;
        DEBUG ((DEBUG_WARN, "%a: Cannot clear RecordId 0 (\"First available\")\n", __FUNCTION__));
      } else if (mErrorSerialization.RecordCount == 0) {
        AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_RECORD_STORE_EMPTY;
        DEBUG ((DEBUG_WARN, "%a: Record Store Empty\n", __FUNCTION__));
      } else if (OSRecordID == ERST_INVALID_RECORD_ID) {
        AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_FAILED;
        DEBUG ((DEBUG_WARN, "%a: Cannot clear RecordId 0xFF...FF (\"Invalid ID\")\n", __FUNCTION__));
      } else {
        /* Find the error record in the storage and mark it as freed */
        Record = ErstFindRecord (OSRecordID);
        if (Record == NULL) {
          DEBUG ((DEBUG_WARN, "%a: RecordId not found\n", __FUNCTION__));
          AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_RECORD_NOT_FOUND;
        } else {
          EfiStatus = ErstClearRecord (Record);
        }
      }

      if (mErrorSerialization.RecordCount > 0) {
        ERSTComm->RecordID = mErrorSerialization.CperInfo[0].RecordId;
      } else {
        ERSTComm->RecordID = ERST_INVALID_RECORD_ID;
      }

      break;

    default:
      DEBUG ((DEBUG_WARN, "%a: Unknown operation %d\n", __FUNCTION__, ERSTComm->Operation));
      AcpiStatus = EFI_ACPI_6_4_ERST_STATUS_FAILED;
      break;
  }

ReturnStatus:
  /* Report the result */
  if (ERSTComm != NULL) {
    ERSTComm->RecordCount = mErrorSerialization.RecordCount;
    if (AcpiStatus == EFI_ACPI_6_4_ERST_STATUS_SUCCESS) {
      ERSTComm->Status = ErstEfiStatusToAcpiStatus (EfiStatus) << ERST_STATUS_BIT_OFFSET;
    } else {
      ERSTComm->Status = AcpiStatus << ERST_STATUS_BIT_OFFSET;
    }
  }

  ErstClearBusy ();

  DEBUG_CODE (
  {
    UINT64 EndTime;
    UINT64 ElapsedTime;
    EndTime = GetTimeInNanoSecond (GetPerformanceCounter ());
    if (EndTime > StartTime) {
      ElapsedTime = EndTime-StartTime;
    } else {
      ElapsedTime = (MAX_UINT64 - StartTime) + EndTime;
    }

    DEBUG ((DEBUG_ERROR, "%a: Function took %llu ns from start to clear busy (WriteRecordTime=%llu = %d%%, SpiTime=%llu = %d%%)\n", __FUNCTION__, ElapsedTime, WriteRecordTime, 100*WriteRecordTime/ElapsedTime, SpiTime, 100*SpiTime/ElapsedTime));
    WriteRecordTime = 0;
    SpiTime         = 0;
  }
    );

  if (NewCper != NULL) {
    ErstFreePoolRecord (NewCper);
    NewCper = NULL;
  }

  DEBUG ((DEBUG_INFO, "%a: ERST handler done, status value is 0x%x\n", __FUNCTION__, ERSTComm->Status >> ERST_STATUS_BIT_OFFSET));
  /* Always return success from the handler - status is reported via ERSTComm */
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ErstAddCperToList (
  IN EFI_COMMON_ERROR_RECORD_HEADER  *Cper,
  IN UINT32                          Offset
  )
{
  EFI_STATUS                  Status;
  ERST_CPER_INFO              CperInfo;
  CPER_ERST_PERSISTENCE_INFO  *CperPI;

  CperInfo.RecordId     = Cper->RecordID;
  CperInfo.RecordLength = Cper->RecordLength;
  CperInfo.RecordOffset = Offset;
  Status                = ErstAllocateNewRecord (&CperInfo, NULL);

  if (!EFI_ERROR (Status)) {
    CperPI = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;
    if (CperPI->Status == ERST_RECORD_STATUS_INCOMING) {
      ASSERT (mErrorSerialization.IncomingCperInfo == NULL);
      mErrorSerialization.IncomingCperInfo = &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount-1];
    } else if (CperPI->Status == ERST_RECORD_STATUS_OUTGOING) {
      ASSERT (mErrorSerialization.OutgoingCperInfo == NULL);
      mErrorSerialization.OutgoingCperInfo = &mErrorSerialization.CperInfo[mErrorSerialization.RecordCount-1];
    }
  }

  return Status;
}

EFI_STATUS
EFIAPI
ErstCollectBlock (
  IN ERST_BLOCK_INFO  *BlockInfo,
  IN UINT32           Base,
  IN UINT32           BlockNum
  )
{
  EFI_STATUS                      Status;
  EFI_COMMON_ERROR_RECORD_HEADER  *Cper;
  CPER_ERST_PERSISTENCE_INFO      *CperPI;
  UINT8                           *BlockData;
  BOOLEAN                         ReclaimBlock = FALSE;
  UINT32                          Offset       = 0;

  BlockData = NULL;
  Cper      = NULL;

  if (BlockInfo == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto ReturnStatus;
  }

  BlockInfo->ValidEntries = 0;
  BlockInfo->UsedSize     = 0;
  BlockInfo->WastedSize   = 0;
  BlockInfo->Base         = Base;

  Cper = (EFI_COMMON_ERROR_RECORD_HEADER *)ErstAllocatePoolCperHeader (sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
  if (Cper == NULL) {
    // GCOVR_EXCL_START - won't test allocation errors
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate space for reading a CPER header\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ReturnStatus;
    // GCOVR_EXCL_STOP
  }

  do {
    Status = ErstReadSpiNor (Cper, Base + Offset, sizeof (EFI_COMMON_ERROR_RECORD_HEADER));
    if (EFI_ERROR (Status)) {
      goto ReturnStatus;
    }

    CperPI = (CPER_ERST_PERSISTENCE_INFO *)&Cper->PersistenceInfo;

    // FREE space doesn't have a valid header, and only comes at the end of a block
    if (CperPI->Status == ERST_RECORD_STATUS_FREE) {
      // verify that the rest of the space actually is free
      BlockData = ErstAllocatePoolBlock (mErrorSerialization.BlockSize);
      if (BlockData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        DEBUG ((DEBUG_ERROR, "%a: Unable to allocate space for reading a block\n", __FUNCTION__));
        goto ReturnStatus;
      }

      Status = ErstReadSpiNor (BlockData, Base + Offset, mErrorSerialization.BlockSize-Offset);
      if (EFI_ERROR (Status)) {
        goto ReturnStatus;
      }

      if (!IsErasedBuffer (BlockData, mErrorSerialization.BlockSize-Offset, 0xFF)) {
        CperPI->Status = ERST_RECORD_STATUS_INVALID;
      }

      ErstFreePoolBlock (BlockData);
      BlockData = NULL;

      break; // FREE/INVALID is last entry in block
    }

    // INCOMING is an incomplete write, so other info might not be valid,
    // and only comes at the end of a block
    if (CperPI->Status == ERST_RECORD_STATUS_INCOMING) {
      Status = ErstAddCperToList (Cper, Base + Offset);
      if (!EFI_ERROR (Status)) {
        BlockInfo->ValidEntries++;
        BlockInfo->UsedSize += mErrorSerialization.BlockSize-Offset;
      }

      break; // INCOMING is last entry in block
    }

    if ((CperPI->Status == ERST_RECORD_STATUS_VALID) ||
        (CperPI->Status == ERST_RECORD_STATUS_OUTGOING) ||
        (CperPI->Status == ERST_RECORD_STATUS_DELETED))
    {
      // Attempt to validate the header if it's expected to be correct
      Status = ErstValidateCperHeader (Cper);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Found invalid CPER header, so marking the rest of the block INVALID\n", __FUNCTION__));
        CperPI->Status = ERST_RECORD_STATUS_INVALID;
        break; // INVALID is the last entry in block
      }

      // Header is valid, so process it
      if ((CperPI->Status == ERST_RECORD_STATUS_VALID) ||
          (CperPI->Status == ERST_RECORD_STATUS_OUTGOING))
      {
        Status = ErstAddCperToList (Cper, Base + Offset);
        if (EFI_ERROR (Status)) {
          goto ReturnStatus;
        }

        BlockInfo->ValidEntries++;
        BlockInfo->UsedSize += Cper->RecordLength;
      } else if (CperPI->Status == ERST_RECORD_STATUS_DELETED) {
        BlockInfo->UsedSize   += Cper->RecordLength;
        BlockInfo->WastedSize += Cper->RecordLength;
      } else {
        // This should be impossible without a code bug
        CperPI->Status = ERST_RECORD_STATUS_INVALID;
        break; // INVALID is the last entry in block
      }
    } else {
      // All other status values are INVALID
      CperPI->Status = ERST_RECORD_STATUS_INVALID;
      break; // INVALID is the last entry in block
    }

    Offset += Cper->RecordLength;
  } while (Offset < (mErrorSerialization.BlockSize - sizeof (EFI_COMMON_ERROR_RECORD_HEADER)));

  if (CperPI->Status == ERST_RECORD_STATUS_INVALID) {
    // INVALID, so other info isn't valid, and goes to the end of a block
    ReclaimBlock           = TRUE;
    BlockInfo->UsedSize   += mErrorSerialization.BlockSize-Offset;
    BlockInfo->WastedSize += mErrorSerialization.BlockSize-Offset;
  }

  if (ReclaimBlock) {
    // Mark for reclaim
    BlockInfo->ValidEntries = -BlockInfo->ValidEntries;
  }

  if ((BlockInfo->ValidEntries == 0) &&
      ((BlockInfo->UsedSize != 0) ||
       ReclaimBlock))
  {
    Status = ErstEraseBlock (BlockInfo);
    if (EFI_ERROR (Status)) {
      // GCOVR_EXCL_START - can't test failing flash access after the first one succeeds
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }
  } else if (mErrorSerialization.BlockSize - Offset < sizeof (EFI_COMMON_ERROR_RECORD_HEADER)) {
    BlockInfo->WastedSize += mErrorSerialization.BlockSize - Offset;
  }

ReturnStatus:
  if (Cper) {
    ErstFreePoolCperHeader (Cper);
    Cper = NULL;
  }

  if (BlockData) {
    ErstFreePoolBlock (BlockData);
    BlockData = NULL;
  }

  return Status;
}

EFI_STATUS
EFIAPI
ErstCopyOutgoingToIncomingCper (
  IN ERST_CPER_INFO  *OutgoingCperInfo,
  IN ERST_CPER_INFO  *IncomingCperInfo
  )
{
  EFI_STATUS                  Status;
  UINT8                       *OutgoingCper;
  UINT8                       *IncomingCper;
  CPER_ERST_PERSISTENCE_INFO  *OutgoingCperPI;
  UINT8                       *Space;
  ERST_BLOCK_INFO             *IncomingBlockInfo;
  UINT32                      ByteIndex;
  UINT32                      RemainingBlockSize;

  OutgoingCper = NULL;
  IncomingCper = NULL;
  Space        = NULL;

  // Make sure length and ID are compatible
  // Note: This only works if SPINOR erases to 1s
  if ((IncomingCperInfo->RecordLength < OutgoingCperInfo->RecordLength) ||
      ((IncomingCperInfo->RecordId & OutgoingCperInfo->RecordId) != OutgoingCperInfo->RecordId))
  {
    DEBUG ((DEBUG_WARN, "%a: RecordLength or RecordID isn't commpatible\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  OutgoingCper = ErstAllocatePoolRecord (OutgoingCperInfo->RecordLength);
  if (OutgoingCper == NULL) {
    // GCOVR_EXCL_START - won't test allocation errors
    DEBUG ((DEBUG_ERROR, "%a: Couldn't allocate space to read Outgoing CPER\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ReturnStatus;
    // GCOVR_EXCL_STOP
  }

  Status = ErstReadSpiNor (OutgoingCper, OutgoingCperInfo->RecordOffset, OutgoingCperInfo->RecordLength);
  if (EFI_ERROR (Status)) {
    goto ReturnStatus;
  }

  IncomingCperInfo->RecordId     = OutgoingCperInfo->RecordId;
  IncomingCperInfo->RecordLength = OutgoingCperInfo->RecordLength;

  IncomingCper = ErstAllocatePoolRecord (IncomingCperInfo->RecordLength);
  if (IncomingCper == NULL) {
    // GCOVR_EXCL_START - won't test allocation errors
    DEBUG ((DEBUG_ERROR, "%a: Couldn't allocate space to read Incoming CPER\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ReturnStatus;
    // GCOVR_EXCL_STOP
  }

  Status = ErstReadSpiNor (IncomingCper, IncomingCperInfo->RecordOffset, IncomingCperInfo->RecordLength);
  if (EFI_ERROR (Status)) {
    // GCOVR_EXCL_START - can't test flash errors after the first read succeeds
    goto ReturnStatus;
    // GCOVR_EXCL_STOP
  }

  // Make sure we can copy a VALID copy of the OUTGOING CPER onto the INCOMING one
  OutgoingCperPI         = (CPER_ERST_PERSISTENCE_INFO *)(&((EFI_COMMON_ERROR_RECORD_HEADER *)OutgoingCper)->PersistenceInfo);
  OutgoingCperPI->Status = ERST_RECORD_STATUS_VALID;
  for (ByteIndex = 0; ByteIndex < OutgoingCperInfo->RecordLength; ByteIndex++) {
    if ((OutgoingCper[ByteIndex] & IncomingCper[ByteIndex]) != OutgoingCper[ByteIndex]) {
      DEBUG ((DEBUG_WARN, "%a: CPER data isn't commpatible at byte 0x%x\n", __FUNCTION__, ByteIndex));
      DEBUG ((DEBUG_INFO, "%a: Outgoing 0x%x Incoming 0x%x\n", __FUNCTION__, OutgoingCper[ByteIndex], IncomingCper[ByteIndex]));
      Status = EFI_INVALID_PARAMETER;
      goto ReturnStatus;
    }
  }

  // Make sure the rest of the incoming block is FREE
  IncomingBlockInfo = ErstGetBlockOfRecord (IncomingCperInfo);
  if (IncomingBlockInfo == NULL) {
    // GCOVR_EXCL_START - Should be imposible without data corruption or code bug
    DEBUG ((DEBUG_ERROR, "%a: Couldn't locate BlockInfo for the Incoming record\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto ReturnStatus;
    // GCOVR_EXCL_STOP
  }

  RemainingBlockSize = mErrorSerialization.BlockSize - ((IncomingCperInfo->RecordOffset - IncomingBlockInfo->Base) + OutgoingCperInfo->RecordLength);
  if (RemainingBlockSize > 0) {
    Space = ErstAllocatePoolBlock (RemainingBlockSize);
    if (Space == NULL) {
      // GCOVR_EXCL_START - won't test allocation errors
      DEBUG ((DEBUG_ERROR, "%a: Couldn't allocate space to read Rest of Block\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }

    Status = ErstReadSpiNor (Space, IncomingCperInfo->RecordOffset + OutgoingCperInfo->RecordLength, RemainingBlockSize);
    if (EFI_ERROR (Status)) {
      // GCOVR_EXCL_START - can't test flash errors after the first read succeeds
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }

    DEBUG ((DEBUG_VERBOSE, "%a: Space 0x%p RemainingBlockSize 0x%x\n", __FUNCTION__, Space, RemainingBlockSize));
    for (ByteIndex = 0; ByteIndex < RemainingBlockSize; ByteIndex++) {
      if (Space[ByteIndex] != 0xFF) {
        DEBUG ((
          DEBUG_WARN,
          "%a: Space after CPER data isn't FREE (found 0x%x at 0x%p)\n",
          __FUNCTION__,
          Space[ByteIndex],
          &Space[ByteIndex]
          ));
        Status = EFI_INVALID_PARAMETER;
        goto ReturnStatus;
      }
    }
  }

  Status = ErstWriteRecord ((EFI_COMMON_ERROR_RECORD_HEADER *)OutgoingCper, OutgoingCperInfo, IncomingCperInfo, FALSE);
  if (EFI_ERROR (Status)) {
    // GCOVR_EXCL_START - can't test flash errors after the first read succeeds
    goto ReturnStatus;
    // GCOVR_EXCL_STOP
  }

  // Now that incoming has a correct size and is valid, update it's BlockInfo
  IncomingBlockInfo->UsedSize = (IncomingCperInfo->RecordOffset)%mErrorSerialization.BlockSize + IncomingCperInfo->RecordLength;

ReturnStatus:
  if (OutgoingCper != NULL) {
    ErstFreePoolRecord (OutgoingCper);
    OutgoingCper = NULL;
  }

  if (IncomingCper != NULL) {
    ErstFreePoolRecord (IncomingCper);
    IncomingCper = NULL;
  }

  if (Space != NULL) {
    ErstFreePoolBlock (Space);
    Space = NULL;
  }

  return Status;
}

EFI_STATUS
EFIAPI
ErstRelocateOutgoing (
  )
{
  EFI_STATUS       Status;
  ERST_BLOCK_INFO  *BlockInfo;

  if ((mErrorSerialization.OutgoingCperInfo == NULL) ||
      (mErrorSerialization.IncomingCperInfo != NULL))
  {
    Status = EFI_UNSUPPORTED;
    goto ReturnStatus;
  }

  // Try to relocate just the outgoing record
  Status = ErstRelocateRecord (mErrorSerialization.OutgoingCperInfo);

  // May need to relocate the whole block due to lack of resources
  if (Status == EFI_OUT_OF_RESOURCES) {
    BlockInfo = ErstGetBlockOfRecord (mErrorSerialization.OutgoingCperInfo);
    if (BlockInfo == NULL) {
      // GCOVR_EXCL_START - Should be imposible without data corruption or code bug
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }

    // Mark the OUTGOING block for reclaim, allowing using the last free block
    BlockInfo->ValidEntries = -BlockInfo->ValidEntries;

    // Must relocate the OUTGOING record first, to avoid creating a second OUTGOING
    Status = ErstRelocateRecord (mErrorSerialization.OutgoingCperInfo);
    if (EFI_ERROR (Status)) {
      goto ReturnStatus;
    }

    // Now that OUTGOING is gone, relocate the rest of the records from its block
    Status = ErstReclaimBlock (BlockInfo);
    if (EFI_ERROR (Status)) {
      goto ReturnStatus;
    }
  }

ReturnStatus:
  return Status;
}

EFI_STATUS
EFIAPI
ErstCollectBlockInfo (
  IN ERST_BLOCK_INFO  *ErstBlockInfo
  )
{
  UINT32           BlockNum;
  EFI_STATUS       Status;
  UINT8            FreeBlocks = 0;
  UINT8            CperStatus;
  ERST_CPER_INFO   *CperInfo;
  ERST_BLOCK_INFO  *BlockInfo;

  // Get ERST block info
  for (BlockNum = 0; BlockNum < mErrorSerialization.NumBlocks; BlockNum++) {
    Status = ErstCollectBlock (&ErstBlockInfo[BlockNum], BlockNum * mErrorSerialization.BlockSize, BlockNum);
    if (EFI_ERROR (Status)) {
      goto ReturnStatus;
    }
  }

  DEBUG ((DEBUG_VERBOSE, "%a: INCOMING 0x%p OUTGOING 0x%p\n", __FUNCTION__, mErrorSerialization.IncomingCperInfo, mErrorSerialization.OutgoingCperInfo));

  /*
    During Init, if an OUTGOING Status is seen and a VALID Status for the same RecordID
    is seen, the OUTGOING will be marked as DELETED.

    But if no VALID is seen and an INCOMING Status is seen for that RecordID, it is possible that the record was being
    moved, and if possible the driver will continue the move of OUTGOING to INCOMING.

    If an OUTGOING Status is seen but no corresponding INCOMING is seen, the OUTGOING
    will be moved to restore it to VALID Status.

  */
  if (mErrorSerialization.OutgoingCperInfo != NULL) {
    CperInfo = ErstFindRecord (mErrorSerialization.OutgoingCperInfo->RecordId);
    if (CperInfo != NULL) {
      DEBUG ((DEBUG_VERBOSE, "%a: Deleting OUTGOING record\n", __FUNCTION__));
      // Valid exists, so delete Outgoing
      Status = ErstClearRecord (mErrorSerialization.OutgoingCperInfo);
      if (EFI_ERROR (Status)) {
        // GCOVR_EXCL_START - Can't test clear failure after reading blocks succeeded
        goto ReturnStatus;
        // GCOVR_EXCL_STOP
      }
    } else if (mErrorSerialization.IncomingCperInfo != NULL) {
      DEBUG ((DEBUG_VERBOSE, "%a: Trying to merge OUTGOING record\n", __FUNCTION__));
      // Valid doesn't exist, but Incoming does, so try to merge Outgoing and Incoming //JDS TODO - this is wrong - INCOMING must be VALID in all but name at this point
      Status = ErstCopyOutgoingToIncomingCper (mErrorSerialization.OutgoingCperInfo, mErrorSerialization.IncomingCperInfo);
      if ((EFI_ERROR (Status)) &&
          (Status != EFI_INVALID_PARAMETER)) // Indicates inability to merge
      {
        // GCOVR_EXCL_START - Can't test read/write failure after reading blocks succeeded
        goto ReturnStatus;
        // GCOVR_EXCL_STOP
      }
    }
  }

  // If an INCOMING Status is seen but no corresponding OUTGOING is seen, it is impossible
  // to determine how much of the INCOMING CPER is missing, and it will be marked as INVALID.
  if (mErrorSerialization.IncomingCperInfo != NULL) {
    DEBUG ((DEBUG_VERBOSE, "%a: Marking INCOMING record as INVALID\n", __FUNCTION__));
    CperStatus = ERST_RECORD_STATUS_INVALID;
    CperInfo   = mErrorSerialization.IncomingCperInfo;
    Status     = ErstWriteCperStatus (&CperStatus, CperInfo);
    if (EFI_ERROR (Status)) {
      // GCOVR_EXCL_START - Can't test write failure after reading blocks succeeded
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }

    mErrorSerialization.UnsyncedSpinorChanges++; // Wrote SPINOR

    // Mark the block for reclaim
    BlockInfo = ErstGetBlockOfRecord (CperInfo);
    if (BlockInfo == NULL) {
      // GCOVR_EXCL_START - Should be imposible without data corruption or code bug
      DEBUG ((DEBUG_ERROR, "%a: Unable to find the block for the Incoming record\n", __FUNCTION__));
      Status = EFI_NOT_FOUND;
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }

    BlockInfo->ValidEntries = -BlockInfo->ValidEntries;

    Status = ErstFreeRecord (CperInfo);
    if (EFI_ERROR (Status)) {
      // GCOVR_EXCL_START - Should be imposible without data corruption or code bug
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }

    Status = ErstDeallocateRecord (CperInfo);
    if (EFI_ERROR (Status)) {
      // GCOVR_EXCL_START - Should be imposible without data corruption or code bug
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }
  }

  // Outgoing couldn't be deleted or merged, so relocate it now that there's no INCOMING
  if (mErrorSerialization.OutgoingCperInfo != NULL) {
    DEBUG ((DEBUG_VERBOSE, "%a: Relocating OUTGOING record\n", __FUNCTION__));
    Status = ErstRelocateOutgoing ();
    if (EFI_ERROR (Status)) {
      // GCOVR_EXCL_START - Can't test write failure after reading blocks succeeded
      goto ReturnStatus;
      // GCOVR_EXCL_STOP
    }
  }

  // Reclaim any remaining blocks that are marked for reclaim
  for (BlockNum = 0; BlockNum < mErrorSerialization.NumBlocks; BlockNum++) {
    if (ErstBlockInfo[BlockNum].ValidEntries < 0) {
      Status = ErstReclaimBlock (&ErstBlockInfo[BlockNum]);
      if (EFI_ERROR (Status)) {
        goto ReturnStatus;
      }
    }
  }

  for (BlockNum = 0; BlockNum < mErrorSerialization.NumBlocks; BlockNum++) {
    if (ErstBlockInfo[BlockNum].ValidEntries == 0) {
      FreeBlocks++;
    } else {
      mErrorSerialization.MostRecentBlock = BlockNum;
    }
  }

  ASSERT (FreeBlocks > 0);

ReturnStatus:
  return Status;
}

EFI_STATUS
EFIAPI
ErrorSerializationInitProtocol (
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  UINT32                     NorErstOffset,
  UINT32                     NorErstSize
  )
{
  EFI_STATUS  Status;

  mErrorSerialization.NorFlashProtocol = NorFlashProtocol;

  if (NorFlashProtocol == NULL) {
    Status = EFI_NO_MEDIA;
    goto Done;
  }

  Status = NorFlashProtocol->GetAttributes (NorFlashProtocol, &mErrorSerialization.NorAttributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Couldn't get MM-NorFlash Protocol's Attributes\n", __FUNCTION__));
    goto Done;
  }

  if ((NorErstOffset + NorErstSize) > mErrorSerialization.NorAttributes.MemoryDensity) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ERST size %u with offset %u doesn't fit in a Nor with size %u\n",
      __FUNCTION__,
      NorErstSize,
      NorErstOffset,
      mErrorSerialization.NorAttributes.MemoryDensity
      ));
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  mErrorSerialization.BlockSize = MAX (ERST_MIN_BLOCK_SIZE, mErrorSerialization.NorAttributes.BlockSize);
  if (mErrorSerialization.BlockSize%mErrorSerialization.NorAttributes.BlockSize != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ERST Block size %u isn't a multiple of NorFlash block size %u\n",
      __FUNCTION__,
      mErrorSerialization.BlockSize,
      mErrorSerialization.NorAttributes.BlockSize
      ));
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: ERST Block size %u, NorFlash block size %u\n",
    __FUNCTION__,
    mErrorSerialization.BlockSize,
    mErrorSerialization.NorAttributes.BlockSize
    ));

  mErrorSerialization.NorErstOffset = NorErstOffset;
  if (mErrorSerialization.NorErstOffset%mErrorSerialization.NorAttributes.BlockSize != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ERST Offset %u isn't a multiple of NorFlash block size %u\n",
      __FUNCTION__,
      mErrorSerialization.NorErstOffset,
      mErrorSerialization.NorAttributes.BlockSize
      ));
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  mErrorSerialization.NumBlocks = NorErstSize/mErrorSerialization.BlockSize;
  if (mErrorSerialization.NumBlocks < 2) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ERST needs at least %u bytes of space in NorFlash\n",
      __FUNCTION__,
      2*mErrorSerialization.BlockSize
      ));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Done;
  }

  mErrorSerialization.MaxRecords = mErrorSerialization.BlockSize*(mErrorSerialization.NumBlocks-1)/sizeof (ERST_CPER_INFO);

  Status = ErstPreAllocateRuntimeMemory (mErrorSerialization.BlockSize, mErrorSerialization.BufferInfo.ErrorLogInfo.Length);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to pre-allocate runtime memory: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  mErrorSerialization.PartitionSize = mErrorSerialization.NumBlocks * (UINTN)mErrorSerialization.BlockSize;

Done:
  return Status;
}

EFI_STATUS
EFIAPI
ErrorSerializationGatherSpinorData (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       BlockInfoLength;
  UINTN       CperInfoLength;

  BlockInfoLength = sizeof (ERST_BLOCK_INFO) * mErrorSerialization.NumBlocks;
  CperInfoLength  = sizeof (ERST_CPER_INFO) * mErrorSerialization.MaxRecords;

  mErrorSerialization.BlockInfo = ErstAllocatePoolBlockInfo (BlockInfoLength);
  if (mErrorSerialization.BlockInfo == NULL) {
    // GCOVR_EXCL_START - won't test allocation errors
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate space for tracking BlockInfo\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupError;
    // GCOVR_EXCL_STOP
  }

  ZeroMem (mErrorSerialization.BlockInfo, BlockInfoLength);

  mErrorSerialization.CperInfo = ErstAllocatePoolRecordInfo (CperInfoLength);
  if (mErrorSerialization.CperInfo == NULL) {
    // GCOVR_EXCL_START - won't test allocation errors
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate space for tracking CperInfo\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupError;
    // GCOVR_EXCL_STOP
  }

  ZeroMem (mErrorSerialization.CperInfo, CperInfoLength);

  // Try to create the ShadowFlash. Ignore the returned Status because we can run without it
  ErstInitShadowFlash ();

  mErrorSerialization.UnsyncedSpinorChanges = 1; // Make sure it's non-zero until after Collecting
  Status                                    = ErstCollectBlockInfo (mErrorSerialization.BlockInfo);
  if (!EFI_ERROR (Status)) {
    mErrorSerialization.UnsyncedSpinorChanges = 0;
  } else {
    goto CleanupError;
  }

  goto ReturnStatus;

CleanupError:
  if (mErrorSerialization.BlockInfo != NULL) {
    ErstFreePoolBlockInfo (mErrorSerialization.BlockInfo);
    mErrorSerialization.BlockInfo = NULL;
  }

  if (mErrorSerialization.CperInfo != NULL) {
    ErstFreePoolRecordInfo (mErrorSerialization.CperInfo);
    mErrorSerialization.CperInfo = NULL;
  }

ReturnStatus:
  return Status;
}

EFI_STATUS
EFIAPI
ErrorSerializationGatherBufferData (
  VOID
  )
{
  UINT64  NsCommBuffMemRegionBase;
  UINT64  NsCommBuffMemRegionSize;

  EFI_HOB_GUID_TYPE  *GuidHob;
  STMM_COMM_BUFFERS  *StmmCommBuffers;

  GuidHob = GetFirstGuidHob (&gNVIDIAStMMBuffersGuid);
  NV_ASSERT_RETURN (
    GuidHob != NULL,
    return EFI_NOT_FOUND,
    "%a: Unable to find HOB for gNVIDIAStMMBuffersGuid\n",
    __FUNCTION__
    );

  StmmCommBuffers         = (STMM_COMM_BUFFERS *)GET_GUID_HOB_DATA (GuidHob);
  NsCommBuffMemRegionBase = StmmCommBuffers->NsBufferAddr;
  NsCommBuffMemRegionSize = StmmCommBuffers->NsBufferSize;
  DEBUG ((DEBUG_INFO, "%a: Base = 0x%llx Size = 0x%llx\n", __FUNCTION__, NsCommBuffMemRegionBase, NsCommBuffMemRegionSize));

  mErrorSerialization.BufferInfo.ErstBase                  = StmmCommBuffers->NsErstUncachedBufAddr;
  mErrorSerialization.BufferInfo.ErstSize                  = StmmCommBuffers->NsErstUncachedBufSize;
  mErrorSerialization.BufferInfo.ErrorLogInfo.PhysicalBase = StmmCommBuffers->NsErstCachedBufAddr;
  mErrorSerialization.BufferInfo.ErrorLogInfo.Length       = StmmCommBuffers->NsErstCachedBufSize;
  mErrorSerialization.BufferInfo.ErrorLogInfo.Attributes   = 0;

  if (mErrorSerialization.BufferInfo.ErstSize < (sizeof (ERST_COMM_STRUCT))) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Erst Buffer size (0x%llx) is too small to hold ERST_COMM_STRUCT (0x%llx)\n",
      __FUNCTION__,
      mErrorSerialization.BufferInfo.ErstSize,
      sizeof (ERST_COMM_STRUCT)
      ));
    return EFI_BUFFER_TOO_SMALL;
  }

  if (mErrorSerialization.BufferInfo.ErrorLogInfo.Length < sizeof (EFI_COMMON_ERROR_RECORD_HEADER)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error Log Buffer size (0x%llx) is too small to hold even a CPER header (0x%llx)\n",
      __FUNCTION__,
      mErrorSerialization.BufferInfo.ErrorLogInfo.Length,
      sizeof (EFI_COMMON_ERROR_RECORD_HEADER)
      ));
    return EFI_BUFFER_TOO_SMALL;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ErrorSerializationSetupOsCommunication (
  VOID
  )
{
  ERST_COMM_STRUCT  *ErstComm;

  mErrorSerialization.ErstLicSwIoBase = TH500_SW_IO6_BASE;

  ErstComm            = (ERST_COMM_STRUCT *)mErrorSerialization.BufferInfo.ErstBase;
  ErstComm->Operation = ERST_OPERATION_INVALID;
  CopyMem (&ErstComm->ErrorLogAddressRange, &mErrorSerialization.BufferInfo.ErrorLogInfo, sizeof (ERST_ERROR_LOG_INFO));
  ErstComm->Status       = EFI_ACPI_6_4_ERST_STATUS_SUCCESS;
  ErstComm->Timings      = ERST_DEFAULT_TIMING;
  ErstComm->Timings    <<= ERST_MAX_TIMING_SHIFT;
  ErstComm->Timings     |= (ERST_DEFAULT_TIMING & ERST_NOMINAL_TIMING_MASK);
  ErstComm->RecordCount  = 0;
  ErstComm->RecordID     = ERST_INVALID_RECORD_ID;
  ErstComm->RecordOffset = 0;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ErrorSerializationReInit (
  VOID
  )
{
  EFI_STATUS        Status;
  ERST_COMM_STRUCT  *ErstComm;
  UINT64            RecordOffset;
  UINT64            RecordID;
  UINT64            Operation;

  DEBUG ((DEBUG_WARN, "%a: ERST running ReInit due to being out of sync with Spinor\n", __FUNCTION__));

  ErstComm = (ERST_COMM_STRUCT *)mErrorSerialization.BufferInfo.ErstBase;

  if (ErstComm != NULL) {
    // Save off the inputs from OS before resetting everything
    RecordOffset = ErstComm->RecordOffset;
    RecordID     = ErstComm->RecordID;
    Operation    = ErstComm->Operation;
  }

  // Free up old data structures
  if (mErrorSerialization.BlockInfo != NULL) {
    ErstFreePoolBlockInfo (mErrorSerialization.BlockInfo);
    mErrorSerialization.BlockInfo = NULL;
  }

  if (mErrorSerialization.CperInfo != NULL) {
    ErstFreePoolRecordInfo (mErrorSerialization.CperInfo);
    mErrorSerialization.CperInfo = NULL;
  }

  Status                         = ErrorSerializationInitialize ();
  mErrorSerialization.InitStatus = Status;

  if (ErstComm != NULL) {
    ErstComm->RecordOffset = RecordOffset;
    ErstComm->RecordID     = RecordID;
    ErstComm->Operation    = Operation;
  }

  return Status;
}

// GCOVR_EXCL_START - gMmst isn't currently stubbed
EFI_STATUS
EFIAPI
ErrorSerializationLocateStorage (
  VOID
  )
{
  EFI_STATUS                 Status;
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;
  EFI_PHYSICAL_ADDRESS       CpuBlParamsAddr;
  UINT16                     DeviceInstance;
  UINT64                     PartitionByteOffset;
  UINT64                     PartitionSize;

  NorFlashProtocol = GetSocketNorFlashProtocol (0);

  if (NorFlashProtocol == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Couldn't get MM-NorFlash Protocol for socket 0\n", __FUNCTION__));
    Status = EFI_NO_MEDIA;

    goto Done;
  }

  Status = GetCpuBlParamsAddrStMm (&CpuBlParamsAddr);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get CpuBl Addr %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  Status = GetPartitionInfoStMm (
             (UINTN)CpuBlParamsAddr,
             TEGRABL_ERST,
             &DeviceInstance,
             &PartitionByteOffset,
             &PartitionSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to get %u PartitionInfo %r\n",
      __FUNCTION__,
      TEGRABL_ERST,
      Status
      ));

    goto Done;
  }

  Status = ErrorSerializationInitProtocol (
             NorFlashProtocol,
             PartitionByteOffset,
             PartitionSize
             );

Done:
  return Status;
  // GCOVR_EXCL_STOP
}

/**
 * Register handler for ErrorSerialization
 *
 * @params[]     None.
 *
 * @retval       EFI_SUCCESS     Successfully registered the ERST MMI
 *                               handler.
 *               OTHER           When trying to register an MMI handler, return
 *                               Status code and stop the rest of the driver
 *                               from progressing.
 **/
STATIC
EFI_STATUS
RegisterErrorSerializationHandler (
  VOID
  )
{
  EFI_STATUS  Status;

  ErrorSerializationProtocol.InterruptHandler = ErrorSerializationEventHandler;

  Status = EFI_SUCCESS;
  Status = gMmst->MmInstallProtocolInterface (
                    &mErrorSerialization.Handle,
                    &gNVIDIAErrorSerializationProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &ErrorSerializationProtocol
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Register ErrorSerialization MMI handler failed (%r)\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}

EFI_STATUS
EFIAPI
ErrorSerializationInitialize (
  VOID
  )
{
  EFI_STATUS        Status;
  ERST_COMM_STRUCT  *ErstComm;

  ZeroMem (&mErrorSerialization, sizeof (mErrorSerialization));

  // Gather and init buffer info
  Status = ErrorSerializationGatherBufferData ();
  if (EFI_ERROR (Status)) {
    goto ReturnStatus;
  }

  // Fill in the OS communications structure
  Status = ErrorSerializationSetupOsCommunication ();
  if (EFI_ERROR (Status)) {
    // GCOVR_EXCL_START - always returns success currently
    goto ReturnStatus;
    // GCOVR_EXCL_STOP
  }

  // Get info required for communicating with Spinor
  Status = ErrorSerializationLocateStorage ();
  if (EFI_ERROR (Status)) {
    goto ReturnStatus;
  }

  // Read and parse the Spinor record headers
  Status = ErrorSerializationGatherSpinorData ();
  if (EFI_ERROR (Status)) {
    goto ReturnStatus;
  }

  // Update the info for OS now that we've read SPINOR
  ErstComm               = (ERST_COMM_STRUCT *)mErrorSerialization.BufferInfo.ErstBase;
  ErstComm->RecordCount  = mErrorSerialization.RecordCount;
  ErstComm->RecordID     = mErrorSerialization.RecordCount ? mErrorSerialization.CperInfo[0].RecordId : ERST_INVALID_RECORD_ID;
  ErstComm->RecordOffset = 0;

ReturnStatus:
  return Status;
}

// GCOVR_EXCL_START
EFI_STATUS
EFIAPI
ErrorSerializationMmDxeInitialize (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS        Status;
  ERST_COMM_STRUCT  *ErstComm;

  ErstMemoryInit ();

  mErrorSerialization.InitStatus = ErrorSerializationInitialize ();

  // Register the interrupt handler
  Status = RegisterErrorSerializationHandler ();
  if ( EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to register interrupt handler: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  ErstClearBusy ();

  ErstComm = (ERST_COMM_STRUCT *)mErrorSerialization.BufferInfo.ErstBase;

  DEBUG_CODE (
    DEBUG ((DEBUG_ERROR, "%a: ERST ErrorSerializationMmDxeInitialize ran and got %d (%r)\n", __FUNCTION__, mErrorSerialization.InitStatus, mErrorSerialization.InitStatus));
    DEBUG ((DEBUG_ERROR, "%a: ERST COMM is 0x%p\n", __FUNCTION__, ErstComm));
    DEBUG ((DEBUG_ERROR, "%a: ERST Base is 0x%llx\n", __FUNCTION__, ErstComm->ErrorLogAddressRange.PhysicalBase));
    );

  if (!EFI_ERROR (mErrorSerialization.InitStatus)) {
    ErstComm->Status = ERST_INIT_SUCCESS;
  }

Done:
  /* Always return success from the Init function, due to issues with Init failure in secure code*/
  return EFI_SUCCESS;
  // GCOVR_EXCL_STOP
}
