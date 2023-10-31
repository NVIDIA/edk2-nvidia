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
  // Dummy value for now
  return 64;
}
