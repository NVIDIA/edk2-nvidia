/** @file
  Host tests for CapsuleAttemptRecordDxe internal functions.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/GoogleTestLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Guid/SystemResourceTable.h>
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include <Library/FdtLib.h>
  #include <Protocol/BlockIo.h>
  #include "../CapsuleAttemptRecordDxeInternal.h"
}

using namespace testing;

STATIC UINT8  mStorage[8192] __attribute__ ((aligned (4096)));

STATIC
EFI_STATUS
EFIAPI
TestReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
TestReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  OUT VOID                  *Buffer
  )
{
  UINT64  Offset;

  if ((MediaId != This->Media->MediaId) || (Buffer == nullptr)) {
    return EFI_INVALID_PARAMETER;
  }

  Offset = Lba * This->Media->BlockSize;
  if ((Offset > sizeof (mStorage)) || (BufferSize > sizeof (mStorage) - Offset)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  CopyMem (Buffer, mStorage + Offset, BufferSize);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
TestWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  )
{
  UINT64  Offset;

  if (This->Media->ReadOnly) {
    return EFI_WRITE_PROTECTED;
  }

  if ((MediaId != This->Media->MediaId) || (Buffer == nullptr)) {
    return EFI_INVALID_PARAMETER;
  }

  Offset = Lba * This->Media->BlockSize;
  if ((Offset > sizeof (mStorage)) || (BufferSize > sizeof (mStorage) - Offset)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  CopyMem (mStorage + Offset, Buffer, BufferSize);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
TestFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

class CapsuleAttemptRecordDxeInternalTest : public Test {
protected:
  EFI_BLOCK_IO_MEDIA Media;
  EFI_BLOCK_IO_PROTOCOL BlockIo;

  void
  SetUp (
    ) override
  {
    ZeroMem (mStorage, sizeof (mStorage));
    ZeroMem (&Media, sizeof (Media));
    ZeroMem (&BlockIo, sizeof (BlockIo));
    Media.MediaId          = 7;
    Media.MediaPresent     = TRUE;
    Media.LogicalPartition = TRUE;
    Media.BlockSize        = 512;
    Media.IoAlign          = 1;
    Media.LastBlock        = ARRAY_SIZE (mStorage) / Media.BlockSize - 1;
    BlockIo.Media          = &Media;
    BlockIo.Reset          = TestReset;
    BlockIo.ReadBlocks     = TestReadBlocks;
    BlockIo.WriteBlocks    = TestWriteBlocks;
    BlockIo.FlushBlocks    = TestFlushBlocks;
  }

  CAPSULE_ATTEMPT_RECORD_CONTEXT
  MakeContext (
    )
  {
    CAPSULE_ATTEMPT_RECORD_CONTEXT  Context;

    ZeroMem (&Context, sizeof (Context));
    Context.RegionSize = sizeof (mStorage);
    Context.SlotSize   = sizeof (mStorage) / 2;
    Context.SlotCount  = 2;
    Context.BlockIo    = &BlockIo;
    return Context;
  }
};

TEST_F (CapsuleAttemptRecordDxeInternalTest, PartitionPatternSupportsOnlyTrailingWildcard) {
  EXPECT_TRUE (CapsuleAttemptRecordPartitionNameMatchesPattern ("RP1", "RP*"));
  EXPECT_TRUE (CapsuleAttemptRecordPartitionNameMatchesPattern ("RP", "RP*"));
  EXPECT_FALSE (CapsuleAttemptRecordPartitionNameMatchesPattern ("XR1", "RP*"));
  EXPECT_TRUE (CapsuleAttemptRecordPartitionNameMatchesPattern ("OTALOG", "OTALOG"));
  EXPECT_FALSE (CapsuleAttemptRecordPartitionNameMatchesPattern ("OTALOG1", "OTALOG"));
  EXPECT_FALSE (CapsuleAttemptRecordPartitionNameMatchesPattern ("RP1", "R*1"));
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, FormatsFmpImageDigestForSha256sumComparison) {
  UINT8  Digest[CAPSULE_ATTEMPT_RECORD_SHA256_DIGEST_SIZE];
  CHAR8  DigestText[CAPSULE_ATTEMPT_RECORD_SHA256_TEXT_SIZE];
  UINTN  Index;

  for (Index = 0; Index < sizeof (Digest); Index++) {
    Digest[Index] = (UINT8)Index;
  }

  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordFormatFmpImageDigest (
      Digest,
      sizeof (Digest),
      DigestText,
      sizeof (DigestText)
      )
    );
  EXPECT_STREQ (
    "sha256:000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f",
    DigestText
    );
  EXPECT_EQ (
    EFI_INVALID_PARAMETER,
    CapsuleAttemptRecordFormatFmpImageDigest (
      Digest,
      sizeof (Digest) - 1,
      DigestText,
      sizeof (DigestText)
      )
    );
  EXPECT_EQ (
    EFI_BUFFER_TOO_SMALL,
    CapsuleAttemptRecordFormatFmpImageDigest (
      Digest,
      sizeof (Digest),
      DigestText,
      sizeof (DigestText) - 1
      )
    );
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, DumpsFmpImageDigest) {
  CAPSULE_ATTEMPT_RECORD_CONTEXT      Context;
  CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  Diagnostics;
  UINT8                               Digest[CAPSULE_ATTEMPT_RECORD_SHA256_DIGEST_SIZE];
  UINT64                              AttemptId;

  Context = MakeContext ();
  SetMem (Digest, sizeof (Digest), 0xA5);
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordBegin (
      &Context,
      CapsuleAttemptRecordSourceSavedCapsule,
      NULL,
      NULL,
      &AttemptId
      )
    );

  ZeroMem (&Diagnostics, sizeof (Diagnostics));
  Diagnostics.Producer       = CapsuleAttemptRecordProducerCheckImage;
  Diagnostics.RawStatus      = EFI_SUCCESS;
  Diagnostics.FmpImageDigest = Digest;
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordUpdateDiagnostics (
      &Context,
      CapsuleAttemptRecordSourceSavedCapsule,
      AttemptId,
      NULL,
      NULL,
      &Diagnostics
      )
    );
  EXPECT_EQ (EFI_SUCCESS, CapsuleAttemptRecordDumpLatest (&Context));
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, ParsesStorageConfiguration) {
  UINT8        Dtb[2048] __attribute__ ((aligned (8)));
  CONST CHAR8  CompatibleList[] =
    "vendor,future-capsule-update-record\0"
    CAPSULE_ATTEMPT_RECORD_STORAGE_COMPATIBLE;
  INT32                           FirmwareNode;
  INT32                           UefiNode;
  INT32                           RecordNode;
  CAPSULE_ATTEMPT_RECORD_CONTEXT  Context;

  ASSERT_EQ (0, FdtCreateEmptyTree (Dtb, sizeof (Dtb)));
  FirmwareNode = FdtAddSubnode (Dtb, 0, "firmware");
  ASSERT_GE (FirmwareNode, 0);
  UefiNode = FdtAddSubnode (Dtb, FirmwareNode, "uefi");
  ASSERT_GE (UefiNode, 0);
  RecordNode = FdtAddSubnode (Dtb, UefiNode, "capsule-update-record");
  ASSERT_GE (RecordNode, 0);
  ASSERT_EQ (
    0,
    FdtSetProp (
      Dtb,
      RecordNode,
      "compatible",
      CompatibleList,
      sizeof (CompatibleList)
      )
    );
  ASSERT_EQ (
    0,
    FdtSetPropString (
      Dtb,
      RecordNode,
      "nvidia,format",
      CAPSULE_ATTEMPT_RECORD_STORAGE_FORMAT
      )
    );
  ASSERT_EQ (0, FdtSetPropString (Dtb, RecordNode, "nvidia,partition-name", "RP1"));
  ASSERT_EQ (0, FdtSetPropU64 (Dtb, RecordNode, "nvidia,offset", 4096));
  ASSERT_EQ (0, FdtSetPropU64 (Dtb, RecordNode, "nvidia,size", 8192));
  ASSERT_EQ (0, FdtSetPropU32 (Dtb, RecordNode, "nvidia,slot-count", 2));
  ASSERT_EQ (0, FdtPack (Dtb));

  ZeroMem (&Context, sizeof (Context));
  EXPECT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordParseStorageConfig (Dtb, FdtTotalSize (Dtb), &Context)
    );
  EXPECT_STREQ ("RP1", Context.PartitionName);
  EXPECT_EQ (4096U, Context.RegionOffset);
  EXPECT_EQ (8192U, Context.RegionSize);
  EXPECT_EQ (4096U, Context.SlotSize);
  EXPECT_EQ (2U, Context.SlotCount);
  EXPECT_EQ (
    EFI_COMPROMISED_DATA,
    CapsuleAttemptRecordParseStorageConfig (
      Dtb,
      FdtTotalSize (Dtb) - 1,
      &Context
      )
    );

  ASSERT_EQ (
    0,
    FdtSetPropU64 (
      Dtb,
      RecordNode,
      "nvidia,size",
      2 * (CAPSULE_ATTEMPT_RECORD_HEADER_SIZE + sizeof (FDT_HEADER))
      )
    );
  EXPECT_EQ (
    EFI_BAD_BUFFER_SIZE,
    CapsuleAttemptRecordParseStorageConfig (Dtb, FdtTotalSize (Dtb), &Context)
    );
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, BuildsEsrtPayloadWithGuidString) {
  UINT8                              Payload[2048] __attribute__ ((aligned (8)));
  UINT32                             PayloadLength;
  CAPSULE_ATTEMPT_RECORD_ESRT_ENTRY  Entry;
  INT32                              Node;
  INT32                              Length;
  CONST CHAR8                        *FirmwareClass;

  ZeroMem (&Entry, sizeof (Entry));
  Entry.FirmwareClass.Data1    = 0xbf0d4599;
  Entry.FirmwareClass.Data2    = 0x20d4;
  Entry.FirmwareClass.Data3    = 0x414e;
  Entry.FirmwareClass.Data4[0] = 0xb2;
  Entry.FirmwareClass.Data4[1] = 0xc5;
  Entry.FirmwareClass.Data4[2] = 0x35;
  Entry.FirmwareClass.Data4[3] = 0x95;
  Entry.FirmwareClass.Data4[4] = 0xb1;
  Entry.FirmwareClass.Data4[5] = 0xcd;
  Entry.FirmwareClass.Data4[6] = 0xa4;
  Entry.FirmwareClass.Data4[7] = 0x02;
  Entry.FirmwareVersion        = 10;
  Entry.LastAttemptVersion     = 11;
  Entry.LastAttemptStatus      = 2;

  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordBuildPayload (
      Payload,
      sizeof (Payload),
      NULL,
      NULL,
      &Entry,
      1,
      &PayloadLength
      )
    );
  EXPECT_EQ (PayloadLength, FdtTotalSize (Payload));
  Node = FdtPathOffset (Payload, "/esrt-entry-0");
  ASSERT_GE (Node, 0);
  FirmwareClass = (CONST CHAR8 *)FdtGetProp (
                                   Payload,
                                   Node,
                                   "nvidia,firmware-class",
                                   &Length
                                   );
  ASSERT_NE (nullptr, FirmwareClass);
  EXPECT_EQ (37, Length);
  EXPECT_STREQ ("bf0d4599-20d4-414e-b2c5-3595b1cda402", FirmwareClass);
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, BeginAndFinalizeShareAttemptIdentity) {
  CAPSULE_ATTEMPT_RECORD_CONTEXT             Context;
  EFI_GUID                                   ImageTypeId;
  EFI_FIRMWARE_IMAGE_DESCRIPTOR              Descriptor;
  CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  ImageInfoSet;
  CAPSULE_ATTEMPT_RECORD_SLOT_INFO           SlotInfo;
  UINT64                                     AttemptId;
  CONST VOID                                 *StoredAttemptId;
  UINT64                                     StoredAttemptIdValue;
  CONST VOID                                 *StoredElapsedTime;
  UINT64                                     StoredElapsedTimeValue;
  UINT64                                     ElapsedTimeNs;
  CONST VOID                                 *StoredActiveBootChain;
  UINT32                                     StoredActiveBootChainValue;
  UINT32                                     ActiveBootChain;
  CONST CHAR8                                *State;
  EFI_TIME                                   Timestamp;
  INT32                                      Length;
  CONST VOID                                 *Payload;

  Context = MakeContext ();
  ZeroMem (&Timestamp, sizeof (Timestamp));
  Timestamp.Year  = 2026;
  Timestamp.Month = 8;
  Timestamp.Day   = 18;
  ElapsedTimeNs   = 90123456789012ULL;
  ActiveBootChain = 1;
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordBegin (
      &Context,
      CapsuleAttemptRecordSourceSavedCapsule,
      &Timestamp,
      &ActiveBootChain,
      &AttemptId
      )
    );
  EXPECT_EQ (1U, AttemptId);
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordValidateSlotBuffer (mStorage, Context.SlotSize, &SlotInfo)
    );
  Payload = mStorage + SlotInfo.PayloadOffset;
  EXPECT_LT (FdtPathOffset (Payload, "/esrt-entry-0"), 0);
  State = (CONST CHAR8 *)FdtGetProp (Payload, 0, "nvidia,attempt-state", &Length);
  ASSERT_NE (nullptr, State);
  EXPECT_STREQ ("in-progress", State);
  State = (CONST CHAR8 *)FdtGetProp (Payload, 0, "nvidia,attempt-source", &Length);
  ASSERT_NE (nullptr, State);
  EXPECT_STREQ ("saved-capsule", State);
  StoredActiveBootChain = FdtGetProp (Payload, 0, "nvidia,active-boot-chain", &Length);
  ASSERT_NE (nullptr, StoredActiveBootChain);
  ASSERT_EQ ((INT32)sizeof (StoredActiveBootChainValue), Length);
  CopyMem (&StoredActiveBootChainValue, StoredActiveBootChain, sizeof (StoredActiveBootChainValue));
  EXPECT_EQ (ActiveBootChain, Fdt32ToCpu (StoredActiveBootChainValue));

  ZeroMem (&ImageTypeId, sizeof (ImageTypeId));
  ImageTypeId.Data1 = 0x12345678;
  ZeroMem (&Descriptor, sizeof (Descriptor));
  Descriptor.ImageTypeId         = ImageTypeId;
  Descriptor.AttributesSupported = IMAGE_ATTRIBUTE_IN_USE;
  Descriptor.AttributesSetting   = IMAGE_ATTRIBUTE_IN_USE;
  ZeroMem (&ImageInfoSet, sizeof (ImageInfoSet));
  ImageInfoSet.ImageInfo         = &Descriptor;
  ImageInfoSet.ImageInfoSize     = sizeof (Descriptor);
  ImageInfoSet.DescriptorVersion = 3;
  ImageInfoSet.DescriptorCount   = 1;
  ImageInfoSet.DescriptorSize    = sizeof (Descriptor);
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordFinalize (
      &Context,
      CapsuleAttemptRecordSourceSavedCapsule,
      AttemptId,
      TRUE,
      &Timestamp,
      &ElapsedTimeNs,
      &ActiveBootChain,
      &ImageTypeId,
      &ImageInfoSet,
      NULL
      )
    );
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordValidateSlotBuffer (
      mStorage + Context.SlotSize,
      Context.SlotSize,
      &SlotInfo
      )
    );
  Payload = mStorage + Context.SlotSize + SlotInfo.PayloadOffset;
  EXPECT_GE (FdtPathOffset (Payload, "/esrt-entry-0"), 0);
  State = (CONST CHAR8 *)FdtGetProp (Payload, 0, "nvidia,attempt-state", &Length);
  ASSERT_NE (nullptr, State);
  EXPECT_STREQ ("completed", State);
  StoredActiveBootChain = FdtGetProp (Payload, 0, "nvidia,active-boot-chain", &Length);
  ASSERT_NE (nullptr, StoredActiveBootChain);
  CopyMem (&StoredActiveBootChainValue, StoredActiveBootChain, sizeof (StoredActiveBootChainValue));
  EXPECT_EQ (ActiveBootChain, Fdt32ToCpu (StoredActiveBootChainValue));
  StoredAttemptId = FdtGetProp (
                      Payload,
                      0,
                      "nvidia,attempt-id",
                      &Length
                      );
  ASSERT_NE (nullptr, StoredAttemptId);
  ASSERT_EQ ((INT32)sizeof (StoredAttemptIdValue), Length);
  CopyMem (&StoredAttemptIdValue, StoredAttemptId, sizeof (StoredAttemptIdValue));
  EXPECT_EQ (AttemptId, Fdt64ToCpu (StoredAttemptIdValue));
  StoredElapsedTime = FdtGetProp (
                        Payload,
                        0,
                        "nvidia,elapsed-time-ns",
                        &Length
                        );
  ASSERT_NE (nullptr, StoredElapsedTime);
  ASSERT_EQ ((INT32)sizeof (StoredElapsedTimeValue), Length);
  CopyMem (
    &StoredElapsedTimeValue,
    StoredElapsedTime,
    sizeof (StoredElapsedTimeValue)
    );
  EXPECT_EQ (ElapsedTimeNs, Fdt64ToCpu (StoredElapsedTimeValue));
  EXPECT_EQ (EFI_SUCCESS, CapsuleAttemptRecordDumpLatest (&Context));
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, BuildsCapsuleOnDiskSource) {
  UINT8                             Payload[4096];
  UINT32                            PayloadLength;
  CAPSULE_ATTEMPT_RECORD_LIFECYCLE  Lifecycle;
  CONST CHAR8                       *Source;
  CONST CHAR8                       *Timestamp;
  INT32                             Length;

  ZeroMem (&Lifecycle, sizeof (Lifecycle));
  Lifecycle.AttemptId            = 1;
  Lifecycle.AttemptIdValid       = TRUE;
  Lifecycle.Timestamp.Year       = 2026;
  Lifecycle.Timestamp.Month      = 8;
  Lifecycle.Timestamp.Day        = 17;
  Lifecycle.Timestamp.Hour       = 12;
  Lifecycle.Timestamp.Minute     = 34;
  Lifecycle.Timestamp.Second     = 56;
  Lifecycle.Timestamp.Nanosecond = 123456789;
  Lifecycle.Timestamp.TimeZone   = 420;
  Lifecycle.TimestampValid       = TRUE;
  Lifecycle.Source               = CapsuleAttemptRecordSourceCapsuleOnDisk;
  Lifecycle.State                = CapsuleAttemptRecordStateInProgress;

  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordBuildPayload (
      Payload,
      sizeof (Payload),
      &Lifecycle,
      NULL,
      NULL,
      0,
      &PayloadLength
      )
    );
  EXPECT_EQ (PayloadLength, FdtTotalSize (Payload));
  Source = (CONST CHAR8 *)FdtGetProp (
                            Payload,
                            0,
                            "nvidia,attempt-source",
                            &Length
                            );
  ASSERT_NE (nullptr, Source);
  EXPECT_STREQ ("capsule-on-disk", Source);
  Timestamp = (CONST CHAR8 *)FdtGetProp (
                               Payload,
                               0,
                               "nvidia,timestamp",
                               &Length
                               );
  ASSERT_NE (nullptr, Timestamp);
  EXPECT_STREQ ("2026-08-17T12:34:56.123456789-07:00", Timestamp);

  Lifecycle.Timestamp.TimeZone = -420;
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordBuildPayload (
      Payload,
      sizeof (Payload),
      &Lifecycle,
      NULL,
      NULL,
      0,
      &PayloadLength
      )
    );
  Timestamp = (CONST CHAR8 *)FdtGetProp (
                               Payload,
                               0,
                               "nvidia,timestamp",
                               &Length
                               );
  ASSERT_NE (nullptr, Timestamp);
  EXPECT_STREQ ("2026-08-17T12:34:56.123456789+07:00", Timestamp);

  Lifecycle.ElapsedTimeValid = TRUE;
  EXPECT_EQ (
    EFI_INVALID_PARAMETER,
    CapsuleAttemptRecordBuildPayload (
      Payload,
      sizeof (Payload),
      &Lifecycle,
      NULL,
      NULL,
      0,
      &PayloadLength
      )
    );
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, ValidatesDaylightAsBitmask) {
  EFI_TIME  Time;

  ZeroMem (&Time, sizeof (Time));
  Time.Year     = 2026;
  Time.Month    = 8;
  Time.Day      = 17;
  Time.TimeZone = EFI_UNSPECIFIED_TIMEZONE;

  Time.Daylight = 0;
  EXPECT_TRUE (CapsuleAttemptRecordIsTimeValid (&Time));
  Time.Daylight = EFI_TIME_ADJUST_DAYLIGHT;
  EXPECT_TRUE (CapsuleAttemptRecordIsTimeValid (&Time));
  Time.Daylight = EFI_TIME_IN_DAYLIGHT;
  EXPECT_TRUE (CapsuleAttemptRecordIsTimeValid (&Time));
  Time.Daylight = EFI_TIME_ADJUST_DAYLIGHT | EFI_TIME_IN_DAYLIGHT;
  EXPECT_TRUE (CapsuleAttemptRecordIsTimeValid (&Time));
  Time.Daylight = 0x04;
  EXPECT_FALSE (CapsuleAttemptRecordIsTimeValid (&Time));
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, RejectsUnrepresentableTimeZoneBoundaries) {
  EFI_TIME  Time;

  ZeroMem (&Time, sizeof (Time));
  Time.Year  = 2026;
  Time.Month = 8;
  Time.Day   = 17;

  Time.TimeZone = -1439;
  EXPECT_TRUE (CapsuleAttemptRecordIsTimeValid (&Time));
  Time.TimeZone = 1439;
  EXPECT_TRUE (CapsuleAttemptRecordIsTimeValid (&Time));
  Time.TimeZone = -1440;
  EXPECT_FALSE (CapsuleAttemptRecordIsTimeValid (&Time));
  Time.TimeZone = 1440;
  EXPECT_FALSE (CapsuleAttemptRecordIsTimeValid (&Time));
  Time.TimeZone = EFI_UNSPECIFIED_TIMEZONE;
  EXPECT_TRUE (CapsuleAttemptRecordIsTimeValid (&Time));
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, BuildsFmpDiagnosticsAndPlatformIdentity) {
  UINT8                               Payload[4096];
  UINT32                              PayloadLength;
  CAPSULE_ATTEMPT_RECORD_LIFECYCLE    Lifecycle;
  CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  Diagnostics;
  CONST CHAR8                         *Value;
  CONST VOID                          *StatusValue;
  CONST VOID                          *DigestValue;
  CONST VOID                          *CompletionValue;
  CONST VOID                          *BootChainValue;
  UINT64                              RawStatus;
  UINT32                              Completion;
  UINT32                              BootChain;
  UINT8                               Digest[CAPSULE_ATTEMPT_RECORD_SHA256_DIGEST_SIZE];
  INT32                               Node;
  INT32                               Length;
  UINTN                               Index;

  ZeroMem (&Lifecycle, sizeof (Lifecycle));
  Lifecycle.AttemptId            = 7;
  Lifecycle.AttemptIdValid       = TRUE;
  Lifecycle.ActiveBootChain      = 0;
  Lifecycle.ActiveBootChainValid = TRUE;
  Lifecycle.Source               = CapsuleAttemptRecordSourceAndroidFastboot;
  Lifecycle.State                = CapsuleAttemptRecordStateInProgress;
  ZeroMem (&Diagnostics, sizeof (Diagnostics));
  Diagnostics.Producer             = CapsuleAttemptRecordProducerSetImage;
  Diagnostics.FailedImage          = "A_cpu-bootloader";
  Diagnostics.RawStatus            = EFI_DEVICE_ERROR;
  Diagnostics.NvidiaStatus         = 0x100c;
  Diagnostics.PlatformSpec         = "3701-0000-0000-A.0-1-2-jetson-agx-orin-devkit-";
  Diagnostics.TnSpec               = "3701-300-0000--1--jetson-agx-orin-devkit-";
  Diagnostics.Completion           = 47;
  Diagnostics.CompletionValid      = TRUE;
  Diagnostics.TargetBootChain      = 1;
  Diagnostics.TargetBootChainValid = TRUE;
  for (Index = 0; Index < sizeof (Digest); Index++) {
    Digest[Index] = (UINT8)Index;
  }

  Diagnostics.FmpImageDigest = Digest;

  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordBuildPayload (
      Payload,
      sizeof (Payload),
      &Lifecycle,
      &Diagnostics,
      NULL,
      0,
      &PayloadLength
      )
    );
  Node = FdtPathOffset (Payload, "/diagnostics");
  ASSERT_GE (Node, 0);
  Value = (CONST CHAR8 *)FdtGetProp (Payload, Node, "nvidia,failed-image", &Length);
  ASSERT_NE (nullptr, Value);
  EXPECT_STREQ ("A_cpu-bootloader", Value);
  Value = (CONST CHAR8 *)FdtGetProp (Payload, Node, "nvidia,platform-spec", &Length);
  ASSERT_NE (nullptr, Value);
  EXPECT_STREQ (Diagnostics.PlatformSpec, Value);
  Value = (CONST CHAR8 *)FdtGetProp (Payload, Node, "nvidia,tnspec", &Length);
  ASSERT_NE (nullptr, Value);
  EXPECT_STREQ (Diagnostics.TnSpec, Value);
  StatusValue = FdtGetProp (Payload, Node, "nvidia,efi-status", &Length);
  ASSERT_NE (nullptr, StatusValue);
  ASSERT_EQ ((INT32)sizeof (RawStatus), Length);
  CopyMem (&RawStatus, StatusValue, sizeof (RawStatus));
  EXPECT_EQ ((UINT64)EFI_DEVICE_ERROR, Fdt64ToCpu (RawStatus));
  CompletionValue = FdtGetProp (Payload, Node, "nvidia,completion-percent", &Length);
  ASSERT_NE (nullptr, CompletionValue);
  ASSERT_EQ ((INT32)sizeof (Completion), Length);
  CopyMem (&Completion, CompletionValue, sizeof (Completion));
  EXPECT_EQ (Diagnostics.Completion, Fdt32ToCpu (Completion));
  BootChainValue = FdtGetProp (Payload, 0, "nvidia,active-boot-chain", &Length);
  ASSERT_NE (nullptr, BootChainValue);
  ASSERT_EQ ((INT32)sizeof (BootChain), Length);
  CopyMem (&BootChain, BootChainValue, sizeof (BootChain));
  EXPECT_EQ (Lifecycle.ActiveBootChain, Fdt32ToCpu (BootChain));
  BootChainValue = FdtGetProp (Payload, Node, "nvidia,target-boot-chain", &Length);
  ASSERT_NE (nullptr, BootChainValue);
  ASSERT_EQ ((INT32)sizeof (BootChain), Length);
  CopyMem (&BootChain, BootChainValue, sizeof (BootChain));
  EXPECT_EQ (Diagnostics.TargetBootChain, Fdt32ToCpu (BootChain));
  Value = (CONST CHAR8 *)FdtGetProp (
                           Payload,
                           0,
                           "nvidia,fmp-image-digest-algorithm",
                           &Length
                           );
  ASSERT_NE (nullptr, Value);
  EXPECT_STREQ ("sha256", Value);
  DigestValue = FdtGetProp (Payload, 0, "nvidia,fmp-image-digest", &Length);
  ASSERT_NE (nullptr, DigestValue);
  ASSERT_EQ ((INT32)sizeof (Digest), Length);
  EXPECT_EQ (0, CompareMem (Digest, DigestValue, sizeof (Digest)));
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, RejectsInvalidFmpCompletionPercentage) {
  UINT8                               Payload[4096];
  UINT32                              PayloadLength;
  CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  Diagnostics;

  ZeroMem (&Diagnostics, sizeof (Diagnostics));
  Diagnostics.Producer        = CapsuleAttemptRecordProducerSetImage;
  Diagnostics.Completion      = 101;
  Diagnostics.CompletionValid = TRUE;

  EXPECT_EQ (
    EFI_INVALID_PARAMETER,
    CapsuleAttemptRecordBuildPayload (
      Payload,
      sizeof (Payload),
      NULL,
      &Diagnostics,
      NULL,
      0,
      &PayloadLength
      )
    );
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, StandaloneFinalizeOmitsPriorAttemptDiagnostics) {
  CAPSULE_ATTEMPT_RECORD_CONTEXT             Context;
  CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS         Diagnostics;
  EFI_GUID                                   ImageTypeId;
  EFI_FIRMWARE_IMAGE_DESCRIPTOR              Descriptor;
  CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  ImageInfoSet;
  CAPSULE_ATTEMPT_RECORD_SLOT_INFO           SlotInfo;
  UINT64                                     AttemptId;
  CONST VOID                                 *Payload;
  CONST CHAR8                                *Source;
  INT32                                      Length;

  Context = MakeContext ();
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordBegin (
      &Context,
      CapsuleAttemptRecordSourceAndroidFastboot,
      NULL,
      NULL,
      &AttemptId
      )
    );

  ZeroMem (&Diagnostics, sizeof (Diagnostics));
  Diagnostics.Producer     = CapsuleAttemptRecordProducerCheckImage;
  Diagnostics.FailedImage  = "A_cpu-bootloader";
  Diagnostics.RawStatus    = EFI_DEVICE_ERROR;
  Diagnostics.NvidiaStatus = 0x100c;
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordUpdateDiagnostics (
      &Context,
      CapsuleAttemptRecordSourceAndroidFastboot,
      AttemptId,
      NULL,
      NULL,
      &Diagnostics
      )
    );

  ZeroMem (&ImageTypeId, sizeof (ImageTypeId));
  ImageTypeId.Data1 = 0x12345678;
  ZeroMem (&Descriptor, sizeof (Descriptor));
  Descriptor.ImageTypeId         = ImageTypeId;
  Descriptor.AttributesSupported = IMAGE_ATTRIBUTE_IN_USE;
  Descriptor.AttributesSetting   = IMAGE_ATTRIBUTE_IN_USE;
  ZeroMem (&ImageInfoSet, sizeof (ImageInfoSet));
  ImageInfoSet.ImageInfo         = &Descriptor;
  ImageInfoSet.ImageInfoSize     = sizeof (Descriptor);
  ImageInfoSet.DescriptorVersion = 3;
  ImageInfoSet.DescriptorCount   = 1;
  ImageInfoSet.DescriptorSize    = sizeof (Descriptor);
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordFinalize (
      &Context,
      CapsuleAttemptRecordSourceSavedCapsule,
      0,
      FALSE,
      NULL,
      NULL,
      NULL,
      &ImageTypeId,
      &ImageInfoSet,
      NULL
      )
    );

  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordValidateSlotBuffer (mStorage, Context.SlotSize, &SlotInfo)
    );
  Payload = mStorage + SlotInfo.PayloadOffset;
  EXPECT_LT (FdtPathOffset (Payload, "/diagnostics"), 0);
  Source = (CONST CHAR8 *)FdtGetProp (
                            Payload,
                            0,
                            "nvidia,attempt-source",
                            &Length
                            );
  ASSERT_NE (nullptr, Source);
  EXPECT_STREQ ("saved-capsule", Source);
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, BlockIoCommitAdvancesSlotsAndRejectsCorruption) {
  CAPSULE_ATTEMPT_RECORD_CONTEXT             Context;
  EFI_GUID                                   ImageTypeId;
  EFI_FIRMWARE_IMAGE_DESCRIPTOR              Descriptor;
  CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  ImageInfoSet;
  CAPSULE_ATTEMPT_RECORD_SLOT_INFO           SlotInfo;

  Context = MakeContext ();
  ZeroMem (&ImageTypeId, sizeof (ImageTypeId));
  ImageTypeId.Data1 = 0x12345678;
  ZeroMem (&Descriptor, sizeof (Descriptor));
  // An FMP descriptor may report zero when the image size is unknown.
  Descriptor.ImageTypeId         = ImageTypeId;
  Descriptor.Size                = 0;
  Descriptor.Version             = 5;
  Descriptor.AttributesSupported = IMAGE_ATTRIBUTE_IN_USE;
  Descriptor.AttributesSetting   = IMAGE_ATTRIBUTE_IN_USE;
  Descriptor.LastAttemptVersion  = 6;
  Descriptor.LastAttemptStatus   = LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL;
  ZeroMem (&ImageInfoSet, sizeof (ImageInfoSet));
  ImageInfoSet.ImageInfo         = &Descriptor;
  ImageInfoSet.ImageInfoSize     = sizeof (Descriptor);
  ImageInfoSet.DescriptorVersion = 3;
  ImageInfoSet.DescriptorCount   = 1;
  ImageInfoSet.DescriptorSize    = sizeof (Descriptor);

  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordUpdateFmpImageInfo (
      &Context,
      &ImageTypeId,
      &ImageInfoSet
      )
    );
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordValidateSlotBuffer (mStorage, Context.SlotSize, &SlotInfo)
    );
  EXPECT_EQ (1U, SlotInfo.Sequence);
  mStorage[16] ^= 1;
  EXPECT_EQ (
    EFI_CRC_ERROR,
    CapsuleAttemptRecordValidateSlotBuffer (mStorage, Context.SlotSize, &SlotInfo)
    );
  mStorage[16] ^= 1;

  Descriptor.Version = 4;
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordUpdateFmpImageInfo (
      &Context,
      &ImageTypeId,
      &ImageInfoSet
      )
    );
  ASSERT_EQ (
    EFI_SUCCESS,
    CapsuleAttemptRecordValidateSlotBuffer (
      mStorage + Context.SlotSize,
      Context.SlotSize,
      &SlotInfo
      )
    );
  EXPECT_EQ (2U, SlotInfo.Sequence);

  mStorage[Context.SlotSize + SlotInfo.PayloadOffset] ^= 1;
  EXPECT_EQ (
    EFI_CRC_ERROR,
    CapsuleAttemptRecordValidateSlotBuffer (
      mStorage + Context.SlotSize,
      Context.SlotSize,
      &SlotInfo
      )
    );
  EXPECT_EQ (EFI_SUCCESS, CapsuleAttemptRecordDumpLatest (&Context));
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, RejectsReadOnlyUnalignedAndNonmatchingUpdates) {
  CAPSULE_ATTEMPT_RECORD_CONTEXT             Context;
  EFI_GUID                                   ImageTypeId;
  EFI_GUID                                   OtherImageTypeId;
  EFI_FIRMWARE_IMAGE_DESCRIPTOR              Descriptor;
  CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  ImageInfoSet;

  Context = MakeContext ();
  ZeroMem (&ImageTypeId, sizeof (ImageTypeId));
  ImageTypeId.Data1 = 1;
  ZeroMem (&OtherImageTypeId, sizeof (OtherImageTypeId));
  OtherImageTypeId.Data1 = 2;
  ZeroMem (&Descriptor, sizeof (Descriptor));
  Descriptor.ImageTypeId         = OtherImageTypeId;
  Descriptor.Size                = 1024;
  Descriptor.Version             = 5;
  Descriptor.AttributesSupported = IMAGE_ATTRIBUTE_IN_USE;
  Descriptor.AttributesSetting   = IMAGE_ATTRIBUTE_IN_USE;
  ZeroMem (&ImageInfoSet, sizeof (ImageInfoSet));
  ImageInfoSet.ImageInfo         = &Descriptor;
  ImageInfoSet.ImageInfoSize     = sizeof (Descriptor);
  ImageInfoSet.DescriptorVersion = 3;
  ImageInfoSet.DescriptorCount   = 1;
  ImageInfoSet.DescriptorSize    = sizeof (Descriptor);

  EXPECT_EQ (
    EFI_NOT_FOUND,
    CapsuleAttemptRecordUpdateFmpImageInfo (
      &Context,
      &ImageTypeId,
      &ImageInfoSet
      )
    );

  Descriptor.ImageTypeId = ImageTypeId;
  Media.ReadOnly         = TRUE;
  EXPECT_EQ (
    EFI_WRITE_PROTECTED,
    CapsuleAttemptRecordUpdateFmpImageInfo (
      &Context,
      &ImageTypeId,
      &ImageInfoSet
      )
    );

  Media.ReadOnly       = FALSE;
  Context.RegionOffset = 1;
  EXPECT_EQ (
    EFI_UNSUPPORTED,
    CapsuleAttemptRecordUpdateFmpImageInfo (
      &Context,
      &ImageTypeId,
      &ImageInfoSet
      )
    );

  if (sizeof (UINTN) > sizeof (UINT32)) {
    Context.RegionOffset = 0;
    Context.SlotSize     = (UINTN)((UINT64)MAX_UINT32 +
                                   CAPSULE_ATTEMPT_RECORD_HEADER_SIZE + 1);
    Context.RegionSize = Context.SlotSize;
    Context.SlotCount  = 1;
    Media.BlockSize    = 1;
    Media.LastBlock    = Context.RegionSize - 1;
    EXPECT_EQ (
      EFI_BAD_BUFFER_SIZE,
      CapsuleAttemptRecordUpdateFmpImageInfo (
        &Context,
        &ImageTypeId,
        &ImageInfoSet
        )
      );
  }
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, SequenceComparisonHandlesWrap) {
  EXPECT_TRUE (CapsuleAttemptRecordSequenceIsNewer (2, 1));
  EXPECT_TRUE (CapsuleAttemptRecordSequenceIsNewer (0, MAX_UINT64));
  EXPECT_FALSE (CapsuleAttemptRecordSequenceIsNewer (1, 1));
  EXPECT_FALSE (CapsuleAttemptRecordSequenceIsNewer (1ULL << 63, 0));
}

TEST_F (CapsuleAttemptRecordDxeInternalTest, ElapsedTicksHandleCounterDirectionAndWrap) {
  EXPECT_EQ (10U, CapsuleAttemptRecordGetElapsedTicks (10, 20, 0, 999));
  EXPECT_EQ (10U, CapsuleAttemptRecordGetElapsedTicks (995, 5, 0, 999));
  EXPECT_EQ (10U, CapsuleAttemptRecordGetElapsedTicks (20, 10, 999, 0));
  EXPECT_EQ (10U, CapsuleAttemptRecordGetElapsedTicks (5, 995, 999, 0));
  EXPECT_EQ (
    10U,
    CapsuleAttemptRecordGetElapsedTicks (MAX_UINT64 - 4, 5, 0, MAX_UINT64)
    );
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
