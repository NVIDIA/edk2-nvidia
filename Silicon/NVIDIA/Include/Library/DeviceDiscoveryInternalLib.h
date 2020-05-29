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

#ifndef __DEVICE_DISCOVERY_INTERNAL_LIB_H__
#define __DEVICE_DISCOVERY_INTERNAL_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Library/TegraPlatformInfoLib.h>

typedef struct {
  CONST CHAR8            *Compatibility;
} NVIDIA_COMPATIBILITY_INTERNAL;

/**
  Retrieve a pointer to the array of the Compatible props to override

**/
EFI_STATUS
EFIAPI
GetDeviceDiscoveryCompatibleInternal (
  OUT NVIDIA_COMPATIBILITY_INTERNAL   **CompatibleInternal
);

#endif //__DEVICE_DISCOVERY_INTERNAL_LIB_H__
