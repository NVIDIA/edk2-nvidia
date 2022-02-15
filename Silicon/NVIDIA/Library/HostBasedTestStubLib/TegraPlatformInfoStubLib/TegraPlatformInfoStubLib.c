/** @file

  Tegra Platform Info Lib stubs for host based tests

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
  UINT32 ChipId;

  ChipId = (UINT32) mock();
  return ChipId;
}

VOID
MockTegraGetChipID (
  UINT32    ChipId
  )
{
  will_return (TegraGetChipID, ChipId);
}
