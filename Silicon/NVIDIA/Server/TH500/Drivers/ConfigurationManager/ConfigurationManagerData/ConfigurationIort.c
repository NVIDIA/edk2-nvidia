/** @file
  Configuration Manager Data of IO Remapping Table

  Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <libfdt.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <Protocol/AmlPatchProtocol.h>

#include <TH500/TH500Definitions.h>
#include "ConfigurationIortPrivate.h"

#define for_each_list_entry(tmp, list)       \
        for (tmp = GetFirstNode (list);      \
             !IsNull (list, tmp);            \
             tmp = GetNextNode (list, tmp))

extern NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol;

STATIC IORT_PRIVATE_DATA  mIortPrivate = {
  .Signature                                               = IORT_DATA_SIGNATURE,
  .IoNodes[IORT_TYPE_INDEX (EArmObjNamedComponent)]        = { sizeof (CM_ARM_NAMED_COMPONENT_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjRootComplex)]           = { sizeof (CM_ARM_ROOT_COMPLEX_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjSmmuV3)]                = { sizeof (CM_ARM_SMMUV3_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjPmcg)]                  = { sizeof (CM_ARM_PMCG_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjIdMappingArray)]        = { sizeof (CM_ARM_ID_MAPPING), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjItsGroup)]              = { sizeof (CM_ARM_ITS_GROUP_NODE), },
  .IoNodes[IORT_TYPE_INDEX (EArmObjGicItsIdentifierArray)] = { sizeof (CM_ARM_ITS_IDENTIFIER), }
};

UINT32  UniqueIdentifier;

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
        EFI_D_INFO,
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
          EFI_D_INFO,
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
          EFI_D_INFO,
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
  Find a IORT node for a given phandle (DTB)

  @param[in] Private       Pointer to the module private data
  @param[in] Phandle       Phandle of DTB node

  @return address of the IORT node if found
  @retval 0 if not found

**/
STATIC
CM_OBJECT_TOKEN
FindIortNodeByPhandle (
  IN IORT_PRIVATE_DATA  *Private,
  IN CM_OBJECT_TOKEN    Phandle
  )
{
  IORT_PROP_NODE  *PropNode;
  LIST_ENTRY      *ListEntry;

  for_each_list_entry (ListEntry, &Private->PropNodeList) {
    PropNode = IORT_PROP_NODE_FROM_LINK (ListEntry);
    if (fdt_get_phandle (Private->DtbBase, PropNode->NodeOffset) == Phandle) {
      return (CM_OBJECT_TOKEN)(PropNode->IortNode);
    }
  }
  return 0;
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
    return TH500_PCIE_ADDRESS_BITS;
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
  Find a PROP node for a given phandle (DTB)

  @param[in] Private       Pointer to the module private data
  @param[in] Phandle       Phandle of DTB node

  @return address of the PROP node if found
  @retval 0 if not found

**/
STATIC
IORT_PROP_NODE *
FindPropNodeByPhandle (
  IN IORT_PRIVATE_DATA  *Private,
  IN CM_OBJECT_TOKEN    Phandle
  )
{
  IORT_PROP_NODE  *PropNode;
  LIST_ENTRY      *ListEntry;

  for_each_list_entry (ListEntry, &Private->PropNodeList) {
    PropNode = IORT_PROP_NODE_FROM_LINK (ListEntry);
    if (fdt_get_phandle (Private->DtbBase, PropNode->NodeOffset) == Phandle) {
      return PropNode;
    }
  }
  return NULL;
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
  IN OUT   IORT_PRIVATE_DATA     *Private,
  IN CONST IORT_DEVICE_NODE_MAP  *DevMap
  )
{
  INT32           NodeOffset;
  IORT_PROP_NODE  *PropNode;
  CONST VOID      *Prop;
  CONST UINT64    *RegProp;
  CONST UINT32    *MsiProp;
  CONST UINT32    *IommusProp;
  CONST UINT32    *IommuMapProp;
  INT32           PropSize;
  CONST CHAR8     *AliasName;

  for ( ; DevMap->Compatibility != NULL; DevMap++) {
    if ((DevMap->ObjectId == EArmObjNamedComponent) && (DevMap->ObjectName == NULL)) {
      DEBUG ((EFI_D_WARN, "%a: Invalid named component \r\n", __FUNCTION__));
      continue;
    }

    NodeOffset = -1;
    do {
      // check for aliases in dtb
      if ((DevMap->ObjectId == EArmObjNamedComponent) && (DevMap->Alias != NULL)) {
        AliasName = fdt_get_alias (Private->DtbBase, DevMap->Alias);
        if (AliasName == NULL) {
          DEBUG ((EFI_D_WARN, "%a: Invalid alias for named component \r\n", __FUNCTION__));
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
      RegProp = fdt_getprop (Private->DtbBase, NodeOffset, "reg", NULL);
      if (RegProp == NULL) {
        DEBUG ((EFI_D_WARN, "%a: Device does not have a reg property. It could be a test device.\r\n", __FUNCTION__));
      }

      MsiProp      = NULL;
      IommusProp   = NULL;
      IommuMapProp = NULL;

      if (DevMap->ObjectId == EArmObjItsGroup) {
        Private->IoNodes[ITSIDENT_TYPE_INDEX].NumberOfNodes++;
        goto AllocatePropNode;
      }

      // Check DTB status and skip if it's not enabled
      Prop = fdt_getprop (Private->DtbBase, NodeOffset, "status", &PropSize);
      if ((Prop != NULL) && !((AsciiStrCmp (Prop, "okay") == 0) || (AsciiStrCmp (Prop, "ok") == 0))) {
        // Alias path would be unique
        if (DevMap->Alias != NULL) {
          break;
        }

        continue;
      }

      // Check "msi-map" property for all DTB nodes
      MsiProp = fdt_getprop (Private->DtbBase, NodeOffset, "msi-map", &PropSize);
      if ((MsiProp == NULL) || (PropSize != MSIMAP_PROP_LENGTH)) {
        MsiProp = NULL;
      } else {
        // Skip if the target DTB node is not valid
        if (FindPropNodeByPhandle (Private, SwapBytes32 (MsiProp[1])) == NULL) {
          // Alias path would be unique
          if (DevMap->Alias != NULL) {
            break;
          }

          continue;
        }

        Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
      }

      // Check "iommu-map" property only for non-SMMUv3 and non-PMCG nodes
      if ((DevMap->ObjectId != EArmObjSmmuV3) && (DevMap->ObjectId != EArmObjPmcg)) {
        IommusProp = fdt_getprop (Private->DtbBase, NodeOffset, "iommus", &PropSize);
        if ((IommusProp != NULL) && (PropSize == IOMMUS_PROP_LENGTH)) {
          // Check DTB status and skip if it's not enabled
          if (FindPropNodeByPhandle (Private, SwapBytes32 (IommusProp[0])) == NULL) {
            // Alias path would be unique
            if (DevMap->Alias != NULL) {
              break;
            }

            continue;
          }

          Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
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
            if (FindPropNodeByPhandle (Private, SwapBytes32 (IommuMapProp[1])) == NULL) {
              // Alias path would be unique
              if (DevMap->Alias != NULL) {
                break;
              }

              continue;
            }

            Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
          }
        }
      } else {
        Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes++;
      }

AllocatePropNode:
      PropNode = AllocateZeroPool (sizeof (IORT_PROP_NODE));
      if (PropNode == NULL) {
        DEBUG ((EFI_D_ERROR, "%a: Failed to allocate list entry\r\n", __FUNCTION__));
        return EFI_OUT_OF_RESOURCES;
      }

      PropNode->RegProp      = RegProp;
      PropNode->MsiProp      = MsiProp;
      PropNode->IommusProp   = IommusProp;
      PropNode->IommuMapProp = IommuMapProp;
      PropNode->NodeOffset   = NodeOffset;
      PropNode->ObjectId     = DevMap->ObjectId;
      PropNode->ObjectName   = DevMap->ObjectName;
      PropNode->Signature    = IORT_PROP_NODE_SIGNATURE;
      InsertTailList (&Private->PropNodeList, &PropNode->Link);
      Private->IoNodes[IORT_TYPE_INDEX (DevMap->ObjectId)].NumberOfNodes++;

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
  IN OUT  IORT_PRIVATE_DATA  *Private
  )
{
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
      DEBUG ((EFI_D_INFO, "%a: No IORT nodes of %d\r\n", __FUNCTION__, (Index + MIN_IORT_OBJID)));
      continue;
    }

    IoNode->NodeArray = AllocateZeroPool (IoNode->NumberOfNodes * IoNode->SizeOfNode);
    if (IoNode->NodeArray == NULL) {
      DEBUG ((
        EFI_D_ERROR,
        "%a: Failed to allocate IORT node of %d\r\n",
        __FUNCTION__,
        (Index + MIN_IORT_OBJID)
        ));
      return EFI_OUT_OF_RESOURCES;
    }

    Index0 = 0;
    for_each_list_entry (ListEntry, &Private->PropNodeList) {
      PropNode = IORT_PROP_NODE_FROM_LINK (ListEntry);
      if (PropNode->ObjectId == (Index + MIN_IORT_OBJID)) {
        ASSERT (Index0 < IoNode->NumberOfNodes);
        PropNode->IortNode = IoNode->NodeArray + (IoNode->SizeOfNode * Index0);
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
  IN OUT  IORT_PRIVATE_DATA  *Private,
  IN OUT  IORT_PROP_NODE     *PropNode
  )
{
  CM_ARM_ITS_GROUP_NODE  *IortNode;
  CM_ARM_ITS_IDENTIFIER  *ItsIdArray;
  UINT32                 ItsId;

  IortNode = PropNode->IortNode;
  if (IortNode->Token != 0) {
    return EFI_SUCCESS;
  }

  ASSERT (Private->ItsIdentifierIndex < Private->IoNodes[ITSIDENT_TYPE_INDEX].NumberOfNodes);

  ItsId             = Private->ItsIdentifierIndex;
  ItsIdArray        = Private->IoNodes[ITSIDENT_TYPE_INDEX].NodeArray;
  ItsIdArray       += ItsId;
  ItsIdArray->ItsId = ItsId;

  IortNode->ItsIdCount = 1;
  IortNode->Token      = (CM_OBJECT_TOKEN)(VOID *)IortNode;
  IortNode->ItsIdToken = (CM_OBJECT_TOKEN)ItsIdArray;
  IortNode->Identifier = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  Private->ItsIdentifierIndex++;

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
  IN OUT  IORT_PRIVATE_DATA  *Private,
  IN OUT  IORT_PROP_NODE     *PropNode
  )
{
  CM_ARM_SMMUV3_NODE  *IortNode;
  CM_ARM_ID_MAPPING   *IdMapping;
  LIST_ENTRY          *ListEntry;
  IORT_PROP_NODE      *TmpPropNode;
  CM_OBJECT_TOKEN     Token;
  CONST UINT32        *MsiProp;

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
        Token = FindIortNodeByPhandle (Private, SwapBytes32 (TmpPropNode->IommusProp[0]));
        if (Token != (CM_OBJECT_TOKEN)IortNode) {
          continue;
        }
      } else if (TmpPropNode->IommuMapProp != NULL) {
        Token = FindIortNodeByPhandle (Private, SwapBytes32 (TmpPropNode->IommuMapProp[1]));
        if (Token != (CM_OBJECT_TOKEN)IortNode) {
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
    Token                           = FindIortNodeByPhandle (Private, SwapBytes32 (MsiProp[1]));
    IdMapping->OutputReferenceToken = Token;
    IdMapping->OutputBase           = SwapBytes32 (MsiProp[2]);
    IdMapping->NumIds               = SwapBytes32 (MsiProp[3]) - 1;
    IdMapping->Flags                = 0;

    if (MsiProp == PropNode->MsiProp) {
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
      (PropNode->MsiProp == NULL) && (PropNode->IdMapCount != 0))
  {
    // As per the IORT specification, DeviceIdMappingIndex must contain a valid
    // index if any one of wired interrupt is zero and msi-map is not defined
    // Retaining this for IORT spec backward compatibility
    IortNode->DeviceIdMappingIndex = PropNode->IdMapCount;
  }

  IortNode->IdMappingCount = PropNode->IdMapCount;
  IortNode->IdMappingToken = (CM_OBJECT_TOKEN)PropNode->IdMapArray;

  return EFI_SUCCESS;
}

/**
  patch SMMUv3 _UID info in dsdt/ssdt table to SMMUv3 iort identifier

  @param[in] IortNode       Pointer to the CM_ARM_SMMUV3_NODE

  @retval EFI_SUCCESS       Success
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
UpdateSmmuV3UidInfo (
  IN  CM_ARM_SMMUV3_NODE  *IortNode
  )
{
  EFI_STATUS            Status;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT32                Identifier;
  UINT32                AcpiSmmuUidPatchNameSize;
  STATIC UINT32         Index = 0;

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
  IN OUT  IORT_PRIVATE_DATA  *Private,
  IN OUT  IORT_PROP_NODE     *PropNode
  )
{
  EFI_STATUS          Status;
  CM_ARM_SMMUV3_NODE  *IortNode;
  CONST VOID          *Prop;
  CONST UINT32        *IrqProp;
  INT32               PropSize;
  INT32               IrqPropSize;
  INT32               IrqPropCnt;
  UINT32              InterruptId;

  IortNode = PropNode->IortNode;
  if (IortNode->Token != 0) {
    return EFI_SUCCESS;
  }

  IortNode->Token        = (CM_OBJECT_TOKEN)(VOID *)IortNode;
  IortNode->VatosAddress = 0;
  IortNode->BaseAddress  = 0;
  if (PropNode->RegProp != NULL) {
    IortNode->BaseAddress = SwapBytes64 (*PropNode->RegProp);
  }

  IortNode->ProximityDomain = 0;
  IortNode->Model           = EFI_ACPI_IORT_SMMUv3_MODEL_GENERIC;
  IortNode->Flags           = EFI_ACPI_IORT_SMMUv3_FLAG_PROXIMITY_DOMAIN;
  IortNode->Identifier      = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  UpdateSmmuV3UidInfo (IortNode);

  if (fdt_get_property (Private->DtbBase, PropNode->NodeOffset, "dma-coherent", NULL) != NULL) {
    IortNode->Flags |= EFI_ACPI_IORT_MEM_ACCESS_PROP_CCA;
  }

  Prop = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "numa-node-id", &PropSize);
  if (Prop != NULL) {
    IortNode->ProximityDomain = SwapBytes32 (*(CONST UINT32 *)Prop);
  }

  Prop = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "interrupt-names", &PropSize);
  if ((Prop == NULL) || (PropSize == 0)) {
    DEBUG ((EFI_D_VERBOSE, "%a: Failed to find \"interrupt-names\"\r\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  IrqProp = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "interrupts", &IrqPropSize);
  if ((IrqProp == NULL) || (IrqPropSize == 0)) {
    DEBUG ((EFI_D_VERBOSE, "%a: Failed to find \"interrupts\"\r\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  IrqPropCnt = IrqPropSize / IRQ_PROP_LENGTH;

  if (0 == AsciiStrCmp (Prop, "combined")) {
    InterruptId              = SwapBytes32 (*(IrqProp + IRQ_PROP_OFFSET_TO_INTID)) + SPI_OFFSET;
    IortNode->EventInterrupt = InterruptId;
    IortNode->PriInterrupt   = InterruptId;
    IortNode->GerrInterrupt  = InterruptId;
    IortNode->SyncInterrupt  = InterruptId;
  } else if ((IrqPropCnt <= MAX_NUM_IRQS_OF_SMMU_V3) && (IrqPropSize >= MIN_NUM_IRQS_OF_SMMU_V3)) {
    UINT32       IrqPropIndex;
    UINT32       StrSize;
    CONST CHAR8  *IrqPropNames[MAX_NUM_IRQS_OF_SMMU_V3] = {
      "eventq", "priq", "gerror", "cmdq-sync"
    };
    UINT32       *Interrupts = &IortNode->EventInterrupt;

    while (PropSize > 0) {
      StrSize = AsciiStrSize (Prop);
      if ((StrSize == 0) || (StrSize > (UINT32)PropSize)) {
        return EFI_NOT_FOUND;
      }

      for (IrqPropIndex = 0; IrqPropIndex < MAX_NUM_IRQS_OF_SMMU_V3; IrqPropIndex++) {
        if (0 == AsciiStrnCmp (Prop, IrqPropNames[IrqPropIndex], StrSize)) {
          break;
        }
      }

      InterruptId              = SwapBytes32 (*(IrqProp + IRQ_PROP_OFFSET_TO_INTID)) + SPI_OFFSET;
      Interrupts[IrqPropIndex] = InterruptId;

      PropSize -= StrSize;
      Prop     += StrSize;
      IrqProp  += IRQ_PROP_CELL_SIZE;
    }
  } else {
    DEBUG ((EFI_D_VERBOSE, "%a: Failed to find interrupts\r\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  // Map SMMU base address in MMU to support SBSA-ACS
  Status = AddIortMemoryRegion (IortNode->BaseAddress, SIZE_4KB);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return SetupIortIdMappingForSmmuV3 (Private, PropNode);
}

/**
  Populate data of PCI RC and ID mapping nodes defining SMMU and MSI setup.
  Mapping PCI nodes to SMMUv3 from the device tree.
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
  IN OUT  IORT_PRIVATE_DATA  *Private,
  IN OUT  IORT_PROP_NODE     *PropNode
  )
{
  CM_ARM_ROOT_COMPLEX_NODE  *IortNode;
  CM_ARM_ID_MAPPING         *IdMapping;
  CONST UINT32              *Prop;

  IortNode = PropNode->IortNode;
  if (IortNode->Token != 0) {
    return EFI_SUCCESS;
  }

  IortNode->Token             = (CM_OBJECT_TOKEN)(VOID *)IortNode;
  IortNode->AllocationHints   = 0;
  IortNode->MemoryAccessFlags = 0;
  IortNode->MemoryAddressSize = GetAddressLimit (Private, PropNode);
  IortNode->CacheCoherent     = 0;
  IortNode->IdMappingCount    = 1;
  IortNode->PciSegmentNumber  = 0;
  IortNode->Identifier        = UniqueIdentifier++;
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

  ASSERT (Private->IdMapIndex < Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes);
  PropNode->IdMapCount = 1;
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
    IdMapping->OutputReferenceToken = FindIortNodeByPhandle (Private, SwapBytes32 (Prop[0]));
    ASSERT (IdMapping->OutputReferenceToken != 0);
    IortNode->IdMappingCount = PropNode->IdMapCount;
    IortNode->IdMappingToken = (CM_OBJECT_TOKEN)(PropNode->IdMapArray);
  } else {
    Prop = (PropNode->IommuMapProp != NULL) ? PropNode->IommuMapProp : PropNode->MsiProp;
    ASSERT (Prop != NULL);

    // Create Id Mapping Node for iommu-map and bind it to the PCI IORT node
    IdMapping->InputBase            = SwapBytes32 (Prop[0]);
    IdMapping->OutputBase           = SwapBytes32 (Prop[2]);
    IdMapping->NumIds               = SwapBytes32 (Prop[3]) - 1;
    IdMapping->Flags                = 0;
    IdMapping->OutputReferenceToken = FindIortNodeByPhandle (Private, SwapBytes32 (Prop[1]));
    ASSERT (IdMapping->OutputReferenceToken != 0);
    IortNode->IdMappingCount = PropNode->IdMapCount;
    IortNode->IdMappingToken = (CM_OBJECT_TOKEN)(PropNode->IdMapArray);
  }

  return EFI_SUCCESS;
}

/**
  Populate data of Named Component and and ID mapping nodes defining SMMU and MSI setup.
  Mapping Named Component nodes to SMMUv3 from the device tree.
  Mapping SMMUV3 to ITS group nodes from the device tree.
  Install the IORT nodes of Named Component and ID mapping

  @param[in, out] Private   Pointer to the module private data

  @return EFI_SUCCESS       IORT nodes of Named Comp and ID mapping are populated and installed
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
SetupIortNodeForNComp (
  IN OUT  IORT_PRIVATE_DATA  *Private,
  IN OUT  IORT_PROP_NODE     *PropNode
  )
{
  CM_ARM_NAMED_COMPONENT_NODE  *IortNode;
  CM_ARM_ID_MAPPING            *IdMapping;
  CONST UINT32                 *Prop;

  IortNode = PropNode->IortNode;
  if (IortNode->Token != 0) {
    return EFI_SUCCESS;
  }

  IortNode->Token             = (CM_OBJECT_TOKEN)(VOID *)IortNode;
  IortNode->AllocationHints   = 0;
  IortNode->MemoryAccessFlags = 0;
  IortNode->Flags             = 0;
  IortNode->AddressSizeLimit  = GetAddressLimit (Private, PropNode);
  IortNode->CacheCoherent     = 0;
  IortNode->ObjectName        = PropNode->ObjectName;
  IortNode->IdMappingCount    = 1;
  IortNode->Identifier        = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  if (fdt_get_property (Private->DtbBase, PropNode->NodeOffset, "dma-coherent", NULL) != NULL) {
    IortNode->CacheCoherent     |= EFI_ACPI_IORT_MEM_ACCESS_PROP_CCA;
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_CPM;
    IortNode->MemoryAccessFlags |= EFI_ACPI_IORT_MEM_ACCESS_FLAGS_DACS;
  }

  ASSERT (Private->IdMapIndex < Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes);
  PropNode->IdMapCount = 1;
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
    IdMapping->OutputReferenceToken = FindIortNodeByPhandle (Private, SwapBytes32 (Prop[0]));
    ASSERT (IdMapping->OutputReferenceToken != 0);
    IortNode->IdMappingCount = PropNode->IdMapCount;
    IortNode->IdMappingToken = (CM_OBJECT_TOKEN)(PropNode->IdMapArray);
  } else {
    Prop = (PropNode->IommuMapProp != NULL) ? PropNode->IommuMapProp : PropNode->MsiProp;
    ASSERT (Prop != NULL);

    // Create Id Mapping Node for iommu-map and bind it to the Named Component node
    IdMapping->InputBase            = SwapBytes32 (Prop[0]);
    IdMapping->OutputBase           = SwapBytes32 (Prop[2]);
    IdMapping->NumIds               = SwapBytes32 (Prop[3]) - 1;
    IdMapping->Flags                = 0;
    IdMapping->OutputReferenceToken = FindIortNodeByPhandle (Private, SwapBytes32 (Prop[1]));
    ASSERT (IdMapping->OutputReferenceToken != 0);
    IortNode->IdMappingCount = PropNode->IdMapCount;
    IortNode->IdMappingToken = (CM_OBJECT_TOKEN)(PropNode->IdMapArray);
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
  IN OUT  IORT_PRIVATE_DATA  *Private,
  IN OUT  IORT_PROP_NODE     *PropNode
  )
{
  CM_ARM_PMCG_NODE     *IortNode;
  CM_ARM_ID_MAPPING    *IdMapping;
  CM_OBJECT_TOKEN      Token;
  CONST UINT64         *RegProp;
  CONST UINT32         *Prop;
  CONST UINT32         *IrqProp;
  INT32                PropSize;
  INT32                RegPropSize;
  INT32                IrqPropSize;
  UINT32               InterruptId;
  TEGRA_PLATFORM_TYPE  PlatformType;

  PlatformType = TegraGetPlatform ();
  if (PlatformType != TEGRA_PLATFORM_SILICON) {
    return EFI_SUCCESS;
  }

  IortNode = PropNode->IortNode;
  if (IortNode->Token != 0) {
    return EFI_SUCCESS;
  }

  IortNode->Token = (CM_OBJECT_TOKEN)(VOID *)IortNode;
  if (PropNode->RegProp != NULL) {
    IortNode->BaseAddress = SwapBytes64 (*PropNode->RegProp);
  }

  RegProp = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "reg", &RegPropSize);
  if ((RegProp != NULL) && ((RegPropSize / REG_PROP_LENGTH) > 1)) {
    IortNode->Page1BaseAddress = SwapBytes64 (*(RegProp + REG_PROP_CELL_SIZE));
  }

  IrqProp = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "interrupts", &IrqPropSize);
  if ((IrqProp == NULL) || (IrqPropSize == 0)) {
    IortNode->IdMappingCount = 1;
  } else {
    IortNode->IdMappingCount    = 0;
    InterruptId                 = SwapBytes32 (*(IrqProp + IRQ_PROP_OFFSET_TO_INTID)) + SPI_OFFSET;
    IortNode->OverflowInterrupt = InterruptId;
  }

  Prop = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "devices", &PropSize);
  if (Prop == NULL) {
    DEBUG ((EFI_D_VERBOSE, "%a: Failed to find \"devices\"\r\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  Token                    = FindIortNodeByPhandle (Private, SwapBytes32 (*Prop));
  IortNode->ReferenceToken = Token;

  IortNode->Identifier = UniqueIdentifier++;
  ASSERT (UniqueIdentifier < 0xFFFFFFFF);

  if (IortNode->IdMappingCount == 1) {
    ASSERT (Private->IdMapIndex < Private->IoNodes[IDMAP_TYPE_INDEX].NumberOfNodes);
    PropNode->IdMapCount = 1;
    IdMapping            = Private->IoNodes[IDMAP_TYPE_INDEX].NodeArray;
    IdMapping           += Private->IdMapIndex;
    Private->IdMapIndex += PropNode->IdMapCount;
    PropNode->IdMapArray = IdMapping;

    Prop = fdt_getprop (Private->DtbBase, PropNode->NodeOffset, "msi-parent", &PropSize);
    ASSERT (Prop != NULL);

    IdMapping->InputBase            = 0;
    IdMapping->OutputBase           = SwapBytes32 (Prop[1]);
    IdMapping->NumIds               = 0;
    IdMapping->Flags                = EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE;
    IdMapping->OutputReferenceToken = FindIortNodeByPhandle (Private, SwapBytes32 (Prop[0]));
    ASSERT (IdMapping->OutputReferenceToken != 0);
    IortNode->IdMappingCount = PropNode->IdMapCount;
    IortNode->IdMappingToken = (CM_OBJECT_TOKEN)(PropNode->IdMapArray);
  }

  return EFI_SUCCESS;
}

// The order must be ITS, SMMUv3, RootComplex and NamedComponent
STATIC CONST IORT_DEVICE_NODE_MAP  mIortDevTypeMap[] = {
  { EArmObjItsGroup,       "arm,gic-v3-its",        SetupIortNodeForItsGroup, NULL,            NULL          },
  { EArmObjSmmuV3,         "arm,smmu-v3",           SetupIortNodeForSmmuV3,   NULL,            NULL          },
  { EArmObjRootComplex,    "nvidia,th500-pcie",     SetupIortNodeForPciRc,    NULL,            NULL          },
  { EArmObjNamedComponent, "nvidia,tegra186-qspi",  SetupIortNodeForNComp,    "socket0_qspi1", "\\_SB_.QSP1" },
  { EArmObjNamedComponent, "nvidia,th500-soc-hwpm", SetupIortNodeForNComp,    NULL,            "\\_SB_.HWP0" },
  { EArmObjNamedComponent, "nvidia,th500-psc",      SetupIortNodeForNComp,    NULL,            "\\_SB_.PSC0" },
  { EArmObjPmcg,           "arm,smmu-v3-pmcg",      SetupIortNodeForPmcg,     NULL,            NULL          },
  { EArmObjMax,            NULL,                    NULL,                     NULL,            NULL          }
};

EFI_STATUS
InitializeIoRemappingNodes (
  VOID
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
    DEBUG ((EFI_D_ERROR, "%a failed to get device tree: %r\r\n", __FUNCTION__, Status));
    return Status;
  }

  // Scan the IORT property nodes in the device tree and add them in the list
  Status = AddIortPropNodes (Private, DevMap);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  // Allocate space for the IORT nodes
  Status = AllocateIortNodes (Private);
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

      Status = DevMap->SetupIortNode (Private, PropNode);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "Failed to setup IORT ObjectId=%d err=%r\r\n", PropNode->ObjectId, Status));
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
InstallIoRemappingTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CM_STD_OBJ_ACPI_TABLE_INFO      *NewAcpiTables;
  IORT_NODE                       *IoNode;
  LIST_ENTRY                      *ListEntry;
  IORT_PROP_NODE                  *PropNode;
  IORT_PRIVATE_DATA               *Private;
  UINTN                           Index;
  TEGRA_PLATFORM_TYPE             PlatformType;

  Private      = &mIortPrivate;
  PlatformType = TegraGetPlatform ();

  // Create a ACPI Table Entry
  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (
                                                      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize +
                                                      (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)),
                                                      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr
                                                      );

      if (NewAcpiTables == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_IO_REMAPPING_TABLE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_IO_REMAPPING_TABLE_REVISION_06;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdIort);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = NULL;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  Repo = *PlatformRepositoryInfo;

  for (Index = 0; Index < MAX_NUMBER_OF_IORT_TYPE; Index++) {
    if ((Index == IORT_TYPE_INDEX (EArmObjPmcg)) &&
        (PlatformType != TEGRA_PLATFORM_SILICON))
    {
      continue;
    }

    IoNode = &Private->IoNodes[Index];
    if ((IoNode->NumberOfNodes != 0) && (Index != IDMAP_TYPE_INDEX)) {
      Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (Index + MIN_IORT_OBJID);
      Repo->CmObjectToken = CM_NULL_TOKEN;
      Repo->CmObjectSize  = IoNode->NumberOfNodes * IoNode->SizeOfNode;
      Repo->CmObjectCount = IoNode->NumberOfNodes;
      Repo->CmObjectPtr   = IoNode->NodeArray;
      Repo++;
      DEBUG ((EFI_D_INFO, "%a: Installed IORT %d\r\n", __FUNCTION__, Index + MIN_IORT_OBJID));
    }
  }

  ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);

  for_each_list_entry (ListEntry, &Private->PropNodeList) {
    PropNode = IORT_PROP_NODE_FROM_LINK (ListEntry);
    if ((PropNode->IdMapArray != NULL) && (PropNode->IdMapCount != 0)) {
      Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjIdMappingArray);
      Repo->CmObjectToken = (CM_OBJECT_TOKEN)PropNode->IdMapArray;
      Repo->CmObjectSize  = PropNode->IdMapCount * sizeof (CM_ARM_ID_MAPPING);
      Repo->CmObjectCount = PropNode->IdMapCount;
      Repo->CmObjectPtr   = PropNode->IdMapArray;
      Repo++;
      ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);
    }
  }
  DEBUG ((EFI_D_INFO, "%a: Installed IORT\r\n", __FUNCTION__));

  *PlatformRepositoryInfo = Repo;

  return EFI_SUCCESS;
}
