/** @file
  Proc hierarchy info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "ProcHierarchyInfoParser.h"
#include "Gic/GicParser.h"
#include "CacheInfo/CacheInfoParser.h"
#include <Library/MpCoreInfoLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/NVIDIADebugLib.h>

/** Proc Hierachy info parser function.

  The following structure is populated:
  - EArmObjProcHierarchyInfo
  It requires tokens from the following structures, whose parsers are called as a result:
  - EArmObjLpiInfo
  - EArmObjCmRef (LpiTokens)
  - EArmObjCacheInfo
  - EArmObjCmRef [for each level of cache hierarchy]
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
  EFI_STATUS                   Status;
  UINT32                       NumCpus;
  UINT32                       CoreIndex;
  UINT32                       ProcHierarchyIndex;
  CM_ARM_PROC_HIERARCHY_INFO   *ProcHierarchyInfo;
  UINTN                        ProcHierarchyInfoSize;
  CM_OBJECT_TOKEN              *SocketTokenMap;
  UINTN                        SocketTokenMapSize;
  CM_OBJECT_TOKEN              *ClusterTokenMap;
  UINTN                        ClusterTokenMapSize;
  CM_OBJECT_TOKEN              LpiToken;
  CM_OBJ_DESCRIPTOR            Desc;
  CACHE_HIERARCHY_INFO_SOCKET  *CacheHierarchyInfo;
  CM_OBJECT_TOKEN              *GicCInfoTokens;
  CM_OBJECT_TOKEN              *ProcHierarchyInfoTokens;
  UINTN                        MaxProcHierarchyInfoEntries;
  UINT32                       SocketId;
  UINT32                       ClusterId;
  UINT32                       CoreId;
  UINTN                        ChipID;
  UINT32                       MaxSocket;
  UINT32                       MaxCluster;
  UINT32                       NumSockets;
  UINT32                       NumClustersPerSocket;
  CM_OBJECT_TOKEN              RootToken;

  ProcHierarchyInfo       = NULL;
  SocketTokenMap          = NULL;
  ClusterTokenMap         = NULL;
  ProcHierarchyInfoTokens = NULL;
  CacheHierarchyInfo      = NULL;
  GicCInfoTokens          = NULL;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  ChipID = TegraGetChipID ();

  Status = MpCoreInfoGetPlatformInfo (&NumCpus, &MaxSocket, &MaxCluster, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get PlatformInfo\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  NumSockets           = MaxSocket + 1;
  NumClustersPerSocket = MaxCluster + 1;

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

  ClusterTokenMapSize = sizeof (CM_OBJECT_TOKEN) * (NumClustersPerSocket * NumSockets);
  ClusterTokenMap     = (CM_OBJECT_TOKEN *)AllocateZeroPool (ClusterTokenMapSize);
  if (ClusterTokenMap == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  MaxProcHierarchyInfoEntries = (NumCpus + (NumClustersPerSocket * NumSockets) + NumSockets + (NumSockets > 1 ? 1 : 0));
  ProcHierarchyInfoSize       = sizeof (CM_ARM_PROC_HIERARCHY_INFO) * MaxProcHierarchyInfoEntries;
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
                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_ID_INVALID,
                                                    EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                    EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
                                                    EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
                                                    );
    ProcHierarchyInfo[ProcHierarchyIndex].ParentToken                = CM_NULL_TOKEN;
    ProcHierarchyInfo[ProcHierarchyIndex].GicCToken                  = CM_NULL_TOKEN;
    ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources       = 0;
    ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CM_NULL_TOKEN;

    RootToken = ProcHierarchyInfo[ProcHierarchyIndex].Token;
    ProcHierarchyIndex++;
  } else {
    RootToken = CM_NULL_TOKEN;
  }

  // Create Sockets & Clusters
  for (SocketId = 0; SocketId < NumSockets; SocketId++) {
    Status = MpCoreInfoGetSocketInfo (SocketId, NULL, &MaxCluster, NULL, NULL);
    if (!EFI_ERROR (Status)) {
      ProcHierarchyInfo[ProcHierarchyIndex].Token = ProcHierarchyInfoTokens[ProcHierarchyIndex];
      SocketTokenMap[SocketId]                    = ProcHierarchyInfo[ProcHierarchyIndex].Token;
      ProcHierarchyInfo[ProcHierarchyIndex].Flags = PROC_NODE_FLAGS (
                                                      EFI_ACPI_6_4_PPTT_PACKAGE_PHYSICAL,
                                                      EFI_ACPI_6_4_PPTT_PROCESSOR_ID_INVALID,
                                                      EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                      EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
                                                      EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
                                                      );
      ProcHierarchyInfo[ProcHierarchyIndex].ParentToken                = RootToken;
      ProcHierarchyInfo[ProcHierarchyIndex].GicCToken                  = CM_NULL_TOKEN;
      ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources       = CacheHierarchyInfo[SocketId].Data.Count;
      ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CacheHierarchyInfo[SocketId].Data.Token;
      ProcHierarchyIndex++;

      // Create Clusters for the socket
      for (ClusterId = 0; ClusterId <= MaxCluster; ClusterId++) {
        Status = MpCoreInfoGetSocketClusterInfo (SocketId, ClusterId, NULL, NULL, NULL);
        if (!EFI_ERROR (Status)) {
          ProcHierarchyInfo[ProcHierarchyIndex].Token = ProcHierarchyInfoTokens[ProcHierarchyIndex];
          ProcHierarchyInfo[ProcHierarchyIndex].Flags = PROC_NODE_FLAGS (
                                                          EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
                                                          EFI_ACPI_6_4_PPTT_PROCESSOR_ID_INVALID,
                                                          EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                          EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
                                                          EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
                                                          );
          ProcHierarchyInfo[ProcHierarchyIndex].ParentToken                = SocketTokenMap[SocketId];
          ProcHierarchyInfo[ProcHierarchyIndex].GicCToken                  = CM_NULL_TOKEN;
          ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources       = CacheHierarchyInfo[SocketId].Cluster[ClusterId].Data.Count;
          ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CacheHierarchyInfo[SocketId].Cluster[ClusterId].Data.Token;
          ClusterTokenMap[ClusterId + (NumClustersPerSocket*SocketId)]     = ProcHierarchyInfo[ProcHierarchyIndex].Token;
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
    ProcHierarchyInfo[ProcHierarchyIndex].ParentToken                = ClusterTokenMap[ClusterId + (NumClustersPerSocket*SocketId)];
    ProcHierarchyInfo[ProcHierarchyIndex].GicCToken                  = GicCInfoTokens[CoreIndex];
    ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources       = CacheHierarchyInfo[SocketId].Cluster[ClusterId].Cpu[CoreId].Data.Count;
    ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CacheHierarchyInfo[SocketId].Cluster[ClusterId].Cpu[CoreId].Data.Token;
    ProcHierarchyInfo[ProcHierarchyIndex].LpiToken                   = LpiToken;
    ProcHierarchyIndex++;
  }

  Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjProcHierarchyInfo);
  Desc.Size     = sizeof (CM_ARM_PROC_HIERARCHY_INFO) * (ProcHierarchyIndex);
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
