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

#ifndef __T186_RESOURCE_CONFIG_H__
#define __T186_RESOURCE_CONFIG_H__

#include <Library/PlatformResourceLib.h>

EFI_STATUS
T186ResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
);

UINT64
T186GetDTBBaseAddress (
  IN UINTN CpuBootloaderAddress
);

#endif //__T186_RESOURCE_CONFIG_H__
