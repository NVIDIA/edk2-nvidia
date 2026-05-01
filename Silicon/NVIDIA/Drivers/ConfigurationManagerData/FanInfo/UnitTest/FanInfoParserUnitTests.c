/** @file

  Host-based unit tests for FanInfoParser().

  Tests cover the directly reachable branches of the top-level
  FanInfoParser function:

    1. NULL ParserHandle triggers ASSERT.
    2. NvGetCmGenerationProtocol fails -> parser returns EFI_NOT_FOUND.
    3. NvGetCmGenerationProtocol succeeds; NvGetCmPatchProtocol fails ->
       parser returns EFI_NOT_FOUND.
    4. Both protocols acquired; DeviceTreeGetNextCompatibleNode returns
       EFI_NOT_FOUND (no "pwm-fan" node in DT) -> parser returns
       EFI_NOT_FOUND.

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

#include "../FanInfoParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "FanInfoParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static EFI_STATUS  mGenProtoStatus   = EFI_NOT_FOUND;
static EFI_STATUS  mPatchProtoStatus = EFI_NOT_FOUND;
static EFI_STATUS  mDtNodeStatus     = EFI_NOT_FOUND;

// Dummy protocol instances (function pointers NULL; never called in
// these test paths).
static NVIDIA_AML_GENERATION_PROTOCOL  mDummyGenProto;
static NVIDIA_AML_PATCH_PROTOCOL       mDummyPatchProto;

EFI_STATUS
EFIAPI
NvGetCmGenerationProtocol (
  IN  CONST HW_INFO_PARSER_HANDLE           ParserHandle,
  OUT       NVIDIA_AML_GENERATION_PROTOCOL  **ProtocolPtr
  )
{
  if (EFI_ERROR (mGenProtoStatus)) {
    return mGenProtoStatus;
  }

  *ProtocolPtr = &mDummyGenProto;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NvGetCmPatchProtocol (
  IN  CONST HW_INFO_PARSER_HANDLE      ParserHandle,
  OUT       NVIDIA_AML_PATCH_PROTOCOL  **ProtocolPtr
  )
{
  if (EFI_ERROR (mPatchProtoStatus)) {
    return mPatchProtoStatus;
  }

  *ProtocolPtr = &mDummyPatchProto;
  return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// DeviceTreeHelperLib stubs.
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
DeviceTreeGetNextCompatibleNode (
  IN      CONST CHAR8  **CompatibleInfo,
  IN OUT        INT32  *NodeOffset
  )
{
  return mDtNodeStatus;
}

EFI_STATUS
EFIAPI
DeviceTreeGetNodePropertyValue32 (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *PropertyName,
  OUT UINT32       *PropertyValue
  )
{
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
DeviceTreeGetNodeByPHandle (
  IN  UINT32  NodePHandle,
  OUT INT32   *NodeOffset
  )
{
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
DeviceTreeGetRegisters (
  IN INT32                              NodeOffset,
  OUT NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray OPTIONAL,
  IN OUT UINT32                         *NumberOfRegisters
  )
{
  return EFI_NOT_FOUND;
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
TestFan_NullHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_EXPECT_ASSERT_FAILURE (FanInfoParser (NULL, 0), NULL);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestFan_GenProtoFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mGenProtoStatus = EFI_NOT_FOUND;
  UT_ASSERT_STATUS_EQUAL (
    FanInfoParser (&mHandleSentinel, 0),
    EFI_NOT_FOUND
    );
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestFan_PatchProtoFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mGenProtoStatus   = EFI_SUCCESS;
  mPatchProtoStatus = EFI_NOT_FOUND;
  UT_ASSERT_STATUS_EQUAL (
    FanInfoParser (&mHandleSentinel, 0),
    EFI_NOT_FOUND
    );
  mGenProtoStatus = EFI_NOT_FOUND;
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestFan_NoDtNodes (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mGenProtoStatus   = EFI_SUCCESS;
  mPatchProtoStatus = EFI_SUCCESS;
  mDtNodeStatus     = EFI_NOT_FOUND;
  UT_ASSERT_STATUS_EQUAL (
    FanInfoParser (&mHandleSentinel, 0),
    EFI_NOT_FOUND
    );
  mGenProtoStatus   = EFI_NOT_FOUND;
  mPatchProtoStatus = EFI_NOT_FOUND;
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
             "FanInfoParser",
             "Fan.InfoParser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "NULL handle triggers ASSERT", "NullHandle", TestFan_NullHandle, NULL, NULL, NULL);
  AddTestCase (Suite, "Generation protocol failure propagated", "GenProtoFail", TestFan_GenProtoFails, NULL, NULL, NULL);
  AddTestCase (Suite, "Patch protocol failure propagated", "PatchProtoFail", TestFan_PatchProtoFails, NULL, NULL, NULL);
  AddTestCase (Suite, "No DT nodes returns EFI_NOT_FOUND", "NoDtNodes", TestFan_NoDtNodes, NULL, NULL, NULL);

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
