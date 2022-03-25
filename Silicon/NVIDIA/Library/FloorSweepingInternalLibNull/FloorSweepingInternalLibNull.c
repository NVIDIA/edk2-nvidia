/** @file
*
*  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/FloorSweepingInternalLib.h>

/**
  Get CPU info for a platform

**/
BOOLEAN
EFIAPI
GetCpuInfoInternal (
  IN  UINTN     EnabledSockets,
  IN  UINTN     MaxSupportedCores,
  OUT UINT64    *EnabledCoresBitMap
  )
{
  return FALSE;
}
