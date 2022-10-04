/** @file

Copyright (c) 2022, NVIDIA Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef STANDLONEMM_OPTEE_DEVICE_MEM_H
#define STANDLONEMM_OPTEE_DEVICE_MEM_H

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

#endif //STANDALONEMM_OPTEE_DEVICE_MEM_H
