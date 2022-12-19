/** @file

  Tegra I2c Controller Driver private structures

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __TEGRA_I2C_PRIVATE_H__
#define __TEGRA_I2C_PRIVATE_H__

#include <PiDxe.h>
#include <Protocol/I2cMaster.h>
#include <Protocol/I2cEnumerate.h>
#include <Protocol/I2cBusConfigurationManagement.h>

#define TEGRA_I2C_SIGNATURE  SIGNATURE_32('T','I','2','C')

// Currently only support enumerating 16 device per controller
#define MAX_I2C_DEVICES        16
#define MAX_SLAVES_PER_DEVICE  1

typedef struct {
  //
  // Standard signature used to identify TegraI2c private data
  //
  UINT32                                           Signature;

  //
  // Protocol instances produced by this driver
  //
  EFI_I2C_MASTER_PROTOCOL                          I2cMaster;
  EFI_I2C_CONTROLLER_CAPABILITIES                  I2cControllerCapabilities;
  EFI_I2C_ENUMERATE_PROTOCOL                       I2cEnumerate;
  EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL    I2CConfiguration;

  //
  // Indicates if the protocols are installed
  //
  BOOLEAN                                          ProtocolsInstalled;

  //
  // Handles
  //
  EFI_HANDLE                                       ControllerHandle;
  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL           *DeviceTreeNode;

  EFI_PHYSICAL_ADDRESS                             BaseAddress;
  BOOLEAN                                          ConfigurationChanged;
  BOOLEAN                                          HighSpeed;
  UINT8                                            PacketId;
  UINT32                                           ControllerId;
  UINTN                                            BusClockHertz;

  UINT32                                           BusId;
  VOID                                             *DeviceTreeBase;
  INT32                                            DeviceTreeNodeOffset;

  //
  // Devices found in device tree
  //
  EFI_I2C_DEVICE                                   I2cDevices[MAX_I2C_DEVICES];
  UINT32                                           SlaveAddressArray[MAX_I2C_DEVICES*MAX_SLAVES_PER_DEVICE];
  UINTN                                            NumberOfI2cDevices;

  UINT32                                           PinControlId;
  BOOLEAN                                          PinControlConfigured;
  BOOLEAN                                          SkipOnExitDisabled;
} NVIDIA_TEGRA_I2C_PRIVATE_DATA;

#define TEGRA_I2C_PRIVATE_DATA_FROM_MASTER(a)     CR(a, NVIDIA_TEGRA_I2C_PRIVATE_DATA, I2cMaster, TEGRA_I2C_SIGNATURE)
#define TEGRA_I2C_PRIVATE_DATA_FROM_ENUMERATE(a)  CR(a, NVIDIA_TEGRA_I2C_PRIVATE_DATA, I2cEnumerate, TEGRA_I2C_SIGNATURE)

/**
 * @addtogroup SPEED_MODES I2C Mode frequencies
 * List of I2C mode frequencies in Hz.
 * @{
 */
#define STD_SPEED      100000UL
#define FM_SPEED       400000UL
#define FM_PLUS_SPEED  1000000UL
#define HS_SPEED       3400000UL
/** @}*/

#define I2C_MAX_PACKET_SIZE     SIZE_4KB
#define I2C_PACKET_HEADER_SIZE  (3 * sizeof(UINT32))

#define I2C_I2C_CNFG_0_OFFSET                   0x00
#define I2C_I2C_CNFG_0_A_MOD                    BIT0
#define I2C_I2C_CNFG_0_LENGTH_SHIFT             1
#define I2C_I2C_CNFG_0_LENGTH_MASK              0xE
#define I2C_I2C_CNFG_0_SLV2                     BIT4
#define I2C_I2C_CNFG_0_START                    BIT5
#define I2C_I2C_CNFG_0_CMD1                     BIT6
#define I2C_I2C_CNFG_0_CMD2                     BIT7
#define I2C_I2C_CNFG_0_NOACK                    BIT8
#define I2C_I2C_CNFG_0_SEND                     BIT9
#define I2C_I2C_CNFG_0_PACKET_MODE_EN           BIT10
#define I2C_I2C_CNFG_0_NEW_MASTER_FSM           BIT11
#define I2C_I2C_CNFG_0_DEBOUNCE_CNT_SHIFT       12
#define I2C_I2C_CNFG_0_DEBOUNCE_CNT_MASK        0x7000
#define I2C_I2C_CNFG_0_MSTR_CLR_BUS_ON_TIMEOUT  BIT15
#define I2C_I2C_CNFG_0_HS_RND_TRIP_DLY_EFFECT   BIT16
#define I2C_I2C_CNFG_0_MULTI_MASTER_MODE        BIT17

#define I2C_I2C_CMD_ADDR0_0_OFFSET          0x04
#define I2C_I2C_CMD_ADDR1_0_OFFSET          0x08
#define I2C_I2C_CMD_DATA1_0_OFFSET          0x0C
#define I2C_I2C_CMD_DATA2_0_OFFSET          0x10
#define I2C_I2C_TLOW_SEXT_0_OFFSET          0x34
#define I2C_I2C_TX_PACKET_FIFO_0_OFFSET     0x50
#define PACKET_HEADER0_HEADER_SIZE_SHIFT    28
#define PACKET_HEADER0_PACKET_ID_SHIFT      16
#define PACKET_HEADER0_CONTROLLER_ID_SHIFT  12
#define PACKET_HEADER0_CONTROLLER_ID_MASK   0xF000
#define PACKET_HEADER0_PROTOCOL_I2C         BIT4

#define I2C_HEADER_HIGHSPEED_MODE     BIT22
#define I2C_HEADER_CONTINUE_ON_NAK    BIT21
#define I2C_HEADER_SEND_START_BYTE    BIT20
#define I2C_HEADER_READ               BIT19
#define I2C_HEADER_10BIT_ADDR         BIT18
#define I2C_HEADER_IE_ENABLE          BIT17
#define I2C_HEADER_REPEAT_START       BIT16
#define I2C_HEADER_CONTINUE_XFER      BIT15
#define I2C_HEADER_MASTER_ADDR_SHIFT  12
#define I2C_HEADER_SLAVE_ADDR_SHIFT   1
#define I2C_HEADER_SLAVE_ADDR_MASK    0x3fe

#define I2C_I2C_RX_FIFO_0_OFFSET  0x54

#define I2C_PACKET_TRANSFER_STATUS_0_OFFSET     0x58
#define PACKET_TRANSFER_COMPLETE                BIT24
#define PACKET_TRANSFER_PKT_ID_SHIFT            16
#define PACKET_TRANSFER_PKT_ID_MASK             0x00FF0000
#define PACKET_TRANSFER_TRANSFER_BYTENUM_SHIFT  4
#define PACKET_TRANSFER_TRANSFER_BYTENUM_MASK   0x0000FFF0
#define PACKET_TRANSFER_NOACK_FOR_ADDR          BIT3
#define PACKET_TRANSFER_NOACK_FOR_DATA          BIT2
#define PACKET_TRANSFER_ARB_LOST                BIT1
#define PACKET_TRANSFER_CONTROLLER_BUSY         BIT0

#define I2C_INTERRUPT_STATUS_REGISTER_0_OFFSET     0x68
#define INTERRUPT_STATUS_BUS_CLEAR_DONE            BIT11
#define INTERRUPT_STATUS_PACKET_XFER_COMPLETE      BIT7
#define INTERRUPT_STATUS_ALL_PACKET_XFER_COMPLETE  BIT6
#define INTERRUPT_STATUS_NOACK                     BIT3
#define INTERRUPT_STATUS_ARB_LOST                  BIT2

#define I2C_I2C_CLK_DIVISOR_REGISTER_0_OFFSET  0x6c
#define I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT    16
#define I2C_CLK_DIVISOR_STD_FAST_MODE_MASK     0xFFFF0000
#define I2C_CLK_DIVISOR_HSMODE_SHIFT           0
#define I2C_CLK_DIVISOR_HSMODE_MASK            0x0000FFFF

#define I2C_I2C_INTERRUPT_SET_REGISTER_0_OFFSET  0x74
#define I2C_I2C_BUS_CLEAR_CONFIG_0_OFFSET        0x84
#define BC_SCLK_THRESHOLD_SHIFT                  16
#define BC_SCLK_THRESHOLD_MASK                   0xFF0000
#define BC_STOP_COND_STOP                        BIT2
#define BC_TERMINATE_IMMEDIATE                   BIT1
#define BC_ENABLE                                BIT0

#define I2C_I2C_CONFIG_LOAD_0_OFFSET               0x8c
#define I2C_I2C_CONFIG_LOAD_0_TIMEOUT_CONFIG_LOAD  BIT2
#define I2C_I2C_CONFIG_LOAD_0_SLV_CONFIG_LOAD      BIT1
#define I2C_I2C_CONFIG_LOAD_0_MSTR_CONFIG_LOAD     BIT0
#define I2C_I2C_CONFIG_LOAD_0_TIMEOUT              20

#define I2C_I2C_CLKEN_OVERRIDE_0_OFFSET            0x90
#define I2C_I2C_INTERFACE_TIMING_0_OFFSET          0x94
#define I2C_I2C_INTERFACE_TIMING_0_THIGH_SHIFT     8
#define I2C_I2C_INTERFACE_TIMING_0_THIGH_MASK      0xFF00
#define I2C_I2C_INTERFACE_TIMING_0_TLOW_SHIFT      0
#define I2C_I2C_INTERFACE_TIMING_0_TLOW_MASK       0x00FF
#define I2C_I2C_HS_INTERFACE_TIMING_0_OFFSET       0x9c
#define I2C_I2C_HS_INTERFACE_TIMING_0_THIGH_SHIFT  8
#define I2C_I2C_HS_INTERFACE_TIMING_0_THIGH_MASK   0xFF00
#define I2C_I2C_HS_INTERFACE_TIMING_0_TLOW_SHIFT   0
#define I2C_I2C_HS_INTERFACE_TIMING_0_TLOW_MASK    0x00FF

#define I2C_I2C_DEBUG_CONTROL_0_OFFSET  0xa4

#define I2C_I2C_MASTER_RESET_CNTRL_0_OFFSET      0xa8
#define I2C_I2C_MASTER_RESET_CNTRL_0_SOFT_RESET  BIT0
#define I2C_SOFT_RESET_DELAY                     5000  // 5 ms

#define I2C_MST_FIFO_CONTROL_0_OFFSET  0xb4
#define TX_FIFO_TRIG_SHIFT             16
#define TX_FIFO_TRIG_MASK              0x7f0000
#define RX_FIFO_TRIG_SHIFT             4
#define RX_FIFO_TRIG_MASK              0x7f0
#define TX_FIFO_FLUSH                  BIT1
#define RX_FIFO_FLUSH                  BIT0

#define I2C_MST_FIFO_STATUS_0_OFFSET  0xb8
#define TX_FIFO_EMPTY_CNT_SHIFT       16
#define TX_FIFO_EMPTY_CNT_MASK        0xFF0000
#define RX_FIFO_FULL_CNT_SHIFT        0
#define RX_FIFO_FULL_CNT_MASK         0x0000FF
#define I2C_TIMEOUT                   (25000 * 2)

#endif
