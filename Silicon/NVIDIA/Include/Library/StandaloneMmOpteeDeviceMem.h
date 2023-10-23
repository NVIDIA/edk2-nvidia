/** @file

SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef STANDLONEMM_OPTEE_DEVICE_MEM_H
#define STANDLONEMM_OPTEE_DEVICE_MEM_H

#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>

#define DEVICE_REGION_NAME_MAX_LEN  32
#define MAX_DEVICE_REGIONS          10
#define OPTEE_OS_UID0               0x384fb3e0
#define OPTEE_OS_UID1               0xe7f811e3
#define OPTEE_OS_UID2               0xaf630002
#define OPTEE_OS_UID3               0xa5d5c51b

typedef struct _EFI_MM_DEVICE_REGION {
  EFI_VIRTUAL_ADDRESS    DeviceRegionStart;
  UINT32                 DeviceRegionSize;
  CHAR8                  DeviceRegionName[DEVICE_REGION_NAME_MAX_LEN];
} EFI_MM_DEVICE_REGION;

EFIAPI
EFI_STATUS
GetDeviceRegion (
  IN CHAR8                 *Name,
  OUT EFI_VIRTUAL_ADDRESS  *DeviceBase,
  OUT UINTN                *DeviceRegionSize
  );

EFIAPI
BOOLEAN
IsOpteePresent (
  VOID
  );

EFIAPI
BOOLEAN
IsQspiPresent (
  VOID
  );

EFIAPI
EFI_STATUS
GetQspiDeviceRegion (
  UINT64  *QspiBaseAddress,
  UINTN   *QspiRegionSize
  );

/**
 * Check if system is T234
 *
 * @retval    TRUE    System is T234
 *            FALSE   System is not T234
 **/
BOOLEAN
EFIAPI
IsT234 (
  VOID
  );

/**
 * Get boot chain value to use for GPT location.  If system does not
 * support per-boot-chain GPT, 0 is returned.
 *
 * @retval UINT32     Boot chain value to use for GPT location
 *
 **/
UINT32
EFIAPI
StmmGetBootChainForGpt (
  VOID
  );

#endif //STANDALONEMM_OPTEE_DEVICE_MEM_H
