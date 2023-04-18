/** @file
  NVIDIA ERST Driver header

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EERROR_SERIALIZATION_MM_H_
#define _EERROR_SERIALIZATION_MM_H_

#include <PiMm.h>
#include <Guid/Cper.h> // From MdePkg
#include <Protocol/NorFlash.h>
#include <Include/Server/Erst.h>

#define ERST_RECORD_SIGNATURE      SIGNATURE_16( 'E', 'R' )
#define ERST_RECORD_VERSION_MAJOR  1
#define ERST_RECORD_VERSION_MINOR  1

#define ERST_MIN_BLOCK_SIZE  SIZE_16KB

#define MAX_NORFLASH_HANDLES  8

#define ERST_SIZE_ASSERT(TypeName, ExpectedSize)          \
  STATIC_ASSERT (                                         \
    sizeof (TypeName) == ExpectedSize,                    \
    "Size of " #TypeName                                  \
    " does not match " #ExpectedSize                      \
    )

extern EFI_GUID  gNVIDIAErrorSerializationProtocolGuid;

typedef struct {
  INT32     ValidEntries;
  UINT32    UsedSize;
  UINT32    WastedSize;
  UINT32    Base;
} ERST_BLOCK_INFO;

typedef struct {
  UINT64    RecordId;
  UINT32    RecordLength;
  UINT32    RecordOffset;
} ERST_CPER_INFO;

typedef struct {
  UINT16    Signature;
  union {
    UINT8    PlatformSerializationData[6];
    struct {
      UINT8    Status;
      UINT8    Major;
      UINT8    Minor;
      UINT8    Reserved[3];
    };
  };
} CPER_ERST_PERSISTENCE_INFO; // AKA "OSPM Reserved" in the ACPI spec
ERST_SIZE_ASSERT (CPER_ERST_PERSISTENCE_INFO, 8);

typedef enum {
  ERST_RECORD_STATUS_FREE     = 0xFF,
  ERST_RECORD_STATUS_INCOMING = 0xFE,
  ERST_RECORD_STATUS_VALID    = 0xF0,
  ERST_RECORD_STATUS_OUTGOING = 0xE0,
  ERST_RECORD_STATUS_DELETED  = 0x80,
  ERST_RECORD_STATUS_INVALID  = 0x00
} ERST_RECORD_STATUS;

typedef struct {
  EFI_HANDLE                   Handle;                // Handle for ERST protocol
  NVIDIA_NOR_FLASH_PROTOCOL    *NorFlashProtocol;     // Protocol for writing the SPINOR
  NOR_FLASH_ATTRIBUTES         NorAttributes;         // Attributes of the SPINOR
  UINT32                       NorErstOffset;         // Offset to the start of the ERST region in the SPINOR
  UINT32                       BlockSize;             // Virtual block size
  UINT32                       NumBlocks;             // Number of Virtual Blocks
  UINT32                       MaxRecords;            // Maximum number of records that can be stored
  UINT32                       RecordCount;           // Count of valid records on SPINOR
  UINT16                       MostRecentBlock;       // Index of most recently-written SPINOR block
  UINT16                       UnsyncedSpinorChanges; // Track how many memory changes are out of sync with SPINOR
  PHYSICAL_ADDRESS             ErstLicSwIoBase;       // Base address for the interrupt controller
  ERST_BUFFER_INFO             BufferInfo;            // Locations of buffers
  ERST_BLOCK_INFO              *BlockInfo;            // Tracking information about the SPI-NOR blocks
  ERST_CPER_INFO               *CperInfo;             // Tracking information about the Valid SPI-NOR records
  ERST_CPER_INFO               *IncomingCperInfo;     // Which CperInfo entry is INCOMING, if any
  ERST_CPER_INFO               *OutgoingCperInfo;     // Which CperInfo entry is OUTGOING, if any
  EFI_STATUS                   InitStatus;            // The status returned from the Init call
  UINTN                        PartitionSize;         // The size of the ERST flash partition
} ERST_PRIVATE_INFO;

typedef
EFI_STATUS
(EFIAPI *ERROR_SERIALIZATION_INTERRUPT_HANDLER)(
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  );

typedef struct {
  ERROR_SERIALIZATION_INTERRUPT_HANDLER    InterruptHandler;
} ERROR_SERIALIZATION_MM_PROTOCOL;

// SPINOR interaction functions - no tracking data required

// Read data from the SPINOR
EFI_STATUS
EFIAPI
ErstReadSpiNor (
  OUT VOID   *Data,
  IN UINT32  Offset,
  IN UINT32  Length
  );

// Write data to the SPINOR
EFI_STATUS
EFIAPI
ErstWriteSpiNor (
  IN VOID    *Data,
  IN UINT32  Offset,
  IN UINT32  Length
  );

// Erase the section of SPINOR
EFI_STATUS
EFIAPI
ErstEraseSpiNor (
  IN UINT32  Offset,
  IN UINT32  Length
  );

// Erase the given block (calls ErstEraseSpiNor)
EFI_STATUS
EFIAPI
ErstEraseBlock (
  IN ERST_BLOCK_INFO  *BlockInfo
  );

// Update the Status field in the Cper Header in SPINOR
EFI_STATUS
EFIAPI
ErstWriteCperStatus (
  IN UINT8           *CperStatus,
  IN ERST_CPER_INFO  *CperInfo
  );

// Data Access functions - uses and modifies tracking data and accesses SPINOR

// Read out the existing record and then write it into a different block
EFI_STATUS
EFIAPI
ErstRelocateRecord (
  ERST_CPER_INFO  *CperInfo
  );

// Relocates all the records from a block
EFI_STATUS
EFIAPI
ErstReclaimBlock (
  ERST_BLOCK_INFO  *BlockInfo
  );

// Replace the cleared record with the last record in the list,
// and mark the record as DELETED in the SPINOR
EFI_STATUS
EFIAPI
ErstClearRecord (
  IN ERST_CPER_INFO  *Record
  );

// Write the provided Record into the SPI-NOR by creating a new copy and
// deleting the old copy, if any. In certain circumstances it would work
// to overwrite the current record, but that's very data dependent and not
// implemented here.
EFI_STATUS
EFIAPI
ErstWriteRecord (
  IN EFI_COMMON_ERROR_RECORD_HEADER  *Cper,
  IN ERST_CPER_INFO                  *CurrentRecord OPTIONAL, // Optional, if replacing
  IN ERST_CPER_INFO                  *NewRecord,              // Required
  IN BOOLEAN                         DummyOp
  );

// Read the specified record into the specified buffer location
EFI_STATUS
EFIAPI
ErstReadRecord (
  IN UINT64                          RecordID,
  IN EFI_COMMON_ERROR_RECORD_HEADER  *Cper,
  IN UINT64                          MaxLength
  );

EFI_STATUS
EFIAPI
ErstCopyOutgoingToIncomingCper (
  IN ERST_CPER_INFO  *OutgoingCperInfo,
  IN ERST_CPER_INFO  *IncomingCperInfo
  );

EFI_STATUS
EFIAPI
ErstRelocateOutgoing (
  );

EFI_STATUS
EFIAPI
ErstCollectBlock (
  IN ERST_BLOCK_INFO  *BlockInfo,
  IN UINT32           Base,
  IN UINT32           BlockNum
  );

EFI_STATUS
EFIAPI
ErstCollectBlockInfo (
  IN ERST_BLOCK_INFO  *ErstBlockInfo
  );

EFI_STATUS
EFIAPI
ErrorSerializationInitProtocol (
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  UINT32                     NorErstOffset,
  UINT32                     NorErstSize
  );

EFI_STATUS
EFIAPI
ErrorSerializationGatherBufferData (
  VOID
  );

EFI_STATUS
EFIAPI
ErrorSerializationGatherSpinorData (
  VOID
  );

EFI_STATUS
EFIAPI
ErrorSerializationSetupOsCommunication (
  VOID
  );

EFI_STATUS
EFIAPI
ErrorSerializationReInit (
  VOID
  );

EFI_STATUS
EFIAPI
ErrorSerializationInitialize (
  VOID
  );

EFI_STATUS
EFIAPI
ErrorSerializationLocateStorage (
  VOID
  );

// Data tracking functions - uses only the tracking data

ERST_CPER_INFO *
ErstFindRecord (
  IN UINT64  RecordID
  );

EFI_STATUS
EFIAPI
ErstValidateCperHeader (
  IN EFI_COMMON_ERROR_RECORD_HEADER  *Cper
  );

EFI_STATUS
EFIAPI
ErstValidateRecord (
  IN EFI_COMMON_ERROR_RECORD_HEADER  *Cper,
  IN UINT64                          RecordID,
  IN UINT64                          RecordLength
  );

EFI_STATUS
EFIAPI
ErstFindFreeSpace (
  IN UINT64   RecordLength,
  OUT UINT32  *NewOffset,
  IN BOOLEAN  DummyOp
  );

ERST_BLOCK_INFO *
ErstGetBlockOfRecord (
  IN ERST_CPER_INFO  *Record
  );

EFI_STATUS
EFIAPI
ErstPrepareNewRecord (
  IN UINT64              RecordId,
  IN UINT64              RecordLength,
  IN OUT ERST_CPER_INFO  *Record,
  IN BOOLEAN             DummyOp
  );

EFI_STATUS
EFIAPI
ErstUndoAllocateRecord (
  ERST_CPER_INFO  *Record
  );

EFI_STATUS
EFIAPI
ErstDeallocateRecord (
  IN ERST_CPER_INFO  *Record
  );

EFI_STATUS
EFIAPI
ErstFreeRecord (
  ERST_CPER_INFO  *Record
  );

UINT16
ErstGetBlockIndexOfRecord (
  IN ERST_CPER_INFO  *Record
  );

EFI_STATUS
EFIAPI
ErstAllocateNewRecord (
  IN ERST_CPER_INFO   *NewRecord,
  OUT ERST_CPER_INFO  **AllocatedRecord OPTIONAL
  );

// Returns the next valid record ID after the given one, if any
UINT64
ErstGetNextRecordID (
  IN UINT64  RecordID
  );

EFI_STATUS
EFIAPI
ErstAddCperToList (
  IN EFI_COMMON_ERROR_RECORD_HEADER  *Cper,
  IN UINT32                          Offset
  );

// OS Interaction functions

// Clear the interrupt status bit that is used as a busy indicator to the OS
VOID
ErstClearBusy (
  );

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
  );

EFI_STATUS
EFIAPI
ErrorSerializationMmDxeInitialize (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *SystemTable
  );

#endif
