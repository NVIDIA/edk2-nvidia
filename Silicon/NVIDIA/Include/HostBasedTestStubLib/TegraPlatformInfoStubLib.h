/** @file

  Tegra Platform Info Lib stubs for host based tests

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef __TEGRA_PLATFORM_INFO_STUB_LIB_H__
#define __TEGRA_PLATFORM_INFO_STUB_LIB_H__

#include <Library/TegraPlatformInfoLib.h>

/**
  Set up mock parameters for TegraGetChipID() stub

  @param[In]  ChipId                Chip ID to return

  @retval None

**/
VOID
MockTegraGetChipID (
  IN  UINT32    ChipId
  );

#endif
