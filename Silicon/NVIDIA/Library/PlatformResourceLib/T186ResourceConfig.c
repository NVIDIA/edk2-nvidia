/** @file
*
*  Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
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
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include "T186ResourceConfigPrivate.h"
#include "T186ResourceConfig.h"

/**
  Installs resources into the HOB list

  This function install all memory regions into the HOB list.
  This function is called by the platform memory initialization library.

  @param  NumberOfMemoryRegions Number of regions installed into HOB list.

  @retval EFI_SUCCESS           Resources have been installed
  @retval EFI_DEVICE_ERROR      Error setting up memory

**/
EFI_STATUS
T186ResourceConfig (
  IN UINTN                 CpuBootloaderAddress,
  OUT TEGRA_RESOURCE_INFO  *PlatformInfo
  )
{
  TEGRA_CPUBL_PARAMS   *CpuBootloaderParams;
  NVDA_MEMORY_REGION   *CarveoutRegions;
  UINTN                CarveoutRegionsCount=0;
  UINTN                TegraSystemMemoryBase;
  UINTN                Index;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  PlatformInfo->SdramSize = (UINTN)MmioRead32 (PcdGet64 (PcdMemorySizeRegisterT186)) << 20;
  PlatformInfo->DtbLoadAddress = CpuBootloaderParams->DtbLoadAddress;
  TegraSystemMemoryBase = TegraGetSystemMemoryBaseAddress(T186_CHIP_ID);

  //Build Carveout regions
  CarveoutRegions = (NVDA_MEMORY_REGION *)AllocatePool (sizeof (NVDA_MEMORY_REGION) * (CARVEOUT_NUM + CpuBootloaderParams->GlobalData.ValidDramBadPageCount));
  ASSERT (CarveoutRegions != NULL);
  if (CarveoutRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  for (Index = CARVEOUT_NVDEC; Index < CARVEOUT_NUM; Index++) {
    if ((CpuBootloaderParams->GlobalData.Carveout[Index].MemoryBaseAddress < TegraSystemMemoryBase) ||
        (CpuBootloaderParams->GlobalData.Carveout[Index].MemoryLength == 0)) {
      continue;
    }
    switch (Index) {
    //Skip free carveouts
    case CARVEOUT_MB2:
    case CARVEOUT_CPUBL:
    case CARVEOUT_RESERVED1:
    case CARVEOUT_PRIMARY:
    case CARVEOUT_EXTENDED:
    case CARVEOUT_MB2_HEAP:
    case CARVEOUT_BO_MTS_PACKAGE:
      break;

    case CARVEOUT_CPUBL_PARAMS:
      //Leave in memory map but marked as used
      BuildMemoryAllocationHob (
        CpuBootloaderParams->GlobalData.Carveout[Index].MemoryBaseAddress,
        EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CpuBootloaderParams->GlobalData.Carveout[Index].MemoryLength)),
        EfiBootServicesData
      );
      break;

    default:
      CarveoutRegions[CarveoutRegionsCount].MemoryBaseAddress = CpuBootloaderParams->GlobalData.Carveout[Index].MemoryBaseAddress;
      CarveoutRegions[CarveoutRegionsCount].MemoryLength      = CpuBootloaderParams->GlobalData.Carveout[Index].MemoryLength;
      CarveoutRegionsCount++;
    }
  }

  //Add bad DRAM carveouts
  for (Index = 0; Index < CpuBootloaderParams->GlobalData.ValidDramBadPageCount; Index++) {
    CarveoutRegions[CarveoutRegionsCount].MemoryBaseAddress = CpuBootloaderParams->GlobalData.DramBadPages[Index];
    CarveoutRegions[CarveoutRegionsCount].MemoryLength      = SIZE_64KB;
    CarveoutRegionsCount++;
  }

  PlatformInfo->CarveoutRegions = CarveoutRegions;
  PlatformInfo->CarveoutRegionsCount = CarveoutRegionsCount;

  return EFI_SUCCESS;
}

/**
  Retrieve DTB Address

**/
UINT64
T186GetDTBBaseAddress (
  IN UINTN CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS   *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  return CpuBootloaderParams->DtbLoadAddress;
}

/**
  Retrieve Recovery Boot Type

**/
TEGRA_RECOVERY_BOOT_TYPE
T186GetRecoveryBootType (
  IN UINTN CpuBootloaderAddress
  )
{
  TEGRA_CPUBL_PARAMS   *CpuBootloaderParams;

  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;

  return CpuBootloaderParams->GlobalData.RecoveryBootType;
}
