/** @file
Misc Library for OPTEE related functions in Standalone MM.

SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <PiMm.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/MmServicesTableLib.h>

#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/ArmSvcLib.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Library/BaseLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

EFIAPI
BOOLEAN
IsOpteePresent (
  VOID
  )
{
  return (FeaturePcdGet (PcdOpteePresent));
}

EFIAPI
EFI_STATUS
GetDeviceRegion (
  IN CHAR8                 *Name,
  OUT EFI_VIRTUAL_ADDRESS  *DeviceBase,
  OUT UINTN                *DeviceRegionSize
  )
{
  EFI_STATUS            Status           = EFI_NOT_FOUND;
  EFI_MM_DEVICE_REGION  *DeviceRegionMap = NULL;
  UINTN                 Index;
  EFI_HOB_GUID_TYPE     *GuidHob;

  GuidHob = GetFirstGuidHob (&gEfiStandaloneMmDeviceMemoryRegions);
  if (GuidHob == NULL) {
    return Status;
  }

  DeviceRegionMap = GET_GUID_HOB_DATA (GuidHob);
  for (Index = 0; Index < MAX_DEVICE_REGIONS; Index++) {
    if (AsciiStrCmp (Name, DeviceRegionMap[Index].DeviceRegionName) == 0) {
      *DeviceBase       = DeviceRegionMap[Index].DeviceRegionStart;
      *DeviceRegionSize = DeviceRegionMap[Index].DeviceRegionSize;
      Status            = EFI_SUCCESS;
      break;
    }
  }

  return Status;
}

EFIAPI
BOOLEAN
IsDeviceTypePresent (
  CONST CHAR8  *DeviceType,
  UINT32       *NumRegions   OPTIONAL
  )
{
  EFI_MM_DEVICE_REGION  *DeviceRegionMap = NULL;
  UINTN                 Index;
  EFI_HOB_GUID_TYPE     *GuidHob;
  BOOLEAN               DeviceTypePresent = FALSE;
  UINT32                NumDevices;

  GuidHob = GetFirstGuidHob (&gEfiStandaloneMmDeviceMemoryRegions);
  NV_ASSERT_RETURN (
    GuidHob != NULL,
    return DeviceTypePresent,
    "%a: Unable to find HOB for gEfiStandaloneMmDeviceMemoryRegions\n",
    __FUNCTION__
    );

  DeviceRegionMap = GET_GUID_HOB_DATA (GuidHob);
  NumDevices      = 0;
  for (Index = 0; Index < MAX_DEVICE_REGIONS; Index++) {
    if (AsciiStrStr (DeviceRegionMap[Index].DeviceRegionName, DeviceType) != NULL) {
      DeviceTypePresent = TRUE;
      NumDevices++;
    }
  }

  if (NumRegions != NULL) {
    *NumRegions = NumDevices;
  }

  return DeviceTypePresent;
}

EFIAPI
BOOLEAN
IsQspiPresent (
  VOID
  )
{
  EFI_STATUS            Status           = EFI_NOT_FOUND;
  EFI_MM_DEVICE_REGION  *DeviceRegionMap = NULL;
  UINTN                 Index;
  EFI_HOB_GUID_TYPE     *GuidHob;
  BOOLEAN               QspiPresent = FALSE;

  GuidHob = GetFirstGuidHob (&gEfiStandaloneMmDeviceMemoryRegions);
  if (GuidHob == NULL) {
    return Status;
  }

  DeviceRegionMap = GET_GUID_HOB_DATA (GuidHob);

  for (Index = 0; Index < MAX_DEVICE_REGIONS; Index++) {
    if (AsciiStrStr (DeviceRegionMap[Index].DeviceRegionName, "qspi") != NULL) {
      QspiPresent = TRUE;
      break;
    }
  }

  return QspiPresent;
}

/**
  * GetDeviceTypeRegions
  * Get all the MMIO regions for a device type across all the sockets.
  *
  * @param[in]   DeviceType    Device type substring.
  * @param[out]  DeviceRegions Allocated region of device regions on success.
  * @param[out]  NumRegions    Number of device regions.
  *
  * @retval  EFI_SUCCESS           Found device type regions.
  *          EFI_NOT_FOUND         Device Memory HOB not found or no device
  *                                regions installed in the HOB.
  *          EFI_OUT_OF_RESOURCES  Failed to allocate the DeviceRegions Buffer.
 **/
EFIAPI
EFI_STATUS
GetDeviceTypeRegions (
  CONST CHAR8           *DeviceType,
  EFI_MM_DEVICE_REGION  **DeviceRegions,
  UINT32                *NumRegions
  )
{
  EFI_MM_DEVICE_REGION  *DeviceMmio;
  UINT32                NumDevices;
  EFI_STATUS            Status;
  UINTN                 Index;
  EFI_HOB_GUID_TYPE     *GuidHob;
  EFI_MM_DEVICE_REGION  *DeviceRegionMap = NULL;
  UINTN                 DeviceIndex;

  Status = EFI_SUCCESS;
  if (IsDeviceTypePresent (DeviceType, &NumDevices) == FALSE) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: There are no %a regions present\n",
      __FUNCTION__,
      DeviceType
      ));
    Status = EFI_NOT_FOUND;
    goto ExitGetDeviceTypeRegions;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: %u %a regions present\n",
    __FUNCTION__,
    NumDevices,
    DeviceType
    ));
  DeviceMmio = AllocateRuntimeZeroPool (sizeof (EFI_MM_DEVICE_REGION) * NumDevices);
  if (DeviceMmio == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate %u bytes\n",
      __FUNCTION__,
      (NumDevices * sizeof (EFI_MM_DEVICE_REGION))
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitGetDeviceTypeRegions;
  }

  GuidHob = GetFirstGuidHob (&gEfiStandaloneMmDeviceMemoryRegions);
  if (GuidHob == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to lookup Device Memory Hob",
      __FUNCTION__
      ));
    Status = EFI_NOT_FOUND;
    FreePool (DeviceMmio);
    goto ExitGetDeviceTypeRegions;
  }

  DeviceRegionMap = GET_GUID_HOB_DATA (GuidHob);
  DeviceIndex     = 0;
  for (DeviceIndex = 0, Index = 0;
       (Index < MAX_DEVICE_REGIONS) && (DeviceIndex < NumDevices);
       Index++)
  {
    if (AsciiStrStr (DeviceRegionMap[Index].DeviceRegionName, DeviceType) != NULL) {
      CopyMem (
        &DeviceMmio[DeviceIndex++],
        &DeviceRegionMap[Index],
        sizeof (EFI_MM_DEVICE_REGION)
        );
    }
  }

  *DeviceRegions = DeviceMmio;
  *NumRegions    = NumDevices;
ExitGetDeviceTypeRegions:
  return Status;
}

EFIAPI
EFI_STATUS
GetQspiDeviceRegion (
  UINT64  *QspiBaseAddress,
  UINTN   *QspiRegionSize
  )
{
  EFI_STATUS  Status = EFI_UNSUPPORTED;

  // OP-TEE path
  if (IsOpteePresent ()) {
    Status = GetDeviceRegion ("qspi0-t194", QspiBaseAddress, QspiRegionSize);
    if (EFI_ERROR (Status)) {
      Status = GetDeviceRegion ("qspi0-t234", QspiBaseAddress, QspiRegionSize);
      if (EFI_ERROR (Status)) {
        Status = EFI_NOT_FOUND;
      }
    }
  }

  return Status;
}

BOOLEAN
EFIAPI
IsT234 (
  VOID
  )
{
  return (IsOpteePresent () && IsDeviceTypePresent ("-t234", NULL));
}

UINT32
EFIAPI
StmmGetBootChainForGpt (
  VOID
  )
{
  UINT32                BootChain = 0;
  EFI_MM_DEVICE_REGION  *ScratchRegions;
  UINT32                NumRegions;
  EFI_STATUS            Status;

  if (IsT234 ()) {
    Status = GetDeviceTypeRegions ("scratch-t234", &ScratchRegions, &NumRegions);
    NV_ASSERT_RETURN ((!EFI_ERROR (Status) && NumRegions == 1), return BootChain, "%a: failed to get scratch region: %r\n", __FUNCTION__, Status);

    Status = GetActiveBootChainStMm (T234_CHIP_ID, ScratchRegions[0].DeviceRegionStart, &BootChain);
    ASSERT_EFI_ERROR (Status);
  }

  return BootChain;
}
