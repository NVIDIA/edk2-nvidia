/** @file

  Android Dtb Image Parser

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AndroidBootDtbImgParser.h"

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/FdtLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define DTB_IMAGE_MAGIC  (0xd7b7ab1e)

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

// Parse Dt_header
// If number of entries == 1, assume that it's the correct DT and pass back.
// If multiple entries, not supported until having identification mechanism.
// if both are available, match both with dt_entry
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
  } else if (DtEntryCount == 1) {
    DtbOffset = SwapBytes32 (DtbImgEntry->DtOffset);
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Multiple dtbs not supported\r\n", __FUNCTION__));
    // TODO: need to add platform identification if multiple dtbs found.
    return EFI_NOT_FOUND;
  }

  *Dtb = (VOID *)((UINT8 *)(*Dtb) + DtbOffset);

  return EFI_SUCCESS;
}

EFI_STATUS
ExtractDtbofromDtboImg (
  IN VOID     **Dtbo,
  OUT UINT32  *DtboCount
  )
{
  EFI_STATUS     Status;
  UINT32         DtEntryOffset;
  UINT32         DtEntryCount;
  DTB_IMG_ENTRY  *DtbImgEntry;
  UINT32         Count;
  UINT32         DtboOffset = 0;

  if ((Dtbo == NULL) || (*Dtbo == NULL) || (DtboCount == NULL)) {
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
  } else if (DtEntryCount == 1) {
    DtboOffset = SwapBytes32 (DtbImgEntry->DtOffset);
    *DtboCount = 1;
  } else {
    DtboOffset = 0;
    *DtboCount = 0;

    for (Count = 0; Count < DtEntryCount; Count++, DtbImgEntry += 1) {
      // TODO: In Native, need to add platform identification if multiple dtbos found.
      //       In Hypervisor, assume all dtbos are needed in dtbo.img
      if (*DtboCount == 0) {
        DtboOffset = SwapBytes32 (DtbImgEntry->DtOffset);
      }

      *DtboCount += 1;
    }
  }

  *Dtbo = (VOID *)((UINT8 *)(*Dtbo) + DtboOffset);

  return EFI_SUCCESS;
}
