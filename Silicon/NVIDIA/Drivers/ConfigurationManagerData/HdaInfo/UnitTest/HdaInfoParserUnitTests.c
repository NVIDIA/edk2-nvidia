/** @file

  Host-based unit tests for HdaInfoParser().

  Tests cover the directly reachable branches of the top-level
  HdaInfoParser function:

    1. AmlParseDefinitionBlock fails -> parser returns that error.
    2. AML setup succeeds; DeviceTreeGetNextCompatibleNode returns
       EFI_NOT_FOUND (no HDA nodes in DT) -> parser returns EFI_SUCCESS
       with no ACPI table installed.

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

#include <Library/AmlLib/AmlLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/NvCmObjectDescUtility.h>

#include "../HdaInfoParser.h"
#include "../../ConfigurationManagerDataRepoLib.h"

#define UNIT_TEST_NAME     "HdaInfoParser Unit Tests"
#define UNIT_TEST_VERSION  "1.0"

// ---------------------------------------------------------------------------
// Configurable stubs.
// ---------------------------------------------------------------------------

static EFI_STATUS  mAmlParseStatus = EFI_UNSUPPORTED;

// Dummy sentinels used as opaque AML node handles.
static UINTN  mDummyRootSentinel = 0xA1B2C3D4;
static UINTN  mDummyNodeSentinel = 0xD4C3B2A1;

// ---------------------------------------------------------------------------
// AML code array stub (referenced as extern in HdaInfoParser.c).
// ---------------------------------------------------------------------------

unsigned char  ssdthda_aml_code[] = { 0 };

// ---------------------------------------------------------------------------
// AmlLib stubs.
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
AmlParseDefinitionBlock (
  IN  CONST EFI_ACPI_DESCRIPTION_HEADER  *DefinitionBlock,
  OUT       AML_ROOT_NODE_HANDLE         *RootPtr
  )
{
  if (EFI_ERROR (mAmlParseStatus)) {
    return mAmlParseStatus;
  }

  *RootPtr = &mDummyRootSentinel;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AmlFindNode (
  IN  AML_NODE_HANDLE  ReferenceNode,
  IN  CONST CHAR8      *AslPath,
  OUT AML_NODE_HANDLE  *OutNode
  )
{
  *OutNode = &mDummyNodeSentinel;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AmlDetachNode (
  IN  AML_NODE_HANDLE  Node
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AmlDeleteTree (
  IN  AML_NODE_HANDLE  Node
  )
{
  return EFI_SUCCESS;
}

// Linker-only stubs for loop-body AML functions (never called in these paths).

EFI_STATUS
EFIAPI
AmlCloneTree (
  IN  AML_NODE_HANDLE  Node,
  OUT AML_NODE_HANDLE  *ClonedNode
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
AmlAttachNode (
  IN  AML_NODE_HANDLE  ParentNode,
  IN  AML_NODE_HANDLE  NewNode
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
AmlDeviceOpUpdateName (
  IN  AML_OBJECT_NODE_HANDLE  DeviceOpNode,
  IN  CONST CHAR8             *NewNameString
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
AmlNameOpUpdateInteger (
  IN  AML_OBJECT_NODE_HANDLE  NameOpNode,
  IN  UINT64                  NewInt
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
AmlCodeGenNameResourceTemplate (
  IN  CONST CHAR8             *NameString,
  IN  AML_NODE_HANDLE         ParentNode  OPTIONAL,
  OUT AML_OBJECT_NODE_HANDLE  *NewObjectNode OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
AmlCodeGenRdMemory32Fixed (
  IN  BOOLEAN                 IsReadWrite,
  IN  UINT32                  Address,
  IN  UINT32                  RangeLength,
  IN  AML_OBJECT_NODE_HANDLE  NameOpNode,
  OUT AML_DATA_NODE_HANDLE    *NewMemNode
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
AmlCodeGenRdInterrupt (
  IN  BOOLEAN                 ResourceConsumer,
  IN  BOOLEAN                 EdgeTriggered,
  IN  BOOLEAN                 ActiveLow,
  IN  BOOLEAN                 Shared,
  IN  UINT32                  *IrqList,
  IN  UINT8                   IrqCount,
  IN  AML_OBJECT_NODE_HANDLE  NameOpNode   OPTIONAL,
  OUT AML_DATA_NODE_HANDLE    *NewRdNode   OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
AmlSerializeDefinitionBlock (
  IN  AML_ROOT_NODE_HANDLE         RootNode,
  OUT EFI_ACPI_DESCRIPTION_HEADER  **Table
  )
{
  return EFI_UNSUPPORTED;
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
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
DeviceTreeGetRegisters (
  IN      INT32                             NodeOffset,
  OUT     NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterArray OPTIONAL,
  IN OUT  UINT32                            *NumberOfRegisters
  )
{
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
DeviceTreeGetInterrupts (
  IN      INT32                              NodeOffset,
  OUT     NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptArray OPTIONAL,
  IN OUT  UINT32                             *NumberOfInterrupts
  )
{
  return EFI_NOT_FOUND;
}

// ---------------------------------------------------------------------------
// NvCmObjectDescUtility stub.
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
TestHda_AmlParseFails (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mAmlParseStatus = EFI_UNSUPPORTED;
  UT_ASSERT_STATUS_EQUAL (
    HdaInfoParser (&mHandleSentinel, 0),
    EFI_UNSUPPORTED
    );
  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestHda_NoDtNodes (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mAmlParseStatus = EFI_SUCCESS;
  UT_ASSERT_STATUS_EQUAL (
    HdaInfoParser (&mHandleSentinel, 0),
    EFI_SUCCESS
    );
  mAmlParseStatus = EFI_UNSUPPORTED;
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
             "HdaInfoParser",
             "Hda.InfoParser",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (Suite, "AML parse failure propagated", "AmlParseFail", TestHda_AmlParseFails, NULL, NULL, NULL);
  AddTestCase (Suite, "No DT nodes returns EFI_SUCCESS", "NoDtNodes", TestHda_NoDtNodes, NULL, NULL, NULL);

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
