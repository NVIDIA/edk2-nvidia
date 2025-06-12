/** @file
  Nvidia's Configuration manager Object Descriptor Utility.

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2021, ARM Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef NV_CM_OBJECT_DESC_UTILITY_H_
#define NV_CM_OBJECT_DESC_UTILITY_H_

#include <ConfigurationManagerObject.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/HwInfoParserLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/AmlPatchProtocol.h>
#include <Protocol/AmlGenerationProtocol.h>

#define FREE_NON_NULL(a) \
  if ((a) != NULL) { \
    FreePool ((a));  \
    (a) = NULL;      \
  }

typedef struct CacheInfoNode {
  NVIDIA_DEVICE_TREE_CACHE_DATA    CacheData;
  CM_OBJECT_TOKEN                  Token;
  UINT32                           Socket;
  UINT32                           Cluster;
  UINT32                           Core;
  BOOLEAN                          IsPrivateRoot;
} CACHE_NODE;

/** Create a CM_OBJ_DESCRIPTOR.

  NOTE: This behaves different from ARM's CreateCmObjDesc!

  @param [in]  ObjectId       CM_OBJECT_ID of the node.
  @param [in]  Count          Number of CmObj stored in the
                              data field.
  @param [in]  Data           Pointer to one or more CmObj objects.
                              The pointer is used AS IS - DATA IS NOT COPIED
  @param [in]  Size           Size of the Data buffer.
  @param [out] NewCmObjDesc   The created CM_OBJ_DESCRIPTOR. The caller is
                              responsible for freeing this when done.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_OUT_OF_RESOURCES    An allocation has failed.
**/
EFI_STATUS
EFIAPI
NvCreateCmObjDesc (
  IN  CM_OBJECT_ID       ObjectId,
  IN  UINT32             Count,
  IN  VOID               *Data,
  IN  UINT32             Size,
  OUT CM_OBJ_DESCRIPTOR  **NewCmObjDesc
  );

/** Free resources allocated for the CM_OBJ_DESCRIPTOR.

  NOTE: Unlike the ARM version, this doesn't free the Data pointer!

  @param [in] CmObjDesc           Pointer to the CM_OBJ_DESCRIPTOR.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvFreeCmObjDesc (
  IN CM_OBJ_DESCRIPTOR  *CmObjDesc
  );

/** Add a single CmObj to the Configuration Manager.

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [in]  ObjectId          CmObj ObjectId.
  @param  [in]  Data              CmObj Data.
  @param  [in]  Size              CmObj Size.
  @param  [out] Token             If provided and success,
                                  token generated for this CmObj.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvAddSingleCmObj (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        CM_OBJECT_ID           ObjectId,
  IN        VOID                   *Data,
  IN        UINT32                 Size,
  OUT       CM_OBJECT_TOKEN        *Token    OPTIONAL
  );

/** Add multiple CmObj to the Configuration Manager
 * producing an ElementTokenMap for the objects.

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [in]  CmObjDesc         CmObjDesc containing multiple CmObj
                                  to add.
  @param  [out] TokenMapPtr       If provided and successful, the pointer
                                  populated with the address of the generated
                                  ElementTokenMap for the added objects.
  @param  [out] TokenPtr          If provided and successful, where to put
                                  the Token for the whole object.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvAddMultipleCmObjGetTokens (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  OUT       CM_OBJECT_TOKEN        **TokenMapPtr   OPTIONAL,
  OUT       CM_OBJECT_TOKEN        *TokenPtr OPTIONAL
  );

/** Add multiple CmObj to the Configuration Manager using a provided
 * TokenMap for them.

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [in]  CmObjDesc         CmObjDesc containing multiple CmObj
                                  to add.
  @param  [in]  ElementTokenMap   The ElementTokenMap for the objects.
  @param  [in]  Token             Token to use for the whole object. If CM_NULL_TOKEN,
                                  a token will be generated.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvAddMultipleCmObjWithTokens (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  IN        CM_OBJECT_TOKEN        *ElementTokenMap,
  IN        CM_OBJECT_TOKEN        Token
  );

/** Add multiple CmObj to the Configuration Manager.

  Get one token referencing a EArchCommonObjCmRef CmObj itself referencing
  the input CmObj. In the table below, RefToken is returned. Use the
  provided ElementTokenMap as the tokens for the objects, if not NULL.

  Token referencing an      Array of tokens             Array of CmObj
  array of EArchCommonObjCmRef     referencing each            from the input:
  CmObj:                    CmObj from the input:

  RefToken         --->     CmObjToken[0]        --->   CmObj[0]
                            CmObjToken[1]        --->   CmObj[1]
                            CmObjToken[2]        --->   CmObj[2]

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [in]  CmObjDesc         CmObjDesc containing multiple CmObj
                                  to add.
  @param  [in]  ElementTokenMap   The ElementTokenMap for the objects, or NULL
  @param  [out] Token             If success, token referencing an array
                                  of EArchCommonObjCmRef CmObj, themselves
                                  referencing the input CmObjs.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_OUT_OF_RESOURCES    An allocation has failed.
**/
EFI_STATUS
EFIAPI
NvAddMultipleCmObjWithCmObjRef (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CM_OBJ_DESCRIPTOR            *CmObjDesc,
  IN  CM_OBJECT_TOKEN              *ElementTokenMap OPTIONAL,
  OUT CM_OBJECT_TOKEN              *Token
  );

/** Allocate tokens for future CmObjs

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [in]  Count             Number of tokens to allocate.
  @param  [out] TokenMapPtr       If successfull, the pointer to the
                                  token array that was allocated. The
                                  user is responsible for freeing the
                                  map after it is done being used by
                                  user code.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvAllocateCmTokens (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        UINT32                 Count,
  OUT       CM_OBJECT_TOKEN        **TokenMapPtr
  );

/** Extend a CmObj in the Configuration Manager

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [in]  CmObjDesc         CmObjDesc describing the added entries.
  @param  [in]  Token             Token of the CmObj to extend.
  @param  [out] TokenMapPtr       If provided and successful, the pointer
                                  populated with the address of the generated
                                  ElementTokenMap for the added objects.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvExtendCmObj (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  IN        CM_OBJECT_TOKEN        Token    OPTIONAL,
  OUT       CM_OBJECT_TOKEN        **TokenMapPtr   OPTIONAL
  );

/** Get the GenerationProtocol used by the parser

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [out] ProtocolPtr       The pointer to populate with the Protocol pointer

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvGetCmGenerationProtocol (
  IN  CONST HW_INFO_PARSER_HANDLE           ParserHandle,
  OUT       NVIDIA_AML_GENERATION_PROTOCOL  **ProtocolPtr
  );

/** Get the PatchProtocol used by the parser

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [out] ProtocolPtr       The pointer to populate with the Protocol pointer

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvGetCmPatchProtocol (
  IN  CONST HW_INFO_PARSER_HANDLE      ParserHandle,
  OUT       NVIDIA_AML_PATCH_PROTOCOL  **ProtocolPtr
  );

/** Find an object in the configuration manager

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [in]  ObjectId          The ID of the object to find.
  @param  [in]  Token             The token of the object to find, or CM_NULL_TOKEN.
  @param  [out] DescPtr           Pointer to where to put the Object Descriptor pointer.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvFindEntry (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        CM_OBJECT_ID           ObjectId,
  IN        CM_OBJECT_TOKEN        Token,
  OUT       CM_OBJ_DESCRIPTOR      **DescPtr
  );

/** Conditionally add an ACPI table Generator to the list

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  NewGenerator    The new Generator to add if not already present.

  @retval EFI_SUCCESS             The table was added, or an existing version
                                  was found to be already present.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
NvAddAcpiTableGenerator (
  IN  CONST HW_INFO_PARSER_HANDLE       ParserHandle,
  IN        CM_STD_OBJ_ACPI_TABLE_INFO  *NewGenerator
  );

/** Find the cache metadata in the configuration manager based on pHandle

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [in]  pHandle           The pHandle of the cache node to find.
  @param  [in]  ICache            If the pHandle is for a "cpu" node, setting this
                                  to TRUE will return the ICache info and FALSE will
                                  return the DCache info. Otherwise, this is ignored.
  @param  [out] CacheNode         Pointer to where to put the CacheNode information.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Couldn't find a CacheNode with the given pHandle.
**/
EFI_STATUS
EFIAPI
NvFindCacheMetadataByPhandle (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        UINT32                 pHandle,
  IN        BOOLEAN                ICache,
  OUT CONST CACHE_NODE             **CacheNode
  );

/** Find the CacheId in the configuration manager based on pHandle

@param  [in]  ParserHandle      A handle to the parser instance.
@param  [in]  pHandle           The pHandle of the cache node to find.
@param  [in]  ICache            If the pHandle is for a "cpu" node, setting this
                                to TRUE will return the ICache Id and FALSE will
                                return the DCache Id. Otherwise, this is ignored.
@param  [out] CacheId           Pointer to where to put the CacheId.

@retval EFI_SUCCESS             The function completed successfully.
@retval EFI_INVALID_PARAMETER   Invalid parameter.
@retval EFI_NOT_FOUND           Couldn't find a CacheId for the given pHandle.
**/
EFI_STATUS
EFIAPI
NvFindCacheIdByPhandle (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        UINT32                 pHandle,
  IN        BOOLEAN                ICache,
  OUT       UINT32                 *CacheId
  );

#endif // NV_CM_OBJECT_DESC_UTILITY_H_
