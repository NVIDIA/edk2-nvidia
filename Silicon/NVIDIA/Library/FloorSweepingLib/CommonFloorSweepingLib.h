/** @file
*
*  Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
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
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
CommonGetEnabledCoresBitMap (
  IN  UINT32  SocketMask,
  IN  UINTN   MaxSupportedCores,
  IN  UINT64  *EnabledCoresBitMap
  );

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
  IN UINT32        LogicalCore,
  IN OUT UINT64    *Mpidr,
  OUT CONST CHAR8  **DtCpuFormat,
  OUT UINTN        *DtCpuId
  );

#endif // __COMMON_FLOOR_SWEEPING_LIB_H__
