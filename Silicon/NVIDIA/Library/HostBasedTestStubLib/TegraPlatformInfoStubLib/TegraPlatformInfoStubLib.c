/** @file

  Tegra Platform Info Lib stubs for host based tests

  Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <HostBasedTestStubLib/TegraPlatformInfoStubLib.h>

UINT32
TegraGetChipID (
  VOID
  )
{
  UINT32  ChipId;

  ChipId = (UINT32)mock ();
  return ChipId;
}

VOID
MockTegraGetChipID (
  UINT32  ChipId
  )
{
  will_return (TegraGetChipID, ChipId);
}

TEGRA_PLATFORM_TYPE
TegraGetPlatform (
  VOID
  )
{
  // Stub not implemented.
  fail ();
  return TEGRA_PLATFORM_UNKNOWN;
}

UINT64
TegraGetSystemMemoryBaseAddress (
  UINT32  ChipID
  )
{
  // Stub not implemented.
  fail ();
  return 0;
}
