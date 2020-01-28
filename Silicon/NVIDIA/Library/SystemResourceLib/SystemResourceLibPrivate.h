/** @file
*
*  Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#ifndef __SYSTEM_RESOURCE_LIB_PRIVATE_H__
#define __SYSTEM_RESOURCE_LIB_PRIVATE_H__

#include <Uefi/UefiBaseType.h>
#include <Library/DramCarveoutLib.h>

typedef struct {
  NVDA_MEMORY_REGION   *CarveoutRegions;
  UINTN                CarveoutRegionsCount;
  UINTN                SdramSize;
  UINTN                DtbLoadAddress;
} TEGRA_RESOURCE_INFO;

#endif //__SYSTEM_RESOURCE_LIB_PRIVATE_H__
