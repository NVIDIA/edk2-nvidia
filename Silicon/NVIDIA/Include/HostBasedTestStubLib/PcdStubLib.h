/** @file

  Pcd Lib stubs for host based tests

  This library allows non-fixed PCDs to be set during test.  Fixed PCDs cannot
  be mocked as they are implemented as #define'd constants.

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCD_STUB_LIB_H__
#define __PCD_STUB_LIB_H__

#include <Library/PcdLib.h>

/**
  Initialize Uefi PCD stub support.

  This should be called once before running tests.

  @retval None

**/
VOID
UefiPcdInit (
  VOID
  );

/**
  Clear Uefi PCD list

  This should be called at the start of a test, before adding PCD values.

  @retval None

**/
VOID
UefiPcdClear (
  VOID
  );

/**
  Set the return of LibPcdGetBool ().

  @param[In]  ExpectedTokenNumber   PCD request expected
  @param[In]  ReturnValue           Value to return when requested

  @retval None
**/
VOID
MockLibPcdGetBool (
  IN UINTN    TokenNumber,
  IN BOOLEAN  ReturnValue
  );

#endif
