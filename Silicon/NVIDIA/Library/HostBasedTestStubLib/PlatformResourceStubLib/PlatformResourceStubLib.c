/** @file

  Platform Resource Lib stubs for host based tests

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <HostBasedTestStubLib/PlatformResourceStubLib.h>

BOOLEAN mBootChainIsInvalid[2] = {FALSE, FALSE};

EFI_STATUS
EFIAPI
GetActiveBootChain (
  OUT UINT32 *BootChain
)
{
  EFI_STATUS    Status;
  UINT32        RequestedBootChain;

  Status = (EFI_STATUS) mock();
  RequestedBootChain = (UINT32) mock();

  if (!EFI_ERROR (Status)) {
    *BootChain = RequestedBootChain;
  }

  return Status;
}

VOID
MockGetActiveBootChain (
  IN  UINT32        ReturnBootChain,
  IN  EFI_STATUS    ReturnStatus
  )
{
  will_return (GetActiveBootChain, ReturnStatus);
  will_return (GetActiveBootChain, ReturnBootChain);
}

EFI_STATUS
EFIAPI
ValidateActiveBootChain (
  VOID
)
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SetNextBootChain (
  IN  UINT32    BootChain
  )
{
  UINT32        OtherBootChain;
  EFI_STATUS    ReturnStatus;

  ReturnStatus = (EFI_STATUS) mock ();

  if (!EFI_ERROR (ReturnStatus)) {
    OtherBootChain = BootChain ^ 1;

    mBootChainIsInvalid[BootChain] = FALSE;
    mBootChainIsInvalid[OtherBootChain] = TRUE;
  }

  return ReturnStatus;
}

VOID
MockSetNextBootChain (
  IN  EFI_STATUS        ReturnStatus
  )
{
  will_return (SetNextBootChain, ReturnStatus);
}

VOID
PlatformResourcesStubLibInit (
  VOID
  )
{
}

VOID
PlatformResourcesStubLibDeinit (
  VOID
  )
{
  mBootChainIsInvalid[0] = FALSE;
  mBootChainIsInvalid[1] = FALSE;
}
