/** @file
  Configuration Manager Data of IO Remapping Table

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/NVIDIADebugLib.h>
#include <libfdt.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "NvCmObjectDescUtility.h"

#include <T234/T234Definitions.h>
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

// JDS TODO - need to refactor to base on the new DeviceTreeHelperLib

/**
  Function map region into GCD and MMU

  @param[in]  BaseAddress     Base address of region
  @param[in]  Size            Size of region

  @return EFI_SUCCESS         GCD/MMU Updated

**/
STATIC
EFI_STATUS
AddIortMemoryRegion (
  IN  UINT64  BaseAddress,
  IN  UINT64  Size
  )
{
  EFI_STATUS  Status;
  UINT64      AlignedBaseAddress;
  UINT64      AlignedSize;
  UINT64      AlignedEnd;
  UINT64      ScanLocation;

  AlignedBaseAddress = BaseAddress & ~(SIZE_4KB-1);
  AlignedSize        = ALIGN_VALUE (Size, SIZE_4KB);
  AlignedEnd         = AlignedBaseAddress + AlignedSize;

  ScanLocation = AlignedBaseAddress;
  while (ScanLocation < AlignedEnd) {
    EFI_GCD_MEMORY_SPACE_DESCRIPTOR  MemorySpace;
    UINT64                           OverlapSize;

    Status = gDS->GetMemorySpaceDescriptor (ScanLocation, &MemorySpace);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_INFO,
        "%a: Failed to GetMemorySpaceDescriptor (0x%llx): %r.\r\n",
        __FUNCTION__,
        ScanLocation,
        Status
        ));
      return Status;
    }

    OverlapSize = MIN (MemorySpace.BaseAddress + MemorySpace.Length, AlignedEnd) - ScanLocation;
    if (MemorySpace.GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
      Status = gDS->AddMemorySpace (
                      EfiGcdMemoryTypeMemoryMappedIo,
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_INFO,
          "%a: Failed to AddMemorySpace: (0x%llx, 0x%llx) %r.\r\n",
          __FUNCTION__,
          ScanLocation,
          OverlapSize,
          Status
          ));
        return Status;
      }

      Status = gDS->SetMemorySpaceAttributes (
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_INFO,
          "%a: Failed to SetMemorySpaceAttributes: (0x%llx, 0x%llx) %r.\r\n",
          __FUNCTION__,
          ScanLocation,
          OverlapSize,
          Status
          ));
        return Status;
      }
    }

    ScanLocation += OverlapSize;
  }

  return EFI_SUCCESS;
}

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
    if (IoNode->NodeArray != NULL) {
      FreePool (IoNode->NodeArray);
    }

    if (IoNode->TokenArray != NULL) {
      FreePool (IoNode->TokenArray);
    }
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
  CONST VOID    *Prop;
  CONST UINT64  *IntProp;
  UINT64        DmaAddr;
  UINT32        AddrLimit;
  INT32         PropSize;

  // TODO: add support for multi 'dma-ranges' entries if needed
  Prop = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "dma-ranges", &PropSize);
  if ((Prop == NULL) || (PropSize != DMARANGE_PROP_LENGTH)) {
    return T234_PCIE_ADDRESS_BITS;
  }

  IntProp  = Prop + sizeof (UINT32);
  DmaAddr  = SwapBytes64 (IntProp[1]);  // DEV DMA range start address
  DmaAddr += SwapBytes64 (IntProp[2]);  // DEV DMA range length

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
  Get #address-cells and #size-cells properties from nvidia,tegra234-host1x

  @param[in]      Private       Pointer to the module private data
  @param[in, out] AddressCells  Pointer to #address-cells data
  @param[in, out] SizeCells     Pointer to #size-cells data

  @return EFI_SUCCESS           AddressCells and SizeCells values updated

**/
STATIC
EFI_STATUS
GetAddressSizeCells (
  IN IORT_PRIVATE_DATA  *Private,
  IN OUT INT32          *AddressCells,
  IN OUT INT32          *SizeCells
  )
{
  INT32        NodeOffset;
  INT32        Length;
  CONST INT32  *Prop;

  NodeOffset = -1;

  NodeOffset = fdt_node_offset_by_compatible (
                 Private->DtbBase,
                 NodeOffset,
                 "nvidia,tegra234-host1x"
                 );

  if (NodeOffset <= 0) {
    return EFI_SUCCESS;
  }

  Prop = fdt_getprop (Private->DtbBase, NodeOffset, "#address-cells", &Length);
  if (Prop == NULL) {
    DEBUG ((DEBUG_WARN, "%a: Device does not have #address-cells property.\r\n", __FUNCTION__));
  }

  if (Length == 4) {
    *AddressCells = fdt32_to_cpu (*Prop);
  }

  Prop = fdt_getprop (Private->DtbBase, NodeOffset, "#size-cells", &Length);
  if (Prop == NULL) {
    DEBUG ((DEBUG_WARN, "%a: Device does not have #size-cells property.\r\n", __FUNCTION__));
  }

  if (Length == 4) {
    *SizeCells = fdt32_to_cpu (*Prop);
  }

  return EFI_SUCCESS;
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
  INT32           NodeOffset;
  IORT_PROP_NODE  *PropNode;
  CONST VOID      *Prop;
  CONST UINT64    *RegProp;
  CONST UINT64    *IortNodeRegProp;
  CONST UINT32    *MsiProp;
  CONST UINT32    *IommusProp;
  CONST UINT32    *IommuMapProp;
  INT32           PropSize;
  INT32           RegPropSize;
  UINT32          ItsNodePresent;
  UINT32          DualSmmuPresent;
  UINT32          Indx;
  CONST CHAR8     *AliasName;
  INT32           AddressCells;
  INT32           SizeCells;

  ItsNodePresent = 0;
  AddressCells   = 1;
  SizeCells      = 1;

  GetAddressSizeCells (Private, &AddressCells, &SizeCells);

  for ( ; DevMap->Compatibility != NULL; DevMap++) {
    if ((DevMap->ObjectId == EArmObjNamedComponent) && (DevMap->ObjectName == NULL)) {
      DEBUG ((DEBUG_WARN, "%a: Invalid named component \r\n", __FUNCTION__));
      continue;
    }

    NodeOffset = -1;
    do {
      if ((DevMap->ObjectId == EArmObjNamedComponent) && (DevMap->Alias != NULL)) {
        AliasName = fdt_get_alias (Private->DtbBase, DevMap->Alias);
        if (AliasName == NULL) {
          DEBUG ((DEBUG_WARN, "%a: Invalid alias for named component %a \r\n", __FUNCTION__, AliasName));
          break;
        }

        NodeOffset = fdt_path_offset (Private->DtbBase, AliasName);
      } else {
        NodeOffset = fdt_node_offset_by_compatible (
                       Private->DtbBase,
                       NodeOffset,
                       DevMap->Compatibility
                       );
      }

      // All the requested DTB nodes are optional
      if (NodeOffset <= 0) {
        break;
      }

      // The reg property is mandatory with requested entries
      RegProp = fdt_getprop (Private->DtbBase, NodeOffset, "reg", &RegPropSize);
      if (RegProp == NULL) {
        DEBUG ((DEBUG_WARN, "%a: Device does not have a reg property. It could be a test device.\r\n", __FUNCTION__));
      }

      if (RegPropSize < (AddressCells + SizeCells) * sizeof (INT32)) {
        DEBUG ((DEBUG_WARN, "%a: Reg property size is smaller than expected\r\n", __FUNCTION__));
        break;
      }

      DualSmmuPresent = 0;
      if ((DevMap->ObjectId == EArmObjSmmuV1SmmuV2) && ((RegPropSize / ((AddressCells + SizeCells) * sizeof (INT32))) > 1)) {
        DualSmmuPresent = 1;
      }

      for (Indx = 0; Indx <= DualSmmuPresent; Indx++) {
        IortNodeRegProp = RegProp + (Indx * REG_PROP_CELL_SIZE);
        MsiProp         = NULL;
        IommusProp      = NULL;
        IommuMapProp    = NULL;

        // Check DTB status and skip if it's not enabled
        Prop = fdt_getprop (Private->DtbBase, NodeOffset, "status", &PropSize);
        if ((Prop != NULL) && !((AsciiStrCmp (Prop, "okay") == 0) || (AsciiStrCmp (Prop, "ok") == 0))) {
          continue;
        }

        if (DevMap->ObjectId == EArmObjItsGroup) {
          ItsNodePresent = 1;
          Private->IoNodes[ITSIDENT_TYPE_INDEX].NumberOfNodes++;
          goto AllocatePropNode;
        }

        // Check "msi-map" property for all DTB nodes
        MsiProp = fdt_getprop (Private->DtbBase, NodeOffset, "msi-map", &PropSize);
        if ((MsiProp == NULL) || (PropSize != MSIMAP_PROP_LENGTH) || (ItsNodePresent != 1)) {
          MsiProp = NULL;
        } else {
          // Skip if the target DTB node is not valid
          if (FindPropNodeByPhandleInstance (Private, SwapBytes32 (MsiProp[1]), 1) == NULL) {
            // Alias path would be unique
            if (DevMap->Alias != NULL) {
              break;
            }

            continue;
          }

          Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
        }

        // Check "iommu-map" property only for non-SMMUv1v2 nodes
        if (DevMap->ObjectId != EArmObjSmmuV1SmmuV2) {
          IommusProp = fdt_getprop (Private->DtbBase, NodeOffset, "iommus", &PropSize);
          if ((IommusProp != NULL) && (PropSize == IOMMUS_PROP_LENGTH)) {
            // Check DTB status and skip if it's not enabled
            if (FindPropNodeByPhandleInstance (Private, SwapBytes32 (IommusProp[0]), 1) == NULL) {
              // Alias path would be unique
              if (DevMap->Alias != NULL) {
                break;
              }

              continue;
            }

            Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
            if (DevMap->DualSmmuPresent == 1) {
              Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
            }
          } else {
            IommuMapProp = fdt_getprop (Private->DtbBase, NodeOffset, "iommu-map", &PropSize);
            if ((IommuMapProp == NULL) || (PropSize != IOMMUMAP_PROP_LENGTH)) {
              IommuMapProp = NULL;
              // Skip this node if both 'iommu-map' and 'msi-map' are not defined
              if (MsiProp == NULL) {
                // Alias path would be unique
                if (DevMap->Alias != NULL) {
                  break;
                }

                continue;
              }
            } else {
              // Check DTB status and skip if it's not enabled
              if (FindPropNodeByPhandleInstance (Private, SwapBytes32 (IommuMapProp[1]), 1) == NULL) {
                // Alias path would be unique
                if (DevMap->Alias != NULL) {
                  break;
                }

                continue;
              }

              Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
              if (DevMap->DualSmmuPresent == 1) {
                Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
              }
            }
          }
        } else {
          Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
        }

AllocatePropNode:
        PropNode = AllocateZeroPool (sizeof (IORT_PROP_NODE));
        if (PropNode == NULL) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to allocate list entry\r\n", __FUNCTION__));
          return EFI_OUT_OF_RESOURCES;
        }

        PropNode->Phandle         = fdt_get_phandle (Private->DtbBase, NodeOffset);
        PropNode->RegProp         = IortNodeRegProp;
        PropNode->MsiProp         = MsiProp;
        PropNode->IommusProp      = IommusProp;
        PropNode->IommuMapProp    = IommuMapProp;
        PropNode->DualSmmuPresent = DevMap->DualSmmuPresent;
        PropNode->NodeOffset      = NodeOffset;
        PropNode->ObjectId        = DevMap->ObjectId;
        PropNode->ObjectName      = DevMap->ObjectName;
        PropNode->Signature       = IORT_PROP_NODE_SIGNATURE;
        InsertTailList (&Private->PropNodeList, &PropNode->Link);
        Private->IoNodes[IORT_TYPE_INDEX (DevMap->ObjectId)].NumberOfNodes++;
      }

      // Alias path would be unique
      if (DevMap->Alias != NULL) {
        break;
      }
    } while (1);
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
        ASSERT (Index0 <= IoNode->SizeOfNode);
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
  EFI_STATUS                 Status;
  CM_ARM_SMMUV1_SMMUV2_NODE  *IortNode;
  CM_ARM_ID_MAPPING          *IdMapping;
  LIST_ENTRY                 *ListEntry;
  IORT_PROP_NODE             *TmpPropNode;
  IORT_PROP_NODE             *IortPropNode;
  CONST UINT32               *MsiProp;
  CM_OBJ_DESCRIPTOR          Desc;

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
      if (TmpPropNode->IommusProp != NULL) {
        IortPropNode = FindPropNodeByPhandleInstance (Private, SwapBytes32 (TmpPropNode->IommusProp[0]), 1);
        if (!IortPropNode || (IortPropNode->IortNode != IortNode)) {
          continue;
        }
      } else if (TmpPropNode->IommuMapProp != NULL) {
        IortPropNode = FindPropNodeByPhandleInstance (Private, SwapBytes32 (TmpPropNode->IommuMapProp[1]), 1);
        if (!IortPropNode || (IortPropNode->IortNode != IortNode)) {
          continue;
        }
      } else {
        continue;
      }
    }

    MsiProp = TmpPropNode->MsiProp;
    if (MsiProp == NULL) {
      continue;
    }

    ASSERT (Private->IdMapIndex < Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes);
    IdMapping->InputBase            = SwapBytes32 (MsiProp[0]);
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, SwapBytes32 (MsiProp[1]), 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    IdMapping->OutputBase           = SwapBytes32 (MsiProp[2]);
    IdMapping->NumIds               = SwapBytes32 (MsiProp[3]) - 1;
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
  CONST VOID                         *Prop;
  INT32                              PropSize;
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

  Prop = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "#global-interrupts", &PropSize);
  if ((Prop == NULL) || (PropSize == 0)) {
    DEBUG ((DEBUG_VERBOSE, "%a: Failed to find \"#global-interrupts\"\r\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto ErrorExit;
  }

  GlobalInterruptCnt = SwapBytes32 (*(CONST UINT32 *)Prop);
  if (GlobalInterruptCnt > 2) {
    DEBUG ((DEBUG_ERROR, "Global interrupts %u more than 2. No space to store more than 2 global interrupts\n", GlobalInterruptCnt));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Status = GetDeviceTreeInterrupts (PropNode->NodeOffset, InterruptData, &InterruptSize);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    if (InterruptData != NULL) {
      FreePool (InterruptData);
      InterruptData = NULL;
    }

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

  if (GlobalInterruptCnt == 1) {
    IortNode->SMMU_NSgIrpt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData[0]);

    IortNode->SMMU_NSgIrptFlags = (InterruptData[0].Flag == INTERRUPT_HI_LEVEL ?
                                   EFI_ACPI_IRQ_LEVEL_TRIGGERED : EFI_ACPI_IRQ_EDGE_TRIGGERED);
  }

  if (GlobalInterruptCnt == 2) {
    IortNode->SMMU_NSgIrpt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData[0]);

    IortNode->SMMU_NSgIrptFlags = (InterruptData[0].Flag == INTERRUPT_HI_LEVEL ?
                                   EFI_ACPI_IRQ_LEVEL_TRIGGERED : EFI_ACPI_IRQ_EDGE_TRIGGERED);
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
  if (InterruptData != NULL) {
    FreePool (InterruptData);
    InterruptData = NULL;
  }

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
  if (PmuInterruptData != NULL) {
    FreePool (PmuInterruptData);
    PmuInterruptData = NULL;
  }

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
  EFI_STATUS                        Status;
  CM_ARM_SMMUV1_SMMUV2_NODE         *IortNode;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterData;
  UINT32                            RegisterSize;

  Status       = EFI_SUCCESS;
  RegisterData = NULL;
  IortNode     = PropNode->IortNode;
  if (IortNode->Token != CM_NULL_TOKEN) {
    goto ErrorExit;
  }

  IortNode->Token      = PropNode->Token;
  IortNode->Identifier = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  IortNode->BaseAddress = 0;
  RegisterSize          = 0;
  Status                = GetDeviceTreeRegisters (PropNode->NodeOffset, RegisterData, &RegisterSize);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    if (RegisterData != NULL) {
      FreePool (RegisterData);
      RegisterData = NULL;
    }

    RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
    if (RegisterData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Status = GetDeviceTreeRegisters (PropNode->NodeOffset, RegisterData, &RegisterSize);
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }
  } else if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  if ((PropNode->RegProp != NULL) && (RegisterData[0].BaseAddress == SwapBytes64 (*PropNode->RegProp)) && (RegisterData != NULL)) {
    IortNode->BaseAddress = RegisterData[0].BaseAddress;
    IortNode->Span        = RegisterData[0].Size;
  }

  if ((RegisterSize > 1) && (RegisterData[1].BaseAddress == SwapBytes64 (*PropNode->RegProp))) {
    IortNode->BaseAddress = RegisterData[1].BaseAddress;
    IortNode->Span        = RegisterData[1].Size;
  }

  IortNode->Model = EFI_ACPI_IORT_SMMUv1v2_MODEL_MMU500;
  IortNode->Flags = EFI_ACPI_IORT_SMMUv1v2_FLAG_COH_WALK;

  Status = SetupGlobalContextIrqForSmmuV1V2 (ParserHandle, Private, PropNode);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = SetupPmuIrqForSmmuV1V2 (ParserHandle, Private, PropNode);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  // Map SMMU base address in MMU to support SBSA-ACS
  Status = AddIortMemoryRegion (IortNode->BaseAddress, SIZE_4KB);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  return SetupIortIdMappingForSmmuV1V2 (ParserHandle, Private, PropNode);

ErrorExit:
  if (RegisterData != NULL) {
    FreePool (RegisterData);
  }

  return Status;
}

/**
  Populate data of PCI RC and ID mapping nodes defining SMMU and MSI setup.
  Mapping PCI nodes to SMMUv1v2 from the device tree.
  Mapping SMMUV1V2 to Gic Msi frame nodes from the device tree.
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
  EFI_STATUS                Status;
  CM_ARM_ROOT_COMPLEX_NODE  *IortNode;
  CM_ARM_ID_MAPPING         *IdMapping;
  CONST UINT32              *Prop;
  INT32                     PropSize;
  UINT32                    IdMapFlags;
  IORT_PROP_NODE            *IortPropNode;
  CM_OBJ_DESCRIPTOR         Desc;

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

  if (fdt_get_property (Private->DtbBase, PropNode->NodeOffset, "dma-coherent", NULL) != NULL) {
    IortNode->CacheCoherent     |= EFI_ACPI_IORT_MEM_ACCESS_PROP_CCA;
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_CPM;
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_DACS;
  }

  Prop = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "ats-supported", NULL);
  if (Prop == NULL) {
    IortNode->AtsAttribute = EFI_ACPI_IORT_ROOT_COMPLEX_ATS_UNSUPPORTED;
  } else {
    IortNode->AtsAttribute = EFI_ACPI_IORT_ROOT_COMPLEX_ATS_SUPPORTED;
  }

  Prop = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "linux,pci-domain", NULL);
  if (Prop != NULL) {
    IortNode->PciSegmentNumber = SwapBytes32 (Prop[0]);
  }

  Prop = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "iommu-map-mask", &PropSize);
  if ((Prop == NULL) || (PropSize == 0) || (SwapBytes32 (*(CONST UINT32 *)Prop) != 0)) {
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

  if (PropNode->IommusProp != NULL) {
    Prop = PropNode->IommusProp;

    // Create Id Mapping Node for iommus and bind it to the PCI IORT node
    IdMapping->InputBase            = 0;
    IdMapping->OutputBase           = SwapBytes32 (Prop[1]);
    IdMapping->NumIds               = 0;
    IdMapping->Flags                = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, SwapBytes32 (Prop[0]), 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    ASSERT (IdMapping->OutputReferenceToken != 0);

    if (PropNode->DualSmmuPresent == 1) {
      IdMapping++;
      IdMapping->InputBase            = 0x1;
      IdMapping->OutputBase           = SwapBytes32 (Prop[1]);
      IdMapping->NumIds               = 0;
      IdMapping->Flags                = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
      IortPropNode                    = FindPropNodeByPhandleInstance (Private, SwapBytes32 (Prop[0]), 2);
      IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
      ASSERT (IdMapping->OutputReferenceToken != 0);
    }
  } else {
    // Prop = (PropNode->IommuMapProp != NULL) ? PropNode->IommuMapProp : PropNode->MsiProp;
    Prop = PropNode->IommuMapProp;
    ASSERT (Prop != NULL);

    // Create Id Mapping Node for iommu-map and bind it to the PCI IORT node
    IdMapping->InputBase            = SwapBytes32 (Prop[0]);
    IdMapping->OutputBase           = SwapBytes32 (Prop[2]);
    IdMapping->NumIds               = SwapBytes32 (Prop[3]) - 1;
    IdMapping->Flags                = IdMapFlags;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, SwapBytes32 (Prop[1]), 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    ASSERT (IdMapping->OutputReferenceToken != 0);

    if (PropNode->DualSmmuPresent == 1) {
      IdMapping++;
      IdMapping->InputBase            = SwapBytes32 (Prop[0]);
      IdMapping->OutputBase           = SwapBytes32 (Prop[2]);
      IdMapping->NumIds               = SwapBytes32 (Prop[3]) - 1;
      IdMapping->Flags                = IdMapFlags;
      IortPropNode                    = FindPropNodeByPhandleInstance (Private, SwapBytes32 (Prop[1]), 2);
      IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
      ASSERT (IdMapping->OutputReferenceToken != 0);
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
  Populate data of Named Component and and ID mapping nodes defining SMMU and MSI setup.
  Mapping Named Component nodes to SMMUv1 v2 from the device tree.
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
  EFI_STATUS                   Status;
  CM_ARM_NAMED_COMPONENT_NODE  *IortNode;
  CM_ARM_ID_MAPPING            *IdMapping;
  CONST UINT32                 *Prop;
  IORT_PROP_NODE               *IortPropNode;
  CM_OBJ_DESCRIPTOR            Desc;

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

  IortNode->CacheCoherent = 0;
  IortNode->Identifier    = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  if (fdt_get_property (Private->DtbBase, PropNode->NodeOffset, "dma-coherent", NULL) != NULL) {
    IortNode->CacheCoherent     |= EFI_ACPI_IORT_MEM_ACCESS_PROP_CCA;
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_CPM;
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_DACS;
  }

  ASSERT (Private->IdMapIndex < Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes);
  IdMapping            = Private->IoNodes[IDMAP_TYPE_INDEX].NodeArray;
  IdMapping           += Private->IdMapIndex;
  Private->IdMapIndex += PropNode->IdMapCount;
  PropNode->IdMapArray = IdMapping;

  if (PropNode->IommusProp != NULL) {
    Prop = PropNode->IommusProp;

    // Create Id Mapping Node for iommus and bind it to the Named component IORT node
    IdMapping->InputBase            = 0x0;
    IdMapping->OutputBase           = SwapBytes32 (Prop[1]);
    IdMapping->NumIds               = 0;
    IdMapping->Flags                = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, SwapBytes32 (Prop[0]), 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    ASSERT (IdMapping->OutputReferenceToken != 0);

    if (PropNode->DualSmmuPresent == 1) {
      IdMapping++;
      IdMapping->InputBase            = 0x1;
      IdMapping->OutputBase           = SwapBytes32 (Prop[1]);
      IdMapping->NumIds               = 0;
      IdMapping->Flags                = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
      IortPropNode                    = FindPropNodeByPhandleInstance (Private, SwapBytes32 (Prop[0]), 2);
      IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
      ASSERT (IdMapping->OutputReferenceToken != 0);
    }
  } else {
    Prop = (PropNode->IommuMapProp != NULL) ? PropNode->IommuMapProp : PropNode->MsiProp;
    ASSERT (Prop != NULL);

    // Create Id Mapping Node for iommu-map and bind it to the Named Component node
    IdMapping->InputBase            = SwapBytes32 (Prop[0]);
    IdMapping->OutputBase           = SwapBytes32 (Prop[2]);
    IdMapping->NumIds               = SwapBytes32 (Prop[3]) - 1;
    IdMapping->Flags                = 0;
    IortPropNode                    = FindPropNodeByPhandleInstance (Private, SwapBytes32 (Prop[1]), 1);
    IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
    ASSERT (IdMapping->OutputReferenceToken != 0);

    if (PropNode->DualSmmuPresent == 1) {
      IdMapping++;
      IdMapping->InputBase            = SwapBytes32 (Prop[0]);
      IdMapping->OutputBase           = SwapBytes32 (Prop[2]);
      IdMapping->NumIds               = SwapBytes32 (Prop[3]) - 1;
      IdMapping->Flags                = 0;
      IortPropNode                    = FindPropNodeByPhandleInstance (Private, SwapBytes32 (Prop[1]), 2);
      IdMapping->OutputReferenceToken = IortPropNode ? IortPropNode->Token : CM_NULL_TOKEN;
      ASSERT (IdMapping->OutputReferenceToken != 0);
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

// The order must be SMMUv1v2, RootComplex and NamedComponent
STATIC CONST IORT_DEVICE_NODE_MAP  mIortDevTypeMap[] = {
  { EArmObjItsGroup,       "arm,gic-v3-its",        SetupIortNodeForItsGroup, NULL,     NULL,         0 },
  { EArmObjSmmuV1SmmuV2,   "arm,mmu-500",           SetupIortNodeForSmmuV1V2, NULL,     NULL,         0 },
  { EArmObjSmmuV1SmmuV2,   "nvidia,tegra234-smmu",  SetupIortNodeForSmmuV1V2, NULL,     NULL,         0 },
  { EArmObjRootComplex,    "nvidia,tegra234-pcie",  SetupIortNodeForPciRc,    NULL,     NULL,         1 },
  { EArmObjNamedComponent, "nvidia,tegra234-nvdla", SetupIortNodeForNComp,    "nvdla0", "\\_SB.DLA0", 1 },
  // { EArmObjNamedComponent, "nvidia,tegra194-rce",    SetupIortNodeForNComp,     NULL,          "\\_SB_.RCE0",    0 },
  // { EArmObjNamedComponent, "nvidia,tegra234-vi",     SetupIortNodeForNComp,     "tegra-vi0",   "\\_SB_.VI00",    0 },
  // { EArmObjNamedComponent, "nvidia,tegra234-vi",     SetupIortNodeForNComp,     "tegra-vi1",   "\\_SB_.VI01",    0 },
  // { EArmObjNamedComponent, "nvidia,tegra194-isp",    SetupIortNodeForNComp,     NULL,          "\\_SB_.ISP0",    0 },
  { EArmObjMax,            NULL,                    NULL,                     NULL,     NULL,         0 }
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

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

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

  Desc.ObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
  Desc.Size     = sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
  Desc.Count    = 1;
  Desc.Data     = &AcpiTableHeader;

  Status = NvExtendCmObj (ParserHandle, &Desc, CM_NULL_TOKEN, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < MAX_NUMBER_OF_IORT_TYPE; Index++) {
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
