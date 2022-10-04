/** @file
*
*  AML generation protocol implementation.
*
*  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#include "SmbiosMiscOem.h"

SMBIOS_MISC_TABLE_DATA (SMBIOS_TABLE_TYPE16, MiscPhysMemArray) = {
  {                                                     // Hdr
    EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY,              // Type,
    0,                                                  // Length,
    0,                                                  // Handle
  },
  MemoryArrayLocationUnknown,
  MemoryArrayUseSystemMemory,
  MemoryErrorCorrectionUnknown,
  0x80000000,                                        // Always use the Extended capacity field.
  0xFFFE,                                            // Default to information not provided.
  0,
  0,
};
