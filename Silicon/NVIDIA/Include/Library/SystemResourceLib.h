/** @file
*
*  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __SYSTEM_RESOURCE_LIB_H__
#define __SYSTEM_RESOURCE_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Register device tree.

  This function copies and registers device tree into the GUID HOB list.

  @param  Physical address of device tree location.
**/
VOID
RegisterDeviceTree (
  IN UINTN  BlDtbLoadAddress
  );

/**
  Installs resources into the HOB list

  This function install all memory regions into the HOB list.
  This function is called by the platform memory initialization library.

  @param  MemoryRegionsCount    Number of regions installed into HOB list.
  @param  MaxRegionStart        Base address of largest region in dram
  @param  MaxRegionSize         Size of largest region

  @retval EFI_SUCCESS           Resources have been installed
  @retval EFI_DEVICE_ERROR      Error setting up memory

**/
EFI_STATUS
InstallSystemResources (
  OUT UINTN                 *MemoryRegionsCount,
  OUT EFI_PHYSICAL_ADDRESS  *MaxRegionStart,
  OUT UINTN                 *MaxRegionSize
  );

#endif //__SYSTEM_RESOURCE_LIB_H__
