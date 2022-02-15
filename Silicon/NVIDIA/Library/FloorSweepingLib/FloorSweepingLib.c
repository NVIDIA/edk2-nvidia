/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <ArmMpidr.h>
#include <PiDxe.h>

#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/FloorSweepingInternalLib.h>
#include <Library/MceAriLib.h>
#include <Library/NvgLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>

// Platform CPU configuration
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))

UINT32
EFIAPI
GetClusterIDFromLinearCoreID (
  IN UINT32 LinearCoreId
)
{
  UINT32        Cluster;

  Cluster = LinearCoreId / PLATFORM_MAX_CORES_PER_CLUSTER;
  ASSERT (Cluster < PLATFORM_MAX_CLUSTERS);

  DEBUG ((DEBUG_INFO, "%a:LinearCoreId=%u Cluster=%u\n",
          __FUNCTION__, LinearCoreId , Cluster));

  return Cluster;
}

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

  // Check the Pcd and modify MPIDR generation if required
  if (!PcdGetBool (PcdAffinityMpIdrSupported)) {
    Mpidr = (UINT64) GET_MPID(Cluster, Core);
  } else {
    Mpidr = (UINT64) GET_AFFINITY_BASED_MPID(0, Cluster, Core, 0);
  }

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

/**
  Check if given core is enabled

**/
BOOLEAN
EFIAPI
IsCoreEnabled (
  IN  UINT32  CpuIndex
)
{
  UINTN     ChipId;
  BOOLEAN   ValidPrivatePlatform;
  BOOLEAN   CoreEnabled;

  CoreEnabled = FALSE;

  ValidPrivatePlatform = IsCoreEnabledInternal ( CpuIndex, &CoreEnabled );
  if (ValidPrivatePlatform) {
    return CoreEnabled;
  }

  ChipId = TegraGetChipID ();

  switch (ChipId) {
  case T194_CHIP_ID:
    CoreEnabled = NvgCoreIsPresent (CpuIndex);
    break;
  case T234_CHIP_ID:
    CoreEnabled = MceAriCoreIsPresent (CpuIndex);
    break;
    default:
      CoreEnabled = FALSE;
      ASSERT (FALSE);
      break;
  }

  return CoreEnabled;
}


/**
  Retrieve number of enabled CPUs for each platform

**/
UINT32
GetNumberOfEnabledCpuCores (
  VOID
)
{
  UINT32    Count;
  UINTN     ChipId;
  BOOLEAN   ValidPrivatePlatform;

  Count = 0;

  ValidPrivatePlatform = GetNumberOfEnabledCpuCoresInternal ( &Count );
  if (ValidPrivatePlatform) {
    return Count;
  }

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case T194_CHIP_ID:
      Count = NvgGetNumberOfEnabledCpuCores ();
      break;
    case T234_CHIP_ID:
      Count = MceAriNumCores ();
      break;
    default:
      ASSERT (FALSE);
      Count = 1;
      break;
  }

  return Count;
}

/**
  Retrieve number of enabled sockets for each platform

**/
UINT32
GetNumberOfEnabledSockets (
  VOID
)
{
  UINT32    Count;
  UINTN     ChipId;
  BOOLEAN   ValidPrivatePlatform;

  Count = 0;

  ValidPrivatePlatform = GetNumberOfEnabledSocketsInternal ( &Count );
  if (ValidPrivatePlatform) {
    return Count;
  }

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case T194_CHIP_ID:
      Count = 1;
      break;
    case T234_CHIP_ID:
      Count = 1;
      break;
    default:
      ASSERT (FALSE);
      Count = 1;
      break;
  }

  return Count;
}
