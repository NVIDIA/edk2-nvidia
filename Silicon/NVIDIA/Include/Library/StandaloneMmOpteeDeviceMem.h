/** @file

SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef STANDLONEMM_OPTEE_DEVICE_MEM_H
#define STANDLONEMM_OPTEE_DEVICE_MEM_H

#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Protocol/NorFlash.h>

#define DEVICE_REGION_NAME_MAX_LEN  32
#define MAX_DEVICE_REGIONS          10
#define OPTEE_OS_UID0               0x384fb3e0
#define OPTEE_OS_UID1               0xe7f811e3
#define OPTEE_OS_UID2               0xaf630002
#define OPTEE_OS_UID3               0xa5d5c51b

typedef struct _NVIDIA_VAR_INT_PROTOCOL NVIDIA_VAR_INT_PROTOCOL;

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

typedef
EFI_STATUS
(EFIAPI *VAR_INT_COMPUTE_MEASUREMENT)(
  IN  NVIDIA_VAR_INT_PROTOCOL     *This,
  IN  CHAR16                      *VariableName,
  IN  EFI_GUID                    *VendorGuid,
  IN  UINT32                      Attributes,
  IN  VOID                        *Data,
  IN  UINTN                       Size
  );

typedef
EFI_STATUS
(EFIAPI *VAR_INT_FUNCTION)(
  IN NVIDIA_VAR_INT_PROTOCOL    *This
  );

typedef
EFI_STATUS
(EFIAPI *VAR_INVALIDATE_FUNCTION)(
  IN  NVIDIA_VAR_INT_PROTOCOL   *This,
  IN  CHAR16                    *VariableName,
  IN  EFI_GUID                  *VendorGuid,
  IN  EFI_STATUS                PreviousResult
  );

struct _NVIDIA_VAR_INT_PROTOCOL {
  VAR_INT_COMPUTE_MEASUREMENT    ComputeNewMeasurement;
  VAR_INT_FUNCTION               WriteNewMeasurement;
  VAR_INVALIDATE_FUNCTION        InvalidateLast;
  VAR_INT_FUNCTION               Validate;
  UINT64                         PartitionByteOffset;
  UINT64                         PartitionSize;
  NVIDIA_NOR_FLASH_PROTOCOL      *NorFlashProtocol;
  UINT64                         BlockSize;
  UINT8                          *CurMeasurement;
  UINT32                         MeasurementSize;
};

#endif //STANDALONEMM_OPTEE_DEVICE_MEM_H
