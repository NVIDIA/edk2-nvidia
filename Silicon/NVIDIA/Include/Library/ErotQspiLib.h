/** @file

  Erot Qspi Library

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EROT_QSPI_LIB_H__
#define __EROT_QSPI_LIB_H__

#include <Library/MctpBaseLib.h>
#include <Protocol/EmbeddedGpio.h>
#include <Protocol/MctpProtocol.h>
#include <Protocol/QspiController.h>

#define EROT_QSPI_CHIP_SELECT_DEFAULT       1
#define EROT_QSPI_PACKET_SIZE               64
#define EROT_QSPI_MESSAGE_SIZE              (4 * 1024)
#define EROT_QSPI_NAME_LENGTH               16
#define EROT_QSPI_HEADER_SIZE               (sizeof (EROT_QSPI_MEDIUM_HEADER) +\
                                             sizeof (MCTP_TRANSPORT_HEADER))
#define EROT_QSPI_TRANSPORT_HEADER_VERSION  1
#define EROT_QSPI_CONTROLLER_EID            0x27
#define EROT_QSPI_EROT_EID                  0x18
#define EROT_QSPI_PRIVATE_DATA_SIGNATURE    SIGNATURE_32 ('F','W','I','M')

//
// QSPI transport MCTP packet and control message timing parameters
//
// Packet timeout
#define QSPI_MCTP_PT_MS_MIN  100
#define QSPI_MCTP_PT_MS_MAX  100
// Number of request retries
#define QSPI_MCTP_MN1_RETRIES  2
// Request-to_response timeout
#define QSPI_MCTP_MT1_MS_MAX  100
// Timeout waiting for a response
#define QSPI_MCTP_MT2_MS_MIN  (QSPI_MCTP_MT1_MS_MAX + (2 * QSPI_MCTP_MT3_MS_MAX))
#define QSPI_MCTP_MT2_MS_MAX  QSPI_MCTP_MT4_MS_MIN
// Transmission delay
#define QSPI_MCTP_MT3_MS_MAX   100
#define QSPI_MCTP_MT3A_MS_MAX  100
// Instance id experation interval
#define QSPI_MCTP_MT4_MS_MIN  (5 * 1000)
#define QSPI_MCTP_MT4_MS_MAX  (6 * 1000)

#pragma pack(1)

typedef struct {
  UINT8    Type;
  UINT8    Length;
  UINT8    Reserved[2];
} EROT_QSPI_MEDIUM_HEADER;

typedef struct {
  EROT_QSPI_MEDIUM_HEADER    MediumHdr;
  MCTP_TRANSPORT_HEADER      TransportHdr;
  UINT8                      Payload[EROT_QSPI_PACKET_SIZE];
} EROT_QSPI_PACKET;

#pragma pack()

typedef struct {
  EMBEDDED_GPIO        *Protocol;
  EMBEDDED_GPIO_PIN    Pin;
} EROT_QSPI_GPIO;

typedef struct {
  UINT32                             Signature;
  CHAR16                             Name[EROT_QSPI_NAME_LENGTH];
  NVIDIA_QSPI_CONTROLLER_PROTOCOL    *Qspi;
  UINT8                              ChipSelect;
  UINT8                              Socket;
  EROT_QSPI_GPIO                     Gpio;

  // transport
  UINT8                              MyEID;
  UINT8                              ErotEID;
  UINT8                              MsgTag;
  EROT_QSPI_PACKET                   Packet;

  // erot state
  BOOLEAN                            ErotIsInitialized;
  BOOLEAN                            HasMessageAvailable;

  // protocol
  EFI_HANDLE                         Handle;
  NVIDIA_MCTP_PROTOCOL               Protocol;
} EROT_QSPI_PRIVATE_DATA;

extern EROT_QSPI_PRIVATE_DATA  *mPrivate;
extern UINTN                   mNumErotQspis;

/**
  Add Erot accessed via given QSPI and chip select.

  @param[in]  Qspi        Qspi protocol.
  @param[in]  ChipSelect  Erot chip select.
  @param[in]  Socket      Erot chip socket.
  @param[in]  Gpio        Pointer to GPIO info.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotQspiAddErot (
  IN  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *Qspi,
  IN  UINT8                            ChipSelect,
  IN  UINT8                            Socket,
  IN  CONST EROT_QSPI_GPIO             *Gpio
  );

/**
  De-initialize library.

  @retval None

**/
VOID
EFIAPI
ErotQspiLibDeinit (
  VOID
  );

/**
  Initialize library.

  @param[in] NumDevices   Maximum number of erots to support.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotQspiLibInit (
  IN  UINTN  NumDevices
  );

#endif
