/** @file
  NVIDIA QSPI Controller Protocol

  Copyright (c) 2019, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_QSPI_CONTROLLER_PROTOCOL_H__
#define __NVIDIA_QSPI_CONTROLLER_PROTOCOL_H__

#include <Library/QspiControllerLib.h>

#define NVIDIA_QSPI_CONTROLLER_PROTOCOL_GUID \
  { \
  0x112e0323, 0x50e0, 0x418a, { 0x94, 0x4e, 0xe2, 0x25, 0x43, 0x51, 0x56, 0x2a } \
  }


//
// Define for forward reference.
//
typedef struct _NVIDIA_QSPI_CONTROLLER_PROTOCOL NVIDIA_QSPI_CONTROLLER_PROTOCOL;


/**
  Perform a single transaction on QSPI bus.

  @param[in] This                  Instance of protocol
  @param[in] Packet                Transaction context

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI * QSPI_CONTROLLER_PERFORM_TRANSACTION)(
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL *This,
  IN QSPI_TRANSACTION_PACKET         *Packet
);

/// NVIDIA_QSPI_CONTROLLER_PROTOCOL protocol structure.
struct _NVIDIA_QSPI_CONTROLLER_PROTOCOL {
  QSPI_CONTROLLER_PERFORM_TRANSACTION PerformTransaction;
};

extern EFI_GUID gNVIDIAQspiControllerProtocolGuid;

#endif
