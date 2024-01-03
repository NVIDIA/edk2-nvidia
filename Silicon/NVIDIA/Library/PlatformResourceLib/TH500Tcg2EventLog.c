/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PrintLib.h>
#include <IndustryStandard/UefiTcgPlatform.h>
#include <Guid/TcgEventHob.h>

#include "TH500ResourceConfigPrivate.h"
#include <TH500/TH500Definitions.h>

#define MAX_EVENT_DATA_SIZE  64

#define SIZE_OF_BLOB_DESCRIPTION_SIZE  sizeof (UINT8)
#define SIZE_OF_BLOB_BASE              sizeof (EFI_PHYSICAL_ADDRESS)
#define SIZE_OF_BLOB_LENGTH            sizeof (UINT64)

typedef struct {
  UINT32           MagicId;
  UINT32           SocketId;
  TCG_EVENTTYPE    EventType;
  CHAR8            *EventStr;
  UINT8            Instance;
} EVENT_TYPE_ENTRY;

EVENT_TYPE_ENTRY  mEventTypeTable[] = {
  { 0x46555345 /* FUSE */, 0, EV_TABLE_OF_DEVICES, "SYS_CONF_FUSE",    0 },
  { 0x42435442 /* BCTB */, 0, EV_TABLE_OF_DEVICES, "SYS_CONF_BCTB",    0 },
  { 0x50534342 /* PSCB */, 0, EV_POST_CODE,        "SYS_CTRL_PSCB",    0 },
  { 0x4d423142 /* MB1B */, 0, EV_POST_CODE,        "SYS_CTRL_MB1B",    0 },
  { 0x4d424354 /* MBCT */, 0, EV_TABLE_OF_DEVICES, "SYS_CONF_MBCT",    0 },
  { 0x4d454d30 /* MEM0 */, 0, EV_TABLE_OF_DEVICES, "SYS_CONF_MEM0",    0 },
  { 0x4d454d31 /* MEM1 */, 0, EV_TABLE_OF_DEVICES, "SYS_CONF_MEM1",    0 },
  { 0x4d454d32 /* MEM2 */, 0, EV_TABLE_OF_DEVICES, "SYS_CONF_MEM2",    0 },
  { 0x4d454d33 /* MEM3 */, 0, EV_TABLE_OF_DEVICES, "SYS_CONF_MEM3",    0 },
  { 0x4d494e46 /* MINF */, 0, EV_POST_CODE,        "SYS_CTRL_MINF",    0 },
  { 0x5342494e /* SBIN */, 1, EV_POST_CODE,        "SYS_CTRL_SBIN%u1", 0 },
  { 0x53424354 /* SBCT */, 1, EV_TABLE_OF_DEVICES, "SYS_CONF_SBCT%u1", 0 },
  { 0x5342494e /* SBIN */, 2, EV_POST_CODE,        "SYS_CTRL_SBIN%u2", 0 },
  { 0x53424354 /* SBCT */, 2, EV_TABLE_OF_DEVICES, "SYS_CONF_SBCT%u2", 0 },
  { 0x5342494e /* SBIN */, 3, EV_POST_CODE,        "SYS_CTRL_SBIN%u3", 0 },
  { 0x53424354 /* SBCT */, 3, EV_TABLE_OF_DEVICES, "SYS_CONF_SBCT%u3", 0 },
  { 0x4d54534d /* MTSM */, 0, EV_POST_CODE,        "SYS_CTRL_MTSM",    0 },
  { 0x5046574d /* PFWM */, 0, EV_POST_CODE,        "SYS_CTRL_PFWM",    0 },
  { 0x42504d46 /* BPMF */, 0, EV_POST_CODE,        "SYS_CTRL_BPMF",    0 },
  { 0x42504d44 /* BPMD */, 0, EV_TABLE_OF_DEVICES, "SYS_CONF_BPMD",    0 },
  { 0x4d423242 /* MB2B */, 0, EV_POST_CODE,        "SYS_CTRL_MB2B",    0 },
  { 0x4350424c /* CPBL */, 0, EV_POST_CODE,        "BL_33",            0 },
  { 0x424c3331 /* BL31 */, 0, EV_POST_CODE,        "SECURE_RT_EL3",    0 },
  { 0x41544644 /* ATFD */, 0, EV_TABLE_OF_DEVICES, "SECURE_DTB_EL3",   0 },
  { 0x424c3332 /* BL32 */, 0, EV_POST_CODE,        "SECURE_RT_EL2",    0 },
  { 0x48414644 /* HAFD */, 0, EV_TABLE_OF_DEVICES, "SECURE_DTB_EL2",   0 },
  { 0x53503031 /* SP01 */, 0, EV_POST_CODE,        "SECURE_RT_EL0_1",  0 },
  { 0x53443031 /* SD01 */, 0, EV_TABLE_OF_DEVICES, "SECURE_DTB_EL0_1", 0 },
  { 0x53503032 /* SP02 */, 0, EV_POST_CODE,        "SECURE_RT_EL0_2",  0 },
  { 0x53443032 /* SD02 */, 0, EV_TABLE_OF_DEVICES, "SECURE_DTB_EL0_2", 0 },
  { 0x53503033 /* SP03 */, 0, EV_POST_CODE,        "SECURE_RT_EL0_3",  0 },
  { 0x53443033 /* SD03 */, 0, EV_TABLE_OF_DEVICES, "SECURE_DTB_EL0_3", 0 },
  { 0x53503034 /* SP04 */, 0, EV_POST_CODE,        "SECURE_RT_EL0_4",  0 },
  { 0x53443034 /* SD04 */, 0, EV_TABLE_OF_DEVICES, "SECURE_DTB_EL0_4", 0 }
};

/**
  Get event data based on magic ID

  @param[in]      LogEntry   Pointer to the measurement entry
  @param[in]      EventType  Pointer to EventType variable
  @param[in,out]  EventSize  Pointer to EventSize
  @param[in,out]  EventData  Pointer to pre-allocated buffer to hold the Event data.

  @retval EFI_SUCCESS        Found event data
  @retval other              Fail to obtain event data
**/
EFI_STATUS
TH500GetEventData (
  IN  TEGRABL_TPM_COMMIT_LOG_ENTRY  *LogEntry,
  OUT TCG_EVENTTYPE                 *EventType,
  IN OUT UINT32                     *EventSize,
  IN OUT UINT8                      *EventData
  )
{
  UINTN   Index;
  UINT8   *DataPtr;
  UINT32  EventStrLen;
  UINT32  NewEventSize;

  if ((LogEntry == NULL) || (EventType == NULL) || (EventSize == NULL) || (EventData == NULL)) {
    ASSERT (FALSE);
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < ARRAY_SIZE (mEventTypeTable); Index++) {
    if ((mEventTypeTable[Index].MagicId == LogEntry->MagicId) &&
        (mEventTypeTable[Index].SocketId == LogEntry->SocketId))
    {
      mEventTypeTable[Index].Instance++;
      break;
    }
  }

  if (Index >= ARRAY_SIZE (mEventTypeTable)) {
    return EFI_NOT_FOUND;
  }

  *EventType = mEventTypeTable[Index].EventType;
  DataPtr    = EventData;

  //
  // Calculate event size based on type
  //
  switch (*EventType) {
    case EV_TABLE_OF_DEVICES:
      // Event is just an ASCII string
      // Copy the event string over with Instance number substituted
      EventStrLen = AsciiSPrint (
                      (CHAR8 *)DataPtr,
                      *EventSize,
                      mEventTypeTable[Index].EventStr,
                      mEventTypeTable[Index].Instance
                      );
      // Event string is NOT null-terminated
      NewEventSize = EventStrLen;
      break;

    case EV_POST_CODE:
      // Event is a UEFI_PLATFORM_FIRMWARE_BLOB2
      // Copy the event string to BlobDescription with Instance number substituted
      EventStrLen = AsciiSPrint (
                      (CHAR8 *)DataPtr + SIZE_OF_BLOB_DESCRIPTION_SIZE,
                      *EventSize,
                      mEventTypeTable[Index].EventStr,
                      mEventTypeTable[Index].Instance
                      );

      // BlobDescriptionSize
      *DataPtr = EventStrLen;
      DataPtr += SIZE_OF_BLOB_DESCRIPTION_SIZE + EventStrLen;

      // BlobBase
      WriteUnaligned64 ((UINT64 *)DataPtr, 0);
      DataPtr += SIZE_OF_BLOB_BASE;

      // BlobLength
      WriteUnaligned64 ((UINT64 *)DataPtr, 0);

      NewEventSize = SIZE_OF_BLOB_DESCRIPTION_SIZE +
                     EventStrLen +
                     SIZE_OF_BLOB_BASE +
                     SIZE_OF_BLOB_LENGTH;
      break;

    default:
      ASSERT (FALSE);
      return EFI_UNSUPPORTED;
  }

  if (NewEventSize >= *EventSize) {
    return EFI_BUFFER_TOO_SMALL;
  }

  *EventSize = NewEventSize;

  return EFI_SUCCESS;
}

/**
  Register TPM Events

  This function copies and registers Pre-UEFI TPM Events into the GUID HOB list.

  @param  TpmLog             Physical address to Pre-UEFI TPM measurement data

  @retval EFI_SUCCESS        Successfully build TPM event log HOBs
  @retval other              Errors
**/
EFI_STATUS
EFIAPI
TH500BuildTcgEventHob (
  IN UINTN  TpmLogAddress
  )
{
  EFI_STATUS                    Status;
  TEGRABL_TPM_COMMIT_LOG        *TpmLog;
  TEGRABL_TPM_COMMIT_LOG_ENTRY  *LogEntry;
  TPMI_ALG_HASH                 HashAlg;
  UINT32                        DigestSize;
  UINT32                        Index;
  VOID                          *HobData;
  TCG_PCR_EVENT2                *TcgPcrEvent2;
  TCG_EVENTTYPE                 EventType;
  UINT32                        EventSize;
  UINT8                         EventData[MAX_EVENT_DATA_SIZE];
  UINT8                         *EventSizePtr;
  UINT8                         *EventPtr;

  TpmLog = (TEGRABL_TPM_COMMIT_LOG *)TpmLogAddress;

  ASSERT (TpmLog != NULL);
  ASSERT (TpmLog->NumMeasurements <= MAX_NUM_MEASUREMENTS);

  //
  // Convert PSC algorithm encoding to TCG encoding
  //
  switch (TpmLog->AlgoType) {
    case ALGO_TYPE_SHA384:
      HashAlg    = TPM_ALG_SHA384;
      DigestSize = SHA384_DIGEST_SIZE;
      break;
    case ALGO_TYPE_SHA256:
      HashAlg    = TPM_ALG_SHA256;
      DigestSize = SHA256_DIGEST_SIZE;
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a: Unrecognized AlgoType %d\n", __FUNCTION__, TpmLog->AlgoType));
      ASSERT (FALSE);
      return EFI_INVALID_PARAMETER;
  }

  //
  // For each pre-UEFI measurement:
  //
  for (Index = 0; Index < TpmLog->NumMeasurements; Index++) {
    LogEntry = &TpmLog->Measurements[Index];
    DEBUG ((DEBUG_INFO, "Import TPM Log  0x%x %d %d\n", LogEntry->MagicId, LogEntry->SocketId, LogEntry->PcrIndex));

    //
    // Sanity checks
    //
    if (LogEntry->SocketId >= TH500_MAX_SOCKETS) {
      DEBUG ((DEBUG_ERROR, "%a: Invalid SocketId %x\n", __FUNCTION__, LogEntry->SocketId));
      ASSERT (FALSE);
      return EFI_INVALID_PARAMETER;
    }

    if (LogEntry->PcrIndex > 1) {
      DEBUG ((DEBUG_ERROR, "%a: Unexpected PCR used: %d\n", __FUNCTION__, LogEntry->PcrIndex));
      ASSERT (FALSE);
      return EFI_INVALID_PARAMETER;
    }

    //
    // Convert magic_id, socket_id to TCG event format
    //
    EventSize = MAX_EVENT_DATA_SIZE;
    Status    = TH500GetEventData (LogEntry, &EventType, &EventSize, EventData);
    if (Status != EFI_SUCCESS) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to process entry %u - %r (Magic Id: %08X)\n",
        __FUNCTION__,
        Index,
        Status,
        LogEntry->MagicId
        ));
      ASSERT (FALSE);
      return Status;
    }

    //
    // Create one HOB for each TCG event
    //
    HobData = BuildGuidHob (
                &gTcgEvent2EntryHobGuid,
                sizeof (TcgPcrEvent2->PCRIndex) +
                sizeof (TcgPcrEvent2->EventType) +
                sizeof (TcgPcrEvent2->Digest.count) +
                sizeof (TcgPcrEvent2->Digest.digests[0].hashAlg) +
                DigestSize +
                sizeof (TcgPcrEvent2->EventSize) +
                EventSize
                );
    if (HobData == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Fail to build HOB for TcgEvent %u\n", __FUNCTION__, Index));
      ASSERT (FALSE);
      return EFI_OUT_OF_RESOURCES;
    }

    //
    // Copy the TCG event data to HOB
    //
    TcgPcrEvent2            = HobData;
    TcgPcrEvent2->PCRIndex  = LogEntry->PcrIndex;
    TcgPcrEvent2->EventType = EventType;

    TcgPcrEvent2->Digest.count              = 1;  // Only support one digest always
    TcgPcrEvent2->Digest.digests[0].hashAlg = HashAlg;
    CopyMem (TcgPcrEvent2->Digest.digests[0].digest.sha256, LogEntry->Digest, DigestSize);

    EventSizePtr = (UINT8 *)&TcgPcrEvent2->Digest.digests[0].digest + DigestSize;
    WriteUnaligned32 ((UINT32 *)EventSizePtr, EventSize);

    EventPtr = (UINT8 *)EventSizePtr + sizeof (TcgPcrEvent2->EventSize);
    CopyMem (EventPtr, EventData, EventSize);
  }

  return EFI_SUCCESS;
}
