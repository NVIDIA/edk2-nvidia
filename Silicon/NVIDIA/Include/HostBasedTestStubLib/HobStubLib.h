/** @file

  Hob Lib stubs for host based tests

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __HOB_STUB_LIB_H__
#define __HOB_STUB_LIB_H__

#include <PiDxe.h>
#include <Library/HobLib.h>

/**
  Set up mock parameters for GetFirstGuidHob()

  @param[In]  Guid                  Guid to return
  @param[In]  Ptr                   Ptr to return

  @retval None

**/
VOID
EFIAPI
MockGetFirstGuidHob (
  IN CONST EFI_GUID  *Guid,
  IN VOID            *Ptr
  );

#endif
