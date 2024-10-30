/** @file
  Configuration Manager Data Library

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2021 - 2022, Arm Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Added single entries get a token.
  Added multi-count entries get sequential tokens for each entry.
  Extended entries DO NOT GET tokens for the additional entries.

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/

#include <Uefi.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <ConfigurationManagerObject.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/TableHelperLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

/** ConfigManagerAllocateToken: allocates tokens to be used for upcoming entries

  This allocates tokens for future ConfigurationManager data, allowing tokens to
  be reserved before the data is ready to be added.

  @param [in]  This               The repo the tokens will belong to
  @param [in]  TokenCount         The number of tokens to allocate
  @param [out] TokenMapPtr        Pointer to the allocated array of tokens,
                                  with TokenCount entries

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_OUT_OF_RESOURCES    Unable to allocate memory.
**/
STATIC
EFI_STATUS
EFIAPI
ConfigManagerTokenProtocolAllocateTokens (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *This,
  IN UINT32                          TokenCount,
  OUT CM_OBJECT_TOKEN                **TokenMapPtr
  )
{
  NV_ASSERT_RETURN (This != NULL, return EFI_INVALID_PARAMETER, "%a: This pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (This->TokenProtocol != NULL, return EFI_INVALID_PARAMETER, "%a; This->TokenProtocol is NULL\n", __FUNCTION__);

  return This->TokenProtocol->AllocateTokens (This->TokenProtocol, TokenCount, TokenMapPtr);
}

/**
  Find an Entry in the repository

  This searches the repository for an object by ID, and possibly
  distinguishes it by token value.

  @param  [in]  This          The repo structure to search.
  @param  [in]  CmObjectId    The Id to search for.
  @param  [in]  Token         The token value to search for. Possibly CM_NULL_TOKEN.
  @param  [out] Entry         Pointer to where to put the located entry pointer.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching entry was found.
**/
STATIC
EFI_STATUS
EFIAPI
ConfigManagerEntryFind (
  IN EDKII_PLATFORM_REPOSITORY_INFO         *This,
  IN CM_OBJECT_ID                           CmObjectId,
  IN CM_OBJECT_TOKEN                        Token,
  OUT EDKII_PLATFORM_REPOSITORY_INFO_ENTRY  **Entry
  )
{
  UINTN                                 Index;
  EDKII_PLATFORM_REPOSITORY_INFO_ENTRY  *Repo;
  CM_OBJ_DESCRIPTOR                     *Desc;
  UINTN                                 SubobjectIndex;
  UINTN                                 TokenCount;

  NV_ASSERT_RETURN (This != NULL, return EFI_INVALID_PARAMETER, "%a: This pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (Entry != NULL, return EFI_INVALID_PARAMETER, "%a: Entry pointer is NULL\n", __FUNCTION__);

  Repo = This->Entries;
  for (Index = 0; Index < This->EntryCount; Index++) {
    Desc = &Repo[Index].CmObjectDesc;

    // ID must match
    if (Desc->ObjectId != CmObjectId) {
      continue;
    }

    if (Token != CM_NULL_TOKEN) {
      if (Token != Repo[Index].Token) {
        if (Repo[Index].ElementTokenMap == NULL) {
          continue;
        }

        // Token must be for a particular element
        TokenCount = Desc->Count;
        for (SubobjectIndex = 0; SubobjectIndex < TokenCount; SubobjectIndex++) {
          if (Token == Repo[Index].ElementTokenMap[SubobjectIndex]) {
            break;
          }
        }

        if (SubobjectIndex == TokenCount) {
          // Token wasn't found
          continue;
        }
      }
    }

    *Entry = &Repo[Index];
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "Failed to find an entry with ID 0x%X, token %u\n", CmObjectId, Token));
  for (Index = 0; Index < This->EntryCount; Index++) {
    DEBUG ((DEBUG_INFO, "Entry[%u] has ID 0x%X\n", Index, Repo[Index].CmObjectDesc.ObjectId));
  }

  return EFI_NOT_FOUND;
}

/**
  Add an Entry to the repository with a specified token map

  This creates a new Entry in the repository, copying the data into the Entry.
  The entry is associated with the specified element token map

  @param  [in]  This            The repo structure to add the entry to.
  @param  [in]  CmObjectId      The Id to add.
  @param  [in]  CmObjectSize    The total data size to add.
  @param  [in]  CmObjectCount   The number of elements in the added data.
  @param  [in]  CmObjectPtr     The data to copy into the Entry.
  @param  [in]  ElementTokenMap Pointer to the ElementTokenMap for the Entry
  @param  [in]  Token           The token for the whole object

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching entry was found.
**/
STATIC
EFI_STATUS
EFIAPI
ConfigManagerEntryAddWithTokenMap (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *This,
  IN CM_OBJECT_ID                    CmObjectId,
  IN UINT32                          CmObjectSize,
  IN UINT32                          CmObjectCount,
  IN VOID                            *CmObjectPtr,
  IN CM_OBJECT_TOKEN                 *ElementTokenMap OPTIONAL,
  IN CM_OBJECT_TOKEN                 Token
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO_ENTRY  *Entry;
  CM_OBJ_DESCRIPTOR                     *Desc;
  VOID                                  *Data;
  UINTN                                 TokenCount;

  NV_ASSERT_RETURN (This != NULL, return EFI_INVALID_PARAMETER, "%a: This pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjectCount != 0, return EFI_INVALID_PARAMETER, "%a: CmObjectCount can't be 0\n", __FUNCTION__);

  // Don't currently support resizing
  if (This->EntryCount >= This->MaxEntries) {
    DEBUG ((DEBUG_ERROR, "%a: Can't add a new entry (current entries = %u, max entries = %u)\n", __FUNCTION__, This->EntryCount, This->MaxEntries));
    return EFI_OUT_OF_RESOURCES;
  }

  // Data must be copied to conform to ARM spec and not create
  // pitfalls with Extend
  Data = NULL;
  if (CmObjectPtr != NULL) {
    Data = AllocateCopyPool (CmObjectSize, CmObjectPtr);
    if (Data == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %u bytes to copy the requested entry into the repo\n", __FUNCTION__, CmObjectSize));
      return EFI_OUT_OF_RESOURCES;
    }
  }

  Entry = &This->Entries[This->EntryCount];
  Desc  = &Entry->CmObjectDesc;

  Desc->ObjectId = CmObjectId;
  Desc->Size     = CmObjectSize;
  Desc->Data     = Data;
  Desc->Count    = CmObjectCount;
  // ArmObjCmRef objects have a >1 count, but shouldn't have an ElementTokenMap
  if ((CmObjectId == CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef)) || (ElementTokenMap == NULL)) {
    Entry->ElementTokenMap = NULL;
  } else {
    TokenCount             = Desc->Count;
    Entry->ElementTokenMap = AllocateCopyPool (TokenCount * sizeof (CM_OBJECT_TOKEN), ElementTokenMap);
    if (Entry->ElementTokenMap == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %u bytes to copy the token map into the repo\n", __FUNCTION__, TokenCount * sizeof (CM_OBJECT_TOKEN)));
      return EFI_OUT_OF_RESOURCES;
    }
  }

  Entry->Token = Token;

  This->EntryCount++;
  return EFI_SUCCESS;
}

/**
  Add an Entry to the repository

  This creates a new Entry in the repository, copying the data into the Entry.
  If Token is non-NULL, the token values for the Entry will be returned in *TokenMap.

  @param  [in]  This          The repo structure to add the entry to.
  @param  [in]  CmObjectId    The Id to add.
  @param  [in]  CmObjectSize  The total data size to add.
  @param  [in]  CmObjectCount The number of elements in the added data.
  @param  [in]  CmObjectPtr   The data to copy into the Entry.
  @param  [out] TokenMapPtr   Pointer to the allocated ElementTokenMap for the data.
  @param  [out] TokenPtr      Pointer to the token for the whole object.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching entry was found.
**/
STATIC
EFI_STATUS
EFIAPI
ConfigManagerEntryAdd (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *This,
  IN CM_OBJECT_ID                    CmObjectId,
  IN UINT32                          CmObjectSize,
  IN UINT32                          CmObjectCount,
  IN VOID                            *CmObjectPtr,
  OUT CM_OBJECT_TOKEN                **TokenMapPtr OPTIONAL,
  OUT CM_OBJECT_TOKEN                *TokenPtr OPTIONAL
  )
{
  EFI_STATUS       Status;
  CM_OBJECT_TOKEN  *LocalMap;
  CM_OBJECT_TOKEN  Token;

  NV_ASSERT_RETURN (This != NULL, return EFI_INVALID_PARAMETER, "%a: This pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjectCount != 0, return EFI_INVALID_PARAMETER, "%a: CmObjectCount can't be 0\n", __FUNCTION__);

  // Don't currently support resizing
  if (This->EntryCount >= This->MaxEntries) {
    DEBUG ((DEBUG_ERROR, "%a: Can't add a new entry (current entries = %u, max entries = %u)\n", __FUNCTION__, This->EntryCount, This->MaxEntries));
    return EFI_OUT_OF_RESOURCES;
  }

  LocalMap = NULL;
  Status   = This->NewTokenMap (This, CmObjectCount + 1, &LocalMap);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Token = LocalMap[CmObjectCount];

  Status = This->NewEntryWithMap (This, CmObjectId, CmObjectSize, CmObjectCount, CmObjectPtr, LocalMap, Token);
  if (EFI_ERROR (Status)) {
    FreePool (LocalMap);
    return Status;
  }

  if (TokenPtr != NULL) {
    *TokenPtr = Token;
  }

  if (TokenMapPtr != NULL) {
    *TokenMapPtr = LocalMap;
  } else {
    FreePool (LocalMap);
  }

  return Status;
}

/**
  Extend an Entry in the repository

  This finds the Entry in the repository and copies the additional data into the Entry.
  If Token is not CM_NULL_TOKEN, the token value will be used to locate the entry.
  If TokenMapPtr is non-null, a free-able pointer to an array of the new tokens will be returned.

  @param  [in]  This          The repo structure of the Entry.
  @param  [in]  CmObjectId    The Id of the Entry to add to.
  @param  [in]  CmObjectSize  The size of the new data being added.
  @param  [in]  CmObjectCount The number of elements being added.
  @param  [in]  CmObjectPtr   The data to add to the Entry.
  @param  [in]  Token         Optionally used to locate the entry.
  @param  [out] TokenMapPtr   ElementTokenMap for the new entries in the extended object

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           No matching entry was found.
**/
STATIC
EFI_STATUS
EFIAPI
ConfigManagerEntryExtend (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *This,
  IN CM_OBJECT_ID                    CmObjectId,
  IN UINT32                          CmObjectSize,
  IN UINT32                          CmObjectCount,
  IN VOID                            *CmObjectPtr,
  IN CM_OBJECT_TOKEN                 Token OPTIONAL,
  OUT CM_OBJECT_TOKEN                **TokenMapPtr OPTIONAL
  )
{
  EFI_STATUS                            Status;
  VOID                                  *NewData;
  UINT32                                ElementSize;
  CM_OBJ_DESCRIPTOR                     *Desc;
  EDKII_PLATFORM_REPOSITORY_INFO_ENTRY  *Entry;
  CM_OBJECT_TOKEN                       *NewTokenMap;
  CM_OBJECT_TOKEN                       *NewTokens;

  NewTokens = NULL;

  NV_ASSERT_RETURN (This != NULL, return EFI_INVALID_PARAMETER, "%a: This pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjectCount != 0, return EFI_INVALID_PARAMETER, "%a: CmObjectCount can't be 0\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjectId != CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef), return EFI_INVALID_PARAMETER, "%a: Can't extend EArmObjCmRef objects\n", __FUNCTION__);

  Status = This->FindEntry (This, CmObjectId, Token, &Entry);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Desc = &Entry->CmObjectDesc;

  ElementSize = Desc->Size/Desc->Count;
  NV_ASSERT_RETURN (ElementSize == (CmObjectSize/CmObjectCount), return EFI_INVALID_PARAMETER, "%a: Previous element size is %u (%u/%u), but extended element size is %u (%u/%u)\n", __FUNCTION__, ElementSize, Desc->Size, Desc->Count, CmObjectSize/CmObjectCount, CmObjectSize, CmObjectCount);

  // Extend the TokenMap with the new entries
  if (Entry->ElementTokenMap != NULL) {
    NewTokens = NULL;
    Status    = This->NewTokenMap (This, CmObjectCount, &NewTokens);
    if (EFI_ERROR (Status) || (NewTokens == NULL)) {
      goto CleanupAndReturn;
    }

    NewTokenMap = ReallocatePool (sizeof (CM_OBJECT_TOKEN) * Desc->Count, sizeof (CM_OBJECT_TOKEN) * (Desc->Count + CmObjectCount), Entry->ElementTokenMap);
    if (NewTokenMap == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to reallocate %u bytes to extend the token map with %u new entries\n", __FUNCTION__, sizeof (CM_OBJECT_TOKEN) *(Desc->Count + CmObjectCount), CmObjectCount));
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }

    CopyMem (&NewTokenMap[Desc->Count], NewTokens, CmObjectCount * sizeof (CM_OBJECT_TOKEN));
    Entry->ElementTokenMap = NewTokenMap;
  }

  // Extend the Data with the new entries
  NewData = ReallocatePool (Desc->Size, Desc->Size + CmObjectSize, Desc->Data);
  if (NewData == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to reallocate %u bytes to extend the object with %u new entries\n", __FUNCTION__, Desc->Size + CmObjectSize, CmObjectCount));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  Desc->Data = NewData;
  CopyMem (Desc->Data + Desc->Size, CmObjectPtr, CmObjectSize);

  // Update the descriptor to recognize the new entries
  Desc->Size  += CmObjectSize;
  Desc->Count += CmObjectCount;

  Status = EFI_SUCCESS;

CleanupAndReturn:
  if (TokenMapPtr != NULL) {
    *TokenMapPtr = NewTokens;
  } else {
    FREE_NON_NULL (NewTokens);
  }

  return Status;
}

STATIC
VOID
PrintObj (
  IN CONST CM_OBJ_DESCRIPTOR  *CmObjDesc
  )
{
  UINTN  ObjId;
  UINTN  NameSpaceId;

  DEBUG_CODE_BEGIN ();
  if ((CmObjDesc == NULL) || (CmObjDesc->Data == NULL)) {
    return;
  }

  NameSpaceId = GET_CM_NAMESPACE_ID (CmObjDesc->ObjectId);
  ObjId       = GET_CM_OBJECT_ID (CmObjDesc->ObjectId);

  //
  // Print the received objects.
  //
  if (NameSpaceId != EObjNameSpaceOem) {
    ParseCmObjDesc (CmObjDesc);
  } else {
    DEBUG ((DEBUG_ERROR, "NameSpaceId 0x%x, ObjId 0x%x is not supported by the parser\n", NameSpaceId, ObjId));
  }

  DEBUG_CODE_END ();
}

/**
  Function called by the parser to extend information and return the token map.

  Function that the parser can use to extend an existing
  CmObj. This function must copy the CmObj data and not rely on
  the parser preserving the CmObj memory.
  This function is responsible for the Token allocation, and returns them.

  @param  [in]  ParserHandle  A handle to the parser instance.
  @param  [in]  Context       A pointer to the caller's context provided in
                              HwInfoParserInit ().
  @param  [in]  CmObjDesc     CM_OBJ_DESCRIPTOR containing the CmObj(s) to add.
  @param  [in]  Token         Token to use to search for the object to extend.
  @param  [out] TokenMapPtr   If success, contain the ElementTokenMap
                              generated for the new CmObj's elements.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvHwInfoExtend (
  IN        HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        VOID                   *Context,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  IN        CM_OBJECT_TOKEN        Token,
  OUT       CM_OBJECT_TOKEN        **TokenMapPtr
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;

  NV_ASSERT_RETURN (ParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: ParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (Context != NULL, return EFI_INVALID_PARAMETER, "%a: Context pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc != NULL, return EFI_INVALID_PARAMETER, "%a: CmObjDesc pointer is NULL\n", __FUNCTION__);

  Repo = (EDKII_PLATFORM_REPOSITORY_INFO *)Context;

  PrintObj (CmObjDesc);

  Status = Repo->ExtendEntry (
                   Repo,
                   CmObjDesc->ObjectId,
                   CmObjDesc->Size,
                   CmObjDesc->Count,
                   CmObjDesc->Data,
                   Token,
                   (TokenMapPtr ? TokenMapPtr : NULL)
                   );

  return Status;
}

/**
  Function called by the parser to add information and return the token map.

  Function that the parser can use to add new
  CmObj. This function must copy the CmObj data and not rely on
  the parser preserving the CmObj memory.
  This function is responsible for the Token allocation, and returns them.

  @param  [in]  ParserHandle  A handle to the parser instance.
  @param  [in]  Context       A pointer to the caller's context provided in
                              HwInfoParserInit ().
  @param  [in]  CmObjDesc     CM_OBJ_DESCRIPTOR containing the CmObj(s) to add.
  @param  [out] TokenMapPtr   If success, contain the ElementTokenMap
                              generated for the CmObj's element.
  @param  [out] TokenPtr      Pointer to where to put the token for the object

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvHwInfoAddGetMap (
  IN        HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        VOID                   *Context,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  OUT       CM_OBJECT_TOKEN        **TokenMapPtr OPTIONAL,
  OUT       CM_OBJECT_TOKEN        *TokenPtr OPTIONAL
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;

  NV_ASSERT_RETURN (ParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: ParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (Context != NULL, return EFI_INVALID_PARAMETER, "%a: Context pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc != NULL, return EFI_INVALID_PARAMETER, "%a: CmObjDesc pointer is NULL\n", __FUNCTION__);

  Repo = (EDKII_PLATFORM_REPOSITORY_INFO *)Context;

  PrintObj (CmObjDesc);

  Status = Repo->NewEntry (
                   Repo,
                   CmObjDesc->ObjectId,
                   CmObjDesc->Size,
                   CmObjDesc->Count,
                   CmObjDesc->Data,
                   TokenMapPtr,
                   TokenPtr
                   );
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  return Status;
}

/**
  Function pointer called by the parser to add information.

  Callback function that the parser can use to add new
  CmObj. This function must copy the CmObj data and not rely on
  the parser preserving the CmObj memory.
  This function is responsible for the Token allocation.

  @param  [in]  ParserHandle  A handle to the parser instance.
  @param  [in]  Context       A pointer to the caller's context provided in
                              HwInfoParserInit ().
  @param  [in]  CmObjDesc     CM_OBJ_DESCRIPTOR containing the CmObj(s) to add.
  @param  [out] TokenPtr      If provided and success, contain the token
                              generated for the entire CmObj.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvHwInfoAdd (
  IN        HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        VOID                   *Context,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  OUT       CM_OBJECT_TOKEN        *TokenPtr OPTIONAL
  )
{
  EFI_STATUS       Status;
  CM_OBJECT_TOKEN  LocalToken;

  Status = NvHwInfoAddGetMap (ParserHandle, Context, CmObjDesc, NULL, &LocalToken);
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  if (TokenPtr != NULL) {
    *TokenPtr = LocalToken;
  }

  return Status;
}

/**
  Function pointer called by the parser to add information with a token map.

  Callback function that the parser can use to add new
  CmObj that already has a token map. This function must copy the CmObj data
  and not rely on the parser preserving the CmObj memory.
  This function uses the caller-provided token map.

  @param  [in]  ParserHandle  A handle to the parser instance.
  @param  [in]  Context       A pointer to the caller's context provided in
                              HwInfoParserInit ().
  @param  [in]  CmObjDesc     CM_OBJ_DESCRIPTOR containing the CmObj(s) to add.
  @param  [in]  ElementTokenMap Contains the ElementTokenMap for the objects being added.
  @param  [in]  Token         Token to use for the whole object. If CM_NULL_TOKEN,
                              a token will be generated.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NvHwInfoAddWithTokenMap (
  IN        HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        VOID                   *Context,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  IN        CM_OBJECT_TOKEN        *ElementTokenMap OPTIONAL,
  IN        CM_OBJECT_TOKEN        Token
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CM_OBJECT_TOKEN                 *LocalTokenMap;

  NV_ASSERT_RETURN (ParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: ParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (Context != NULL, return EFI_INVALID_PARAMETER, "%a: Context pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (CmObjDesc != NULL, return EFI_INVALID_PARAMETER, "%a: CmObjDesc pointer is NULL\n", __FUNCTION__);

  Repo = (EDKII_PLATFORM_REPOSITORY_INFO *)Context;

  PrintObj (CmObjDesc);

  // Allocate a token if needed
  if (Token == CM_NULL_TOKEN) {
    Status = Repo->NewTokenMap (Repo, 1, &LocalTokenMap);
    NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);
    Token = LocalTokenMap[0];
    FreePool (LocalTokenMap);
  }

  Status = Repo->NewEntryWithMap (
                   Repo,
                   CmObjDesc->ObjectId,
                   CmObjDesc->Size,
                   CmObjDesc->Count,
                   CmObjDesc->Data,
                   ElementTokenMap,
                   Token
                   );
  NV_ASSERT_EFI_ERROR_RETURN (Status, return Status);

  return Status;
}

/** NvHwInfoParse: sequentially call the given parsers/dispatchers

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  The function will continue running all the parsers even if some hit
  errors, and will return the first error code (other than EFI_NOT_FOUND)
  it encounters.

  @param [in]  ParserHandle      A handle to the parser instance.
  @param [in]  FdtBranch         When searching for DT node name, restrict
                                 the search to this Device Tree branch.
  @param [in]  HwInfoParserTable The table of parser functions to call
  @param [in]  TableSize         The number of parsers in the table

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
NvHwInfoParse (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch,
  IN  CONST PARSER_INFO            *HwInfoParserTable,
  IN        UINT32                 TableSize
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  ReturnStatus = EFI_SUCCESS;
  UINT32      Index;

  NV_ASSERT_RETURN (ParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: ParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN ((HwInfoParserTable != NULL) || (TableSize == 0), return EFI_INVALID_PARAMETER, "%a: HwInfoParserTable is NULL while TableSize is not\n", __FUNCTION__);

  for (Index = 0; Index < TableSize; Index++) {
    DEBUG ((DEBUG_ERROR, "%a: Calling %a\n", __FUNCTION__, HwInfoParserTable[Index].ParserName));
    Status = HwInfoParserTable[Index].Parser (
                                        ParserHandle,
                                        FdtBranch
                                        );
    if (Status == EFI_NOT_FOUND) {
      DEBUG ((DEBUG_WARN, "%a: \"%a\" Parser at index %u in the table returned %r - Ignoring it\n", __FUNCTION__, HwInfoParserTable[Index].ParserName, Index, Status));
    } else if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: \"%a\" Parser at index %u in the table returned %r. This may be a fatal error, but attempting to continue anyway\n", __FUNCTION__, HwInfoParserTable[Index].ParserName, Index, Status));
      if (!EFI_ERROR (ReturnStatus)) {
        ReturnStatus = Status;
      }
    }
  }

  return ReturnStatus;
}

/**
  Function called to look up an object or element in the ConfigManager.

  @param  [in]  ParserHandle  A handle to the parser instance.
  @param  [in]  Context       A pointer to the caller's context provided in
                              HwInfoParserInit ().
  @param  [in]  ObjectId      ObjectId of the object to find.
  @param  [in]  Token         Token of the object or element to find, or CM_NULL_TOKEN.
  @param  [out] Desc          Pointer of where to put the resulting CmObjDesc.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           The requested object wasn't found.
**/
EFI_STATUS
EFIAPI
NvHwInfoFind (
  IN        HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        VOID                   *Context,
  IN        CM_OBJECT_ID           ObjectId,
  IN        CM_OBJECT_TOKEN        Token,
  OUT       CM_OBJ_DESCRIPTOR      **Desc
  )
{
  EFI_STATUS                            Status;
  EDKII_PLATFORM_REPOSITORY_INFO        *Repo;
  EDKII_PLATFORM_REPOSITORY_INFO_ENTRY  *Entry;

  NV_ASSERT_RETURN (ParserHandle != NULL, return EFI_INVALID_PARAMETER, "%a: ParserHandle pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (Context != NULL, return EFI_INVALID_PARAMETER, "%a: Context pointer is NULL\n", __FUNCTION__);

  Repo = (EDKII_PLATFORM_REPOSITORY_INFO *)Context;

  Status = Repo->FindEntry (
                   Repo,
                   ObjectId,
                   Token,
                   &Entry
                   );
  if (!EFI_ERROR (Status)) {
    *Desc = &Entry->CmObjectDesc;
  }

  return Status;
}

/** Initialize new SSDT table.

  @param [in]  GenerationProtocol  The table generator to initialize

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
InitializeSsdtTableGenerator (
  IN NVIDIA_AML_GENERATION_PROTOCOL  *GenerationProtocol
  )
{
  EFI_STATUS                   Status;
  EFI_ACPI_DESCRIPTION_HEADER  SsdtTableHeader;

  NV_ASSERT_RETURN (GenerationProtocol != NULL, return EFI_INVALID_PARAMETER, "%a: GenerationProtocol is NULL\n", __FUNCTION__);

  SsdtTableHeader.Signature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
  SsdtTableHeader.Length    = sizeof (EFI_ACPI_DESCRIPTION_HEADER);
  SsdtTableHeader.Revision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
  SsdtTableHeader.Checksum  = 0;
  CopyMem (SsdtTableHeader.OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (SsdtTableHeader.OemId));
  SsdtTableHeader.OemTableId      = PcdGet64 (PcdAcpiDefaultOemTableId);
  SsdtTableHeader.OemRevision     = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  SsdtTableHeader.CreatorId       = FixedPcdGet32 (PcdAcpiDefaultCreatorId);
  SsdtTableHeader.CreatorRevision = FixedPcdGet32 (PcdAcpiDefaultCreatorRevision);

  Status = GenerationProtocol->InitializeTable (GenerationProtocol, &SsdtTableHeader);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return GenerationProtocol->StartScope (GenerationProtocol, "_SB");
}

/** ConfigurationManagerDataInit: allocates and initializes the structure

  This allocates space for the ConfigurationManager data, and initializes
  the fields before returning it to the caller.

  @param [in]  MaxEntries        The maximum number of entries to support
  @param [out] Repo              Pointer to the initialized data repository

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_OUT_OF_RESOURCES    Unable to allocate memory.
**/
EFI_STATUS
EFIAPI
ConfigurationManagerDataInit (
  IN  UINT32                          MaxEntries,
  OUT EDKII_PLATFORM_REPOSITORY_INFO  **Repo
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *LocalRepo;
  VOID                            *AcpiTableProtocol;

  LocalRepo = NULL;

  NV_ASSERT_RETURN (MaxEntries != 0, return EFI_INVALID_PARAMETER, "%a: Max entries can't be 0\n", __FUNCTION__);
  NV_ASSERT_RETURN (Repo != NULL, return EFI_INVALID_PARAMETER, "%a: Repo pointer can't be NULL\n", __FUNCTION__);

  // Allocate Structure
  LocalRepo = AllocateZeroPool (sizeof (EDKII_PLATFORM_REPOSITORY_INFO));
  if (LocalRepo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %u bytes for the repo\n", __FUNCTION__, sizeof (EDKII_PLATFORM_REPOSITORY_INFO)));
    return EFI_OUT_OF_RESOURCES;
  }

  LocalRepo->Entries = AllocateZeroPool (MaxEntries * sizeof (EDKII_PLATFORM_REPOSITORY_INFO_ENTRY));
  if (LocalRepo->Entries == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %u bytes for %u repo entries\n", __FUNCTION__, sizeof (EDKII_PLATFORM_REPOSITORY_INFO_ENTRY) * MaxEntries, MaxEntries));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  // Initialize the fields
  LocalRepo->EntryCount      = 0;
  LocalRepo->MaxEntries      = MaxEntries;
  LocalRepo->NewEntry        = ConfigManagerEntryAdd;
  LocalRepo->NewEntryWithMap = ConfigManagerEntryAddWithTokenMap;
  LocalRepo->NewTokenMap     = ConfigManagerTokenProtocolAllocateTokens;
  LocalRepo->ExtendEntry     = ConfigManagerEntryExtend;
  LocalRepo->FindEntry       = ConfigManagerEntryFind;

  Status = gBS->LocateProtocol (&gNVIDIAConfigurationManagerTokenProtocolGuid, NULL, (VOID **)&LocalRepo->TokenProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to locate the Token Protocol\n", __FUNCTION__));
    goto CleanupAndReturn;
  }

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, &AcpiTableProtocol);
  if (EFI_ERROR (Status)) {
    // These protocols are not present when ACPI is not supported, ignore error.
    DEBUG ((DEBUG_ERROR, "%a: Unable to locate the ACPI Table Protocol\n", __FUNCTION__));
    LocalRepo->PatchProtocol      = NULL;
    LocalRepo->GenerationProtocol = NULL;
    Status                        = EFI_SUCCESS;
  } else {
    // Add pointers to the Repo for patching and generating ACPI tables
    Status = gBS->LocateProtocol (&gNVIDIAAmlPatchProtocolGuid, NULL, (VOID **)&LocalRepo->PatchProtocol);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    Status = gBS->LocateProtocol (&gNVIDIAAmlGenerationProtocolGuid, NULL, (VOID **)&LocalRepo->GenerationProtocol);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    Status = InitializeSsdtTableGenerator (LocalRepo->GenerationProtocol);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }
  }

CleanupAndReturn:
  if (EFI_ERROR (Status)) {
    if (LocalRepo != NULL) {
      FREE_NON_NULL (LocalRepo->Entries);
      FreePool (LocalRepo);
    }

    *Repo = NULL;
  } else {
    *Repo = LocalRepo;
  }

  return Status;
}
