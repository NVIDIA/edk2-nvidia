/** @file

  QSPI Controller Library

  SPDX-FileCopyrightText: Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/QspiControllerLib.h>
#include <Library/TimerLib.h>

#include "QspiControllerLibPrivate.h"

BOOLEAN  TimeOutMessage = FALSE;

/**
  Flush QSPI Controller FIFO.

  If FIFO is not already empty, flush the respective TX/RX FIFO. Wait for the
  register state to reflect FIFO has been flushed.

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  TxFifo                   TRUE for Tx Fifo, FALSE for Rx Fifo

  @retval EFI_SUCCESS              FIFO flush successful
  @retval Others                   FIFO flush failed.
**/
STATIC
EFI_STATUS
QspiFlushFifo (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN BOOLEAN               TxFifo
  )
{
  UINT32  Timeout;

  Timeout = 0;
  if (TxFifo) {
    // Flush TX FIFO if it is not already empty.
    if (QSPI_FIFO_STATUS_0_FIFO_EMPTY != MmioBitFieldRead32 (
                                           QspiBaseAddress + QSPI_FIFO_STATUS_0,
                                           QSPI_FIFO_STATUS_0_TX_FIFO_EMPTY_BIT,
                                           QSPI_FIFO_STATUS_0_TX_FIFO_EMPTY_BIT
                                           ))
    {
      // Flush FIFO.
      MmioBitFieldWrite32 (
        QspiBaseAddress + QSPI_FIFO_STATUS_0,
        QSPI_FIFO_STATUS_0_TX_FIFO_FLUSH_BIT,
        QSPI_FIFO_STATUS_0_TX_FIFO_FLUSH_BIT,
        QSPI_FIFO_STATUS_0_FIFO_FLUSH
        );
      // Wait for FIFO to be flushed.
      while (QSPI_FIFO_STATUS_0_FIFO_FLUSH == MmioBitFieldRead32 (
                                                QspiBaseAddress + QSPI_FIFO_STATUS_0,
                                                QSPI_FIFO_STATUS_0_TX_FIFO_FLUSH_BIT,
                                                QSPI_FIFO_STATUS_0_TX_FIFO_FLUSH_BIT
                                                ))
      {
        MicroSecondDelay (1);
        if (Timeout != TIMEOUT) {
          Timeout++;
          if (Timeout == TIMEOUT) {
            if (TimeOutMessage == FALSE) {
              DEBUG ((DEBUG_ERROR, "%a QSPI Transactions Slower Than Usual.\n", __FUNCTION__));
              TimeOutMessage = TRUE;
            }

            return EFI_NOT_READY;
          }
        }
      }
    }
  } else {
    // Flush RX FIFO if it is not already empty.
    if (QSPI_FIFO_STATUS_0_FIFO_EMPTY != MmioBitFieldRead32 (
                                           QspiBaseAddress + QSPI_FIFO_STATUS_0,
                                           QSPI_FIFO_STATUS_0_RX_FIFO_EMPTY_BIT,
                                           QSPI_FIFO_STATUS_0_RX_FIFO_EMPTY_BIT
                                           ))
    {
      // Flush FIFO.
      MmioBitFieldWrite32 (
        QspiBaseAddress + QSPI_FIFO_STATUS_0,
        QSPI_FIFO_STATUS_0_RX_FIFO_FLUSH_BIT,
        QSPI_FIFO_STATUS_0_RX_FIFO_FLUSH_BIT,
        QSPI_FIFO_STATUS_0_FIFO_FLUSH
        );
      // Wait for FIFO to be flushed.
      while (QSPI_FIFO_STATUS_0_FIFO_FLUSH == MmioBitFieldRead32 (
                                                QspiBaseAddress + QSPI_FIFO_STATUS_0,
                                                QSPI_FIFO_STATUS_0_RX_FIFO_FLUSH_BIT,
                                                QSPI_FIFO_STATUS_0_RX_FIFO_FLUSH_BIT
                                                ))
      {
        MicroSecondDelay (1);
        if (Timeout != TIMEOUT) {
          Timeout++;
          if (Timeout == TIMEOUT) {
            if (TimeOutMessage == FALSE) {
              DEBUG ((DEBUG_ERROR, "%a QSPI Transactions Slower Than Usual.\n", __FUNCTION__));
              TimeOutMessage = TRUE;
            }

            return EFI_NOT_READY;
          }
        }
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Configure the CS pin

  Configure whether to enable or disable CS for a slave

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  ChipSelect               Chip select to configure
  @param  Enable                   TRUE for Tx Fifo, FALSE for Rx Fifo
**/
STATIC
VOID
QspiConfigureCS (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN UINT8                 ChipSelect,
  IN BOOLEAN               Enable
  )
{
  // Configure CS
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_CS_SEL_LSB,
    QSPI_COMMAND_0_CS_SEL_MSB,
    QSPI_COMMAND_0_CS_SEL_CS0 + ChipSelect
    );

  if (Enable) {
    // Configure CS pin low.
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_COMMAND_0,
      QSPI_COMMAND_0_CS_SW_VAL_BIT,
      QSPI_COMMAND_0_CS_SW_VAL_BIT,
      QSPI_COMMAND_0_CS_SW_VAL_LOW
      );
  } else {
    // Configure CS pin high.
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_COMMAND_0,
      QSPI_COMMAND_0_CS_SW_VAL_BIT,
      QSPI_COMMAND_0_CS_SW_VAL_BIT,
      QSPI_COMMAND_0_CS_SW_VAL_HIGH
      );
  }

  DEBUG ((DEBUG_INFO, "QSPI CS Configured.\n"));
}

/**
  Clear Transaction Status

  If transaction status is ready, clear it by writing to the same bit.

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
**/
STATIC
VOID
QspiClearTransactionStatus (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress
  )
{
  if (QSPI_TRANSFER_STATUS_0_RDY_READY == MmioBitFieldRead32 (
                                            QspiBaseAddress + QSPI_TRANSFER_STATUS_0,
                                            QSPI_TRANSFER_STATUS_0_RDY_BIT,
                                            QSPI_TRANSFER_STATUS_0_RDY_BIT
                                            ))
  {
    // Clear transaction status
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_TRANSFER_STATUS_0,
      QSPI_TRANSFER_STATUS_0_RDY_BIT,
      QSPI_TRANSFER_STATUS_0_RDY_BIT,
      QSPI_TRANSFER_STATUS_0_RDY_READY
      );
  }
}

/**
  Wait for transaction status to be ready.

  Wait for transaction status bit to be ready within the stipulated timeout.

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.

  @retval EFI_SUCCESS              Transaction status ready.
  @retval Others                   Transaction status did not get ready.
**/
STATIC
EFI_STATUS
QspiWaitTransactionStatusReady (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress
  )
{
  UINT32  Timeout;

  Timeout = 0;
  // Wait for transaction status to be ready.
  while (QSPI_TRANSFER_STATUS_0_RDY_NOT_READY == MmioBitFieldRead32 (
                                                   QspiBaseAddress + QSPI_TRANSFER_STATUS_0,
                                                   QSPI_TRANSFER_STATUS_0_RDY_BIT,
                                                   QSPI_TRANSFER_STATUS_0_RDY_BIT
                                                   ))
  {
    MicroSecondDelay (1);
    if (Timeout != TIMEOUT) {
      Timeout++;
      if (Timeout == TIMEOUT) {
        if (TimeOutMessage == FALSE) {
          DEBUG ((DEBUG_ERROR, "%a QSPI Transactions Slower Than Usual.\n", __FUNCTION__));
          TimeOutMessage = TRUE;
        }

        return EFI_NOT_READY;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Setup Wait Cycles

  Configure controller for transaction based on packet length and
  width.

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  WaitCycles               Number of wait cycles.
**/
STATIC
VOID
QspiPerformWaitCycleConfiguration (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN UINT8                 WaitCycles
  )
{
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_MISC_0,
    QSPI_MISC_0_WAIT_CYCLES_LSB,
    QSPI_MISC_0_WAIT_CYCLES_MSB,
    WaitCycles
    );
}

/**
  Enable/disable Combined sequence mode

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  Packet                   QSPI transaction context
  @param  Enable                   TRUE: enable wait state, FALSE: disable wait state
**/
VOID
QspiConfigureCombinedSequenceMode (
  IN EFI_PHYSICAL_ADDRESS     QspiBaseAddress,
  IN QSPI_TRANSACTION_PACKET  *Packet,
  IN BOOLEAN                  Enable
  )
{
  UINT8  CmdSize  = 1;
  UINT8  AddrSize = 0;

  if ((Packet->Control & QSPI_CONTROLLER_CONTROL_CMB_SEQ_MODE_3B_ADDR) != 0) {
    AddrSize = 3;
  } else if ((Packet->Control & QSPI_CONTROLLER_CONTROL_CMB_SEQ_MODE_4B_ADDR) != 0) {
    AddrSize = 4;
  } else {
    // Exit if not Combined sequence mode
    return;
  }

  if (Enable) {
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_GLOBAL_CONFIG_0,
      QSPI_GLOBAL_CONFIG_0_CMB_SEQ_EN_BIT,
      QSPI_GLOBAL_CONFIG_0_CMB_SEQ_EN_BIT,
      QSPI_GLOBAL_CONFIG_0_CMB_SEQ_EN_ENABLE
      );
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_CMB_SEQ_CMD_CFG_0,
      QSPI_CMB_SEQ_CMD_CFG_0_COMMAND_X1_X2_X4_LSB,
      QSPI_CMB_SEQ_CMD_CFG_0_COMMAND_X1_X2_X4_MSB,
      QSPI_CMB_SEQ_CMD_CFG_0_COMMAND_X1_X2_X4_SINGLE
      );
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_CMB_SEQ_CMD_CFG_0,
      QSPI_CMB_SEQ_CMD_CFG_0_COMMAND_SDR_DDR_BIT,
      QSPI_CMB_SEQ_CMD_CFG_0_COMMAND_SDR_DDR_BIT,
      QSPI_CMB_SEQ_CMD_CFG_0_COMMAND_SDR_DDR_SDR
      );
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_CMB_SEQ_CMD_CFG_0,
      QSPI_CMB_SEQ_CMD_CFG_0_COMMAND_SIZE_LSB,
      QSPI_CMB_SEQ_CMD_CFG_0_COMMAND_SIZE_MSB,
      (8 * CmdSize) - 1
      );
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_CMB_SEQ_ADDR_CFG_0,
      QSPI_CMB_SEQ_ADDR_CFG_0_ADDRESS_X1_X2_X4_LSB,
      QSPI_CMB_SEQ_ADDR_CFG_0_ADDRESS_X1_X2_X4_MSB,
      QSPI_CMB_SEQ_ADDR_CFG_0_ADDRESS_X1_X2_X4_SINGLE
      );
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_CMB_SEQ_ADDR_CFG_0,
      QSPI_CMB_SEQ_ADDR_CFG_0_ADDRESS_SDR_DDR_BIT,
      QSPI_CMB_SEQ_ADDR_CFG_0_ADDRESS_SDR_DDR_BIT,
      QSPI_CMB_SEQ_ADDR_CFG_0_ADDRESS_SDR_DDR_SDR
      );
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_CMB_SEQ_ADDR_CFG_0,
      QSPI_CMB_SEQ_ADDR_CFG_0_ADDRESS_SIZE_LSB,
      QSPI_CMB_SEQ_ADDR_CFG_0_ADDRESS_SIZE_MSB,
      (8 * AddrSize) - 1
      );
    MmioWrite32 (QspiBaseAddress + QSPI_CMB_SEQ_CMD_0, Packet->Command);
    MmioWrite32 (QspiBaseAddress + QSPI_CMB_SEQ_ADDR_0, Packet->Address);
  } else {
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_GLOBAL_CONFIG_0,
      QSPI_GLOBAL_CONFIG_0_CMB_SEQ_EN_BIT,
      QSPI_GLOBAL_CONFIG_0_CMB_SEQ_EN_BIT,
      QSPI_GLOBAL_CONFIG_0_CMB_SEQ_EN_DISABLE
      );
  }
}

/**
  Perform transaction configuration

  Configure controller for transaction based on packet length and
  width.

  @param  QspiBaseAddress          Base Address for QSPI Controller in use.
  @param  PacketLen                Size of packets.
  @param  BlockLen                 Number of packets.
**/
STATIC
VOID
QspiPerformTransactionConfiguration (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN UINT32                PacketLen,
  IN UINT32                BlockLen
  )
{
  // Select Single Data Rate mode.
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_SDR_DDR_SEL_BIT,
    QSPI_COMMAND_0_SDR_DDR_SEL_BIT,
    QSPI_COMMAND_0_SDR_DDR_SEL_SDR
    );
  // Select single bit transfer mode.
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_INTERFACE_WIDTH_LSB,
    QSPI_COMMAND_0_INTERFACE_WIDTH_MSB,
    QSPI_COMMAND_0_INTERFACE_WIDTH_SINGLE
    );
  // Configure unpacked mode.
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_PACKED_BIT,
    QSPI_COMMAND_0_PACKED_BIT,
    QSPI_COMMAND_0_PACKED_ENABLE
    );
  // Configure packet width. Number of bits - 1
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_BIT_LENGTH_LSB,
    QSPI_COMMAND_0_BIT_LENGTH_MSB,
    (PacketLen * 8) - 1
    );
  // Configure number of packets. Number of packets - 1
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_DMA_BLK_SIZE_0,
    QSPI_DMA_BLK_SIZE_0_BLOCK_SIZE_LSB,
    QSPI_DMA_BLK_SIZE_0_BLOCK_SIZE_MSB,
    BlockLen - 1
    );
}

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
STATIC
EFI_STATUS
QspiPerformReceive (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN VOID                  *Buffer,
  IN UINT32                Len,
  IN UINT32                PacketLen
  )
{
  EFI_STATUS  Status;
  UINT32      Count;
  UINT8       *BufferTrack;
  UINT32      Data;
  UINT32      Stride;

  BufferTrack = Buffer;

  // Clear transaction status
  QspiClearTransactionStatus (QspiBaseAddress);
  // Perform transaction packet width and size configuration
  QspiPerformTransactionConfiguration (QspiBaseAddress, PacketLen, Len);
  // Enable RX
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_RX_EN_BIT,
    QSPI_COMMAND_0_RX_EN_BIT,
    QSPI_COMMAND_0_RX_EN_ENABLE
    );
  // Enable PIO transfer
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_PIO_BIT,
    QSPI_COMMAND_0_PIO_BIT,
    QSPI_COMMAND_0_PIO_EN
    );
  // Wait for transaction to complete
  Status = QspiWaitTransactionStatusReady (QspiBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Read data from RX FIFO
  Count = 0;
  while (Count < Len) {
    // RX FIFO should not be already empty
    if (QSPI_FIFO_STATUS_0_FIFO_EMPTY == MmioBitFieldRead32 (
                                           QspiBaseAddress + QSPI_FIFO_STATUS_0,
                                           QSPI_FIFO_STATUS_0_RX_FIFO_EMPTY_BIT,
                                           QSPI_FIFO_STATUS_0_RX_FIFO_EMPTY_BIT
                                           ))
    {
      DEBUG ((DEBUG_ERROR, "%a QSPI Rx FIFO Empty.\n", __FUNCTION__));
      return EFI_DEVICE_ERROR;
    }

    if (PacketLen == sizeof (UINT8)) {
      // Since we are using packed mode. we always read 4B but discard what we did not request.
      Data   = MmioRead32 (QspiBaseAddress + QSPI_RX_FIFO_0);
      Stride = ((Len - Count) >= sizeof (UINT32)) ? sizeof (UINT32) : (Len - Count);
      CopyMem (BufferTrack, &Data, Stride);
      BufferTrack += Stride;
      Count       += Stride;
    } else if (PacketLen == sizeof (UINT32)) {
      *(UINT32 *)BufferTrack = (UINT32)MmioRead32 (QspiBaseAddress + QSPI_RX_FIFO_0);
      BufferTrack           += sizeof (UINT32);
      Count++;
    }
  }

  // Disable RX
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_RX_EN_BIT,
    QSPI_COMMAND_0_RX_EN_BIT,
    QSPI_COMMAND_0_RX_EN_DISABLE
    );
  // Disable PIO transfer
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_PIO_BIT,
    QSPI_COMMAND_0_PIO_BIT,
    QSPI_COMMAND_0_PIO_DIS
    );

  DEBUG ((DEBUG_INFO, "QSPI Data Received.\n"));

  return EFI_SUCCESS;
}

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
STATIC
EFI_STATUS
QspiPerformTransmit (
  IN EFI_PHYSICAL_ADDRESS  QspiBaseAddress,
  IN VOID                  *Buffer,
  IN UINT32                Len,
  IN UINT32                PacketLen
  )
{
  EFI_STATUS  Status;
  UINT32      Count;
  UINT8       *BufferTrack;
  UINT32      Data;
  UINT32      Stride;

  BufferTrack = Buffer;

  // Clear transaction status
  QspiClearTransactionStatus (QspiBaseAddress);
  // Perform transaction packet width and size configuration
  QspiPerformTransactionConfiguration (QspiBaseAddress, PacketLen, Len);
  // Enable TX
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_TX_EN_BIT,
    QSPI_COMMAND_0_TX_EN_BIT,
    QSPI_COMMAND_0_TX_EN_ENABLE
    );
  // Write data to TX FIFO
  Count = 0;
  while (Count < Len) {
    // TX FIFO should not be already full
    if (QSPI_FIFO_STATUS_0_FIFO_FULL == MmioBitFieldRead32 (
                                          QspiBaseAddress + QSPI_FIFO_STATUS_0,
                                          QSPI_FIFO_STATUS_0_TX_FIFO_FULL_BIT,
                                          QSPI_FIFO_STATUS_0_TX_FIFO_FULL_BIT
                                          ))
    {
      DEBUG ((DEBUG_ERROR, "%a QSPI Tx FIFO Full.\n", __FUNCTION__));
      return EFI_DEVICE_ERROR;
    }

    if (PacketLen == sizeof (UINT8)) {
      // Since we are using packed mode. we always write 4B with dummy bytes if needed.
      Stride = ((Len - Count) >= sizeof (UINT32)) ? sizeof (UINT32) : (Len - Count);
      CopyMem (&Data, BufferTrack, Stride);
      MmioWrite32 (QspiBaseAddress + QSPI_TX_FIFO_0, Data);
      BufferTrack += Stride;
      Count       += Stride;
    } else if (PacketLen == sizeof (UINT32)) {
      MmioWrite32 (QspiBaseAddress + QSPI_TX_FIFO_0, *(UINT32 *)BufferTrack);
      BufferTrack += sizeof (UINT32);
      Count++;
    }
  }

  // Enable PIO transfer
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_PIO_BIT,
    QSPI_COMMAND_0_PIO_BIT,
    QSPI_COMMAND_0_PIO_EN
    );
  // Wait for transaction to complete
  Status = QspiWaitTransactionStatusReady (QspiBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Disable TX
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_TX_EN_BIT,
    QSPI_COMMAND_0_TX_EN_BIT,
    QSPI_COMMAND_0_TX_EN_DISABLE
    );
  // Disable PIO transfer.
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_PIO_BIT,
    QSPI_COMMAND_0_PIO_BIT,
    QSPI_COMMAND_0_PIO_DIS
    );

  DEBUG ((DEBUG_INFO, "QSPI Data Transmitted.\n"));

  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS  Status;
  UINTN       ChipSelect;

  // Configure master mode.
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_M_S_BIT,
    QSPI_COMMAND_0_M_S_BIT,
    QSPI_COMMAND_0_M_S_MASTER
    );
  // Only master mode 0 is supported.
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_MODE_LSB,
    QSPI_COMMAND_0_MODE_MSB,
    QSPI_COMMAND_0_MODE_MODE0
    );
  // Configure CS to be software controlled.
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_CS_SW_HW_BIT,
    QSPI_COMMAND_0_CS_SW_HW_BIT,
    QSPI_COMMAND_0_CS_SW_HW_SOFTWARE
    );
  // Configure byte order to be big-endian.
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_EN_LE_BYTE_BIT,
    QSPI_COMMAND_0_EN_LE_BYTE_BIT,
    QSPI_COMMAND_0_EN_LE_BYTE_DISABLE
    );
  for (ChipSelect = 0; ChipSelect < NumChipSelects; ChipSelect++) {
    // Configure CS to be inactive high.
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_COMMAND_0,
      QSPI_COMMAND_0_CS_POL_INACTIVE0_BIT + ChipSelect,
      QSPI_COMMAND_0_CS_POL_INACTIVE0_BIT + ChipSelect,
      QSPI_COMMAND_0_CS_POL_INACTIVE_HIGH
      );
    // Configure CS to be high.
    QspiConfigureCS (QspiBaseAddress, ChipSelect, FALSE);
  }

  // Configure pin to drive low strength during idle.
  MmioBitFieldWrite32 (
    QspiBaseAddress + QSPI_COMMAND_0,
    QSPI_COMMAND_0_IDLE_SDA_LSB,
    QSPI_COMMAND_0_IDLE_SDA_LSB,
    QSPI_COMMAND_0_IDLE_SDA_DRIVE_LOW
    );
  // Flush TX FIFO
  Status = QspiFlushFifo (QspiBaseAddress, TRUE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Flush RX FIFO
  Status = QspiFlushFifo (QspiBaseAddress, FALSE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_INFO, "QSPI Initialized.\n"));

  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS  Status;
  UINT8       *Buffer;
  UINT32      TransactionWidth;
  UINT32      TransactionCount;
  UINT32      Count;

  // Check for invalid buffer address and size combinations.
  if (((Packet->TxBuf == NULL) &&
       (Packet->TxLen != 0)) ||
      ((Packet->TxBuf != NULL) &&
       (Packet->TxLen == 0)) ||
      ((Packet->RxBuf == NULL) &&
       (Packet->RxLen != 0)) ||
      ((Packet->RxBuf != NULL) &&
       (Packet->RxLen == 0)))
  {
    return EFI_INVALID_PARAMETER;
  }

  // Setup Wait Cycles.
  QspiPerformWaitCycleConfiguration (QspiBaseAddress, Packet->WaitCycles);
  // Enable CS
  QspiConfigureCS (QspiBaseAddress, Packet->ChipSelect, TRUE);
  // Enable Combined sequence mode
  QspiConfigureCombinedSequenceMode (QspiBaseAddress, Packet, TRUE);

  // If transmission buffer address valid, start transmission
  if (Packet->TxBuf != NULL) {
    DEBUG ((DEBUG_INFO, "QSPI Tx Args: 0x%p %d.\n", Packet->TxBuf, Packet->TxLen));
    Buffer = Packet->TxBuf;
    Count  = Packet->TxLen;
    // Based on transmission buffer length, calculate packet width and packets in current transaction.
    // Packet width can be 1B or 4B. Maximum number of packets in a single transaction can be 64.
    while (Count > 0) {
      TransactionWidth = (Count % sizeof (UINT32)) ? sizeof (UINT8) : sizeof (UINT32);
      TransactionCount = MIN (MAX_FIFO_PACKETS, (Count / TransactionWidth));
      DEBUG ((DEBUG_INFO, "QSPI Tx Transaction: Count: %u Width: %u.\n", TransactionCount, TransactionWidth));
      Status = QspiPerformTransmit (QspiBaseAddress, Buffer, TransactionCount, TransactionWidth);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Buffer += (TransactionWidth * TransactionCount);
      Count  -= (TransactionWidth * TransactionCount);
    }
  }

  // If reception buffer address valid, start reception
  if (Packet->RxBuf != NULL) {
    DEBUG ((DEBUG_INFO, "QSPI Rx Args: 0x%p %d.\n", Packet->RxBuf, Packet->RxLen));
    Buffer = Packet->RxBuf;
    Count  = Packet->RxLen;
    // Based on reception buffer length, calculate packet width and packets in current transaction.
    // Packet width can be 1B or 4B. Maximum number of packets in a single transaction can be 64.
    while (Count > 0) {
      TransactionWidth = (Count % sizeof (UINT32)) ? sizeof (UINT8) : sizeof (UINT32);
      TransactionCount = MIN (MAX_FIFO_PACKETS, (Count / TransactionWidth));
      DEBUG ((DEBUG_INFO, "QSPI Rx Transaction: Count: %u Width: %u.\n", TransactionCount, TransactionWidth));
      Status = QspiPerformReceive (QspiBaseAddress, Buffer, TransactionCount, TransactionWidth);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Buffer += (TransactionWidth * TransactionCount);
      Count  -= (TransactionWidth * TransactionCount);
    }
  }

  // Disable Combined sequence mode
  QspiConfigureCombinedSequenceMode (QspiBaseAddress, Packet, FALSE);
  // Disable CS
  QspiConfigureCS (QspiBaseAddress, Packet->ChipSelect, FALSE);

  // Wait for the controller to clear state before starting next transaction.
  if ((Packet->Control & QSPI_CONTROLLER_CONTROL_FAST_MODE) == 0) {
    MicroSecondDelay (QSPI_CLEAR_STATE_DELAY);
  }

  return EFI_SUCCESS;
}

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
  )
{
  if (Enable) {
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_GLOBAL_CONFIG_0,
      QSPI_GLOBAL_CONFIG_0_WAIT_STATE_EN_BIT,
      QSPI_GLOBAL_CONFIG_0_WAIT_STATE_EN_BIT,
      QSPI_GLOBAL_CONFIG_0_WAIT_STATE_EN_ENABLE
      );
  } else {
    MmioBitFieldWrite32 (
      QspiBaseAddress + QSPI_GLOBAL_CONFIG_0,
      QSPI_GLOBAL_CONFIG_0_WAIT_STATE_EN_BIT,
      QSPI_GLOBAL_CONFIG_0_WAIT_STATE_EN_BIT,
      QSPI_GLOBAL_CONFIG_0_WAIT_STATE_EN_DISABLE
      );
  }

  return EFI_SUCCESS;
}
