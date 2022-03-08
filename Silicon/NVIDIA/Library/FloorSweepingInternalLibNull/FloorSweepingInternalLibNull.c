/** @file
*
*  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/FloorSweepingInternalLib.h>


BOOLEAN
EFIAPI
IsCoreEnabledInternal (
  IN  UINT32  CpuNum,
  OUT BOOLEAN *CoreEnabled
)
{
  return FALSE;
}

/**
  Retrieve number of enabled CPUs for each platform

**/
BOOLEAN
EFIAPI
GetNumberOfEnabledCpuCoresInternal (
  OUT UINT32 *NumCpus
)
{
  return FALSE;
}

/**
  Retrieve number of sockets for each platform

**/
BOOLEAN
EFIAPI
GetNumberOfEnabledSocketsInternal (
  OUT UINT32 *NumSockets
)
{
  return FALSE;
}
