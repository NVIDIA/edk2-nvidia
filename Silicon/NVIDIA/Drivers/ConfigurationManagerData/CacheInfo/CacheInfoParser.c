/** @file
  Cache info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Terminology overview:
    CacheData - This is the information parsed from the DTB about a single cache.
                The CacheId is the phandle, and the next level value is the phandle
                of the cache this one flows into.
    CacheNode - This is a linked list node that contains CacheData as well as additional
                computed information about the cache instance. This is used in processing
                the CacheData and eventually is used to create the CacheInfo.
    CacheInfo - This is the ConfigurationManager data structure for a single cache. It
                is built from the CacheNode and its CacheData information. The CacheId is
                a uniquely generated value based on the cache level, type, socket, cluster, & core.
                If this is the only cache that flows into another one, then that one's token
                is the next-level. Otherwise next-level is NULL.
    CacheNodes - Cache nodes that are specified with a compatible = "cache" in the DTB
    CpuCacheNodes - Cache nodes that are specified as part of a CPU definition in the DTB
    CacheTracker - Structure to keep pointers and counts for CacheNodes and CpuCacheNodes

**/

#include "NvCmObjectDescUtility.h"
#include "CacheInfoParser.h"
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PrintLib.h>

/** A helper macro for populating the Cache Type Structure's attributes
*/
#define CACHE_ATTRIBUTES(                                               \
                                                                        AllocationType,                                               \
                                                                        CacheType,                                                    \
                                                                        WritePolicy                                                   \
                                                                        )                                                             \
  (                                                                     \
    AllocationType |                                                    \
    (CacheType << 2) |                                                  \
    (WritePolicy << 4)                                                  \
  )

#define GET_CACHE_ID(Level, Type, Core, Cluster, Socket)  ((((3 - (Level)) << 24)) | (((Type) << 16)) | (((Core) << 12)) |(((Cluster) << 8)) | ((Socket) + 1))

#define UNDEFINED_SOCKET   MAX_UINT32
#define UNDEFINED_CLUSTER  MAX_UINT32
#define UNDEFINED_CORE     MAX_UINT32

#define UNUSED_SOCKET   (UNDEFINED_SOCKET - 1)
#define UNUSED_CLUSTER  (UNDEFINED_CLUSTER - 1)
#define UNUSED_CORE     (UNDEFINED_CORE - 1)

typedef struct _CACHE_NODE CACHE_NODE;

struct _CACHE_NODE {
  NVIDIA_DEVICE_TREE_CACHE_DATA    CacheData;
  CM_OBJECT_TOKEN                  Token;
  CACHE_NODE                       *NextLevel;
  UINT32                           Socket;
  UINT32                           Cluster;
  UINT32                           Core;
};

typedef struct {
  // Cache Nodes
  CACHE_NODE    *CacheNodes;
  UINT32        CacheNodeCount;

  // Cpu Cache Nodes
  CACHE_NODE    *CpuCacheNodes;
  UINT32        CpuCacheNodeCount;
} CACHE_TRACKER;

// Find the Node with the corresponding cache pHandle
STATIC
CACHE_NODE *
EFIAPI
FindpHandleInTracker (
  IN UINT32         pHandle,
  IN CACHE_TRACKER  *Tracker
  )
{
  CACHE_NODE  *CacheNode;
  UINT32      Index;

  for (Index = 0; Index < Tracker->CacheNodeCount; Index++) {
    CacheNode = &Tracker->CacheNodes[Index];
    if (CacheNode->CacheData.CacheId == pHandle) {
      return CacheNode;
    }
  }

  for (Index = 0; Index < Tracker->CpuCacheNodeCount; Index++) {
    CacheNode = &Tracker->CpuCacheNodes[Index];
    if (CacheNode->CacheData.CacheId == pHandle) {
      return CacheNode;
    }
  }

  return NULL;
}

// Generate the CacheInfo structure for the given CacheNode
STATIC
EFI_STATUS
EFIAPI
GetCacheInfoFromCacheNode (
  IN CACHE_NODE          *CacheNode,
  OUT CM_ARM_CACHE_INFO  *CacheInfo
  )
{
  if ((CacheNode == NULL) ||
      (CacheInfo == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  CacheInfo->Token = CacheNode->Token;

  if ((CacheNode->NextLevel != NULL) &&
      ((CacheNode->Socket == CacheNode->NextLevel->Socket) &&
       (CacheNode->Cluster == CacheNode->NextLevel->Cluster) &&
       (CacheNode->Core == CacheNode->NextLevel->Core)))
  {
    CacheInfo->NextLevelOfCacheToken = CacheNode->NextLevel->Token;
  } else {
    CacheInfo->NextLevelOfCacheToken = CM_NULL_TOKEN;
  }

  CacheInfo->Size         = CacheNode->CacheData.CacheSize;
  CacheInfo->NumberOfSets = CacheNode->CacheData.CacheSets;
  CacheInfo->LineSize     = CacheNode->CacheData.CacheLineSize;

  // Calculate Associativity
  CacheInfo->Associativity = CacheInfo->Size / (CacheInfo->LineSize * CacheInfo->NumberOfSets);

  // Assign Attributes
  switch (CacheNode->CacheData.Type) {
    case CACHE_TYPE_ICACHE:
      CacheInfo->Attributes =  CACHE_ATTRIBUTES (
                                 EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ,
                                 EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                                 EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                                 );
      break;

    case CACHE_TYPE_DCACHE:
      CacheInfo->Attributes =  CACHE_ATTRIBUTES (
                                 EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                                 EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                                 EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                                 );
      break;

    case CACHE_TYPE_UNIFIED:
      CacheInfo->Attributes =  CACHE_ATTRIBUTES (
                                 EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                                 EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                                 EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                                 );
      break;

    default:
      return EFI_INVALID_PARAMETER;
  }

  CacheInfo->CacheId = GET_CACHE_ID (CacheNode->CacheData.CacheLevel, CacheNode->CacheData.Type, CacheNode->Core, CacheNode->Cluster, CacheNode->Socket);

  return EFI_SUCCESS;
}

STATIC_ASSERT ((CM_NULL_TOKEN == 0), "Need to initialize Token values to CM_NULL_TOKEN below if CM_NULL_TOKEN isn't zero!");

EFI_STATUS
EFIAPI
AllocateCacheHierarchyInfo (
  CACHE_HIERARCHY_INFO_SOCKET  **HierarchyInfoPtr
  )
{
  EFI_STATUS                   Status;
  UINT32                       SocketIndex;
  UINT32                       ClusterIndex;
  UINT32                       MaxSocket;
  UINT32                       MaxCluster;
  UINT32                       MaxCore;
  CACHE_HIERARCHY_INFO_SOCKET  *Socket;

  if (HierarchyInfoPtr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = MpCoreInfoGetPlatformInfo (NULL, &MaxSocket, &MaxCluster, &MaxCore);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get MpCoreInfo\n", __FUNCTION__, Status));
    *HierarchyInfoPtr = NULL;
    return Status;
  }

  Socket = AllocateZeroPool ((MaxSocket+1) * sizeof (CACHE_HIERARCHY_INFO_SOCKET));
  NV_ASSERT_RETURN (Socket != NULL, return EFI_OUT_OF_RESOURCES, "%a: Unable to allocate space for CACHE_HIERARCHY_INFO_SOCKET\n", __FUNCTION__);

  for (SocketIndex = 0; SocketIndex <= MaxSocket; SocketIndex++) {
    Socket[SocketIndex].Cluster = AllocateZeroPool ((MaxCluster+1) * sizeof (CACHE_HIERARCHY_INFO_CLUSTER));
    NV_ASSERT_RETURN (
      Socket[SocketIndex].Cluster != NULL,
      { Status = EFI_OUT_OF_RESOURCES;
        goto CleanupAndReturn;
      },
      "%a: Unable to allocate space for CACHE_HIERARCHY_INFO_CLUSTER\n",
      __FUNCTION__
      );
    for (ClusterIndex = 0; ClusterIndex <= MaxCluster; ClusterIndex++) {
      Socket[SocketIndex].Cluster[ClusterIndex].Cpu = AllocateZeroPool ((MaxCore+1) * sizeof (CACHE_HIERARCHY_INFO_CPU));
      NV_ASSERT_RETURN (
        Socket[SocketIndex].Cluster[ClusterIndex].Cpu != NULL,
        { Status = EFI_OUT_OF_RESOURCES;
          goto CleanupAndReturn;
        },
        "%a: Unable to allocate space for CACHE_HIERARCHY_INFO_CPU\n",
        __FUNCTION__
        );
    }
  }

CleanupAndReturn:
  if (EFI_ERROR (Status)) {
    FreeCacheHierarchyInfo (Socket);
  } else {
    *HierarchyInfoPtr = Socket;
  }

  return Status;
}

VOID
FreeCacheHierarchyInfo (
  CACHE_HIERARCHY_INFO_SOCKET  *Socket
  )
{
  EFI_STATUS  Status;
  UINT32      SocketIndex;
  UINT32      ClusterIndex;
  UINT32      MaxSocket;
  UINT32      MaxCluster;

  Status = MpCoreInfoGetPlatformInfo (NULL, &MaxSocket, &MaxCluster, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get MpCoreInfo\n", __FUNCTION__, Status));
    return;
  }

  if (Socket != NULL) {
    for (SocketIndex = 0; SocketIndex <= MaxSocket; SocketIndex++) {
      if (Socket[SocketIndex].Cluster != NULL) {
        for (ClusterIndex = 0; ClusterIndex <= MaxCluster; ClusterIndex++) {
          FREE_NON_NULL (Socket[SocketIndex].Cluster[ClusterIndex].Cpu);
        }

        FreePool (Socket[SocketIndex].Cluster);
      }
    }

    FreePool (Socket);
  }
}

// Note: Socket/Cluster/Core are physical
STATIC
EFI_STATUS
EFIAPI
GeneratePrivateDataForPosition (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CACHE_TRACKER                *CacheTracker,
  IN  UINT32                       Socket,
  IN  UINT32                       Cluster,
  IN  UINT32                       Core,
  OUT CM_OBJECT_TOKEN              *Token,
  OUT UINTN                        *Count
  )
{
  EFI_STATUS       Status;
  UINT32           Index;
  CACHE_NODE       *Node;
  CM_OBJECT_TOKEN  *PrivateData;
  UINT32           PrivateCount;
  UINT32           PrivateDataIndex;

  PrivateData = NULL;

  if ((ParserHandle == NULL) ||
      (CacheTracker == NULL) ||
      (Token == NULL) ||
      (Count == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  // Count the number of matching nodes
  PrivateCount = 0;
  for (Index = 0; Index < (CacheTracker->CacheNodeCount + CacheTracker->CpuCacheNodeCount); Index++) {
    if (Index < CacheTracker->CacheNodeCount) {
      Node = &CacheTracker->CacheNodes[Index];
    } else {
      Node = &CacheTracker->CpuCacheNodes[Index - CacheTracker->CacheNodeCount];
    }

    if ((Node->Socket == Socket) &&
        (Node->Cluster == Cluster) &&
        (Node->Core == Core))
    {
      PrivateCount++;
    }
  }

  // Allocate the needed space
  if (PrivateCount > 0) {
    PrivateData = AllocatePool (sizeof (CM_OBJECT_TOKEN) * PrivateCount);
    if (PrivateData == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to allocate space for PrivateData for Socket %u Cluster %u Core %u\n", __FUNCTION__, Socket, Cluster, Core));
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }
  } else {
    Status = EFI_SUCCESS;
    *Token = CM_NULL_TOKEN;
    goto CleanupAndReturn;
  }

  // Gather the tokens
  PrivateDataIndex = 0;
  for (Index = 0; (Index < (CacheTracker->CacheNodeCount + CacheTracker->CpuCacheNodeCount)) && (PrivateDataIndex < PrivateCount); Index++) {
    if (Index < CacheTracker->CacheNodeCount) {
      Node = &CacheTracker->CacheNodes[Index];
    } else {
      Node = &CacheTracker->CpuCacheNodes[Index - CacheTracker->CacheNodeCount];
    }

    if ((Node->Socket == Socket) &&
        (Node->Cluster == Cluster) &&
        (Node->Core == Core))
    {
      PrivateData[PrivateDataIndex] = Node->Token;
      PrivateDataIndex++;
    }
  }

  // Add the PrivateData to the CM and fill in the return pointers
  Status = NvAddSingleCmObj (
             ParserHandle,
             CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef),
             PrivateData,
             sizeof (CM_OBJECT_TOKEN) * PrivateCount,
             Token
             );
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  *Count = PrivateCount;
  FREE_NON_NULL (PrivateData);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
GenerateHierarchyInfo (
  IN  CONST HW_INFO_PARSER_HANDLE     ParserHandle,
  IN  CACHE_TRACKER                   *CacheTracker,
  IN OUT CACHE_HIERARCHY_INFO_SOCKET  *HierarchyInfo
  )
{
  EFI_STATUS                 Status;
  UINT32                     MaxSocket;
  UINT32                     MaxCluster;
  UINT32                     MaxCore;
  UINT32                     Socket;
  UINT32                     Cluster;
  UINT32                     Core;
  CACHE_HIERARCHY_INFO_DATA  *Data;

  // The CacheTracker nodes all have hierarchy info set

  Status = MpCoreInfoGetPlatformInfo (NULL, &MaxSocket, &MaxCluster, &MaxCore);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get MpCoreInfo\n", __FUNCTION__, Status));
    return Status;
  }

  // Call GeneratePrivateDataForPosition(Socket, Cluster, Core) for each combination of valid number + UNUSED
  for (Socket = 0; Socket <= MaxSocket; Socket++) {
    Data   = &HierarchyInfo[Socket].Data;
    Status = GeneratePrivateDataForPosition (ParserHandle, CacheTracker, Socket, UNUSED_CLUSTER, UNUSED_CORE, &Data->Token, &Data->Count);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get Private Data for Socket %u\n", __FUNCTION__, Status, Socket));
      return Status;
    }

    for (Cluster = 0; Cluster <= MaxCluster; Cluster++) {
      Data   = &HierarchyInfo[Socket].Cluster[Cluster].Data;
      Status = GeneratePrivateDataForPosition (ParserHandle, CacheTracker, Socket, Cluster, UNUSED_CORE, &Data->Token, &Data->Count);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get Private Data for Socket %u Cluster %u\n", __FUNCTION__, Status, Socket, Cluster));
        return Status;
      }

      for (Core = 0; Core <= MaxCore; Core++) {
        Data   = &HierarchyInfo[Socket].Cluster[Cluster].Cpu[Core].Data;
        Status = GeneratePrivateDataForPosition (ParserHandle, CacheTracker, Socket, Cluster, Core, &Data->Token, &Data->Count);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get Private Data for Socket %u Cluster %u Core %u\n", __FUNCTION__, Status, Socket, Cluster, Core));
          return Status;
        }
      }
    }
  }

  return Status;
}

// Building the Cache info structure
STATIC
EFI_STATUS
EFIAPI
BuildCacheInfoStruct (
  IN CACHE_TRACKER      *CacheTracker,
  IN CM_ARM_CACHE_INFO  *CacheInfoStruct,
  IN UINT32             CacheInfoCount,
  OUT CM_OBJECT_TOKEN   **CacheInfoTokens
  )
{
  EFI_STATUS       Status;
  CACHE_NODE       *CacheNode;
  UINT32           Index;
  CM_OBJECT_TOKEN  *CacheInfoTokenMap;

  CacheInfoTokenMap = NULL;

  if ((CacheTracker == NULL) ||
      (CacheInfoStruct == NULL) ||
      (CacheInfoCount != (CacheTracker->CacheNodeCount + CacheTracker->CpuCacheNodeCount)))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (CacheInfoTokens != NULL) {
    CacheInfoTokenMap = AllocateZeroPool (CacheInfoCount * sizeof (CM_OBJECT_TOKEN));
    if (CacheInfoTokenMap == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to allocate space for CacheInfoTokenMap\n", __FUNCTION__));
      *CacheInfoTokens = NULL;
    }
  }

  // Get the CacheInfo for each CacheNode
  for (Index = 0; Index < CacheInfoCount; Index++) {
    if (Index < CacheTracker->CacheNodeCount) {
      CacheNode = &CacheTracker->CacheNodes[Index];
    } else {
      CacheNode = &CacheTracker->CpuCacheNodes[Index - CacheTracker->CacheNodeCount];
    }

    Status = GetCacheInfoFromCacheNode (CacheNode, &CacheInfoStruct[Index]);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    if (CacheInfoTokenMap) {
      CacheInfoTokenMap[Index] = CacheInfoStruct[Index].Token;
    }
  }

  if (CacheInfoTokenMap) {
    *CacheInfoTokens = CacheInfoTokenMap;
  }

CleanupAndReturn:
  if (EFI_ERROR (Status)) {
    FREE_NON_NULL (CacheInfoTokenMap);
  }

  return Status;
}

// Create the data for a node
STATIC
EFI_STATUS
EFIAPI
CreateCacheNodeFromOffset (
  CACHE_TRACKER                  *CacheTracker,
  CACHE_NODE                     *Node,
  INT32                          NodeOffset,
  NVIDIA_DEVICE_TREE_CACHE_TYPE  CacheType,
  CM_OBJECT_TOKEN                Token,
  UINT32                         Socket,
  UINT32                         Cluster,
  UINT32                         Core
  )
{
  EFI_STATUS  Status;

  if (Node == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Node->CacheData.Type = CacheType;
  Status               = DeviceTreeGetCacheData (NodeOffset, &Node->CacheData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get CacheData\n", __FUNCTION__, Status));
    return Status;
  }

  Node->Token = Token;

  if ((CacheTracker != NULL) &&
      (Node->CacheData.NextLevelCache != 0))
  {
    Node->NextLevel = FindpHandleInTracker (Node->CacheData.NextLevelCache, CacheTracker);
  }

  Node->Socket  = Socket;
  Node->Cluster = Cluster;
  Node->Core    = Core;

  return Status;
}

STATIC CONST CHAR8  *CacheCompatibleInfo[] = {
  "cache",
  // Support old DTB compatible strings as well
  "l3-cache",
  "l2-cache",
  NULL
};

// Gets the data for the CacheNodes (compatible = "cache")
STATIC
EFI_STATUS
EFIAPI
GetCacheNodeData (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CACHE_TRACKER             *CacheTracker
  )
{
  EFI_STATUS       Status;
  UINT32           NodeCount;
  UINT32           NodeIndex;
  INT32            NodeOffset;
  CACHE_NODE       *CacheNodes;
  CACHE_NODE       *Node;
  CM_OBJECT_TOKEN  *TokenMap;

  CacheNodes = NULL;
  TokenMap   = NULL;

  if (CacheTracker == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Get the count of "cache" nodes
  NodeCount = 0;
  Status    = DeviceTreeGetCompatibleNodeCount (CacheCompatibleInfo, &NodeCount);
  if (EFI_ERROR (Status) || (NodeCount == 0)) {
    return Status;
  }

  // Allocate space for the nodes
  CacheNodes = (CACHE_NODE *)AllocateZeroPool (sizeof (CACHE_NODE) * NodeCount);
  if (CacheNodes == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate for CacheNodes\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  // Allocate tokens for the nodes
  Status = NvAllocateCmTokens (ParserHandle, NodeCount, &TokenMap);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  // Get the data for the CacheNodes
  NodeOffset = -1;
  for (NodeIndex = 0; NodeIndex < NodeCount; NodeIndex++) {
    Status = DeviceTreeGetNextCompatibleNode (CacheCompatibleInfo, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get next cache node (index %u) after offset 0x%x\n", __FUNCTION__, Status, NodeIndex, NodeOffset));
      goto CleanupAndReturn;
    }

    Status = CreateCacheNodeFromOffset (NULL, &CacheNodes[NodeIndex], NodeOffset, CACHE_TYPE_UNIFIED, TokenMap[NodeIndex], UNDEFINED_SOCKET, UNDEFINED_CLUSTER, UNDEFINED_CORE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get CacheData for index %u at offset 0x%x\n", __FUNCTION__, Status, NodeIndex, NodeOffset));
      goto CleanupAndReturn;
    }
  }

  // Add the info to the Tracker
  CacheTracker->CacheNodeCount = NodeCount;
  CacheTracker->CacheNodes     = CacheNodes;

  // Update the next pointers now that we have all the non-core cache nodes in the Tracker
  for (NodeIndex = 0; NodeIndex < NodeCount; NodeIndex++) {
    Node = &CacheNodes[NodeIndex];

    if (Node->CacheData.NextLevelCache != 0) {
      Node->NextLevel = FindpHandleInTracker (Node->CacheData.NextLevelCache, CacheTracker);
    }
  }

CleanupAndReturn:
  if (EFI_ERROR (Status)) {
    FREE_NON_NULL (CacheNodes);
  }

  FREE_NON_NULL (TokenMap);
  return Status;
}

// Gets the data for the CpuCacheNodes (associated with an enabled core)
STATIC
EFI_STATUS
EFIAPI
GetCpuCacheNodeData (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CACHE_TRACKER             *CacheTracker
  )
{
  EFI_STATUS       Status;
  UINT32           NodeIndex;
  CACHE_NODE       *CacheNodes;
  CM_OBJECT_TOKEN  *TokenMap;
  UINT32           NumEnabledCores;
  UINT64           CoreId;
  INT32            CpuCacheOffset;
  UINT32           Socket;
  UINT32           Cluster;
  UINT32           Core;
  UINT32           CoreIndex;

  CacheNodes = NULL;
  TokenMap   = NULL;

  if ((CacheTracker == NULL) ||
      (ParserHandle == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = MpCoreInfoGetPlatformInfo (&NumEnabledCores, NULL, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get MpCoreInfo\n", __FUNCTION__, Status));
    return Status;
  }

  // Allocate space for the nodes (potentially I & D caches per core)
  CacheNodes = (CACHE_NODE *)AllocateZeroPool (sizeof (CACHE_NODE) * (NumEnabledCores * 2));
  if (CacheNodes == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate for CpuCacheNodes\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  // Allocate tokens for the nodes
  Status = NvAllocateCmTokens (ParserHandle, (NumEnabledCores * 2), &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to allocate tokens CpuCacheNodes\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  CpuCacheOffset = -1;
  NodeIndex      = 0;
  for (CoreIndex = 0; CoreIndex < NumEnabledCores; CoreIndex++) {
    // Find the next device = "cpu" node
    Status = DeviceTreeGetNextCpuNode (&CpuCacheOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to find the next Cpu node (index %u) after offset 0x%x\n", __FUNCTION__, Status, CoreIndex, CpuCacheOffset));
      goto CleanupAndReturn;
    }

    // determine the socket, cluster, and core for the cpu
    Status = DeviceTreeGetNodePropertyValue64 (CpuCacheOffset, "reg", &CoreId);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get CoreId (reg property) for CoreIndex %u\n", __FUNCTION__, Status, CoreIndex));
      goto CleanupAndReturn;
    }

    DEBUG_CODE_BEGIN ();
    UINT64  ProcessorId;
    Status = MpCoreInfoGetProcessorIdFromIndex (CoreIndex, &ProcessorId);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get ProcessorId for CoreIndex %u\n", __FUNCTION__, Status, CoreIndex));
      goto CleanupAndReturn;
    }

    NV_ASSERT_RETURN (
      ProcessorId == CoreId,
      { Status = EFI_INVALID_PARAMETER;
        goto CleanupAndReturn;
      },
      "DeviceTree for CoreIndex %u has CoreId = 0x%lx, but expected 0x%lx\n",
      CoreIndex,
      CoreId,
      ProcessorId
      );
    DEBUG_CODE_END ();

    Status = MpCoreInfoGetProcessorLocation (CoreId, &Socket, &Cluster, &Core);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get ProcessorLocation for CoreId %lx\n", __FUNCTION__, Status, CoreId));
      goto CleanupAndReturn;
    }

    // Gather the I & D cache info from the CpuCacheOffset

    // I CacheData
    Status = CreateCacheNodeFromOffset (CacheTracker, &CacheNodes[NodeIndex], CpuCacheOffset, CACHE_TYPE_ICACHE, TokenMap[NodeIndex], Socket, Cluster, Core);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get ICacheData for Logical Core %u\n", __FUNCTION__, Status, CoreIndex));
    } else {
      NodeIndex++;
    }

    // D CacheData
    Status = CreateCacheNodeFromOffset (CacheTracker, &CacheNodes[NodeIndex], CpuCacheOffset, CACHE_TYPE_DCACHE, TokenMap[NodeIndex], Socket, Cluster, Core);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get DCacheData for Logical Core %u\n", __FUNCTION__, Status, CoreIndex));
    } else {
      NodeIndex++;
    }
  }

  // Add the info to the Tracker
  CacheTracker->CpuCacheNodeCount = NodeIndex;
  CacheTracker->CpuCacheNodes     = CacheNodes;

CleanupAndReturn:
  if (EFI_ERROR (Status)) {
    FREE_NON_NULL (CacheNodes);
  }

  FREE_NON_NULL (TokenMap);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
GenerateCacheInfo (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CACHE_TRACKER                *CacheTracker,
  OUT CM_OBJECT_TOKEN              **Tokens
  )
{
  EFI_STATUS         Status;
  CM_OBJ_DESCRIPTOR  Desc;
  UINT32             CacheInfoCount;
  CM_ARM_CACHE_INFO  *CacheInfoStruct;
  CM_OBJECT_TOKEN    *CacheInfoTokens;

  CacheInfoStruct = NULL;
  CacheInfoTokens = NULL;

  if (CacheTracker == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CacheInfoCount = CacheTracker->CacheNodeCount + CacheTracker->CpuCacheNodeCount;
  if (CacheInfoCount > 0) {
    CacheInfoStruct = (CM_ARM_CACHE_INFO *)AllocatePool (sizeof (CM_ARM_CACHE_INFO) * CacheInfoCount);
    if (CacheInfoStruct == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate for CacheInfoStruct\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }

    Status = BuildCacheInfoStruct (CacheTracker, CacheInfoStruct, CacheInfoCount, &CacheInfoTokens);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r from BuildCacheINfoStruct (CacheInfoCount = %u)\n", __FUNCTION__, Status, CacheInfoCount));
      goto CleanupAndReturn;
    }

    Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCacheInfo);
    Desc.Size     = sizeof (CM_ARM_CACHE_INFO) * CacheInfoCount;
    Desc.Count    = CacheInfoCount;
    Desc.Data     = CacheInfoStruct;
    Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, CacheInfoTokens, CM_NULL_TOKEN);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r from NvAddMultipleCmObjWithTokens\n", __FUNCTION__, Status));
      goto CleanupAndReturn;
    }
  } else {
    Status = EFI_NOT_FOUND;
  }

CleanupAndReturn:
  if (EFI_ERROR (Status)) {
    FREE_NON_NULL (CacheInfoStruct);
    FREE_NON_NULL (CacheInfoTokens);
  }

  if (Tokens != NULL) {
    *Tokens = CacheInfoTokens;
  } else {
    FREE_NON_NULL (CacheInfoTokens);
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
FixupSocketClusterCoreFields (
  IN  CACHE_TRACKER  *CacheTracker
  )
{
  UINT32      CpuIndex;
  CACHE_NODE  *Node;
  CACHE_NODE  *Next;

  if (CacheTracker == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Go through all the CpuCacheInfo entries, and propagate their Socket/Cluster/Core info upward
  for (CpuIndex = 0; CpuIndex < CacheTracker->CpuCacheNodeCount; CpuIndex++) {
    Node = &CacheTracker->CpuCacheNodes[CpuIndex];
    Next = Node->NextLevel;

    // Trace the chain up from this Node
    while (Next != NULL) {
      // The first time we see something flow into Next, propagate the source's values
      if ((Next->Socket == UNDEFINED_SOCKET) &&
          (Next->Cluster == UNDEFINED_CLUSTER) &&
          (Next->Core == UNDEFINED_CORE))
      {
        Next->Socket  = Node->Socket;
        Next->Cluster = Node->Cluster;
        Next->Core    = Node->Core;
      } else {
        // Otherwise, Socket should be correct and Cluster and/or Core might be unused
        if (Next->Socket != Node->Socket) {
          DEBUG ((DEBUG_ERROR, "%a: Need a level higher than socket for an L%u cache from CpuIndex %u to flow into\n", __FUNCTION__, Next->CacheData.CacheLevel, CpuIndex));
          return EFI_UNSUPPORTED;
        } else {
          // Sockets match
          if (Next->Cluster != UNUSED_CLUSTER) {
            if (Next->Cluster == Node->Cluster) {
              // Clusters match
              if ((Next->Core != UNUSED_CORE) &&
                  (Next->Core != Node->Core))
              {
                // Core doesn't match, so multiple core-level caches flow into this
                Next->Core = UNUSED_CORE;
              }
            } else {
              // Cluster doesn't match, so multiple cluster-level caches flow into this
              Next->Cluster = UNUSED_CLUSTER;
              Next->Core    = UNUSED_CORE;
            }
          } else if (Next->Cluster != Node->Cluster) {
            // Cluster doesn't match, so multiple cluster-level caches flow into this
            Next->Cluster = UNUSED_CLUSTER;
            Next->Core    = UNUSED_CORE;
          }
        }
      }

      // Follow the chain up
      Node = Next;
      Next = Next->NextLevel;
    }
  }

  // JDS TODO - do we want to sanity-check the result?
  return EFI_SUCCESS;
}

/** Cache info parser function.

  The following structures are populated:
  - EArmObjCacheInfo
  - EArmObjCmRef [for each level of cache hierarchy]

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.
  @param [out] HierarchyInfo   Where to put the structure containing the
                               cache hierarchy information. Caller is
                               responsible for calling FreeCacheHierarchyInfo
                               to free it once no longer needed. Note:
                               Sockets/Clusters/Cores are physical.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
CacheInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE        ParserHandle,
  IN        INT32                        FdtBranch,
  OUT       CACHE_HIERARCHY_INFO_SOCKET  **HierarchyInfo
  )
{
  EFI_STATUS                   Status;
  CACHE_HIERARCHY_INFO_SOCKET  *CacheHierarchyInfo;
  CACHE_TRACKER                *CacheTracker;

  CacheHierarchyInfo = NULL;
  CacheTracker       = NULL;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  // Allocate necessary buffers

  Status = AllocateCacheHierarchyInfo (&CacheHierarchyInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate for CacheHierarchyInfo\n", __FUNCTION__));
    goto CleanupAndReturn;
  }

  CacheTracker = AllocateZeroPool (sizeof (CACHE_TRACKER));
  if (CacheTracker == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate for CacheTracker\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  // Gather and process data

  Status = GetCacheNodeData (ParserHandle, CacheTracker);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  Status = GetCpuCacheNodeData (ParserHandle, CacheTracker);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  Status = FixupSocketClusterCoreFields (CacheTracker);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  // Generate CacheInfo and PrivateResource/hierarchy objects and add them to ConfigManager

  Status = GenerateCacheInfo (ParserHandle, CacheTracker, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  Status = GenerateHierarchyInfo (ParserHandle, CacheTracker, CacheHierarchyInfo);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  if ((HierarchyInfo != NULL) && !EFI_ERROR (Status)) {
    *HierarchyInfo = CacheHierarchyInfo;
  } else {
    FreeCacheHierarchyInfo (CacheHierarchyInfo);
  }

  if (CacheTracker != NULL) {
    FREE_NON_NULL (CacheTracker->CacheNodes);
    FREE_NON_NULL (CacheTracker->CpuCacheNodes);
    FREE_NON_NULL (CacheTracker);
  }

  return Status;
}
