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

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include "FloorSweepingLibPrivate.h"

UINT32
GetNumberOfEnabledCpuCores (
  VOID
  )
{
  UINT64 Data;

  WriteNvgChannelIdx(TEGRA_NVG_CHANNEL_NUM_CORES);
  Data = ReadNvgChannelData();

  return (Data & 0xF);
}

UINT32
ConvertCpuLogicalToMpidr (
  IN UINT32 LogicalCore
  )
{
  UINT32 NumCores;
  UINT32 Mpidr = 0;
  UINT64 Data = 0;

  NumCores = GetNumberOfEnabledCpuCores();
  if (LogicalCore < NumCores) {
    WriteNvgChannelIdx (TEGRA_NVG_CHANNEL_LOGICAL_TO_MPIDR);

    /* Write the logical core id */
    WriteNvgChannelData (LogicalCore);

    /* Read-back the MPIDR */
    Data = ReadNvgChannelData ();
    Mpidr = (Data & 0xFFFFFFFF);

    DEBUG ((DEBUG_INFO, "NVG: Logical CPU: %u; MPIDR: 0x%x\n", LogicalCore, Mpidr));
  } else {
    DEBUG ((DEBUG_ERROR, "Core: %u is not present\r\n", LogicalCore));
  }

  return Mpidr;
}

