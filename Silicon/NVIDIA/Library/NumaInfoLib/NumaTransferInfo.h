/** @file

  Numa Transfer Info definitions

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NUMA_TRANSFER_INFO_H_
#define _NUMA_TRANSFER_INFO_H_

#include <Uefi.h>

// Initiator Class: CPU, GPU, PCIE
// Target (memory) Class: CPU, GPU, PCIE
// Locality: Local (for PCIE this means on the same PCIE RP, for GPU this means same GPU), same Socket (but not local), Remote socket
// Note: CXL devices fall under PCIE

// Have a platform template 3D array indexed by Initiator, Target, Locality
// Each entry then has R/W Latency, BW, slit Distance between Initiator and Target for the Locality
// This template table can come from DTB, and can have a dummy table compiled into UEFI

// Eventually, when calculating actual tables:
// For PCIE initiators, add the specific CDAT info to go from device to its RP
// For PCIE targets, add the specific CDAT info to go from target RP to device
// Note: Latencies and Distances are added, Bandwidths are minimums

#define UNREACHABLE_LATENCY    0xFFFF
#define UNREACHABLE_BANDWIDTH  0xFFFF
#define NORMALIZED_DISTANCE    10
#define UNREACHABLE_DISTANCE   0xFF

typedef enum {
  INITIATOR_CPU = 0,
  INITIATOR_GPU,
  INITIATOR_PCIE, // includes CXL

  INITIATOR_MAX
} NUMA_INITIATOR_TYPE;

typedef enum {
  TARGET_CPU = 0,
  TARGET_GPU,
  TARGET_PCIE, // includes CXL

  TARGET_MAX
} NUMA_TARGET_TYPE;

typedef enum {
  LOCALITY_LOCAL = 0,      // initiator/target are same
  LOCALITY_SAME_SOCKET,    // same socket but not same location or not same type
  LOCALITY_REMOTE_SOCKET,  // not same socket

  LOCALITY_MAX
} NUMA_LOCALITY_TYPE;

typedef struct {
  UINT8     NormalizedDistance;
  UINT16    ReadLatency;
  UINT16    WriteLatency;
  UINT16    AccessBandwidth;
} NUMA_TRANSFER_INFO;

extern NUMA_TRANSFER_INFO  mSimTransferInfoTemplate[INITIATOR_MAX][TARGET_MAX][LOCALITY_MAX];

#endif // _NUMA_TRANSFER_INFO_H_
