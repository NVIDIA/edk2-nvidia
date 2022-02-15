/** @file

  UEFI Runtime Services Table Lib stubs for host based tests

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef __UEFI_RUNTIME_SERVICES_TABLE_STUB_LIB_H__
#define __UEFI_RUNTIME_SERVICES_TABLE_STUB_LIB_H__

#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>

/**
  Set up mock parameters for UefiGetVariable() stub

  @param[In]  Name                  Variable name to mock
  @param[In]  Guid                  Variable guid to mock
  @param[In]  ReturnStatus          Status to return

  @retval None

**/
VOID
MockUefiGetVariable (
  IN  CHAR16        *Name,
  IN  EFI_GUID      *Guid,
  IN  EFI_STATUS    ReturnStatus
  );

/**
  Set up mock parameters for UefiSetVariable() stub

  @param[In]  Name                  Variable name to mock
  @param[In]  Guid                  Variable guid to mock
  @param[In]  ReturnStatus          Status to return

  @retval None

**/
VOID
MockUefiSetVariable (
  IN  CHAR16        *Name,
  IN  EFI_GUID      *Guid,
  IN  EFI_STATUS    ReturnStatus
  );

/**
  Initialize Uefi Runtime Services Table stub lib

  @retval None

**/
VOID
UefiRuntimeServicesTableInit (
  IN  BOOLEAN   PreserveVariables
  );

/**
  De-initialize Uefi Runtime Services Table stub lib

  @retval None

**/
VOID
UefiRuntimeServicesTableDeinit (
  IN  BOOLEAN   PreserveVariables
  );

#endif
