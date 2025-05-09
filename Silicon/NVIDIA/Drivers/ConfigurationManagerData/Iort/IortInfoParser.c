/** @file
  Configuration Manager Data of IO Remapping Table

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "IortInfoParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/HobLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>

#include "ConfigurationIortPrivate.h"

#define for_each_list_entry(tmp, list)       \
        for (tmp = GetFirstNode (list);      \
             !IsNull (list, tmp);            \
             tmp = GetNextNode (list, tmp))

STATIC IORT_PRIVATE_DATA  mIortPrivate = {
  .Signature                                               = IORT_DATA_SIGNATURE,
  .IoNodes[IORT_TYPE_INDEX (EArmObjItsGroup)]              = { sizeof (CM_ARM_ITS_GROUP_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjNamedComponent)]        = { sizeof (CM_ARM_NAMED_COMPONENT_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjRootComplex)]           = { sizeof (CM_ARM_ROOT_COMPLEX_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjSmmuV1SmmuV2)]          = { sizeof (CM_ARM_SMMUV1_SMMUV2_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjSmmuV3)]                = { sizeof (CM_ARM_SMMUV3_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjPmcg)]                  = { sizeof (CM_ARM_PMCG_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjGicItsIdentifierArray)] = { sizeof (CM_ARM_ITS_IDENTIFIER), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjIdMappingArray)]        = { sizeof (CM_ARM_ID_MAPPING), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjSmmuInterruptArray)]    = { sizeof (CM_ARM_SMMU_INTERRUPT), }
};

UINT32  UniqueIdentifier;

/**
  Clean all IORT property nodes built in the list

  @param[in, out] Private   Pointer to the module private data

  @return EFI_SUCCESS       IORT nodes of Named Comp and ID mapping are populated and installed
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
VOID
CleanIortPropNodes (
  IN OUT  IORT_PRIVATE_DATA  *Private
  )
{
  IORT_PROP_NODE  *NodeEntry;
  LIST_ENTRY      *PropNode;
  IORT_NODE       *IoNode;
  UINT32          Index;

  for (Index = 0; Index < MAX_NUMBER_OF_IORT_TYPE; Index++) {
    IoNode = &mIortPrivate.IoNodes[Index];
    FREE_NON_NULL (IoNode->NodeArray);
    FREE_NON_NULL (IoNode->TokenArray);
  }

  while (!IsListEmpty (&Private->PropNodeList)) {
    PropNode = GetFirstNode (&Private->PropNodeList);
    RemoveEntryList (PropNode);
    NodeEntry = IORT_PROP_NODE_FROM_LINK (PropNode);
    if (NULL != NodeEntry) {
      FreePool (NodeEntry);
    }
  }
}

/**
  Find a prop node for a given phandle and instance (DTB)

  @param[in] Private       Pointer to the module private data
  @param[in] Phandle       Phandle of DTB node
  @param[in] NodeInstance  Node instance in linked list (1-based index)

  @return address of the IORT node if found
  @retval 0 if not found

**/
STATIC
IORT_PROP_NODE *
FindPropNodeByPhandleInstance (
  IN IORT_PRIVATE_DATA  *Private,
  IN UINT32             Phandle,
  IN UINT32             NodeInstance
  )
{
  IORT_PROP_NODE  *PropNode;
  LIST_ENTRY      *ListEntry;
  UINT32          Instance;

  ASSERT (NodeInstance != 0);
  if (Phandle == NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
    DEBUG ((DEBUG_ERROR, "%a: Trying to look up node via invalid phandle!\n", __FUNCTION__));
    return NULL;
  }

  Instance = 1;
  for_each_list_entry (ListEntry, &Private->PropNodeList) {
    PropNode = IORT_PROP_NODE_FROM_LINK (ListEntry);
    if (PropNode->Phandle == Phandle) {
      if (NodeInstance == Instance) {
        return PropNode;
      } else {
        Instance++;
        continue;
      }
    }
  }
  return NULL;
}

/**
  Compute Address Limit from DTB property 'dma-ranges'

  @param[in] Private       Pointer to the module private data
  @param[in] Phandle       Pointer to prop node

  @return number of physcal address bits

**/
STATIC
UINT32
GetAddressLimit (
  IN IORT_PRIVATE_DATA  *Private,
  IN IORT_PROP_NODE     *PropNode
  )
{
  EFI_STATUS                      Status;
  UINT64                          DmaAddr;
  UINT32                          AddrLimit;
  NVIDIA_DEVICE_TREE_RANGES_DATA  DmaRange;
  UINT32                          NumRanges;
  VOID                            *Hob;

  NumRanges = 1;
  Status    = DeviceTreeGetRanges (PropNode->NodeOffset, "dma-ranges", &DmaRange, &NumRanges);
  if (EFI_ERROR (Status)) {
    Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
    if ((Hob != NULL) &&
        (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
    {
      AddrLimit = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->PcieAddressBits;
    } else {
      ASSERT (FALSE);
      AddrLimit = 0;
    }

    return AddrLimit;
  }

  DmaAddr  = DmaRange.ParentAddress;
  DmaAddr += DmaRange.Size;

  // Compute Log2 value of 64bit DMA end address
  AddrLimit = 0;
  if (DmaAddr != 0) {
    DmaAddr--;
    while (DmaAddr != 0) {
      AddrLimit++;
      DmaAddr >>= 1;
    }
  }

  return AddrLimit;
}

/**
  Add all IORT property nodes in the device tree to the list

  @param[in, out] Private       Pointer to the module private data
  @param[in]      DevMap        Pointer to the IORT node type map

  @return EFI_SUCCESS           List built
  @retval !(EFI_SUCCESS)        Other errors

**/
STATIC
EFI_STATUS
AddIortPropNodes (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT   IORT_PRIVATE_DATA      *Private,
  IN CONST IORT_DEVICE_NODE_MAP   *DevMap
  )
{
  EFI_STATUS                             Status;
  INT32                                  NodeOffset;
  IORT_PROP_NODE                         *PropNode;
  NVIDIA_DEVICE_TREE_MSI_IOMMU_MAP_DATA  MsiProp;
  NVIDIA_DEVICE_TREE_IOMMUS_DATA         IommusProp;
  NVIDIA_DEVICE_TREE_MSI_IOMMU_MAP_DATA  IommuMapProp;
  UINT32                                 DevicesPhandle;
  BOOLEAN                                ItsNodePresent;
  UINT32                                 DualSmmuPresent;
  UINT32                                 Indx;
  UINT32                                 NumberOfRegisters;
  NVIDIA_DEVICE_TREE_REGISTER_DATA       *RegisterArray;
  CONST CHAR8                            *CompatibleInfo[2];
  UINT32                                 ArraySize;

  ItsNodePresent    = FALSE;
  CompatibleInfo[1] = NULL;

  for ( ; DevMap->Compatibility != NULL; DevMap++) {
    CompatibleInfo[0] = DevMap->Compatibility;
    NodeOffset        = -1;

    if (DevMap->ObjectId == EArmObjNamedComponent) {
      if (DevMap->ObjectName == NULL) {
        DEBUG ((DEBUG_WARN, "%a: Invalid named component (compatibility=\"%a\")\r\n", __FUNCTION__, DevMap->Compatibility));
        continue;
      }

      // Check for Alias, and use only that instead of all compatible nodes if found
      if (DevMap->Alias != NULL) {
        Status = DeviceTreeGetNodeByPath (DevMap->Alias, &NodeOffset);
        if (EFI_ERROR (Status)) {
          if (Status == EFI_NOT_FOUND) {
            DEBUG ((DEBUG_WARN, "%a: Didn't find the node for alias %a in DTB\n", __FUNCTION__, DevMap->Alias));
          } else {
            DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get the node for alias %a\n", __FUNCTION__, Status, DevMap->Alias));
          }

          continue;
        }

        // Check DTB status and skip if it's not enabled
        Status = DeviceTreeCheckNodeCompatibility (CompatibleInfo, NodeOffset);
        if (EFI_ERROR (Status)) {
          continue;
        }
      }
    }

    do {
      // Find the next compatible node (if not using Alias)
      if (DevMap->Alias == NULL) {
        Status = DeviceTreeGetNextCompatibleNode (CompatibleInfo, &NodeOffset);
        if (Status == EFI_NOT_FOUND) {
          Status = EFI_SUCCESS;
          break;
        } else if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get a %a node\n", __FUNCTION__, Status, DevMap->Compatibility));
          break;
        }
      }

      // The reg property is mandatory with requested entries
      NumberOfRegisters = 0;
      Status            = DeviceTreeGetRegisters (NodeOffset, NULL, &NumberOfRegisters);
      if (EFI_ERROR (Status) && (Status != EFI_BUFFER_TOO_SMALL)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get register count for %a node\n", __FUNCTION__, Status, DevMap->Compatibility));
        continue;
      }

      if (NumberOfRegisters == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Found zero registers for %a node\n", __FUNCTION__, DevMap->Compatibility));
        Status = EFI_DEVICE_ERROR;
        continue;
      }

      RegisterArray = (NVIDIA_DEVICE_TREE_REGISTER_DATA  *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * NumberOfRegisters);
      if (RegisterArray == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate space for %u registers for %a node\n", __FUNCTION__, NumberOfRegisters, DevMap->Compatibility));
        Status = EFI_OUT_OF_RESOURCES;
        continue;
      }

      Status = DeviceTreeGetRegisters (NodeOffset, RegisterArray, &NumberOfRegisters);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get registers for %a node\n", __FUNCTION__, Status, DevMap->Compatibility));
        continue;
      }

      DualSmmuPresent = 0;
      if ((DevMap->ObjectId == EArmObjSmmuV1SmmuV2) && (NumberOfRegisters > 1)) {
        DualSmmuPresent = 1;
      }

      for (Indx = 0; Indx <= DualSmmuPresent; Indx++) {
        MsiProp.Controller.Phandle      = NVIDIA_DEVICE_TREE_PHANDLE_INVALID;
        IommusProp.IommuPhandle         = NVIDIA_DEVICE_TREE_PHANDLE_INVALID;
        IommuMapProp.Controller.Phandle = NVIDIA_DEVICE_TREE_PHANDLE_INVALID;
        DevicesPhandle                  = NVIDIA_DEVICE_TREE_PHANDLE_INVALID;

        if (DevMap->ObjectId == EArmObjItsGroup) {
          ItsNodePresent = TRUE;
          Private->IoNodes[ITSIDENT_TYPE_INDEX].NumberOfNodes++;
          goto AllocatePropNode;
        }

        // Check "msi-map" property for all DTB nodes
        if (ItsNodePresent) {
          ArraySize = 1;
          Status    = DeviceTreeGetMsiIommuMap (NodeOffset, "msi-map", &MsiProp, &ArraySize);
          if (!EFI_ERROR (Status)) {
            Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
          }
        }

        // Check "iommu-map" property only for non-SMMUv1v2, non-SMMUv3, and non-PMCG nodes
        if ((DevMap->ObjectId != EArmObjSmmuV1SmmuV2) && (DevMap->ObjectId != EArmObjSmmuV3) && (DevMap->ObjectId != EArmObjPmcg)) {
          ArraySize = 1;
          Status    = DeviceTreeGetIommus (NodeOffset, &IommusProp, &ArraySize);
          if (!EFI_ERROR (Status) && (IommusProp.IommuPhandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID)) {
            // Check DTB status and skip if it's not enabled
            if (FindPropNodeByPhandleInstance (Private, IommusProp.IommuPhandle, 1) == NULL) {
              continue;
            }

            Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
            if (DevMap->DualSmmuPresent == 1) {
              Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
            }
          } else {
            ArraySize = 1;
            Status    = DeviceTreeGetMsiIommuMap (NodeOffset, "iommu-map", &IommuMapProp, &ArraySize);
            if (EFI_ERROR (Status) || (IommuMapProp.Controller.Phandle == NVIDIA_DEVICE_TREE_PHANDLE_INVALID)) {
              // Skip this node if both 'iommu-map' and 'msi-map' are not defined
              if (MsiProp.Controller.Phandle == NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
                continue;
              }
            } else {
              // Check DTB status and skip if it's not enabled
              if (FindPropNodeByPhandleInstance (Private, IommuMapProp.Controller.Phandle, 1) == NULL) {
                continue;
              }

              Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
              if (DevMap->DualSmmuPresent == 1) {
                Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
              }
            }
          }
        } else {
          // Check "devices" property for all PMCG nodes
          if (DevMap->ObjectId == EArmObjPmcg) {
            Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "devices", &DevicesPhandle);
            if (!EFI_ERROR (Status)) {
              // Skip if the target DTB node is not valid
              if (FindPropNodeByPhandleInstance (Private, DevicesPhandle, 1) == NULL) {
                continue;
              }
            }
          }

          Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
        }

AllocatePropNode:
        PropNode = AllocateZeroPool (sizeof (IORT_PROP_NODE));
        if (PropNode == NULL) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to allocate list entry\r\n", __FUNCTION__));
          return EFI_OUT_OF_RESOURCES;
        }

        Status = DeviceTreeGetNodePHandle (NodeOffset, &PropNode->Phandle);
        if (EFI_ERROR (Status)) {
          if (Status != EFI_NOT_FOUND) {
            DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get Phandle for %a NodeOffset 0x%x\n", __FUNCTION__, Status, DevMap->Compatibility, NodeOffset));
          }

          PropNode->Phandle = NVIDIA_DEVICE_TREE_PHANDLE_INVALID;
        }

        PropNode->RegCount = NumberOfRegisters;
        PropNode->RegArray = &RegisterArray[Indx];
        CopyMem (&PropNode->MsiProp, &MsiProp, sizeof (MsiProp));
        CopyMem (&PropNode->IommusProp, &IommusProp, sizeof (IommusProp));
        CopyMem (&PropNode->IommuMapProp, &IommuMapProp, sizeof (IommuMapProp));
        PropNode->DevicesPhandle  = DevicesPhandle;
        PropNode->DualSmmuPresent = DevMap->DualSmmuPresent;
        PropNode->NodeOffset      = NodeOffset;
        PropNode->ObjectId        = DevMap->ObjectId;
        PropNode->ObjectName      = DevMap->ObjectName;
        PropNode->Signature       = IORT_PROP_NODE_SIGNATURE;
        InsertTailList (&Private->PropNodeList, &PropNode->Link);
        Private->IoNodes[IORT_TYPE_INDEX (DevMap->ObjectId)].NumberOfNodes++;
        DEBUG ((DEBUG_INFO, "%a: Added a node of type %a, Name %a, Alias %a\n", __FUNCTION__, DevMap->Compatibility, DevMap->ObjectName, DevMap->Alias));
      }
    } while (DevMap->Alias == NULL);
  }

  return EFI_SUCCESS;
}

/**
  Allocate a space for the IORT nodes as many as present in the device tree
  and update the module private structure with the allocated space

  @param[in, out] Private       Pointer to the module private data

  @return EFI_SUCCESS           Successful allocation
  @retval !(EFI_SUCCESS)        Other errors

**/
STATIC
EFI_STATUS
AllocateIortNodes (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA       *Private
  )
{
  EFI_STATUS      Status;
  UINTN           Index;
  IORT_NODE       *IoNode;
  UINTN           Index0;
  LIST_ENTRY      *ListEntry;
  IORT_PROP_NODE  *PropNode;

  for (Index = 0; Index < MAX_NUMBER_OF_IORT_TYPE; Index++) {
    IoNode = &Private->IoNodes[Index];
    if (IoNode->SizeOfNode == 0) {
      continue;
    }

    if (IoNode->NumberOfNodes == 0) {
      DEBUG ((DEBUG_INFO, "%a: No IORT nodes of %d\r\n", __FUNCTION__, (Index + MIN_IORT_OBJID)));
      continue;
    }

    IoNode->NodeArray = AllocateZeroPool (IoNode->NumberOfNodes * IoNode->SizeOfNode);
    if (IoNode->NodeArray == NULL) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to allocate IORT node of %d\r\n",
        __FUNCTION__,
        (Index + MIN_IORT_OBJID)
        ));
      return EFI_OUT_OF_RESOURCES;
    }

    Status = NvAllocateCmTokens (ParserHandle, IoNode->NumberOfNodes, &IoNode->TokenArray);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to allocate token array for IORT node of %d (%r)\r\n",
        __FUNCTION__,
        (Index + MIN_IORT_OBJID),
        Status
        ));
      return Status;
    }

    Index0 = 0;
    for_each_list_entry (ListEntry, &Private->PropNodeList) {
      PropNode = IORT_PROP_NODE_FROM_LINK (ListEntry);
      if (PropNode->ObjectId == (Index + MIN_IORT_OBJID)) {
        ASSERT (Index0 < IoNode->NumberOfNodes);
        PropNode->IortNode = IoNode->NodeArray + (IoNode->SizeOfNode * Index0);
        PropNode->Token    = IoNode->TokenArray[Index0];
        Index0++;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Populate data of ITS Group Node and install the IORT nodes of GIC ITS
  and ITS identifier array

  @param[in, out] Private   Pointer to the module private data
  @param[in, out] PropNode  Pointer to the PropNode

  @return EFI_SUCCESS       IORT nodes of GIC ITS and ITS Identifier are
                            populated and installed
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupIortNodeForItsGroup (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA       *Private,
  IN OUT  IORT_PROP_NODE          *PropNode
  )
{
  EFI_STATUS             Status;
  CM_ARM_ITS_GROUP_NODE  *IortNode;
  CM_ARM_ITS_IDENTIFIER  *ItsIdArray;
  UINT32                 ItsId;
  CM_OBJ_DESCRIPTOR      Desc;

  IortNode = PropNode->IortNode;
  if (IortNode->Token != CM_NULL_TOKEN) {
    return EFI_SUCCESS;
  }

  ASSERT (Private->ItsIdentifierIndex < Private->IoNodes[ITSIDENT_TYPE_INDEX].NumberOfNodes);

  ItsId             = Private->ItsIdentifierIndex;
  ItsIdArray        = Private->IoNodes[ITSIDENT_TYPE_INDEX].NodeArray;
  ItsIdArray       += ItsId;
  ItsIdArray->ItsId = ItsId;

  IortNode->ItsIdCount = 1;
  IortNode->Token      = PropNode->Token;
  if (IortNode->ItsIdCount > 0) {
    Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicItsIdentifierArray);
    Desc.Size     = IortNode->ItsIdCount * sizeof (CM_ARM_ITS_IDENTIFIER);
    Desc.Count    = IortNode->ItsIdCount;
    Desc.Data     = ItsIdArray;

    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, &IortNode->ItsIdToken);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add %u ItsIds due to error code %r\n", __FUNCTION__, IortNode->ItsIdCount, Status));
      return Status;
    }
  } else {
    IortNode->ItsIdToken = CM_NULL_TOKEN;
    DEBUG ((DEBUG_ERROR, "%a: warning: Didn't find any ItsIds\n", __FUNCTION__));
  }

  IortNode->Identifier = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  Private->ItsIdentifierIndex++;

  return EFI_SUCCESS;
}

/**
  Populate IDMAP entries for SMMUv1/v2 from the device tree

  @param[in, out] Private   Pointer to the module private data
  @param[in, out] PropNode  Pointer to the PropNode

  @return EFI_SUCCESS       SmmuV1/V2 nodes are populated and installed
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupIortIdMappingForSmmuV1V2 (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA       *Private,
  IN OUT  IORT_PROP_NODE          *PropNode
  )
{
  EFI_STATUS                                   Status;
  CM_ARM_SMMUV1_SMMUV2_NODE                    *IortNode;
  CM_ARM_ID_MAPPING                            *IdMapping;
  LIST_ENTRY                                   *ListEntry;
  IORT_PROP_NODE                               *TmpPropNode;
  IORT_PROP_NODE                               *IortPropNode;
  CONST NVIDIA_DEVICE_TREE_MSI_IOMMU_MAP_DATA  *MsiProp;
  CM_OBJ_DESCRIPTOR                            Desc;

  IortNode = PropNode->IortNode;
  if (IortNode->IdMappingToken != 0) {
    return EFI_SUCCESS;
  }

  IdMapping            = Private->IoNodes[IDMAP_TYPE_INDEX].NodeArray;
  IdMapping           += Private->IdMapIndex;
  PropNode->IdMapArray = IdMapping;

  for_each_list_entry (ListEntry, &Private->PropNodeList) {
    TmpPropNode = IORT_PROP_NODE_FROM_LINK (ListEntry);
    if (TmpPropNode != PropNode) {
      if (TmpPropNode->IommusProp.IommuPhandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
        IortPropNode = FindPropNodeByPhandleInstance (Private, TmpPropNode->IommusProp.IommuPhandle, 1);
        if (!IortPropNode || (IortPropNode->IortNode != IortNode)) {
          continue;
        }
      } else if (TmpPropNode->IommuMapProp.Controller.Phandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
        IortPropNode = FindPropNodeByPhandleInstance (Private, TmpPropNode->IommuMapProp.Controller.Phandle, 1);
        if (!IortPropNode || (IortPropNode->IortNode != IortNode)) {
          continue;
        }
      } else {
        continue;
      }
    }

    MsiProp = &TmpPropNode->MsiProp;
    if (MsiProp->Controller.Phandle == NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
      continue;
    }

    ASSERT (Private->IdMapIndex < Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes);
    IdMapping->InputBase            = MsiProp->RidBase;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, MsiProp->Controller.Phandle, 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    IdMapping->OutputBase           = MsiProp->Controller.Base;
    IdMapping->NumIds               = MsiProp->Length - 1;
    IdMapping->Flags                = 0;

    IdMapping++;
    Private->IdMapIndex++;
    PropNode->IdMapCount++;
  }

  IortNode->IdMappingCount = PropNode->IdMapCount;
  if (PropNode->IdMapCount > 0) {
    Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjIdMappingArray);
    Desc.Size     = PropNode->IdMapCount * sizeof (CM_ARM_ID_MAPPING);
    Desc.Count    = PropNode->IdMapCount;
    Desc.Data     = PropNode->IdMapArray;

    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, &IortNode->IdMappingToken);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add %u IdMaps due to error code %r\n", __FUNCTION__, PropNode->IdMapCount, Status));
      return Status;
    }
  } else {
    IortNode->IdMappingToken = CM_NULL_TOKEN;
    DEBUG ((DEBUG_ERROR, "%a: warning: Didn't find any IdMaps\n", __FUNCTION__));
  }

  return EFI_SUCCESS;
}

/**
  Populate IDMAP entries for SMMUv3 from the device tree

  @param[in, out] Private   Pointer to the module private data
  @param[in, out] PropNode  Pointer to the PropNode

  @return EFI_SUCCESS       SmmuV3 nodes are populated and installed
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupIortIdMappingForSmmuV3 (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA       *Private,
  IN OUT  IORT_PROP_NODE          *PropNode
  )
{
  EFI_STATUS                                   Status;
  CM_ARM_SMMUV3_NODE                           *IortNode;
  CM_ARM_ID_MAPPING                            *IdMapping;
  LIST_ENTRY                                   *ListEntry;
  IORT_PROP_NODE                               *TmpPropNode;
  IORT_PROP_NODE                               *IortPropNode;
  CONST NVIDIA_DEVICE_TREE_MSI_IOMMU_MAP_DATA  *MsiProp;
  CM_OBJ_DESCRIPTOR                            Desc;

  IortNode = PropNode->IortNode;
  if (IortNode->IdMappingToken != 0) {
    return EFI_SUCCESS;
  }

  IdMapping            = Private->IoNodes[IDMAP_TYPE_INDEX].NodeArray;
  IdMapping           += Private->IdMapIndex;
  PropNode->IdMapArray = IdMapping;

  for_each_list_entry (ListEntry, &Private->PropNodeList) {
    TmpPropNode = IORT_PROP_NODE_FROM_LINK (ListEntry);
    if (TmpPropNode != PropNode) {
      if (TmpPropNode->IommusProp.IommuPhandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
        IortPropNode = FindPropNodeByPhandleInstance (Private, TmpPropNode->IommusProp.IommuPhandle, 1);
        if (!IortPropNode || (IortPropNode->IortNode != IortNode)) {
          continue;
        }
      } else if (TmpPropNode->IommuMapProp.Controller.Phandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
        IortPropNode = FindPropNodeByPhandleInstance (Private, TmpPropNode->IommuMapProp.Controller.Phandle, 1);
        if (!IortPropNode || (IortPropNode->IortNode != IortNode)) {
          continue;
        }
      } else {
        continue;
      }
    }

    MsiProp = &TmpPropNode->MsiProp;
    if (MsiProp->Controller.Phandle == NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
      continue;
    }

    ASSERT (Private->IdMapIndex < Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes);
    IdMapping->InputBase            = MsiProp->RidBase;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, MsiProp->Controller.Phandle, 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    IdMapping->OutputBase           = MsiProp->Controller.Base;
    IdMapping->NumIds               = MsiProp->Length - 1;
    IdMapping->Flags                = 0;

    if (MsiProp == &PropNode->MsiProp) {
      IortNode->DeviceIdMappingIndex = PropNode->IdMapCount;
      IdMapping->Flags               = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
    }

    IdMapping++;
    Private->IdMapIndex++;
    PropNode->IdMapCount++;
  }

  // Validation check for DeviceIdMappingIndex
  if (((IortNode->PriInterrupt == 0) || (IortNode->GerrInterrupt == 0) || \
       (IortNode->SyncInterrupt == 0) || (IortNode->EventInterrupt == 0)) && \
      (PropNode->MsiProp.Controller.Phandle == NVIDIA_DEVICE_TREE_PHANDLE_INVALID) && (PropNode->IdMapCount != 0))
  {
    // As per the IORT specification, DeviceIdMappingIndex must contain a valid
    // index if any one of wired interrupt is zero and msi-map is not defined
    // Retaining this for IORT spec backward compatibility
    IortNode->DeviceIdMappingIndex = PropNode->IdMapCount;
  }

  IortNode->IdMappingCount = PropNode->IdMapCount;
  if (PropNode->IdMapCount > 0) {
    Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjIdMappingArray);
    Desc.Size     = PropNode->IdMapCount * sizeof (CM_ARM_ID_MAPPING);
    Desc.Count    = PropNode->IdMapCount;
    Desc.Data     = PropNode->IdMapArray;

    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, &IortNode->IdMappingToken);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add %u IdMaps due to error code %r\n", __FUNCTION__, PropNode->IdMapCount, Status));
      return Status;
    }
  } else {
    IortNode->IdMappingToken = CM_NULL_TOKEN;
    DEBUG ((DEBUG_ERROR, "%a: warning: Didn't find any IdMaps\n", __FUNCTION__));
  }

  return EFI_SUCCESS;
}

/**
  Populate Global and context interrupts for SMMUv1/v2 from the device tree

  @param[in, out] Private   Pointer to the module private data
  @param[in, out] PropNode  Pointer to the PropNode

  @return EFI_SUCCESS       Global and context interrupts for SmmuV1/V2 nodes are populated
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupGlobalContextIrqForSmmuV1V2 (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA       *Private,
  IN OUT  IORT_PROP_NODE          *PropNode
  )
{
  EFI_STATUS                         Status;
  CM_ARM_SMMUV1_SMMUV2_NODE          *IortNode;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptData;
  UINT32                             InterruptSize;
  UINT32                             GlobalInterruptCnt;
  CM_ARM_GENERIC_INTERRUPT           *ContextInterruptArray;
  UINT32                             IrqCnt;
  UINT32                             InterruptIndx;
  CM_OBJ_DESCRIPTOR                  Desc;

  IortNode      = PropNode->IortNode;
  InterruptSize = 0;
  InterruptData = NULL;

  Status = DeviceTreeGetNodePropertyValue32 (PropNode->NodeOffset, "#global-interrupts", &GlobalInterruptCnt);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find \"#global-interrupts\"\r\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto ErrorExit;
  }

  if (GlobalInterruptCnt > 2) {
    DEBUG ((DEBUG_ERROR, "Global interrupts %u more than 2. No space to store more than 2 global interrupts\n", GlobalInterruptCnt));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Status = GetDeviceTreeInterrupts (PropNode->NodeOffset, InterruptData, &InterruptSize);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    FREE_NON_NULL (InterruptData);

    InterruptData = (NVIDIA_DEVICE_TREE_INTERRUPT_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_INTERRUPT_DATA) * InterruptSize);
    if (InterruptData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Status = GetDeviceTreeInterrupts (PropNode->NodeOffset, InterruptData, &InterruptSize);
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }
  } else if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  IrqCnt                          = InterruptSize;
  IortNode->ContextInterruptCount = IrqCnt - GlobalInterruptCnt;

  if (GlobalInterruptCnt >= 1) {
    IortNode->SMMU_NSgIrpt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData[0]);

    IortNode->SMMU_NSgIrptFlags = (InterruptData[0].Flag == INTERRUPT_HI_LEVEL ?
                                   EFI_ACPI_IRQ_LEVEL_TRIGGERED : EFI_ACPI_IRQ_EDGE_TRIGGERED);
  }

  if (GlobalInterruptCnt == 2) {
    IortNode->SMMU_NSgCfgIrpt      = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData[1]);
    IortNode->SMMU_NSgCfgIrptFlags = (InterruptData[1].Flag == INTERRUPT_HI_LEVEL ?
                                      EFI_ACPI_IRQ_LEVEL_TRIGGERED : EFI_ACPI_IRQ_EDGE_TRIGGERED);
  }

  // Each interrupt is described by two 4 byte fields: Bytes 0:3 GSIV of interrupt, Bytes 4:7 Interrupt flags
  ContextInterruptArray = AllocateZeroPool (IortNode->ContextInterruptCount * sizeof (CM_ARM_GENERIC_INTERRUPT));
  if (ContextInterruptArray == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  for (InterruptIndx = 0; InterruptIndx < IortNode->ContextInterruptCount; InterruptIndx++) {
    ContextInterruptArray[InterruptIndx].Interrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData[InterruptIndx + GlobalInterruptCnt]);
    ContextInterruptArray[InterruptIndx].Flags     = (InterruptData[InterruptIndx + GlobalInterruptCnt].Flag == INTERRUPT_HI_LEVEL ?
                                                      EFI_ACPI_IRQ_LEVEL_TRIGGERED : EFI_ACPI_IRQ_EDGE_TRIGGERED);
  }

  PropNode->ContextInterruptCnt   = IortNode->ContextInterruptCount;
  PropNode->ContextInterruptArray = (VOID *)ContextInterruptArray;

  if (PropNode->ContextInterruptCnt != 0) {
    Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSmmuInterruptArray);
    Desc.Size     = PropNode->ContextInterruptCnt * sizeof (CM_ARM_SMMU_INTERRUPT);
    Desc.Count    = PropNode->ContextInterruptCnt;
    Desc.Data     = PropNode->ContextInterruptArray;

    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, &IortNode->ContextInterruptToken);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add %u ContextInterrupts due to error code %r\n", __FUNCTION__, PropNode->ContextInterruptCnt, Status));
      goto ErrorExit;
    }
  } else {
    IortNode->ContextInterruptToken = CM_NULL_TOKEN;
  }

ErrorExit:
  FREE_NON_NULL (InterruptData);

  return Status;
}

/**
  Populate Pmu interrupts for SMMUv1/v2 from the device tree

  @param[in, out] Private   Pointer to the module private data
  @param[in, out] PropNode  Pointer to the PropNode

  @return EFI_SUCCESS       Pmu interrupts for SmmuV1/V2 nodes are populated
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupPmuIrqForSmmuV1V2 (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA       *Private,
  IN OUT  IORT_PROP_NODE          *PropNode
  )
{
  EFI_STATUS                         Status;
  CM_ARM_SMMUV1_SMMUV2_NODE          *IortNode;
  UINT32                             PmuHandle;
  UINT32                             NumPmuHandles;
  CM_ARM_GENERIC_INTERRUPT           *PmuInterruptArray;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *PmuInterruptData;
  UINT32                             PmuInterruptSize;
  UINT32                             PmuInterruptIndx;
  CM_OBJ_DESCRIPTOR                  Desc;

  IortNode         = PropNode->IortNode;
  PmuInterruptSize = 0;
  PmuInterruptData = NULL;

  NumPmuHandles = 1;
  Status        = GetMatchingEnabledDeviceTreeNodes ("arm,cortex-a78-pmu", &PmuHandle, &NumPmuHandles);
  if (EFI_ERROR (Status)) {
    NumPmuHandles = 1;
    Status        = GetMatchingEnabledDeviceTreeNodes ("arm,armv8-pmuv3", &PmuHandle, &NumPmuHandles);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to find Pmu Irq err=%r\r\n", Status));
      NumPmuHandles = 0;
      goto ErrorExit;
    }
  }

  Status = GetDeviceTreeInterrupts (PmuHandle, PmuInterruptData, &PmuInterruptSize);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    if (PmuInterruptData != NULL) {
      FreePool (PmuInterruptData);
      PmuInterruptData = NULL;
    }

    PmuInterruptData = (NVIDIA_DEVICE_TREE_INTERRUPT_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_INTERRUPT_DATA) * PmuInterruptSize);
    if (PmuInterruptData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Status = GetDeviceTreeInterrupts (PmuHandle, PmuInterruptData, &PmuInterruptSize);
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }
  } else if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  IortNode->PmuInterruptCount = PmuInterruptSize;
  // Each interrupt is described by two 4 byte fields: Bytes 0:3 GSIV of interrupt, Bytes 4:7 Interrupt flags
  PmuInterruptArray = AllocateZeroPool (IortNode->PmuInterruptCount * sizeof (CM_ARM_GENERIC_INTERRUPT));
  if (PmuInterruptArray == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  for (PmuInterruptIndx = 0; PmuInterruptIndx < IortNode->PmuInterruptCount; PmuInterruptIndx++) {
    PmuInterruptArray[PmuInterruptIndx].Interrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (PmuInterruptData[PmuInterruptIndx]);
    PmuInterruptArray[PmuInterruptIndx].Flags     = (PmuInterruptData[PmuInterruptIndx].Flag == INTERRUPT_HI_LEVEL ?
                                                     EFI_ACPI_IRQ_LEVEL_TRIGGERED : EFI_ACPI_IRQ_EDGE_TRIGGERED);
  }

  PropNode->PmuInterruptCnt   = IortNode->PmuInterruptCount;
  PropNode->PmuInterruptArray = (VOID *)PmuInterruptArray;
  if (PropNode->PmuInterruptCnt != 0) {
    Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSmmuInterruptArray);
    Desc.Size     = PropNode->PmuInterruptCnt * sizeof (CM_ARM_SMMU_INTERRUPT);
    Desc.Count    = PropNode->PmuInterruptCnt;
    Desc.Data     = PropNode->PmuInterruptArray;

    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, &IortNode->PmuInterruptToken);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add %u PmuInterrupts due to error code %r\n", __FUNCTION__, PropNode->PmuInterruptCnt, Status));
      goto ErrorExit;
    }
  } else {
    IortNode->PmuInterruptToken = CM_NULL_TOKEN;
  }

ErrorExit:
  FREE_NON_NULL (PmuInterruptData);

  return Status;
}

/**
  Populate data of SMMUv1/v2 from the device tree and install the IORT nodes of SmmuV1/V2

  @param[in, out] Private   Pointer to the module private data
  @param[in, out] PropNode  Pointer to the PropNode

  @return EFI_SUCCESS       SmmuV1/V2 nodes are populated and installed
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupIortNodeForSmmuV1V2 (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA       *Private,
  IN OUT  IORT_PROP_NODE          *PropNode
  )
{
  EFI_STATUS                 Status;
  CM_ARM_SMMUV1_SMMUV2_NODE  *IortNode;

  Status   = EFI_SUCCESS;
  IortNode = PropNode->IortNode;
  if (IortNode->Token != CM_NULL_TOKEN) {
    goto ErrorExit;
  }

  IortNode->Token      = PropNode->Token;
  IortNode->Identifier = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  IortNode->BaseAddress = 0;

  NV_ASSERT_RETURN (PropNode->RegArray != NULL, return EFI_DEVICE_ERROR, "%a: Trying to parse a node that has a NULL RegArray\n", __FUNCTION__);

  IortNode->BaseAddress = PropNode->RegArray[0].BaseAddress;
  IortNode->Span        = PropNode->RegArray[0].Size;
  IortNode->Model       = EFI_ACPI_IORT_SMMUv1v2_MODEL_MMU500;
  IortNode->Flags       = EFI_ACPI_IORT_SMMUv1v2_FLAG_COH_WALK;

  Status = SetupGlobalContextIrqForSmmuV1V2 (ParserHandle, Private, PropNode);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = SetupPmuIrqForSmmuV1V2 (ParserHandle, Private, PropNode);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  return SetupIortIdMappingForSmmuV1V2 (ParserHandle, Private, PropNode);

ErrorExit:
  return Status;
}

/**
  patch SMMUv3 _UID info in TH500 SMMU CMDQ-V nodes

  @param[in] IortNode       Pointer to the CM_ARM_SMMUV3_NODE

  @retval EFI_SUCCESS       Success
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
UpdateTH500SmmuV3CmdqvUidInfo (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CM_ARM_SMMUV3_NODE           *IortNode
  )
{
  EFI_STATUS                 Status;
  NVIDIA_AML_NODE_INFO       AcpiNodeInfo;
  UINT32                     Identifier;
  UINT32                     AcpiSmmuUidPatchNameSize;
  NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol;
  STATIC UINT32              Index = 0;

  STATIC CHAR8 *CONST  AcpiSmmuUidPatchName[] = {
    "_SB_.SQ00._UID",
    "_SB_.SQ01._UID",
    "_SB_.SQ02._UID",
    "_SB_.GQ00._UID",
    "_SB_.GQ01._UID",
    "_SB_.SQ10._UID",
    "_SB_.SQ11._UID",
    "_SB_.SQ12._UID",
    "_SB_.GQ10._UID",
    "_SB_.GQ11._UID",
    "_SB_.SQ20._UID",
    "_SB_.SQ21._UID",
    "_SB_.SQ22._UID",
    "_SB_.GQ20._UID",
    "_SB_.GQ21._UID",
    "_SB_.SQ30._UID",
    "_SB_.SQ31._UID",
    "_SB_.SQ32._UID",
    "_SB_.GQ30._UID",
    "_SB_.GQ31._UID",
  };

  Status = EFI_SUCCESS;

  AcpiSmmuUidPatchNameSize = ARRAY_SIZE (AcpiSmmuUidPatchName);
  if (Index >= AcpiSmmuUidPatchNameSize) {
    DEBUG ((DEBUG_ERROR, "%a: Index %u is larger than AcpiSmmuUidPatchNameSize %u\n", __FUNCTION__, Index, AcpiSmmuUidPatchNameSize));
    goto ErrorExit;
  }

  Identifier = 0;

  Status = NvGetCmPatchProtocol (ParserHandle, &PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PatchProtocol->FindNode (PatchProtocol, AcpiSmmuUidPatchName[Index], &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, AcpiSmmuUidPatchName[Index]));
    goto ErrorExit;
  }

  if (AcpiNodeInfo.Size != sizeof (Identifier)) {
    DEBUG ((DEBUG_ERROR, "%a: Unexpected size of node %a - %d\n", __FUNCTION__, AcpiSmmuUidPatchName[Index], AcpiNodeInfo.Size));
    goto ErrorExit;
  }

  Identifier = IortNode->Identifier;
  Status     = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &Identifier, sizeof (Identifier));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, AcpiSmmuUidPatchName[Index]));
    goto ErrorExit;
  }

ErrorExit:
  Index++;
  NV_ASSERT_RETURN (!EFI_ERROR (Status), return Status, "SMMUv3 UID Patching Failure.\n");
  return Status;
}

/**
  Populate data of SMMUv3 from the device tree and install the IORT nodes of SmmuV3

  @param[in, out] Private   Pointer to the module private data
  @param[in, out] PropNode  Pointer to the PropNode

  @return EFI_SUCCESS       SmmuV3 nodes are populated and installed
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupIortNodeForSmmuV3 (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA        *Private,
  IN OUT  IORT_PROP_NODE           *PropNode
  )
{
  EFI_STATUS                         Status;
  CM_ARM_SMMUV3_NODE                 *IortNode;
  UINT32                             InterruptId;
  UINT32                             NumberOfInterrupts;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  InterruptData[MAX_NUM_IRQS_OF_SMMU_V3];
  UINT32                             IntIndex;
  UINT32                             ChipID;
  TEGRA_PLATFORM_TYPE                PlatformType;

  IortNode = PropNode->IortNode;
  if (IortNode->Token != CM_NULL_TOKEN) {
    return EFI_SUCCESS;
  }

  IortNode->Token        = PropNode->Token;
  IortNode->VatosAddress = 0;
  NV_ASSERT_RETURN (PropNode->RegArray != NULL, return EFI_DEVICE_ERROR, "%a: Trying to parse a node that has a NULL RegArray\n", __FUNCTION__);
  IortNode->BaseAddress     = PropNode->RegArray[0].BaseAddress;
  IortNode->ProximityDomain = 0;
  IortNode->Model           = EFI_ACPI_IORT_SMMUv3_MODEL_GENERIC;
  IortNode->Flags           = EFI_ACPI_IORT_SMMUv3_FLAG_PROXIMITY_DOMAIN;
  IortNode->Identifier      = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  ChipID = TegraGetChipID ();
  switch (ChipID) {
    case TH500_CHIP_ID:
      UpdateTH500SmmuV3CmdqvUidInfo (ParserHandle, IortNode);
      break;
    default:
      break;
  }

  Status = DeviceTreeGetNodeProperty (PropNode->NodeOffset, "dma-coherent", NULL, NULL);
  if (!EFI_ERROR (Status)) {
    IortNode->Flags |= EFI_ACPI_IORT_SMMUv3_FLAG_COHAC_OVERRIDE;
  }

  PlatformType = TegraGetPlatform ();
  if (PlatformType == TEGRA_PLATFORM_VDK) {
    ChipID = TegraGetChipID ();
    if (ChipID == T264_CHIP_ID) {
      IortNode->Flags |= EFI_ACPI_IORT_SMMUv3_FLAG_COHAC_OVERRIDE;
    }
  }

  Status = DeviceTreeGetNodePropertyValue32 (PropNode->NodeOffset, "numa-node-id", &IortNode->ProximityDomain);
  if (EFI_ERROR (Status)) {
    IortNode->ProximityDomain = 0;
  }

  // Parse the interrupt information
  NumberOfInterrupts = MAX_NUM_IRQS_OF_SMMU_V3;
  Status             = DeviceTreeGetInterrupts (PropNode->NodeOffset, InterruptData, &NumberOfInterrupts);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get up to %d interrupts (DTB says there are %u)\n", __FUNCTION__, Status, MAX_NUM_IRQS_OF_SMMU_V3, NumberOfInterrupts));
    return Status;
  }

  for (IntIndex = 0; IntIndex < NumberOfInterrupts; IntIndex++) {
    if (InterruptData[IntIndex].Name == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Found interrupt data without name data for interrupt index %u\n", __FUNCTION__, IntIndex));
      return EFI_NOT_FOUND;
    }
  }

  if (0 == AsciiStrCmp (InterruptData[0].Name, "combined")) {
    InterruptId              = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData[0]);
    IortNode->EventInterrupt = InterruptId;
    IortNode->PriInterrupt   = InterruptId;
    IortNode->GerrInterrupt  = InterruptId;
    IortNode->SyncInterrupt  = InterruptId;
  } else if ((NumberOfInterrupts <= MAX_NUM_IRQS_OF_SMMU_V3) && (NumberOfInterrupts >= MIN_NUM_IRQS_OF_SMMU_V3)) {
    for (IntIndex = 0; IntIndex < NumberOfInterrupts; IntIndex++) {
      UINT32  *Interrupt;

      if (0 == AsciiStrCmp (InterruptData[IntIndex].Name, "eventq")) {
        Interrupt = &IortNode->EventInterrupt;
      } else if (0 == AsciiStrCmp (InterruptData[IntIndex].Name, "priq")) {
        Interrupt = &IortNode->PriInterrupt;
      } else if (0 == AsciiStrCmp (InterruptData[IntIndex].Name, "gerror")) {
        Interrupt = &IortNode->GerrInterrupt;
      } else if (0 == AsciiStrCmp (InterruptData[IntIndex].Name, "cmdq-sync")) {
        Interrupt = &IortNode->SyncInterrupt;
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Found unknown interrupt name \"%a\"\n", __FUNCTION__, InterruptData[IntIndex].Name));
        return EFI_DEVICE_ERROR;
      }

      *Interrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData[IntIndex]);
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a: NumInterrupts was %u, but must be between %d and %d\r\n", __FUNCTION__, NumberOfInterrupts, MIN_NUM_IRQS_OF_SMMU_V3, MAX_NUM_IRQS_OF_SMMU_V3));
    return EFI_NOT_FOUND;
  }

  return SetupIortIdMappingForSmmuV3 (ParserHandle, Private, PropNode);
}

/**
  Populate data of PCI RC and ID mapping nodes defining SMMU and MSI setup.
  Mapping PCI nodes to SMMUv1v2/SMMUv3 from the device tree.
  Mapping SMMUV1V2 to Gic Msi frame nodes from the device tree.
  Mapping SMMUV3 to ITS group nodes from the device tree.
  Install the IORT nodes of PCI RC and ID mapping

  @param[in, out] Private   Pointer to the module private data
  @param[in, out] PropNode  Pointer to the PropNode

  @return EFI_SUCCESS       IORT nodes of PCI RC and ID mapping are populated and installed
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupIortNodeForPciRc (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA       *Private,
  IN OUT  IORT_PROP_NODE          *PropNode
  )
{
  EFI_STATUS                             Status;
  CM_ARM_ROOT_COMPLEX_NODE               *IortNode;
  CM_ARM_ID_MAPPING                      *IdMapping;
  NVIDIA_DEVICE_TREE_IOMMUS_DATA         *Prop;
  NVIDIA_DEVICE_TREE_MSI_IOMMU_MAP_DATA  *Map;
  UINT32                                 IdMapFlags;
  IORT_PROP_NODE                         *IortPropNode;
  CM_OBJ_DESCRIPTOR                      Desc;
  UINT32                                 IommuMapMask;

  IortNode = PropNode->IortNode;
  if (IortNode->Token != CM_NULL_TOKEN) {
    return EFI_SUCCESS;
  }

  IortNode->Token             = PropNode->Token;
  IortNode->AllocationHints   = 0;
  IortNode->MemoryAccessFlags = 0;
  IortNode->MemoryAddressSize = GetAddressLimit (Private, PropNode);
  IortNode->CacheCoherent     = 0;

  if (PropNode->DualSmmuPresent == 1) {
    IortNode->IdMappingCount = 2;
  } else {
    IortNode->IdMappingCount = 1;
  }

  IortNode->PciSegmentNumber = 0;
  IortNode->Identifier       = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  Status = DeviceTreeGetNodeProperty (PropNode->NodeOffset, "dma-coherent", NULL, NULL);
  if (!EFI_ERROR (Status)) {
    IortNode->CacheCoherent     |= EFI_ACPI_IORT_MEM_ACCESS_PROP_CCA;
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_CPM;
  }

  Status = DeviceTreeGetNodeProperty (PropNode->NodeOffset, "nvidia,canwbs-supported", NULL, NULL);
  if (!EFI_ERROR (Status)) {
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_CANWBS;
  }

  Status = DeviceTreeGetNodeProperty (PropNode->NodeOffset, "nvidia,dacs-supported", NULL, NULL);
  if (!EFI_ERROR (Status)) {
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_DACS;
  }

  Status = DeviceTreeGetNodeProperty (PropNode->NodeOffset, "ats-supported", NULL, NULL);
  if (!EFI_ERROR (Status)) {
    IortNode->AtsAttribute = EFI_ACPI_IORT_ROOT_COMPLEX_ATS_SUPPORTED;
  } else {
    IortNode->AtsAttribute = EFI_ACPI_IORT_ROOT_COMPLEX_ATS_UNSUPPORTED;
  }

  Status = DeviceTreeGetNodePropertyValue32 (PropNode->NodeOffset, "linux,pci-domain", &IortNode->PciSegmentNumber);
  if (EFI_ERROR (Status)) {
    IortNode->PciSegmentNumber = 0;
  }

  Status = DeviceTreeGetNodePropertyValue32 (PropNode->NodeOffset, "iommu-map-mask", &IommuMapMask);
  if (EFI_ERROR (Status) || (IommuMapMask != 0)) {
    IdMapFlags = 0;
  } else {
    IdMapFlags = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
  }

  ASSERT (Private->IdMapIndex < Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes);
  PropNode->IdMapCount = IortNode->IdMappingCount;
  IdMapping            = Private->IoNodes[IDMAP_TYPE_INDEX].NodeArray;
  IdMapping           += Private->IdMapIndex;
  Private->IdMapIndex += PropNode->IdMapCount;
  PropNode->IdMapArray = IdMapping;

  if (PropNode->IommusProp.IommuPhandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
    Prop = &PropNode->IommusProp;

    // Create Id Mapping Node for iommus and bind it to the PCI IORT node
    IdMapping->InputBase            = 0;
    IdMapping->OutputBase           = Prop->MasterDeviceId;
    IdMapping->NumIds               = 0;
    IdMapping->Flags                = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, Prop->IommuPhandle, 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    ASSERT (IdMapping->OutputReferenceToken != CM_NULL_TOKEN);

    if (PropNode->DualSmmuPresent == 1) {
      IdMapping++;
      IdMapping->InputBase            = 0x1;
      IdMapping->OutputBase           = Prop->MasterDeviceId;
      IdMapping->NumIds               = 0;
      IdMapping->Flags                = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
      IortPropNode                    = FindPropNodeByPhandleInstance (Private, Prop->IommuPhandle, 2);
      IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
      ASSERT (IdMapping->OutputReferenceToken != CM_NULL_TOKEN);
    }
  } else {
    Map = (PropNode->IommuMapProp.Controller.Phandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID) ? &PropNode->IommuMapProp : &PropNode->MsiProp;
    ASSERT (Map->Controller.Phandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID);

    // Create Id Mapping Node for iommu-map and bind it to the PCI IORT node
    IdMapping->InputBase  = Map->RidBase;
    IdMapping->OutputBase = Map->Controller.Base;

    if (IdMapFlags == EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE) {
      IdMapping->NumIds = 0;
    } else {
      IdMapping->NumIds = Map->Length - 1;
    }

    IdMapping->Flags                = IdMapFlags;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, Map->Controller.Phandle, 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    ASSERT (IdMapping->OutputReferenceToken != CM_NULL_TOKEN);

    if (PropNode->DualSmmuPresent == 1) {
      IdMapping++;
      IdMapping->InputBase  = Map->RidBase;
      IdMapping->OutputBase = Map->Controller.Base;

      if (IdMapFlags == EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE) {
        IdMapping->InputBase = Map->RidBase + 1;
        IdMapping->NumIds    = 0;
      } else {
        IdMapping->InputBase = Map->RidBase;
        IdMapping->NumIds    = Map->Length - 1;
      }

      IdMapping->Flags                = IdMapFlags;
      IortPropNode                    = FindPropNodeByPhandleInstance (Private, Map->Controller.Phandle, 2);
      IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
      ASSERT (IdMapping->OutputReferenceToken != CM_NULL_TOKEN);
    }
  }

  IortNode->IdMappingCount = PropNode->IdMapCount;
  if (PropNode->IdMapCount > 0) {
    Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjIdMappingArray);
    Desc.Size     = PropNode->IdMapCount * sizeof (CM_ARM_ID_MAPPING);
    Desc.Count    = PropNode->IdMapCount;
    Desc.Data     = PropNode->IdMapArray;

    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, &IortNode->IdMappingToken);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add %u IdMaps due to error code %r\n", __FUNCTION__, PropNode->IdMapCount, Status));
      return Status;
    }
  } else {
    IortNode->IdMappingToken = CM_NULL_TOKEN;
    DEBUG ((DEBUG_ERROR, "%a: warning: Didn't find any IdMaps\n", __FUNCTION__));
  }

  return EFI_SUCCESS;
}

/**
  Populate data of Named Component and ID mapping nodes defining SMMU and MSI setup.
  Mapping Named Component nodes to SMMUv1v2/SMMUv3 from the device tree.
  Mapping SMMUV3 to ITS group nodes from the device tree.
  Install the IORT nodes of Named Component and ID mapping

  @param[in, out] Private   Pointer to the module private data

  @return EFI_SUCCESS       IORT nodes of Named Comp and ID mapping are populated and installed
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupIortNodeForNComp (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA       *Private,
  IN OUT  IORT_PROP_NODE          *PropNode
  )
{
  EFI_STATUS                             Status;
  CM_ARM_NAMED_COMPONENT_NODE            *IortNode;
  CM_ARM_ID_MAPPING                      *IdMapping;
  NVIDIA_DEVICE_TREE_IOMMUS_DATA         *Prop;
  NVIDIA_DEVICE_TREE_MSI_IOMMU_MAP_DATA  *Map;
  IORT_PROP_NODE                         *IortPropNode;
  CM_OBJ_DESCRIPTOR                      Desc;

  IortNode = PropNode->IortNode;
  if (IortNode->Token != CM_NULL_TOKEN) {
    return EFI_SUCCESS;
  }

  IortNode->Token             = PropNode->Token;
  IortNode->AllocationHints   = 0;
  IortNode->MemoryAccessFlags = 0;
  IortNode->CacheCoherent     = 0;
  IortNode->Flags             = 0;
  IortNode->AddressSizeLimit  = GetAddressLimit (Private, PropNode);
  IortNode->ObjectName        = PropNode->ObjectName;

  if (PropNode->DualSmmuPresent == 1) {
    PropNode->IdMapCount = 2;
  } else {
    PropNode->IdMapCount = 1;
  }

  IortNode->Identifier = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  Status = DeviceTreeGetNodeProperty (PropNode->NodeOffset, "dma-coherent", NULL, NULL);
  if (!EFI_ERROR (Status)) {
    IortNode->CacheCoherent     |= EFI_ACPI_IORT_MEM_ACCESS_PROP_CCA;
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_CPM;
  }

  Status = DeviceTreeGetNodeProperty (PropNode->NodeOffset, "nvidia,dacs-supported", NULL, NULL);
  if (!EFI_ERROR (Status)) {
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_DACS;
  }

  ASSERT (Private->IdMapIndex < Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes);
  IdMapping            = Private->IoNodes[IDMAP_TYPE_INDEX].NodeArray;
  IdMapping           += Private->IdMapIndex;
  Private->IdMapIndex += PropNode->IdMapCount;
  PropNode->IdMapArray = IdMapping;

  if (PropNode->IommusProp.IommuPhandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
    Prop = &PropNode->IommusProp;

    // Create Id Mapping Node for iommus and bind it to the Named component IORT node
    IdMapping->InputBase            = 0x0;
    IdMapping->OutputBase           = Prop->MasterDeviceId;
    IdMapping->NumIds               = 0;
    IdMapping->Flags                = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, Prop->IommuPhandle, 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    ASSERT (IdMapping->OutputReferenceToken != CM_NULL_TOKEN);

    if (PropNode->DualSmmuPresent == 1) {
      IdMapping++;
      IdMapping->InputBase            = 0x1;
      IdMapping->OutputBase           = Prop->MasterDeviceId;
      IdMapping->NumIds               = 0;
      IdMapping->Flags                = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
      IortPropNode                    = FindPropNodeByPhandleInstance (Private, Prop->IommuPhandle, 2);
      IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
      ASSERT (IdMapping->OutputReferenceToken != CM_NULL_TOKEN);
    }
  } else {
    Map = (PropNode->IommuMapProp.Controller.Phandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID) ? &PropNode->IommuMapProp : &PropNode->MsiProp;
    ASSERT (Map->Controller.Phandle != NVIDIA_DEVICE_TREE_PHANDLE_INVALID);

    // Create Id Mapping Node for iommu-map and bind it to the Named Component node
    IdMapping->InputBase            = Map->RidBase;
    IdMapping->OutputBase           = Map->Controller.Base;
    IdMapping->NumIds               = Map->Length - 1;
    IdMapping->Flags                = 0;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, Map->Controller.Phandle, 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    ASSERT (IdMapping->OutputReferenceToken != CM_NULL_TOKEN);

    if (PropNode->DualSmmuPresent == 1) {
      IdMapping++;
      IdMapping->InputBase            = Map->RidBase;
      IdMapping->OutputBase           = Map->Controller.Base;
      IdMapping->NumIds               = Map->Length - 1;
      IdMapping->Flags                = 0;
      IortPropNode                    = FindPropNodeByPhandleInstance (Private, Map->Controller.Phandle, 2);
      IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
      ASSERT (IdMapping->OutputReferenceToken != CM_NULL_TOKEN);
    }
  }

  IortNode->IdMappingCount = PropNode->IdMapCount;
  if (PropNode->IdMapCount > 0) {
    Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjIdMappingArray);
    Desc.Size     = PropNode->IdMapCount * sizeof (CM_ARM_ID_MAPPING);
    Desc.Count    = PropNode->IdMapCount;
    Desc.Data     = PropNode->IdMapArray;

    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, &IortNode->IdMappingToken);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add %u IdMaps due to error code %r\n", __FUNCTION__, PropNode->IdMapCount, Status));
      return Status;
    }
  } else {
    IortNode->IdMappingToken = CM_NULL_TOKEN;
    DEBUG ((DEBUG_ERROR, "%a: warning: Didn't find any IdMaps\n", __FUNCTION__));
  }

  return EFI_SUCCESS;
}

/**
  Populate data of PMCG from the device tree and install the IORT nodes of PMCG

  @param[in, out] Private   Pointer to the module private data
  @param[in, out] PropNode  Pointer to the PropNode

  @return EFI_SUCCESS       PMCG nodes are populated and installed
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupIortNodeForPmcg (
  IN CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT  IORT_PRIVATE_DATA       *Private,
  IN OUT  IORT_PROP_NODE          *PropNode
  )
{
  EFI_STATUS                          Status;
  CM_ARM_PMCG_NODE                    *IortNode;
  CM_ARM_ID_MAPPING                   *IdMapping;
  TEGRA_PLATFORM_TYPE                 PlatformType;
  IORT_PROP_NODE                      *IortPropNode;
  CM_OBJ_DESCRIPTOR                   Desc;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA   InterruptData;
  UINT32                              NumberOfInterrupts;
  NVIDIA_DEVICE_TREE_CONTROLLER_DATA  MsiParent;
  UINT32                              ArraySize;

  PlatformType = TegraGetPlatform ();
  if (PlatformType != TEGRA_PLATFORM_SILICON) {
    return EFI_SUCCESS;
  }

  IortNode = PropNode->IortNode;
  if (IortNode->Token != CM_NULL_TOKEN) {
    return EFI_SUCCESS;
  }

  IortNode->Token = PropNode->Token;
  NV_ASSERT_RETURN (PropNode->RegArray != NULL, return EFI_DEVICE_ERROR, "%a: Trying to parse a node that has a NULL RegArray\n", __FUNCTION__);
  IortNode->BaseAddress = PropNode->RegArray[0].BaseAddress;

  if (PropNode->RegCount > 1) {
    IortNode->Page1BaseAddress = PropNode->RegArray[1].BaseAddress;
  }

  // Only expect 1 interrupt
  NumberOfInterrupts = 1;
  Status             = DeviceTreeGetInterrupts (PropNode->NodeOffset, &InterruptData, &NumberOfInterrupts);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get the interrupt for node. DTB says it has %u interrupts\n", __FUNCTION__, Status, NumberOfInterrupts));
    NumberOfInterrupts = 0;
  }

  if (NumberOfInterrupts == 0) {
    IortNode->IdMappingCount = 1;
  } else {
    IortNode->IdMappingCount    = 0;
    IortNode->OverflowInterrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData);
  }

  if (PropNode->DevicesPhandle == NVIDIA_DEVICE_TREE_PHANDLE_INVALID) {
    DEBUG ((DEBUG_VERBOSE, "%a: Failed to find \"devices\"\r\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  IortPropNode             = FindPropNodeByPhandleInstance (Private, PropNode->DevicesPhandle, 1);
  IortNode->ReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;

  IortNode->Identifier = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  if (IortNode->IdMappingCount == 1) {
    ASSERT (Private->IdMapIndex < Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes);
    PropNode->IdMapCount = 1;
    IdMapping            = Private->IoNodes[IDMAP_TYPE_INDEX].NodeArray;
    IdMapping           += Private->IdMapIndex;
    Private->IdMapIndex += PropNode->IdMapCount;
    PropNode->IdMapArray = IdMapping;

    ArraySize = 1;
    Status    = DeviceTreeGetMsiParent (PropNode->NodeOffset, &MsiParent, &ArraySize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get msi-parent\n", __FUNCTION__, Status));
      return Status;
    }

    IdMapping->InputBase            = 0;
    IdMapping->OutputBase           = MsiParent.Base;
    IdMapping->NumIds               = 0;
    IdMapping->Flags                = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, MsiParent.Phandle, 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    ASSERT (IdMapping->OutputReferenceToken != CM_NULL_TOKEN);
    IortNode->IdMappingCount = PropNode->IdMapCount;

    Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjIdMappingArray);
    Desc.Size     = PropNode->IdMapCount * sizeof (CM_ARM_ID_MAPPING);
    Desc.Count    = PropNode->IdMapCount;
    Desc.Data     = PropNode->IdMapArray;

    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, &IortNode->IdMappingToken);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add %u IdMaps due to error code %r\n", __FUNCTION__, PropNode->IdMapCount, Status));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

// The order must be ITS, SMMUv1v2/SMMUv3, RootComplex and NamedComponent
STATIC CONST IORT_DEVICE_NODE_MAP  mIortDevTypeMap[] = {
  { EArmObjItsGroup,       "arm,gic-v3-its",        SetupIortNodeForItsGroup, NULL,            NULL,          0 },
  { EArmObjSmmuV1SmmuV2,   "arm,mmu-500",           SetupIortNodeForSmmuV1V2, NULL,            NULL,          0 },
  { EArmObjSmmuV1SmmuV2,   "nvidia,tegra234-smmu",  SetupIortNodeForSmmuV1V2, NULL,            NULL,          0 },
  { EArmObjSmmuV3,         "arm,smmu-v3",           SetupIortNodeForSmmuV3,   NULL,            NULL,          0 },
  { EArmObjRootComplex,    "nvidia,tegra234-pcie",  SetupIortNodeForPciRc,    NULL,            NULL,          1 },
  { EArmObjRootComplex,    "nvidia,th500-pcie",     SetupIortNodeForPciRc,    NULL,            NULL,          0 },
  { EArmObjRootComplex,    "nvidia,tegra264-pcie",  SetupIortNodeForPciRc,    NULL,            NULL,          0 },
  { EArmObjRootComplex,    "pci-host-ecam-generic", SetupIortNodeForPciRc,    NULL,            NULL,          0 },
  { EArmObjNamedComponent, "nvidia,tegra234-nvdla", SetupIortNodeForNComp,    "nvdla0",        "\\_SB.DLA0",  1 },
  { EArmObjNamedComponent, "nvidia,tegra264-nvdla", SetupIortNodeForNComp,    NULL,            "\\_SB.DLA0",  0 },
  { EArmObjNamedComponent, "nvidia,tegra186-qspi",  SetupIortNodeForNComp,    "socket0_qspi1", "\\_SB_.QSP1", 0 },
  { EArmObjNamedComponent, "nvidia,th500-soc-hwpm", SetupIortNodeForNComp,    NULL,            "\\_SB_.HWP0", 0 },
  { EArmObjNamedComponent, "nvidia,th500-psc",      SetupIortNodeForNComp,    NULL,            "\\_SB_.PSC0", 0 },
  { EArmObjPmcg,           "arm,smmu-v3-pmcg",      SetupIortNodeForPmcg,     NULL,            NULL,          0 },
  // { EArmObjNamedComponent, "nvidia,tegra194-rce",    SetupIortNodeForNComp,     NULL,          "\\_SB_.RCE0",    0 },
  // { EArmObjNamedComponent, "nvidia,tegra234-vi",     SetupIortNodeForNComp,     "tegra-vi0",   "\\_SB_.VI00",    0 },
  // { EArmObjNamedComponent, "nvidia,tegra234-vi",     SetupIortNodeForNComp,     "tegra-vi1",   "\\_SB_.VI01",    0 },
  // { EArmObjNamedComponent, "nvidia,tegra194-isp",    SetupIortNodeForNComp,     NULL,          "\\_SB_.ISP0",    0 },
  { EArmObjMax,            NULL,                    NULL,                     NULL,            NULL,          0 }
};

EFI_STATUS
InitializeIoRemappingNodes (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle
  )
{
  CONST IORT_DEVICE_NODE_MAP  *DevMap;
  IORT_PROP_NODE              *PropNode;
  IORT_PRIVATE_DATA           *Private;
  LIST_ENTRY                  *ListEntry;
  EFI_STATUS                  Status;

  // Identifier for all IORT nodes
  UniqueIdentifier = 0;

  Private = &mIortPrivate;
  DevMap  = mIortDevTypeMap;
  InitializeListHead (&Private->PropNodeList);

  // JDS TODO - remove this once its no longer used
  Status = DtPlatformLoadDtb (&Private->DtbBase, &Private->DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a failed to get device tree: %r\r\n", __FUNCTION__, Status));
    return Status;
  }

  // Scan the IORT property nodes in the device tree and add them in the list
  Status = AddIortPropNodes (ParserHandle, Private, DevMap);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  // Allocate space for the IORT nodes
  Status = AllocateIortNodes (ParserHandle, Private);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  // Populate IORT nodes
  for ( ; DevMap->Compatibility != NULL; DevMap++) {
    if (DevMap->SetupIortNode == NULL) {
      continue;
    }

    for_each_list_entry (ListEntry, &Private->PropNodeList) {
      PropNode = IORT_PROP_NODE_FROM_LINK (ListEntry);
      if (PropNode->ObjectId != DevMap->ObjectId) {
        continue;
      }

      Status = DevMap->SetupIortNode (ParserHandle, Private, PropNode);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to setup IORT ObjectId=%d err=%r\r\n", PropNode->ObjectId, Status));
        goto ErrorExit;
      }
    }
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    CleanIortPropNodes (Private);
  }

  return Status;
}

EFI_STATUS
IortInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                  Status;
  IORT_NODE                   *IoNode;
  IORT_PRIVATE_DATA           *Private;
  UINTN                       Index;
  CM_OBJ_DESCRIPTOR           Desc;
  CM_STD_OBJ_ACPI_TABLE_INFO  AcpiTableHeader;
  UINTN                       DataSize;
  UINT32                      EnableIortTableGen;
  TEGRA_PLATFORM_TYPE         PlatformType;
  UINT32                      ChipID;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  ChipID = TegraGetChipID ();

  // T234 require enable variable
  if ((ChipID == T234_CHIP_ID)) {
    DataSize = sizeof (EnableIortTableGen);
    Status   = gRT->GetVariable (IORT_TABLE_GEN, &gNVIDIATokenSpaceGuid, NULL, &DataSize, &EnableIortTableGen);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r querying %s variable\n", __FUNCTION__, Status, IORT_TABLE_GEN));
      goto CleanupAndReturn;
    }

    if (EnableIortTableGen == 0) {
      Status = EFI_SUCCESS;
      goto CleanupAndReturn;
    }
  }

  InitializeIoRemappingNodes (ParserHandle);

  Private = &mIortPrivate;

  // Create a ACPI Table Entry
  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_IO_REMAPPING_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_IO_REMAPPING_TABLE_REVISION_06;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdIort);
  AcpiTableHeader.AcpiTableData      = NULL;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to add IORT ACPI table - %r\r\n", __func__, Status));
    goto CleanupAndReturn;
  }

  PlatformType = TegraGetPlatform ();

  for (Index = 0; Index < MAX_NUMBER_OF_IORT_TYPE; Index++) {
    if ((Index == IORT_TYPE_INDEX (EArmObjPmcg)) &&
        (PlatformType != TEGRA_PLATFORM_SILICON))
    {
      continue;
    }

    IoNode = &Private->IoNodes[Index];
    if ((IoNode->NumberOfNodes != 0) && (Index != IDMAP_TYPE_INDEX)) {
      Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (Index + MIN_IORT_OBJID);
      Desc.Size     = IoNode->NumberOfNodes * IoNode->SizeOfNode;
      Desc.Count    = IoNode->NumberOfNodes;
      Desc.Data     = IoNode->NodeArray;

      Status = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, IoNode->TokenArray, CM_NULL_TOKEN);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add IoNodes for index %lu\n", __FUNCTION__, Status, Index));
        goto CleanupAndReturn;
      }

      DEBUG ((DEBUG_INFO, "%a: Installed IORT %d \r\r\n", __FUNCTION__, Index + MIN_IORT_OBJID));
    }
  }

  DEBUG ((DEBUG_INFO, "%a: Installed IORT \r\r\n", __FUNCTION__));

CleanupAndReturn:
  return Status;
}

REGISTER_PARSER_FUNCTION (IortInfoParser, "skip-iort-table")
