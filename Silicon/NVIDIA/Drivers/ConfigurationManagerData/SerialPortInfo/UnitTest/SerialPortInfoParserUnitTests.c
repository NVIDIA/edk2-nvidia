/** @file

  Host-based unit tests for SerialPortInfoParser().

  Tests cover the directly reachable branches of the top-level
  SerialPortInfoParser function:

    1. NULL ParserHandle triggers ASSERT.
    2. PcdSerialPortConfig and PcdSerialTypeConfig both default to 0
       (SBSA type, not disabled), DeviceTreeGetCompatibleNodeCount
       returns EFI_NOT_FOUND -> parser propagates EFI_NOT_FOUND.

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

#include "../SerialPortInfoParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "SerialPortInfoParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static EFI_STATUS  mCountStatus = EFI_NOT_FOUND;
static UINT32      mNodeCount   = 0;

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
DeviceTreeGetRegisters (
  IN INT32                              NodeOffset,
  OUT NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray OPTIONAL,
  IN OUT UINT32                         *NumberOfRegisters
  )
{
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
DeviceTreeGetInterrupts (
  IN INT32                               NodeOffset,
  OUT NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptArray OPTIONAL,
  IN OUT UINT32                          *NumberOfInterrupts
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
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NvAddMultipleCmObjGetTokens (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  OUT       CM_OBJECT_TOKEN        **TokenMapPtr OPTIONAL,
  OUT       CM_OBJECT_TOKEN        *TokenPtr OPTIONAL
  )
{
  return EFI_SUCCESS;
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
static UINTN  mHandleSentinel = 0x5E41A190;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

UNIT_TEST_STATUS
EFIAPI
TestSerial_NullHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_EXPECT_ASSERT_FAILURE (SerialPortInfoParser (NULL, 0), NULL);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestSerial_CountFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mCountStatus = EFI_NOT_FOUND;
  mNodeCount   = 0;
  UT_ASSERT_STATUS_EQUAL (
    SerialPortInfoParser (&mHandleSentinel, 0),
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
             "SerialPortInfoParser",
             "SerialPort.InfoParser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "NULL handle triggers ASSERT", "NullHandle", TestSerial_NullHandle, NULL, NULL, NULL);
  AddTestCase (Suite, "Compatible node count failure propagated", "CountFail", TestSerial_CountFails, NULL, NULL, NULL);

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
