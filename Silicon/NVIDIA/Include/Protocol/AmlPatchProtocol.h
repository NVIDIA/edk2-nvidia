/** @file
*
*  AML patching protocol definition.
*
*  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __AML_OFFSET_TABLE_H
#define __AML_OFFSET_TABLE_H

// Intended to match the offset table entry struct from the generated offset
// table file, which is why C types are used instead of UEFI types.
typedef struct {
  char                  *Pathname;         /* Full pathname (from root) to the object */
  unsigned short        ParentOpcode;      /* AML opcode for the parent object */
  unsigned long         NamesegOffset;     /* Offset of last nameseg in the parent namepath */
  unsigned char         Opcode;            /* AML opcode for the data */
  unsigned long         Offset;            /* Offset for the data */
  unsigned long long    Value;             /* Original value of the data (as applicable) */
} AML_OFFSET_TABLE_ENTRY;

#endif /* __AML_OFFSET_TABLE_H */

#ifndef __AML_PATCH_PROTOCOL_H__
#define __AML_PATCH_PROTOCOL_H__

  #include <Uefi/UefiBaseType.h>
  #include <Library/BaseLib.h>
  #include <IndustryStandard/Acpi10.h>

//
// Define for forward reference.
//
typedef struct _NVIDIA_AML_PATCH_PROTOCOL NVIDIA_AML_PATCH_PROTOCOL;

typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER    *AmlTable;       /* Pointer to start of AML Table w/node */
  AML_OFFSET_TABLE_ENTRY         *AmlOffsetEntry; /* Pointer to offset table entry for node*/
  UINTN                          Size;            /* Size of AML node */
} NVIDIA_AML_NODE_INFO;

/**
  Register an array of AML Tables and their corresponding offset tables. These
  are the arrays that will be used to find, verify, and update nodes by the
  rest of the patching interface. The given arrays should match in length, and
  an AML table at a particular index should have its offset table at the matching
  index in the offset table array.

  @param[in]  This              Instance of AML patching protocol.
  @param[in]  AmlTables         Array of pointers to AML tables to be used by the
                                interface. Iteration through the AML tables will
                                be done in the order they are given.
  @param[in]  OffsetTables      Array of pointers to AML offset tables to be used
                                by the interface. An offset table should have the
                                same index as its corresponding AML table does
                                in the given AmlTables array.
  @param[in]  NumTables         The number of AML tables in the given array.
                                Must be nonzero. The array of AML tables and
                                the array of offset tables should each have
                                a length of NumTables.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate enough memory to
                                store the given arrays.
  @retval EFI_INVALID_PARAMETER One of the given arrays was NULL,
                                or the given NumTables was 0.
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AML_PATCH_REGISTER_TABLES)(
  IN NVIDIA_AML_PATCH_PROTOCOL    *This,
  IN EFI_ACPI_DESCRIPTION_HEADER  **AmlTables,
  IN AML_OFFSET_TABLE_ENTRY       **OffsetTables,
  IN UINTN                        NumTables
  );

/**
  Find the AML Node for the given path name. Will use the registered AML Tables
  and offset tables to search for a node with the given path name.

  @param[in]  This              Instance of AML patching protocol.
  @param[in]  PathName          Ascii string containing a the path name for the
                                desired node. The path name must appear in one
                                of the registered offset tables.
  @param[out] AmlNodeInfo       Pointer to an NVIDIA_AML_NODE_INFO struct that will be
                                populated with the AML table, offset entry, and
                                size of the node if found.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_FOUND         Could not find the given PathName in any of the
                                registered offset tables.
  @retval EFI_NOT_READY         AML and offset tables have not been registered yet.
  @retval EFI_INVALID_PARAMETER This, PathName, or AmlNodeInfo was NULL
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AML_PATCH_FIND_NODE)(
  IN NVIDIA_AML_PATCH_PROTOCOL  *This,
  IN CHAR8                      *PathName,
  OUT NVIDIA_AML_NODE_INFO      *AmlNodeInfo
  );

/**
  Find the AML Node for the given path name. Will use the AML Tables and offset
  tables to search for a node with the given path name.

  @param[in]  This              Instance of AML patching protocol.
  @param[in]  AmlNodeInfo       Pointer to the AmlNodeInfo for the node whose
                                data will be retrieved.
  @param[out] Data              Pointer to a buffer that will be populated with
                                the data of the node.
  @param[in] Size               Size of the buffer. Should be greater than or
                                equal to the size of the AML node's data.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_UNSUPPORTED       The opcode of the AML node is not supported.
  @retval EFI_BUFFER_TOO_SMALL  The given data buffer is too small for the data
                                of the AML node.
  @retval EFI_INVALID_PARAMETER This, AmlNodeInfo, or Data was NULL,
                                or the given AmlNodeInfo opcode did not match
                                the data's opcode.
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AML_PATCH_GET_NODE_DATA)(
  IN NVIDIA_AML_PATCH_PROTOCOL  *This,
  IN NVIDIA_AML_NODE_INFO       *AmlNodeInfo,
  OUT VOID                      *Data,
  IN  UINTN                     Size
  );

/**
  Set the data of the AML Node with the given AmlNodeInfo.

  @param[in]  This              Instance of AML patching protocol.
  @param[in]  AmlNodeInfo       Pointer to the AmlNodeInfo for the node whose
                                data will be set.
  @param[in]  Data              Pointer to a buffer with the data to set the
                                node's data to. The opcode and any internal length
                                fields must match what is already present for
                                the node in both the AML and offset tables.
  @param[in]  Size              The number of bytes that should be set for
                                the given node. This must match the current
                                size of the node in the AML table. Must be nonzero.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_BAD_BUFFER_SIZE   The given Size, the size in AmlNodeInfo, or the
                                found size do not all match.
  @retval EFI_UNSUPPORTED       The opcode of the aml node is not supported.
  @retval EFI_INVALID_PARAMETER This, AmlNodeInfo, or Data was NULL,
                                or the given AmlNodeInfo or the given data
                                did not match what was expected for the node.
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AML_PATCH_SET_NODE_DATA)(
  IN NVIDIA_AML_PATCH_PROTOCOL  *This,
  IN NVIDIA_AML_NODE_INFO       *AmlNodeInfo,
  IN VOID                       *Data,
  IN UINTN                      Size
  );

/**
  Update the name of the AML Node with the given AmlNodeInfo. The name is located
  using the NamesegOffset of the AML offset entry. Does not update the
  name stored in the AML offset entry.

  @param[in]  This              Instance of AML patching protocol.
  @param[in]  AmlNodeInfo       Pointer to the AmlNodeInfo for the node whose
                                name will be updated.
  @param[in]  NewName           Pointer to a buffer with the new name. Must have
                                a length between 1 and 4. If the length is < 4,
                                then the name will be padded to 4 bytes using
                                the '_' character. The first character in the
                                given name must be inclusive of 'A'-'Z' and '_'.
                                The rest of the characters must be inclusive of
                                'A'-'Z', '0'-'9', and '_'.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_BAD_BUFFER_SIZE   The given NewName's length was not between 1-4
  @retval EFI_INVALID_PARAMETER This, AmlNodeInfo, or NewName was NULL,
                                or NewName's characters do not follow the
                                AML spec guidelines.
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AML_PATCH_UPDATE_NODE_NAME)(
  IN NVIDIA_AML_PATCH_PROTOCOL  *This,
  IN NVIDIA_AML_NODE_INFO       *AmlNodeInfo,
  IN CHAR8                      *NewName
  );

// NVIDIA_AML_PATCH_PROTOCOL protocol structure.
struct _NVIDIA_AML_PATCH_PROTOCOL {
  NVIDIA_AML_PATCH_REGISTER_TABLES     RegisterAmlTables;
  NVIDIA_AML_PATCH_FIND_NODE           FindNode;
  NVIDIA_AML_PATCH_GET_NODE_DATA       GetNodeData;
  NVIDIA_AML_PATCH_SET_NODE_DATA       SetNodeData;
  NVIDIA_AML_PATCH_UPDATE_NODE_NAME    UpdateNodeName;
};

#endif /* __AML_PATCH_PROTOCOL_H__ */
