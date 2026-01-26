/** @file
  Stub implementations for CompareAndSyncBootOptions tests.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef COMPARE_AND_SYNC_BOOT_OPTIONS_STUB_H_
#define COMPARE_AND_SYNC_BOOT_OPTIONS_STUB_H_

#include <Uefi.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
  Setup mock runtime services and reset state.

**/
VOID
SetupMocks (
  VOID
  );

/**
  Cleanup mock state.

**/
VOID
TeardownMocks (
  VOID
  );

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
  );

/**
  Configure SetVariable mock result for a specific variable.

  @param[in]  VarName     Variable name
  @param[in]  Status      Status to return

**/
VOID
SetMockSetVariableResult (
  IN CONST CHAR16  *VarName,
  IN EFI_STATUS    Status
  );

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
  );

/**
  Configure CreateHttpBootOption mock result.

  @param[in]  Status      Status to return
  @param[in]  OptionNum   Option number to return

**/
VOID
SetMockCreateBootOptionResult (
  IN EFI_STATUS  Status,
  IN UINT16      OptionNum
  );

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
  );

/**
  Check if GetVariable was called for a specific variable.

  @param[in]  VarName     Variable name

  @retval TRUE    Variable was read
  @retval FALSE   Variable was not read

**/
BOOLEAN
WasGetVariableCalled (
  IN CONST CHAR16  *VarName
  );

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
  );

#ifdef __cplusplus
}
#endif

#endif // COMPARE_AND_SYNC_BOOT_OPTIONS_STUB_H_
