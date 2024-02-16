/** @file
  Sdhci info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef SDHCI_INFO_PARSER_H_
#define SDHCI_INFO_PARSER_H_

#include <Protocol/AmlPatchProtocol.h>
#include <Library/HwInfoParserLib.h>

#define ACPI_SDCT_REG0  "SDCT.REG0"
#define ACPI_SDCT_UID   "SDCT._UID"
#define ACPI_SDCT_INT0  "SDCT.INT0"
#define ACPI_SDCT_RMV   "SDCT._RMV"

extern AML_OFFSET_TABLE_ENTRY  SSDT_SDCTEMP_OffsetTable[];
extern unsigned char           sdctemplate_aml_code[];

/** Sdhci info parser function.

  Adds SDHCI information to the SSDT ACPI table being generated

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
SdhciInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif // SDHCI_INFO_PARSER_H_
