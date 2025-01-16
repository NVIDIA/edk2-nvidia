/** @file

  Tegra CPU Frequency Driver Private header.

  SPDX-FileCopyrightText: Copyright (c) 2022-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef  TEGRA_CPU_FREQ_DXE_PRIVATE_H
#define TEGRA_CPU_FREQ_DXE_PRIVATE_H

#include <Uefi.h>
#include <Library/BaseLib.h>

#define T234_SCRATCH_FREQ_CORE_REG(cluster, core)      (0x2000 + ((cluster * 4 + core) * 8))
#define T264_CLUSTER_OFFSET(cluster)                   ((cluster % 2) * 0x10000)
#define T264_SWITCH_OFFSET(cluster)                    (0x40000 + ((cluster / 2) * 0x400000))
#define T264_SCRATCH_FREQ_CORE_REG(cluster)            (T264_SWITCH_OFFSET(cluster) + T264_CLUSTER_OFFSET(cluster))
#define TH500_SCRATCH_FREQ_CORE_REG(cluster)           (((cluster >> 1) << 14) | ((cluster & 0x1) << 12))
#define T234_CLUSTER_ACTMON_REFCLK_REG(cluster, core)  (0x30000 + (cluster * 0x10000) + 0x9000 + (core * 8) + 0x20)
#define T234_CLUSTER_ACTMON_CORE_REG(cluster, core)    (0x30000 + (cluster * 0x10000) + 0x9000 + (core * 8) + 0x40)
#define T234_REFCLK_FREQ   408000000
#define T264_REFCLK_FREQ   1000000000
#define TH500_REFCLK_FREQ  1000000000
#define T410_REFCLK_FREQ   1000000000
#define T410_SW_FREQ_BASE_REG(cluster)         (0x900000 + (cluster * 0x1000))
#define T410_CPPC_MAX_PERF_REG(cluster, core)  (T410_SW_FREQ_BASE_REG(cluster) + (core * 4))
#define T410_CPPC_MAX_PERF_BITS  9
#define T410_CPPC_MIN_PERF_REG(cluster, core)  (T410_SW_FREQ_BASE_REG(cluster) + 0x8 + (core * 4))
#define T410_CPPC_MIN_PERF_BITS  9
#define T410_CPPC_EPP_REG(cluster, core)  (T410_SW_FREQ_BASE_REG(cluster) + 0x18 + (core * 4))
#define T410_CPPC_EPP_BITS  8
#define T410_CPPC_AUTONOMOUS_SELECT_REG(cluster, core)  (T410_SW_FREQ_BASE_REG(cluster) + 0x20 + (core * 4))
#define T410_CPPC_AUTONOMOUS_SELECT_BITS  1
#define T410_CPPC_TIME_WINDOW_REG(cluster, core)  (T410_SW_FREQ_BASE_REG(cluster) + 0x28 + (core * 4))
#define T410_CPPC_TIME_WINDOW_BITS  10
#define T410_CPPC_PERF_LIMITED_REG(cluster, core)  (T410_SW_FREQ_BASE_REG(cluster) + 0x30 + (core * 4))
#define T410_CPPC_PERF_LIMITED_BITS  2
#define T410_CPPC_DESIRED_PERF_REG(cluster, core)  (T410_SW_FREQ_BASE_REG(cluster) + 0x38 + (core * 4))
#define T410_CPPC_DESIRED_PERF_BITS  9
#define T410_DUMMY_PDIV              1
#define T410_DUMMY_MDIV              111
#define T410_DUMMY_NDIV_MAX          350
#define T410_DUMMY_NDIV_MIN          9
#define HZ_TO_MHZ(x)  (x/1000000)

#pragma pack (1)
typedef struct {
  UINT32    ClusterId;
} BPMP_CPU_NDIV_LIMITS_REQUEST;

typedef struct {
  UINT32    ref_clk_hz;
  UINT16    pdiv;
  UINT16    mdiv;
  UINT16    ndiv_max;
  UINT16    ndiv_min;
} BPMP_CPU_NDIV_LIMITS_RESPONSE;
#pragma pack ()

#endif
