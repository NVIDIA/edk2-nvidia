/** @file

  MCE ARI library

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
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

#endif
