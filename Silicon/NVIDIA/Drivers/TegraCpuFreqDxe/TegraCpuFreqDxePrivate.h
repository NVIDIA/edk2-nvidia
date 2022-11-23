/** @file

  Tegra CPU Frequency Driver Private header.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef  TEGRA_CPU_FREQ_DXE_PRIVATE_H
#define TEGRA_CPU_FREQ_DXE_PRIVATE_H

#include <Uefi.h>
#include <Library/BaseLib.h>

#define SCRATCH_FREQ_CORE_REG(linear_id)          (0x2000 + (linear_id * 8))
#define TH500_SCRATCH_FREQ_CORE_REG(Cluster)      (((Cluster >> 1) << 14) | ((Cluster & 0x1) << 12))
#define CLUSTER_ACTMON_REFCLK_REG(cluster, core)  (0x30000 + (cluster * 0x10000) + 0x9000 + (core * 8) + 0x20)
#define CLUSTER_ACTMON_CORE_REG(cluster, core)    (0x30000 + (cluster * 0x10000) + 0x9000 + (core * 8) + 0x40)
#define NDIV_MASK          0x1FF
#define REFCLK_FREQ        408000000
#define TH500_REFCLK_FREQ  1000000000
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

UINT64
GetT194PmCntr (
  VOID
  );

VOID
SetT194CpuNdiv (
  IN UINT64  Ndiv
  );

UINT64
GetT194CpuNdiv (
  VOID
  );

#endif
