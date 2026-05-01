/** @file

  Host-based unit tests for EthernetInfoParser().

  Tests cover the directly reachable branches of the top-level
  EthernetInfoParser function:

    1. NULL ParserHandle triggers ASSERT.
    2. Non-VDK platform -> parser returns EFI_SUCCESS immediately
       (Ethernet SSDT is only installed on VDK).

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

#include <Library/TegraPlatformInfoLib.h>
#include <Library/NvCmObjectDescUtility.h>

#include "../EthernetInfoParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "EthernetInfoParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static TEGRA_PLATFORM_TYPE  mPlatform = TEGRA_PLATFORM_SILICON;

// ---------------------------------------------------------------------------
// AML code array stub referenced as extern in EthernetInfoParser.c.
// ---------------------------------------------------------------------------

unsigned char  ssdteth_aml_code[] = { 0 };

// ---------------------------------------------------------------------------
// TegraPlatformInfoLib stub.
// ---------------------------------------------------------------------------

TEGRA_PLATFORM_TYPE
TegraGetPlatform (
  VOID
  )
{
  return mPlatform;
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
TestEthernetInfo_NullHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UT_EXPECT_ASSERT_FAILURE (EthernetInfoParser (NULL, 0), NULL);
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestEthernetInfo_NonVdkPlatform (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mPlatform = TEGRA_PLATFORM_SILICON;
  UT_ASSERT_STATUS_EQUAL (
    EthernetInfoParser (&mHandleSentinel, 0),
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
             "EthernetInfoParser",
             "EthernetInfo.InfoParser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "NULL handle triggers ASSERT", "NullHandle", TestEthernetInfo_NullHandle, NULL, NULL, NULL);
  AddTestCase (Suite, "Non-VDK platform returns EFI_SUCCESS", "NonVdkPlatform", TestEthernetInfo_NonVdkPlatform, NULL, NULL, NULL);

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
