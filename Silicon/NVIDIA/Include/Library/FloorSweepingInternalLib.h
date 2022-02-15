/** @file
*
*  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __FLOOR_SWEEPING_INTERNAL_LIB_H__
#define __FLOOR_SWEEPING_INTERNAL_LIB_H__

/**
  Check if the given core is enabled or not

**/
BOOLEAN
EFIAPI
IsCoreEnabledInternal (
  IN  UINT32  CpuNum,
  OUT BOOLEAN *CoreEnabled
);

/**
  Retrieve number of CPUs for each platform

**/
BOOLEAN
EFIAPI
GetNumberOfEnabledCpuCoresInternal (
  OUT UINT32 *NumCpus
);

#endif //__FLOOR_SWEEPING_INTERNAL_LIB_H__
