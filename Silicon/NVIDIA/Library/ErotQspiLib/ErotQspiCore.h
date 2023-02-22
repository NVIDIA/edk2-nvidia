/** @file

  Erot Qspi library core routines

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EROT_QSPI_CORE_H__
#define __EROT_QSPI_CORE_H__

#define EROT_QSPI_MSG_TYPE_INFO     0x01
#define EROT_QSPI_MSG_TYPE_MCTP     0x02
#define EROT_QSPI_MSG_TYPE_SET_CFG  0x03
#define EROT_QSPI_MSG_TYPE_ERROR    0xFF

#define EROT_QSPI_MS_TO_NS(ms)  ((ms)*1000ULL*1000ULL)

/**
  Get free-running nanosecond counter.

  @retval UINT64            Counter value.

**/
UINT64
EFIAPI
ErotQspiNsCounter (
  VOID
  );

/**
  Print data buffer.

  @param[in]  String        String to print as prefix before data.
  @param[in]  Buffer        Pointer to data.
  @param[in]  Length        Number of data bytes to print.

  @retval None

**/
VOID
EFIAPI
ErotQspiPrintBuffer (
  IN CONST CHAR8  *String,
  IN CONST VOID   *Buffer,
  IN UINTN        Length
  );

/**
  Receive MCTP packet from erot.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Length        Pointer to save received packet size in bytes.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotQspiRecvPacket (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN OUT UINTN               *Length
  );

/**
  Send MCTP packet to erot.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Length        Number of bytes in packet.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotQspiSendPacket (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINTN                   Length
  );

/**
  Check if erot has interrupt request pending.

  @param[in]  Private       Pointer to private structure for erot.

  @retval BOOLEAN           TRUE if interrupt is pending.

**/
BOOLEAN
EFIAPI
ErotQspiHasInterruptReq (
  IN  EROT_QSPI_PRIVATE_DATA  *Private
  );

/**
  De-initialize erot spb interface.

  @param[in]  Private       Pointer to private structure for erot.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotQspiSpbDeinit (
  IN EROT_QSPI_PRIVATE_DATA  *Private
  );

/**
  Initialize erot spb interface.

  @param[in]  Private       Pointer to private structure for erot.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotQspiSpbInit (
  IN EROT_QSPI_PRIVATE_DATA  *Private
  );

#endif
