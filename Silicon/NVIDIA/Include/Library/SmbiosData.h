/** @file
*
*  SmbiosData.h
*
*  Copyright (c) 2021 NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

 * Portions provided under the following terms:
 * Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.

 * SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
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
} SmbiosMiscData;

typedef struct {
  PROCESSOR_CHARACTERISTIC_FLAGS CpuCapability;
  OEM_MISC_PROCESSOR_DATA        CpuData;
  UINT8                          NumCacheLevels;
  CM_ARM_CACHE_INFO              *CacheData;
} SmbiosCpuData;

#endif
