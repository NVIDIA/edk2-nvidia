/** @file
  Configuration Manager Data Repo Lib

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CONFIGURATION_MANAGER_DATA_REPO_LIB_H__
#define CONFIGURATION_MANAGER_DATA_REPO_LIB_H__

#include <Library/ConfigurationManagerDataLib.h>

#define CONCAT(a, b)                                    a##b
#define REGISTER_PARSER_FUNCTION_NAME(parser_function)  CONCAT(Register, parser_function)
#define REGISTER_PARSER_FUNCTION(PARSER_FUNCTION, PARSER_SKIP_STRING) \
EFIAPI \
EFI_STATUS \
REGISTER_PARSER_FUNCTION_NAME (PARSER_FUNCTION) (\
  IN EFI_HANDLE        ImageHandle,\
  IN EFI_SYSTEM_TABLE  *SystemTable\
  )\
{\
  EFI_STATUS  Status;\
  PARSER_INFO Parser = CREATE_PARSER (PARSER_FUNCTION);\
\
  Status = ConfigManagerDataRepoRegisterParser (&Parser, PARSER_SKIP_STRING);\
  if (EFI_ERROR (Status)) {\
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the %a parser\n", __FUNCTION__, Status, Parser.ParserName));\
    return Status;\
  }\
\
  return EFI_SUCCESS;\
}\


/**
  Function to register a parser for use by the ConfigManager.

  @param  [in]  Parser            The CM parser library to register.
  @param  [in]  ParserSkipString  The DTB property to check to see if
                                  the parser should be skipped.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
ConfigManagerDataRepoRegisterParser (
  PARSER_INFO  *Parser,
  CHAR8        *ParserSkipString
  );

#endif // CONFIGURATION_MANAGER_DATA_REPO_LIB_H__
