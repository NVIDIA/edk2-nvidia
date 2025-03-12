/** @file
  EDK2 API for AvbOpteeInterfaceSmc

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#include <Library/BaseLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/OpteeNvLib.h>
#include <Library/MemoryAllocationLib.h>

#define DEFAULT_OPTEE_SHM_SIZE  (16 * 0x1000)

/**
  Init the optee interface for AVB

  @retval EFI_SUCCESS  Init Optee interface successfully.
**/
EFI_STATUS
AvbOpteeInterfaceInit (
  VOID
  )
{
  EFI_STATUS        Status = EFI_SUCCESS;
  VOID              *ShmBuf;
  OPTEE_SHM_COOKIE  *Cookie;
  UINT64            Cap;

  if (!IsOpteePresent ()) {
    DEBUG ((DEBUG_WARN, "%a:OP-TEE not present\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto Exit;
  }

  Status = OpteeExchangeCapabilities (&Cap);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_WARN,
      "%a:Got %r trying to get capabilities of OP-TEE failed\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

  if (Cap & OPTEE_SMC_SEC_CAP_HAVE_RESERVED_SHM) {
    Status = OpteeInit ();
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a:Got %r trying to initialize OP-TEE\n", __FUNCTION__, Status));
      goto Exit;
    }
  }

  ShmBuf = AllocateAlignedPages (
             EFI_SIZE_TO_PAGES (DEFAULT_OPTEE_SHM_SIZE),
             DEFAULT_OPTEE_SHM_SIZE
             );
  if (ShmBuf == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate shared memory\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = OpteeSetProperties (
             (UINT64)ShmBuf,
             (UINT64)ShmBuf,
             DEFAULT_OPTEE_SHM_SIZE
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Got %r trying to set properties\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

  Cookie = AllocateAlignedPages (
             EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_COOKIE)),
             OPTEE_MSG_PAGE_SIZE
             );
  if (Cookie == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate shared memory cookie",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Cookie->Size = DEFAULT_OPTEE_SHM_SIZE;
  Cookie->Addr = (UINT8 *)ShmBuf;

  Status = OpteeRegisterShm (
             ShmBuf,
             (UINT64)Cookie,
             OPTEE_MSG_PAGE_SIZE,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Got %r trying to register shared memory\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

  Status = OpteeSetShmCookie ((UINT64)Cookie);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Got %r trying to set shared memory cookie\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

Exit:
  if (EFI_ERROR (Status)) {
    if (ShmBuf != NULL) {
      FreePages (ShmBuf, EFI_SIZE_TO_PAGES (DEFAULT_OPTEE_SHM_SIZE));
    }

    if (Cookie != NULL) {
      FreePages (Cookie, EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_COOKIE)));
    }
  }

  return Status;
}

/**
  Native SMC implementation
  Invoke an AVB TA cmd request

  @param[inout] AvbTaArg  OPTEE_INVOKE_FUNCTION_ARG for AVB TA cmd

  @retval EFI_SUCCESS     The operation completed successfully.
**/
EFI_STATUS
AvbOpteeInvoke (
  IN OUT OPTEE_INVOKE_FUNCTION_ARG  *InvokeFunctionArg
  )
{
  EFI_STATUS              Status         = EFI_SUCCESS;
  OPTEE_OPEN_SESSION_ARG  OpenSessionArg = { 0 };

  CopyMem (&OpenSessionArg.Uuid, &gOpteeAvbTaGuid, sizeof (EFI_GUID));

  Status = OpteeOpenSession (&OpenSessionArg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to open Optee session\n", __FUNCTION__, Status));
    goto Exit;
  } else if (OpenSessionArg.Return != OPTEE_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to open avb ta ssesion, OP-TEE return %u\n",
      __func__,
      OpenSessionArg.Return
      ));
    Status = EFI_NOT_READY;
    goto Exit;
  }

  InvokeFunctionArg->Session = OpenSessionArg.Session;

  Status = OpteeInvokeFunction (InvokeFunctionArg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to invoke Optee function\n", __FUNCTION__, Status));
  } else if (InvokeFunctionArg->Return != OPTEE_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "OP-TEE Invoke Function failed with return: %x and return origin: %d\n",
      InvokeFunctionArg->Return,
      InvokeFunctionArg->ReturnOrigin
      ));
    Status = (InvokeFunctionArg->Return == OPTEE_ERROR_ITEM_NOT_FOUND) ? EFI_NOT_FOUND : EFI_NO_RESPONSE;
  }

  OpteeCloseSession (OpenSessionArg.Session);

Exit:
  return Status;
}
