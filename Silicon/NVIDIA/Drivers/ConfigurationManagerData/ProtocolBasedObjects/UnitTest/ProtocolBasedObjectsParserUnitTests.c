/** @file

  Host-based unit tests for ProtocolBasedObjectsParser().

  Tests cover the directly reachable branches of the top-level
  ProtocolBasedObjectsParser function:

    1. NULL ParserHandle triggers ASSERT.
    2. EfiLocateProtocolBuffer returns EFI_NOT_FOUND -> zero protocols,
       loop does not execute, parser returns EFI_SUCCESS.

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

#include "../ProtocolBasedObjectsParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "ProtocolBasedObjectsParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// GUID definition (used by EfiLocateProtocolBuffer stub; never compared).
// ---------------------------------------------------------------------------

EFI_GUID  gNVIDIAConfigurationManagerDataObjectGuid = {
  0xac9d2c92, 0xd226, 0x469d, { 0x9a, 0xb4, 0x9f, 0x74, 0x91, 0xaa, 0x57, 0xcb }
};

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static EFI_STATUS  mLocateProtocolBufferStatus = EFI_NOT_FOUND;

// ---------------------------------------------------------------------------
// UefiLib stub.
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
EfiLocateProtocolBuffer (
  IN  EFI_GUID  *Protocol,
  OUT UINTN     *NoProtocols,
  OUT VOID      ***Buffer
  )
{
  if (mLocateProtocolBufferStatus == EFI_SUCCESS) {
    *NoProtocols = 0;
    *Buffer      = NULL;
  }

  return mLocateProtocolBufferStatus;
}

// ---------------------------------------------------------------------------
// NvCmObjectDescUtility stubs (linker only; never called in these paths).
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
NvFindEntry (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        CM_OBJECT_ID           ObjectId,
  IN        CM_OBJECT_TOKEN        Token,
  OUT       CM_OBJ_DESCRIPTOR      **DescPtr
  )
{
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
NvAddMultipleCmObjWithTokens (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  IN        CM_OBJECT_TOKEN        *ElementTokenMap,
  IN        CM_OBJECT_TOKEN        Token
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
NvAddMultipleCmObjGetTokens (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  OUT       CM_OBJECT_TOKEN        **TokenMapPtr   OPTIONAL,
  OUT       CM_OBJECT_TOKEN        *TokenPtr OPTIONAL
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
TestProtocolBasedObjects_NullHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_EXPECT_ASSERT_FAILURE (ProtocolBasedObjectsParser (NULL, 0), NULL);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestProtocolBasedObjects_NoProtocols (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mLocateProtocolBufferStatus = EFI_NOT_FOUND;
  UT_ASSERT_STATUS_EQUAL (
    ProtocolBasedObjectsParser (&mHandleSentinel, 0),
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
             "ProtocolBasedObjectsParser",
             "ProtocolBasedObjects.Parser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "NULL handle triggers ASSERT", "NullHandle", TestProtocolBasedObjects_NullHandle, NULL, NULL, NULL);
  AddTestCase (Suite, "No protocols returns EFI_SUCCESS", "NoProtocols", TestProtocolBasedObjects_NoProtocols, NULL, NULL, NULL);

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
