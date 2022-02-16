/** @file
*
*  Copyright (c) 2018, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __DRAM_CARVEOUT_LIB_H__
#define __DRAM_CARVEOUT_LIB_H__

#include <Uefi/UefiBaseType.h>

///
/// Describes memory regions
///
typedef struct {
  EFI_PHYSICAL_ADDRESS MemoryBaseAddress;
  UINT64               MemoryLength;
} NVDA_MEMORY_REGION;

/**
  Installs resources DRAM the HOB list

  This function install the specified DRAM regions into memory while removing
  the carveout regions.
  This function is called by the platform memory initialization library.

  @param  DramRegions              Sorted list of available DRAM regions
  @param  DramRegionsCount         Number of regions in DramRegions.
  @param  CarveoutRegions          Sorted list of carveout regions that will be
                                   removed from DramRegions.
  @param  CarveoutRegionsCount     Number of regions in CarveoutRegions.
  @param  FinalRegionsCount        Number of regions installed into HOB list.

  @retval EFI_SUCCESS              Resources have been installed
  @retval EFI_DEVICE_ERROR         Error setting up memory

**/
EFI_STATUS
InstallDramWithCarveouts (
  IN  NVDA_MEMORY_REGION *DramRegions,
  IN  UINTN              DramRegionsCount,
  IN  NVDA_MEMORY_REGION *CarveoutRegions,
  IN  UINTN              CarveoutRegionsCount,
  OUT UINTN              *FinalRegionsCount
);

#endif //__DRAM_CARVEOUT_LIB_H__
