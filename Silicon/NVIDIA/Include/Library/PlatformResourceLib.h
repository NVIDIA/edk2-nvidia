/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __PLATFORM_RESOURCE_LIB_H__
#define __PLATFORM_RESOURCE_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Library/DramCarveoutLib.h>

typedef struct {
  NVDA_MEMORY_REGION   *CarveoutRegions;
  UINTN                CarveoutRegionsCount;
  UINTN                SdramSize;
  UINTN                DtbLoadAddress;
} TEGRA_RESOURCE_INFO;

/**
  Retrieve CPU BL Address

**/
UINTN
EFIAPI
GetCPUBLBaseAddress (
  VOID
);

/**
  Retrieve DTB Address

**/
UINT64
EFIAPI
GetDTBBaseAddress (
  VOID
);

/**
  Retrieve Resource Config

**/
EFI_STATUS
EFIAPI
GetResourceConfig (
  OUT TEGRA_RESOURCE_INFO *PlatformInfo
);

#endif //__PLATFORM_RESOURCE_LIB_H__
