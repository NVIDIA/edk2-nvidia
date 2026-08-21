/** @file
  Internal capsule attempt record DXE driver implementation.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/SystemResourceTable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/PartitionInfo.h>

#include "CapsuleAttemptRecordDxeInternal.h"

STATIC CONST UINT8  mRecordMagic[8] = {
  'N', 'V', 'C', 'A', 'R', '0', '0', '1'
};

STATIC CONST CHAR8  *CONST  mPartitionAllowList[] = {
  "OTALOG",
  "RP*",
  NULL
};
STATIC CONST CHAR8  *CONST  mPartitionDenyList[] = {
  NULL
};

#define CAPSULE_ATTEMPT_RECORD_TIMESTAMP_SIZE  36U

BOOLEAN
CapsuleAttemptRecordIsTimeValid (
  IN CONST EFI_TIME  *Time
  )
{
  STATIC CONST UINT8  DaysPerMonth[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };
  UINT8               DaysInMonth;
  BOOLEAN             LeapYear;

  //
  // UEFI permits TimeZone values of -1440 and 1440, but those values cannot
  // be represented by the timestamp's signed HH:MM offset.
  //
  if ((Time == NULL) ||
      (Time->Year < 1900) || (Time->Year > 9999) ||
      (Time->Month < 1) || (Time->Month > 12) ||
      (Time->Hour > 23) || (Time->Minute > 59) || (Time->Second > 59) ||
      (Time->Nanosecond > 999999999) ||
      ((Time->TimeZone != EFI_UNSPECIFIED_TIMEZONE) &&
       ((Time->TimeZone <= -1440) || (Time->TimeZone >= 1440))) ||
      ((Time->Daylight & ~(EFI_TIME_ADJUST_DAYLIGHT | EFI_TIME_IN_DAYLIGHT)) != 0))
  {
    return FALSE;
  }

  DaysInMonth = DaysPerMonth[Time->Month - 1];
  LeapYear    = (BOOLEAN)(((Time->Year % 4) == 0) &&
                          (((Time->Year % 100) != 0) || ((Time->Year % 400) == 0)));
  if ((Time->Month == 2) && LeapYear) {
    DaysInMonth++;
  }

  return (BOOLEAN)((Time->Day >= 1) && (Time->Day <= DaysInMonth));
}

STATIC
EFI_STATUS
FormatTimestamp (
  IN  CONST EFI_TIME  *Time,
  OUT CHAR8           *Timestamp,
  IN  UINTN           TimestampSize
  )
{
  CHAR8  TimeZone[7];
  INT16  TimeZoneMinutes;
  UINTN  AbsoluteMinutes;
  UINTN  Hours;
  UINTN  Minutes;

  if ((Time == NULL) || (Timestamp == NULL) ||
      (TimestampSize < CAPSULE_ATTEMPT_RECORD_TIMESTAMP_SIZE) ||
      !CapsuleAttemptRecordIsTimeValid (Time))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (Time->TimeZone == EFI_UNSPECIFIED_TIMEZONE) {
    TimeZone[0] = '\0';
  } else if (Time->TimeZone == 0) {
    AsciiStrCpyS (TimeZone, sizeof (TimeZone), "Z");
  } else {
    //
    // EFI_TIME.TimeZone uses LocalTime = UTC - TimeZone, while an ISO 8601
    // offset expresses LocalTime - UTC. Invert the UEFI value for the suffix.
    //
    TimeZoneMinutes = (INT16)-Time->TimeZone;
    AbsoluteMinutes = (TimeZoneMinutes < 0) ?
                      (UINTN)(-TimeZoneMinutes) : (UINTN)TimeZoneMinutes;
    Hours       = AbsoluteMinutes / 60;
    Minutes     = AbsoluteMinutes % 60;
    TimeZone[0] = (TimeZoneMinutes < 0) ? '-' : '+';
    TimeZone[1] = (CHAR8)('0' + (Hours / 10));
    TimeZone[2] = (CHAR8)('0' + (Hours % 10));
    TimeZone[3] = ':';
    TimeZone[4] = (CHAR8)('0' + (Minutes / 10));
    TimeZone[5] = (CHAR8)('0' + (Minutes % 10));
    TimeZone[6] = '\0';
  }

  AsciiSPrint (
    Timestamp,
    TimestampSize,
    "%04u-%02u-%02uT%02u:%02u:%02u.%09u%a",
    Time->Year,
    Time->Month,
    Time->Day,
    Time->Hour,
    Time->Minute,
    Time->Second,
    Time->Nanosecond,
    TimeZone
    );
  return EFI_SUCCESS;
}

BOOLEAN
CapsuleAttemptRecordPartitionNameMatchesPattern (
  IN CONST CHAR8  *PartitionName,
  IN CONST CHAR8  *Pattern
  )
{
  UINTN  PatternLength;

  if ((PartitionName == NULL) || (Pattern == NULL)) {
    return FALSE;
  }

  PatternLength = AsciiStrLen (Pattern);
  if ((PatternLength > 0) && (Pattern[PatternLength - 1] == '*')) {
    return (BOOLEAN)(AsciiStrnCmp (PartitionName, Pattern, PatternLength - 1) == 0);
  }

  return (BOOLEAN)(AsciiStrCmp (PartitionName, Pattern) == 0);
}

STATIC
BOOLEAN
PartitionNameIsPermitted (
  IN CONST CHAR8  *PartitionName
  )
{
  UINTN  Index;

  for (Index = 0; mPartitionDenyList[Index] != NULL; Index++) {
    if (CapsuleAttemptRecordPartitionNameMatchesPattern (
          PartitionName,
          mPartitionDenyList[Index]
          ))
    {
      return FALSE;
    }
  }

  if (mPartitionAllowList[0] == NULL) {
    return TRUE;
  }

  for (Index = 0; mPartitionAllowList[Index] != NULL; Index++) {
    if (CapsuleAttemptRecordPartitionNameMatchesPattern (
          PartitionName,
          mPartitionAllowList[Index]
          ))
    {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
EFI_STATUS
GetRequiredStringProperty (
  IN  CONST VOID   *Dtb,
  IN  INT32        Node,
  IN  CONST CHAR8  *PropertyName,
  OUT CONST CHAR8  **String,
  OUT UINTN        *StringLength
  )
{
  INT32        Length;
  CONST CHAR8  *Value;

  Value = FdtGetProp (Dtb, Node, PropertyName, &Length);
  if ((Value == NULL) || (Length <= 1) || (Value[Length - 1] != '\0')) {
    return EFI_NOT_FOUND;
  }

  if (AsciiStrnLenS (Value, (UINTN)Length) != (UINTN)(Length - 1)) {
    return EFI_COMPROMISED_DATA;
  }

  *String       = Value;
  *StringLength = (UINTN)Length - 1;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetRequiredUnsignedProperty (
  IN  CONST VOID   *Dtb,
  IN  INT32        Node,
  IN  CONST CHAR8  *PropertyName,
  OUT UINT64       *Value
  )
{
  INT32       Length;
  CONST VOID  *Property;
  UINT32      Value32;
  UINT64      Value64;

  Property = FdtGetProp (Dtb, Node, PropertyName, &Length);
  if ((Property == NULL) || ((Length != sizeof (UINT32)) && (Length != sizeof (UINT64)))) {
    return EFI_NOT_FOUND;
  }

  if (Length == sizeof (UINT32)) {
    CopyMem (&Value32, Property, sizeof (Value32));
    *Value = Fdt32ToCpu (Value32);
  } else {
    CopyMem (&Value64, Property, sizeof (Value64));
    *Value = Fdt64ToCpu (Value64);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
CapsuleAttemptRecordParseStorageConfig (
  IN     CONST VOID                      *PlatformDtb,
  IN     UINTN                           PlatformDtbSize,
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context
  )
{
  EFI_STATUS   Status;
  INT32        Node;
  INT32        Compatibility;
  CONST CHAR8  *String;
  UINTN        StringLength;
  UINT64       Value;

  if ((PlatformDtb == NULL) || (Context == NULL) ||
      (PlatformDtbSize < sizeof (FDT_HEADER)))
  {
    return EFI_INVALID_PARAMETER;
  }

  if ((FdtCheckHeader (PlatformDtb) != 0) ||
      (FdtTotalSize (PlatformDtb) > PlatformDtbSize))
  {
    DEBUG ((DEBUG_ERROR, "%a: DT header invalid\n", __FUNCTION__));
    return EFI_COMPROMISED_DATA;
  }

  Node = FdtPathOffset (PlatformDtb, CAPSULE_ATTEMPT_RECORD_STORAGE_NODE_PATH);
  if (Node < 0) {
    DEBUG ((DEBUG_ERROR, "%a: DT node not found: %a\n", __FUNCTION__, CAPSULE_ATTEMPT_RECORD_STORAGE_NODE_PATH));
    return EFI_NOT_FOUND;
  }

  Compatibility = FdtNodeCheckCompatible (
                    PlatformDtb,
                    Node,
                    CAPSULE_ATTEMPT_RECORD_STORAGE_COMPATIBLE
                    );
  if (Compatibility < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: compatible property invalid: %a (%d)\n",
      __FUNCTION__,
      FdtStrerror (Compatibility),
      Compatibility
      ));
    return EFI_COMPROMISED_DATA;
  }

  if (Compatibility > 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: compatible unsupported: %a\n",
      __FUNCTION__,
      CAPSULE_ATTEMPT_RECORD_STORAGE_COMPATIBLE
      ));
    return EFI_UNSUPPORTED;
  }

  Status = GetRequiredStringProperty (
             PlatformDtb,
             Node,
             "nvidia,format",
             &String,
             &StringLength
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: format missing or invalid: %r\n", __FUNCTION__, Status));
    return EFI_UNSUPPORTED;
  }

  if (AsciiStrCmp (String, CAPSULE_ATTEMPT_RECORD_STORAGE_FORMAT) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: format unsupported: %a\n", __FUNCTION__, String));
    return EFI_UNSUPPORTED;
  }

  StringLength = 0;
  Status       = GetRequiredStringProperty (
                   PlatformDtb,
                   Node,
                   "nvidia,partition-name",
                   &String,
                   &StringLength
                   );
  if (EFI_ERROR (Status) || (StringLength > CAPSULE_ATTEMPT_RECORD_PARTITION_NAME_MAX)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: partition name missing or too long: %r length=%u\n",
      __FUNCTION__,
      Status,
      StringLength
      ));
    return EFI_UNSUPPORTED;
  }

  if (!PartitionNameIsPermitted (String)) {
    DEBUG ((DEBUG_ERROR, "%a: partition denied by policy: %a\n", __FUNCTION__, String));
    return EFI_ACCESS_DENIED;
  }

  ZeroMem (Context, sizeof (*Context));
  CopyMem (Context->PartitionName, String, StringLength);

  Context->RegionOffset = 0;
  Status                = GetRequiredUnsignedProperty (
                            PlatformDtb,
                            Node,
                            "nvidia,offset",
                            &Context->RegionOffset
                            );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: offset missing or invalid: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Context->RegionSize = 0;
  Status              = GetRequiredUnsignedProperty (
                          PlatformDtb,
                          Node,
                          "nvidia,size",
                          &Context->RegionSize
                          );
  if (EFI_ERROR (Status) || (Context->RegionSize == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: size missing or invalid: %r size=%Lu\n", __FUNCTION__, Status, Context->RegionSize));
    return EFI_UNSUPPORTED;
  }

  Value  = 0;
  Status = GetRequiredUnsignedProperty (
             PlatformDtb,
             Node,
             "nvidia,slot-count",
             &Value
             );
  if (EFI_ERROR (Status) || (Value == 0) || (Value > CAPSULE_ATTEMPT_RECORD_MAX_SLOTS)) {
    DEBUG ((DEBUG_ERROR, "%a: slot count missing or invalid: %r count=%Lu\n", __FUNCTION__, Status, Value));
    return EFI_UNSUPPORTED;
  }

  Context->SlotCount = (UINT32)Value;
  if ((Context->RegionSize % Context->SlotCount) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: region size is not divisible by slot count: size=%Lu count=%u\n",
      __FUNCTION__,
      Context->RegionSize,
      Context->SlotCount
      ));
    return EFI_UNSUPPORTED;
  }

  Value = Context->RegionSize / Context->SlotCount;
  if ((Value > MAX_UINTN) ||
      (Value <= CAPSULE_ATTEMPT_RECORD_HEADER_SIZE + sizeof (FDT_HEADER)))
  {
    DEBUG ((DEBUG_ERROR, "%a: computed slot size is unusable: %Lu\n", __FUNCTION__, Value));
    return EFI_BAD_BUFFER_SIZE;
  }

  Context->SlotSize = (UINTN)Value;
  DEBUG ((
    DEBUG_INFO,
    "%a: storage DT accepted: partition=%a offset=%Lu size=%Lu slots=%u\n",
    __FUNCTION__,
    Context->PartitionName,
    Context->RegionOffset,
    Context->RegionSize,
    Context->SlotCount
    ));
  return EFI_SUCCESS;
}

STATIC
BOOLEAN
PartitionNameMatchesGptName (
  IN CONST CHAR8   *PartitionName,
  IN CONST CHAR16  *GptName
  )
{
  UINTN  Index;

  for (Index = 0; Index < CAPSULE_ATTEMPT_RECORD_PARTITION_NAME_MAX; Index++) {
    if (PartitionName[Index] == '\0') {
      return (BOOLEAN)(GptName[Index] == L'\0');
    }

    if ((GptName[Index] > MAX_UINT8) ||
        ((CHAR8)GptName[Index] != PartitionName[Index]))
    {
      return FALSE;
    }
  }

  return (BOOLEAN)(PartitionName[Index] == '\0');
}

STATIC
EFI_STATUS
ValidateBlockIoRegion (
  IN CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context
  )
{
  EFI_BLOCK_IO_MEDIA  *Media;
  UINT64              PartitionSize;

  Media = Context->BlockIo->Media;
  if ((Media == NULL) || !Media->MediaPresent || !Media->LogicalPartition ||
      (Media->BlockSize == 0))
  {
    DEBUG ((DEBUG_ERROR, "%a: BlockIo media is unavailable or invalid\n", __FUNCTION__));
    return EFI_NO_MEDIA;
  }

  if (((Context->RegionOffset % Media->BlockSize) != 0) ||
      ((Context->SlotSize % Media->BlockSize) != 0))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: region is not block aligned: offset=%Lu slot-size=%u block-size=%u\n",
      __FUNCTION__,
      Context->RegionOffset,
      Context->SlotSize,
      Media->BlockSize
      ));
    return EFI_UNSUPPORTED;
  }

  if (Media->LastBlock == MAX_UINT64) {
    DEBUG ((DEBUG_ERROR, "%a: partition LastBlock is invalid\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  if ((Media->LastBlock + 1) > DivU64x32 (MAX_UINT64, Media->BlockSize)) {
    DEBUG ((DEBUG_ERROR, "%a: partition size calculation overflows\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  PartitionSize = MultU64x32 (Media->LastBlock + 1, Media->BlockSize);
  if ((Context->RegionOffset > PartitionSize) ||
      (Context->RegionSize > PartitionSize - Context->RegionOffset))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: region exceeds partition: offset=%Lu size=%Lu partition-size=%Lu\n",
      __FUNCTION__,
      Context->RegionOffset,
      Context->RegionSize,
      PartitionSize
      ));
    return EFI_BAD_BUFFER_SIZE;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
LocatePartition (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context
  )
{
  EFI_STATUS                   Status;
  EFI_HANDLE                   *Handles;
  UINTN                        HandleCount;
  UINTN                        Index;
  UINTN                        MatchCount;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  EFI_BLOCK_IO_PROTOCOL        *BlockIo;

  if (gBS == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: partition lookup unavailable before boot services\n", __FUNCTION__));
    return EFI_NOT_READY;
  }

  Handles     = NULL;
  HandleCount = 0;
  Status      = gBS->LocateHandleBuffer (
                       ByProtocol,
                       &gEfiPartitionInfoProtocolGuid,
                       NULL,
                       &HandleCount,
                       &Handles
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: partition handle lookup failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  MatchCount = 0;
  BlockIo    = NULL;
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiPartitionInfoProtocolGuid,
                    (VOID **)&PartitionInfo
                    );
    if (EFI_ERROR (Status) || (PartitionInfo->Type != PARTITION_TYPE_GPT) ||
        !PartitionNameMatchesGptName (
           Context->PartitionName,
           PartitionInfo->Info.Gpt.PartitionName
           ))
    {
      continue;
    }

    DEBUG ((DEBUG_INFO, "%a: GPT name matched on handle %u\n", __FUNCTION__, Index));

    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiBlockIoProtocolGuid,
                    (VOID **)&BlockIo
                    );
    if (EFI_ERROR (Status) || (BlockIo == NULL)) {
      DEBUG ((DEBUG_ERROR, "%a: partition lacks BlockIo: handle=%u status=%r\n", __FUNCTION__, Index, Status));
      continue;
    }

    Context->BlockIo = BlockIo;
    Status           = ValidateBlockIoRegion (Context);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: partition region rejected: handle=%u status=%r\n", __FUNCTION__, Index, Status));
      Context->BlockIo = NULL;
      continue;
    }

    MatchCount++;
  }

  FreePool (Handles);
  if (MatchCount == 0) {
    Context->BlockIo = NULL;
    DEBUG ((DEBUG_ERROR, "%a: no usable partition named %a found\n", __FUNCTION__, Context->PartitionName));
    return EFI_NOT_FOUND;
  }

  if (MatchCount > 1) {
    Context->BlockIo = NULL;
    DEBUG ((DEBUG_ERROR, "%a: partition %a matched %u usable partitions\n", __FUNCTION__, Context->PartitionName, MatchCount));
    return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_INFO, "%a: partition %a located successfully\n", __FUNCTION__, Context->PartitionName));
  return EFI_SUCCESS;
}

STATIC
VOID *
AllocateBlockIoBuffer (
  IN CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context
  )
{
  UINTN  Alignment;

  Alignment = Context->BlockIo->Media->IoAlign;
  if (Alignment < EFI_PAGE_SIZE) {
    Alignment = EFI_PAGE_SIZE;
  }

  return AllocateAlignedPages (EFI_SIZE_TO_PAGES (Context->SlotSize), Alignment);
}

STATIC
VOID
FreeBlockIoBuffer (
  IN CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context,
  IN VOID                            *Buffer
  )
{
  if (Buffer != NULL) {
    FreeAlignedPages (Buffer, EFI_SIZE_TO_PAGES (Context->SlotSize));
  }
}

STATIC
EFI_STATUS
ReadSlot (
  IN  CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context,
  IN  UINT32                          SlotIndex,
  OUT VOID                            *Buffer
  )
{
  EFI_LBA     Lba;
  EFI_STATUS  Status;

  if (SlotIndex >= Context->SlotCount) {
    DEBUG ((DEBUG_ERROR, "%a: slot read index out of range: %u\n", __FUNCTION__, SlotIndex));
    return EFI_INVALID_PARAMETER;
  }

  Lba = DivU64x32 (
          Context->RegionOffset + MultU64x64 (SlotIndex, Context->SlotSize),
          Context->BlockIo->Media->BlockSize
          );
  {
    Status = Context->BlockIo->ReadBlocks (
                                 Context->BlockIo,
                                 Context->BlockIo->Media->MediaId,
                                 Lba,
                                 Context->SlotSize,
                                 Buffer
                                 );
    return Status;
  }
}

BOOLEAN
CapsuleAttemptRecordSequenceIsNewer (
  IN UINT64  Sequence,
  IN UINT64  Reference
  )
{
  UINT64  Difference;

  Difference = Sequence - Reference;
  return (BOOLEAN)((Difference != 0) && (Difference < MAX_INT64 + 1ULL));
}

UINT64
CapsuleAttemptRecordGetElapsedTicks (
  IN UINT64  StartCounter,
  IN UINT64  EndCounter,
  IN UINT64  CounterStartValue,
  IN UINT64  CounterEndValue
  )
{
  if (CounterStartValue < CounterEndValue) {
    if (EndCounter >= StartCounter) {
      return EndCounter - StartCounter;
    }

    return (CounterEndValue - StartCounter) +
           (EndCounter - CounterStartValue) + 1;
  }

  if (EndCounter <= StartCounter) {
    return StartCounter - EndCounter;
  }

  return (StartCounter - CounterEndValue) +
         (CounterStartValue - EndCounter) + 1;
}

EFI_STATUS
CapsuleAttemptRecordValidateSlotBuffer (
  IN  CONST VOID                        *SlotBuffer,
  IN  UINTN                             SlotSize,
  OUT CAPSULE_ATTEMPT_RECORD_SLOT_INFO  *SlotInfo
  )
{
  CAPSULE_ATTEMPT_RECORD_HEADER  Header;
  UINT32                         HeaderCrc;
  UINT32                         ExpectedHeaderCrc;

  if ((SlotBuffer == NULL) || (SlotInfo == NULL) ||
      (SlotSize < CAPSULE_ATTEMPT_RECORD_HEADER_SIZE))
  {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (SlotInfo, sizeof (*SlotInfo));
  CopyMem (&Header, SlotBuffer, sizeof (Header));
  if ((CompareMem (Header.Magic, mRecordMagic, sizeof (mRecordMagic)) != 0) ||
      (Header.Version != CAPSULE_ATTEMPT_RECORD_HEADER_VERSION) ||
      (Header.HeaderSize != CAPSULE_ATTEMPT_RECORD_HEADER_SIZE) ||
      (Header.Flags != 0) ||
      (Header.Reserved != 0))
  {
    DEBUG ((
      DEBUG_INFO,
      "%a: slot header invalid: version=%u size=%u flags=%u reserved=%u\n",
      __FUNCTION__,
      Header.Version,
      Header.HeaderSize,
      Header.Flags,
      Header.Reserved
      ));
    return EFI_COMPROMISED_DATA;
  }

  HeaderCrc         = Header.HeaderCrc;
  Header.HeaderCrc  = 0;
  ExpectedHeaderCrc = CalculateCrc32 (&Header, sizeof (Header));
  if (HeaderCrc != ExpectedHeaderCrc) {
    DEBUG ((DEBUG_INFO, "%a: slot header CRC mismatch: stored=0x%x expected=0x%x\n", __FUNCTION__, HeaderCrc, ExpectedHeaderCrc));
    return EFI_CRC_ERROR;
  }

  SlotInfo->Sequence      = Header.Sequence;
  SlotInfo->PayloadOffset = Header.PayloadOffset;
  SlotInfo->PayloadLength = Header.PayloadLength;
  SlotInfo->PayloadFormat = Header.PayloadFormat;
  if ((SlotInfo->PayloadOffset != CAPSULE_ATTEMPT_RECORD_HEADER_SIZE) ||
      (SlotInfo->PayloadLength == 0) ||
      (SlotInfo->PayloadOffset > SlotSize) ||
      (SlotInfo->PayloadLength > SlotSize - SlotInfo->PayloadOffset))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: slot payload bounds invalid: offset=%u length=%u slot-size=%u\n",
      __FUNCTION__,
      SlotInfo->PayloadOffset,
      SlotInfo->PayloadLength,
      SlotSize
      ));
    return EFI_COMPROMISED_DATA;
  }

  if (Header.PayloadCrc != CalculateCrc32 (
                             (UINT8 *)SlotBuffer + SlotInfo->PayloadOffset,
                             SlotInfo->PayloadLength
                             ))
  {
    DEBUG ((DEBUG_ERROR, "%a: slot payload CRC mismatch\n", __FUNCTION__));
    return EFI_CRC_ERROR;
  }

  if ((SlotInfo->PayloadFormat == CAPSULE_ATTEMPT_RECORD_PAYLOAD_FORMAT) &&
      ((SlotInfo->PayloadLength < sizeof (FDT_HEADER)) ||
       (FdtCheckHeader ((UINT8 *)SlotBuffer + SlotInfo->PayloadOffset) != 0) ||
       (FdtTotalSize ((UINT8 *)SlotBuffer + SlotInfo->PayloadOffset) !=
        SlotInfo->PayloadLength)))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: slot DTB payload invalid: format=%u length=%u\n",
      __FUNCTION__,
      SlotInfo->PayloadFormat,
      SlotInfo->PayloadLength
      ));
    return EFI_COMPROMISED_DATA;
  }

  SlotInfo->Valid = TRUE;
  DEBUG ((
    DEBUG_INFO,
    "%a: slot valid: sequence=%Lu format=%u length=%u\n",
    __FUNCTION__,
    SlotInfo->Sequence,
    SlotInfo->PayloadFormat,
    SlotInfo->PayloadLength
    ));
  return EFI_SUCCESS;
}

STATIC
VOID
GuidToCanonicalString (
  IN  CONST EFI_GUID  *Guid,
  OUT CHAR8           *String
  )
{
  UINTN  Index;

  AsciiSPrint (
    String,
    37,
    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    Guid->Data1,
    Guid->Data2,
    Guid->Data3,
    Guid->Data4[0],
    Guid->Data4[1],
    Guid->Data4[2],
    Guid->Data4[3],
    Guid->Data4[4],
    Guid->Data4[5],
    Guid->Data4[6],
    Guid->Data4[7]
    );

  for (Index = 0; String[Index] != '\0'; Index++) {
    if ((String[Index] >= 'A') && (String[Index] <= 'F')) {
      String[Index] = (CHAR8)(String[Index] - 'A' + 'a');
    }
  }
}

EFI_STATUS
CapsuleAttemptRecordBuildPayload (
  OUT VOID                                      *Payload,
  IN  UINT32                                    PayloadCapacity,
  IN  CONST CAPSULE_ATTEMPT_RECORD_LIFECYCLE    *Lifecycle OPTIONAL,
  IN  CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  *Diagnostics OPTIONAL,
  IN  CONST CAPSULE_ATTEMPT_RECORD_ESRT_ENTRY   *Entries OPTIONAL,
  IN  UINTN                                     EntryCount,
  OUT UINT32                                    *PayloadLength
  )
{
  INT32  FdtStatus;
  INT32  Node;
  UINTN  Index;
  CHAR8  NodeName[32];
  CHAR8  GuidString[37];
  CHAR8  Timestamp[CAPSULE_ATTEMPT_RECORD_TIMESTAMP_SIZE];

  if ((Payload == NULL) || (PayloadLength == NULL) ||
      ((EntryCount != 0) && (Entries == NULL)) ||
      ((Lifecycle == NULL) && (Diagnostics == NULL) && (EntryCount == 0)))
  {
    return EFI_INVALID_PARAMETER;
  }

  FdtStatus = FdtCreateEmptyTree (Payload, PayloadCapacity);
  if (FdtStatus != 0) {
    return EFI_BUFFER_TOO_SMALL;
  }

  if ((FdtSetPropString (
         Payload,
         0,
         "compatible",
         CAPSULE_ATTEMPT_RECORD_PAYLOAD_COMPATIBLE
         ) != 0) ||
      (FdtSetPropU32 (
         Payload,
         0,
         "nvidia,format-version",
         CAPSULE_ATTEMPT_RECORD_PAYLOAD_FORMAT
         ) != 0))
  {
    return EFI_BUFFER_TOO_SMALL;
  }

  if (Lifecycle != NULL) {
    CONST CHAR8  *Source;
    CONST CHAR8  *State;

    switch (Lifecycle->Source) {
      case CapsuleAttemptRecordSourceAndroidFastboot:
        Source = "android-fastboot";
        break;
      case CapsuleAttemptRecordSourceCapsuleOnDisk:
        Source = "capsule-on-disk";
        break;
      case CapsuleAttemptRecordSourceSavedCapsule:
        Source = "saved-capsule";
        break;
      case CapsuleAttemptRecordSourceUnknown:
        Source = "unknown";
        break;
      default:
        return EFI_INVALID_PARAMETER;
    }

    State = (Lifecycle->State == CapsuleAttemptRecordStateInProgress) ?
            "in-progress" : "completed";
    if ((Lifecycle->State != CapsuleAttemptRecordStateInProgress) &&
        (Lifecycle->State != CapsuleAttemptRecordStateCompleted))
    {
      return EFI_INVALID_PARAMETER;
    }

    if ((FdtSetPropU64 (
           Payload,
           0,
           "nvidia,attempt-id",
           Lifecycle->AttemptId
           ) != 0) ||
        (FdtSetPropString (
           Payload,
           0,
           "nvidia,attempt-source",
           Source
           ) != 0) ||
        (FdtSetPropString (
           Payload,
           0,
           "nvidia,attempt-state",
           State
           ) != 0))
    {
      return EFI_BUFFER_TOO_SMALL;
    }

    if (Lifecycle->ActiveBootChainValid &&
        (FdtSetPropU32 (
           Payload,
           0,
           "nvidia,active-boot-chain",
           Lifecycle->ActiveBootChain
           ) != 0))
    {
      return EFI_BUFFER_TOO_SMALL;
    }

    if (Lifecycle->TimestampValid) {
      if (EFI_ERROR (
            FormatTimestamp (
              &Lifecycle->Timestamp,
              Timestamp,
              sizeof (Timestamp)
              )
            ))
      {
        return EFI_INVALID_PARAMETER;
      }

      if (FdtSetPropString (
            Payload,
            0,
            "nvidia,timestamp",
            Timestamp
            ) != 0)
      {
        return EFI_BUFFER_TOO_SMALL;
      }
    }

    if (Lifecycle->ElapsedTimeValid) {
      if (Lifecycle->State != CapsuleAttemptRecordStateCompleted) {
        return EFI_INVALID_PARAMETER;
      }

      if (FdtSetPropU64 (
            Payload,
            0,
            "nvidia,elapsed-time-ns",
            Lifecycle->ElapsedTimeNs
            ) != 0)
      {
        return EFI_BUFFER_TOO_SMALL;
      }
    }
  }

  if (Diagnostics != NULL) {
    CONST CHAR8  *Producer;

    if (Diagnostics->CompletionValid && (Diagnostics->Completion > 100)) {
      return EFI_INVALID_PARAMETER;
    }

    if (Diagnostics->Producer == CapsuleAttemptRecordProducerCheckImage) {
      Producer = "fmp-tegra-check-image";
    } else if (Diagnostics->Producer == CapsuleAttemptRecordProducerSetImage) {
      Producer = "fmp-tegra-set-image";
    } else {
      return EFI_INVALID_PARAMETER;
    }

    if ((Diagnostics->FmpImageDigest != NULL) &&
        ((FdtSetPropString (
            Payload,
            0,
            "nvidia,fmp-image-digest-algorithm",
            "sha256"
            ) != 0) ||
         (FdtSetProp (
            Payload,
            0,
            "nvidia,fmp-image-digest",
            Diagnostics->FmpImageDigest,
            CAPSULE_ATTEMPT_RECORD_SHA256_DIGEST_SIZE
            ) != 0)))
    {
      return EFI_BUFFER_TOO_SMALL;
    }

    Node = FdtAddSubnode (Payload, 0, "diagnostics");
    if ((Node < 0) ||
        (FdtSetPropString (Payload, Node, "nvidia,producer", Producer) != 0) ||
        (FdtSetPropU64 (Payload, Node, "nvidia,efi-status", (UINT64)Diagnostics->RawStatus) != 0) ||
        (FdtSetPropU32 (Payload, Node, "nvidia,nvidia-status", Diagnostics->NvidiaStatus) != 0))
    {
      return EFI_BUFFER_TOO_SMALL;
    }

    if (((Diagnostics->FailedImage != NULL) &&
         (FdtSetPropString (Payload, Node, "nvidia,failed-image", Diagnostics->FailedImage) != 0)) ||
        ((Diagnostics->PlatformSpec != NULL) &&
         (FdtSetPropString (Payload, Node, "nvidia,platform-spec", Diagnostics->PlatformSpec) != 0)) ||
        ((Diagnostics->TnSpec != NULL) &&
         (FdtSetPropString (Payload, Node, "nvidia,tnspec", Diagnostics->TnSpec) != 0)) ||
        (Diagnostics->CompletionValid &&
         (FdtSetPropU32 (Payload, Node, "nvidia,completion-percent", Diagnostics->Completion) != 0)) ||
        (Diagnostics->TargetBootChainValid &&
         (FdtSetPropU32 (Payload, Node, "nvidia,target-boot-chain", Diagnostics->TargetBootChain) != 0)))
    {
      return EFI_BUFFER_TOO_SMALL;
    }
  }

  for (Index = 0; Index < EntryCount; Index++) {
    AsciiSPrint (NodeName, sizeof (NodeName), "esrt-entry-%u", (UINT32)Index);
    Node = FdtAddSubnode (Payload, 0, NodeName);
    if (Node < 0) {
      return EFI_BUFFER_TOO_SMALL;
    }

    GuidToCanonicalString (&Entries[Index].FirmwareClass, GuidString);
    if ((FdtSetPropString (
           Payload,
           Node,
           "nvidia,firmware-class",
           GuidString
           ) != 0) ||
        (FdtSetPropU32 (
           Payload,
           Node,
           "nvidia,firmware-version",
           Entries[Index].FirmwareVersion
           ) != 0) ||
        (FdtSetPropU32 (
           Payload,
           Node,
           "nvidia,lowest-supported-firmware-version",
           Entries[Index].LowestSupportedFirmwareVersion
           ) != 0) ||
        (FdtSetPropU32 (
           Payload,
           Node,
           "nvidia,last-attempt-version",
           Entries[Index].LastAttemptVersion
           ) != 0) ||
        (FdtSetPropU32 (
           Payload,
           Node,
           "nvidia,last-attempt-status",
           Entries[Index].LastAttemptStatus
           ) != 0))
    {
      return EFI_BUFFER_TOO_SMALL;
    }
  }

  if (FdtPack (Payload) != 0) {
    return EFI_COMPROMISED_DATA;
  }

  *PayloadLength = FdtTotalSize (Payload);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
NormalizeFmpImageInfo (
  IN  CONST EFI_GUID                                   *ImageTypeId,
  IN  CONST CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  *ImageInfoSet,
  OUT CAPSULE_ATTEMPT_RECORD_ESRT_ENTRY                *Entry
  )
{
  UINTN                                DescriptorIndex;
  UINTN                                RequiredSize;
  UINTN                                DescriptorsSize;
  CONST EFI_FIRMWARE_IMAGE_DESCRIPTOR  *Descriptor;
  BOOLEAN                              Found;

  if ((ImageInfoSet->ImageInfo == NULL) ||
      (ImageInfoSet->DescriptorCount == 0) ||
      (ImageInfoSet->DescriptorSize == 0) ||
      (ImageInfoSet->DescriptorVersion == 0))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (ImageInfoSet->DescriptorCount > MAX_UINTN / ImageInfoSet->DescriptorSize) {
    DEBUG ((DEBUG_ERROR, "%a: FMP descriptor array overflow\n", __FUNCTION__));
    return EFI_BAD_BUFFER_SIZE;
  }

  DescriptorsSize = ImageInfoSet->DescriptorCount * ImageInfoSet->DescriptorSize;
  if (DescriptorsSize > ImageInfoSet->ImageInfoSize) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: FMP descriptor array exceeds buffer: required=%u available=%u\n",
      __FUNCTION__,
      DescriptorsSize,
      ImageInfoSet->ImageInfoSize
      ));
    return EFI_BAD_BUFFER_SIZE;
  }

  RequiredSize = OFFSET_OF (EFI_FIRMWARE_IMAGE_DESCRIPTOR, AttributesSetting) +
                 sizeof (Descriptor->AttributesSetting);
  if (ImageInfoSet->DescriptorSize < RequiredSize) {
    DEBUG ((DEBUG_ERROR, "%a: FMP v1 desc is too small: %u\n", __FUNCTION__, ImageInfoSet->DescriptorSize));
    return EFI_INVALID_PARAMETER;
  }

  if (ImageInfoSet->DescriptorVersion >= 2) {
    RequiredSize = OFFSET_OF (
                     EFI_FIRMWARE_IMAGE_DESCRIPTOR,
                     LowestSupportedImageVersion
                     ) + sizeof (Descriptor->LowestSupportedImageVersion);
    if (ImageInfoSet->DescriptorSize < RequiredSize) {
      DEBUG ((DEBUG_ERROR, "%a: FMP v2 desc is too small: %u\n", __FUNCTION__, ImageInfoSet->DescriptorSize));
      return EFI_INVALID_PARAMETER;
    }
  }

  if (ImageInfoSet->DescriptorVersion >= 3) {
    RequiredSize = OFFSET_OF (EFI_FIRMWARE_IMAGE_DESCRIPTOR, LastAttemptStatus) +
                   sizeof (Descriptor->LastAttemptStatus);
    if (ImageInfoSet->DescriptorSize < RequiredSize) {
      DEBUG ((DEBUG_ERROR, "%a: FMP v3 desc is too small: %u\n", __FUNCTION__, ImageInfoSet->DescriptorSize));
      return EFI_INVALID_PARAMETER;
    }
  }

  Found = FALSE;
  for (DescriptorIndex = 0;
       DescriptorIndex < ImageInfoSet->DescriptorCount;
       DescriptorIndex++)
  {
    Descriptor = (CONST EFI_FIRMWARE_IMAGE_DESCRIPTOR *)(
                                                         (CONST UINT8 *)ImageInfoSet->ImageInfo +
                                                         DescriptorIndex * ImageInfoSet->DescriptorSize
                                                         );
    if (!CompareGuid (&Descriptor->ImageTypeId, ImageTypeId) ||
        ((Descriptor->AttributesSupported & IMAGE_ATTRIBUTE_IN_USE) == 0) ||
        ((Descriptor->AttributesSetting & IMAGE_ATTRIBUTE_IN_USE) == 0))
    {
      continue;
    }

    if (Found && (Entry->FirmwareVersion <= Descriptor->Version)) {
      continue;
    }

    DEBUG ((
      DEBUG_INFO,
      "%a: using FMP descriptor %u: version=%u lowest=%u last-version=%u last-status=%u\n",
      __FUNCTION__,
      DescriptorIndex,
      Descriptor->Version,
      (ImageInfoSet->DescriptorVersion >= 2) ? Descriptor->LowestSupportedImageVersion : 0,
      (ImageInfoSet->DescriptorVersion >= 3) ? Descriptor->LastAttemptVersion : 0,
      (ImageInfoSet->DescriptorVersion >= 3) ? Descriptor->LastAttemptStatus : LAST_ATTEMPT_STATUS_SUCCESS
      ));

    Found                                 = TRUE;
    Entry->FirmwareClass                  = Descriptor->ImageTypeId;
    Entry->FirmwareVersion                = Descriptor->Version;
    Entry->LowestSupportedFirmwareVersion = 0;
    Entry->LastAttemptVersion             = 0;
    Entry->LastAttemptStatus              = LAST_ATTEMPT_STATUS_SUCCESS;
    if (ImageInfoSet->DescriptorVersion >= 2) {
      Entry->LowestSupportedFirmwareVersion =
        Descriptor->LowestSupportedImageVersion;
    }

    if (ImageInfoSet->DescriptorVersion >= 3) {
      Entry->LastAttemptVersion = Descriptor->LastAttemptVersion;
      Entry->LastAttemptStatus  = Descriptor->LastAttemptStatus;
    }
  }

  if (!Found) {
    DEBUG ((DEBUG_INFO, "%a: no matching in-use system FMP desc found\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
FindLatestSlot (
  IN  CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context,
  OUT UINT32                          *LatestSlot,
  OUT UINT64                          *LatestSequence,
  OUT UINT32                          *FirstInvalidSlot OPTIONAL
  )
{
  EFI_STATUS                        Status;
  VOID                              *Buffer;
  UINT32                            Index;
  CAPSULE_ATTEMPT_RECORD_SLOT_INFO  SlotInfo;
  BOOLEAN                           Found;

  Buffer = AllocateBlockIoBuffer (Context);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Found = FALSE;
  if (FirstInvalidSlot != NULL) {
    *FirstInvalidSlot = MAX_UINT32;
  }

  for (Index = 0; Index < Context->SlotCount; Index++) {
    Status = ReadSlot (Context, Index, Buffer);
    if (!EFI_ERROR (Status)) {
      Status = CapsuleAttemptRecordValidateSlotBuffer (
                 Buffer,
                 Context->SlotSize,
                 &SlotInfo
                 );
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: slot %u is not valid: %r\n", __FUNCTION__, Index, Status));
      if ((FirstInvalidSlot != NULL) && (*FirstInvalidSlot == MAX_UINT32)) {
        *FirstInvalidSlot = Index;
      }

      continue;
    }

    DEBUG ((DEBUG_INFO, "%a: slot %u valid with sequence %Lu\n", __FUNCTION__, Index, SlotInfo.Sequence));

    if (!Found || CapsuleAttemptRecordSequenceIsNewer (
                    SlotInfo.Sequence,
                    *LatestSequence
                    ))
    {
      Found           = TRUE;
      *LatestSlot     = Index;
      *LatestSequence = SlotInfo.Sequence;
    }
  }

  FreeBlockIoBuffer (Context, Buffer);
  if (!Found) {
    DEBUG ((DEBUG_INFO, "%a: slot scan found no valid records\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  DEBUG ((DEBUG_INFO, "%a: latest record is slot %u sequence %Lu\n", __FUNCTION__, *LatestSlot, *LatestSequence));
  return EFI_SUCCESS;
}

STATIC
VOID
BuildSlotHeader (
  OUT UINT8   *SlotBuffer,
  IN  UINT64  Sequence,
  IN  UINT32  PayloadLength
  )
{
  CAPSULE_ATTEMPT_RECORD_HEADER  Header;

  ZeroMem (&Header, sizeof (Header));
  CopyMem (Header.Magic, mRecordMagic, sizeof (mRecordMagic));
  Header.Version       = CAPSULE_ATTEMPT_RECORD_HEADER_VERSION;
  Header.HeaderSize    = CAPSULE_ATTEMPT_RECORD_HEADER_SIZE;
  Header.Sequence      = Sequence;
  Header.PayloadOffset = CAPSULE_ATTEMPT_RECORD_HEADER_SIZE;
  Header.PayloadLength = PayloadLength;
  Header.PayloadFormat = CAPSULE_ATTEMPT_RECORD_PAYLOAD_FORMAT;
  Header.PayloadCrc    = CalculateCrc32 (
                           SlotBuffer + CAPSULE_ATTEMPT_RECORD_HEADER_SIZE,
                           PayloadLength
                           );
  Header.HeaderCrc = CalculateCrc32 (&Header, sizeof (Header));
  CopyMem (SlotBuffer, &Header, sizeof (Header));
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordInitialize (
  IN  CONST VOID                      *PlatformDtb,
  IN  UINTN                           PlatformDtbSize,
  OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  **Context
  )
{
  EFI_STATUS                      Status;
  CAPSULE_ATTEMPT_RECORD_CONTEXT  *NewContext;

  if (Context == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Context   = NULL;
  NewContext = AllocateZeroPool (sizeof (*NewContext));
  if (NewContext == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = CapsuleAttemptRecordParseStorageConfig (
             PlatformDtb,
             PlatformDtbSize,
             NewContext
             );
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: DT parse succeeded\n", __FUNCTION__));
    Status = LocatePartition (NewContext);
  } else {
    DEBUG ((DEBUG_INFO, "%a: DT parse failed: %r\n", __FUNCTION__, Status));
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: initialization failed: %r\n", __FUNCTION__, Status));
    FreePool (NewContext);
    return Status;
  }

  *Context = NewContext;
  DEBUG ((DEBUG_INFO, "%a: initialization succeeded\n", __FUNCTION__));
  return EFI_SUCCESS;
}

VOID
EFIAPI
CapsuleAttemptRecordDeinitialize (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  **Context
  )
{
  if ((Context != NULL) && (*Context != NULL)) {
    FreePool (*Context);
    *Context = NULL;
  }
}

STATIC
EFI_STATUS
CommitRecord (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT            *Context,
  IN OUT CAPSULE_ATTEMPT_RECORD_LIFECYCLE          *Lifecycle,
  IN     CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  *Diagnostics OPTIONAL,
  IN     CONST CAPSULE_ATTEMPT_RECORD_ESRT_ENTRY   *Entries OPTIONAL,
  IN     UINTN                                     EntryCount
  )
{
  EFI_STATUS                        Status;
  VOID                              *SlotBuffer;
  VOID                              *VerifyBuffer;
  UINT32                            PayloadLength;
  UINT32                            LatestSlot;
  UINT32                            TargetSlot;
  UINT32                            FirstInvalidSlot;
  UINT64                            LatestSequence;
  UINT64                            NewSequence;
  EFI_LBA                           Lba;
  CAPSULE_ATTEMPT_RECORD_SLOT_INFO  SlotInfo;

  if ((Context == NULL) || (Context->BlockIo == NULL) ||
      (Lifecycle == NULL) || ((EntryCount != 0) && (Entries == NULL)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = ValidateBlockIoRegion (Context);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: update region validation failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (Context->BlockIo->Media->ReadOnly) {
    DEBUG ((DEBUG_ERROR, "%a: update rejected read-only media\n", __FUNCTION__));
    return EFI_WRITE_PROTECTED;
  }

  if ((Context->SlotSize - CAPSULE_ATTEMPT_RECORD_HEADER_SIZE) > MAX_UINT32) {
    DEBUG ((DEBUG_ERROR, "%a: payload capacity exceeds limit: %u\n", __FUNCTION__, Context->SlotSize));
    return EFI_BAD_BUFFER_SIZE;
  }

  SlotBuffer   = AllocateBlockIoBuffer (Context);
  VerifyBuffer = AllocateBlockIoBuffer (Context);
  if ((SlotBuffer == NULL) || (VerifyBuffer == NULL)) {
    FreeBlockIoBuffer (Context, SlotBuffer);
    FreeBlockIoBuffer (Context, VerifyBuffer);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = FindLatestSlot (
             Context,
             &LatestSlot,
             &LatestSequence,
             &FirstInvalidSlot
             );
  if (Status == EFI_NOT_FOUND) {
    TargetSlot  = 0;
    NewSequence = 1;
    DEBUG ((DEBUG_INFO, "%a: creating first record in slot 0 sequence 1\n", __FUNCTION__));
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: slot scan failed: %r\n", __FUNCTION__, Status));
    goto Done;
  } else {
    NewSequence = LatestSequence + 1;
    if (FirstInvalidSlot != MAX_UINT32) {
      TargetSlot = FirstInvalidSlot;
    } else if (Context->SlotCount == 1) {
      TargetSlot = 0;
    } else {
      TargetSlot = (LatestSlot + 1) % Context->SlotCount;
    }

    DEBUG ((
      DEBUG_INFO,
      "%a: advancing record: latest-slot=%u latest-sequence=%Lu first-invalid=%u target-slot=%u new-sequence=%Lu\n",
      __FUNCTION__,
      LatestSlot,
      LatestSequence,
      FirstInvalidSlot,
      TargetSlot,
      NewSequence
      ));
  }

  if (!Lifecycle->AttemptIdValid) {
    Lifecycle->AttemptId      = NewSequence;
    Lifecycle->AttemptIdValid = TRUE;
  }

  ZeroMem (SlotBuffer, Context->SlotSize);
  Status = CapsuleAttemptRecordBuildPayload (
             (UINT8 *)SlotBuffer + CAPSULE_ATTEMPT_RECORD_HEADER_SIZE,
             (UINT32)(Context->SlotSize - CAPSULE_ATTEMPT_RECORD_HEADER_SIZE),
             Lifecycle,
             Diagnostics,
             Entries,
             EntryCount,
             &PayloadLength
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: payload build failed: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  DEBUG ((DEBUG_INFO, "%a: payload built: length=%u\n", __FUNCTION__, PayloadLength));

  BuildSlotHeader (SlotBuffer, NewSequence, PayloadLength);
  Lba = DivU64x32 (
          Context->RegionOffset + MultU64x64 (TargetSlot, Context->SlotSize),
          Context->BlockIo->Media->BlockSize
          );
  DEBUG ((
    DEBUG_INFO,
    "%a: writing record: slot=%u sequence=%Lu lba=%Lu size=%u\n",
    __FUNCTION__,
    TargetSlot,
    NewSequence,
    Lba,
    Context->SlotSize
    ));
  Status = Context->BlockIo->WriteBlocks (
                               Context->BlockIo,
                               Context->BlockIo->Media->MediaId,
                               Lba,
                               Context->SlotSize,
                               SlotBuffer
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: record write failed: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  Status = Context->BlockIo->FlushBlocks (Context->BlockIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: record flush failed: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  Status = ReadSlot (Context, TargetSlot, VerifyBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: record verification read failed: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  if (CompareMem (SlotBuffer, VerifyBuffer, Context->SlotSize) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: record verification byte comparison failed\n", __FUNCTION__));
    Status = EFI_COMPROMISED_DATA;
    goto Done;
  }

  Status = CapsuleAttemptRecordValidateSlotBuffer (
             VerifyBuffer,
             Context->SlotSize,
             &SlotInfo
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: record verification validation failed: %r\n", __FUNCTION__, Status));
  } else {
    DEBUG ((DEBUG_INFO, "%a: record update verified successfully: slot=%u sequence=%Lu\n", __FUNCTION__, TargetSlot, NewSequence));
  }

Done:
  FreeBlockIoBuffer (Context, VerifyBuffer);
  FreeBlockIoBuffer (Context, SlotBuffer);
  return Status;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordBegin (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context,
  IN     CAPSULE_ATTEMPT_RECORD_SOURCE   Source,
  IN     CONST EFI_TIME                  *Timestamp OPTIONAL,
  IN     CONST UINT32                    *ActiveBootChain OPTIONAL,
  OUT    UINT64                          *AttemptId
  )
{
  EFI_STATUS                        Status;
  CAPSULE_ATTEMPT_RECORD_LIFECYCLE  Lifecycle;

  if ((AttemptId == NULL) || (Source <= CapsuleAttemptRecordSourceUnknown) ||
      (Source >= CapsuleAttemptRecordSourceMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&Lifecycle, sizeof (Lifecycle));
  Lifecycle.AttemptId      = 0;
  Lifecycle.AttemptIdValid = FALSE;
  if (Timestamp != NULL) {
    Lifecycle.Timestamp      = *Timestamp;
    Lifecycle.TimestampValid = TRUE;
  }

  if (ActiveBootChain != NULL) {
    Lifecycle.ActiveBootChain      = *ActiveBootChain;
    Lifecycle.ActiveBootChainValid = TRUE;
  }

  Lifecycle.Source = Source;
  Lifecycle.State  = CapsuleAttemptRecordStateInProgress;
  Status           = CommitRecord (Context, &Lifecycle, NULL, NULL, 0);
  if (!EFI_ERROR (Status)) {
    *AttemptId = Lifecycle.AttemptId;
  }

  return Status;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordFinalize (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT                   *Context,
  IN     CAPSULE_ATTEMPT_RECORD_SOURCE                    Source,
  IN     UINT64                                           AttemptId,
  IN     BOOLEAN                                          AttemptIdValid,
  IN     CONST EFI_TIME                                   *Timestamp OPTIONAL,
  IN     CONST UINT64                                     *ElapsedTimeNs OPTIONAL,
  IN     CONST UINT32                                     *ActiveBootChain OPTIONAL,
  IN     CONST EFI_GUID                                   *ImageTypeId,
  IN     CONST CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  *ImageInfoSet,
  IN     CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS         *Diagnostics OPTIONAL
  )
{
  EFI_STATUS                         Status;
  CAPSULE_ATTEMPT_RECORD_ESRT_ENTRY  Entry;
  CAPSULE_ATTEMPT_RECORD_LIFECYCLE   Lifecycle;

  if ((ImageTypeId == NULL) || (ImageInfoSet == NULL) ||
      (Source < CapsuleAttemptRecordSourceUnknown) ||
      (Source >= CapsuleAttemptRecordSourceMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&Entry, sizeof (Entry));
  Status = NormalizeFmpImageInfo (ImageTypeId, ImageInfoSet, &Entry);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FMP image info normalization failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  ZeroMem (&Lifecycle, sizeof (Lifecycle));
  Lifecycle.AttemptId      = AttemptId;
  Lifecycle.AttemptIdValid = AttemptIdValid;
  if (Timestamp != NULL) {
    Lifecycle.Timestamp      = *Timestamp;
    Lifecycle.TimestampValid = TRUE;
  }

  if (ElapsedTimeNs != NULL) {
    Lifecycle.ElapsedTimeNs    = *ElapsedTimeNs;
    Lifecycle.ElapsedTimeValid = TRUE;
  }

  if (ActiveBootChain != NULL) {
    Lifecycle.ActiveBootChain      = *ActiveBootChain;
    Lifecycle.ActiveBootChainValid = TRUE;
  }

  Lifecycle.Source = Source;
  Lifecycle.State  = CapsuleAttemptRecordStateCompleted;
  return CommitRecord (Context, &Lifecycle, Diagnostics, &Entry, 1);
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordUpdateDiagnostics (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT            *Context,
  IN     CAPSULE_ATTEMPT_RECORD_SOURCE             Source,
  IN     UINT64                                    AttemptId,
  IN     CONST EFI_TIME                            *Timestamp OPTIONAL,
  IN     CONST UINT32                              *ActiveBootChain OPTIONAL,
  IN     CONST CAPSULE_ATTEMPT_RECORD_DIAGNOSTICS  *Diagnostics
  )
{
  CAPSULE_ATTEMPT_RECORD_LIFECYCLE  Lifecycle;

  if ((Diagnostics == NULL) ||
      (Source <= CapsuleAttemptRecordSourceUnknown) ||
      (Source >= CapsuleAttemptRecordSourceMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&Lifecycle, sizeof (Lifecycle));
  Lifecycle.AttemptId      = AttemptId;
  Lifecycle.AttemptIdValid = TRUE;
  if (Timestamp != NULL) {
    Lifecycle.Timestamp      = *Timestamp;
    Lifecycle.TimestampValid = TRUE;
  }

  if (ActiveBootChain != NULL) {
    Lifecycle.ActiveBootChain      = *ActiveBootChain;
    Lifecycle.ActiveBootChainValid = TRUE;
  }

  Lifecycle.Source = Source;
  Lifecycle.State  = CapsuleAttemptRecordStateInProgress;
  return CommitRecord (Context, &Lifecycle, Diagnostics, NULL, 0);
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordUpdateFmpImageInfo (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT                   *Context,
  IN     CONST EFI_GUID                                   *ImageTypeId,
  IN     CONST CAPSULE_ATTEMPT_RECORD_FMP_IMAGE_INFO_SET  *ImageInfoSet
  )
{
  return CapsuleAttemptRecordFinalize (
           Context,
           CapsuleAttemptRecordSourceUnknown,
           0,
           FALSE,
           NULL,
           NULL,
           NULL,
           ImageTypeId,
           ImageInfoSet,
           NULL
           );
}

STATIC
EFI_STATUS
GetRequiredPayloadU32 (
  IN  CONST VOID   *Payload,
  IN  INT32        Node,
  IN  CONST CHAR8  *PropertyName,
  OUT UINT32       *Value
  )
{
  INT32       Length;
  CONST VOID  *Property;
  UINT32      PropertyValue;

  Property = FdtGetProp (Payload, Node, PropertyName, &Length);
  if ((Property == NULL) || (Length != sizeof (PropertyValue))) {
    return EFI_COMPROMISED_DATA;
  }

  CopyMem (&PropertyValue, Property, sizeof (PropertyValue));
  *Value = Fdt32ToCpu (PropertyValue);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DumpPayloadString (
  IN CONST UINT8  *Value,
  IN UINTN        Length
  )
{
  UINTN  Index;

  if (Length == 0) {
    DEBUG ((DEBUG_ERROR, " = \"\";\n"));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_ERROR, " = \""));
  for (Index = 0; Index < Length - 1; Index++) {
    switch (Value[Index]) {
      case '\a':
        DEBUG ((DEBUG_ERROR, "\\a"));
        break;
      case '\b':
        DEBUG ((DEBUG_ERROR, "\\b"));
        break;
      case '\t':
        DEBUG ((DEBUG_ERROR, "\\t"));
        break;
      case '\n':
        DEBUG ((DEBUG_ERROR, "\\n"));
        break;
      case '\v':
        DEBUG ((DEBUG_ERROR, "\\v"));
        break;
      case '\f':
        DEBUG ((DEBUG_ERROR, "\\f"));
        break;
      case '\r':
        DEBUG ((DEBUG_ERROR, "\\r"));
        break;
      case '\\':
        DEBUG ((DEBUG_ERROR, "\\\\"));
        break;
      case '"':
        DEBUG ((DEBUG_ERROR, "\\\""));
        break;
      case '\0':
        DEBUG ((DEBUG_ERROR, "\\0"));
        break;
      default:
        DEBUG ((DEBUG_ERROR, "%c", Value[Index]));
        break;
    }
  }

  DEBUG ((DEBUG_ERROR, "\";\n"));
  return EFI_SUCCESS;
}

EFI_STATUS
CapsuleAttemptRecordFormatFmpImageDigest (
  IN  CONST UINT8  *FmpImageDigest,
  IN  UINTN        FmpImageDigestSize,
  OUT CHAR8        *DigestText,
  IN  UINTN        DigestTextSize
  )
{
  STATIC CONST CHAR8  HexDigits[] = "0123456789abcdef";
  UINTN               Index;
  UINTN               TextIndex;

  if ((FmpImageDigest == NULL) || (DigestText == NULL) ||
      (FmpImageDigestSize != CAPSULE_ATTEMPT_RECORD_SHA256_DIGEST_SIZE))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (DigestTextSize < CAPSULE_ATTEMPT_RECORD_SHA256_TEXT_SIZE) {
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (DigestText, "sha256:", sizeof ("sha256:") - 1);
  TextIndex = sizeof ("sha256:") - 1;
  for (Index = 0; Index < FmpImageDigestSize; Index++) {
    DigestText[TextIndex++] = HexDigits[FmpImageDigest[Index] >> 4];
    DigestText[TextIndex++] = HexDigits[FmpImageDigest[Index] & 0xF];
  }

  DigestText[TextIndex] = '\0';
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DumpPayloadFmpImageDigest (
  IN CONST VOID  *Payload
  )
{
  EFI_STATUS   Status;
  CONST CHAR8  *Algorithm;
  CONST VOID   *Digest;
  UINTN        AlgorithmLength;
  INT32        DigestLength;
  CHAR8        DigestText[CAPSULE_ATTEMPT_RECORD_SHA256_TEXT_SIZE];

  Digest = FdtGetProp (
             Payload,
             0,
             "nvidia,fmp-image-digest",
             &DigestLength
             );
  Status = GetRequiredStringProperty (
             Payload,
             0,
             "nvidia,fmp-image-digest-algorithm",
             &Algorithm,
             &AlgorithmLength
             );
  if ((Digest == NULL) && (Status == EFI_NOT_FOUND)) {
    return EFI_SUCCESS;
  }

  if ((Digest == NULL) || EFI_ERROR (Status) ||
      (AsciiStrCmp (Algorithm, "sha256") != 0) ||
      (DigestLength != CAPSULE_ATTEMPT_RECORD_SHA256_DIGEST_SIZE))
  {
    return EFI_COMPROMISED_DATA;
  }

  Status = CapsuleAttemptRecordFormatFmpImageDigest (
             Digest,
             DigestLength,
             DigestText,
             sizeof (DigestText)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_ERROR, "Capsule attempt FMP image digest: %a\n", DigestText));
  return EFI_SUCCESS;
}

STATIC
BOOLEAN
PayloadPropertyIsString (
  IN CONST UINT8  *Value,
  IN UINTN        Length
  )
{
  UINTN  Index;
  UINTN  NullCount;

  if ((Length == 0) || (Value[Length - 1] != '\0')) {
    return FALSE;
  }

  NullCount = 0;
  for (Index = 0; Index < Length; Index++) {
    if (Value[Index] == '\0') {
      NullCount++;
    } else if (((Value[Index] < 0x20) || (Value[Index] > 0x7E)) &&
               (Value[Index] != '\a') && (Value[Index] != '\b') &&
               (Value[Index] != '\t') && (Value[Index] != '\n') &&
               (Value[Index] != '\v') && (Value[Index] != '\f') &&
               (Value[Index] != '\r'))
    {
      return FALSE;
    }
  }

  // Match dtc's heuristic that avoids treating mostly-NUL cell data as text.
  return (BOOLEAN)(NullCount <= Length - NullCount);
}

STATIC
EFI_STATUS
DumpPayloadPropertyValue (
  IN CONST VOID  *Property,
  IN UINTN       PropertyLength
  )
{
  CONST UINT8  *Value;
  UINTN        Index;
  UINT32       Cell;

  Value = Property;
  if (PropertyLength == 0) {
    DEBUG ((DEBUG_ERROR, ";\n"));
    return EFI_SUCCESS;
  }

  if (PayloadPropertyIsString (Value, PropertyLength)) {
    return DumpPayloadString (Value, PropertyLength);
  }

  if ((PropertyLength % sizeof (Cell)) == 0) {
    DEBUG ((DEBUG_ERROR, " = <"));
    for (Index = 0; Index < PropertyLength; Index += sizeof (Cell)) {
      CopyMem (&Cell, Value + Index, sizeof (Cell));
      DEBUG ((
        DEBUG_ERROR,
        (Index == 0) ? "0x%x" : " 0x%x",
        Fdt32ToCpu (Cell)
        ));
    }

    DEBUG ((DEBUG_ERROR, ">;\n"));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_ERROR, " = ["));
  for (Index = 0; Index < PropertyLength; Index++) {
    DEBUG ((DEBUG_ERROR, (Index == 0) ? "%02x" : " %02x", Value[Index]));
  }

  DEBUG ((DEBUG_ERROR, "];\n"));
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DumpPayloadNode (
  IN CONST VOID  *Payload,
  IN INT32       Node
  )
{
  EFI_STATUS   Status;
  CONST CHAR8  *Name;
  CONST CHAR8  *PropertyName;
  CONST VOID   *Property;
  INT32        NameLength;
  INT32        PropertyLength;
  INT32        PropertyOffset;

  Name = FdtGetName ((VOID *)Payload, Node, &NameLength);
  if ((Name == NULL) || (NameLength < 0)) {
    return EFI_COMPROMISED_DATA;
  }

  DEBUG ((DEBUG_ERROR, "Capsule attempt node: %a\n", Name));
  for (PropertyOffset = FdtFirstPropertyOffset (Payload, Node);
       PropertyOffset >= 0;
       PropertyOffset = FdtNextPropertyOffset (Payload, PropertyOffset))
  {
    Property = FdtGetPropertyValueByOffset (
                 Payload,
                 PropertyOffset,
                 &PropertyName,
                 &PropertyLength
                 );
    if ((Property == NULL) || (PropertyName == NULL) || (PropertyLength < 0)) {
      return EFI_COMPROMISED_DATA;
    }

    DEBUG ((DEBUG_ERROR, "  %a", PropertyName));
    Status = DumpPayloadPropertyValue (Property, PropertyLength);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DumpPayload (
  IN CONST VOID  *Payload,
  IN UINT32      PayloadLength
  )
{
  EFI_STATUS   Status;
  CONST CHAR8  *String;
  UINTN        StringLength;
  UINT32       FormatVersion;
  INT32        Node;
  INT32        AttemptIdLength;
  CONST VOID   *AttemptIdProperty;
  UINT64       AttemptIdValue;
  UINT64       AttemptId;
  CONST CHAR8  *Source;
  CONST CHAR8  *State;
  CONST CHAR8  *Timestamp;
  CONST VOID   *SourceProperty;
  CONST VOID   *StateProperty;
  CONST VOID   *TimestampProperty;
  CONST VOID   *ElapsedTimeProperty;
  CONST VOID   *ActiveBootChainProperty;
  INT32        SourcePropertyLength;
  INT32        StatePropertyLength;
  INT32        TimestampPropertyLength;
  INT32        ElapsedTimePropertyLength;
  INT32        ActiveBootChainPropertyLength;
  UINTN        SourceLength;
  UINTN        StateLength;
  UINTN        TimestampLength;
  EFI_STATUS   SourceStatus;
  EFI_STATUS   StateStatus;
  EFI_STATUS   TimestampStatus;
  UINT64       ElapsedTimeValue;
  UINT64       ElapsedTimeNs;
  UINT64       ElapsedTimeSeconds;
  UINT64       ElapsedTimeHours;
  UINT32       ElapsedTimeNanoseconds;
  UINT32       ElapsedTimeSecondsWithinHour;
  UINT32       ElapsedTimeMinutes;
  UINT32       ElapsedTimeSecondsWithinMinute;
  UINT32       ActiveBootChainValue;
  UINT32       ActiveBootChain;

  if ((FdtCheckHeader (Payload) != 0) || (FdtTotalSize (Payload) != PayloadLength)) {
    return EFI_COMPROMISED_DATA;
  }

  Status = GetRequiredStringProperty (
             Payload,
             0,
             "compatible",
             &String,
             &StringLength
             );
  if (EFI_ERROR (Status) ||
      (AsciiStrCmp (String, CAPSULE_ATTEMPT_RECORD_PAYLOAD_COMPATIBLE) != 0))
  {
    return EFI_UNSUPPORTED;
  }

  Status = GetRequiredPayloadU32 (
             Payload,
             0,
             "nvidia,format-version",
             &FormatVersion
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (FormatVersion != CAPSULE_ATTEMPT_RECORD_PAYLOAD_FORMAT) {
    return EFI_UNSUPPORTED;
  }

  AttemptIdProperty = FdtGetProp (
                        Payload,
                        0,
                        "nvidia,attempt-id",
                        &AttemptIdLength
                        );
  SourceProperty = FdtGetProp (
                     Payload,
                     0,
                     "nvidia,attempt-source",
                     &SourcePropertyLength
                     );
  StateProperty = FdtGetProp (
                    Payload,
                    0,
                    "nvidia,attempt-state",
                    &StatePropertyLength
                    );
  TimestampProperty = FdtGetProp (
                        Payload,
                        0,
                        "nvidia,timestamp",
                        &TimestampPropertyLength
                        );
  ElapsedTimeProperty = FdtGetProp (
                          Payload,
                          0,
                          "nvidia,elapsed-time-ns",
                          &ElapsedTimePropertyLength
                          );
  ActiveBootChainProperty = FdtGetProp (
                              Payload,
                              0,
                              "nvidia,active-boot-chain",
                              &ActiveBootChainPropertyLength
                              );
  SourceStatus = GetRequiredStringProperty (
                   Payload,
                   0,
                   "nvidia,attempt-source",
                   &Source,
                   &SourceLength
                   );
  StateStatus = GetRequiredStringProperty (
                  Payload,
                  0,
                  "nvidia,attempt-state",
                  &State,
                  &StateLength
                  );
  TimestampStatus = GetRequiredStringProperty (
                      Payload,
                      0,
                      "nvidia,timestamp",
                      &Timestamp,
                      &TimestampLength
                      );
  if ((AttemptIdProperty == NULL) && (SourceProperty == NULL) &&
      (StateProperty == NULL))
  {
    DEBUG ((DEBUG_ERROR, "Capsule attempt lifecycle: legacy Phase 1 record\n"));
  } else {
    if ((AttemptIdProperty == NULL) ||
        (AttemptIdLength != sizeof (AttemptIdValue)) ||
        EFI_ERROR (SourceStatus) || EFI_ERROR (StateStatus))
    {
      return EFI_COMPROMISED_DATA;
    }

    CopyMem (&AttemptIdValue, AttemptIdProperty, sizeof (AttemptIdValue));
    AttemptId = Fdt64ToCpu (AttemptIdValue);
    DEBUG ((
      DEBUG_ERROR,
      "Capsule attempt lifecycle: id=%Lu source=%a state=%a\n",
      AttemptId,
      Source,
      State
      ));
  }

  if (ActiveBootChainProperty != NULL) {
    if (ActiveBootChainPropertyLength != sizeof (ActiveBootChainValue)) {
      return EFI_COMPROMISED_DATA;
    }

    CopyMem (
      &ActiveBootChainValue,
      ActiveBootChainProperty,
      sizeof (ActiveBootChainValue)
      );
    ActiveBootChain = Fdt32ToCpu (ActiveBootChainValue);
    DEBUG ((DEBUG_ERROR, "Capsule attempt active boot chain: %u\n", ActiveBootChain));
  }

  if (TimestampProperty != NULL) {
    if (EFI_ERROR (TimestampStatus)) {
      return EFI_COMPROMISED_DATA;
    }

    DEBUG ((DEBUG_ERROR, "Capsule attempt timestamp: %a\n", Timestamp));
  }

  if (ElapsedTimeProperty != NULL) {
    if (ElapsedTimePropertyLength != sizeof (ElapsedTimeValue)) {
      return EFI_COMPROMISED_DATA;
    }

    CopyMem (
      &ElapsedTimeValue,
      ElapsedTimeProperty,
      sizeof (ElapsedTimeValue)
      );
    ElapsedTimeNs      = Fdt64ToCpu (ElapsedTimeValue);
    ElapsedTimeSeconds = DivU64x32Remainder (
                           ElapsedTimeNs,
                           1000000000U,
                           &ElapsedTimeNanoseconds
                           );
    ElapsedTimeHours = DivU64x32Remainder (
                         ElapsedTimeSeconds,
                         3600U,
                         &ElapsedTimeSecondsWithinHour
                         );
    ElapsedTimeMinutes             = ElapsedTimeSecondsWithinHour / 60U;
    ElapsedTimeSecondsWithinMinute = ElapsedTimeSecondsWithinHour % 60U;
    DEBUG ((
      DEBUG_ERROR,
      "Capsule attempt elapsed time: %Lu:%02u:%02u.%09u\n",
      ElapsedTimeHours,
      ElapsedTimeMinutes,
      ElapsedTimeSecondsWithinMinute,
      ElapsedTimeNanoseconds
      ));
  }

  Status = DumpPayloadFmpImageDigest (Payload);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FdtForEachSubnode (Node, Payload, 0) {
    Status = DumpPayloadNode (Payload, Node);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CapsuleAttemptRecordDumpLatest (
  IN OUT CAPSULE_ATTEMPT_RECORD_CONTEXT  *Context
  )
{
  EFI_STATUS                        Status;
  UINT32                            LatestSlot;
  UINT64                            LatestSequence;
  VOID                              *Buffer;
  CAPSULE_ATTEMPT_RECORD_SLOT_INFO  SlotInfo;

  if ((Context == NULL) || (Context->BlockIo == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ValidateBlockIoRegion (Context);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: dump region validation failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = FindLatestSlot (Context, &LatestSlot, &LatestSequence, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: dump found no readable record: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Buffer = AllocateBlockIoBuffer (Context);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = ReadSlot (Context, LatestSlot, Buffer);
  if (!EFI_ERROR (Status)) {
    Status = CapsuleAttemptRecordValidateSlotBuffer (
               Buffer,
               Context->SlotSize,
               &SlotInfo
               );
  }

  if (!EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Capsule attempt record: slot=%u sequence=%Lu payload-format=%u length=%u\n",
      LatestSlot,
      LatestSequence,
      SlotInfo.PayloadFormat,
      SlotInfo.PayloadLength
      ));
    if (SlotInfo.PayloadFormat != CAPSULE_ATTEMPT_RECORD_PAYLOAD_FORMAT) {
      DEBUG ((DEBUG_ERROR, "%a: dump payload format unsupported: %u\n", __FUNCTION__, SlotInfo.PayloadFormat));
      Status = EFI_UNSUPPORTED;
    } else {
      Status = DumpPayload (
                 (UINT8 *)Buffer + SlotInfo.PayloadOffset,
                 SlotInfo.PayloadLength
                 );
    }
  }

  FreeBlockIoBuffer (Context, Buffer);
  return Status;
}
