/** @file

  Configuration Manager Data Driver private structures of IO Remapping Table

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONFIGURATION_IORT_PRIVATE_H__
#define __CONFIGURATION_IORT_PRIVATE_H__

#include <IndustryStandard/IoRemappingTable.h>

#define IORT_TABLE_GEN  L"IortTableGen"

#define MIN_NUM_IRQS_OF_SMMU_V3  2
#define MAX_NUM_IRQS_OF_SMMU_V3  4

#define IOMMUMAP_PROP_LENGTH  (4 * sizeof (UINT32))
#define IOMMUS_PROP_LENGTH    (2 * sizeof (UINT32))
#define MSIMAP_PROP_LENGTH    (4 * sizeof (UINT32))
#define DMARANGE_PROP_LENGTH  (3 * sizeof (UINT64) + sizeof (UINT32))

#define TRANSLATE_BASE_ADDR_TO_ID(a)  ((a>>32) & 0x0000F000)

/**
 * Valid Arm Object IDs and its structure name of IO Remapping:
 *  EArmObjItsGroup,                  CM_ARM_ITS_GROUP_NODE         ///< 18 - ITS Group
 *  EArmObjNamedComponent,            CM_ARM_NAMED_COMPONENT_NODE   ///< 19 - Named Component
 *  EArmObjRootComplex,               CM_ARM_ROOT_COMPLEX_NODE      ///< 20 - Root Complex
 *  EArmObjSmmuV1SmmuV2,              CM_ARM_SMMUV1_SMMUV2_NODE     ///< 21 - SMMUv1 or SMMUv2
 *  EArmObjSmmuV3,                    CM_ARM_SMMUV3_NODE            ///< 22 - SMMUv3
 *  EArmObjPmcg,                      CM_ARM_PMCG_NODE              ///< 23 - PMCG
 *  EArmObjGicItsIdentifierArray,     CM_ARM_ITS_IDENTIFIER         ///< 24 - GIC ITS Identifier Array
 *  EArmObjIdMappingArray,            CM_ARM_ID_MAPPING             ///< 25 - ID Mapping Array
 *  EArmObjSmmuInterruptArray,        CM_ARM_SMMU_INTERRUPT         ///< 26 - SMMU Interrupt Array
 */
#define MIN_IORT_OBJID  (EArmObjItsGroup)
#define MAX_IORT_OBJID  (EArmObjSmmuInterruptArray)
#define IORT_TYPE_INDEX(a)  (a - MIN_IORT_OBJID)
#define IDMAP_TYPE_INDEX         IORT_TYPE_INDEX(EArmObjIdMappingArray)
#define ITSIDENT_TYPE_INDEX      IORT_TYPE_INDEX(EArmObjGicItsIdentifierArray)
#define MAX_NUMBER_OF_IORT_TYPE  (MAX_IORT_OBJID - MIN_IORT_OBJID + 1)

typedef struct {
  CONST UINT32       SizeOfNode;
  UINT32             NumberOfNodes;
  VOID               *NodeArray;
  CM_OBJECT_TOKEN    *TokenArray;
} IORT_NODE;

#define IORT_PROP_NODE_SIGNATURE  SIGNATURE_32('I','O','P','N')
typedef struct {
  UINT32                                    Signature;
  INT32                                     NodeOffset;
  UINT32                                    Phandle;
  EARM_OBJECT_ID                            ObjectId;
  CM_OBJECT_TOKEN                           Token;
  UINT32                                    DualSmmuPresent;
  CONST UINT32                              *IommusProp;   // Pointer to DTB, or NULL
  CONST UINT32                              *IommuMapProp; // Pointer to DTB, or NULL
  CONST UINT32                              *MsiProp;      // Pointer to DTB, or NULL
  UINT32                                    RegCount;
  CONST NVIDIA_DEVICE_TREE_REGISTER_DATA    *RegArray; // Allocated Array
  VOID                                      *IortNode; // Pointer to a spot within Private->IoNodes[Type].NodeArray
  UINT32                                    ContextInterruptCnt;
  VOID                                      *ContextInterruptArray;
  UINT32                                    PmuInterruptCnt;
  VOID                                      *PmuInterruptArray;
  UINT32                                    IdMapCount;
  VOID                                      *IdMapArray; // Pointer to a spot within Private->IoNodes[IdMap].NodeArray
  LIST_ENTRY                                Link;
  CHAR8                                     *ObjectName;
} IORT_PROP_NODE;
#define IORT_PROP_NODE_FROM_LINK(a) \
        CR(a, IORT_PROP_NODE, Link, IORT_PROP_NODE_SIGNATURE)

#define IORT_DATA_SIGNATURE  SIGNATURE_64('I','O','R','E','M','A','P','T')
typedef struct {
  UINT64        Signature;
  VOID          *DtbBase;
  UINTN         DtbSize;
  UINT32        IdMapIndex;
  UINT32        ItsIdentifierIndex;
  LIST_ENTRY    PropNodeList;
  IORT_NODE     IoNodes[MAX_NUMBER_OF_IORT_TYPE];
} IORT_PRIVATE_DATA;

/**
  SetupIortNode() parses DTB node and updates feilds in
  the corresponding CM object.

  @param[in, out] Private    Pointer to the module private data
  @param[in, out] PropNode   Pointer to the the DTB node

  @return EFI_SUCCESS        Successfully parsed DTB node
  @retval !(EFI_SUCCESS)     Other errors

**/
typedef
EFI_STATUS
(*SetupIortNodeFunc) (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  OUT  IORT_PRIVATE_DATA       *Private,
  IN  OUT  IORT_PROP_NODE          *PropNode
  );

/**
 * Arm Object IDs of the system valid for IORT_DEVICE_NODE_MAP:
 *  - Present: EArmObjNamedComponent, EArmObjRootComplex, EArmObjSmmuV3
 *  - Not sure: EArmObjPmcg
 */
typedef struct {
  EARM_OBJECT_ID       ObjectId;
  CONST CHAR8          *Compatibility;
  SetupIortNodeFunc    SetupIortNode;
  CONST CHAR8          *Alias;
  CHAR8                *ObjectName;
  UINT32               DualSmmuPresent;
} IORT_DEVICE_NODE_MAP;

#endif
