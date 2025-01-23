/** @file

  CXL Driver header

  SPDX-FileCopyrightText: Copyright (c) 2025-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CXL_DXE_H__
#define __CXL_DXE_H__

#include <Library/UefiLib.h>

#include <Protocol/DriverBinding.h>
#include <Protocol/DevicePath.h>
#include <Protocol/PciIo.h>

/**
  Function to parse a type of CXL device.

  @param  [in]  Controller        The Controller handle of the device.
  @param  [in]  PciIo             The PciIo protocol of the device.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
typedef
EFI_STATUS
(EFIAPI *CXL_TYPE_PARSER)(
  IN  EFI_HANDLE                   Controller,
  IN  EFI_PCI_IO_PROTOCOL          *PciIo
  );

typedef struct CxlParserInfo {
  CONST CHAR8        *ParserName;
  CXL_TYPE_PARSER    Parser;
} CXL_PARSER_INFO;

/**
  Function to register a parser for use by the CXL Driver.

  @param  [in]  Parser            The CXL parser library to register.
  @param  [in]  ParserSkipString  The DTB property to check to see if
                                  the parser should be skipped.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
CxlDxeRegisterTypeParser (
  CXL_PARSER_INFO  *Parser,
  CHAR8            *ParserSkipString
  );

#define EXPAND_MACRO(X)                                     X
#define STR(X)                                              #X
#define CREATE_CXL_PARSER(PARSER)                           {STR(PARSER), EXPAND_MACRO(PARSER)}
#define CONCAT3(a, b, c)                                    a##b##c
#define REGISTER_CXL_PARSER_FUNCTION_NAME(parser_function)  CONCAT3(Register, parser_function, Parser)
#define REGISTER_CXL_PARSER_FUNCTION(PARSER_FUNCTION, PARSER_SKIP_STRING) \
EFIAPI \
EFI_STATUS \
REGISTER_CXL_PARSER_FUNCTION_NAME (PARSER_FUNCTION) (\
  IN EFI_HANDLE        ImageHandle,\
  IN EFI_SYSTEM_TABLE  *SystemTable\
  )\
{\
  EFI_STATUS  Status;\
  CXL_PARSER_INFO Parser = CREATE_CXL_PARSER (PARSER_FUNCTION);\
\
  Status = CxlDxeRegisterTypeParser (&Parser, PARSER_SKIP_STRING);\
  if (EFI_ERROR (Status)) {\
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the %a parser\n", __FUNCTION__, Status, Parser.ParserName));\
    return Status;\
  }\
\
  return EFI_SUCCESS;\
}\

#endif /* __CXL_DXE_H__ */
