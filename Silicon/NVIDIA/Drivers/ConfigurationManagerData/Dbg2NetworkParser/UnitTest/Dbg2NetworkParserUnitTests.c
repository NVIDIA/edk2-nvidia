/** @file

  Host-based unit tests for Dbg2NetworkParser().

  Tests cover the directly reachable branches of the top-level
  Dbg2NetworkParser function:

    1. NULL ParserHandle triggers ASSERT.
    2. NvCreateCmObjDesc fails -> parser returns that error before
       accessing gRT (which is NULL in host context).

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

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Library/NvCmObjectDescUtility.h>

#include "../Dbg2NetworkParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "Dbg2NetworkParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Global boot/runtime services (unused in tested paths; must be defined).
// ---------------------------------------------------------------------------

EFI_BOOT_SERVICES     *gBS = NULL;
EFI_RUNTIME_SERVICES  *gRT = NULL;
EFI_SYSTEM_TABLE      *gST = NULL;

// ---------------------------------------------------------------------------
// GUID definitions (never compared in these test paths).
// ---------------------------------------------------------------------------

EFI_GUID  gNVIDIAPublicVariableGuid = {
  0x781e084c, 0xa330, 0x417c, { 0xb6, 0x78, 0x38, 0xe6, 0x96, 0x38, 0x0c, 0xb9 }
};
EFI_GUID  gEfiPciIoProtocolGuid = {
  0x4CF5B200, 0x68B8, 0x4CA5, { 0x9E, 0xEC, 0xB2, 0x3E, 0x3F, 0x50, 0x02, 0x9A }
};

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static EFI_STATUS  mCreateCmObjDescStatus = EFI_OUT_OF_RESOURCES;

// ---------------------------------------------------------------------------
// NvCmObjectDescUtility stubs.
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
NvCreateCmObjDesc (
  IN  CM_OBJECT_ID       ObjectId,
  IN  UINT32             Count,
  IN  VOID               *Data,
  IN  UINT32             Size,
  OUT CM_OBJ_DESCRIPTOR  **NewCmObjDesc
  )
{
  return mCreateCmObjDescStatus;
}

EFI_STATUS
EFIAPI
NvAddMultipleCmObjGetTokens (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  OUT       CM_OBJECT_TOKEN        **TokenMapPtr   OPTIONAL,
  OUT       CM_OBJECT_TOKEN        *TokenPtr OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
NvExtendCmObj (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  IN        CM_OBJECT_TOKEN        Token    OPTIONAL,
  OUT       CM_OBJECT_TOKEN        **TokenMapPtr   OPTIONAL
  )
{
  return EFI_NOT_FOUND;
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
TestDbg2Network_NullHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_EXPECT_ASSERT_FAILURE (Dbg2NetworkParser (NULL, 0), NULL);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestDbg2Network_CreateCmObjDescFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mCreateCmObjDescStatus = EFI_OUT_OF_RESOURCES;
  UT_ASSERT_STATUS_EQUAL (
    Dbg2NetworkParser (&mHandleSentinel, 0),
    EFI_OUT_OF_RESOURCES
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
             "Dbg2NetworkParser",
             "Dbg2Network.Parser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "NULL handle triggers ASSERT", "NullHandle", TestDbg2Network_NullHandle, NULL, NULL, NULL);
  AddTestCase (Suite, "NvCreateCmObjDesc failure returns error", "CreateCmObjDescFails", TestDbg2Network_CreateCmObjDescFails, NULL, NULL, NULL);

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
