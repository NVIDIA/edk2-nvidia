/** @file
  Host-build wrapper for GenericWatchdogInfoParser.c.

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

UINT64
EFIAPI
GenericWatchdogTestPcdGet64_PcdGenericWatchdogControlBase (
  VOID
  );

UINT64
EFIAPI
GenericWatchdogTestPcdGet64_PcdGenericWatchdogRefreshBase (
  VOID
  );

UINT32
EFIAPI
GenericWatchdogTestPcdGet32_PcdGenericWatchdogEl2IntrNum (
  VOID
  );

#undef PcdGet64
#define PcdGet64(TokenName)  GenericWatchdogTestPcdGet64_##TokenName ()

#undef PcdGet32
#define PcdGet32(TokenName)  GenericWatchdogTestPcdGet32_##TokenName ()

#include "../GenericWatchdogInfoParser.c"
