/** @file
  Internal capsule attempt record DXE driver definitions.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CAPSULE_ATTEMPT_RECORD_DXE_INTERNAL_H__
#define __CAPSULE_ATTEMPT_RECORD_DXE_INTERNAL_H__

#include <Uefi.h>
#include <Protocol/BlockIo.h>
#include <Protocol/FirmwareManagement.h>
#include <Protocol/CapsuleAttemptRecordProtocol.h>

#define CAPSULE_ATTEMPT_RECORD_STORAGE_NODE_PATH   "/firmware/uefi/capsule-update-record"
#define CAPSULE_ATTEMPT_RECORD_STORAGE_COMPATIBLE  "nvidia,capsule-update-record-storage-v1"
#define CAPSULE_ATTEMPT_RECORD_STORAGE_FORMAT      "nvidia,capsule-attempt-dtb-v1"

#define CAPSULE_ATTEMPT_RECORD_PAYLOAD_COMPATIBLE  "nvidia,capsule-attempt-record-v1"
#define CAPSULE_ATTEMPT_RECORD_PAYLOAD_FORMAT      1U
#define CAPSULE_ATTEMPT_RECORD_HEADER_VERSION      1U
#define CAPSULE_ATTEMPT_RECORD_HEADER_SIZE         48U
#define CAPSULE_ATTEMPT_RECORD_MAX_SLOTS           2U
#define CAPSULE_ATTEMPT_RECORD_PARTITION_NAME_MAX  36U
#define CAPSULE_ATTEMPT_RECORD_IMAGE_NAME_MAX      64U
#define CAPSULE_ATTEMPT_RECORD_PLATFORM_SPEC_MAX   256U

typedef struct {
  UINT8     Magic[8];
  UINT16    Version;
  UINT16    HeaderSize;
  UINT32    Flags;
  UINT64    Sequence;
  UINT32    PayloadOffset;
  UINT32    PayloadLength;
  UINT32    PayloadFormat;
  UINT32    PayloadCrc;
  UINT32    HeaderCrc;
  UINT32    Reserved;
} CAPSULE_ATTEMPT_RECORD_HEADER;

STATIC_ASSERT (
  sizeof (CAPSULE_ATTEMPT_RECORD_HEADER) == CAPSULE_ATTEMPT_RECORD_HEADER_SIZE,
  "Unexpected capsule attempt record header size"
  );
STATIC_ASSERT (
  OFFSET_OF (CAPSULE_ATTEMPT_RECORD_HEADER, Sequence) == 16,
  "Unexpected capsule attempt record sequence offset"
  );
STATIC_ASSERT (
  OFFSET_OF (CAPSULE_ATTEMPT_RECORD_HEADER, PayloadOffset) == 24,
  "Unexpected capsule attempt record payload offset field"
  );
STATIC_ASSERT (
  OFFSET_OF (CAPSULE_ATTEMPT_RECORD_HEADER, PayloadCrc) == 36,
  "Unexpected capsule attempt record payload CRC offset"
  );
STATIC_ASSERT (
  OFFSET_OF (CAPSULE_ATTEMPT_RECORD_HEADER, HeaderCrc) == 40,
  "Unexpected capsule attempt record header CRC offset"
  );
STATIC_ASSERT (
  OFFSET_OF (CAPSULE_ATTEMPT_RECORD_HEADER, Reserved) == 44,
  "Unexpected capsule attempt record reserved field offset"
  );

typedef struct {
  BOOLEAN    Valid;
  UINT64     Sequence;
  UINT32     PayloadOffset;
  UINT32     PayloadLength;
  UINT32     PayloadFormat;
} CAPSULE_ATTEMPT_RECORD_SLOT_INFO;

typedef struct {
  EFI_GUID    FirmwareClass;
  UINT32      FirmwareVersion;
  UINT32      LowestSupportedFirmwareVersion;
  UINT32      LastAttemptVersion;
  UINT32      LastAttemptStatus;
} CAPSULE_ATTEMPT_RECORD_ESRT_ENTRY;

typedef struct {
  CONST EFI_FIRMWARE_IMAGE_DESCRIPTOR    *ImageInfo;
  UINTN                                  ImageInfoSize;
  UINT32                                 DescriptorVersion;
  UINT8                                  DescriptorCount;
  UINTN                                  DescriptorSize;
  UINT32                                 PackageVersion;
} CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET;

typedef struct {
  CHAR8                    PartitionName[CAPSULE_ATTEMPT_RECORD_PARTITION_NAME_MAX + 1];
  UINT64                   RegionOffset;
  UINT64                   RegionSize;
  UINTN                    SlotSize;
  UINT32                   SlotCount;
  EFI_BLOCK_IO_PROTOCOL    *BlockIo;
} CAPSULE_ATTEMPT_RECORD_CONTEXT;

typedef enum {
  CapsuleAttemptRecordStateInProgress,
  CapsuleAttemptRecordStateCompleted
} CAPSULE_ATTEMPT_RECORD_STATE;

typedef struct {
  UINT64                           AttemptId;
  BOOLEAN                          AttemptIdValid;
  CAPSULE_ATTEMPT_RECORD_SOURCE    Source;
  CAPSULE_ATTEMPT_RECORD_STATE     State;
} CAPSULE_ATTEMPT_RECORD_LIFECYCLE;

typedef enum {
  CapsuleAttemptRecordProducerCheckImage,
  CapsuleAttemptRecordProducerSetImage
} CAPSULE_ATTEMPT_RECORD_PRODUCER;

typedef struct {
  CAPSULE_ATTEMPT_RECORD_PRODUCER    Producer;
  CONST CHAR8                        *FailedImage;
  EFI_STATUS                         RawStatus;
  UINT32                             NvidiaStatus;
  CONST CHAR8                        *PlatformSpec;
  CONST CHAR8                        *TnSpec;
  UINT32                             Completion;
  BOOLEAN                            CompletionValid;
} CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS;

EFI_STATUS
EFIAPI
CapsuleAttemptRecordInitialize (
  IN  CONST VOID                      *PlatformDtb,
  IN  UINTN                           PlatformDtbSize,
  OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  **Context
  );

VOID
EFIAPI
CapsuleAttemptRecordDeinitialize (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  **Context
  );

EFI_STATUS
EFIAPI
CapsuleAttemptRecordUpdateFmpImageInfo (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT                   *Context,
  IN     CONST EFI_GUID                                   *ImageTypeId,
  IN     CONST CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  *ImageInfoSet
  );

EFI_STATUS
EFIAPI
CapsuleAttemptRecordBegin (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context,
  IN     CAPSULE_ATTEMPT_RECORD_SOURCE   Source,
  OUT    UINT64                          *AttemptId
  );

EFI_STATUS
EFIAPI
CapsuleAttemptRecordFinalize (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT                   *Context,
  IN     CAPSULE_ATTEMPT_RECORD_SOURCE                    Source,
  IN     UINT64                                           AttemptId,
  IN     BOOLEAN                                          AttemptIdValid,
  IN     CONST EFI_GUID                                   *ImageTypeId,
  IN     CONST CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  *ImageInfoSet,
  IN     CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS         *Diagnostics OPTIONAL
  );

EFI_STATUS
EFIAPI
CapsuleAttemptRecordUpdateDiagnostics (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT            *Context,
  IN     CAPSULE_ATTEMPT_RECORD_SOURCE             Source,
  IN     UINT64                                    AttemptId,
  IN     CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  *Diagnostics
  );

EFI_STATUS
EFIAPI
CapsuleAttemptRecordDumpLatest (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context
  );

BOOLEAN
CapsuleAttemptRecordPartitionNameMatchesPattern (
  IN CONST CHAR8  *PartitionName,
  IN CONST CHAR8  *Pattern
  );

EFI_STATUS
CapsuleAttemptRecordParseStorageConfig (
  IN     CONST VOID                      *PlatformDtb,
  IN     UINTN                           PlatformDtbSize,
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context
  );

EFI_STATUS
CapsuleAttemptRecordValidateSlotBuffer (
  IN  CONST VOID                        *SlotBuffer,
  IN  UINTN                             SlotSize,
  OUT CAPSULE_ATTEMPT_RECORD_SLOT_INFO  *SlotInfo
  );

BOOLEAN
CapsuleAttemptRecordSequenceIsNewer (
  IN UINT64  Sequence,
  IN UINT64  Reference
  );

/**
  Builds a capsule attempt record payload.

  At least one of Lifecycle, Diagnostics, or a nonzero EntryCount is required.
  Entries is required when EntryCount is nonzero.
**/
EFI_STATUS
CapsuleAttemptRecordBuildPayload (
  OUT VOID                                      *Payload,
  IN  UINT32                                    PayloadCapacity,
  IN  CONST CAPSULE_ATTEMPT_RECORD_LIFECYCLE    *Lifecycle OPTIONAL,
  IN  CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  *Diagnostics OPTIONAL,
  IN  CONST CAPSULE_ATTEMPT_RECORD_ESRT_ENTRY   *Entries OPTIONAL,
  IN  UINTN                                     EntryCount,
  OUT UINT32                                    *PayloadLength
  );

#endif
