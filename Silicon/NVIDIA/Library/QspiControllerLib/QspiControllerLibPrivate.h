/** @file

  QSPI Controller Library Private Structures

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __QSPI_CONTROLLER_LIBRARY_PRIVATE_H__
#define __QSPI_CONTROLLER_LIBRARY_PRIVATE_H__

#define TIMEOUT                              100


#define QSPI_COMMAND_0                       0x0

#define QSPI_COMMAND_0_PIO_BIT               31
#define QSPI_COMMAND_0_M_S_BIT               30
#define QSPI_COMMAND_0_MODE_MSB              29
#define QSPI_COMMAND_0_MODE_LSB              28
#define QSPI_COMMAND_0_CS_SEL_MSB            27
#define QSPI_COMMAND_0_CS_SEL_LSB            26
#define QSPI_COMMAND_0_CS_POL_INACTIVE0_BIT  22
#define QSPI_COMMAND_0_CS_SW_HW_BIT          21
#define QSPI_COMMAND_0_CS_SW_VAL_BIT         20
#define QSPI_COMMAND_0_IDLE_SDA_MSB          19
#define QSPI_COMMAND_0_IDLE_SDA_LSB          18
#define QSPI_COMMAND_0_RX_EN_BIT             12
#define QSPI_COMMAND_0_TX_EN_BIT             11
#define QSPI_COMMAND_0_SDR_DDR_SEL_BIT       9
#define QSPI_COMMAND_0_INTERFACE_WIDTH_MSB   8
#define QSPI_COMMAND_0_INTERFACE_WIDTH_LSB   7
#define QSPI_COMMAND_0_PACKED_BIT            5
#define QSPI_COMMAND_0_BIT_LENGTH_MSB        4
#define QSPI_COMMAND_0_BIT_LENGTH_LSB        0

#define QSPI_COMMAND_0_PIO_EN                 1
#define QSPI_COMMAND_0_PIO_DIS                0
#define QSPI_COMMAND_0_M_S_MASTER             1
#define QSPI_COMMAND_0_MODE_MODE0             0
#define QSPI_COMMAND_0_CS_SEL_CS0             0
#define QSPI_COMMAND_0_CS_POL_INACTIVE0_HIGH  1
#define QSPI_COMMAND_0_CS_SW_HW_SOFTWARE      1
#define QSPI_COMMAND_0_CS_SW_VAL_LOW          0
#define QSPI_COMMAND_0_CS_SW_VAL_HIGH         1
#define QSPI_COMMAND_0_IDLE_SDA_DRIVE_LOW     0
#define QSPI_COMMAND_0_RX_EN_DISABLE          0
#define QSPI_COMMAND_0_RX_EN_ENABLE           1
#define QSPI_COMMAND_0_TX_EN_DISABLE          0
#define QSPI_COMMAND_0_TX_EN_ENABLE           1
#define QSPI_COMMAND_0_SDR_DDR_SEL_SDR        0
#define QSPI_COMMAND_0_INTERFACE_WIDTH_SINGLE 0
#define QSPI_COMMAND_0_PACKED_ENABLE          1


#define QSPI_TRANSFER_STATUS_0               0x10

#define QSPI_TRANSFER_STATUS_0_RDY_BIT       30
#define QSPI_TRANSFER_STATUS_0_BLK_CNT_MSB   27
#define QSPI_TRANSFER_STATUS_0_BLK_CNT_LSB   0

#define QSPI_TRANSFER_STATUS_0_RDY_READY     1
#define QSPI_TRANSFER_STATUS_0_RDY_NOT_READY 0


#define QSPI_DMA_BLK_SIZE_0                  0x24

#define QSPI_DMA_BLK_SIZE_0_BLOCK_SIZE_MSB   27
#define QSPI_DMA_BLK_SIZE_0_BLOCK_SIZE_LSB   0


#define QSPI_FIFO_STATUS_0                   0x14

#define QSPI_FIFO_STATUS_0_RX_FIFO_FLUSH_BIT 15
#define QSPI_FIFO_STATUS_0_TX_FIFO_FLUSH_BIT 14
#define QSPI_FIFO_STATUS_0_TX_FIFO_FULL_BIT  3
#define QSPI_FIFO_STATUS_0_TX_FIFO_EMPTY_BIT 2
#define QSPI_FIFO_STATUS_0_RX_FIFO_EMPTY_BIT 0

#define QSPI_FIFO_STATUS_0_FIFO_FLUSH        1
#define QSPI_FIFO_STATUS_0_FIFO_FULL         1
#define QSPI_FIFO_STATUS_0_FIFO_EMPTY        1


#define QSPI_TX_FIFO_0                       0x108


#define QSPI_RX_FIFO_0                       0x188


#define QSPI_MISC_0                          0x194


#define MAX_FIFO_PACKETS                     64


#endif
