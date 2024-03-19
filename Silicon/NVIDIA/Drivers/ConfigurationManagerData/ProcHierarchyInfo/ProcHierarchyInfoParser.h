/** @file
  Proc hierarchy info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef PROC_HIERARCHY_INFO_PARSER_H_
#define PROC_HIERARCHY_INFO_PARSER_H_

#include <Library/NvCmObjectDescUtility.h>

/** A helper macro for populating the Processor Hierarchy Node flags
*/
#define PROC_NODE_FLAGS(                                                \
                                                                        PhysicalPackage,                                              \
                                                                        AcpiProcessorIdValid,                                         \
                                                                        ProcessorIsThread,                                            \
                                                                        NodeIsLeaf,                                                   \
                                                                        IdenticalImplementation                                       \
                                                                        )                                                             \
  (                                                                     \
    PhysicalPackage |                                                   \
    (AcpiProcessorIdValid << 1) |                                       \
    (ProcessorIsThread << 2) |                                          \
    (NodeIsLeaf << 3) |                                                 \
    (IdenticalImplementation << 4)                                      \
  )

/** Proc Hierachy info parser function.

  The following structure is populated:
  - EArmObjProcHierarchyInfo
  It requires tokens from the following structures, whose parsers are called as a result:
  - EArmObjLpiInfo
  - EArmObjCmRef (LpiTokens)
  - EArmObjCacheInfo
  - EArmObjCmRef [for each level of cache hierarchy]
  - EArmObjGicCInfo

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
ProcHierarchyInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif // PROC_HIERARCHY_INFO_PARSER_H_
