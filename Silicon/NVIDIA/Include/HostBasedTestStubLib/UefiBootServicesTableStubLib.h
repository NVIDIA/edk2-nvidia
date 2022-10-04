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
  Initialize Uefi Boot Services Table stub lib

  @retval None

**/
VOID
UefiBootServicesTableInit (
  VOID
  );

/**
  De-initialize Uefi Boot Services Table stub lib

  @retval None

**/
VOID
UefiBootServicesTableDeinit (
  VOID
  );

#endif
