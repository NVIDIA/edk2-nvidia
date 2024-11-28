/** @file

  Null Erot library

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>
#include <Library/DebugLib.h>
#include <Library/ErotLib.h>

UINTN
EFIAPI
ErotGetNumErots (
  VOID
  )
{
  return 0;
}

NVIDIA_MCTP_PROTOCOL *
EFIAPI
ErotGetMctpProtocolByIndex (
  IN UINTN  ErotIndex
  )
{
  return NULL;
}

NVIDIA_MCTP_PROTOCOL *
EFIAPI
ErotGetMctpProtocolBySocket (
  IN UINTN  Socket
  )
{
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
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
ErotSendBootComplete (
  IN UINTN  Socket,
  IN UINTN  BootSlot
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
ErotLibDeinit (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
ErotLibInit (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}
