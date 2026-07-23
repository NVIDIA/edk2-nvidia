/** @file
  Decode a Tegra T23x PMC RST_STATUS[RST_SOURCE] value into an Android canonical
  boot reason string. Pure logic (no MMIO / no library dependencies) so it can
  be exhaustively unit-tested on a host.

  The RST_SOURCE enum is SoC-specific. This file implements the T23x family
  mapping only, mirroring the per-SoC split used elsewhere (e.g.
  PlatformResourceLib's T234ResourceConfig.c). A different SoC family must
  supply its own <Soc>PmcSourceToReason and its own
  host unit test. Platform selection is at build time: the caller's PCD
  (PcdBootReasonPmcRstStatusAddress) is only set under CONFIG_SOC_T23X, so this
  decode is never reached on other SoCs.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef T23X_PMC_BOOT_REASON_H_
#define T23X_PMC_BOOT_REASON_H_

#include <Uefi.h>

/**
  Map a T23x PMC RST_STATUS[RST_SOURCE] value (PMC_IMPL_RST_STATUS_0, bits[7:2],
  64 sources) to an Android canonical boot reason.

  @param[in]  Source  The 6-bit RST_SOURCE enum.

  @return  A static canonical reason string (never NULL). Unknown sources map
           to "reboot".
**/
CONST CHAR8 *
T23xPmcSourceToReason (
  IN  UINT8  Source
  );

#endif // T23X_PMC_BOOT_REASON_H_
