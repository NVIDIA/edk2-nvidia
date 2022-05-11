/** @file

  MCE ARI library

  Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCE_ARI_LIB__
#define __MCE_ARI_LIB__

#include <Uefi/UefiBaseType.h>

#define MCE_ARI_APERTURE_SIZE                   0x10000

// Gives the ARI aperture offset from PcdTegraMceAriApertureBaseAddress for
// a given Linear Core Id, both split and locked modes.
#define MCE_ARI_APERTURE_OFFSET(LinearCoreId)    \
  (MCE_ARI_APERTURE_SIZE * (LinearCoreId))

/**
  Returns the number of CPU cores enabled on the system

  @return       UINT32          Number of CPU cores enabled
**/
UINT32
EFIAPI
MceAriNumCores (
  VOID
  );

/**
  Returns the MCE ARI interface version.

  @return       UINT64          ARI Version: [63:32] Major version,
                                              [31:0] Minor version.
**/
UINT64
EFIAPI
MceAriGetVersion (
  VOID
  );

/**
  Checks to see if the core with the given MPIDR is enabled

  @param[in]    Mpidr           Mpidr of the CPU (Affinity bits only)
  @param[out]   DtCpuId         LinearCoreId of the CPU, if enabled

  @return       EFI_SUCCESS     CPU enabled
  @return       EFI_NOT_FOUND   CPU not enabled
**/
EFI_STATUS
EFIAPI
MceAriCheckCoreEnabled (
  IN  UINT64    *Mpidr,
  OUT UINTN     *DtCpuId
  );

/**
  Returns flag indicating presence of cluster after CPU floorsweeping

  @param[in]    ClusterId       Cluster ID

  @return       TRUE            Cluster is present
  @return       FALSE           Cluster is not present

**/
BOOLEAN
EFIAPI
MceAriClusterIsPresent (
  IN  UINTN ClusterId
  );

/**
  Returns flag indicating presence of a core after CPU floorsweeping

  @param[in]    CoreId          Core ID

  @return       TRUE            Core is present
  @return       FALSE           Core is not present

**/
BOOLEAN
EFIAPI
MceAriCoreIsPresent (
  IN  UINTN     CoreId
  );

/**
  Initiate an SCF level Cache Clean

**/
VOID
EFIAPI
MceAriSCFCacheClean (
  VOID
  );

/**
  Initiate an SCF level Cache Clean Invalidate

**/
VOID
EFIAPI
MceAriSCFCacheCleanInvalidate (
  VOID
  );

#endif
