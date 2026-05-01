/** @file

  Host-based unit tests for TpmInfoParser().

  Tests cover the directly reachable branches of the top-level
  TpmInfoParser function:

    1. NULL ParserHandle triggers ASSERT.
    2. PcdGetBool(PcdTpmEnable) returns FALSE (PcdStubLib default) ->
       parser returns EFI_SUCCESS before calling Tpm2GetCapabilityManufactureID.

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

#include "../TpmInfoParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "TpmInfoParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// AML code symbol required by TpmInfoParser.c (referenced but not
// dereferenced in the PcdTpmEnable=FALSE code path).
// ---------------------------------------------------------------------------

unsigned char  ssdttpm_th500_aml_code[] = { 0 };

// ---------------------------------------------------------------------------
// Tpm2CommandLib stub (linker only; never called when PcdTpmEnable=FALSE).
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
Tpm2GetCapabilityManufactureID (
  OUT UINT32  *ManufacturerID
  )
{
  return EFI_UNSUPPORTED;
}

// ---------------------------------------------------------------------------
// TpmMeasurementLib stub (linker only; never called when PcdTpmEnable=FALSE).
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
TpmMeasureAndLogData (
  IN UINT32  PcrIndex,
  IN UINT32  EventType,
  IN VOID    *EventLog,
  IN UINT32  LogLen,
  IN VOID    *HashData,
  IN UINTN   HashDataLen
  )
{
  return EFI_UNSUPPORTED;
}

// ---------------------------------------------------------------------------
// NvCmObjectDescUtility stub (linker only; never called in these paths).
// ---------------------------------------------------------------------------

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
TestTpmInfo_NullHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_EXPECT_ASSERT_FAILURE (TpmInfoParser (NULL, 0), NULL);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestTpmInfo_TpmDisabled (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  // PcdStubLib returns FALSE for PcdGetBool -> parser returns EFI_SUCCESS.
  UT_ASSERT_STATUS_EQUAL (
    TpmInfoParser (&mHandleSentinel, 0),
    EFI_SUCCESS
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
             "TpmInfoParser",
             "TpmInfo.Parser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "NULL handle triggers ASSERT", "NullHandle", TestTpmInfo_NullHandle, NULL, NULL, NULL);
  AddTestCase (Suite, "PcdTpmEnable=FALSE returns EFI_SUCCESS", "TpmDisabled", TestTpmInfo_TpmDisabled, NULL, NULL, NULL);

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
