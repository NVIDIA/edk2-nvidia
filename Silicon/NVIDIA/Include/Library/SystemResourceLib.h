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

#ifndef __SYSTEM_RESOURCE_LIB_H__
#define __SYSTEM_RESOURCE_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Installs resources into the HOB list

  This function install all memory regions into the HOB list.
  This function is called by the platform memory initialization library.

  @param  MemoryRegionsCount    Number of regions installed into HOB list.

  @retval EFI_SUCCESS           Resources have been installed
  @retval EFI_DEVICE_ERROR      Error setting up memory

**/
EFI_STATUS
InstallSystemResources (
  OUT UINTN *MemoryRegionsCount
);

#endif //__SYSTEM_RESOURCE_LIB_H__
