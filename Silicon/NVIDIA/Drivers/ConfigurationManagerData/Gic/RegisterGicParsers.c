/** @file
  Register the Gic parsers

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "../ConfigurationManagerDataRepoLib.h"

#include "GicParser.h"
#include <Library/TegraPlatformInfoLib.h>
#include <Library/NVIDIADebugLib.h>

#define ADD_SINGLE_PARSER(PARSER, SKIP) \
  do {\
    PARSER_INFO Parser = CREATE_PARSER (PARSER);\
    Status = ConfigManagerDataRepoRegisterParser (&Parser, SKIP);\
    if (EFI_ERROR (Status)) {\
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the %a parser\n", __FUNCTION__, Status, Parser.ParserName));\
      return Status;\
    }\
  } while (FALSE);

REGISTER_PARSER_FUNCTION (GicDParser, NULL)
REGISTER_PARSER_FUNCTION (GicRedistributorParser, NULL)
REGISTER_PARSER_FUNCTION (GicItsParser, NULL)
REGISTER_PARSER_FUNCTION (GicMsiFrameParser, NULL)

/** Registers the Gic parsers

  The following parsers are potentially registered:
    GicDParser
    GicRedistributorParser
    GicItsParser
    GicMsiFrameParser

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
EFIAPI
EFI_STATUS
RegisterGicParsers (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = RegisterGicDParser (ImageHandle, SystemTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: got %r trying to register GicDParser\n", __FUNCTION__, Status));
  }

  Status = RegisterGicRedistributorParser (ImageHandle, SystemTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: got %r trying to register GicRedistributorParser\n", __FUNCTION__, Status));
  }

  Status = RegisterGicItsParser (ImageHandle, SystemTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: got %r trying to register GicItsParser\n", __FUNCTION__, Status));
  }

  Status = RegisterGicMsiFrameParser (ImageHandle, SystemTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: got %r trying to register GicMsiFrameParser\n", __FUNCTION__, Status));
  }

  return Status;
}
