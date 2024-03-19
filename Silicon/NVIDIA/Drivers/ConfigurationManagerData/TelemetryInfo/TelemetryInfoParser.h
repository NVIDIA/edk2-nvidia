/** @file
  Patches the DSDT with Telemetry info

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef TELEMETRY_INFO_PARSER_H_
#define TELEMETRY_INFO_PARSER_H_

#include <Library/NvCmObjectDescUtility.h>

/** DSDT patcher for Telemetry info.

  The DSDT table is potentially patched with the following information:
    "_SB_.BPM0.TBUF",
    "_SB_.BPM1.TBUF",
    "_SB_.BPM2.TBUF",
    "_SB_.BPM3.TBUF",
    "_SB_.PM00._STA",
    "_SB_.PM01._STA",
    "_SB_.PM02._STA",
    "_SB_.PM03._STA",
    "_SB_.PM10._STA",
    "_SB_.PM11._STA",
    "_SB_.PM12._STA",
    "_SB_.PM13._STA",
    "_SB_.PM20._STA",
    "_SB_.PM21._STA",
    "_SB_.PM22._STA",
    "_SB_.PM23._STA",
    "_SB_.PM30._STA",
    "_SB_.PM31._STA",
    "_SB_.PM32._STA",
    "_SB_.PM33._STA",
    "_SB_.BPM0.TIME",
    "_SB_.BPM1.TIME",
    "_SB_.BPM2.TIME",
    "_SB_.BPM3.TIME",
    "_SB_.PM01.MINP",
    "_SB_.PM11.MINP",
    "_SB_.PM21.MINP",
    "_SB_.PM31.MINP",
    "_SB_.PM01.MAXP",
    "_SB_.PM11.MAXP",
    "_SB_.PM21.MAXP",
    "_SB_.PM31.MAXP",

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
TelemetryInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif // TELEMETRY_INFO_PARSER_H_
