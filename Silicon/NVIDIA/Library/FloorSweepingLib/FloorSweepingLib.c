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
#include <Library/MpCoreInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>

#include "CommonFloorSweepingLib.h"

#define THERMAL_COOLING_DEVICE_ENTRY_SIZE  (3 * sizeof (INT32))
#define MAX_CHILD_NODES_FOR_RENAME         100

STATIC
UINT32
EFIAPI
GetSocketMask (
  UINT32  *MaxPossibleSockets OPTIONAL
  )
{
  STATIC UINT32                 SocketMask              = 0;
  STATIC UINT32                 FixedMaxPossibleSockets = 0;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  if (SocketMask == 0) {
    Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
    NV_ASSERT_RETURN (
      (Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)),
      return 0,
      "Failed to get PlatformResourceInfo\r\n"
      );

    PlatformResourceInfo    = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
    SocketMask              = PlatformResourceInfo->SocketMask;
    FixedMaxPossibleSockets = PlatformResourceInfo->MaxPossibleSockets;
    ASSERT (FixedMaxPossibleSockets <= 32);
  }

  if (MaxPossibleSockets != NULL) {
    *MaxPossibleSockets = FixedMaxPossibleSockets;
  }

  return SocketMask;
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
  UINT32  SocketMask;

  SocketMask = GetSocketMask (NULL);
  return (SocketMask & (1 << SocketIndex)) != 0;
}

/**
  Get the first enabled socket

  @retval  First enabled socket
**/
UINT32
EFIAPI
GetFirstEnabledSocket (
  VOID
  )
{
  UINT32  SocketMask;
  UINT32  MaxPossibleSockets;
  UINT32  SocketIndex;

  SocketMask = GetSocketMask (&MaxPossibleSockets);
  for (SocketIndex = 0; SocketIndex < MaxPossibleSockets; SocketIndex++) {
    if ((SocketMask & (1 << SocketIndex)) != 0) {
      return SocketIndex;
    }
  }

  return 0;
}

/**
  Get the next enabled socket

  @param[in, out]  SocketId  Socket index. On input the last socket id, on output the next enabled socket id
                             if error is returned, SocketId is set to MAX_UINT32

  @retval  EFI_SUCCESS - Socket found
  @retval  EFI_NOT_FOUND - No more sockets
**/
EFI_STATUS
EFIAPI
GetNextEnabledSocket (
  IN OUT UINT32  *SocketId
  )
{
  UINT32  SocketMask;
  UINT32  SocketIndex;
  UINT32  MaxPossibleSockets;

  SocketMask = GetSocketMask (&MaxPossibleSockets);
  for (SocketIndex = *SocketId + 1; SocketIndex < MaxPossibleSockets; SocketIndex++) {
    if ((SocketMask & (1 << SocketIndex)) != 0) {
      *SocketId = SocketIndex;
      return EFI_SUCCESS;
    }
  }

  *SocketId = MAX_UINT32;
  return EFI_NOT_FOUND;
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

/** Checks to see if any of the cpu nodes in the cluster are exist */
STATIC
BOOLEAN
EFIAPI
AreValidCpuNodesInCluster (
  IN VOID   *Dtb,
  IN INT32  ClusterNodeOffset
  )
{
  INT32       CpuNodeOffset;
  INT32       ActualCpuNodeOffset;
  CONST VOID  *Property;
  BOOLEAN     HasValidCpuNodes;

  HasValidCpuNodes = FALSE;

  CpuNodeOffset = fdt_first_subnode (Dtb, ClusterNodeOffset);
  if (CpuNodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a:Failed to get first cpu node offset: %d\n", __func__, CpuNodeOffset));
    return FALSE;
  }

  while (!HasValidCpuNodes) {
    Property      = fdt_getprop (Dtb, CpuNodeOffset, "cpu", NULL);
    CpuNodeOffset = fdt_next_subnode (Dtb, CpuNodeOffset);
    // Check if subnode is a cpu node
    if (Property == NULL) {
      if (CpuNodeOffset < 0) {
        DEBUG ((DEBUG_INFO, "%a: No more cpu nodes\n", __func__));
        break;
      } else {
        continue;
      }
    }

    ActualCpuNodeOffset = fdt_node_offset_by_phandle (Dtb, fdt32_to_cpu (*(UINT32 *)Property));
    if (ActualCpuNodeOffset < 0) {
      DEBUG ((DEBUG_INFO, "%a: Failed to get cpu node offset by phandle\n", __func__));
      continue;
    }

    HasValidCpuNodes = TRUE;
  }

  return HasValidCpuNodes;
}

/**
  Floorsweep CPUs in DTB

**/
EFI_STATUS
EFIAPI
UpdateCpuFloorsweepingConfig (
  IN INT32  CpusOffset,
  IN VOID   *Dtb
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
  UINT32      Socket;
  UINT32      MaxPossibleSockets;
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

    if (MpCoreInfoIsProcessorEnabled (Mpidr) == EFI_SUCCESS) {
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
  GetSocketMask (&MaxPossibleSockets);
  for (Socket = 0; (Socket < MaxPossibleSockets) && HasSocketNode; Socket++) {
    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "socket%u", Socket);
    CpuMapSocketOffset = fdt_subnode_offset (Dtb, CpuMapOffset, SocketNodeStr);
    if (CpuMapSocketOffset < 0) {
      HasSocketNode      = FALSE;
      CpuMapSocketOffset = CpuMapOffset;
    } else if (!IsSocketEnabled (Socket)) {
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
        if (!AreValidCpuNodesInCluster (Dtb, NodeOffset)) {
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
              // Check if the cpu node is missing if so remove from the cpu-map
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

          Status = RenameChildNodesSequentially (Dtb, NodeOffset, "core%u", CoreNodeStr, sizeof (CoreNodeStr), MAX_CHILD_NODES_FOR_RENAME);
          if (EFI_ERROR (Status)) {
            return Status;
          }
        }

        Cluster++;
      } else {
        break;
      }
    }

    Status = RenameChildNodesSequentially (Dtb, CpuMapSocketOffset, "cluster%u", ClusterNodeStr, sizeof (ClusterNodeStr), MAX_CHILD_NODES_FOR_RENAME);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (HasSocketNode) {
    Status = RenameChildNodesSequentially (Dtb, CpuMapOffset, "socket%u", SocketNodeStr, sizeof (SocketNodeStr), MAX_CHILD_NODES_FOR_RENAME);
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

  return UpdateCpuFloorsweepingConfig (CpusOffset, Dtb);
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
  UINT32                           SocketMask;
  UINTN                            ChipId;
  EFI_STATUS                       Status;
  CONST TEGRA_FLOOR_SWEEPING_INFO  *FloorSweepingInfo;

  SocketMask = GetSocketMask (NULL);
  if (SocketMask == 0) {
    return EFI_DEVICE_ERROR;
  }

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
    TH500FloorSweepSockets (SocketMask, Dtb);
    Status = TH500FloorSweepCpus (Dtb);
  } else {
    Status = CommonFloorSweepCpus (Dtb);
  }

  if (!EFI_ERROR (Status)) {
    Status = CommonFloorSweepPcie (Dtb);
  }

  if (!EFI_ERROR (Status)) {
    Status = CommonFloorSweepScfCache (Dtb);
  }

  if (!EFI_ERROR (Status)) {
    Status = CommonFloorSweepIps ();
  }

  if (!EFI_ERROR (Status) && FloorSweepingInfo->HasGlobalThermals) {
    Status = FloorSweepGlobalThermals (Dtb);
  }

  return Status;
}
