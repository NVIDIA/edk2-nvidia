/** @file

  Erot library

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>
#include <Library/DebugLib.h>
#include <Library/ErotLib.h>
#include <Library/MctpNvVdmLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/MctpProtocol.h>

STATIC BOOLEAN               mErotLibInitialized = FALSE;
STATIC UINTN                 mNumErots           = 0;
STATIC NVIDIA_MCTP_PROTOCOL  **mErots            = NULL;

/**
  Locate MCTP protocol interfaces for erots.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotLocateProtocols (
  VOID
  )
{
  EFI_STATUS              Status;
  UINTN                   Index;
  UINTN                   NumHandles;
  EFI_HANDLE              *HandleBuffer;
  NVIDIA_MCTP_PROTOCOL    *Protocol;
  MCTP_DEVICE_ATTRIBUTES  Attributes;

  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gNVIDIAMctpProtocolGuid,
                        NULL,
                        &NumHandles,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: LocateHandleBuffer failed for gNVIDIAMctpProtocolGuid:%r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  DEBUG ((DEBUG_INFO, "%a: got %d MCTP protocol handles\n", __FUNCTION__, NumHandles));

  mErots = (NVIDIA_MCTP_PROTOCOL **)
           AllocateRuntimeZeroPool (NumHandles * sizeof (VOID *));
  if (mErots == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: mErots allocate failed\n", __FUNCTION__));
    goto Done;
  }

  mNumErots = 0;
  for (Index = 0; Index < NumHandles; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gNVIDIAMctpProtocolGuid,
                    (VOID **)&Protocol
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get MCTP Protocol for index=%u: %r\n",
        __FUNCTION__,
        Index,
        Status
        ));
      goto Done;
    }

    Status = Protocol->GetDeviceAttributes (Protocol, &Attributes);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: %u get attr failed: %r\n", __FUNCTION__, Index, Status));
      continue;
    }

    if (Attributes.DeviceType != DEVICE_TYPE_EROT) {
      continue;
    }

    DEBUG ((DEBUG_INFO, "%a: Got %s MCTP protocol\n", __FUNCTION__, Attributes.DeviceName));

    mErots[mNumErots++] = Protocol;
  }

  if (mNumErots == 0) {
    Status = EFI_NOT_FOUND;
    DEBUG ((DEBUG_ERROR, "%a: No erots found\n", __FUNCTION__));
  } else {
    Status = EFI_SUCCESS;
  }

Done:
  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
    HandleBuffer = NULL;
  }

  if (EFI_ERROR (Status)) {
    if (mErots != NULL) {
      FreePool (mErots);
      mErots = NULL;
    }

    mNumErots = 0;
  }

  return Status;
}

UINTN
EFIAPI
ErotGetNumErots (
  VOID
  )
{
  return mNumErots;
}

NVIDIA_MCTP_PROTOCOL *
EFIAPI
ErotGetMctpProtocolByIndex (
  IN UINTN  ErotIndex
  )
{
  if (ErotIndex < mNumErots) {
    return mErots[ErotIndex];
  }

  return NULL;
}

NVIDIA_MCTP_PROTOCOL *
EFIAPI
ErotGetMctpProtocolBySocket (
  IN UINTN  Socket
  )
{
  MCTP_DEVICE_ATTRIBUTES  Attributes;
  EFI_STATUS              Status;
  NVIDIA_MCTP_PROTOCOL    *Protocol;
  UINTN                   Index;

  for (Index = 0; Index < mNumErots; Index++) {
    Protocol = mErots[Index];
    Status   = Protocol->GetDeviceAttributes (Protocol, &Attributes);
    ASSERT_EFI_ERROR (Status);

    if (Attributes.Socket == Socket) {
      return Protocol;
    }
  }

  return NULL;
}

EFI_STATUS
EFIAPI
ErotSendRequestToAll (
  IN  VOID                 *Request,
  IN  UINTN                RequestLength,
  OUT VOID                 *ResponseBuffer,
  IN  UINTN                ResponseBufferLength,
  IN  EROT_RESPONSE_CHECK  ResponseCheck
  )
{
  NVIDIA_MCTP_PROTOCOL    *Protocol;
  MCTP_DEVICE_ATTRIBUTES  Attributes;
  EFI_STATUS              Status;
  UINTN                   ResponseLength;
  UINTN                   Index;
  BOOLEAN                 Error;

  Status = ErotLibInit ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Error = FALSE;
  for (Index = 0; Index < ErotGetNumErots (); Index++) {
    Protocol = ErotGetMctpProtocolByIndex (Index);
    Status   = Protocol->GetDeviceAttributes (Protocol, &Attributes);
    ASSERT_EFI_ERROR (Status);

    DEBUG ((DEBUG_INFO, "%a: sending req to %s\n", __FUNCTION__, Attributes.DeviceName));

    Protocol->DoRequest (
                Protocol,
                Request,
                RequestLength,
                ResponseBuffer,
                ResponseBufferLength,
                &ResponseLength
                );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: req to %s failed: %r\n", __FUNCTION__, Attributes.DeviceName, Status));
      Error = TRUE;
      continue;
    }

    Status = ResponseCheck (Protocol, Request, RequestLength, ResponseBuffer, ResponseLength);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: req to %s failed rsp: %r\n", __FUNCTION__, Attributes.DeviceName, Status));
      Error = TRUE;
      continue;
    }
  }

  if (Error) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ErotSendBootComplete (
  IN UINTN  Socket,
  IN UINTN  BootSlot
  )
{
  EFI_STATUS                      Status;
  MCTP_NV_BOOT_COMPLETE_REQUEST   Req;
  MCTP_NV_BOOT_COMPLETE_RESPONSE  Rsp;
  MCTP_DEVICE_ATTRIBUTES          Attributes;
  NVIDIA_MCTP_PROTOCOL            *Protocol;
  UINTN                           ResponseLength;
  EFI_HANDLE                      Handle;

  Status = ErotLibInit ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Protocol = ErotGetMctpProtocolBySocket (Socket);
  if (Protocol == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: no protocol for socket %u\n", __FUNCTION__, Socket));
    return EFI_INVALID_PARAMETER;
  }

  Status = Protocol->GetDeviceAttributes (Protocol, &Attributes);
  ASSERT_EFI_ERROR (Status);

  MctpNvBootCompleteFillReq (&Req, BootSlot);

  Status = Protocol->DoRequest (Protocol, &Req, sizeof (Req), &Rsp, sizeof (Rsp), &ResponseLength);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s request failed: %r\n", __FUNCTION__, Attributes.DeviceName, Status));
    return Status;
  }

  if (ResponseLength != sizeof (Rsp)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s bad resp length: %u!=%u\n",
      __FUNCTION__,
      Attributes.DeviceName,
      ResponseLength,
      sizeof (Rsp)
      ));
    return EFI_DEVICE_ERROR;
  }

  if (Rsp.CompletionCode != MCTP_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s failed: 0x%x\n",
      __FUNCTION__,
      Attributes.DeviceName,
      Rsp.CompletionCode
      ));
    return EFI_DEVICE_ERROR;
  }

  if (Socket == 0) {
    Handle = NULL;
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Handle,
                    &gNVIDIAErotBootCompleteProtocolGuid,
                    NULL,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: install protocol failed: %r\n", __FUNCTION__, Status));
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ErotLibDeinit (
  VOID
  )
{
  if (!mErotLibInitialized) {
    return EFI_SUCCESS;
  }

  if (mErots != NULL) {
    FreePool (mErots);
    mErots = NULL;
  }

  mNumErots           = 0;
  mErotLibInitialized = TRUE;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ErotLibInit (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mErotLibInitialized) {
    return EFI_SUCCESS;
  }

  Status = ErotLocateProtocols ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mErotLibInitialized = TRUE;

  return EFI_SUCCESS;
}
