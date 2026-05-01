/** @file

  Host-based unit tests for IpmiParser().

  Tests cover the directly reachable branches of the top-level IpmiParser
  function:

    1. DeviceTreeGetCompatibleNodeCount returns an error ->
         IpmiParser propagates that error.
    2. DeviceTreeGetCompatibleNodeCount succeeds with one node,
       but NvAddMultipleCmObjWithTokens fails ->
         IpmiParser propagates EFI_ABORTED.

  DeviceTreeGetNextCompatibleNode is stubbed to return EFI_NOT_FOUND so
  the SSIF node-iteration loop is skipped in both test cases.

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
#include <Library/DeviceTreeHelperLib.h>

#include "../IpmiParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "IpmiParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static EFI_STATUS  mCountStatus        = EFI_NOT_FOUND;
static UINT32      mNodeCount          = 0;
static EFI_STATUS  mNvAddObjWithStatus = EFI_SUCCESS;

EFI_STATUS
EFIAPI
DeviceTreeGetCompatibleNodeCount (
  IN      CONST CHAR8  **CompatibleInfo,
  OUT UINT32           *NodeCount
  )
{
  *NodeCount = mNodeCount;
  return mCountStatus;
}

EFI_STATUS
EFIAPI
DeviceTreeGetNextCompatibleNode (
  IN      CONST CHAR8  **CompatibleInfo,
  IN OUT        INT32  *NodeOffset
  )
{
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
DeviceTreeGetNodePropertyValue32 (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *Property,
  OUT UINT32       *PropertyValue
  )
{
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
NvAllocateCmTokens (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        UINT32                 Count,
  OUT       CM_OBJECT_TOKEN        **TokenMapPtr
  )
{
  *TokenMapPtr = AllocateZeroPool (Count * sizeof (CM_OBJECT_TOKEN));
  return (*TokenMapPtr != NULL) ? EFI_SUCCESS : EFI_OUT_OF_RESOURCES;
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
  return mNvAddObjWithStatus;
}

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
static UINTN  mHandleSentinel = 0x1421;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

UNIT_TEST_STATUS
EFIAPI
TestIpmi_CountFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mCountStatus = EFI_DEVICE_ERROR;
  mNodeCount   = 0;
  UT_ASSERT_STATUS_EQUAL (
    IpmiParser (&mHandleSentinel, 0),
    EFI_DEVICE_ERROR
    );
  mCountStatus = EFI_NOT_FOUND;
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestIpmi_AddObjectFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mCountStatus        = EFI_SUCCESS;
  mNodeCount          = 1;
  mNvAddObjWithStatus = EFI_ABORTED;
  UT_ASSERT_STATUS_EQUAL (
    IpmiParser (&mHandleSentinel, 0),
    EFI_ABORTED
    );
  mCountStatus        = EFI_NOT_FOUND;
  mNodeCount          = 0;
  mNvAddObjWithStatus = EFI_SUCCESS;
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
             "IpmiParser",
             "Ipmi.Parser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "Count failure propagated", "CountFail", TestIpmi_CountFails, NULL, NULL, NULL);
  AddTestCase (Suite, "AddObject failure propagated", "AddObjectFail", TestIpmi_AddObjectFails, NULL, NULL, NULL);

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
