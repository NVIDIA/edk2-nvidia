/** @file
  Proc hierarchy info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ProcHierarchyInfoParser.h"
#include "../Gic/GicParser.h"
#include "../CacheInfo/CacheInfoParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/BaseMemoryLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>

/** Proc Hierachy info parser function.

  The following structure is populated:
  - EArmObjProcHierarchyInfo
  It requires tokens from the following structures, whose parsers are called as a result:
  - EArmObjLpiInfo
  - EArchCommonObjCmRef (LpiTokens)
  - EArchCommonObjCacheInfo
  - EArchCommonObjCmRef [for each level of cache hierarchy]
  - EArmObjGicCInfo

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
ProcHierarchyInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                          Status;
  UINT32                              NumCpus;
  UINT32                              NumCores;
  UINT32                              CoreIndex;
  UINT32                              ProcHierarchyIndex;
  CM_ARCH_COMMON_PROC_HIERARCHY_INFO  *ProcHierarchyInfo;
  UINTN                               ProcHierarchyInfoSize;
  CM_OBJECT_TOKEN                     *SocketTokenMap;
  UINTN                               SocketTokenMapSize;
  CM_OBJECT_TOKEN                     *ClusterTokenMap;
  UINTN                               ClusterTokenMapSize;
  CM_OBJECT_TOKEN                     *CoreTokenMap;
  UINTN                               CoreTokenMapSize;
  CM_OBJECT_TOKEN                     LpiToken;
  CM_OBJ_DESCRIPTOR                   Desc;
  CACHE_HIERARCHY_INFO_SOCKET         *CacheHierarchyInfo;
  CM_OBJECT_TOKEN                     *GicCInfoTokens;
  CM_OBJECT_TOKEN                     *ProcHierarchyInfoTokens;
  UINTN                               MaxProcHierarchyInfoEntries;
  UINT64                              ProcessorId;
  UINT32                              SocketId;
  UINT32                              ClusterId;
  UINT32                              CoreId;
  UINT32                              ThreadId;
  UINT32                              MaxSocket;
  UINT32                              NumSockets;
  UINT32                              MaxClustersPerSocket;
  UINT32                              MaxCoresPerSocket;
  UINT32                              MaxCoresPerCluster;
  UINT32                              MaxThreadsPerCore;
  CM_OBJECT_TOKEN                     RootToken;
  BOOLEAN                             CreateClusters;

  ProcHierarchyInfo       = NULL;
  SocketTokenMap          = NULL;
  ClusterTokenMap         = NULL;
  CoreTokenMap            = NULL;
  ProcHierarchyInfoTokens = NULL;
  CacheHierarchyInfo      = NULL;
  GicCInfoTokens          = NULL;

  NV_ASSERT_RETURN (
    ParserHandle != NULL,
  {
    return EFI_INVALID_PARAMETER;
  },
    "%a: ParserHandle is NULL\n",
    __FUNCTION__
    );

  Status = MpCoreInfoGetPlatformInfo (&NumCores, &MaxSocket, &MaxClustersPerSocket, &MaxCoresPerCluster, &MaxThreadsPerCore);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: MpCoreInfoGetPlatformInfo failed: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  NumSockets           = MaxSocket + 1;
  MaxClustersPerSocket = MaxClustersPerSocket + 1;
  MaxCoresPerCluster   = MaxCoresPerCluster + 1;
  MaxThreadsPerCore    = MaxThreadsPerCore + 1;
  NumCpus              = NumCores / MaxThreadsPerCore;
  MaxCoresPerSocket    = MaxCoresPerCluster * MaxClustersPerSocket;
  DEBUG ((DEBUG_INFO, "%a: NumSockets = %u, MaxClustersPerSocket = %u, MaxCoresPerSocket = %u, MaxCoresPerCluster = %u\n", __FUNCTION__, NumSockets, MaxClustersPerSocket, MaxCoresPerSocket, MaxCoresPerCluster));
  DEBUG ((DEBUG_INFO, "%a: NumCpus = %u, MaxThreadsPerCore = %u\n", __FUNCTION__, NumCpus, MaxThreadsPerCore));

  Status = LpiParser (ParserHandle, FdtBranch, &LpiToken);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: LpiParser failed: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  Status = CacheInfoParser (ParserHandle, FdtBranch, &CacheHierarchyInfo);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: CacheInfoParser failed: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  Status = GicCParser (ParserHandle, FdtBranch, &GicCInfoTokens);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GicCParser failed: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  // Gather proc hierarchy info
  SocketTokenMapSize = sizeof (CM_OBJECT_TOKEN) * NumSockets;
  SocketTokenMap     = (CM_OBJECT_TOKEN *)AllocateZeroPool (SocketTokenMapSize);
  if (SocketTokenMap == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate SocketTokenMap: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  ClusterTokenMapSize = sizeof (CM_OBJECT_TOKEN) * (MaxClustersPerSocket * NumSockets);
  ClusterTokenMap     = (CM_OBJECT_TOKEN *)AllocateZeroPool (ClusterTokenMapSize);
  if (ClusterTokenMap == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate ClusterTokenMap: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  CoreTokenMapSize = sizeof (CM_OBJECT_TOKEN) * (MaxCoresPerSocket * NumSockets);
  CoreTokenMap     = (CM_OBJECT_TOKEN *)AllocateZeroPool (CoreTokenMapSize);
  if (CoreTokenMap == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate CoreTokenMap: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  // Account for threads, cpus, clusters, sockets and a root node in multi socket scenario
  MaxProcHierarchyInfoEntries = (NumCores + NumCpus +
                                 (MaxClustersPerSocket * NumSockets) + NumSockets + (NumSockets > 1 ? 1 : 0));
  ProcHierarchyInfoSize = sizeof (CM_ARCH_COMMON_PROC_HIERARCHY_INFO) * MaxProcHierarchyInfoEntries;
  ProcHierarchyInfo     = AllocateZeroPool (ProcHierarchyInfoSize);
  if (ProcHierarchyInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate ProcHierarchyInfo: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  Status = NvAllocateCmTokens (ParserHandle, MaxProcHierarchyInfoEntries, &ProcHierarchyInfoTokens);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NvAllocateCmTokens failed: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  ProcHierarchyIndex = 0;

  // Create Root Node if multi-socket
  if (NumSockets > 1) {
    ProcHierarchyInfo[ProcHierarchyIndex].Token = ProcHierarchyInfoTokens[ProcHierarchyIndex];
    ProcHierarchyInfo[ProcHierarchyIndex].Flags = PROC_NODE_FLAGS (
                                                    EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                    EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
                                                    EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
                                                    );
    ProcHierarchyInfo[ProcHierarchyIndex].ParentToken                = CM_NULL_TOKEN;
    ProcHierarchyInfo[ProcHierarchyIndex].AcpiIdObjectToken          = CM_NULL_TOKEN;
    ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources       = 0;
    ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CM_NULL_TOKEN;
    ProcHierarchyInfo[ProcHierarchyIndex].OverrideNameUidEnabled     = TRUE;
    ProcHierarchyInfo[ProcHierarchyIndex].OverrideUid                = GEN_CONTAINER_UID (0, 0, 0, 0);
    ProcHierarchyInfo[ProcHierarchyIndex].OverrideName               = 0;

    RootToken = ProcHierarchyInfo[ProcHierarchyIndex].Token;
    ProcHierarchyIndex++;
  } else {
    RootToken = CM_NULL_TOKEN;
  }

  // Create Sockets
  MPCORE_FOR_EACH_ENABLED_SOCKET (SocketId) {
    ProcHierarchyInfo[ProcHierarchyIndex].Token = ProcHierarchyInfoTokens[ProcHierarchyIndex];
    SocketTokenMap[SocketId]                    = ProcHierarchyInfo[ProcHierarchyIndex].Token;
    ProcHierarchyInfo[ProcHierarchyIndex].Flags = PROC_NODE_FLAGS (
                                                    EFI_ACPI_6_4_PPTT_PACKAGE_PHYSICAL,
                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                    EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
                                                    EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
                                                    );

    ProcHierarchyInfo[ProcHierarchyIndex].ParentToken                = RootToken;
    ProcHierarchyInfo[ProcHierarchyIndex].AcpiIdObjectToken          = CM_NULL_TOKEN;
    ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources       = CacheHierarchyInfo[SocketId].Data.Count;
    ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CacheHierarchyInfo[SocketId].Data.Token;
    ProcHierarchyInfo[ProcHierarchyIndex].OverrideNameUidEnabled     = TRUE;
    ProcHierarchyInfo[ProcHierarchyIndex].OverrideUid                = GEN_CONTAINER_UID (1, SocketId, 0, 0);
    ProcHierarchyInfo[ProcHierarchyIndex].OverrideName               = SocketId;
    ProcHierarchyIndex++;
  }

  CreateClusters = FALSE;
  if (MaxCoresPerCluster > 1) {
    DEBUG ((DEBUG_INFO, "%a: MaxCoresPerCluster > 1, keeping cluster creation\n", __FUNCTION__));
    CreateClusters = TRUE;
  } else {
    MPCORE_FOR_EACH_ENABLED_SOCKET (SocketId) {
      for (ClusterId = 0; ClusterId < MaxClustersPerSocket; ClusterId++) {
        // Check that that cluster exists
        Status = MpCoreInfoGetSocketClusterInfo (SocketId, ClusterId, NULL, NULL, NULL, NULL);
        if (!EFI_ERROR (Status)) {
          if (CacheHierarchyInfo[SocketId].Cluster[ClusterId].Data.Count != 0) {
            DEBUG ((DEBUG_INFO, "%a: Socket %u Cluster %u has private data, keeping cluster creation\n", __FUNCTION__, SocketId, ClusterId));
            CreateClusters = TRUE;
            break;
          }
        }
      }

      if (CreateClusters) {
        break;
      }
    }
  }

  if (CreateClusters) {
    // Create Clusters for each socket
    MPCORE_FOR_EACH_ENABLED_SOCKET (SocketId) {
      // Create Clusters for the socket
      for (ClusterId = 0; ClusterId < MaxClustersPerSocket; ClusterId++) {
        Status = MpCoreInfoGetSocketClusterInfo (SocketId, ClusterId, NULL, NULL, NULL, NULL);
        if (!EFI_ERROR (Status)) {
          ProcHierarchyInfo[ProcHierarchyIndex].Token = ProcHierarchyInfoTokens[ProcHierarchyIndex];
          ProcHierarchyInfo[ProcHierarchyIndex].Flags = PROC_NODE_FLAGS (
                                                          EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
                                                          EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
                                                          EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                          EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
                                                          EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
                                                          );
          ProcHierarchyInfo[ProcHierarchyIndex].ParentToken                = SocketTokenMap[SocketId];
          ProcHierarchyInfo[ProcHierarchyIndex].AcpiIdObjectToken          = CM_NULL_TOKEN;
          ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources       = CacheHierarchyInfo[SocketId].Cluster[ClusterId].Data.Count;
          ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CacheHierarchyInfo[SocketId].Cluster[ClusterId].Data.Token;

          ProcHierarchyInfo[ProcHierarchyIndex].OverrideNameUidEnabled = TRUE;
          ProcHierarchyInfo[ProcHierarchyIndex].OverrideUid            = GEN_CONTAINER_UID (2, SocketId, ClusterId, 0);
          ProcHierarchyInfo[ProcHierarchyIndex].OverrideName           = ClusterId;
          ClusterTokenMap[ClusterId + (MaxClustersPerSocket*SocketId)] = ProcHierarchyInfo[ProcHierarchyIndex].Token;
          ProcHierarchyIndex++;
        } else if (Status != EFI_NOT_FOUND) {
          DEBUG ((DEBUG_ERROR, "%a: MpCoreInfoGetSocketClusterInfo failed for Socket %u Cluster %u: %r\n", __FUNCTION__, SocketId, ClusterId, Status));
          // No need to goto cleanup as this cluster might just not exist
        }
      }
    }
  }

  for (CoreIndex = 0; CoreIndex < NumCores; CoreIndex++) {
    Status = MpCoreInfoGetProcessorIdFromIndex (CoreIndex, &ProcessorId);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: MpCoreInfoGetProcessorIdFromIndex failed for CoreIndex %u: %r\n", __FUNCTION__, CoreIndex, Status));
      goto CleanupAndReturn;
    }

    Status = MpCoreInfoGetProcessorLocation (ProcessorId, &SocketId, &ClusterId, &CoreId, &ThreadId);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: MpCoreInfoGetProcessorLocation failed for ProcessorId 0x%lx: %r\n", __FUNCTION__, ProcessorId, Status));
      goto CleanupAndReturn;
    }

    // Build cpu core node
    ProcHierarchyInfo[ProcHierarchyIndex].Token = ProcHierarchyInfoTokens[ProcHierarchyIndex];
    if (CreateClusters) {
      ProcHierarchyInfo[ProcHierarchyIndex].ParentToken = ClusterTokenMap[ClusterId + (MaxClustersPerSocket*SocketId)];
    } else {
      ProcHierarchyInfo[ProcHierarchyIndex].ParentToken = SocketTokenMap[SocketId];
    }

    if (ThreadId == 0) {
      if (MaxThreadsPerCore > 1) {
        // Only build once per core
        ProcHierarchyInfo[ProcHierarchyIndex].AcpiIdObjectToken = CM_NULL_TOKEN;
        ProcHierarchyInfo[ProcHierarchyIndex].Flags             = PROC_NODE_FLAGS (
                                                                    EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
                                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
                                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                                    EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
                                                                    EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
                                                                    );
        DEBUG ((DEBUG_INFO, "%a: Building multi-thread container ID: %llx Flags: %x Token: %x\n", __FUNCTION__, ProcessorId, ProcHierarchyInfo[ProcHierarchyIndex].Flags, ProcHierarchyInfo[ProcHierarchyIndex].Token));
        ProcHierarchyInfo[ProcHierarchyIndex].LpiToken = CM_NULL_TOKEN;
      } else {
        ProcHierarchyInfo[ProcHierarchyIndex].AcpiIdObjectToken = GicCInfoTokens[CoreIndex];
        ProcHierarchyInfo[ProcHierarchyIndex].Flags             = PROC_NODE_FLAGS (
                                                                    EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
                                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
                                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                                    EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
                                                                    EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
                                                                    );
        DEBUG ((DEBUG_INFO, "%a: Building single-thread object ID: %llx Flags: %x Token: %x\n", __FUNCTION__, ProcessorId, ProcHierarchyInfo[ProcHierarchyIndex].Flags, ProcHierarchyInfo[ProcHierarchyIndex].Token));
        ProcHierarchyInfo[ProcHierarchyIndex].LpiToken = LpiToken;
      }

      ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources       = CacheHierarchyInfo[SocketId].Cluster[ClusterId].Cpu[CoreId].Data.Count;
      ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CacheHierarchyInfo[SocketId].Cluster[ClusterId].Cpu[CoreId].Data.Token;
      ProcHierarchyInfo[ProcHierarchyIndex].OverrideNameUidEnabled     = TRUE;
      ProcHierarchyInfo[ProcHierarchyIndex].OverrideName               = CoreId;
      if (!CreateClusters) {
        // If clusters are not created, then the core name needs to be unique across all cores on the socket
        ProcHierarchyInfo[ProcHierarchyIndex].OverrideName += (ClusterId * MaxCoresPerCluster);
      }

      // This is only used for the container node type if there are threads
      ProcHierarchyInfo[ProcHierarchyIndex].OverrideUid = GEN_CONTAINER_UID (3, SocketId, ClusterId, CoreId);

      CoreTokenMap[CoreId + (MaxCoresPerSocket*SocketId)] = ProcHierarchyInfo[ProcHierarchyIndex].Token;

      ProcHierarchyIndex++;
    }

    // Insert the actual thread nodes if needed
    if (MaxThreadsPerCore > 1) {
      ProcHierarchyInfo[ProcHierarchyIndex].Token = ProcHierarchyInfoTokens[ProcHierarchyIndex];
      ProcHierarchyInfo[ProcHierarchyIndex].Flags = PROC_NODE_FLAGS (
                                                      EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
                                                      EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
                                                      EFI_ACPI_6_4_PPTT_PROCESSOR_IS_THREAD,
                                                      EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
                                                      EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
                                                      );
      ProcHierarchyInfo[ProcHierarchyIndex].ParentToken = CoreTokenMap[CoreId + (MaxCoresPerSocket*SocketId)];
      DEBUG ((DEBUG_INFO, "%a: Building multi-thread object ID: %llx Flags: %x Token: %x ParentToken: %x\n", __FUNCTION__, ProcessorId, ProcHierarchyInfo[ProcHierarchyIndex].Flags, ProcHierarchyInfo[ProcHierarchyIndex].Token, ProcHierarchyInfo[ProcHierarchyIndex].ParentToken));
      ProcHierarchyInfo[ProcHierarchyIndex].AcpiIdObjectToken          = GicCInfoTokens[CoreIndex];
      ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources       = 0;
      ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CM_NULL_TOKEN;
      ProcHierarchyInfo[ProcHierarchyIndex].LpiToken                   = LpiToken;
      ProcHierarchyInfo[ProcHierarchyIndex].OverrideNameUidEnabled     = TRUE;
      ProcHierarchyInfo[ProcHierarchyIndex].OverrideName               = ThreadId;
      ProcHierarchyIndex++;
    }
  }

  Desc.ObjectId = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjProcHierarchyInfo);
  Desc.Size     = sizeof (CM_ARCH_COMMON_PROC_HIERARCHY_INFO) * (ProcHierarchyIndex);
  Desc.Count    = ProcHierarchyIndex;
  Desc.Data     = ProcHierarchyInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, ProcHierarchyInfoTokens, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NvAddMultipleCmObjWithTokens failed: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  FREE_NON_NULL (ProcHierarchyInfo);
  FREE_NON_NULL (SocketTokenMap);
  FREE_NON_NULL (ClusterTokenMap);
  FREE_NON_NULL (ProcHierarchyInfoTokens);
  FREE_NON_NULL (GicCInfoTokens);
  FreeCacheHierarchyInfo (CacheHierarchyInfo);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Exiting with error status: %r\n", __FUNCTION__, Status));
  }

  return Status;
}

REGISTER_PARSER_FUNCTION (ProcHierarchyInfoParser, NULL)
