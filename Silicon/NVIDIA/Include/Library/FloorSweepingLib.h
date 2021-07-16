/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
*  Portions provided under the following terms:
*  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#ifndef __FLOOR_SWEEPING_LIB_H__
#define __FLOOR_SWEEPING_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Returns number of CPU cores supported on the system

  @return Number of CPU cores

**/
UINT32
GetNumberOfEnabledCpuCores (
  VOID
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
  IN UINT32         LogicalCore,
  IN OUT UINT64     *Mpidr,
  OUT CONST CHAR8   **DtCpuFormat,
  OUT UINTN         *DtCpuId
  );

/**
  Returns flag indicating presence of cluster after CPU floorsweeping

  @param[in]    Cluster         Cluster ID

  @return       TRUE            Cluster is present
  @return       FALSE           Cluster is not present

**/
BOOLEAN
EFIAPI
ClusterIsPresent (
  IN  UINTN ClusterId
  );

#endif //__FLOOR_SWEEPING_LIB_H__
