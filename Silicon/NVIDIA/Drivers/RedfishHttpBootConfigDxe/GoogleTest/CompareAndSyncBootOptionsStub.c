/** @file
  Stub implementations for CompareAndSyncBootOptions tests.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include "CompareAndSyncBootOptionsStub.h"

//
// Mock state tracking for GetVariable
//
typedef struct {
  CHAR16        VarName[256];
  EFI_STATUS    ReturnStatus;
  UINT8         Data[512];
  UINTN         DataSize;
  BOOLEAN       Called;
} MOCK_GET_VARIABLE_ENTRY;

//
// Mock state tracking for SetVariable
//
typedef struct {
  CHAR16        VarName[256];
  EFI_STATUS    ReturnStatus;
  UINTN         LastSize;
  BOOLEAN       Called;
} MOCK_SET_VARIABLE_ENTRY;

static MOCK_GET_VARIABLE_ENTRY  gMockGetVarTable[10];
static MOCK_SET_VARIABLE_ENTRY  gMockSetVarTable[10];
static UINTN                    gMockGetVarCount = 0;
static UINTN                    gMockSetVarCount = 0;

//
// Mock ParseHttpBootUri state
//
static EFI_STATUS       gMockParseStatus = EFI_SUCCESS;
static EFI_MAC_ADDRESS  gMockParseMac;
static BOOLEAN          gMockParseOnce = FALSE;
static CHAR16           *gMockParseUri = NULL;

//
// Mock CreateHttpBootOption state
//
static EFI_STATUS       gMockCreateStatus    = EFI_SUCCESS;
static UINT16           gMockCreateOptionNum = 0x8C7D;
static BOOLEAN          gMockCreateCalled    = FALSE;
static EFI_MAC_ADDRESS  gMockCreateMacAddr;
static CHAR16           gMockCreateUri[512];

//
// Mock Runtime Services table
//
static EFI_RUNTIME_SERVICES  gMockRuntimeServices;

//
// Global runtime services pointer (set by SetupMocks)
//
EFI_RUNTIME_SERVICES  *gRT;

//
// Mock GetVariable implementation
//
static
EFI_STATUS
EFIAPI
MockGetVariable (
  IN     CHAR16    *VariableName,
  IN     EFI_GUID  *VendorGuid,
  OUT    UINT32    *Attributes OPTIONAL,
  IN OUT UINTN     *DataSize,
  OUT    VOID      *Data OPTIONAL
  )
{
  UINTN  i;

  for (i = 0; i < gMockGetVarCount; i++) {
    if (StrCmp (VariableName, gMockGetVarTable[i].VarName) == 0) {
      gMockGetVarTable[i].Called = TRUE;

      if (EFI_ERROR (gMockGetVarTable[i].ReturnStatus)) {
        if (gMockGetVarTable[i].ReturnStatus == EFI_BUFFER_TOO_SMALL) {
          *DataSize = gMockGetVarTable[i].DataSize;
        }

        return gMockGetVarTable[i].ReturnStatus;
      }

      if (*DataSize < gMockGetVarTable[i].DataSize) {
        *DataSize = gMockGetVarTable[i].DataSize;
        return EFI_BUFFER_TOO_SMALL;
      }

      *DataSize = gMockGetVarTable[i].DataSize;
      if ((Data != NULL) && (gMockGetVarTable[i].DataSize > 0)) {
        CopyMem (Data, gMockGetVarTable[i].Data, gMockGetVarTable[i].DataSize);
      }

      return gMockGetVarTable[i].ReturnStatus;
    }
  }

  return EFI_NOT_FOUND;
}

//
// Mock SetVariable implementation
//
static
EFI_STATUS
EFIAPI
MockSetVariable (
  IN CHAR16    *VariableName,
  IN EFI_GUID  *VendorGuid,
  IN UINT32    Attributes,
  IN UINTN     DataSize,
  IN VOID      *Data
  )
{
  UINTN  i;

  for (i = 0; i < gMockSetVarCount; i++) {
    if (StrCmp (VariableName, gMockSetVarTable[i].VarName) == 0) {
      gMockSetVarTable[i].Called   = TRUE;
      gMockSetVarTable[i].LastSize = DataSize;
      return gMockSetVarTable[i].ReturnStatus;
    }
  }

  // Default: add new entry if space available
  if (gMockSetVarCount < 10) {
    StrCpyS (gMockSetVarTable[gMockSetVarCount].VarName, 256, VariableName);
    gMockSetVarTable[gMockSetVarCount].Called       = TRUE;
    gMockSetVarTable[gMockSetVarCount].LastSize     = DataSize;
    gMockSetVarTable[gMockSetVarCount].ReturnStatus = EFI_SUCCESS;
    gMockSetVarCount++;
  }

  return EFI_SUCCESS;
}

/**
  Setup mock runtime services and reset state.

**/
VOID
SetupMocks (
  VOID
  )
{
  ZeroMem (&gMockRuntimeServices, sizeof (gMockRuntimeServices));
  gMockRuntimeServices.GetVariable = MockGetVariable;
  gMockRuntimeServices.SetVariable = MockSetVariable;
  gRT                              = &gMockRuntimeServices;

  gMockGetVarCount = 0;
  gMockSetVarCount = 0;
  ZeroMem (gMockGetVarTable, sizeof (gMockGetVarTable));
  ZeroMem (gMockSetVarTable, sizeof (gMockSetVarTable));

  gMockParseStatus = EFI_SUCCESS;
  ZeroMem (&gMockParseMac, sizeof (gMockParseMac));
  gMockParseOnce = FALSE;
  gMockParseUri  = NULL;

  gMockCreateStatus    = EFI_SUCCESS;
  gMockCreateOptionNum = 0x8C7D;
  gMockCreateCalled    = FALSE;
  ZeroMem (&gMockCreateMacAddr, sizeof (gMockCreateMacAddr));
  ZeroMem (gMockCreateUri, sizeof (gMockCreateUri));
}

/**
  Cleanup mock state.

**/
VOID
TeardownMocks (
  VOID
  )
{
  // Nothing to cleanup currently
}

/**
  Configure GetVariable mock result for a specific variable.

  @param[in]  VarName     Variable name
  @param[in]  Status      Status to return
  @param[in]  Data        Data to return (copied)
  @param[in]  Size        Size of data

**/
VOID
SetMockGetVariableResult (
  IN CONST CHAR16  *VarName,
  IN EFI_STATUS    Status,
  IN VOID          *Data,
  IN UINTN         Size
  )
{
  if (gMockGetVarCount < 10) {
    StrCpyS (gMockGetVarTable[gMockGetVarCount].VarName, 256, VarName);
    gMockGetVarTable[gMockGetVarCount].ReturnStatus = Status;
    gMockGetVarTable[gMockGetVarCount].DataSize     = Size;
    if ((Data != NULL) && (Size > 0) && (Size <= sizeof (gMockGetVarTable[gMockGetVarCount].Data))) {
      CopyMem (gMockGetVarTable[gMockGetVarCount].Data, Data, Size);
    }

    gMockGetVarTable[gMockGetVarCount].Called = FALSE;
    gMockGetVarCount++;
  }
}

/**
  Configure SetVariable mock result for a specific variable.

  @param[in]  VarName     Variable name
  @param[in]  Status      Status to return

**/
VOID
SetMockSetVariableResult (
  IN CONST CHAR16  *VarName,
  IN EFI_STATUS    Status
  )
{
  if (gMockSetVarCount < 10) {
    StrCpyS (gMockSetVarTable[gMockSetVarCount].VarName, 256, VarName);
    gMockSetVarTable[gMockSetVarCount].ReturnStatus = Status;
    gMockSetVarTable[gMockSetVarCount].Called       = FALSE;
    gMockSetVarCount++;
  }
}

/**
  Configure ParseHttpBootUri mock result.

  @param[in]  Status      Status to return
  @param[in]  Mac         MAC address to return (can be NULL)
  @param[in]  Once        "once" flag to return
  @param[in]  Uri         URI pointer to return

**/
VOID
SetMockParseResult (
  IN EFI_STATUS       Status,
  IN EFI_MAC_ADDRESS  *Mac,
  IN BOOLEAN          Once,
  IN CHAR16           *Uri
  )
{
  gMockParseStatus = Status;
  if (Mac != NULL) {
    CopyMem (&gMockParseMac, Mac, sizeof (EFI_MAC_ADDRESS));
  } else {
    ZeroMem (&gMockParseMac, sizeof (EFI_MAC_ADDRESS));
  }

  gMockParseOnce = Once;
  gMockParseUri  = Uri;
}

/**
  Configure CreateHttpBootOption mock result.

  @param[in]  Status      Status to return
  @param[in]  OptionNum   Option number to return

**/
VOID
SetMockCreateBootOptionResult (
  IN EFI_STATUS  Status,
  IN UINT16      OptionNum
  )
{
  gMockCreateStatus    = Status;
  gMockCreateOptionNum = OptionNum;
}

/**
  Check if SetVariable was called with expected parameters.

  @param[in]  VarName         Variable name
  @param[in]  ExpectedSize    Expected size parameter

  @retval TRUE    Variable was set with expected size
  @retval FALSE   Variable was not set or size doesn't match

**/
BOOLEAN
WasSetVariableCalled (
  IN CONST CHAR16  *VarName,
  IN UINTN         ExpectedSize
  )
{
  UINTN  i;

  for (i = 0; i < gMockSetVarCount; i++) {
    if (StrCmp (VarName, gMockSetVarTable[i].VarName) == 0) {
      return gMockSetVarTable[i].Called && (gMockSetVarTable[i].LastSize == ExpectedSize);
    }
  }

  return FALSE;
}

/**
  Check if GetVariable was called for a specific variable.

  @param[in]  VarName     Variable name

  @retval TRUE    Variable was read
  @retval FALSE   Variable was not read

**/
BOOLEAN
WasGetVariableCalled (
  IN CONST CHAR16  *VarName
  )
{
  UINTN  i;

  for (i = 0; i < gMockGetVarCount; i++) {
    if (StrCmp (VarName, gMockGetVarTable[i].VarName) == 0) {
      return gMockGetVarTable[i].Called;
    }
  }

  return FALSE;
}

/**
  Check if CreateHttpBootOption was called and retrieve the parameters.

  @param[out]  MacAddr     MAC address that was passed (optional)
  @param[out]  Uri         URI that was passed (optional)

  @retval TRUE    CreateHttpBootOption was called
  @retval FALSE   CreateHttpBootOption was not called

**/
BOOLEAN
WasCreateHttpBootOptionCalled (
  OUT EFI_MAC_ADDRESS  *MacAddr OPTIONAL,
  OUT CHAR16           **Uri OPTIONAL
  )
{
  if (!gMockCreateCalled) {
    return FALSE;
  }

  if (MacAddr != NULL) {
    CopyMem (MacAddr, &gMockCreateMacAddr, sizeof (EFI_MAC_ADDRESS));
  }

  if (Uri != NULL) {
    *Uri = gMockCreateUri;
  }

  return TRUE;
}

//
// Stub implementations of functions used by CompareAndSyncBootOptions
//

/**
  Stub implementation of ParseHttpBootUri.

**/
EFI_STATUS
ParseHttpBootUri (
  IN  CHAR16           *UriString,
  OUT EFI_MAC_ADDRESS  *MacAddr,
  OUT BOOLEAN          *Once,
  OUT CHAR16           **Uri
  )
{
  if (EFI_ERROR (gMockParseStatus)) {
    return gMockParseStatus;
  }

  CopyMem (MacAddr, &gMockParseMac, sizeof (EFI_MAC_ADDRESS));
  *Once = gMockParseOnce;
  *Uri  = gMockParseUri;

  return gMockParseStatus;
}

/**
  Stub implementation of CreateHttpBootOption.

**/
EFI_STATUS
CreateHttpBootOption (
  IN  EFI_MAC_ADDRESS  *MacAddr,
  IN  CONST CHAR16     *Uri,
  OUT UINT16           *OptionNum
  )
{
  // Capture parameters
  gMockCreateCalled = TRUE;
  CopyMem (&gMockCreateMacAddr, MacAddr, sizeof (EFI_MAC_ADDRESS));
  StrCpyS (gMockCreateUri, 512, Uri);

  if (EFI_ERROR (gMockCreateStatus)) {
    return gMockCreateStatus;
  }

  *OptionNum = gMockCreateOptionNum;
  return gMockCreateStatus;
}
