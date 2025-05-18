/** @file
  Android Boot Config Driver

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ANDROID_BOOTDTBIMGPARSER_H__
#define __ANDROID_BOOTDTBIMGPARSER_H__

#include <Uefi.h>

/*
 * @brief Android DTB
 *
 * @param magic DT table magic value
 * @param total_size size(dt_table_header + all dt_table_entry + all dtbs)
 * @param header_size sizeof(dt_table_header)
 * @param dt_entry_size sizeof(dt_table_entry)
 * @param dt_entry_count number of dt_table_entry
 * @param dt_entries_offset offset to first dt_table_entry from dt_table_header
 * @param page_size assumed flash page size
 * @param version DTB image version
 */
typedef struct {
  UINT32    Magic;
  UINT32    TotalSize;
  UINT32    HeaderSize;
  UINT32    DtEntrySize;
  UINT32    DtEntryCount;
  UINT32    DtEntriesOffset;
  UINT32    PageSize;
  UINT32    Version;
} DTB_IMG_HEADER;

/*
 * @brief Android DTB image entry
 *
 * @param dt_size size of the DTB
 * @param dt_offset offset to the DTB from dt_table_header
 * @param id Nvidia processor board ID
 * @param rev Nvidia processor Fab
 * @param custom[0] Nvidia processor Sku
 * @param custom[1:3] Unused
 */
typedef struct {
  UINT32    DtSize;
  UINT32    DtOffset;
  UINT32    Id;
  UINT32    Rev;
  UINT32    Custom[4];
} DTB_IMG_ENTRY;

/*
 * Extract out the dtb(s) from dtb image.
 *
 * @param Dtb pointer to the dtb partition.
 * @return EFI_SUCCESS if success, else if not.
 */
EFI_STATUS
ExtractDtbfromDtbImg (
  VOID  **Dtb
  );

/*
 * Extract out the dtbo(s) from dtbo image.
 *
 * @param Dtbo pointer to the dtbo partition.
 * @param DtboCount record the number of dtbo.
 * @return EFI_SUCCESS if success, else if not.
 */
EFI_STATUS
ExtractDtbofromDtboImg (
  VOID    **Dtbo,
  UINT32  *DtboCount
  );

#endif /* __ANDROID_BOOTDTBIMGPARSER_H__ */
