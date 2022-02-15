/** @file
*
*  SmbiosData.h
*
*  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __SMBIOS_DATA__
#define __SMBIOS_DATA__

#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Protocol/Eeprom.h>
#include <Library/OemMiscLib.h>
#include <ConfigurationManagerObject.h>

typedef struct {
  CHAR16  *BoardSku;
  CHAR16  *BoardSerialNumber;
  CHAR16  *BoardVersion;
  CHAR16  *BoardAssetTag;
  CHAR16  *BoardProductName;
} SmbiosMiscData;

typedef struct {
  PROCESSOR_CHARACTERISTIC_FLAGS CpuCapability;
  OEM_MISC_PROCESSOR_DATA        CpuData;
  UINT8                          NumCacheLevels;
  CM_ARM_CACHE_INFO              *CacheData;
} SmbiosCpuData;

#endif
