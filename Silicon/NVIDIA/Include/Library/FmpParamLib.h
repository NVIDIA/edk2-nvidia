/** @file

  FMP parameter library

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FMP_PARAM_LIB_H__
#define __FMP_PARAM_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  Get lowest supported version from PCD or DTB.

  @param[in]  Lsv                   Pointer to return lowest supported version.

  @retval none

**/
EFI_STATUS
EFIAPI
FmpParamGetLowestSupportedVersion (
  OUT UINT32  *Lsv
  );

/**
  Initialize FMP parameter library.  Must be called before any other library
  API is used.

  @retval none

**/
VOID
EFIAPI
FmpParamLibInit (
  VOID
  );

#endif
