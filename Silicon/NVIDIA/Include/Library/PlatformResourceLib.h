/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
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

typedef enum {
  TegrablBootInvalid,
  TegrablBootColdBoot,
  TegrablBootRcm,
  TegrablBootTypeMax,
} TEGRA_BOOT_TYPE;

typedef struct {
  NVDA_MEMORY_REGION   *CarveoutRegions;
  UINTN                CarveoutRegionsCount;
  UINTN                SdramSize;
  UINTN                DtbLoadAddress;
} TEGRA_RESOURCE_INFO;

/**
  Retrieve Tegra UART Base Address

**/
UINTN
EFIAPI
GetTegraUARTBaseAddress (
  VOID
);

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
  Retrieve RCM Blob Address

**/
UINT64
EFIAPI
GetRCMBaseAddress (
  VOID
);

/**
  Retrieve Boot Type

**/
TEGRA_BOOT_TYPE
EFIAPI
GetBootType (
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

/**
  Retrieve GR Blob Address

**/
UINT64
EFIAPI
GetGRBlobBaseAddress (
  VOID
);

/**
  Retrieve GR Output Base and Size

**/
BOOLEAN
EFIAPI
GetGROutputBaseAndSize (
  OUT UINTN *Base,
  OUT UINTN *Size
);

/**
  Retrieve FSI NS Base and Size

**/
BOOLEAN
EFIAPI
GetFsiNsBaseAndSize (
  OUT UINTN *Base,
  OUT UINTN *Size
);

#endif //__PLATFORM_RESOURCE_LIB_H__
