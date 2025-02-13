/** @file

  Numa Transfer Info for T41X sim

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NumaTransferInfo.h"

// Polutated with values for T410 sim
NUMA_TRANSFER_INFO  mSimTransferInfoTemplate[INITIATOR_MAX][TARGET_MAX][LOCALITY_MAX] = {
  // CPU
  [INITIATOR_CPU][TARGET_CPU][LOCALITY_LOCAL] =
  { NORMALIZED_DISTANCE, 160, 85, 512 },

  // All CPU memory is local rather than same socket to its CPU
  [INITIATOR_CPU][TARGET_CPU][LOCALITY_SAME_SOCKET] =
  { UNREACHABLE_DISTANCE, UNREACHABLE_LATENCY, UNREACHABLE_LATENCY, UNREACHABLE_BANDWIDTH },

  [INITIATOR_CPU][TARGET_CPU][LOCALITY_REMOTE_SOCKET] =
  { 40, 445, 370, 100 },

  // All GPU memory is not local to the CPU
  [INITIATOR_CPU][TARGET_GPU][LOCALITY_LOCAL] =
  { UNREACHABLE_DISTANCE, UNREACHABLE_LATENCY, UNREACHABLE_LATENCY, UNREACHABLE_BANDWIDTH },

  [INITIATOR_CPU][TARGET_GPU][LOCALITY_SAME_SOCKET] =
  { 80, 520, 385, 450 },

  [INITIATOR_CPU][TARGET_GPU][LOCALITY_REMOTE_SOCKET] =
  { 120, 0xEEEE, 0xEEEE, 1 },

  // All PCIE memory is not local to the CPU
  [INITIATOR_CPU][TARGET_PCIE][LOCALITY_LOCAL] =
  { UNREACHABLE_DISTANCE, UNREACHABLE_LATENCY, UNREACHABLE_LATENCY, UNREACHABLE_BANDWIDTH },

  [INITIATOR_CPU][TARGET_PCIE][LOCALITY_SAME_SOCKET] =
  { 90, 620, 485, 350 },   // JDS TODO - made up to be a bit worse than GPU

  [INITIATOR_CPU][TARGET_PCIE][LOCALITY_REMOTE_SOCKET] =
  { 130, 0xEEFF, 0xEEFF, 1 },   // JDS TODO - made up to be a bit worse than GPU

  // GPU
  // All CPU memory is not local to the GPU
  [INITIATOR_GPU][TARGET_CPU][LOCALITY_LOCAL] =
  { UNREACHABLE_DISTANCE, UNREACHABLE_LATENCY, UNREACHABLE_LATENCY, UNREACHABLE_BANDWIDTH },

  [INITIATOR_GPU][TARGET_CPU][LOCALITY_SAME_SOCKET] =
  { 80, 640, 565, 450 },

  [INITIATOR_GPU][TARGET_CPU][LOCALITY_REMOTE_SOCKET] =
  { 120, 1400, 1325, 450 },

  [INITIATOR_GPU][TARGET_GPU][LOCALITY_LOCAL] =
  { NORMALIZED_DISTANCE, 324, 115, 3000 },

  [INITIATOR_GPU][TARGET_GPU][LOCALITY_SAME_SOCKET] =
  { 40, 1100, 965, 450 },   // TODO - figure out a reasonable number rather than copying remote

  [INITIATOR_GPU][TARGET_GPU][LOCALITY_REMOTE_SOCKET] =
  { 40, 1100, 965, 450 },

  // All PCIE memory is not local to the GPU
  [INITIATOR_GPU][TARGET_PCIE][LOCALITY_LOCAL] =
  { NORMALIZED_DISTANCE, UNREACHABLE_LATENCY, UNREACHABLE_LATENCY, UNREACHABLE_BANDWIDTH },

  [INITIATOR_GPU][TARGET_PCIE][LOCALITY_SAME_SOCKET] =
  { 60, 1200, 1065, 350 },   // JDS TODO - made up to be a bit worse than GPU to same socket, which is a copy of GPU to remote socket

  [INITIATOR_GPU][TARGET_PCIE][LOCALITY_REMOTE_SOCKET] =
  { 140, 0xEFFF, 0xEFFF, 1 },   // JDS TODO - made up to be a bit worse than CPU to remote GPU

  // PCIE
  // All CPU memory is not local to the PCIE
  [INITIATOR_PCIE][TARGET_CPU][LOCALITY_LOCAL] =
  { UNREACHABLE_DISTANCE, UNREACHABLE_LATENCY, UNREACHABLE_LATENCY, UNREACHABLE_BANDWIDTH },

  [INITIATOR_PCIE][TARGET_CPU][LOCALITY_SAME_SOCKET] =
  { 90, 740, 665, 350 },   // JDS TODO - made up to be a bit worse than GPU

  [INITIATOR_PCIE][TARGET_CPU][LOCALITY_REMOTE_SOCKET] =
  { 130, 1500, 1425, 350 },   // JDS TODO - made up to be a bit worse than GPU

  // All GPU memory is not local to the PCIE
  [INITIATOR_PCIE][TARGET_GPU][LOCALITY_LOCAL] =
  { NORMALIZED_DISTANCE, UNREACHABLE_LATENCY, UNREACHABLE_LATENCY, UNREACHABLE_BANDWIDTH },

  [INITIATOR_PCIE][TARGET_GPU][LOCALITY_SAME_SOCKET] =
  { 60, 1200, 1065, 350 },   // JDS TODO - made up to be a bit worse than GPU to same socket GPU, which is a copy of GPU to remote socket

  [INITIATOR_PCIE][TARGET_GPU][LOCALITY_REMOTE_SOCKET] =
  { 140, 0xEFFF, 0xEFFF, 1 },   // JDS TODO - made up to be a bit worse than CPU to remote GPU

  [INITIATOR_PCIE][TARGET_PCIE][LOCALITY_LOCAL] =
  { NORMALIZED_DISTANCE, 424, 215, 2500 },   // JDS TODO - made up to be a bit worse than GPU to self

  [INITIATOR_PCIE][TARGET_PCIE][LOCALITY_SAME_SOCKET] =
  { 40, 1100, 965, 450 },   // JDS TODO - made up to be a bit worse than GPU to same socket GPU, which is a copy of GPU to remote socket

  [INITIATOR_PCIE][TARGET_PCIE][LOCALITY_REMOTE_SOCKET] =
  { 50, 1300, 1065, 400 },   // JDS TODO - made up to be a bit worse than GPU to remote GPU
};
