/** @file

  MCE ARI library

  Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCE_ARI_LIB__
#define __MCE_ARI_LIB__

#include <Uefi/UefiBaseType.h>

#define MCE_ARI_APERTURE_SIZE  0x10000

// Gives the ARI aperture offset from PcdTegraMceAriApertureBaseAddress for
// a given Linear Core Id, both split and locked modes.
#define MCE_ARI_APERTURE_OFFSET(LinearCoreId)    \
  (MCE_ARI_APERTURE_SIZE * (LinearCoreId))

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
  IN  UINT64  *Mpidr,
  OUT UINTN   *DtCpuId
  );

/**
  Fills in bit map of enabled cores

**/
EFI_STATUS
EFIAPI
MceAriGetEnabledCoresBitMap (
  IN  UINT64  *EnabledCoresBitMap
  );

#endif
