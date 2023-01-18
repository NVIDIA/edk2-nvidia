/** @file

  FwVariableLib - Firmware variable support library

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FW_VARIABLE_LIB_H__
#define __FW_VARIABLE_LIB_H__

#include <Uefi/UefiBaseType.h>

#define MAX_VARIABLE_NAME  (256 * sizeof(CHAR16))

/**
  Delete all Firmware Variables

  @retval         EFI_SUCCESS          All variables deleted
  @retval         EFI_OUT_OF_RESOURCES Unable to allocate space for variable names
  @retval         others               Error deleting or getting next variable

**/
EFI_STATUS
EFIAPI
FwVariableDeleteAll (
  );

#endif
