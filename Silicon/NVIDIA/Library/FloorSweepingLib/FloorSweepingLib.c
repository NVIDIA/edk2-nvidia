/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <Library/HobLib.h>
#include <Library/MceAriLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NvgLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>

#include "CommonFloorSweepingLib.h"

#define THERMAL_COOLING_DEVICE_ENTRY_SIZE  (3 * sizeof (INT32))

// Platform CPU configuration
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))
#define PLATFORM_MAX_SOCKETS            (PcdGet32 (PcdTegraMaxSockets))
#define PLATFORM_MAX_CORES_PER_SOCKET   ((PLATFORM_MAX_CLUSTERS /   \
                                          PLATFORM_MAX_SOCKETS) *   \
                                         PLATFORM_MAX_CORES_PER_CLUSTER)

typedef struct {
  UINTN     MaxClusters;
  UINTN     MaxCoresPerCluster;
  UINTN     MaxCores;

  UINT32    SocketMask;
  UINTN     EnabledCores;
  UINT64    EnabledCoresBitMap[ALIGN_VALUE (MAX_SUPPORTED_CORES, 64) / 64];
} PLATFORM_CPU_INFO;

STATIC PLATFORM_CPU_INFO                   mCpuInfo       = { 0 };
STATIC CONST TEGRA_PLATFORM_RESOURCE_INFO  *mResourceInfo = NULL;

EFI_STATUS
EFIAPI
TH500FloorSweepCpus (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  );

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
  static BOOLEAN     InfoFilled = FALSE;
  PLATFORM_CPU_INFO  *Info;
  UINTN              Core;

  Info = &mCpuInfo;
  if (!InfoFilled) {
    VOID  *Hob;

    Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
    if ((Hob != NULL) &&
        (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
    {
      mResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
    }

    NV_ASSERT_RETURN ((mResourceInfo != NULL), return NULL, "%a: no resource info!\n", __FUNCTION__);

    Info->MaxClusters        = mResourceInfo->MaxPossibleClusters;
    Info->MaxCoresPerCluster = mResourceInfo->MaxPossibleCoresPerCluster;
    Info->MaxCores           = mResourceInfo->MaxPossibleCores;

    Info->SocketMask   = mResourceInfo->SocketMask;
    Info->EnabledCores = mResourceInfo->NumberOfEnabledCores;
    CopyMem (Info->EnabledCoresBitMap, mResourceInfo->EnabledCoresBitMap, sizeof (Info->EnabledCoresBitMap));

    DEBUG ((
      DEBUG_INFO,
      "%a: MaxClusters=%u MaxCoresPerCluster=%u MaxCores=%u\n",
      __FUNCTION__,
      Info->MaxClusters,
      Info->MaxCoresPerCluster,
      Info->MaxCores
      ));
    DEBUG ((
      DEBUG_INFO,
      "%a: SocketMask=0x%x EnabledCores=%u\n",
      __FUNCTION__,
      Info->SocketMask,
      Info->EnabledCores
      ));
    DEBUG_CODE (
      for (Core = 0; Core < Info->MaxCores; Core += 64) {
      UINTN Index = Core / 64;

      DEBUG ((
        DEBUG_INFO,
        "EnabledCoresBitMap[%u]=0x%016llx ",
        Index,
        Info->EnabledCoresBitMap[Index]
        ));
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
  IN UINT32  LinearCoreId
  )
{
  UINT32  Cluster;

  Cluster = LinearCoreId / PLATFORM_MAX_CORES_PER_CLUSTER;
  ASSERT (Cluster < PLATFORM_MAX_CLUSTERS);

  DEBUG ((
    DEBUG_INFO,
    "%a:LinearCoreId=%u Cluster=%u\n",
    __FUNCTION__,
    LinearCoreId,
    Cluster
    ));

  return Cluster;
}

UINT64
EFIAPI
GetMpidrFromLinearCoreID (
  IN UINT32  LinearCoreId
  )
{
  UINTN   Socket;
  UINTN   Cluster;
  UINTN   Core;
  UINT64  Mpidr;
  UINT32  SocketCoreId;

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
    Mpidr = (UINT64)GET_MPID (Cluster, Core);
  } else {
    Mpidr = (UINT64)GET_AFFINITY_BASED_MPID (Socket, Cluster, Core, 0);
  }

  DEBUG ((
    DEBUG_INFO,
    "%a:LinearCoreId=%u Socket=%u Cluster=%u, Core=%u, Mpidr=0x%llx \n",
    __FUNCTION__,
    LinearCoreId,
    Socket,
    Cluster,
    Core,
    Mpidr
    ));

  return Mpidr;
}

BOOLEAN
EFIAPI
ClusterIsPresent (
  IN  UINTN  Socket,
  IN  UINTN  ClusterId
  )
{
  PLATFORM_CPU_INFO  *Info;
  UINTN              Core;
  UINT32             SocketCoreStart;

  Info            = FloorSweepCpuInfo ();
  SocketCoreStart = Socket * PLATFORM_MAX_CORES_PER_SOCKET;

  for (Core = 0; Core < Info->MaxCoresPerCluster; Core++) {
    if (IsCoreEnabled (SocketCoreStart + Core + (ClusterId * Info->MaxCoresPerCluster))) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Check if given socket is enabled

**/
BOOLEAN
EFIAPI
IsSocketEnabled (
  IN UINT32  SocketIndex
  )
{
  PLATFORM_CPU_INFO  *Info;

  Info = FloorSweepCpuInfo ();

  return ((Info->SocketMask & (1UL << SocketIndex)) != 0);
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
  PLATFORM_CPU_INFO  *Info;
  UINTN              Index;
  UINTN              Bit;

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
  return (UINT32)FloorSweepCpuInfo ()->EnabledCores;
}

/**
  Rename cpu-map child nodes sequentially starting at 0 to meet DTB spec.

 **/
STATIC
EFI_STATUS
EFIAPI
RenameChildNodesSequentially (
  IN VOID         *Dtb,
  IN INT32        NodeOffset,
  IN CONST CHAR8  *ChildNameFormat,
  IN CHAR8        *ChildNameStr,
  IN UINTN        ChildNameStrSize,
  IN UINTN        MaxChildNodes
  )
{
  INT32        ChildOffset;
  UINT32       ChildIndex;
  CONST CHAR8  *DtbChildName;
  INT32        FdtErr;

  ChildIndex = 0;
  fdt_for_each_subnode (ChildOffset, Dtb, NodeOffset) {
    NV_ASSERT_RETURN ((ChildIndex < MaxChildNodes), return EFI_UNSUPPORTED, "%a: hit max nodes=%u for %a\n", __FUNCTION__, MaxChildNodes, ChildNameFormat);

    AsciiSPrint (ChildNameStr, ChildNameStrSize, ChildNameFormat, ChildIndex);
    DtbChildName = fdt_get_name (Dtb, ChildOffset, NULL);

    DEBUG ((DEBUG_INFO, "%a: Checking %a==%a (%u)\n", __FUNCTION__, ChildNameStr, DtbChildName, ChildIndex));
    if (AsciiStrCmp (ChildNameStr, DtbChildName) != 0) {
      FdtErr = fdt_set_name (Dtb, ChildOffset, ChildNameStr);
      NV_ASSERT_RETURN ((FdtErr >= 0), return EFI_DEVICE_ERROR, "%a: Failed to update name %a: %a\n", __FUNCTION__, ChildNameStr, fdt_strerror (FdtErr));

      DEBUG ((DEBUG_INFO, "%a: Updated %a\n", __FUNCTION__, ChildNameStr));
    }

    ChildIndex++;
  }

  return EFI_SUCCESS;
}

/**
  Detect if given phandle is referenced as next-level-cache by any cpu node

**/
STATIC
BOOLEAN
EFIAPI
PhandleIsNextLevelCache (
  IN VOID    *Dtb,
  IN INT32   CpusOffset,
  IN UINT32  Phandle,
  IN UINTN   Level
  )
{
  INT32       NodeOffset;
  CONST VOID  *Property;
  UINT32      L2CachePhandle;
  UINT32      L3CachePhandle;
  INT32       L2NodeOffset;

  fdt_for_each_subnode (NodeOffset, Dtb, CpusOffset) {
    Property = fdt_getprop (Dtb, NodeOffset, "device_type", NULL);
    if ((Property == NULL) || (AsciiStrCmp (Property, "cpu") != 0)) {
      continue;
    }

    Property = fdt_getprop (Dtb, NodeOffset, "status", NULL);
    if ((Property != NULL) && (AsciiStrCmp (Property, "fail") == 0)) {
      continue;
    }

    // L2
    Property = fdt_getprop (Dtb, NodeOffset, "next-level-cache", NULL);
    if (Property == NULL) {
      continue;
    }

    L2CachePhandle = fdt32_to_cpu (*(UINT32 *)Property);
    DEBUG ((DEBUG_VERBOSE, "%a: checking phandle 0x%x for 0x%x\r\n", __FUNCTION__, L2CachePhandle, Phandle));
    if (L2CachePhandle == Phandle) {
      return TRUE;
    }

    if (Level < 3) {
      continue;
    }

    // L3
    L2NodeOffset = fdt_node_offset_by_phandle (Dtb, L2CachePhandle);
    if (L2NodeOffset < 0) {
      DEBUG ((DEBUG_ERROR, "%a: no l2 at phandle=0x%x\n", __FUNCTION__, L2CachePhandle));
      continue;
    }

    Property = fdt_getprop (Dtb, L2NodeOffset, "next-level-cache", NULL);
    if (Property == NULL) {
      continue;
    }

    L3CachePhandle = fdt32_to_cpu (*(UINT32 *)Property);
    DEBUG ((DEBUG_VERBOSE, "%a: checking l3 phandle 0x%x for 0x%x\r\n", __FUNCTION__, L3CachePhandle, Phandle));
    if (L3CachePhandle == Phandle) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Floorsweep CPUs in DTB

**/
EFI_STATUS
EFIAPI
UpdateCpuFloorsweepingConfig (
  IN UINT32  SocketMask,
  IN INT32   CpusOffset,
  IN VOID    *Dtb
  )
{
  UINTN       Cpu;
  UINT32      Cluster;
  UINT64      Mpidr;
  INT32       CpuMapOffset;
  INT32       FdtErr;
  UINT64      Tmp64;
  UINT32      Tmp32;
  CHAR8       ClusterNodeStr[] = "cluster10";
  INT32       NodeOffset;
  INT32       TmpOffset;
  CHAR8       CoreNodeStr[] = "coreXX";
  EFI_STATUS  Status;
  CHAR8       SocketNodeStr[] = "socketXX";
  INT32       CpuMapSocketOffset;
  UINTN       Socket;
  BOOLEAN     HasSocketNode;
  UINT32      L2CachePhandle;
  UINT32      L3CachePhandle;

  Cpu        = 0;
  NodeOffset = fdt_first_subnode (Dtb, CpusOffset);
  while (NodeOffset > 0) {
    CONST VOID  *Property;
    INT32       Length;

    Property = fdt_getprop (Dtb, NodeOffset, "device_type", &Length);
    if ((Property == NULL) || (AsciiStrCmp (Property, "cpu") != 0)) {
      NodeOffset = fdt_next_subnode (Dtb, NodeOffset);
      continue;
    }

    // retrieve mpidr for this cpu node
    Property = fdt_getprop (Dtb, NodeOffset, "reg", &Length);
    if ((Property == NULL) || ((Length != sizeof (Tmp64)) && (Length != sizeof (Tmp32)))) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed to get MPIDR for /cpus/%a, len=%d\n",
        fdt_get_name (Dtb, NodeOffset, NULL),
        Length
        ));
      return EFI_DEVICE_ERROR;
    }

    if (Length == sizeof (Tmp64)) {
      Tmp64 = *(CONST UINT64 *)Property;
      Mpidr = fdt64_to_cpu (Tmp64);
    } else {
      Tmp32 = *(CONST UINT32 *)Property;
      Mpidr = fdt32_to_cpu (Tmp32);
    }

    if (IsMpidrEnabled (Mpidr)) {
      NodeOffset = fdt_next_subnode (Dtb, NodeOffset);
    } else {
      Property = fdt_getprop (Dtb, NodeOffset, "next-level-cache", NULL);
      if (Property != NULL) {
        L2CachePhandle = fdt32_to_cpu (*(UINT32 *)Property);
      }

      if (PcdGetBool (PcdFloorsweepCpusByDtbNop)) {
        TmpOffset  = NodeOffset;
        NodeOffset = fdt_next_subnode (Dtb, NodeOffset);

        FdtErr = fdt_nop_node (Dtb, TmpOffset);
        if (FdtErr < 0) {
          DEBUG ((DEBUG_ERROR, "Failed to delete /cpus/cpu@%u node: %a\r\n", Cpu, fdt_strerror (FdtErr)));
          return EFI_DEVICE_ERROR;
        }
      } else {
        FdtErr = fdt_setprop (Dtb, NodeOffset, "status", "fail", sizeof ("fail"));
        if (FdtErr < 0) {
          DEBUG ((DEBUG_ERROR, "Failed to disable /cpus/cpu@%u node: %a\r\n", Cpu, fdt_strerror (FdtErr)));

          return EFI_DEVICE_ERROR;
        }

        NodeOffset = fdt_next_subnode (Dtb, NodeOffset);
      }

      DEBUG ((DEBUG_INFO, "Disabled cpu-%u node in FDT\r\n", Cpu));

      if ((Property != NULL) && !PhandleIsNextLevelCache (Dtb, CpusOffset, L2CachePhandle, 2)) {
        TmpOffset = fdt_node_offset_by_phandle (Dtb, L2CachePhandle);

        Property = fdt_getprop (Dtb, TmpOffset, "next-level-cache", NULL);
        if (Property != NULL) {
          L3CachePhandle = fdt32_to_cpu (*(UINT32 *)Property);
        }

        // special case if cache node to delete is node after disabled cpu node
        // when cache nodes are subnodes of cpus node.
        if (TmpOffset == NodeOffset) {
          NodeOffset = fdt_next_subnode (Dtb, NodeOffset);
          DEBUG ((DEBUG_INFO, "%a: l2 cache phandle=0x%x followed disabled cpu %u\r\n", __FUNCTION__, L2CachePhandle, Cpu));
        }

        if (TmpOffset >= 0) {
          FdtErr = fdt_nop_node (Dtb, TmpOffset);
          if (FdtErr < 0) {
            DEBUG ((DEBUG_ERROR, "Failed to delete l2 cache node 0x%x: %a\r\n", L2CachePhandle, fdt_strerror (FdtErr)));
            return EFI_DEVICE_ERROR;
          }

          DEBUG ((DEBUG_INFO, "Deleted l2 cache node 0x%x\r\n", L2CachePhandle));

          if ((Property != NULL) && !PhandleIsNextLevelCache (Dtb, CpusOffset, L3CachePhandle, 3)) {
            TmpOffset = fdt_node_offset_by_phandle (Dtb, L3CachePhandle);

            // special case if l3 node to delete is node after deleted l2 node
            if (TmpOffset == NodeOffset) {
              NodeOffset = fdt_next_subnode (Dtb, NodeOffset);
              DEBUG ((DEBUG_INFO, "%a: l3 cache phandle=0x%x followed deleted cpu %u\r\n", __FUNCTION__, L3CachePhandle, Cpu));
            }

            if (TmpOffset >= 0) {
              FdtErr = fdt_nop_node (Dtb, TmpOffset);
              if (FdtErr < 0) {
                DEBUG ((DEBUG_ERROR, "Failed to delete l3 cache node 0x%x: %a\r\n", L3CachePhandle, fdt_strerror (FdtErr)));
                return EFI_DEVICE_ERROR;
              }

              DEBUG ((DEBUG_INFO, "Deleted l3 cache node 0x%x\r\n", L3CachePhandle));
            } else {
              DEBUG ((DEBUG_ERROR, "%a: Missing l3 cache phandle=0x%x\r\n", __FUNCTION__, L3CachePhandle));
            }
          }
        } else {
          DEBUG ((DEBUG_ERROR, "%a: Missing l2 cache phandle=0x%x\r\n", __FUNCTION__, L2CachePhandle));
        }
      }
    }

    Cpu++;
  }

  CpuMapOffset = fdt_subnode_offset (Dtb, CpusOffset, "cpu-map");

  if (CpuMapOffset < 0) {
    DEBUG ((DEBUG_ERROR, "/cpus/cpu-map does not exist\r\n"));
    return EFI_DEVICE_ERROR;
  }

  HasSocketNode = TRUE;
  for (Socket = 0; (Socket < PLATFORM_MAX_SOCKETS) && HasSocketNode; Socket++) {
    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "socket%u", Socket);
    CpuMapSocketOffset = fdt_subnode_offset (Dtb, CpuMapOffset, SocketNodeStr);
    if (CpuMapSocketOffset < 0) {
      HasSocketNode      = FALSE;
      CpuMapSocketOffset = CpuMapOffset;
    } else if (!(SocketMask & (1UL << Socket))) {
      FdtErr = fdt_del_node (Dtb, CpuMapSocketOffset);
      if (FdtErr < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to delete /cpus/cpu-map/%a node: %a\n", SocketNodeStr, fdt_strerror (FdtErr)));
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((DEBUG_INFO, "Deleted socket%u node\n", Socket));
      continue;
    }

    Cluster = 0;
    while (TRUE) {
      AsciiSPrint (ClusterNodeStr, sizeof (ClusterNodeStr), "cluster%u", Cluster);
      NodeOffset = fdt_subnode_offset (Dtb, CpuMapSocketOffset, ClusterNodeStr);
      if (NodeOffset >= 0) {
        if (!ClusterIsPresent (Socket, Cluster)) {
          FdtErr = fdt_del_node (Dtb, NodeOffset);
          if (FdtErr < 0) {
            DEBUG ((
              DEBUG_ERROR,
              "Failed to delete /cpus/cpu-map/%a node: %a\r\n",
              ClusterNodeStr,
              fdt_strerror (FdtErr)
              ));
            return EFI_DEVICE_ERROR;
          }

          DEBUG ((DEBUG_INFO, "Deleted cluster%u node in FDT\r\n", Cluster));
        } else {
          INT32       ClusterCpuNodeOffset;
          CONST VOID  *Property;

          ClusterCpuNodeOffset = fdt_first_subnode (Dtb, NodeOffset);
          while (ClusterCpuNodeOffset > 0) {
            Property = fdt_getprop (Dtb, ClusterCpuNodeOffset, "cpu", NULL);

            TmpOffset            = ClusterCpuNodeOffset;
            ClusterCpuNodeOffset = fdt_next_subnode (Dtb, ClusterCpuNodeOffset);

            if (Property != NULL) {
              if (fdt_node_offset_by_phandle (Dtb, fdt32_to_cpu (*(UINT32 *)Property)) < 0) {
                FdtErr = fdt_nop_node (Dtb, TmpOffset);
                if (FdtErr < 0) {
                  DEBUG ((
                    DEBUG_ERROR,
                    "Failed to delete /cpus/cpu-map/%a cpu node: %a\r\n",
                    ClusterNodeStr,
                    fdt_strerror (FdtErr)
                    ));
                  return EFI_DEVICE_ERROR;
                }
              }
            }
          }

          Status = RenameChildNodesSequentially (Dtb, NodeOffset, "core%u", CoreNodeStr, sizeof (CoreNodeStr), 100);
          if (EFI_ERROR (Status)) {
            return Status;
          }
        }

        Cluster++;
      } else {
        break;
      }
    }

    Status = RenameChildNodesSequentially (Dtb, CpuMapSocketOffset, "cluster%u", ClusterNodeStr, sizeof (ClusterNodeStr), 100);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (HasSocketNode) {
    Status = RenameChildNodesSequentially (Dtb, CpuMapOffset, "socket%u", SocketNodeStr, sizeof (SocketNodeStr), 100);
    if (EFI_ERROR (Status)) {
      return Status;
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
  IN VOID  *Dtb
  )
{
  INT32  CpusOffset;

  CpusOffset = fdt_path_offset (Dtb, "/cpus");
  if (CpusOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find /cpus node\n"));
    return EFI_DEVICE_ERROR;
  }

  return UpdateCpuFloorsweepingConfig (0x1, CpusOffset, Dtb);
}

/**
  Floorsweep global thermal mappings.  Examines thermal-zones subnodes
  for cooling-maps entries and removes cooling-device list entries
  that reference deleted phandles (e.g. cpus that were floorswept).

**/
EFI_STATUS
EFIAPI
FloorSweepGlobalThermals (
  IN VOID  *Dtb
  )
{
  INT32        ThermsOffset;
  INT32        ThermSubNodeOffset;
  INT32        CoolingMapsSubNodeOffset;
  INT32        MapSubNodeOffset;
  UINT32       Phandle;
  INT32        Length;
  CONST UINT8  *CoolingDeviceList;
  CONST CHAR8  *ThermNodeName;
  CONST CHAR8  *MapName;
  UINT8        *Buffer;
  INT32        Index;
  INT32        BufferLength;
  INT32        EntrySize;
  INT32        FdtErr;
  UINTN        NumMaps;
  INT32        TmpOffset;

  ThermsOffset = fdt_path_offset (Dtb, "/thermal-zones");
  if (ThermsOffset < 0) {
    DEBUG ((DEBUG_INFO, "Failed to find /thermal-zones node\n"));
    return EFI_SUCCESS;
  }

  EntrySize          = THERMAL_COOLING_DEVICE_ENTRY_SIZE;
  ThermSubNodeOffset = 0;
  fdt_for_each_subnode (ThermSubNodeOffset, Dtb, ThermsOffset) {
    ThermNodeName = fdt_get_name (Dtb, ThermSubNodeOffset, NULL);

    CoolingMapsSubNodeOffset = fdt_subnode_offset (Dtb, ThermSubNodeOffset, "cooling-maps");
    if (CoolingMapsSubNodeOffset < 0) {
      DEBUG ((DEBUG_INFO, "/thermal-zones/%a/cooling-maps does not exist\n", ThermNodeName));
      continue;
    }

    NumMaps          = 0;
    MapSubNodeOffset = fdt_first_subnode (Dtb, CoolingMapsSubNodeOffset);
    while (MapSubNodeOffset > 0) {
      NumMaps++;
      MapName           = fdt_get_name (Dtb, MapSubNodeOffset, NULL);
      CoolingDeviceList = (CONST UINT8 *)fdt_getprop (Dtb, MapSubNodeOffset, "cooling-device", &Length);
      if (CoolingDeviceList == NULL) {
        DEBUG ((DEBUG_ERROR, "/thermal-zones/%a/cooling-maps/%a missing cooling-device property\n", ThermNodeName, MapName));
        MapSubNodeOffset = fdt_next_subnode (Dtb, MapSubNodeOffset);
        continue;
      }

      DEBUG ((DEBUG_INFO, "/thermal-zones/%a/cooling-maps/%a len=%d\n", ThermNodeName, MapName, Length));

      Buffer = (UINT8 *)AllocatePool (Length);
      if (Buffer == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: alloc failed\n", __FUNCTION__));
        return EFI_OUT_OF_RESOURCES;
      }

      // build new list, skipping entries that have a deleted phandle
      BufferLength = 0;
      for (Index = 0; Index < Length; Index += EntrySize) {
        Phandle = fdt32_to_cpu (*(CONST UINT32 *)&CoolingDeviceList[Index]);
        if (fdt_node_offset_by_phandle (Dtb, Phandle) < 0) {
          DEBUG ((DEBUG_INFO, "/thermal-zones/%a/cooling-maps/%a deleted Phandle=0x%x\n", ThermNodeName, MapName, Phandle));
        } else {
          CopyMem (&Buffer[BufferLength], &CoolingDeviceList[Index], EntrySize);
          BufferLength += EntrySize;
        }
      }

      // update DT if new list is different than original
      if (BufferLength != Length) {
        if (BufferLength != 0) {
          fdt_nop_property (Dtb, MapSubNodeOffset, "cooling-device");
          fdt_setprop (Dtb, MapSubNodeOffset, "cooling-device", Buffer, BufferLength);
          MapSubNodeOffset = fdt_next_subnode (Dtb, MapSubNodeOffset);
        } else {
          DEBUG ((DEBUG_INFO, "/thermal-zones/%a/cooling-maps/%a cooling-device empty, deleting\n", ThermNodeName, MapName));

          TmpOffset        = MapSubNodeOffset;
          MapSubNodeOffset = fdt_next_subnode (Dtb, MapSubNodeOffset);

          FdtErr = fdt_nop_node (Dtb, TmpOffset);
          if (FdtErr < 0) {
            DEBUG ((
              DEBUG_ERROR,
              "Failed to delete /thermal-zones/%a/cooling-maps map: %a\r\n",
              ThermNodeName,
              fdt_strerror (FdtErr)
              ));
            FreePool (Buffer);
            return EFI_DEVICE_ERROR;
          }

          NumMaps--;
        }
      } else {
        MapSubNodeOffset = fdt_next_subnode (Dtb, MapSubNodeOffset);
      }

      FreePool (Buffer);
    }

    DEBUG ((DEBUG_INFO, "/thermal-zones/%a/cooling-maps has %u maps\n", ThermNodeName, NumMaps));

    if (NumMaps == 0) {
      FdtErr = fdt_del_node (Dtb, CoolingMapsSubNodeOffset);
      if (FdtErr < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to delete /thermal-zones/%a/cooling-maps: %a\r\n", ThermNodeName, fdt_strerror (FdtErr)));
        return EFI_DEVICE_ERROR;
      }
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
TH500FloorSweepSockets (
  IN  UINT32  SocketMask,
  IN  VOID    *Dtb
  )
{
  CHAR8   SocketNodeStr[] = "/socket@xx";
  UINT32  MaxSockets;
  INT32   NodeOffset;

  MaxSockets = 0;

  while (TRUE) {
    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u", MaxSockets);
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

  for (UINT32 Socket = 0; Socket < MaxSockets; Socket++) {
    if (SocketMask & (1UL << Socket)) {
      continue;
    }

    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u", Socket);
    NodeOffset = fdt_path_offset (Dtb, SocketNodeStr);
    if (NodeOffset >= 0) {
      DEBUG ((DEBUG_INFO, "Deleting socket@%u node\n", Socket));
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
  IN VOID  *Dtb
  )
{
  PLATFORM_CPU_INFO                *Info;
  UINTN                            ChipId;
  EFI_STATUS                       Status;
  CONST TEGRA_FLOOR_SWEEPING_INFO  *FloorSweepingInfo;

  Info   = FloorSweepCpuInfo ();
  ChipId = TegraGetChipID ();

  if (ChipId == T234_CHIP_ID) {
    Status = FloorSweepGlobalCpus (Dtb);
    if (!EFI_ERROR (Status)) {
      Status = FloorSweepGlobalThermals (Dtb);
    }

    return Status;
  }

  // common floorsweeping flow
  Status = CommonInitializeGlobalStructures (Dtb, &FloorSweepingInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (ChipId == TH500_CHIP_ID) {
    TH500FloorSweepSockets (Info->SocketMask, Dtb);
    Status = TH500FloorSweepCpus (Info->SocketMask, Dtb);
  } else {
    Status = CommonFloorSweepCpus (Info->SocketMask, Dtb);
  }

  if (!EFI_ERROR (Status)) {
    Status = CommonFloorSweepPcie (Info->SocketMask, Dtb);
  }

  if (!EFI_ERROR (Status)) {
    Status = CommonFloorSweepScfCache (Info->SocketMask, Dtb);
  }

  if (!EFI_ERROR (Status)) {
    Status = CommonFloorSweepIps (Info->SocketMask);
  }

  if (!EFI_ERROR (Status) && FloorSweepingInfo->HasGlobalThermals) {
    Status = FloorSweepGlobalThermals (Dtb);
  }

  return Status;
}

/**
  Get First Enabled Core on Socket.

**/
EFI_STATUS
EFIAPI
GetFirstEnabledCoreOnSocket (
  IN   UINTN  Socket,
  OUT  UINTN  *LinearCoreId
  )
{
  UINTN  SocketCore;

  if (!IsSocketEnabled (Socket)) {
    return EFI_INVALID_PARAMETER;
  }

  for (SocketCore = Socket * PLATFORM_MAX_CORES_PER_SOCKET;
       SocketCore < (Socket + 1) * PLATFORM_MAX_CORES_PER_SOCKET;
       SocketCore++)
  {
    if (IsCoreEnabled (SocketCore)) {
      *LinearCoreId = SocketCore;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Get Number of Enabled Cores on Socket.

**/
EFI_STATUS
EFIAPI
GetNumEnabledCoresOnSocket (
  IN   UINTN  Socket,
  OUT  UINTN  *NumEnabledCores
  )
{
  UINTN  SocketCore;
  UINTN  EnabledCoreCount = 0;

  if (!IsSocketEnabled (Socket)) {
    return EFI_INVALID_PARAMETER;
  }

  for (SocketCore = Socket * PLATFORM_MAX_CORES_PER_SOCKET;
       SocketCore < (Socket + 1) * PLATFORM_MAX_CORES_PER_SOCKET;
       SocketCore++)
  {
    if (IsCoreEnabled (SocketCore)) {
      EnabledCoreCount++;
    }
  }

  *NumEnabledCores = EnabledCoreCount;

  return EFI_SUCCESS;
}
