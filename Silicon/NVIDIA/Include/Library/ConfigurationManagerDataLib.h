/** @file

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CONFIGURATION_MANAGER_DATA_LIB_H__
#define CONFIGURATION_MANAGER_DATA_LIB_H__

#include <Protocol/AmlPatchProtocol.h>
#include <Protocol/AmlGenerationProtocol.h>
#include <Protocol/ConfigurationManagerTokenProtocol.h>
#include <ConfigurationManagerObject.h>
#include <Library/HwInfoParserLib.h>

#define FREE_NON_NULL(a) \
  if ((a) != NULL) { \
    FreePool ((a));  \
    (a) = NULL;      \
  }

// Forward declarations - must match ConfigurationManagerTokenProtocol.h
typedef struct _NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL;
typedef struct PlatformRepositoryInfo                        EDKII_PLATFORM_REPOSITORY_INFO;

/** Function pointer to a parser function.

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    Handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
typedef
EFI_STATUS
(EFIAPI *HW_INFO_PARSER_FUNC)(
  IN  CONST HW_INFO_PARSER_HANDLE ParserHandle,
  IN        INT32                 FdtBranch
  );

typedef struct ParserInfo {
  CONST CHAR8            *ParserName;
  HW_INFO_PARSER_FUNC    Parser;
} PARSER_INFO;

#define EXPAND_MACRO(X)        X
#define STR(X)                 #X
#define CREATE_PARSER(PARSER)  {STR(PARSER), EXPAND_MACRO(PARSER)}

/** The configuration manager version
*/
#define CONFIGURATION_MANAGER_REVISION  CREATE_REVISION (2, 0)

/** The OEM ID
*/
#define CFG_MGR_OEM_ID  { 'N', 'V', 'I', 'D', 'I', 'A' }

/** A structure describing the platform configuration
    manager repository information
*/
typedef struct PlatformRepositoryInfoEntry {
  // Configuration Manager Object Description
  CM_OBJ_DESCRIPTOR    CmObjectDesc;

  // Token for the entire object
  CM_OBJECT_TOKEN      Token;

  // Array of Tokens for the individual items in the descriptor
  CM_OBJECT_TOKEN      *ElementTokenMap;
} EDKII_PLATFORM_REPOSITORY_INFO_ENTRY;

struct PlatformRepositoryInfo {
  EDKII_PLATFORM_REPOSITORY_INFO_ENTRY           *Entries;
  UINT32                                         EntryCount;
  UINT32                                         MaxEntries;

  // AML Patch protocol
  NVIDIA_AML_PATCH_PROTOCOL                      *PatchProtocol;
  NVIDIA_AML_GENERATION_PROTOCOL                 *GenerationProtocol;

  // Token Protocol
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL    *TokenProtocol;

  // Add new entry and return token map for it
  EFI_STATUS                                     (*NewEntry)(
    struct PlatformRepositoryInfo  *This,
    CM_OBJECT_ID                   CmObjectId,
    UINT32                         CmObjectSize,
    UINT32                         CmObjectCount,
    VOID                           *CmObjectPtr,
    CM_OBJECT_TOKEN                **CmObjectTokenMap,
    CM_OBJECT_TOKEN                *CmObjectToken
    );

  // Add new entry with the specified token map
  EFI_STATUS    (*NewEntryWithMap)(
    struct PlatformRepositoryInfo  *This,
    CM_OBJECT_ID                   CmObjectId,
    UINT32                         CmObjectSize,
    UINT32                         CmObjectCount,
    VOID                           *CmObjectPtr,
    CM_OBJECT_TOKEN                *CmObjectTokenMap,
    CM_OBJECT_TOKEN                CmObjectToken
    );

  // Allocate a TokenMap with tokens for a new entry
  EFI_STATUS    (*NewTokenMap)(
    struct PlatformRepositoryInfo  *This,
    UINT32                         TokenCount,
    CM_OBJECT_TOKEN                **TokenMap
    );

  // Extend an existing entry with additional elements and return new tokens for them
  EFI_STATUS    (*ExtendEntry)(
    struct PlatformRepositoryInfo  *This,
    CM_OBJECT_ID                   CmObjectId,
    UINT32                         CmObjectSize,
    UINT32                         CmObjectCount,
    VOID                           *CmObjectPtr,
    CM_OBJECT_TOKEN                CmObjectToken,
    CM_OBJECT_TOKEN                **CmObjectTokenMap
    );

  // Find an entry
  EFI_STATUS    (*FindEntry)(
    struct PlatformRepositoryInfo         *This,
    CM_OBJECT_ID                          CmObjectId,
    CM_OBJECT_TOKEN                       Token,
    EDKII_PLATFORM_REPOSITORY_INFO_ENTRY  **Entry
    );
};

extern EFI_GUID  gNVIDIAConfigurationManagerDataProtocolGuid;

/** The EOEM_OBJECT_ID enum describes the Object IDs
    in the OEM Namespace
*/
typedef enum OemObjectID {
  EOemObjReserved,                                             ///<  0 - Reserved
  EOemObjCmParser,                                             ///<  1 - Config Manager Parser
  EOemObjCmCacheMetadata,                                      ///<  2 - Cache Metadata
  EOemObjMax
} EOEM_OBJECT_ID;

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
  );

/** NvHwInfoParse: sequentially call the given parsers/dispatchers

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle      A handle to the parser instance.
  @param [in]  FdtBranch         When searching for DT node name, restrict
                                 the search to this Device Tree branch.
  @param [in]  HwInfoParserTable The table of parser functions to call
  @param [in]  TableSize         The number of parsers in the table

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
NvHwInfoParse (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch,
  IN  CONST PARSER_INFO            *HwInfoParserTable,
  IN        UINT32                 TableSize
  );

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
  );

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
  );

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
  IN        CM_OBJECT_TOKEN        *ElementTokenMap,
  IN        CM_OBJECT_TOKEN        Token
  );

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
  );

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
  );

#endif // CONFIGURATION_MANAGER_DATA_LIB_H__
