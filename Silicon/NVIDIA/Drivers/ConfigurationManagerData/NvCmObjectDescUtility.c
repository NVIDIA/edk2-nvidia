/** @file
  Nvidia's Configuration manager Object Descriptor Utility.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2021, ARM Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <ConfigurationManagerObject.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <ArmNameSpaceObjects.h>
#include <Library/NVIDIADebugLib.h>

#include "NvCmObjectDescUtility.h"
#include "NvCmObjectDescUtilityPrivate.h"

/** Create a CM_OBJ_DESCRIPTOR.

  @param [in]  ObjectId       CM_OBJECT_ID of the node.
  @param [in]  Count          Number of CmObj stored in the
                              data field.
  @param [in]  Data           Pointer to one or more CmObj objects.
                              The content of this Data buffer is copied.
  @param [in]  Size           Size of the Data buffer.
  @param [out] NewCmObjDesc   The created CM_OBJ_DESCRIPTOR.

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
  )
{
  EFI_STATUS         Status;
  CM_OBJ_DESCRIPTOR  *CmObjDesc;
  VOID               *DataBuffer;

  CmObjDesc  = NULL;
  DataBuffer = NULL;

  NV_ASSERT_RETURN (Count > 0, return EFI_INVALID_PARAMETER, "%a: Count can't be 0\n", __FUNCTION__);
  NV_ASSERT_RETURN (NewCmObjDesc != NULL, return EFI_INVALID_PARAMETER, "%a: NewCmObjDesc pointer can't be NULL\n", __FUNCTION__);

  CmObjDesc = AllocateZeroPool (sizeof (CM_OBJ_DESCRIPTOR));
  NV_ASSERT_RETURN (CmObjDesc != NULL, return EFI_OUT_OF_RESOURCES, "%a: Unable to allocate space for a CM_OBJ_DESCRIPTOR\n", __FUNCTION__);

  if (Size > 0) {
    DataBuffer = AllocateCopyPool (Size, Data);
    NV_ASSERT_RETURN (
      DataBuffer != NULL,
      { Status = EFI_OUT_OF_RESOURCES;
        goto CleanupAndReturn;
      },
      "%a: Unable to allocate %u bytes to add the data to the descriptor\n",
      __FUNCTION__,
      Size
      );
  }

  CmObjDesc->ObjectId = ObjectId;
  CmObjDesc->Count    = Count;
  CmObjDesc->Data     = DataBuffer;
  CmObjDesc->Size     = Size;

  *NewCmObjDesc = CmObjDesc;
  Status        = EFI_SUCCESS;

CleanupAndReturn:
  if (EFI_ERROR (Status)) {
    FREE_NON_NULL (CmObjDesc);
    FREE_NON_NULL (DataBuffer);
  }

  return Status;
}

/** Free resources allocated for the CM_OBJ_DESCRIPTOR.

  @param [in] CmObjDesc           Pointer to the CM_OBJ_DESCRIPTOR.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvFreeCmObjDesc (
  IN CM_OBJ_DESCRIPTOR  *CmObjDesc
  )
{
  NV_ASSERT_RETURN (CmObjDesc != NULL, return EFI_INVALID_PARAMETER, "%a: CmObjDesc can't be NULL\n", __FUNCTION__);

  if (CmObjDesc->Data != NULL) {
    FreePool (CmObjDesc->Data);
  }

  FreePool (CmObjDesc);
  return EFI_SUCCESS;
}

/** Add a single CmObj to the Configuration Manager.

  @param  [in]  ParserHandle   A handle to the parser instance.
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
  )
{
  EFI_STATUS                 Status;
  CM_OBJ_DESCRIPTOR          CmObjDesc;
  FDT_HW_INFO_PARSER_HANDLE  FdtParserHandle;

  FdtParserHandle = (FDT_HW_INFO_PARSER_HANDLE)ParserHandle;

  NV_ASSERT_RETURN (FdtParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: FdtParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (FdtParserHandle->HwInfoAdd != NULL, return EFI_INVALID_PARAMETER, "%a: FdtParserHandle->HwInfoAdd pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN ((Data != NULL) || (Size == 0), return EFI_INVALID_PARAMETER, "%a: Data is NULL while Size is not\n", __FUNCTION__);

  CmObjDesc.ObjectId = ObjectId;
  // Special case EArmObjCmRef, since it is a list of CM references
  // that has multi-count, but should only have a single Token
  if (ObjectId == CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef)) {
    CmObjDesc.Count = Size/sizeof (CM_ARM_OBJ_REF);
  } else {
    CmObjDesc.Count = 1;
  }

  CmObjDesc.Data = Data;
  CmObjDesc.Size = Size;

  // Add the CmObj.
  Status = FdtParserHandle->HwInfoAdd (
                              FdtParserHandle,
                              FdtParserHandle->Context,
                              &CmObjDesc,
                              Token
                              );
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  return Status;
}

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
  )
{
  EFI_STATUS                 Status;
  FDT_HW_INFO_PARSER_HANDLE  FdtParserHandle;

  FdtParserHandle = (FDT_HW_INFO_PARSER_HANDLE)ParserHandle;

  NV_ASSERT_RETURN (FdtParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: FdtParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (FdtParserHandle->HwInfoAdd != NULL, return EFI_INVALID_PARAMETER, "%a: FdtParserHandle->HwInfoAdd pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc != NULL, return EFI_INVALID_PARAMETER, "%a: CmObjDesc pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc->Count > 0, return EFI_INVALID_PARAMETER, "%a: CmObjDesc's count can't be 0\n", __FUNCTION__);
  NV_ASSERT_RETURN ((CmObjDesc->Data != NULL) || (CmObjDesc->Size == 0), return EFI_INVALID_PARAMETER, "%a: CmObjDesc's Data is NULL while Size is not\n", __FUNCTION__);

  // Add the multi-object array and let tokens be generated
  Status = NvHwInfoAddGetMap (FdtParserHandle, FdtParserHandle->Context, CmObjDesc, TokenMapPtr, TokenPtr);
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  return Status;
}

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
  )
{
  EFI_STATUS                 Status;
  FDT_HW_INFO_PARSER_HANDLE  FdtParserHandle;

  FdtParserHandle = (FDT_HW_INFO_PARSER_HANDLE)ParserHandle;

  NV_ASSERT_RETURN (FdtParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: FdtParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc != NULL, return EFI_INVALID_PARAMETER, "%a: CmObjDesc pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc->Count > 0, return EFI_INVALID_PARAMETER, "%a: CmObjDesc's count can't be 0\n", __FUNCTION__);
  NV_ASSERT_RETURN ((CmObjDesc->Data != NULL) || (CmObjDesc->Size == 0), return EFI_INVALID_PARAMETER, "%a: CmObjDesc's Data is NULL while Size is not\n", __FUNCTION__);
  NV_ASSERT_RETURN (ElementTokenMap != NULL, return EFI_INVALID_PARAMETER, "%a: ElementTokenMap pointer is NULL\n", __FUNCTION__);

  // Add the multi-object array with the provided tokens
  Status = NvHwInfoAddWithTokenMap (FdtParserHandle, FdtParserHandle->Context, CmObjDesc, ElementTokenMap, Token);
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  return Status;
}

/** Add multiple CmObj to the Configuration Manager.

  Get one token referencing a EArmObjCmRef CmObj itself referencing
  the input CmObj. In the table below, RefToken is returned. Use the
  provided ElementTokenMap as the tokens for the objects, if not NULL.

  Token referencing an      Array of tokens             Array of CmObj
  array of EArmObjCmRef     referencing each            from the input:
  CmObj:                    CmObj from the input:

  RefToken         --->     CmObjToken[0]        --->   CmObj[0]
                            CmObjToken[1]        --->   CmObj[1]
                            CmObjToken[2]        --->   CmObj[2]

  @param  [in]  ParserHandle      A handle to the parser instance.
  @param  [in]  CmObjDesc         CmObjDesc containing multiple CmObj
                                  to add.
  @param  [in]  ElementTokenMap   The ElementTokenMap for the objects, or NULL
  @param  [out] Token             If success, token referencing an array
                                  of EArmObjCmRef CmObj, themselves
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
  )
{
  EFI_STATUS                 Status;
  CM_OBJ_DESCRIPTOR          CmObjRef;
  FDT_HW_INFO_PARSER_HANDLE  FdtParserHandle;
  CM_OBJECT_TOKEN            *LocalElementTokenMap;

  FdtParserHandle = (FDT_HW_INFO_PARSER_HANDLE)ParserHandle;

  NV_ASSERT_RETURN (FdtParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: FdtParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (FdtParserHandle->HwInfoAdd != NULL, return EFI_INVALID_PARAMETER, "%a: FdtParserHandle->HwInfoAdd pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc != NULL, return EFI_INVALID_PARAMETER, "%a: CmObjDesc pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc->Count > 0, return EFI_INVALID_PARAMETER, "%a: CmObjDesc's count can't be 0\n", __FUNCTION__);
  NV_ASSERT_RETURN ((CmObjDesc->Data != NULL) || (CmObjDesc->Size == 0), return EFI_INVALID_PARAMETER, "%a: CmObjDesc's Data is NULL while Size is not\n", __FUNCTION__);
  NV_ASSERT_RETURN (Token != NULL, return EFI_INVALID_PARAMETER, "%a: Token pointer is NULL\n", __FUNCTION__);

  // Add the input CmObjs.
  if (ElementTokenMap != NULL) {
    Status               = NvHwInfoAddWithTokenMap (FdtParserHandle, FdtParserHandle->Context, CmObjDesc, ElementTokenMap, CM_NULL_TOKEN);
    LocalElementTokenMap = ElementTokenMap;
  } else {
    Status = NvAddMultipleCmObjGetTokens (
               FdtParserHandle,
               CmObjDesc,
               &LocalElementTokenMap,
               NULL
               );
  }

  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  CmObjRef.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  CmObjRef.Data     = LocalElementTokenMap;
  CmObjRef.Count    = CmObjDesc->Count;
  CmObjRef.Size     = CmObjDesc->Count * sizeof (CM_OBJECT_TOKEN);

  // Add the ElementTokenMap as an object
  Status = FdtParserHandle->HwInfoAdd (
                              FdtParserHandle,
                              FdtParserHandle->Context,
                              &CmObjRef,
                              Token
                              );
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  if (ElementTokenMap == NULL) {
    FreePool (LocalElementTokenMap);
  }

  return Status;
}

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
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  FDT_HW_INFO_PARSER_HANDLE       FdtParserHandle;

  FdtParserHandle = (FDT_HW_INFO_PARSER_HANDLE)ParserHandle;

  NV_ASSERT_RETURN (FdtParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: FdtParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (Count > 0, return EFI_INVALID_PARAMETER, "%a: Count can't be 0\n", __FUNCTION__);
  NV_ASSERT_RETURN (TokenMapPtr != NULL, return EFI_INVALID_PARAMETER, "%a: TokenMapPtr is NULL\n", __FUNCTION__);

  Repo = (EDKII_PLATFORM_REPOSITORY_INFO  *)(FdtParserHandle->Context);

  Status = Repo->NewTokenMap (Repo, Count, TokenMapPtr);
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  return Status;
}

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
  )
{
  EFI_STATUS                 Status;
  CM_OBJECT_TOKEN            *ElementTokenMap;
  FDT_HW_INFO_PARSER_HANDLE  FdtParserHandle;

  ElementTokenMap = NULL;

  FdtParserHandle = (FDT_HW_INFO_PARSER_HANDLE)ParserHandle;

  NV_ASSERT_RETURN (ParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: ParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (FdtParserHandle->HwInfoAdd != NULL, return EFI_INVALID_PARAMETER, "%a: FdtParserHandle->HwInfoAdd pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc != NULL, return EFI_INVALID_PARAMETER, "%a: CmObjDesc pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc->Count > 0, return EFI_INVALID_PARAMETER, "%a: CmObjDesc's count can't be 0\n", __FUNCTION__);
  NV_ASSERT_RETURN ((CmObjDesc->Data != NULL) || (CmObjDesc->Size == 0), return EFI_INVALID_PARAMETER, "%a: CmObjDesc's Data is NULL while Size is not\n", __FUNCTION__);

  // Extend the multi-object array and let tokens be generated
  Status = NvHwInfoExtend (FdtParserHandle, FdtParserHandle->Context, CmObjDesc, Token, &ElementTokenMap);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  if (TokenMapPtr != NULL) {
    *TokenMapPtr = ElementTokenMap;
  } else {
    FREE_NON_NULL (ElementTokenMap);
  }

  return Status;
}

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
  )
{
  FDT_HW_INFO_PARSER_HANDLE       FdtParserHandle;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  NVIDIA_AML_GENERATION_PROTOCOL  *GenerationProtocol;

  if ((ParserHandle == NULL) || (ProtocolPtr == NULL)) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  FdtParserHandle    = (FDT_HW_INFO_PARSER_HANDLE)ParserHandle;
  Repo               = (EDKII_PLATFORM_REPOSITORY_INFO  *)(FdtParserHandle->Context);
  GenerationProtocol = Repo->GenerationProtocol;
  *ProtocolPtr       = GenerationProtocol;
  return EFI_SUCCESS;
}

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
  )
{
  FDT_HW_INFO_PARSER_HANDLE       FdtParserHandle;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  NVIDIA_AML_PATCH_PROTOCOL       *PatchProtocol;

  if ((ParserHandle == NULL) || (ProtocolPtr == NULL)) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  FdtParserHandle = (FDT_HW_INFO_PARSER_HANDLE)ParserHandle;
  Repo            = (EDKII_PLATFORM_REPOSITORY_INFO  *)(FdtParserHandle->Context);
  PatchProtocol   = Repo->PatchProtocol;
  *ProtocolPtr    = PatchProtocol;
  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS                 Status;
  FDT_HW_INFO_PARSER_HANDLE  FdtParserHandle;

  FdtParserHandle = (FDT_HW_INFO_PARSER_HANDLE)ParserHandle;

  NV_ASSERT_RETURN (FdtParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: FdtParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (DescPtr != NULL, return EFI_INVALID_PARAMETER, "%a: Result pointer is NULL\n", __FUNCTION__);

  // Find the object in the repo
  Status = NvHwInfoFind (FdtParserHandle, FdtParserHandle->Context, ObjectId, Token, DescPtr);
  DEBUG ((DEBUG_INFO, "%a: Returning Status\"%r\" trying to find ObjectId 0x%x, token 0x%x\n", __FUNCTION__, Status, ObjectId, Token));

  return Status;
}
