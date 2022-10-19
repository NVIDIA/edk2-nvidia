/** @file
*
*  Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
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

#include <Uefi/UefiBaseType.h>
#include <TH500/TH500Definitions.h>

// Platform CPU configuration
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))
#define PLATFORM_MAX_SOCKETS            (PcdGet32 (PcdTegraMaxSockets))
#define PLATFORM_MAX_CORES_PER_SOCKET   ((PLATFORM_MAX_CLUSTERS /   \
                                          PLATFORM_MAX_SOCKETS) *   \
                                         PLATFORM_MAX_CORES_PER_CLUSTER)

// Platform CPU Floorsweeping scratch offsets from TH500_SCRATCH_BASE_SOCKET_X
#define CPU_FLOORSWEEPING_DISABLE_OFFSET_0  0x78
#define CPU_FLOORSWEEPING_DISABLE_OFFSET_1  0x7C
#define CPU_FLOORSWEEPING_DISABLE_OFFSET_2  0x80

// Platform CPU Floorsweeping scratch masks
#define CPU_FLOORSWEEPING_DISABLE_MASK_0  0x00000000
#define CPU_FLOORSWEEPING_DISABLE_MASK_1  0x00000000
#define CPU_FLOORSWEEPING_DISABLE_MASK_2  0xFFF00000

// PCI-e floorsweeping scratch offset from TH500_SCRATCH_BASE_SOCKET_X
#define PCIE_FLOORSWEEPING_DISABLE_OFFSET  0x74

// Platform PCI-e floorsweeping scratch masks
#define PCIE_SIM_FLOORSWEEPING_INFO      0x1F3
#define PCIE_FPGA_FLOORSWEEPING_INFO     0x2FF
#define PCIE_FLOORSWEEPING_DISABLE_MASK  0xFFFFFC00

#define PCIE_ID_TO_SOCKET(PcieId)     ((PcieId) >> 4)
#define PCIE_ID_TO_INTERFACE(PcieId)  ((PcieId) & 0xfUL)

// SCF Cache floorsweeping scratch offsets from TH500_SCRATCH_BASE_SOCKET_X
#define SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_0  0x8C
#define SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_1  0x90
#define SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_2  0x94

// Platform SCF Cache floorsweeping scratch masks
#define SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_0  0x00000000
#define SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_1  0x00000000
#define SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_2  0xFFF00000

#define SCF_CACHE_SLICE_SIZE  (SIZE_1MB + SIZE_512KB)
#define SCF_CACHE_SLICE_SETS  2048

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
TH500GetEnabledCoresBitMap (
  IN  UINT32  SocketMask,
  IN  UINTN   MaxSupportedCores,
  IN  UINT64  *EnabledCoresBitMap
  );

/**
  Floorsweep PCIe

**/
EFI_STATUS
EFIAPI
TH500FloorSweepPcie (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  );

/**
  Floorsweep ScfCache

**/
EFI_STATUS
EFIAPI
TH500FloorSweepScfCache (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  );

/**
  Floorsweep Cpus

**/
EFI_STATUS
EFIAPI
TH500FloorSweepCpus (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  );

EFI_STATUS
EFIAPI
TH500CheckAndRemapCpu (
  IN UINT32        LogicalCore,
  IN OUT UINT64    *Mpidr,
  OUT CONST CHAR8  **DtCpuFormat,
  OUT UINTN        *DtCpuId
  );
