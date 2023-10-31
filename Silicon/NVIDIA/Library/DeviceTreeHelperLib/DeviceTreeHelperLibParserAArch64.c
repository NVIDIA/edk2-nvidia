/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "DeviceTreeHelperLibPrivate.h"

/**
  Returns the cache block size in bytes based on the hardware

  @retval BlockSize - Block size in bytes

**/
UINT32
DeviceTreeGetCacheBlockSizeBytesFromHW (
  VOID
  )
{
  // Get the value from hardware
  UINT64  DczidReg;
  UINT32  BlockSizeBytes;

  asm ("mrs %0, dczid_el0" : "=r" (DczidReg));
  BlockSizeBytes = 4 << (DczidReg & 0xF);
  return BlockSizeBytes;
}
