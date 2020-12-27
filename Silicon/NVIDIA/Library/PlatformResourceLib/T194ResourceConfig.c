/** @file
*
*  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
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

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>
#include <Library/DramCarveoutLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/GoldenRegisterLib.h>
#include "T194ResourceConfigPrivate.h"
#include "T194ResourceConfig.h"

/**
  Installs resources into the HOB list

  This function install all memory regions into the HOB list.
  This function is called by the platform memory initialization library.

  @param  NumberOfMemoryRegions Number of regions installed into HOB list.

  @retval EFI_SUCCESS           Resources have been installed
  @retval EFI_DEVICE_ERROR      Error setting up memory

**/
EFI_STATUS
T194ResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
  )
{
  TEGRA_CPUBL_PARAMS   *CpuBootloaderParams;
  NVDA_MEMORY_REGION   *CarveoutRegions;
  UINTN                CarveoutRegionsCount=0;
  UINTN                Index;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  PlatformInfo->SdramSize = CpuBootloaderParams->SdramSize;
  PlatformInfo->DtbLoadAddress = CpuBootloaderParams->BlDtbLoadAddress;

  //Build Carveout regions
  CarveoutRegions = (NVDA_MEMORY_REGION *)AllocatePool (sizeof (NVDA_MEMORY_REGION) * (CARVEOUT_NUM));
  ASSERT (CarveoutRegions != NULL);
  if (CarveoutRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  for (Index = CARVEOUT_NONE; Index < CARVEOUT_NUM; Index++) {
    if (Index == CARVEOUT_MISC) {
      //Leave in memory map but marked as used
      BuildMemoryAllocationHob (
        CpuBootloaderParams->CarveoutInfo[Index].Base,
        EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CpuBootloaderParams->CarveoutInfo[Index].Size)),
        (ValidateGrBlobHeader(GetGRBlobBaseAddress ()) == EFI_SUCCESS) ? EfiReservedMemoryType : EfiBootServicesData
      );
    } else if ((Index != CARVEOUT_CPUBL) &&
               (Index != CARVEOUT_OS) &&
               (Index != CARVEOUT_MB2) &&
               (Index != CARVEOUT_RCM_BLOB) &&
               (CpuBootloaderParams->CarveoutInfo[Index].Size != 0)) {
      CarveoutRegions[CarveoutRegionsCount].MemoryBaseAddress = CpuBootloaderParams->CarveoutInfo[Index].Base;
      CarveoutRegions[CarveoutRegionsCount].MemoryLength      = CpuBootloaderParams->CarveoutInfo[Index].Size;
      CarveoutRegionsCount++;
    }
  }

  PlatformInfo->CarveoutRegions = CarveoutRegions;
  PlatformInfo->CarveoutRegionsCount = CarveoutRegionsCount;

  return EFI_SUCCESS;
}

/**
  Retrieve DTB Address

**/
UINT64
T194GetDTBBaseAddress (
  IN UINTN CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS   *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  return CpuBootloaderParams->BlDtbLoadAddress;
}

/**
  Retrieve RCM Blob Address

**/
UINT64
T194GetRCMBaseAddress (
  IN UINTN CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS   *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  return CpuBootloaderParams->CarveoutInfo[CARVEOUT_RCM_BLOB].Base;
}

/**
  Retrieve Boot Type

**/
TEGRA_BOOT_TYPE
T194GetBootType (
  IN UINTN CpuBootloaderAddress
  )
{
  return TegrablBootColdBoot;
}

/**
  Retrieve GR Blob Address

**/
UINT64
T194GetGRBlobBaseAddress (
  IN UINTN CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS         *CpuBootloaderParams;
  UINT64                     MemoryBase;
  UINT64                     MemorySize;
  EFI_FIRMWARE_VOLUME_HEADER *FvHeader;
  UINT64                     FvOffset;
  UINT64                     FvSize;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  MemoryBase = CpuBootloaderParams->CarveoutInfo[CARVEOUT_CPUBL].Base;
  MemorySize = CpuBootloaderParams->CarveoutInfo[CARVEOUT_CPUBL].Size;
  FvOffset = 0;

  while (FvOffset < MemorySize) {
    FvHeader = (EFI_FIRMWARE_VOLUME_HEADER *)(VOID *)(MemoryBase + FvOffset);
    if (FvHeader->Signature == EFI_FVH_SIGNATURE) {
      break;
    }
    FvOffset += SIZE_64KB;
  }
  ASSERT (FvOffset < MemorySize);
  FvSize = FvHeader->FvLength;
  // Make UEFI FV size aligned to 64KB.
  FvSize = ALIGN_VALUE (FvSize, SIZE_64KB);

  return (UINT64)FvHeader + FvSize;
}

/**
  Retrieve GR Output Base and Size

**/
BOOLEAN
T194GetGROutputBaseAndSize (
  IN  UINTN CpuBootloaderAddress,
  OUT UINTN *Base,
  OUT UINTN *Size
  )
{
  TEGRA_CPUBL_PARAMS *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  *Base = CpuBootloaderParams->GoldenRegisterAddress;
  *Size = CpuBootloaderParams->GoldenRegisterSize;

  return TRUE;
}
