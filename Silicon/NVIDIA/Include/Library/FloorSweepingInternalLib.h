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
  Get CPU info for a platform

**/
BOOLEAN
EFIAPI
GetCpuInfoInternal (
  IN  UINTN     EnabledSockets,
  IN  UINTN     MaxSupportedCores,
  OUT UINT64    *EnabledCoresBitMap
  );

#endif //__FLOOR_SWEEPING_INTERNAL_LIB_H__
