/** @file
  Cache info parser for T194.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "CacheInfoParser.h"
#include <Library/NVIDIADebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MpCoreInfoLib.h>

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

/** Cache Info
 */
STATIC
CM_ARM_CACHE_INFO  CacheInfo_T194[] = {
  // L3 Cache Info
  {
    .Token                 = 0, // Will be populated later
    .NextLevelOfCacheToken = CM_NULL_TOKEN,
    .Size                  = 0x400000,
    .NumberOfSets          = 4096,
    .Associativity         = 16,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
  },
  // L2 Cache Info
  {
    .Token                 = 0,             // Will be populated later
    .NextLevelOfCacheToken = CM_NULL_TOKEN, // Only populated if the next level is private to this hierarchy node
    .Size                  = 0x200000,
    .NumberOfSets          = 2048,
    .Associativity         = 16,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
  },
  // L1I Cache Info
  {
    .Token                 = 0,             // Will be populated later
    .NextLevelOfCacheToken = CM_NULL_TOKEN, // Only populated if the next level is private to this hierarchy node
    .Size                  = 0x20000,
    .NumberOfSets          = 512,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
  },
  // L1D Cache Info
  {
    .Token                 = 0,             // Will be populated later
    .NextLevelOfCacheToken = CM_NULL_TOKEN, // Only populated if the next level is private to this hierarchy node
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
  },
};

/** Cache info parser function for T194.

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
                               responsible for calling FreeCacheHierarchy
                               to free it once no longer needed.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
CacheInfoParserT194 (
  IN  CONST HW_INFO_PARSER_HANDLE        ParserHandle,
  IN        INT32                        FdtBranch,
  OUT       CACHE_HIERARCHY_INFO_SOCKET  **HierarchyInfo
  )
{
  EFI_STATUS                   Status;
  CM_OBJ_DESCRIPTOR            Desc;
  CM_ARM_OBJ_REF               CcplexResources[1];
  CM_ARM_OBJ_REF               CarmelCoreClusterResources[1];
  CM_ARM_OBJ_REF               CarmelCoreResources[2];
  CACHE_HIERARCHY_INFO_SOCKET  *CacheHierarchyInfo;
  CM_OBJECT_TOKEN              *CacheInfoTokens;
  UINT32                       SocketIndex;
  UINT32                       ClusterIndex;
  UINT32                       CoreIndex;
  UINT32                       MaxSocket;
  UINT32                       MaxCluster;
  UINT32                       MaxCore;

  CacheInfoTokens    = NULL;
  CacheHierarchyInfo = NULL;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  // Add the caches
  Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCacheInfo);
  Desc.Size     = sizeof (CacheInfo_T194);
  Desc.Count    = ARRAY_SIZE (CacheInfo_T194);
  Desc.Data     = &CacheInfo_T194;
  Status        = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, &CacheInfoTokens, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  // List which level of hierarchy they are at
  Status = AllocateCacheHierarchyInfo (&CacheHierarchyInfo);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  // Level 3 cache
  CcplexResources[0].ReferenceToken = CacheInfoTokens[0];
  CacheHierarchyInfo[0].Data.Count  = ARRAY_SIZE (CcplexResources);
  Status                            = NvAddSingleCmObj (
                                        ParserHandle,
                                        CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef),
                                        CcplexResources,
                                        sizeof (CcplexResources),
                                        &CacheHierarchyInfo[0].Data.Token
                                        );
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  // Level 2 cache
  CarmelCoreClusterResources[0].ReferenceToken = CacheInfoTokens[1];
  CacheHierarchyInfo->Cluster[0].Data.Count    = ARRAY_SIZE (CarmelCoreClusterResources);
  Status                                       = NvAddSingleCmObj (
                                                   ParserHandle,
                                                   CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef),
                                                   CarmelCoreClusterResources,
                                                   sizeof (CarmelCoreClusterResources),
                                                   &CacheHierarchyInfo->Cluster[0].Data.Token
                                                   );
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  // Level 1 cache
  CarmelCoreResources[0].ReferenceToken            = CacheInfoTokens[2];
  CarmelCoreResources[1].ReferenceToken            = CacheInfoTokens[3];
  CacheHierarchyInfo->Cluster[0].Cpu[0].Data.Count = ARRAY_SIZE (CarmelCoreResources);
  Status                                           = NvAddSingleCmObj (
                                                       ParserHandle,
                                                       CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef),
                                                       CarmelCoreResources,
                                                       sizeof (CarmelCoreResources),
                                                       &CacheHierarchyInfo->Cluster[0].Cpu[0].Data.Token
                                                       );
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  Status = MpCoreInfoGetPlatformInfo (NULL, &MaxSocket, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get MpCoreInfo\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  // Copy Data to other sockets, clusters, and cores
  for (SocketIndex = 0; SocketIndex <= MaxSocket; SocketIndex++) {
    if (!(SocketIndex == 0)) {
      CopyMem (&CacheHierarchyInfo[SocketIndex].Data, &CacheHierarchyInfo[0].Data, sizeof (CACHE_HIERARCHY_INFO_DATA));
    }

    Status = MpCoreInfoGetSocketInfo (SocketIndex, NULL, &MaxCluster, NULL, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get SocketInfo for Socket %u\n", __FUNCTION__, Status, SocketIndex));
      goto CleanupAndReturn;
    }

    for (ClusterIndex = 0; ClusterIndex <= MaxCluster; ClusterIndex++) {
      Status = MpCoreInfoGetSocketClusterInfo (SocketIndex, ClusterIndex, NULL, &MaxCore, NULL);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get SocketClusterInfo for Socket %u Cluster %u\n", __FUNCTION__, Status, SocketIndex, ClusterIndex));
        goto CleanupAndReturn;
      }

      if (!((SocketIndex == 0) && (ClusterIndex == 0))) {
        CopyMem (&CacheHierarchyInfo[SocketIndex].Cluster[ClusterIndex].Data, &CacheHierarchyInfo[0].Cluster[0].Data, sizeof (CACHE_HIERARCHY_INFO_DATA));
      }

      for (CoreIndex = 0; CoreIndex <= MaxCore; CoreIndex++) {
        if (!((SocketIndex == 0) && (ClusterIndex == 0) && (CoreIndex == 0))) {
          CopyMem (&CacheHierarchyInfo[SocketIndex].Cluster[ClusterIndex].Cpu[CoreIndex].Data, &CacheHierarchyInfo[0].Cluster[0].Cpu[0].Data, sizeof (CACHE_HIERARCHY_INFO_DATA));
        }
      }
    }
  }

CleanupAndReturn:
  FREE_NON_NULL (CacheInfoTokens);
  if ((HierarchyInfo != NULL) && !EFI_ERROR (Status)) {
    *HierarchyInfo = CacheHierarchyInfo;
  } else {
    FreeCacheHierarchyInfo (CacheHierarchyInfo);
  }

  return Status;
}
