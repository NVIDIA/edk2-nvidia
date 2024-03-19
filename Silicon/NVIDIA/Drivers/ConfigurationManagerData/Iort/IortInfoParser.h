/** @file
  IORT info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef IORT_INFO_PARSER_H_
#define IORT_INFO_PARSER_H_

#include <Library/NvCmObjectDescUtility.h>

/** IORT info parser function.

  The following structures are potentially populated:
  - EArmObjGicItsIdentifierArray
  - EArmObjIdMappingArray
  - EArmObjSmmuInterruptArray [ContextInterrupt, PmuInterrupt]
  - EArmObjIdMappingArray [PciRc, NamedComponent, SmmuV1V2]
  - EArmObjItsGroup
  - EArmObjNamedComponent
  - EArmObjRootComplex
  - EArmObjSmmuV1SmmuV2
  - EArmObjSmmuV3
  - EArmObjPmcg
  - EArmObjGicItsIdentifierArray
  - EArmObjIdMappingArray
  - EArmObjSmmuInterruptArray

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
IortInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif // IORT_INFO_PARSER_H_
