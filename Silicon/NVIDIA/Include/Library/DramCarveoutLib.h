/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
  EFI_PHYSICAL_ADDRESS    MemoryBaseAddress;
  UINT64                  MemoryLength;
} NVDA_MEMORY_REGION;

/**
  Installs DRAM resources to the HOB list

  This function install the specified DRAM regions into memory while removing
  the carveout regions.
  This function is called by the platform memory initialization library.

  @param  InputDramRegions            Unsorted list of available DRAM regions
  @param  DramRegionsCount            Number of regions in DramRegions.
  @param  UefiDramRegionIndex         Index of UEFI usable regions in DramRegions.
  @param  CarveoutRegions             Unsorted list of carveout regions that will be
                                      removed from DramRegions.
  @param  CarveoutRegionsCount        Number of regions in CarveoutRegions.
  @param  UsableCarveoutRegions       Unsorted list of usable carveout regions that will be
                                      added to DramRegions.
  @param  UsableCarveoutRegionsCount  Number of regions in UsableCarveoutRegions.
  @param  FinalRegionsCount           Number of regions installed into HOB list.
  @param  MaxRegionStart              Base address of largest region in DRAM usable by UEFI
  @param  MaxRegionSize               Size of largest region region in DRAM usable by UEFI

  @retval EFI_SUCCESS                 Resources have been installed
  @retval EFI_DEVICE_ERROR            Error setting up memory

**/
EFI_STATUS
InstallDramWithCarveouts (
  IN  CONST NVDA_MEMORY_REGION  *InputDramRegions,
  IN  UINTN                     DramRegionsCount,
  IN  UINTN                     UefiDramRegionIndex,
  IN  NVDA_MEMORY_REGION        *CarveoutRegions,
  IN  UINTN                     CarveoutRegionsCount,
  IN  NVDA_MEMORY_REGION        *UsableCarveoutRegions,
  IN  UINTN                     UsableCarveoutRegionsCount,
  OUT UINTN                     *FinalRegionsCount,
  OUT EFI_PHYSICAL_ADDRESS      *MaxRegionStart,
  OUT UINTN                     *MaxRegionSize
  );

#endif //__DRAM_CARVEOUT_LIB_H__
