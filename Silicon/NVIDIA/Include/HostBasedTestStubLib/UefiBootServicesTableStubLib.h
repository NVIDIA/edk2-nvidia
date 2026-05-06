/** @file

  UEFI Boot Services Table Lib stubs for host based tests

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __UEFI_BOOT_SERVICES_TABLE_STUB_LIB_H__
#define __UEFI_BOOT_SERVICES_TABLE_STUB_LIB_H__

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>

/**
  Set up mock parameters for UefiCreateEventEx() stub

  @param[In]  *EventGroup           Pointer to event GUID to expect
  @param[In]  *EventSavePtr         Pointer to save returned event
  @param[In]  ReturnStatus          Status to return

  @retval None

**/
VOID
MockUefiCreateEventEx (
  IN  EFI_GUID    *EventGroup,
  IN  EFI_EVENT   *EventSavePtr,
  IN  EFI_STATUS  ReturnStatus
  );

/**
  Set up mock parameters for InstallMultipleProtocolInterfaces() stub

  @param[In]  ReturnStatus          Status to return

  @retval None

**/
VOID
EFIAPI
MockInstallMultipleProtocolInterfaces (
  IN  EFI_STATUS  ReturnStatus,
  ...
  );

/**
  Set up mock parameters for UefiLocateProtocol() stub

  @param[In]  ProtocolGuid              Pointer to GUID to expect
  @param[In]  ReturnProtocolInterface   Pointer to save returned interface
  @param[In]  ReturnStatus              Status to return

  @retval None

**/
VOID
MockUefiLocateProtocol (
  IN  EFI_GUID    *ProtocolGuid,
  IN  VOID        *ReturnProtocolInterface,
  IN  EFI_STATUS  ReturnStatus
  );

/**
  Initialize UEFI Boot Services Table stub.

  If the table is already allocated (e.g. a prior test failed before
  UefiBootServicesTableDeinit), this function releases it first so callers
  may invoke Init from test setup without depending on prior cleanup.

  @retval None

**/
VOID
UefiBootServicesTableInit (
  VOID
  );

/**
  De-initialize UEFI Boot Services Table stub.

  No operation if the table was not initialized, so it is safe to call from
  test teardown even when setup was skipped or failed partway.

  @retval None

**/
VOID
UefiBootServicesTableDeinit (
  VOID
  );

#endif
