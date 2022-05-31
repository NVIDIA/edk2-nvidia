/** @file
  This file provides SMBIOS Misc Type.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>
  Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2015, Hisilicon Limited. All rights reserved.<BR>
  Copyright (c) 2015, Linaro Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SmbiosMiscOem.h"

SMBIOS_MISC_TABLE_EXTERNS (
  SMBIOS_TABLE_TYPE16,
  MiscPhysMemArray,
  MiscPhysMemArray
  )

SMBIOS_MISC_DATA_TABLE mSmbiosMiscOemDataTable[] = {
  // Typ16
  SMBIOS_MISC_TABLE_ENTRY_DATA_AND_FUNCTION (
    MiscPhysMemArray,
    MiscPhysMemArray
    ),
};

//
// Number of Data Table entries.
//
UINTN  mSmbiosMiscOemDataTableEntries =
  (sizeof (mSmbiosMiscOemDataTable)) / sizeof (SMBIOS_MISC_DATA_TABLE);
