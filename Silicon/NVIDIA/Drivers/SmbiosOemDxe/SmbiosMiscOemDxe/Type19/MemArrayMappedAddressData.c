/** @file
*
*  SMBIOS Type19 input template data.
*
*  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#include "SmbiosMiscOem.h"

SMBIOS_MISC_TABLE_DATA (SMBIOS_TABLE_TYPE19, MiscMemArrayMap) = {
  {                                                     // Hdr
    EFI_SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS,        // Type,
    0,                                                  // Length,
    0,                                                  // Handle
  },
  0x0,                                                  // Starting Address
  0x0,                                                  // Ending Address
  0,                                                    // Phys Mem Array Handle
  1,                                                    // Partition Width
  0x0,                                                  // Extended Starting Address
  0x0                                                   // Extended Ending Address
};
