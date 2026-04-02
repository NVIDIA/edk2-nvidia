/** @file

  Android Dtb Image Parser

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AndroidBootDtbImgParser.h"

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/FdtLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/NctLib.h>

#define DTB_IMAGE_MAGIC  (0xd7b7ab1e)

STATIC
EFI_STATUS
GetNctBoardInfo (
  OUT UINT32  *ProcBoardId,
  OUT UINT32  *ProcFab,
  OUT UINT32  *ProcSku
  )
{
  EFI_STATUS  Status;
  NCT_ITEM    NctItem;

  if ((ProcBoardId == NULL) || (ProcFab == NULL) || (ProcSku == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = NctReadItem (NCT_ID_BOARD_INFO, &NctItem);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read NCT board info: %r\n", __FUNCTION__, Status));
    return Status;
  }

  *ProcBoardId = NctItem.BoardInfo.ProcBoardId;
  *ProcFab     = NctItem.BoardInfo.ProcFab;
  *ProcSku     = NctItem.BoardInfo.ProcSku;

  DEBUG ((
    DEBUG_ERROR,
    "%a: NCT ProcBoardId=%u ProcFab=0x%x ProcSku=%u\n",
    __FUNCTION__,
    *ProcBoardId,
    *ProcFab,
    *ProcSku
    ));

  return EFI_SUCCESS;
}

/**
 * Match a dt_table_entry against NCT board identification.
 *
 * Fields are compared after big-endian to native byte-swap.
 * Entry fields: Id <-> ProcBoardId, Rev <-> ProcFab, Custom[0] <-> ProcSku.
 */
STATIC
BOOLEAN
MatchDtEntry (
  IN DTB_IMG_ENTRY  *Entry,
  IN UINT32         ProcBoardId,
  IN UINT32         ProcFab,
  IN UINT32         ProcSku
  )
{
  UINT32  EntryId      = SwapBytes32 (Entry->Id);
  UINT32  EntryRev     = SwapBytes32 (Entry->Rev);
  UINT32  EntryCustom0 = SwapBytes32 (Entry->Custom[0]);

  return (EntryId == ProcBoardId) &&
         (EntryRev == ProcFab) &&
         (EntryCustom0 == ProcSku);
}

/**
 * Check whether the platform is running in hypervisor mode by inspecting
 * the nvidia,tegra-hypervisor-mode property in the UEFI DTB /chosen node.
 */
STATIC
BOOLEAN
IsHypervisorMode (
  VOID
  )
{
  EFI_STATUS  Status;
  VOID        *UefiDtb;
  INT32       ChosenNode;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &UefiDtb);
  if (EFI_ERROR (Status) || (UefiDtb == NULL)) {
    return FALSE;
  }

  ChosenNode = FdtPathOffset (UefiDtb, "/chosen");
  if (ChosenNode < 0) {
    return FALSE;
  }

  return (FdtGetProperty (UefiDtb, ChosenNode, "nvidia,tegra-hypervisor-mode", NULL) != NULL);
}

STATIC
EFI_STATUS
ParseDtHeader (
  VOID    *Dtb,
  UINT32  *DtEntryOffset,
  UINT32  *DtEntryCount
  )
{
  DTB_IMG_HEADER  *DtHeader;

  if ((Dtb == NULL) || (DtEntryOffset == NULL) || (DtEntryCount == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid Parameters \r\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  DtHeader = (DTB_IMG_HEADER *)Dtb;

  // Verify this code if Google's dt_table_header handling changes.
  if (SwapBytes32 (DtHeader->Magic) != DTB_IMAGE_MAGIC) {
    DEBUG ((DEBUG_ERROR, "%a: DT Header Magic Not found %u, %u, %u \r\n", __FUNCTION__, DtHeader->Magic, DTB_IMAGE_MAGIC, SwapBytes32 (DtHeader->Magic)));
    return EFI_NOT_FOUND;
  }

  *DtEntryOffset = SwapBytes32 (DtHeader->DtEntriesOffset);
  *DtEntryCount  = SwapBytes32 (DtHeader->DtEntryCount);
  return EFI_SUCCESS;
}

EFI_STATUS
ExtractDtbfromDtbImg (
  VOID  **Dtb
  )
{
  EFI_STATUS     Status;
  UINT32         DtEntryOffset;
  UINT32         DtEntryCount;
  DTB_IMG_ENTRY  *DtbImgEntry;
  UINT32         DtbOffset = 0;
  UINT32         ProcBoardId;
  UINT32         ProcFab;
  UINT32         ProcSku;
  UINT32         Count;
  BOOLEAN        Found = FALSE;

  if ((Dtb == NULL) || (*Dtb == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid Parameters \r\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Status = ParseDtHeader (*Dtb, &DtEntryOffset, &DtEntryCount);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DtbImgEntry = (DTB_IMG_ENTRY *)((UINT8 *)(*Dtb) + DtEntryOffset);

  if (DtEntryCount == 0) {
    DEBUG ((DEBUG_ERROR, "%a: No DT entries found\r\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  if ((DtEntryCount == 1) || IsHypervisorMode ()) {
    DtbOffset = SwapBytes32 (DtbImgEntry->DtOffset);
    goto Done;
  }

  Status = GetNctBoardInfo (&ProcBoardId, &ProcFab, &ProcSku);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NCT board info unavailable, using first DTB entry\r\n", __FUNCTION__));
    DtbOffset = SwapBytes32 (DtbImgEntry->DtOffset);
    goto Done;
  }

  for (Count = 0; Count < DtEntryCount; Count++) {
    if (MatchDtEntry (&DtbImgEntry[Count], ProcBoardId, ProcFab, ProcSku)) {
      DtbOffset = SwapBytes32 (DtbImgEntry[Count].DtOffset);
      Found     = TRUE;
      DEBUG ((
        DEBUG_ERROR,
        "%a: Matched DTB entry %u (Id=%u Rev=0x%x Custom0=%u)\r\n",
        __FUNCTION__,
        Count,
        SwapBytes32 (DtbImgEntry[Count].Id),
        SwapBytes32 (DtbImgEntry[Count].Rev),
        SwapBytes32 (DtbImgEntry[Count].Custom[0])
        ));
      break;
    }
  }

  if (!Found) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: No matching DTB for ProcBoardId=%u ProcFab=0x%x ProcSku=%u\r\n",
      __FUNCTION__,
      ProcBoardId,
      ProcFab,
      ProcSku
      ));
    return EFI_NOT_FOUND;
  }

Done:
  *Dtb = (VOID *)((UINT8 *)(*Dtb) + DtbOffset);

  return EFI_SUCCESS;
}

EFI_STATUS
ExtractDtbofromDtboImg (
  IN VOID     **Dtbo,
  OUT UINT32  *DtboCount,
  OUT UINT32  *DtboStartIdx
  )
{
  EFI_STATUS     Status;
  UINT32         DtEntryOffset;
  UINT32         DtEntryCount;
  DTB_IMG_ENTRY  *DtbImgEntry;
  UINT32         Count;
  UINT32         DtboOffset = 0;
  UINT32         ProcBoardId;
  UINT32         ProcFab;
  UINT32         ProcSku;
  UINT32         FirstMatch;
  UINT32         MatchCount = 0;

  if ((Dtbo == NULL) || (*Dtbo == NULL) || (DtboCount == NULL) || (DtboStartIdx == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid Parameters \r\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Status = ParseDtHeader (*Dtbo, &DtEntryOffset, &DtEntryCount);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DtbImgEntry = (DTB_IMG_ENTRY *)((UINT8 *)(*Dtbo) + DtEntryOffset);

  if (DtEntryCount == 0) {
    DEBUG ((DEBUG_ERROR, "%a: No DT entries found\r\n", __FUNCTION__));
    *DtboCount = 0;
    return EFI_UNSUPPORTED;
  }

  if (DtEntryCount == 1) {
    DtboOffset    = SwapBytes32 (DtbImgEntry->DtOffset);
    *DtboCount    = 1;
    *DtboStartIdx = 0;
    goto Done;
  }

  if (IsHypervisorMode ()) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Hypervisor mode, using all %u DTBO entries\r\n",
      __FUNCTION__,
      DtEntryCount
      ));
    DtboOffset    = SwapBytes32 (DtbImgEntry->DtOffset);
    *DtboCount    = DtEntryCount;
    *DtboStartIdx = 0;
    goto Done;
  }

  Status = GetNctBoardInfo (&ProcBoardId, &ProcFab, &ProcSku);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: NCT board info unavailable, using all DTBO entries\r\n",
      __FUNCTION__
      ));
    DtboOffset    = SwapBytes32 (DtbImgEntry->DtOffset);
    *DtboCount    = DtEntryCount;
    *DtboStartIdx = 0;
    goto Done;
  }

  FirstMatch = DtEntryCount;
  for (Count = 0; Count < DtEntryCount; Count++) {
    if (MatchDtEntry (&DtbImgEntry[Count], ProcBoardId, ProcFab, ProcSku)) {
      if (!MatchCount) {
        FirstMatch = Count;
      }

      MatchCount++;
      DEBUG ((
        DEBUG_ERROR,
        "%a: Matched DTBO entry %u (Id=%u Rev=0x%x Custom0=%u)\r\n",
        __FUNCTION__,
        Count,
        SwapBytes32 (DtbImgEntry[Count].Id),
        SwapBytes32 (DtbImgEntry[Count].Rev),
        SwapBytes32 (DtbImgEntry[Count].Custom[0])
        ));
    } else if (MatchCount) {
      break;
    }
  }

  if (!MatchCount) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: No matching DTBO entries for ProcBoardId=%u ProcFab=0x%x ProcSku=%u\r\n",
      __FUNCTION__,
      ProcBoardId,
      ProcFab,
      ProcSku
      ));
    *DtboCount    = 0;
    *DtboStartIdx = 0;
    return EFI_NOT_FOUND;
  }

  DtboOffset    = SwapBytes32 (DtbImgEntry[FirstMatch].DtOffset);
  *DtboCount    = MatchCount;
  *DtboStartIdx = FirstMatch;

Done:
  *Dtbo = (VOID *)((UINT8 *)(*Dtbo) + DtboOffset);

  return EFI_SUCCESS;
}
