/** @file

  MmServices Table Lib stubs for host based tests

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MM_SERVICES_TABLE_STUB_LIB_H__
#define __MM_SERVICES_TABLE_STUB_LIB_H__

#include <Library/MmServicesTableLib.h>

/**
  Initialize the MM Services Table.

  @param[In Out] None

  @retval None

**/
VOID
MmServicesTableInit (
  VOID
  );

/**
  DeInitialize the MM Services Table.

  @param[In Out] None

  @retval None

**/
VOID
MmServicesTableDeinit (
  VOID
  );

/**
  Set up mock parameters for MmINstallProtocolInterface () stub

  @param[In]  *ProtocolGuid     Pointer to event GUID to expect.
  @param[In]  MockHandle        Handle to return.
  @param[In]  ReturnStatus      Status to return.


  @retval None

**/
VOID
MockMmInstallProtocolInterface (
  IN EFI_GUID    *ProtocolGuid,
  IN EFI_HANDLE  *MockHandle,
  IN EFI_STATUS  ReturnStatus
  );

/**
  Set up mock parameters for MmLocateProtocolInterface () stub

  @param[In]  *ProtocolGuid     Pointer to event GUID to expect.
  @param[In]  ReturnStatus      Status to return.
  @param[In]  MockInterface     Pointer to expected interface.

  @retval None

**/
VOID
MockMmLocateProtocolInterface (
  IN EFI_GUID    *ProtocolGuid,
  IN EFI_STATUS  ReturnStatus,
  IN VOID        *MockInterface
  );

#endif
