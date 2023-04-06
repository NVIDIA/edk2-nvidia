/** @file
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __FLOOR_SWEEPING_LIB_H__
#define __FLOOR_SWEEPING_LIB_H__

#include <Uefi/UefiBaseType.h>

#define GET_AFFINITY_BASED_MPID(Aff3, Aff2, Aff1, Aff0)         \
  (((UINT64)(Aff3) << 32) | ((Aff2) << 16) | ((Aff1) << 8) | (Aff0))

/**
  Returns the Cluster ID given the Linear Core ID

  @param[in]    UINT32          Linear Core ID

  @return       UINT32          Cluster ID
**/
UINT32
EFIAPI
GetClusterIDFromLinearCoreID (
  IN UINT32  LinearCoreId
  );

/**
  Returns the MPIDR given the Linear Core ID

  @param[in]    UINT32          Linear Core ID

  @return       UINT64          Mpidr
**/
UINT64
EFIAPI
GetMpidrFromLinearCoreID (
  IN UINT32  LinearCoreId
  );

/**
  Checks if CPU is enabled and remaps MPIDR for Device Tree, if needed.
  MPIDR for Device Tree only has affinity bits.

  @param[in]     LogicalCore        Logical CPU core ID
  @param[in/out] Mpidr              In: MPIDR from cpu DT node
                                    Out: MPIDR to use in cpu DT node
  @param[out]    DtCpuFormat        Format specification string for DT cpu label
  @param[out]    DtCpuId            Dt Cpu Id value to print using DtCpuFormat

  @return       EFI_SUCCESS         CPU enabled and other values returned
  @return       EFI_NOT_FOUND       CPU not enabled

**/
EFI_STATUS
EFIAPI
CheckAndRemapCpu (
  IN UINT32        LogicalCore,
  IN OUT UINT64    *Mpidr,
  OUT CONST CHAR8  **DtCpuFormat,
  OUT UINTN        *DtCpuId
  );

/**
  Returns flag indicating presence of cluster after CPU floorsweeping

  @param[in]    Socket          Socket number
  @param[in]    Cluster         Cluster ID

  @return       TRUE            Cluster is present
  @return       FALSE           Cluster is not present

**/
BOOLEAN
EFIAPI
ClusterIsPresent (
  IN  UINTN  Socket,
  IN  UINTN  ClusterId
  );

/**
  Check if given socket is enabled

**/
BOOLEAN
EFIAPI
IsSocketEnabled (
  IN UINT32  SocketIndex
  );

/**
  Check if given core is enabled

**/
BOOLEAN
EFIAPI
IsCoreEnabled (
  IN  UINT32  CpuIndex
  );

/**
  Retrieve number of enabled CPUs for each platform

**/
UINT32
EFIAPI
GetNumberOfEnabledCpuCores (
  VOID
  );

/**
  Floorsweep DTB

**/
EFI_STATUS
EFIAPI
FloorSweepDtb (
  IN VOID  *Dtb
  );

/**
  Get First Enabled Core on Socket.

**/
EFI_STATUS
EFIAPI
GetFirstEnabledCoreOnSocket (
  IN   UINTN  Socket,
  OUT  UINTN  *LinearCoreId
  );

/**
  Get Number of Enabled Cores on Socket.

**/
EFI_STATUS
EFIAPI
GetNumEnabledCoresOnSocket (
  IN   UINTN  Socket,
  OUT  UINTN  *NumEnabledCores
  );

#endif //__FLOOR_SWEEPING_LIB_H__
