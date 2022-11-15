/** @file

  Erot library

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EROT_LIB_H__
#define __EROT_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/MctpProtocol.h>

typedef
EFI_STATUS
(EFIAPI *EROT_RESPONSE_CHECK)(
  IN NVIDIA_MCTP_PROTOCOL   *This,
  IN CONST VOID             *Request,
  IN UINTN                  RequestLength,
  IN CONST VOID             *Response,
  IN UINTN                  ResponseLength
  );

/**
  Get number of erots.  Must call ErotLibInit() before using.

  @retval UINTN           Number of erots.

**/
UINTN
EFIAPI
ErotGetNumErots (
  VOID
  );

/**
  Get MCTP protocol interface by erot index.  Must call ErotLibInit() before using.

  @param[in]  ErotIndex             Index of erot.

  @retval NVIDIA_MCTP_PROTOCOL *    Pointer to protocol interface.

**/
NVIDIA_MCTP_PROTOCOL *
EFIAPI
ErotGetMctpProtocolByIndex (
  IN UINTN  ErotIndex
  );

/**
  Get MCTP protocol interface by erot socket.  Must call ErotLibInit() before using.

  @param[in]  Socket                Socket of erot.

  @retval NVIDIA_MCTP_PROTOCOL *    Pointer to protocol interface.

**/
NVIDIA_MCTP_PROTOCOL *
EFIAPI
ErotGetMctpProtocolBySocket (
  IN UINTN  Socket
  );

/**
  Send MCTP request to all erots.

  @param[in]  Request              Pointer to request message.
  @param[in]  RequestLength        Length of request message.
  @param[out] ResponseBuffer       Pointer to response buffer.
  @param[in]  ResponseBufferLength Length of response buffer.
  @param[out] ResponseLength       Pointer to return response length.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotSendRequestToAll (
  IN  VOID                 *Request,
  IN  UINTN                RequestLength,
  OUT VOID                 *ResponseBuffer,
  IN  UINTN                ResponseBufferLength,
  IN  EROT_RESPONSE_CHECK  ResponseCheck
  );

/**
  Send boot complete message to erot.

  @param[in]  Socket               Socket of erot.
  @param[in]  BootSlot             BootSlot that socket booted from.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotSendBootComplete (
  IN UINTN  Socket,
  IN UINTN  BootSlot
  );

/**
  Deinitialize Erot Library.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotLibDeinit (
  VOID
  );

/**
  Initialize Erot Library.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotLibInit (
  VOID
  );

#endif
