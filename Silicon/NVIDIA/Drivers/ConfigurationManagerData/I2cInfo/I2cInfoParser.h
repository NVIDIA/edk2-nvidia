/** @file
  I2C info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef I2C_INFO_PARSER_H_
#define I2C_INFO_PARSER_H_

#include <Protocol/AmlPatchProtocol.h>
#include <Library/HwInfoParserLib.h>

#define ACPI_I2CT_REG0  "I2CT.REG0"
#define ACPI_I2CT_UID   "I2CT._UID"
#define ACPI_I2CT_INT0  "I2CT.INT0"

extern AML_OFFSET_TABLE_ENTRY  SSDT_I2CTEMP_OffsetTable[];
extern unsigned char           i2ctemplate_aml_code[];

/** I2C info parser function.

  Adds I2C information to the SSDT ACPI table being generated

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
I2cInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif // I2C_INFO_PARSER_H_
