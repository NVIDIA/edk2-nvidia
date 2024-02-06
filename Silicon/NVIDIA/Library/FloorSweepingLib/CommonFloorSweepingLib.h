/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __COMMON_FLOOR_SWEEPING_LIB_H__
#define __COMMON_FLOOR_SWEEPING_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <TH500/TH500Definitions.h>

// Platform CPU configuration
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))
#define PLATFORM_MAX_SOCKETS            (PcdGet32 (PcdTegraMaxSockets))
#define PLATFORM_MAX_CORES_PER_SOCKET   ((PLATFORM_MAX_CLUSTERS /   \
                                          PLATFORM_MAX_SOCKETS) *   \
                                         PLATFORM_MAX_CORES_PER_CLUSTER)

/**
  Floorsweep PCIe

**/
EFI_STATUS
EFIAPI
CommonFloorSweepPcie (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  );

/**
  Floorsweep ScfCache

**/
EFI_STATUS
EFIAPI
CommonFloorSweepScfCache (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  );

/**
  Floorsweep Cpus

**/
EFI_STATUS
EFIAPI
CommonFloorSweepCpus (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  );

EFI_STATUS
EFIAPI
CommonCheckAndRemapCpu (
  IN UINT32      LogicalCore,
  IN OUT UINT64  *Mpidr
  );

#endif // __COMMON_FLOOR_SWEEPING_LIB_H__
