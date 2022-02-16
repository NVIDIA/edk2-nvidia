/** @file
*
*  Copyright (c) 2019-2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __QSPI_CONTROLLER_LIB_H__
#define __QSPI_CONTROLLER_LIB_H__


typedef struct {
  VOID   *TxBuf;
  UINT32  TxLen;
  VOID   *RxBuf;
  UINT32  RxLen;
  UINT8   WaitCycles;
} QSPI_TRANSACTION_PACKET;


/**
  Initialize the QSPI Driver

  Configure the basic controller configuration to be able to talk to a slave.
  Once basic configuration is done, flush both TX and RX FIFO to be able to
  start transactions.

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.

  @retval EFI_SUCCESS              Controller initialized successfully.
  @retval Others                   Controller initialization failed.
**/
EFI_STATUS
QspiInitialize (
  IN EFI_PHYSICAL_ADDRESS QspiBaseAddress
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
  IN EFI_PHYSICAL_ADDRESS    QspiBaseAddress,
  IN QSPI_TRANSACTION_PACKET *Packet
);


#endif //__QSPI_CONTROLLER_LIB_H__
