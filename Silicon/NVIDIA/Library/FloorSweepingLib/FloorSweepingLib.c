/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
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

#include <ArmMpidr.h>
#include <PiDxe.h>

#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/MceAriLib.h>
#include <Library/NvgLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>

// Platform CPU configuration
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))

UINT64
EFIAPI
GetMpidrFromLinearCoreID (
  IN UINT32 LinearCoreId
)
{
  UINTN         Cluster;
  UINTN         Core;
  UINT64        Mpidr;

  Cluster = LinearCoreId / PLATFORM_MAX_CORES_PER_CLUSTER;
  ASSERT (Cluster < PLATFORM_MAX_CLUSTERS);

  Core = LinearCoreId % PLATFORM_MAX_CORES_PER_CLUSTER;
  ASSERT (Core < PLATFORM_MAX_CORES_PER_CLUSTER);

  Mpidr = (UINT64) GET_MPID(Cluster, Core);

  DEBUG ((DEBUG_INFO, "%a:LinearCoreId=%u Cluster=%u, Core=%u, Mpidr=0x%llx \n",
          __FUNCTION__, LinearCoreId , Cluster, Core, Mpidr));

  return Mpidr;
}

EFI_STATUS
EFIAPI
CheckAndRemapCpu (
  IN UINT32         LogicalCore,
  IN OUT UINT64     *Mpidr,
  OUT CONST CHAR8   **DtCpuFormat,
  OUT UINTN         *DtCpuId
  )
{
  UINTN         ChipId;
  EFI_STATUS    Status;

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case T194_CHIP_ID:
      Status = NvgConvertCpuLogicalToMpidr (LogicalCore, Mpidr);
      *Mpidr &= MPIDR_AFFINITY_MASK;
      *DtCpuFormat = "cpu@%x";
      *DtCpuId = *Mpidr;
      break;
    case T234_CHIP_ID:
      Status = MceAriCheckCoreEnabled (Mpidr, DtCpuId);
      *DtCpuFormat = "cpu@%u";
      break;
    default:
      ASSERT (FALSE);
      *Mpidr = 0;
      break;
  }
  DEBUG ((DEBUG_INFO, "%a: ChipId=0x%x, Mpidr=0x%llx Status=%r\n", __FUNCTION__, ChipId, *Mpidr, Status));

  return Status;
}

BOOLEAN
EFIAPI
ClusterIsPresent (
  IN  UINTN ClusterId
  )
{
  UINTN         ChipId;
  BOOLEAN       Present;

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case T194_CHIP_ID:
      Present = NvgClusterIsPresent (ClusterId);
      break;
    case T234_CHIP_ID:
      Present = MceAriClusterIsPresent (ClusterId);
      break;
    default:
      ASSERT (FALSE);
      Present = FALSE;
      break;
  }
  DEBUG ((DEBUG_INFO, "%a: ChipId=0x%x, ClusterId=%u, Present=%d\n",
          __FUNCTION__, ChipId, ClusterId, Present));

  return Present;
}
