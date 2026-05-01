/** @file
  Host-build wrapper for FanInfoParser.c.

  FanInfoParser.c reaches DynamicTablesPkg and DeviceTreeHelperLib
  headers before UEFI base types are defined in a HOST_APPLICATION
  build.  This wrapper includes <Uefi.h> first so all UEFI types are
  visible when the parser source is compiled.

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include "../FanInfoParser.c"
