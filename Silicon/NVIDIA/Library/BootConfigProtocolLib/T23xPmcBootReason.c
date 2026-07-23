/** @file
  Decode a Tegra T23x PMC RST_STATUS[RST_SOURCE] value into an Android canonical
  boot reason string. Pure logic (no MMIO / no library dependencies) so it can
  be exhaustively unit-tested on a host. T23x (Tegra234/238/239) only; other
  SoCs supply their own decode (see T23xPmcBootReason.h).

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "T23xPmcBootReason.h"

/**
  Map a T23x PMC RST_STATUS[RST_SOURCE] value to an Android canonical boot reason.
  Values per the T23x PMC_IMPL_RST_STATUS_0 RST_SOURCE field (64 sources).

  The 64 hardware sources are bucketed into a small, stable reason vocabulary
  (cold / warm / watchdog / shutdown,thermal / shutdown,undervoltage / reboot)
  rather than exposing a distinct string per source - aligned with the Shield-1
  (cboot) bootreason set (cold / warm / watchdog / shutdown / reboot), so
  consumers see the common causes and not raw register detail. Rare hardware
  faults with no Shield-1 bucket (clock/fuse/HSM) fold into reboot, and
  kernel_panic is intentionally left to ramoops/bootstat, not decoded here.

  NB: SYS_RESET_N(0) is the power-on source but can also be an *external* reset
  asserted by the PMIC/core-rail regulators (e.g. a DA9092 over-voltage -> SHDN
  chain, per bug 5166812) - the PMC cannot distinguish these, so it maps to cold.
**/
CONST CHAR8 *
T23xPmcSourceToReason (
  IN  UINT8  Source
  )
{
  switch (Source) {
    case 0:                                            // SYS_RESET_N (external reset / power-on)
      return "cold";

    // Thermal sensors -> thermal shutdown.
    case 8: case 9: case 10: case 31:                  // SENSOR / AOTAG / VFSENSOR / FSI_THERM
      return "shutdown,thermal";

    // Voltage monitors + brownout / core-rail power loss -> undervoltage shutdown.
    case 37: case 51: case 52: case 53: case 60: case 62: case 63: // VMON (SoC/CPU/FSI/VMON_PLUS)
    case 27: case 54: case 55: case 56: case 58: case 59: case 61: // VREFRO power-bad / POD
      return "shutdown,undervoltage";

    // Software / boot / debug resets -> warm.
    case 11: case 12: case 23:                         // MAINSWRST / SC7 / PSC_SW
    case 14: case 19: case 24:                         // CSITE / BPMPBOOT / CSITE_SW
      return "warm";

    // Watchdog timers (AO/BPMP/SCE/SPE/RCE/PVA/DCE/PSC/FSI/TOP/APE) -> watchdog.
    case 1:  case 3:  case 4:  case 5:  case 15: case 16: case 17:
    case 21: case 22: case 30: case 32: case 33: case 34: case 35:
    case 38: case 39: case 40: case 41: case 42: case 43: case 44:
      return "watchdog";

    // Everything else -> reboot. This includes the rare hardware-fault resets
    // that Shield-1 has no dedicated bucket for: clock/frequency safety monitors
    // (SCPM/FMON/CSDC - bug 5166812 saw SCPM_RTC_XTAL(46)/FMON_32K(49)), the
    // hardware safety mechanism (HSM, 13), fuse-CRC (20), plus unassigned/
    // LP-exit sources.
    default:
      return "reboot";
  }
}
