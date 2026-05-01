/** @file

  Host-based unit tests for SsdtTableGeneratorParser().

  Tests cover the directly reachable branches of the top-level
  SsdtTableGeneratorParser function:

    1. NULL ParserHandle triggers ASSERT.
    2. NvGetCmGenerationProtocol fails -> parser returns that error.

  All external library functions are stubbed inline.

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Library/DebugLib.h>
#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER  __LINE__
#endif
#include <Library/UnitTestLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

#include <Library/NvCmObjectDescUtility.h>

#include "../SsdtTableGeneratorParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "SsdtTableGeneratorParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static EFI_STATUS  mGetCmGenerationProtocolStatus = EFI_NOT_FOUND;

// ---------------------------------------------------------------------------
// NvCmObjectDescUtility stubs.
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
NvGetCmGenerationProtocol (
  IN  CONST HW_INFO_PARSER_HANDLE           ParserHandle,
  OUT       NVIDIA_AML_GENERATION_PROTOCOL  **ProtocolPtr
  )
{
  return mGetCmGenerationProtocolStatus;
}

EFI_STATUS
EFIAPI
NvAddAcpiTableGenerator (
  IN  CONST HW_INFO_PARSER_HANDLE       ParserHandle,
  IN        CM_STD_OBJ_ACPI_TABLE_INFO  *NewGenerator
  )
{
  return EFI_UNSUPPORTED;
}

// ---------------------------------------------------------------------------
// ConfigurationManagerDataRepoLib stub.
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
ConfigManagerDataRepoRegisterParser (
  PARSER_INFO  *Parser,
  CHAR8        *ParserSkipString
  )
{
  return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Dummy parser handle (HW_INFO_PARSER_HANDLE is VOID *).
// ---------------------------------------------------------------------------

static UINTN  mHandleSentinel = 0xA1C9E53F;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

UNIT_TEST_STATUS
EFIAPI
TestSsdtTableGenerator_NullHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_EXPECT_ASSERT_FAILURE (SsdtTableGeneratorParser (NULL, 0), NULL);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestSsdtTableGenerator_GetProtocolFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mGetCmGenerationProtocolStatus = EFI_NOT_FOUND;
  UT_ASSERT_STATUS_EQUAL (
    SsdtTableGeneratorParser (&mHandleSentinel, 0),
    EFI_NOT_FOUND
    );
  return UNIT_TEST_PASSED;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
UefiTestMain (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      Suite;

  Framework = NULL;
  Status    = InitUnitTestFramework (
                &Framework,
                UNIT_TEST_NAME,
                gEfiCallerBaseName,
                UNIT_TEST_VERSION
                );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CreateUnitTestSuite (
             &Suite,
             Framework,
             "SsdtTableGeneratorParser",
             "SsdtTableGenerator.Parser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "NULL handle triggers ASSERT", "NullHandle", TestSsdtTableGenerator_NullHandle, NULL, NULL, NULL);
  AddTestCase (Suite, "NvGetCmGenerationProtocol failure returns error", "GetProtocolFails", TestSsdtTableGenerator_GetProtocolFails, NULL, NULL, NULL);

  Status = RunAllTestSuites (Framework);

Done:
  if (Framework != NULL) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  return UefiTestMain ();
}
