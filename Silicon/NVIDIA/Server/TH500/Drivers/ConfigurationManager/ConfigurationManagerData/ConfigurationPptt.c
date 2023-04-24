/** @file
  Configuration Manager Library for Processor Topology

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/

#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <ConfigurationManagerObject.h>
#include <Library/FloorSweepingLib.h>
#include <Library/PcdLib.h>
#include <Library/NvgLib.h>
#include <Library/ArmGicLib.h>
#include <Library/ConfigurationManagerLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/PrintLib.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <libfdt.h>
#include "ConfigurationPpttPrivate.h"

LIST_ENTRY  *CachePtrList;

UINT32
EFIAPI
GetCacheIdFrompHandle (
  UINT32  pHandle
  )
{
  CACHE_NODE  *CacheNode;
  LIST_ENTRY  *ListEntry;

  for (ListEntry = GetFirstNode (CachePtrList);
       !IsNull (CachePtrList, ListEntry);
       ListEntry = GetNextNode (CachePtrList, ListEntry))
  {
    CacheNode = CACHE_NODE_FROM_LINK (ListEntry);
    if (CacheNode->PHandle == pHandle ) {
      return CacheNode->CachePtr->CacheId;
    }
  }

  return CM_NULL_TOKEN;
}

// Building the Cache info structure to add as repo object
STATIC
VOID
EFIAPI
BuildCacheInfoStruct (
  CM_ARM_CACHE_INFO  *CacheInfoStruct
  )
{
  CACHE_NODE  *CacheNode;
  LIST_ENTRY  *ListEntry;
  UINT32      Index;

  Index = 0;

  for (ListEntry = GetFirstNode (CachePtrList);
       !IsNull (CachePtrList, ListEntry);
       ListEntry = GetNextNode (CachePtrList, ListEntry))
  {
    CacheNode = CACHE_NODE_FROM_LINK (ListEntry);

    CacheInfoStruct[Index].Token                 = CacheNode->CachePtr->Token;
    CacheInfoStruct[Index].NextLevelOfCacheToken = CacheNode->CachePtr->NextLevelOfCacheToken;
    CacheInfoStruct[Index].Size                  = CacheNode->CachePtr->Size;
    CacheInfoStruct[Index].NumberOfSets          = CacheNode->CachePtr->NumberOfSets;
    CacheInfoStruct[Index].Associativity         = CacheNode->CachePtr->Associativity;
    CacheInfoStruct[Index].Attributes            = CacheNode->CachePtr->Attributes;
    CacheInfoStruct[Index].LineSize              = CacheNode->CachePtr->LineSize;
    CacheInfoStruct[Index].CacheId               = CacheNode->CachePtr->CacheId;
    Index++;
  }
}

// Identify Next Level of Cache token to establish cache hierarchy
STATIC
CM_OBJECT_TOKEN
EFIAPI
FindNextLevelCacheToken (
  UINT32  NextLevelCachepHandle
  )
{
  CACHE_NODE  *CacheNode;
  LIST_ENTRY  *ListEntry;

  for (ListEntry = GetFirstNode (CachePtrList);
       !IsNull (CachePtrList, ListEntry);
       ListEntry = GetNextNode (CachePtrList, ListEntry))
  {
    CacheNode = CACHE_NODE_FROM_LINK (ListEntry);
    if (CacheNode->PHandle == NextLevelCachepHandle ) {
      return CacheNode->CachePtr->Token;
    }
  }

  return CM_NULL_TOKEN;
}

// Collecting L1 cache properties from the cpu node
STATIC
EFI_STATUS
EFIAPI
CollectCacheInfoCpuNode (
  VOID               *DeviceTree,
  INT32              NodeOffset,
  CM_ARM_CACHE_INFO  *CacheInfo
  )
{
  CONST UINT32     *ICacheSize;
  CONST UINT32     *ICacheSets;
  CONST UINT32     *ILineSize;
  CONST UINT32     *DCacheSize;
  CONST UINT32     *DCacheSets;
  CONST UINT32     *DLineSize;
  CONST UINT32     *NextLevelCache;
  UINT32           NextLevelCachepHandle;
  CM_OBJECT_TOKEN  NextLevelCacheToken;
  INT32            Length;

  ICacheSize = fdt_getprop (DeviceTree, NodeOffset, "i-cache-size", &Length);
  if (ICacheSize == NULL ) {
    return EFI_INVALID_PARAMETER;
  }

  CacheInfo[0].Size = SwapBytes32 (*ICacheSize);

  // Parse the "cache-line-size" property (mandatory)
  ILineSize = fdt_getprop (DeviceTree, NodeOffset, "i-cache-line-size", &Length);
  if (ILineSize == NULL ) {
    return EFI_INVALID_PARAMETER;
  }

  CacheInfo[0].LineSize = SwapBytes32 (*ILineSize);

  // Parse the "cache-sets" property (mandatory)
  ICacheSets = fdt_getprop (DeviceTree, NodeOffset, "i-cache-sets", &Length);
  if (ICacheSets == NULL ) {
    return EFI_INVALID_PARAMETER;
  }

  CacheInfo[0].NumberOfSets = SwapBytes32 (*ICacheSets);

  // Calculate Associativity
  CacheInfo[0].Associativity = CacheInfo[0].Size / (CacheInfo[0].LineSize * CacheInfo[0].NumberOfSets);

  // Assign Attributes
  CacheInfo[0].Attributes =  CACHE_ATTRIBUTES (
                               EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               );

  // D-Cache
  DCacheSize = fdt_getprop (DeviceTree, NodeOffset, "d-cache-size", &Length);
  if (DCacheSize == NULL ) {
    return EFI_INVALID_PARAMETER;
  }

  CacheInfo[1].Size = SwapBytes32 (*DCacheSize);

  // Parse the "cache-line-size" property (mandatory)
  DLineSize = fdt_getprop (DeviceTree, NodeOffset, "d-cache-line-size", &Length);
  if (DLineSize == NULL ) {
    return EFI_INVALID_PARAMETER;
  }

  CacheInfo[1].LineSize = SwapBytes32 (*DLineSize);

  // Parse the "cache-sets" property (mandatory)
  DCacheSets = fdt_getprop (DeviceTree, NodeOffset, "d-cache-sets", &Length);
  if (DCacheSets == NULL ) {
    return EFI_INVALID_PARAMETER;
  }

  CacheInfo[1].NumberOfSets = SwapBytes32 (*DCacheSets);

  // Calculate Associativity
  CacheInfo[1].Associativity = CacheInfo[1].Size / (CacheInfo[1].LineSize * CacheInfo[1].NumberOfSets);

  // Assign Attributes
  CacheInfo[1].Attributes =  CACHE_ATTRIBUTES (
                               EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               );

  // "next-level-cache" property (optional property)
  NextLevelCache = fdt_getprop (DeviceTree, NodeOffset, "next-level-cache", &Length);
  if (NextLevelCache == NULL) {
    CacheInfo[0].NextLevelOfCacheToken = CM_NULL_TOKEN;
    CacheInfo[1].NextLevelOfCacheToken = CM_NULL_TOKEN;
  } else {
    // go through Cache linked list and find the next level token
    NextLevelCachepHandle              = SwapBytes32 (*NextLevelCache);
    NextLevelCacheToken                = FindNextLevelCacheToken (NextLevelCachepHandle);
    CacheInfo[0].NextLevelOfCacheToken = NextLevelCacheToken;
    CacheInfo[1].NextLevelOfCacheToken = NextLevelCacheToken;
  }

  CacheInfo[0].Token = REFERENCE_TOKEN (CacheInfo[0]);
  CacheInfo[1].Token = REFERENCE_TOKEN (CacheInfo[1]);

  return EFI_SUCCESS;
}

// Collecting cache properties for non cpu nodes.
STATIC
EFI_STATUS
EFIAPI
GetCacheInfo (
  VOID               *Dtb,
  INT32              CacheOffset,
  CM_ARM_CACHE_INFO  *CacheInfo
  )
{
  CONST UINT32  *Property;

  Property = fdt_getprop (Dtb, CacheOffset, "cache-size", NULL);
  if (Property != NULL) {
    CacheInfo->Size = SwapBytes32 (*Property);
  }

  Property = fdt_getprop (Dtb, CacheOffset, "cache-sets", NULL);
  if (Property != NULL) {
    CacheInfo->NumberOfSets = SwapBytes32 (*Property);
  }

  Property = fdt_getprop (Dtb, CacheOffset, "cache-line-size", NULL);
  if (Property != NULL) {
    CacheInfo->LineSize = SwapBytes32 (*Property);
  }

  // Calculate Associativity
  CacheInfo->Associativity = CacheInfo->Size / (CacheInfo->LineSize * CacheInfo->NumberOfSets);

  // Assign Attributes
  CacheInfo->Attributes =  CACHE_ATTRIBUTES (
                             EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                             EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                             EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                             );

  CacheInfo->NextLevelOfCacheToken = CM_NULL_TOKEN;

  CacheInfo->Token = REFERENCE_TOKEN (CacheInfo[0]);

  return EFI_SUCCESS;
}

// Retrieve the node offset of a node whose pHandle is read as a property
STATIC
INT32
EFIAPI
GetNodeOffsetFromHandleRef  (
  CONST VOID   *Dtb,
  INT32        NodeOffset,
  CONST CHAR8  *PropStr
  )
{
  CONST UINT32  *Property;
  INT32         Length;
  UINT32        PropertypHandle;
  INT32         PropNodeOffset;

  Property = fdt_getprop (Dtb, NodeOffset, PropStr, &Length);
  if (Property == NULL ) {
    return -1;
  }

  PropertypHandle = SwapBytes32 (*Property);
  PropNodeOffset  = fdt_node_offset_by_phandle (Dtb, PropertypHandle);

  return PropNodeOffset;
}

/** Initialize the cache resources and proc hierarchy entries in the platform configuration repository.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
EFI_STATUS
EFIAPI
UpdateCpuInfo (
  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo
  )
{
  EFI_STATUS                      Status;
  CM_ARM_PROC_HIERARCHY_INFO      *ProcHierarchyInfo;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  VOID                            *Dtb;
  UINTN                           DtbSize;
  UINT32                          CacheNodeCntr;
  INT32                           NodeOffset;
  INT32                           PrevNodeOffset;
  UINT32                          NumCpus;
  UINT32                          ProcHierarchyIndex;
  CACHE_NODE                      *CacheNode;
  CM_ARM_CACHE_INFO               *CacheInfoStruct;
  CM_ARM_CACHE_INFO               *CacheInfo;
  CM_OBJECT_TOKEN                 *SocketTokenMap;
  CM_OBJECT_TOKEN                 PrivateResources[10];
  CM_OBJECT_TOKEN                 *SocketPrivateResources;
  CM_OBJECT_TOKEN                 *CorePrivateResources;
  UINT32                          EnabledCoreCntr;
  UINT32                          ResIndex;
  UINT32                          *CpuIdleHandles;
  UINT32                          Index;
  UINT32                          NumberOfCpuIdles;
  UINT32                          NumberOfLpiStates;
  CM_OBJECT_TOKEN                 LpiToken;
  CM_OBJECT_TOKEN                 *LpiTokenMap;
  CM_ARM_LPI_INFO                 *LpiInfo;
  VOID                            *DeviceTreeBase;
  CONST VOID                      *Property;
  INT32                           PropertyLen;
  UINT32                          WakeupLatencyUs;

  CacheNodeCntr   = 0;
  EnabledCoreCntr = 0;

  CachePtrList = AllocateZeroPool (sizeof (LIST_ENTRY));
  InitializeListHead (CachePtrList);

  Repo = *PlatformRepositoryInfo;

  NumCpus = GetNumberOfEnabledCpuCores ();

  // Build LPI stuctures
  NumberOfCpuIdles = 0;

  Status = GetMatchingEnabledDeviceTreeNodes ("arm,idle-state", NULL, &NumberOfCpuIdles);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    NumberOfCpuIdles = 0;
  } else {
    CpuIdleHandles = AllocateZeroPool (sizeof (UINT32) * NumberOfCpuIdles);
    if (CpuIdleHandles == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for cpuidle cores\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    Status = GetMatchingEnabledDeviceTreeNodes ("arm,idle-state", CpuIdleHandles, &NumberOfCpuIdles);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get cpuidle cores %r\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  // 1 extra for WFI state
  LpiTokenMap = AllocateZeroPool (sizeof (CM_OBJECT_TOKEN) * (NumberOfCpuIdles + 1));
  if (LpiTokenMap == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for lpi token map\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  LpiToken = REFERENCE_TOKEN (LpiTokenMap);

  LpiInfo = AllocateZeroPool (sizeof (CM_ARM_LPI_INFO) * (NumberOfCpuIdles + 1));
  if (LpiInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for lpi info\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index <= NumberOfCpuIdles; Index++) {
    LpiTokenMap[Index] = REFERENCE_TOKEN (LpiInfo[Index]);
  }

  NumberOfLpiStates = 0;

  // Create WFI entry
  LpiInfo[NumberOfLpiStates].MinResidency                          = 1;
  LpiInfo[NumberOfLpiStates].WorstCaseWakeLatency                  = 1;
  LpiInfo[NumberOfLpiStates].Flags                                 = 1;
  LpiInfo[NumberOfLpiStates].ArchFlags                             = 0;
  LpiInfo[NumberOfLpiStates].EnableParentState                     = FALSE;
  LpiInfo[NumberOfLpiStates].IsInteger                             = FALSE;
  LpiInfo[NumberOfLpiStates].RegisterEntryMethod.AccessSize        = 3;
  LpiInfo[NumberOfLpiStates].RegisterEntryMethod.Address           = 0xFFFFFFFF;
  LpiInfo[NumberOfLpiStates].RegisterEntryMethod.AddressSpaceId    = EFI_ACPI_6_3_FUNCTIONAL_FIXED_HARDWARE;
  LpiInfo[NumberOfLpiStates].RegisterEntryMethod.RegisterBitOffset = 0;
  LpiInfo[NumberOfLpiStates].RegisterEntryMethod.RegisterBitWidth  = 0x20;
  CopyMem (LpiInfo[NumberOfLpiStates].StateName, "WFI", sizeof ("WFI"));

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjLpiInfo);
  Repo->CmObjectToken = REFERENCE_TOKEN (LpiInfo[NumberOfLpiStates]);
  Repo->CmObjectSize  = sizeof (CM_ARM_LPI_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = &LpiInfo[NumberOfLpiStates];
  Repo++;
  NumberOfLpiStates++;

  for (Index = 0; Index < NumberOfCpuIdles; Index++) {
    Status = GetDeviceTreeNode (CpuIdleHandles[Index], &DeviceTreeBase, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get idle state node - %r\r\n", Status));
      continue;
    }

    Property = fdt_getprop (DeviceTreeBase, NodeOffset, "arm,psci-suspend-param", NULL);
    if (Property == NULL) {
      DEBUG ((DEBUG_ERROR, "Failed to get psci-suspend-param\r\n"));
      continue;
    }

    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.Address = SwapBytes32 (*(CONST UINT32 *)Property);

    Property = fdt_getprop (DeviceTreeBase, NodeOffset, "min-residency-us", NULL);
    if (Property == NULL) {
      DEBUG ((DEBUG_ERROR, "Failed to get min-residency-us\r\n"));
      continue;
    }

    LpiInfo[NumberOfLpiStates].MinResidency = SwapBytes32 (*(CONST UINT32 *)Property);

    Property = fdt_getprop (DeviceTreeBase, NodeOffset, "wakeup-latency-us", NULL);
    if (Property == NULL) {
      Property = fdt_getprop (DeviceTreeBase, NodeOffset, "entry-latency-us", NULL);
      if (Property == NULL) {
        DEBUG ((DEBUG_ERROR, "Failed to get entry-latency-us\r\n"));
        continue;
      }

      WakeupLatencyUs = SwapBytes32 (*(CONST UINT32 *)Property);
      Property        = fdt_getprop (DeviceTreeBase, NodeOffset, "exit-latency-us", NULL);
      if (Property == NULL) {
        DEBUG ((DEBUG_ERROR, "Failed to get exit-latency-us\r\n"));
        continue;
      }

      WakeupLatencyUs += SwapBytes32 (*(CONST UINT32 *)Property);
    } else {
      WakeupLatencyUs = SwapBytes32 (*(CONST UINT32 *)Property);
    }

    LpiInfo[NumberOfLpiStates].WorstCaseWakeLatency = WakeupLatencyUs;

    LpiInfo[NumberOfLpiStates].Flags                                 = 1;
    LpiInfo[NumberOfLpiStates].ArchFlags                             = 1;
    LpiInfo[NumberOfLpiStates].EnableParentState                     = TRUE;
    LpiInfo[NumberOfLpiStates].IsInteger                             = FALSE;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.AccessSize        = 3;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.AddressSpaceId    = EFI_ACPI_6_3_FUNCTIONAL_FIXED_HARDWARE;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.RegisterBitOffset = 0;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.RegisterBitWidth  = 0x20;
    Property                                                         = fdt_getprop (DeviceTreeBase, NodeOffset, "idle-state-name", &PropertyLen);
    if (Property != NULL) {
      CopyMem (LpiInfo[NumberOfLpiStates].StateName, Property, PropertyLen);
    }

    Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjLpiInfo);
    Repo->CmObjectToken = REFERENCE_TOKEN (LpiInfo[NumberOfLpiStates]);
    Repo->CmObjectSize  = sizeof (CM_ARM_LPI_INFO);
    Repo->CmObjectCount = 1;
    Repo->CmObjectPtr   = &LpiInfo[NumberOfLpiStates];
    Repo++;

    NumberOfLpiStates++;
  }

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (LpiTokenMap);
  Repo->CmObjectSize  = sizeof (CM_OBJECT_TOKEN) * NumberOfLpiStates;
  Repo->CmObjectCount = NumberOfLpiStates;
  Repo->CmObjectPtr   = LpiTokenMap;
  Repo++;

  // TODO: Get Enabled Sockets and enabled Cluster info

  // Space for top level node, sockets, clusters and cores
  ProcHierarchyInfo = AllocateZeroPool (sizeof (CM_ARM_PROC_HIERARCHY_INFO) * (PLATFORM_MAX_SOCKETS + PLATFORM_MAX_CLUSTERS + NumCpus + 1));
  if (ProcHierarchyInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  SocketTokenMap = (CM_OBJECT_TOKEN *)AllocateZeroPool (sizeof (CM_OBJECT_TOKEN) * (PLATFORM_MAX_SOCKETS));
  if (SocketTokenMap == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = DtPlatformLoadDtb (&Dtb, &DtbSize);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Build top level node
  ProcHierarchyIndex = 0;

  // loop through all sockets and see if there are private resources for socket
  INT32   SocketOffset;
  UINT32  SocketPrivateResourceCntr;
  CHAR8   SocketNodeStr[] = "/socket@xx";

  // This could be different for Tegra vs Server
  for (UINT32 Socket = 0; Socket < PLATFORM_MAX_SOCKETS; Socket++) {
    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u", Socket);
    SocketOffset              = fdt_path_offset (Dtb, SocketNodeStr);
    SocketPrivateResourceCntr = 0;
    ResIndex                  = 0;

    // check if socket exists
    if (SocketOffset < 0) {
      continue;
    }

    NodeOffset     = 0;
    PrevNodeOffset = 0;
    for (NodeOffset = fdt_first_subnode (Dtb, SocketOffset);
         NodeOffset > 0;
         NodeOffset = fdt_next_subnode (Dtb, PrevNodeOffset))
    {
      CONST VOID  *Property;
      INT32       Length;

      Property       = fdt_getprop (Dtb, NodeOffset, "device_type", &Length);
      PrevNodeOffset = NodeOffset;
      if ((Property == NULL) || (AsciiStrCmp (Property, "cache") != 0)) {
        continue;
      }

      // Gather the cache info and increment Private resources counter
      // Allocate space for this Cache node
      CacheNode = (CACHE_NODE *)AllocateZeroPool (sizeof (CACHE_NODE));
      if (CacheNode == NULL) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to allocate for CacheNode\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      CacheInfo = (CM_ARM_CACHE_INFO *)AllocateZeroPool (sizeof (CM_ARM_CACHE_INFO));
      if (CacheInfo == NULL) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to allocate for CacheInfo\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      // Obtain the cache info into the allocated space
      Status             = GetCacheInfo (Dtb, NodeOffset, CacheInfo);
      CacheInfo->CacheId = GET_CACHE_ID (3, CACHE_TYPE_UNIFIED, 0, 0, Socket);

      // Add this cache node to the linked list
      CacheNode->Signature = CACHE_NODE_SIGNATURE;
      CacheNode->PHandle   = fdt_get_phandle (Dtb, NodeOffset);
      CacheNode->CachePtr  = CacheInfo;
      InsertHeadList (CachePtrList, &CacheNode->Link);
      PrivateResources[ResIndex] = REFERENCE_TOKEN (CacheInfo[0]);
      ResIndex++;

      CacheNodeCntr++;

      SocketPrivateResourceCntr++;
    }

    if (SocketPrivateResourceCntr > 0) {
      SocketPrivateResources = (CM_OBJECT_TOKEN *)AllocateZeroPool (sizeof (CM_OBJECT_TOKEN) * SocketPrivateResourceCntr);
      if (SocketPrivateResources == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      for (UINT32 RefIndex = 0; RefIndex < SocketPrivateResourceCntr; RefIndex++) {
        SocketPrivateResources[RefIndex] = PrivateResources[RefIndex];
      }

      // Create object in repo
      Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
      Repo->CmObjectToken = REFERENCE_TOKEN (*SocketPrivateResources);
      Repo->CmObjectSize  = sizeof (SocketPrivateResources) * SocketPrivateResourceCntr;
      Repo->CmObjectCount = SocketPrivateResourceCntr;
      Repo->CmObjectPtr   = SocketPrivateResources;
      Repo++;
    }

    // Build Socket Proc topology node and socket token map
    ProcHierarchyInfo[ProcHierarchyIndex].Token = REFERENCE_TOKEN (ProcHierarchyInfo[ProcHierarchyIndex]);
    ProcHierarchyInfo[ProcHierarchyIndex].Flags = PROC_NODE_FLAGS (
                                                    EFI_ACPI_6_3_PPTT_PACKAGE_PHYSICAL,
                                                    EFI_ACPI_6_3_PPTT_PROCESSOR_ID_INVALID,
                                                    EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                    EFI_ACPI_6_3_PPTT_NODE_IS_NOT_LEAF,
                                                    EFI_ACPI_6_3_PPTT_IMPLEMENTATION_IDENTICAL
                                                    );
    ProcHierarchyInfo[ProcHierarchyIndex].ParentToken          = CM_NULL_TOKEN;
    ProcHierarchyInfo[ProcHierarchyIndex].GicCToken            = CM_NULL_TOKEN;
    ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources = SocketPrivateResourceCntr;
    if (SocketPrivateResourceCntr > 0) {
      ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken =  REFERENCE_TOKEN (*SocketPrivateResources);
    } else {
      ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CM_NULL_TOKEN;
    }

    ProcHierarchyInfo[ProcHierarchyIndex].OverrideNameUidEnabled = TRUE;
    ProcHierarchyInfo[ProcHierarchyIndex].OverrideUid            = Socket;
    ProcHierarchyInfo[ProcHierarchyIndex].OverrideName           = Socket;

    SocketTokenMap[Socket] = ProcHierarchyInfo[ProcHierarchyIndex].Token;

    ProcHierarchyIndex++;

    // Now Loop through cluster topology using cpu-map path
    INT32  CpusOffset;
    INT32  CpuMapOffset;
    INT32  ClusterOffset;
    INT32  Cluster;
    CHAR8  ClusterNodeStr[] = "clusterxx";
    CHAR8  CpusNodeStr[]    = "/socket@xxxxxxx";

    AsciiSPrint (CpusNodeStr, sizeof (CpusNodeStr), "/socket@%u/cpus", Socket);
    CpusOffset = fdt_path_offset (Dtb, CpusNodeStr);
    if (CpusOffset < 0) {
      DEBUG ((EFI_D_ERROR, "Failed to find /cpus node\n"));
      continue;
    }

    CpuMapOffset = fdt_subnode_offset (Dtb, CpusOffset, "cpu-map");
    if (CpuMapOffset < 0) {
      DEBUG ((EFI_D_ERROR, "/cpus/cpu-map does not exist\r\n"));
      continue;
    }

    // Run a for loop for max clusters per socket
    for (Cluster = 0;
         Cluster < PLATFORM_MAX_CLUSTERS/PLATFORM_MAX_SOCKETS;
         Cluster++)
    {
      AsciiSPrint (ClusterNodeStr, sizeof (ClusterNodeStr), "cluster%d", Cluster);
      ClusterOffset = fdt_subnode_offset (Dtb, CpuMapOffset, ClusterNodeStr);
      if (ClusterOffset >= 0) {
        // Loop through all cores and collect resources
        INT32   Core;
        CHAR8   CoreNodeStr[] = "corexx";
        INT32   CoreOffset;
        INT32   CpuCacheOffset;
        INT32   LCacheOffset;
        UINT32  CorePrivateResourceCntr;

        // Run a for loop for max cores per cluster
        for (Core = 0; Core < PLATFORM_MAX_CORES_PER_CLUSTER; Core++) {
          CorePrivateResourceCntr = 0;
          ResIndex                = 0;
          AsciiSPrint (CoreNodeStr, sizeof (CoreNodeStr), "core%d", Core);
          CoreOffset = fdt_subnode_offset (Dtb, ClusterOffset, CoreNodeStr);

          // check if core exists
          if (CoreOffset >= 0) {
            // TODO: Dynamically scan all properties

            CpuCacheOffset = GetNodeOffsetFromHandleRef (Dtb, CoreOffset, "cpu");
            LCacheOffset   = GetNodeOffsetFromHandleRef (Dtb, CoreOffset, "l2-cache");

            // Gather cpu private cache resources

            if (LCacheOffset > 0) {
              CorePrivateResourceCntr++;
              // Allocate space for this Cache node
              CacheInfo = (CM_ARM_CACHE_INFO *)AllocateZeroPool (sizeof (CM_ARM_CACHE_INFO));
              if (CacheInfo == NULL) {
                DEBUG ((EFI_D_ERROR, "%a: Failed to allocate for CacheInfo\r\n", __FUNCTION__));
                Status = EFI_OUT_OF_RESOURCES;
                goto Exit;
              }

              Status             = GetCacheInfo (Dtb, LCacheOffset, CacheInfo);
              CacheInfo->CacheId = GET_CACHE_ID (2, CACHE_TYPE_UNIFIED, Core, Cluster, Socket);
              CacheNode          = (CACHE_NODE *)AllocateZeroPool (sizeof (CACHE_NODE));
              if (CacheNode == NULL) {
                DEBUG ((EFI_D_ERROR, "%a: Failed to allocate for CacheNode\r\n", __FUNCTION__));
                Status = EFI_OUT_OF_RESOURCES;
                goto Exit;
              }

              CacheNode->Signature = CACHE_NODE_SIGNATURE;
              CacheNode->PHandle   = fdt_get_phandle (Dtb, LCacheOffset);
              CacheNode->CachePtr  = CacheInfo;
              InsertHeadList (CachePtrList, &CacheNode->Link);
              PrivateResources[ResIndex] = REFERENCE_TOKEN (CacheInfo[0]);
              ResIndex++;
            }

            if (CpuCacheOffset > 0) {
              CorePrivateResourceCntr += 2;
              // Allocate space for I cache and D cache for this cache node
              CacheInfo = (CM_ARM_CACHE_INFO *)AllocateZeroPool (sizeof (CM_ARM_CACHE_INFO) * 2);
              if (CacheInfo == NULL) {
                DEBUG ((EFI_D_ERROR, "%a: Failed to allocate for CacheInfo\r\n", __FUNCTION__));
                Status = EFI_OUT_OF_RESOURCES;
                goto Exit;
              }

              Status               = CollectCacheInfoCpuNode (Dtb, CpuCacheOffset, CacheInfo);
              CacheInfo[0].CacheId = GET_CACHE_ID (1, CACHE_TYPE_ICACHE, Core, Cluster, Socket);
              CacheInfo[1].CacheId = GET_CACHE_ID (1, CACHE_TYPE_DCACHE, Core, Cluster, Socket);

              // I Cache
              CacheNode = (CACHE_NODE *)AllocateZeroPool (sizeof (CACHE_NODE));
              if (CacheNode == NULL) {
                DEBUG ((EFI_D_ERROR, "%a: Failed to allocate for CacheNode\r\n", __FUNCTION__));
                Status = EFI_OUT_OF_RESOURCES;
                goto Exit;
              }

              CacheNode->Signature = CACHE_NODE_SIGNATURE;
              CacheNode->PHandle   = fdt_get_phandle (Dtb, CpuCacheOffset);
              CacheNode->CachePtr  = &CacheInfo[0];
              InsertHeadList (CachePtrList, &CacheNode->Link);
              PrivateResources[ResIndex] = REFERENCE_TOKEN (CacheInfo[0]);
              ResIndex++;

              // D Cache
              CacheNode = (CACHE_NODE *)AllocateZeroPool (sizeof (CACHE_NODE));
              if (CacheNode == NULL) {
                DEBUG ((EFI_D_ERROR, "%a: Failed to allocate for CacheNode\r\n", __FUNCTION__));
                Status = EFI_OUT_OF_RESOURCES;
                goto Exit;
              }

              CacheNode->Signature = CACHE_NODE_SIGNATURE;
              CacheNode->CachePtr  = &CacheInfo[1];
              InsertHeadList (CachePtrList, &CacheNode->Link);
              PrivateResources[ResIndex] = REFERENCE_TOKEN (CacheInfo[1]);
            }

            CacheNodeCntr = CacheNodeCntr + CorePrivateResourceCntr;

            if (CorePrivateResourceCntr > 0) {
              CorePrivateResources = (CM_OBJECT_TOKEN *)AllocateZeroPool (sizeof (CM_OBJECT_TOKEN) * CorePrivateResourceCntr);
              if (CorePrivateResources == NULL) {
                Status = EFI_OUT_OF_RESOURCES;
                goto Exit;
              }

              for (UINT32 RefIndex = 0; RefIndex < CorePrivateResourceCntr; RefIndex++) {
                CorePrivateResources[RefIndex] = PrivateResources[RefIndex];
              }

              // Add CM_OBJ_REF to repo
              Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
              Repo->CmObjectToken = REFERENCE_TOKEN (*CorePrivateResources);
              Repo->CmObjectSize  = sizeof (CorePrivateResources) * CorePrivateResourceCntr;
              Repo->CmObjectCount = CorePrivateResourceCntr;
              Repo->CmObjectPtr   = CorePrivateResources;
              Repo++;
            }

            // Create proc topology Node for Core
            ProcHierarchyInfo[ProcHierarchyIndex].Token = REFERENCE_TOKEN (ProcHierarchyInfo[ProcHierarchyIndex]);
            ProcHierarchyInfo[ProcHierarchyIndex].Flags = PROC_NODE_FLAGS (
                                                            EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
                                                            EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID,
                                                            EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                            EFI_ACPI_6_3_PPTT_NODE_IS_LEAF,
                                                            EFI_ACPI_6_3_PPTT_IMPLEMENTATION_NOT_IDENTICAL
                                                            );
            ProcHierarchyInfo[ProcHierarchyIndex].ParentToken          = SocketTokenMap[Socket];
            ProcHierarchyInfo[ProcHierarchyIndex].GicCToken            = GetGicCToken (EnabledCoreCntr);
            ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources = CorePrivateResourceCntr;
            if (CorePrivateResourceCntr > 0) {
              ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken =  REFERENCE_TOKEN (*CorePrivateResources);
            } else {
              ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = CM_NULL_TOKEN;
            }

            ProcHierarchyInfo[ProcHierarchyIndex].LpiToken               = LpiToken;
            ProcHierarchyInfo[ProcHierarchyIndex].OverrideNameUidEnabled = TRUE;
            ProcHierarchyInfo[ProcHierarchyIndex].OverrideName           = (Cluster * PLATFORM_MAX_CORES_PER_CLUSTER) + Core;

            ProcHierarchyIndex++;
            EnabledCoreCntr++;
          } else {
            continue;
          }
        } // loop for core
      } else {
        continue;
      }
    } // loop for cluster
  }

  // Ensure EnabledCoreCntr is same as the floorswept value
  ASSERT (EnabledCoreCntr == NumCpus);

  FreePool (SocketTokenMap);

  // Build the CacheInfoStruct Table using the CachePtr list generated before
  if (CacheNodeCntr > 0) {
    CacheInfoStruct = (CM_ARM_CACHE_INFO *)AllocatePool (sizeof (CM_ARM_CACHE_INFO) * CacheNodeCntr);
    if (CacheInfoStruct == NULL) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate for CacheInfoStruct\r\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    BuildCacheInfoStruct (CacheInfoStruct);

    Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCacheInfo);
    Repo->CmObjectToken = CM_NULL_TOKEN;
    Repo->CmObjectSize  = sizeof (CM_ARM_CACHE_INFO) * CacheNodeCntr;
    Repo->CmObjectCount = CacheNodeCntr;
    Repo->CmObjectPtr   = CacheInfoStruct;
    Repo++;
  }

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjProcHierarchyInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_ARM_PROC_HIERARCHY_INFO) * (ProcHierarchyIndex);
  Repo->CmObjectCount = ProcHierarchyIndex;
  Repo->CmObjectPtr   = ProcHierarchyInfo;
  Repo++;

  *PlatformRepositoryInfo = Repo;

Exit:
  if (EFI_ERROR (Status)) {
    if (SocketTokenMap != NULL) {
      FreePool (SocketTokenMap);
    }

    if (ProcHierarchyInfo != NULL) {
      FreePool (ProcHierarchyInfo);
    }

    if (SocketPrivateResources != NULL) {
      FreePool (SocketPrivateResources);
    }

    if (CorePrivateResources != NULL) {
      FreePool (CorePrivateResources);
    }
  }

  return Status;
}
