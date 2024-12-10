/** @file

  MmServices Table Lib stubs for host based tests

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stddef.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <HostBasedTestStubLib/MmServicesTableStubLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <PiMm.h>
#include <Protocol/MmBase.h>
#include <Library/MmServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

EFI_MM_SYSTEM_TABLE  *gMmst = NULL;

STATIC
EFI_STATUS
EFIAPI
MmLocateProtocolInterfaceStub (
  IN   EFI_GUID  *Protocol,
  IN   VOID      *Registration OPTIONAL,
  OUT  VOID      **Interface
  )
{
  EFI_STATUS  Status;
  VOID        *InterfacePtr;

  check_expected_ptr (Protocol);

  Status       = (EFI_STATUS)mock ();
  InterfacePtr = (VOID *)mock ();
  if (!EFI_ERROR (Status)) {
    *Interface = InterfacePtr;
  }

  return Status;
}

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
  )
{
  expect_value (MmLocateProtocolInterfaceStub, Protocol, ProtocolGuid);
  will_return (MmLocateProtocolInterfaceStub, ReturnStatus);
  will_return (MmLocateProtocolInterfaceStub, MockInterface);
}

STATIC
EFI_STATUS
EFIAPI
MmInstallProtocolInterfaceStub (
  IN OUT EFI_HANDLE      *UserHandle,
  IN EFI_GUID            *Protocol,
  IN EFI_INTERFACE_TYPE  InterfaceType,
  IN VOID                *Interface
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handle;

  check_expected_ptr (Protocol);

  Status = (EFI_STATUS)mock ();
  Handle = (EFI_HANDLE *)mock ();
  if (!EFI_ERROR (Status) && (*UserHandle == NULL)) {
    *UserHandle = Handle;
  }

  return Status;
}

/**
  Set up mock parameters for MmInstallProtocolInterface () stub

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
  )
{
  expect_value (MmInstallProtocolInterfaceStub, Protocol, ProtocolGuid);
  will_return (MmInstallProtocolInterfaceStub, ReturnStatus);
  will_return (MmInstallProtocolInterfaceStub, MockHandle);
}

/**
  Initialize the MM Services Table.

  @param[In Out] None

  @retval None

**/
VOID
MmServicesTableInit (
  VOID
  )
{
  ASSERT (gMmst == NULL);
  gMmst = AllocateZeroPool (sizeof (*gMmst));

  gMmst->MmInstallProtocolInterface = MmInstallProtocolInterfaceStub;
  gMmst->MmLocateProtocol           = MmLocateProtocolInterfaceStub;
}

/**
  DeInitialize the MM Services Table.

  @param[In Out] None

  @retval None

**/
VOID
MmServicesTableDeinit (
  VOID
  )
{
  FreePool (gMmst);
  gMmst = NULL;
}
