/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __QSPI_CONTROLLER_LIB_H__
#define __QSPI_CONTROLLER_LIB_H__

#define QSPI_CONTROLLER_CONTROL_FAST_MODE             0x01
#define QSPI_CONTROLLER_CONTROL_CMB_SEQ_MODE_3B_ADDR  0x02
#define QSPI_CONTROLLER_CONTROL_CMB_SEQ_MODE_4B_ADDR  0x04

typedef struct {
  VOID      *TxBuf;
  UINT32    TxLen;
  VOID      *RxBuf;
  UINT32    RxLen;
  UINT8     WaitCycles;
  UINT8     ChipSelect;
  UINT8     Control;
  UINT32    Command;    // Only valid if 'Control' = CMB_SEQ_MODE_xxx
  UINT32    Address;    // Only valid if 'Control' = CMB_SEQ_MODE_xxx
} QSPI_TRANSACTION_PACKET;

/**
  Initialize the QSPI Driver

  Configure the basic controller configuration to be able to talk to a slave.
  Once basic configuration is done, flush both TX and RX FIFO to be able to
  start transactions.

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  NumChipSelects           Number of chip selects supported.

  @retval EFI_SUCCESS              Controller initialized successfully.
  @retval Others                   Controller initialization failed.
**/
EFI_STATUS
QspiInitialize (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN UINT8                 NumChipSelects
  );

/**
  Perform Transaction

  Check transaction packet to be valid. For both Rx and Tx, calculate packet
  width and count for each individual transaction and then process it.

  QSPI transaction packet will have context for both TX as well as RX even
  if we are doing either one and not both. Set the RX context correctly if
  only TX needs to be done without any RX. Also, if RX or TX buffer addresses
  are not NULL, their respective sizes cannot be 0.

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  Packet                   QSPI transaction context

  @retval EFI_SUCCESS              Transaction successful.
  @retval Others                   Transaction failed.
**/
EFI_STATUS
QspiPerformTransaction (
  IN EFI_PHYSICAL_ADDRESS     QspiBaseAddress,
  IN QSPI_TRANSACTION_PACKET  *Packet
  );

/**
  Enable/disable polling for wait state

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  Enable                   TRUE: enable wait state, FALSE: disable wait state

  @retval EFI_SUCCESS              Wait state is enabled
  @retval Others                   Wait state cannot be enabled
**/
EFI_STATUS
QspiEnableWaitState (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN BOOLEAN               Enable
  );

/**
  Transmit data over QSPI

  Configure controller in TX mode and start transaction in PIO mode.

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  Buffer                   Address of buffer containing data to
                                   be transmitted.
  @param  Len                      Number of packets.
  @param  PacketLen                Size of individual packet.

  @retval EFI_SUCCESS              Data transmitted successfully.
  @retval Others                   Data transmission failed.
**/
EFI_STATUS
QspiPerformTransmit (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN VOID                  *Buffer,
  IN UINT32                Len,
  IN UINT32                PacketLen
  );

/**
  Receive data over QSPI

  Configure controller in RX mode and start transaction in PIO mode.

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  Buffer                   Address of buffer where data should be
                                   received.
  @param  Len                      Number of packets.
  @param  PacketLen                Size of individual packet.

  @retval EFI_SUCCESS              Data received successfully.
  @retval Others                   Data reception failed.
**/
EFI_STATUS
QspiPerformReceive (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN VOID                  *Buffer,
  IN UINT32                Len,
  IN UINT32                PacketLen
  );

/**
  Configure the CS pin

  Configure whether to enable or disable CS for a slave

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  ChipSelect               Chip select to configure
  @param  Enable                   TRUE for Tx Fifo, FALSE for Rx Fifo
**/
VOID
QspiConfigureCS (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN UINT8                 ChipSelect,
  IN BOOLEAN               Enable
  );

#endif //__QSPI_CONTROLLER_LIB_H__
