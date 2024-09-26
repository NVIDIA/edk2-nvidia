/** @file
  Proc hierarchy info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/TegraPlatformInfoLib.h>

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
  UINT32                              CoreIndex;
  UINT32                              ProcHierarchyIndex;
  CM_ARCH_COMMON_PROC_HIERARCHY_INFO  *ProcHierarchyInfo;
  UINTN                               ProcHierarchyInfoSize;
  CM_OBJECT_TOKEN                     *SocketTokenMap;
  UINTN                               SocketTokenMapSize;
  CM_OBJECT_TOKEN                     *ClusterTokenMap;
  UINTN                               ClusterTokenMapSize;
  CM_OBJECT_TOKEN                     LpiToken;
  CM_OBJ_DESCRIPTOR                   Desc;
  CACHE_HIERARCHY_INFO_SOCKET         *CacheHierarchyInfo;
  CM_OBJECT_TOKEN                     *GicCInfoTokens;
  CM_OBJECT_TOKEN                     *ProcHierarchyInfoTokens;
  UINTN                               MaxProcHierarchyInfoEntries;
  UINT32                              SocketId;
  UINT32                              ClusterId;
  UINT32                              CoreId;
  UINTN                               ChipID;
  UINT32                              MaxSocket;
  UINT32                              NumSockets;
  UINT32                              MaxClustersPerSocket;
  UINT32                              MaxCoresPerSocket;
  UINT32                              MaxCoresPerCluster;
  CM_OBJECT_TOKEN                     RootToken;
  UINT32                              CoresStartIndex;
  BOOLEAN                             CollapseClusters;
  UINT32                              SearchIndex;
  UINT32                              ClusterIndex;

  ProcHierarchyInfo       = NULL;
  SocketTokenMap          = NULL;
  ClusterTokenMap         = NULL;
  ProcHierarchyInfoTokens = NULL;
  CacheHierarchyInfo      = NULL;
  GicCInfoTokens          = NULL;
  CollapseClusters        = TRUE;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  ChipID = TegraGetChipID ();

  Status = MpCoreInfoGetPlatformInfo (&NumCpus, &MaxSocket, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get PlatformInfo\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  NumSockets           = MaxSocket + 1;
  MaxClustersPerSocket = (PcdGet32 (PcdTegraMaxClusters)) / (PcdGet32 (PcdTegraMaxSockets));
  MaxCoresPerCluster   = PcdGet32 (PcdTegraMaxCoresPerCluster);
  MaxCoresPerSocket    = MaxCoresPerCluster * MaxClustersPerSocket;
  DEBUG ((DEBUG_ERROR, "%a: NumSockets = %u, MaxClustersPerSocket = %u, MaxCoresPerSocket = %u, MaxCoresPerCluster = %u\n", __FUNCTION__, NumSockets, MaxClustersPerSocket, MaxCoresPerSocket, MaxCoresPerCluster));

  Status = LpiParser (ParserHandle, FdtBranch, &LpiToken);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  if (ChipID == T194_CHIP_ID) {
    Status = CacheInfoParserT194 (ParserHandle, FdtBranch, &CacheHierarchyInfo);
  } else {
    Status = CacheInfoParser (ParserHandle, FdtBranch, &CacheHierarchyInfo);
  }

  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  Status = GicCParser (ParserHandle, FdtBranch, &GicCInfoTokens);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  // Gather proc hierarchy info
  SocketTokenMapSize = sizeof (CM_OBJECT_TOKEN) * NumSockets;
  SocketTokenMap     = (CM_OBJECT_TOKEN *)AllocateZeroPool (SocketTokenMapSize);
  if (SocketTokenMap == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  ClusterTokenMapSize = sizeof (CM_OBJECT_TOKEN) * (MaxClustersPerSocket * NumSockets);
  ClusterTokenMap     = (CM_OBJECT_TOKEN *)AllocateZeroPool (ClusterTokenMapSize);
  if (ClusterTokenMap == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  MaxProcHierarchyInfoEntries = (NumCpus + (MaxClustersPerSocket * NumSockets) + NumSockets + (NumSockets > 1 ? 1 : 0));
  ProcHierarchyInfoSize       = sizeof (CM_ARCH_COMMON_PROC_HIERARCHY_INFO) * MaxProcHierarchyInfoEntries;
  ProcHierarchyInfo           = AllocateZeroPool (ProcHierarchyInfoSize);
  if (ProcHierarchyInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  Status = NvAllocateCmTokens (ParserHandle, MaxProcHierarchyInfoEntries, &ProcHierarchyInfoTokens);
  if (EFI_ERROR (Status)) {
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

  // Create Sockets & Clusters
  for (SocketId = 0; SocketId < NumSockets; SocketId++) {
    Status = MpCoreInfoGetSocketInfo (SocketId, NULL, NULL, NULL, NULL);
    if (!EFI_ERROR (Status)) {
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

      // Create Clusters for the socket
      for (ClusterId = 0; ClusterId <= MaxClustersPerSocket; ClusterId++) {
        Status = MpCoreInfoGetSocketClusterInfo (SocketId, ClusterId, NULL, NULL, NULL);
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
          if (CollapseClusters && (CacheHierarchyInfo[SocketId].Cluster[ClusterId].Data.Count != 0)) {
            DEBUG ((DEBUG_ERROR, "%a: Cannot collapse clusters because Socket %u Cluster %u has private resources\n", __FUNCTION__, SocketId, ClusterId));
            CollapseClusters = FALSE;
          }

          ProcHierarchyInfo[ProcHierarchyIndex].OverrideNameUidEnabled = TRUE;
          ProcHierarchyInfo[ProcHierarchyIndex].OverrideUid            = GEN_CONTAINER_UID (2, SocketId, ClusterId, 0);
          ProcHierarchyInfo[ProcHierarchyIndex].OverrideName           = ClusterId;
          ClusterTokenMap[ClusterId + (MaxClustersPerSocket*SocketId)] = ProcHierarchyInfo[ProcHierarchyIndex].Token;
          ProcHierarchyIndex++;
        } else if (Status != EFI_NOT_FOUND) {
          DEBUG ((DEBUG_ERROR, "%a: Got %r getting info about Socket %u Cluster %u\n", __FUNCTION__, Status, SocketId, ClusterId));
        }
      }
    } else if (Status != EFI_NOT_FOUND) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r getting info about Socket %u\n", __FUNCTION__, Status, SocketId));
      goto CleanupAndReturn;
    }
  }

  CoresStartIndex = ProcHierarchyIndex;
  for (CoreIndex = 0; CoreIndex < NumCpus; CoreIndex++) {
    UINT64  ProcessorId;
    Status = MpCoreInfoGetProcessorIdFromIndex (CoreIndex, &ProcessorId);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get ProcessorId for CoreIndex %u\n", __FUNCTION__, Status, CoreIndex));
      goto CleanupAndReturn;
    }

    Status = MpCoreInfoGetProcessorLocation (ProcessorId, &SocketId, &ClusterId, &CoreId);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get ProcessorLocation for ProcessorId %lx\n", __FUNCTION__, Status, ProcessorId));
      goto CleanupAndReturn;
    }

    // Build cpu core node
    ProcHierarchyInfo[ProcHierarchyIndex].Token = ProcHierarchyInfoTokens[ProcHierarchyIndex];
    ProcHierarchyInfo[ProcHierarchyIndex].Flags = PROC_NODE_FLAGS (
                                                    EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                    EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
                                                    EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
                                                    );
    ProcHierarchyInfo[ProcHierarchyIndex].ParentToken                = ClusterTokenMap[ClusterId + (MaxClustersPerSocket*SocketId)];
    ProcHierarchyInfo[ProcHierarchyIndex].AcpiIdObjectToken          = GicCInfoTokens[CoreIndex];
    ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources       = CacheHierarchyInfo[SocketId].Cluster[ClusterId].Cpu[CoreId].Data.Count;
    ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CacheHierarchyInfo[SocketId].Cluster[ClusterId].Cpu[CoreId].Data.Token;
    ProcHierarchyInfo[ProcHierarchyIndex].LpiToken                   = LpiToken;
    ProcHierarchyInfo[ProcHierarchyIndex].OverrideNameUidEnabled     = TRUE;
    ProcHierarchyInfo[ProcHierarchyIndex].OverrideName               = CoreId;
    // Note: No need for OverrideUid, since it's not used for processor nodes
    ProcHierarchyIndex++;
  }

  // Determine if all cores belong to a unique cluster
  for (CoreIndex = 0; (CoreIndex < NumCpus) && CollapseClusters; CoreIndex++) {
    for (SearchIndex = CoreIndex + 1; (SearchIndex < NumCpus) && CollapseClusters; SearchIndex++) {
      if (ProcHierarchyInfo[CoresStartIndex + CoreIndex].ParentToken == ProcHierarchyInfo[CoresStartIndex + SearchIndex].ParentToken) {
        DEBUG ((DEBUG_ERROR, "%a: Cannot collapse clusters because CoreIndex %u and SearchIndex %u have the same ParentToken 0x%x\n", __FUNCTION__, CoreIndex, SearchIndex, ProcHierarchyInfo[CoresStartIndex + CoreIndex].ParentToken));
        CollapseClusters = FALSE;
      }
    }
  }

  // If all cores are part of unique clusters and no cluster has private resources, merge the core into its cluster node
  if (CollapseClusters) {
    // Pop cores off the list and merge them into their parent clusters
    while (ProcHierarchyIndex > CoresStartIndex) {
      ProcHierarchyIndex--;
      for (ClusterIndex = CoresStartIndex; ClusterIndex > 0; ClusterIndex--) {
        if (ProcHierarchyInfo[ClusterIndex].Token == ProcHierarchyInfo[ProcHierarchyIndex].ParentToken) {
          // Copy the ParentToken from the Cluster to the Core
          ProcHierarchyInfo[ProcHierarchyIndex].ParentToken = ProcHierarchyInfo[ClusterIndex].ParentToken;

          // Combine the Cluster name and Core name
          ProcHierarchyInfo[ProcHierarchyIndex].OverrideName += (ProcHierarchyInfo[ClusterIndex].OverrideName * MaxCoresPerCluster);

          // Replace the Cluster with the updated core
          CopyMem (&ProcHierarchyInfo[ClusterIndex], &ProcHierarchyInfo[ProcHierarchyIndex], sizeof (ProcHierarchyInfo[ClusterIndex]));

          // Done with this cluster/core
          DEBUG ((DEBUG_INFO, "%a: Collapsed core at index %u into cluster at index %u\n", __FUNCTION__, ProcHierarchyIndex, ClusterIndex));
          break;
        }
      }

      NV_ASSERT_RETURN (
        ClusterIndex > 0,
        { Status = EFI_DEVICE_ERROR;
          goto CleanupAndReturn;
        },
        "%a: CODE BUG - ClusterIndex reached 0 without finding a parent cluster for CoreIndex %u\n",
        __FUNCTION__,
        ProcHierarchyIndex
        );
    }
  }

  Desc.ObjectId = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjProcHierarchyInfo);
  Desc.Size     = sizeof (CM_ARCH_COMMON_PROC_HIERARCHY_INFO) * (ProcHierarchyIndex);
  Desc.Count    = ProcHierarchyIndex;
  Desc.Data     = ProcHierarchyInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, ProcHierarchyInfoTokens, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  FREE_NON_NULL (ProcHierarchyInfo);
  FREE_NON_NULL (SocketTokenMap);
  FREE_NON_NULL (ClusterTokenMap);
  FREE_NON_NULL (ProcHierarchyInfoTokens);
  FREE_NON_NULL (GicCInfoTokens);
  FreeCacheHierarchyInfo (CacheHierarchyInfo);

  return Status;
}

REGISTER_PARSER_FUNCTION (ProcHierarchyInfoParser, NULL)
