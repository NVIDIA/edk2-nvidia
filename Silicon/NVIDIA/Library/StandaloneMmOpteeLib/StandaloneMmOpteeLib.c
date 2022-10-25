/** @file
Misc Library for OPTEE related functions in Standalone MM.

Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#define HIDREV_OFFSET             0x4
#define HIDREV_PRE_SI_PLAT_SHIFT  0x14
#define HIDREV_PRE_SI_PLAT_MASK   0xf

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
  } else {
    Status = GetDeviceRegion ("qspi0", QspiBaseAddress, QspiRegionSize);
    if (EFI_ERROR (Status)) {
      Status = EFI_NOT_FOUND;
    }
  }

  return Status;
}

EFIAPI
TEGRA_PLATFORM_TYPE
GetPlatformTypeMm (
  VOID
  )
{
  TEGRA_PLATFORM_TYPE  PlatformType;
  UINT64               MiscAddress;
  UINTN                MiscRegionSize;
  EFI_STATUS           Status;
  UINT32               HidRev;

  Status = GetDeviceRegion ("tegra-misc", &MiscAddress, &MiscRegionSize);
  if (EFI_ERROR (Status)) {
    PlatformType = TEGRA_PLATFORM_UNKNOWN;
  } else {
    HidRev       = MmioRead32 (MiscAddress + HIDREV_OFFSET);
    PlatformType = ((HidRev >> HIDREV_PRE_SI_PLAT_SHIFT) & HIDREV_PRE_SI_PLAT_MASK);
    if (PlatformType >= TEGRA_PLATFORM_UNKNOWN) {
      PlatformType =  TEGRA_PLATFORM_UNKNOWN;
    }
  }

  return PlatformType;
}

EFIAPI
BOOLEAN
InFbc (
  VOID
  )
{
  EFI_HOB_GUID_TYPE  *GuidHob;
  STMM_COMM_BUFFERS  *StmmCommBuffers;
  BOOLEAN            Fbc;

  GuidHob = GetFirstGuidHob (&gNVIDIAStMMBuffersGuid);
  if (GuidHob == NULL) {
    if (IsOpteePresent ()) {
      Fbc = TRUE;
      goto ExitInFbc;
    } else {
      ASSERT_EFI_ERROR (EFI_NOT_FOUND);
    }
  }

  StmmCommBuffers = (STMM_COMM_BUFFERS *)GET_GUID_HOB_DATA (GuidHob);
  Fbc             = StmmCommBuffers->Fbc;
ExitInFbc:
  return Fbc;
}

EFIAPI
TEGRA_BOOT_TYPE
GetBootType (
  VOID
  )
{
  EFI_HOB_GUID_TYPE             *GuidHob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  TEGRA_BOOT_TYPE               BootType;

  GuidHob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if (GuidHob == NULL) {
    if (IsOpteePresent ()) {
      BootType = TegrablBootInvalid;
      goto ExitBootType;
    } else {
      ASSERT_EFI_ERROR (EFI_NOT_FOUND);
    }
  }

  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (GuidHob);
  BootType             = PlatformResourceInfo->BootType;
ExitBootType:
  return BootType;
}
