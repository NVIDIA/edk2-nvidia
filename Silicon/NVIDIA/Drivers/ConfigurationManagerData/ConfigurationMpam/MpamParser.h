/** @file
  Memory System Resource Partitioning and Monitoring Table Parser.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MPAM_PARSER_H_
#define MPAM_PARSER_H_

// Generate socket Id from Phy addr. Max 4 sockets supported
#define SOCKETID_FROM_PHYS_ADDR(phys)  (((phys) >> 43) & 0x3)

EFI_STATUS
EFIAPI
UpdateResourceNodeInfo (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle
  );

EFI_STATUS
EFIAPI
UpdateMscNodeInfo (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle
  );

/**
  Checks if MPAM nodes are enabled in the device tree

  @retval   TRUE or FALSE
 */
BOOLEAN
EFIAPI
IsMpamEnabled (
  VOID
  );

/** MPAM parser function.
  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.
  @param [in]  ParserHandle A handle to the parser instance.
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
MpamParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif
