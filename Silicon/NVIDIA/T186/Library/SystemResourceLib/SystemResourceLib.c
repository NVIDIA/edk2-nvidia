/** @file
*
*  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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
#include <Library/SystemResourceLib.h>
#include <Library/DramCarveoutLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Pi/PiHob.h>
#include "SystemResourceLibPrivate.h"
#include <libfdt.h>

STATIC
EFI_STATUS
InstallMmioRegions (
  OUT UINTN *MmioRegionCount
)
{
  UINTN RegionCount;
  CONST NVDA_MEMORY_REGION MmioRegions[] = {
    { FixedPcdGet64 (PcdSerialRegisterBase),        SIZE_4KB },
    { FixedPcdGet64 (PcdGicDistributorBase),        SIZE_4KB },
    { FixedPcdGet64 (PcdGicInterruptInterfaceBase), SIZE_4KB },
  };
  EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttribute = (EFI_RESOURCE_ATTRIBUTE_PRESENT |
                                                    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
                                                    EFI_RESOURCE_ATTRIBUTE_TESTED
                                                   );

  for (RegionCount = 0; RegionCount < ARRAY_SIZE (MmioRegions); RegionCount++) {
    BuildResourceDescriptorHob (
      EFI_RESOURCE_FIRMWARE_DEVICE,
      ResourceAttribute,
      MmioRegions[RegionCount].MemoryBaseAddress,
      MmioRegions[RegionCount].MemoryLength
    );
  }

  *MmioRegionCount = RegionCount;

  return EFI_SUCCESS;
}

/**
  Installs resources into the HOB list

  This function install all memory regions into the HOB list.
  This function is called by the platform memory initialization library.

  @param  NumberOfMemoryRegions Number of regions installed into HOB list.

  @retval EFI_SUCCESS           Resources have been installed
  @retval EFI_DEVICE_ERROR      Error setting up memory

**/
EFI_STATUS
InstallSystemResources (
  OUT UINTN *MemoryRegionsCount
  )
{
  EFI_STATUS           Status;
  NVDA_MEMORY_REGION   DramRegion;
  NVDA_MEMORY_REGION   *CarveoutRegions = NULL;
  UINTN                CarveoutRegionsCount = 0;
  UINTN                FinalDramRegionsCount = 0;
  UINTN                CpuBootloaderAddress;
  TEGRA_CPUBL_PARAMS   *CpuBootloaderParams;
  UINTN                Index;
  EFI_PHYSICAL_ADDRESS *DeviceTreeHobData = NULL;

  if (NULL == MemoryRegionsCount) {
    return EFI_INVALID_PARAMETER;
  }

  *MemoryRegionsCount = 0;

  //Install MMIO regions
  Status = InstallMmioRegions (MemoryRegionsCount);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //Build DRAM regions
  DramRegion.MemoryBaseAddress = PcdGet64 (PcdSystemMemoryBase);
  DramRegion.MemoryLength = (UINTN)MmioRead32 (PcdGet64 (PcdMemorySizeRegister)) << 20;
  ASSERT (DramRegion.MemoryLength != 0);

  //Build Carveout regions
  CpuBootloaderAddress = (UINTN)MmioRead32 (PcdGet64 (PcdBootloaderInfoLocationAddress));
  //Address may be encoded as number of 64KiB pages from 0.
  if (CpuBootloaderAddress < DramRegion.MemoryBaseAddress) {
    CpuBootloaderAddress <<= 16;
  }
  CpuBootloaderParams = (TEGRA_CPUBL_PARAMS *)(VOID *)CpuBootloaderAddress;
  ASSERT (CpuBootloaderParams != NULL);
  if (CpuBootloaderParams == NULL) {
    return EFI_DEVICE_ERROR;
  }

  CarveoutRegions = (NVDA_MEMORY_REGION *)AllocatePool (sizeof (NVDA_MEMORY_REGION) * (CARVEOUT_NUM + CpuBootloaderParams->GlobalData.ValidDramBadPageCount));
  ASSERT (CarveoutRegions != NULL);
  if (CarveoutRegions == NULL) {
    return EFI_DEVICE_ERROR;
  }

  for (Index = CARVEOUT_NVDEC; Index < CARVEOUT_NUM; Index++) {
    if ((CpuBootloaderParams->GlobalData.Carveout[Index].MemoryBaseAddress < DramRegion.MemoryBaseAddress) ||
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

  //Adjust Carveout to be 64KiB aligned and sized to meet UEFI memory map requirements.
  for (Index = 0; Index < CarveoutRegionsCount; Index++) {
    UINTN AddressShift = (CarveoutRegions[Index].MemoryBaseAddress & (SIZE_64KB-1));
    CarveoutRegions[Index].MemoryBaseAddress -= AddressShift;
    CarveoutRegions[Index].MemoryLength = ALIGN_VALUE (CarveoutRegions[Index].MemoryLength + AddressShift, SIZE_64KB);
  }

  Status = InstallDramWithCarveouts (
             &DramRegion,
             1,
             CarveoutRegions,
             CarveoutRegionsCount,
             &FinalDramRegionsCount
           );

  if (!EFI_ERROR (Status)) {
    *MemoryRegionsCount += FinalDramRegionsCount;
  }
  FreePool (CarveoutRegions);

  //Register Device Tree
  if (0 != CpuBootloaderParams->DtbLoadAddress) {
    if (fdt_check_header ((VOID *)CpuBootloaderParams->DtbLoadAddress) == 0) {
      UINTN DtbSize = fdt_totalsize ((VOID *)CpuBootloaderParams->DtbLoadAddress);
      EFI_PHYSICAL_ADDRESS AlignedDtb = CpuBootloaderParams->DtbLoadAddress & ~(SIZE_4KB-1);
      BuildMemoryAllocationHob (
        AlignedDtb,
        EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (DtbSize + (CpuBootloaderParams->DtbLoadAddress - AlignedDtb))),
        EfiBootServicesData
      );

      DeviceTreeHobData = (EFI_PHYSICAL_ADDRESS *)BuildGuidHob ( &gFdtHobGuid, sizeof (EFI_PHYSICAL_ADDRESS));
      if (NULL != DeviceTreeHobData) {
        *DeviceTreeHobData = CpuBootloaderParams->DtbLoadAddress;
      }
    }
  }

  return Status;
}
