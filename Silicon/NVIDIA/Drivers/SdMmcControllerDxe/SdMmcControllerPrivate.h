/** @file

  SD MMC Controller Driver private structures

  Copyright (c) 2018-2019, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SD_MMC_CONTROLLER_PRIVATE_H__
#define __SD_MMC_CONTROLLER_PRIVATE_H__

#include <PiDxe.h>

#define SD_MMC_HC_CLOCK_CTRL       0x2C
#define SD_MMC_CLK_CTRL_SD_CLK_EN  BIT2

#define SD_MMC_MAX_CLOCK  255000000

#define SDHCI_CLOCK_NAME      "sdhci"
#define SDHCI_CLOCK_OLD_NAME  "sdmmc"

/* Tegra SDHOST controller vendor register definitions */
#define SDHCI_TEGRA_VENDOR_CLOCK_CTRL               0x100
#define SDHCI_CLOCK_CTRL_TAP_MASK                   0x00ff0000
#define SDHCI_CLOCK_CTRL_TAP_SHIFT                  16
#define SDHCI_CLOCK_CTRL_BASE_CLOCK_OVERRIDE_START  8
#define SDHCI_CLOCK_CTRL_BASE_CLOCK_OVERRIDE_END    15
#define SDHCI_CLOCK_CTRL_SDR50_TUNING_OVERRIDE      5
#define SDHCI_CLOCK_CTRL_PADPIPE_CLKEN_OVERRIDE     3
#define SDHCI_CLOCK_CTRL_SPI_MODE_CLKEN_OVERRIDE    2

#define SDHCI_TEGRA_VENDOR_MISC_CTRL           0x120
#define SDHCI_MISC_CTRL_ENABLE_SDR104_BIT      3
#define SDHCI_MISC_CTRL_ENABLE_SDR50           4
#define SDHCI_MISC_CTRL_ENABLE_SDHCI_SPEC_300  5
#define SDHCI_MISC_CTRL_ENABLE_DDR50           9
#define SDHCI_MISC_CTRL_ENABLE_SDR104          3

typedef struct {
  UINT32    TimeoutFreq   : 6; // bit 0:5
  UINT32    Reserved      : 1; // bit 6
  UINT32    TimeoutUnit   : 1; // bit 7
  UINT32    BaseClkFreq   : 8; // bit 8:15
  UINT32    MaxBlkLen     : 2; // bit 16:17
  UINT32    BusWidth8     : 1; // bit 18
  UINT32    Adma2         : 1; // bit 19
  UINT32    Reserved2     : 1; // bit 20
  UINT32    HighSpeed     : 1; // bit 21
  UINT32    Sdma          : 1; // bit 22
  UINT32    SuspRes       : 1; // bit 23
  UINT32    Voltage33     : 1; // bit 24
  UINT32    Voltage30     : 1; // bit 25
  UINT32    Voltage18     : 1; // bit 26
  UINT32    SysBus64V4    : 1; // bit 27
  UINT32    SysBus64V3    : 1; // bit 28
  UINT32    AsyncInt      : 1; // bit 29
  UINT32    SlotType      : 2; // bit 30:31
  UINT32    Sdr50         : 1; // bit 32
  UINT32    Sdr104        : 1; // bit 33
  UINT32    Ddr50         : 1; // bit 34
  UINT32    Reserved3     : 1; // bit 35
  UINT32    DriverTypeA   : 1; // bit 36
  UINT32    DriverTypeC   : 1; // bit 37
  UINT32    DriverTypeD   : 1; // bit 38
  UINT32    DriverType4   : 1; // bit 39
  UINT32    TimerCount    : 4; // bit 40:43
  UINT32    Reserved4     : 1; // bit 44
  UINT32    TuningSDR50   : 1; // bit 45
  UINT32    RetuningMod   : 2; // bit 46:47
  UINT32    ClkMultiplier : 8; // bit 48:55
  UINT32    Reserved5     : 7; // bit 56:62
  UINT32    Hs400         : 1; // bit 63
} SD_MMC_HC_SLOT_CAP;

#endif
