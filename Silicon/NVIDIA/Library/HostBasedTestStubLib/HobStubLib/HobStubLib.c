/** @file

  Hob Lib stubs for host based tests

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <HostBasedTestStubLib/HobStubLib.h>

VOID *
EFIAPI
GetFirstGuidHob (
  IN CONST EFI_GUID  *Guid
  )
{
  check_expected (Guid);

  return (VOID *)mock ();
}

VOID
EFIAPI
MockGetFirstGuidHob (
  IN CONST EFI_GUID  *Guid,
  IN VOID            *Ptr
  )
{
  expect_value (GetFirstGuidHob, Guid, Guid);
  will_return (GetFirstGuidHob, Ptr);
}
