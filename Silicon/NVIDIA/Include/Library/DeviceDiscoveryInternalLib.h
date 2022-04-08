/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
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
