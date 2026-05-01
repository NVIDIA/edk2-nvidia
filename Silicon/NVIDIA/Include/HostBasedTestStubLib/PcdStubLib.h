/** @file

  Pcd Lib stubs for host based tests

  This library allows non-fixed PCDs to be set during test.  Fixed PCDs cannot
  be mocked as they are implemented as #define'd constants.

  SPDX-FileCopyrightText: Copyright (c) 2023-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCD_STUB_LIB_H__
#define __PCD_STUB_LIB_H__

#include <Library/PcdLib.h>

/**  Initialize Uefi PCD stub support. Call once before running tests.  **/
VOID
UefiPcdInit (
  VOID
  );

/**  Clear Uefi PCD list. Call at the start of a test, before adding values.  **/
VOID
UefiPcdClear (
  VOID
  );

/**  Set the return of LibPcdGet8() for a given token.  **/
VOID
MockLibPcdGet8 (
  IN UINTN  TokenNumber,
  IN UINT8  ReturnValue
  );

/**  Set the return of LibPcdGet16() for a given token.  **/
VOID
MockLibPcdGet16 (
  IN UINTN   TokenNumber,
  IN UINT16  ReturnValue
  );

/**  Set the return of LibPcdGet32() for a given token.  **/
VOID
MockLibPcdGet32 (
  IN UINTN   TokenNumber,
  IN UINT32  ReturnValue
  );

/**  Set the return of LibPcdGet64() for a given token.  **/
VOID
MockLibPcdGet64 (
  IN UINTN   TokenNumber,
  IN UINT64  ReturnValue
  );

/**  Set the return of LibPcdGetBool() for a given token.  **/
VOID
MockLibPcdGetBool (
  IN UINTN    TokenNumber,
  IN BOOLEAN  ReturnValue
  );

#endif
