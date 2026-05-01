/** @file

  Host-based unit tests for NvdlaInfoParser().

  Tests cover the two directly reachable branches of the top-level
  NvdlaInfoParser function:

    1. NvCreateCmObjDesc fails -> NvdlaInfoParser propagates that error.
    2. NvCreateCmObjDesc succeeds but no compatible DT nodes exist
       (DeviceTreeGetNextCompatibleNode returns EFI_NOT_FOUND) ->
       Uid remains 0, ACPI table block is skipped, NvdlaInfoParser
       returns EFI_NOT_FOUND.

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

#include "../NvdlaInfoParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"
#include "../../ResourceTokenUtility.h"

#define UNIT_TEST_NAME     "NvdlaInfoParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static EFI_STATUS  mCreateStatus = EFI_SUCCESS;

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
  if (EFI_ERROR (mCreateStatus)) {
    return mCreateStatus;
  }

  *NewCmObjDesc = AllocateZeroPool (sizeof (CM_OBJ_DESCRIPTOR));
  return (*NewCmObjDesc != NULL) ? EFI_SUCCESS : EFI_OUT_OF_RESOURCES;
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
CreateMemoryRangesObject (
  IN  CONST HW_INFO_PARSER_HANDLE                   ParserHandle,
  IN        INT32                                   NodeOffset,
  IN        INT32                                   ResourceMax,
  OUT       CM_ARCH_COMMON_MEMORY_RANGE_DESCRIPTOR  **MemoryRanges OPTIONAL,
  OUT       UINT32                                  *MemoryRangeCount OPTIONAL,
  OUT       CM_OBJECT_TOKEN                         *Token OPTIONAL
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CreateInterruptsObject (
  IN  CONST HW_INFO_PARSER_HANDLE     ParserHandle,
  IN        INT32                     NodeOffset,
  IN        INT32                     ResourceMax,
  OUT       CM_ARM_GENERIC_INTERRUPT  *Interrupts OPTIONAL,
  OUT       UINT32                    *InterruptCount OPTIONAL,
  OUT       CM_OBJECT_TOKEN           *Token OPTIONAL
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NvExtendCmObj (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  IN        CM_OBJECT_TOKEN        Token,
  OUT       CM_OBJECT_TOKEN        *UpdatedToken OPTIONAL
  )
{
  return EFI_NOT_FOUND;
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
NvAddAcpiTableGenerator (
  IN  CONST HW_INFO_PARSER_HANDLE       ParserHandle,
  IN        CM_STD_OBJ_ACPI_TABLE_INFO  *NewGenerator
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
static UINTN  mHandleSentinel = 0xB5D2F413;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

UNIT_TEST_STATUS
EFIAPI
TestNvdla_CreateDescFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mCreateStatus = EFI_OUT_OF_RESOURCES;
  UT_ASSERT_STATUS_EQUAL (
    NvdlaInfoParser (&mHandleSentinel, 0),
    EFI_OUT_OF_RESOURCES
    );
  mCreateStatus = EFI_SUCCESS;
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestNvdla_NoNodesFound (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mCreateStatus = EFI_SUCCESS;
  UT_ASSERT_STATUS_EQUAL (
    NvdlaInfoParser (&mHandleSentinel, 0),
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
             "NvdlaInfoParser",
             "Nvdla.InfoParser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "CreateDesc failure propagated", "CreateDescFail", TestNvdla_CreateDescFails, NULL, NULL, NULL);
  AddTestCase (Suite, "No nodes returns EFI_NOT_FOUND", "NoNodesFound", TestNvdla_NoNodesFound, NULL, NULL, NULL);

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
