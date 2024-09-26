/** @file
  Cache info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CACHE_INFO_PARSER_H_
#define CACHE_INFO_PARSER_H_

#include <ConfigurationManagerObject.h>
#include <Library/HwInfoParserLib.h>

typedef struct CacheHierarchyInfoData {
  CM_OBJECT_TOKEN    Token;
  UINTN              Count;
} CACHE_HIERARCHY_INFO_DATA;

typedef struct CacheHierarchyInfoCpu {
  CACHE_HIERARCHY_INFO_DATA    Data;
} CACHE_HIERARCHY_INFO_CPU;

typedef struct CacheHierarchyInfoCluster {
  CACHE_HIERARCHY_INFO_DATA    Data;
  CACHE_HIERARCHY_INFO_CPU     *Cpu;
} CACHE_HIERARCHY_INFO_CLUSTER;

typedef struct CacheHierarchyInfoSocket {
  CACHE_HIERARCHY_INFO_DATA       Data;
  CACHE_HIERARCHY_INFO_CLUSTER    *Cluster;
} CACHE_HIERARCHY_INFO_SOCKET;

EFI_STATUS
EFIAPI
AllocateCacheHierarchyInfo (
  CACHE_HIERARCHY_INFO_SOCKET  **HierarchyInfoPtr
  );

VOID
FreeCacheHierarchyInfo (
  CACHE_HIERARCHY_INFO_SOCKET  *Socket
  );

/** Cache info parser function.

  The following structures are populated:
  - EArchCommonObjCacheInfo
  - EArchCommonObjCmRef [for each level of cache hierarchy]

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.
  @param [out] HierarchyInfo   Where to put the structure containing the
                               cache hierarchy information. Caller is
                               responsible for calling FreeCacheHierarchyInfo
                               to free it once no longer needed. Note:
                               Sockets/Clusters/Cores are logical, not physical.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
CacheInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE        ParserHandle,
  IN        INT32                        FdtBranch,
  OUT       CACHE_HIERARCHY_INFO_SOCKET  **HierarchyInfo
  );

/** Cache info parser function for T194.

  The following structures are populated:
  - EArchCommonObjCacheInfo
  - EArchCommonObjCmRef [for each level of cache hierarchy]

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.
  @param [out] HierarchyInfo   Where to put the structure containing the
                               cache hierarchy information. Caller is
                               responsible for calling FreeCacheHierarchyInfo
                               to free it once no longer needed.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
CacheInfoParserT194 (
  IN  CONST HW_INFO_PARSER_HANDLE        ParserHandle,
  IN        INT32                        FdtBranch,
  OUT       CACHE_HIERARCHY_INFO_SOCKET  **HierarchyInfo
  );

#endif // CACHE_INFO_PARSER_H_
