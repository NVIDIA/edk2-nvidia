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
#include <Library/HobLib.h>
#include <Library/MceAriLib.h>
#include <Library/NvgLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>

// Platform CPU configuration
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))
#define PLATFORM_MAX_SOCKETS            (PcdGet32 (PcdTegraMaxSockets))
#define PLATFORM_MAX_CORES_PER_SOCKET   ((PLATFORM_MAX_CLUSTERS /   \
                                          PLATFORM_MAX_SOCKETS) *   \
                                         PLATFORM_MAX_CORES_PER_CLUSTER)

#define MAX_SUPPORTED_CORES             1024

typedef struct {
  UINTN                 MaxClusters;
  UINTN                 MaxCoresPerCluster;
  UINTN                 MaxCores;

  UINTN                 EnabledSockets;
  UINTN                 EnabledCores;

  UINT64                EnabledCoresBitMap[ALIGN_VALUE (MAX_SUPPORTED_CORES, 64) / 64];

} PLATFORM_CPU_INFO;

STATIC PLATFORM_CPU_INFO mCpuInfo = {0};

/**
  Get CPU info for a platform

**/
STATIC
EFI_STATUS
EFIAPI
GetCpuInfo (
  IN  UINTN     EnabledSockets,
  IN  UINTN     MaxSupportedCores,
  OUT UINT64    *EnabledCoresBitMap
  )
{
  BOOLEAN       ValidPrivatePlatform;
  EFI_STATUS    Status;

  Status = EFI_SUCCESS;
  ValidPrivatePlatform = GetCpuInfoInternal (EnabledSockets,
                                             MaxSupportedCores,
                                             EnabledCoresBitMap);
  if (!ValidPrivatePlatform) {
    UINTN     ChipId;

    ChipId = TegraGetChipID ();

    switch (ChipId) {
      case T194_CHIP_ID:
        Status = NvgGetEnabledCoresBitMap (EnabledCoresBitMap);
        break;
      case T234_CHIP_ID:
        Status = MceAriGetEnabledCoresBitMap (EnabledCoresBitMap);
        break;
      default:
        ASSERT (FALSE);
        Status = EFI_UNSUPPORTED;
        break;
    }
  }

  return Status;
}

/**
  Get floor sweep cpu info

**/
STATIC
PLATFORM_CPU_INFO *
EFIAPI
FloorSweepCpuInfo (
  VOID
  )
{
  static BOOLEAN    InfoFilled = FALSE;
  PLATFORM_CPU_INFO *Info;
  UINTN             Core;
  EFI_STATUS        Status;

  Info = &mCpuInfo;
  if (!InfoFilled) {
    VOID            *Hob;

    Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
    if ((Hob != NULL) &&
        (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO))) {
      Info->EnabledSockets = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->NumSockets;
    } else {
      ASSERT (FALSE);
      Info->EnabledSockets = 1;
    }

    Info->MaxClusters = PLATFORM_MAX_CLUSTERS;
    Info->MaxCoresPerCluster = PLATFORM_MAX_CORES_PER_CLUSTER;
    Status = GetCpuInfo (Info->EnabledSockets,
                         MAX_SUPPORTED_CORES,
                         Info->EnabledCoresBitMap);
    if (EFI_ERROR (Status)) {
      ASSERT (FALSE);

      Info->MaxClusters = 1;
      Info->MaxCoresPerCluster = 1;
      Info->EnabledCoresBitMap[0] = 0x1;
    }

    Info->MaxCores = Info->MaxCoresPerCluster * Info->MaxClusters;

    Info->EnabledCores = 0;
    for (Core = 0; Core < Info->MaxCores; Core++) {
      UINTN Index   = Core / 64;
      UINTN Bit     = Core % 64;

      if (Info->EnabledCoresBitMap[Index] & (1ULL << Bit)) {
        Info->EnabledCores++;
      }
    }

    DEBUG ((DEBUG_INFO, "%a: MaxClusters=%u MaxCoresPerCluster=%u MaxCores=%u\n",
            __FUNCTION__,  Info->MaxClusters, Info->MaxCoresPerCluster,
            Info->MaxCores));
    DEBUG ((DEBUG_INFO, "%a: EnabledSockets=%u EnabledCores=%u\n",
            __FUNCTION__, Info->EnabledSockets, Info->EnabledCores));
    DEBUG_CODE (
      for (Core = 0; Core < Info->MaxCores; Core += 64) {
        UINTN Index = Core / 64;

        DEBUG ((DEBUG_INFO, "EnabledCoresBitMap[%u]=0x%016llx ",
                Index, Info->EnabledCoresBitMap[Index]));
      }
      DEBUG ((DEBUG_INFO, "\n"));
      );

    InfoFilled = TRUE;
  }

  return Info;
}

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
  UINTN         Socket;
  UINTN         Cluster;
  UINTN         Core;
  UINT64        Mpidr;
  UINT32        SocketCoreId;

  Socket = LinearCoreId / PLATFORM_MAX_CORES_PER_SOCKET;
  ASSERT (Socket < PLATFORM_MAX_SOCKETS);

  SocketCoreId = LinearCoreId - (Socket * PLATFORM_MAX_CORES_PER_SOCKET);

  Cluster = SocketCoreId / PLATFORM_MAX_CORES_PER_CLUSTER;
  ASSERT (Cluster < PLATFORM_MAX_CLUSTERS);

  Core = SocketCoreId % PLATFORM_MAX_CORES_PER_CLUSTER;
  ASSERT (Core < PLATFORM_MAX_CORES_PER_CLUSTER);

  // Check the Pcd and modify MPIDR generation if required
  if (!PcdGetBool (PcdAffinityMpIdrSupported)) {
    ASSERT (Socket == 0);
    Mpidr = (UINT64) GET_MPID(Cluster, Core);
  } else {
    Mpidr = (UINT64) GET_AFFINITY_BASED_MPID(Socket, Cluster, Core, 0);
  }

  DEBUG ((DEBUG_INFO, "%a:LinearCoreId=%u Socket=%u Cluster=%u, Core=%u, Mpidr=0x%llx \n",
          __FUNCTION__, LinearCoreId, Socket, Cluster, Core, Mpidr));

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
  BOOLEAN       ValidPrivatePlatform;
  UINTN         ChipId;
  EFI_STATUS    Status;

  ValidPrivatePlatform = CheckAndRemapCpuInternal (LogicalCore,
                                                   Mpidr,
                                                   DtCpuFormat,
                                                   DtCpuId,
                                                   &Status);
  if (ValidPrivatePlatform) {
    return Status;
  }

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
      Status = EFI_UNSUPPORTED;
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
  PLATFORM_CPU_INFO *Info;
  UINTN             Core;

  Info = FloorSweepCpuInfo ();

  for (Core = 0; Core < Info->MaxCoresPerCluster; Core++) {
    if (IsCoreEnabled (Core + (ClusterId * Info->MaxCoresPerCluster))) {
      return TRUE;
    }
  }

  return FALSE;
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
  PLATFORM_CPU_INFO *Info;
  UINTN             Index;
  UINTN             Bit;

  Info = FloorSweepCpuInfo ();

  Index = CpuIndex / 64;
  Bit   = CpuIndex % 64;

  return ((Info->EnabledCoresBitMap[Index] & (1ULL << Bit)) != 0);
}

/**
  Retrieve number of enabled CPUs for each platform

**/
UINT32
EFIAPI
GetNumberOfEnabledCpuCores (
  VOID
)
{
  return (UINT32) FloorSweepCpuInfo ()->EnabledCores;
}

/**
  Floorsweep CPUs in DTB

**/
EFI_STATUS
EFIAPI
UpdateCpuFloorsweepingConfig (
  IN INT32 CpusOffset,
  IN VOID *Dtb
)
{
  UINTN Cpu;
  UINT32 Cluster;
  UINT64 Mpidr;
  INT32 CpuMapOffset;
  INT32 FdtErr;
  UINT64 Tmp64;
  UINT32 Tmp32;
  CHAR8  CpuNodeStr[] = "cpu@ffffffff";
  CHAR8  ClusterNodeStr[] = "cluster10";
  UINT32 AddressCells;
  INT32 NodeOffset;
  INT32 PrevNodeOffset;

  AddressCells  = fdt_address_cells (Dtb, CpusOffset);

  /* Update the correct MPIDR and enable the DT nodes of each enabled CPU;
   * disable the DT nodes of the floorswept cores.*/
  NodeOffset = 0;
  Cpu = 0;
  PrevNodeOffset = 0;
  for (NodeOffset = fdt_first_subnode(Dtb, CpusOffset);
       NodeOffset > 0;
       NodeOffset = fdt_next_subnode(Dtb, PrevNodeOffset)) {
    CONST VOID  *Property;
    INT32       Length;
    EFI_STATUS  Status;
    UINTN       DtCpuId;
    CONST CHAR8 *DtCpuFormat;

    Property = fdt_getprop(Dtb, NodeOffset, "device_type", &Length);
    if ((Property == NULL) || (AsciiStrCmp(Property, "cpu") != 0)) {
      PrevNodeOffset = NodeOffset;
      continue;
    }

    // retrieve mpidr for this cpu node
    Property = fdt_getprop (Dtb, NodeOffset, "reg", &Length);
    if ((Property == NULL) || ((Length != sizeof (Tmp64)) && (Length != sizeof (Tmp32)))) {
      DEBUG ((DEBUG_ERROR, "Failed to get MPIDR for /cpus/%a, len=%u\n",
              fdt_get_name (Dtb, NodeOffset, NULL), Length));
      return EFI_DEVICE_ERROR;
    }
    if (Length == sizeof (Tmp64)) {
      Tmp64 = *(CONST UINT64 *)Property;
      Mpidr = fdt64_to_cpu (Tmp64);
    } else {
      Tmp32 = *(CONST UINT32 *)Property;
      Mpidr = fdt32_to_cpu (Tmp32);
    }

    Status = CheckAndRemapCpu (Cpu, &Mpidr, &DtCpuFormat, &DtCpuId);
    if (!EFI_ERROR (Status)) {
      AsciiSPrint (CpuNodeStr, sizeof (CpuNodeStr), DtCpuFormat, DtCpuId);
      FdtErr = fdt_set_name (Dtb, NodeOffset, CpuNodeStr);
      if (FdtErr < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to set name to %a: %a\r\n", CpuNodeStr, fdt_strerror (FdtErr)));
        return EFI_DEVICE_ERROR;
      }

      if (AddressCells == 2) {
        Tmp64 = cpu_to_fdt64 ((UINT64)Mpidr);
        FdtErr = fdt_setprop (Dtb, NodeOffset, "reg", &Tmp64, sizeof(Tmp64));
      } else {
        Tmp32 = cpu_to_fdt32 ((UINT32)Mpidr);
        FdtErr = fdt_setprop (Dtb, NodeOffset, "reg", &Tmp32, sizeof(Tmp32));
      }
      if (FdtErr < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to add MPIDR to /cpus/%a/reg: %a\r\n", CpuNodeStr, fdt_strerror(FdtErr)));
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((DEBUG_INFO, "Enabled %a, index=%u, (mpidr: 0x%llx) node in FDT\r\n",
              CpuNodeStr, Cpu, Mpidr));
      PrevNodeOffset = NodeOffset;
    } else {
      FdtErr = fdt_del_node(Dtb, NodeOffset);
      if (FdtErr < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to delete /cpus/cpu@%u node: %a\r\n", Cpu, fdt_strerror(FdtErr)));
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((DEBUG_INFO, "Deleted cpu-%u node in FDT\r\n", Cpu));
    }
    Cpu++;
  }

  CpuMapOffset = fdt_subnode_offset(Dtb, CpusOffset, "cpu-map");

  if (CpuMapOffset < 0) {
    DEBUG ((DEBUG_ERROR, "/cpus/cpu-map does not exist\r\n"));
    return EFI_DEVICE_ERROR;
  }

  Cluster = 0;
  while (TRUE) {
    AsciiSPrint (ClusterNodeStr, sizeof (ClusterNodeStr),"cluster%u", Cluster);
    NodeOffset = fdt_subnode_offset(Dtb, CpuMapOffset, ClusterNodeStr);
    if (NodeOffset >= 0) {
      if (!ClusterIsPresent (Cluster)) {
        FdtErr = fdt_del_node(Dtb, NodeOffset);
        if (FdtErr < 0) {
          DEBUG ((DEBUG_ERROR, "Failed to delete /cpus/cpu-map/%a node: %a\r\n",
                  ClusterNodeStr, fdt_strerror(FdtErr)));
          return EFI_DEVICE_ERROR;
        }
        DEBUG ((DEBUG_INFO, "Deleted cluster%u node in FDT\r\n", Cluster));
      } else {
        INT32       ClusterCpuNodeOffset;
        INT32       PrevClusterCpuNodeOffset;
        CONST VOID  *Property;
        CONST CHAR8 *NodeName;
        PrevClusterCpuNodeOffset = 0;
        fdt_for_each_subnode (ClusterCpuNodeOffset, Dtb, NodeOffset) {
          Property = fdt_getprop(Dtb, ClusterCpuNodeOffset, "cpu", NULL);
          if (Property != NULL) {
            if (fdt_node_offset_by_phandle (Dtb, fdt32_to_cpu(*(UINT32 *)Property)) < 0) {
              NodeName = fdt_get_name (Dtb, ClusterCpuNodeOffset, NULL);
              FdtErr = fdt_del_node(Dtb, ClusterCpuNodeOffset);
              if (FdtErr < 0) {
                DEBUG ((DEBUG_ERROR, "Failed to delete /cpus/cpu-map/%a/%a node: %a\r\n",
                        ClusterNodeStr, NodeName, fdt_strerror(FdtErr)));
                return EFI_DEVICE_ERROR;
              }
              ClusterCpuNodeOffset = PrevClusterCpuNodeOffset;
            }
          }
          PrevClusterCpuNodeOffset = ClusterCpuNodeOffset;
        }
      }
      Cluster++;
    } else {
      break;
    }
  }

  return EFI_SUCCESS;
}

/**
  Floorsweep global cpus

**/
EFI_STATUS
EFIAPI
FloorSweepGlobalCpus (
  IN VOID *Dtb
  )
{
  INT32                 CpusOffset;

  CpusOffset = fdt_path_offset (Dtb, "/cpus");
  if (CpusOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /cpus node\n"));
    return EFI_DEVICE_ERROR;
  }

  return UpdateCpuFloorsweepingConfig (CpusOffset, Dtb);
}

/**
  Floorsweep sockets

**/
EFI_STATUS
EFIAPI
FloorSweepSockets (
  IN  UINTN     EnabledSockets,
  IN  VOID      *Dtb
  )
{
  CHAR8                         SocketNodeStr[] = "/socket@xx";
  UINT32                        NumSockets;
  UINT32                        MaxSockets;
  INT32                         NodeOffset;

  NumSockets = 0;
  MaxSockets = 0;

  while (TRUE) {
    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr),"/socket@%u", MaxSockets);
    NodeOffset = fdt_path_offset (Dtb, SocketNodeStr);
    if (NodeOffset < 0) {
      break;
    } else {
      MaxSockets++;
    }

    ASSERT (MaxSockets < 100);      // enforce socket@xx string max
  }

  if (MaxSockets == 0) {
    MaxSockets = 1;
  }

  for (UINT32 Count = EnabledSockets; Count < MaxSockets; Count++) {
    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u", Count);
    NodeOffset = fdt_path_offset (Dtb, SocketNodeStr);
    if (NodeOffset >= 0) {
      DEBUG ((DEBUG_INFO, "Deleting socket@%u node\n", Count));
      fdt_del_node (Dtb, NodeOffset);
    }
  }

  return EFI_SUCCESS;
}

/**
  Floorsweep DTB

**/
EFI_STATUS
EFIAPI
FloorSweepDtb (
  IN VOID *Dtb
  )
{
  BOOLEAN               ValidPrivatePlatform;
  PLATFORM_CPU_INFO     *Info;
  UINTN                 ChipId;
  EFI_STATUS            Status;

  Info = FloorSweepCpuInfo ();
  FloorSweepSockets (Info->EnabledSockets, Dtb);

  ValidPrivatePlatform = FloorSweepDtbInternal (Info->EnabledSockets, Dtb);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipId = TegraGetChipID ();

  switch (ChipId) {
    case T194_CHIP_ID:
    case T234_CHIP_ID:
      Status = FloorSweepGlobalCpus (Dtb);
      break;
    default:
      Status = EFI_UNSUPPORTED;
      break;
  }

  return Status;
}
