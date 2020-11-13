/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __FLOOR_SWEEPING_LIB_H__
#define __FLOOR_SWEEPING_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Returns number of CPU cores supported on the system

  @return Number of CPU cores

**/
UINT32
GetNumberOfEnabledCpuCores (
  VOID
  );

/**
  Returns the Mpidr for a specified logical CPU

  @param LogicalCore Logical CPU core ID

  @return Mpidr of the CPU

**/
UINT32
ConvertCpuLogicalToMpidr (
  IN UINT32 LogicalCore
  );

#endif //__FLOOR_SWEEPING_LIB_H__
