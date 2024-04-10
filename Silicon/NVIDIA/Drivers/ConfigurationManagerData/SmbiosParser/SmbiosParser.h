/** @file
  SMBIOS TableList Parser.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

/**
  Install Smbios TableList Parser

  @param [in]  ParserHandle A handle to the parser instance.
  @param [in]  FdtBranch    When searching for DT node name, restrict
                            the search to this Device Tree branch.

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/

EFI_STATUS
EFIAPI
InstallCmSmbiosTableListParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );
