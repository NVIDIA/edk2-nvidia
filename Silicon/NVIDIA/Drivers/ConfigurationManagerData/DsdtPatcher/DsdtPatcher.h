/** @file
  Patches to the DSDT

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef DSDT_PATCHER_H_
#define DSDT_PATCHER_H_

#include <Library/NvCmObjectDescUtility.h>

#define ACPI_PLAT_INFO  "_SB_.PLAT"
#define ACPI_GED1_SMR1  "_SB_.GED1.SMR1"
#define ACPI_QSPI1_STA  "_SB_.QSP1._STA"
#define ACPI_I2C3_STA   "_SB_.I2C3._STA"
#define ACPI_SSIF_STA   "_SB_.I2C3.SSIF._STA"

/** DSDT patcher function.

  The DSDT table is potentially patched with the following information:
    "_SB_.PLAT"
    "_SB_.GED1.SMR1"
    "_SB_.QSP1._STA"
    "_SB_.I2C3._STA"
    "_SB_.I2C3.SSIF._STA"

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
DsdtPatcher (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif // DSDT_PATCHER_H_
