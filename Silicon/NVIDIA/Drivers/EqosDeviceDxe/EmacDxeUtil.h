/** @file

  Copyright (c) 2019 - 2020, NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Portions provided under the following terms:
  Copyright (c) 2019-2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2020 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/


#ifndef EMAC_DXE_UTIL_H__
#define EMAC_DXE_UTIL_H__

#include <Protocol/SimpleNetwork.h>
#include "osi_core.h"
#include "osi_dma.h"

// Most common CRC32 Polynomial for little endian machines
#define CRC_POLYNOMIAL                                            0xEDB88320
#define HASH_TABLE_REG(n)                                         0x10 + (0x4 * n)
#define RX_MAX_PACKET                                             1600

#define CONFIG_ETH_BUFSIZE                                         2048
#define CONFIG_TX_DESCR_NUM                                        10
#define TX_TOTAL_BUFSIZE                                           (CONFIG_ETH_BUFSIZE * CONFIG_TX_DESCR_NUM)
#define CONFIG_RX_DESCR_NUM                                        10
#define RX_TOTAL_BUFSIZE                                           (CONFIG_ETH_BUFSIZE * CONFIG_RX_DESCR_NUM)

#define MAC_CONFIGURATION_OFFSET                                   0x0
#define MAC_CONFIGURATION_RE                                       BIT0
#define MAC_CONFIGURATION_TE                                       BIT1
#define MAC_CONFIGURATION_PRELEN_SHIFT                             2
#define MAC_CONFIGURATION_PRELEN_MASK                              (0x3 << MAC_CONFIGURATION_PRELEN_SHIFT)
#define MAC_CONFIGURATION_DC                                       BIT4
#define MAC_CONFIGURATION_BL_SHIFT                                 5
#define MAC_CONFIGURATION_BL_MASK                                 (0x3 << MAC_CONFIGURATION_BL_SHIFT)
#define MAC_CONFIGURATION_DR                                       BIT8
#define MAC_CONFIGURATION_DCRS                                     BIT9
#define MAC_CONFIGURATION_DO                                       BIT10
#define MAC_CONFIGURATION_ECRSFD                                   BIT11
#define MAC_CONFIGURATION_LM                                       BIT12
#define MAC_CONFIGURATION_DM                                       BIT13
#define MAC_CONFIGURATION_FES                                      BIT14
#define MAC_CONFIGURATION_PS                                       BIT15
#define MAC_CONFIGURATION_JE                                       BIT16
#define MAC_CONFIGURATION_JD                                       BIT17
#define MAC_CONFIGURATION_BE                                       BIT18
#define MAC_CONFIGURATION_WD                                       BIT19
#define MAC_CONFIGURATION_ACS                                      BIT20
#define MAC_CONFIGURATION_CST                                      BIT21
#define MAC_CONFIGURATION_S2KP                                     BIT22
#define MAC_CONFIGURATION_GPSLCE                                   BIT23
#define MAC_CONFIGURATION_IPG_SHIFT                                24
#define MAC_CONFIGURATION_IPG_MASK                                 (0x7 << MAC_CONFIGURATION_IPG_SHIFT)
#define MAC_CONFIGURATION_IPG_40_BIT_TIMES                         0x7
#define MAC_CONFIGURATION_IPC                                      BIT27
#define MAC_CONFIGURATION_SARC_SHIFT                               28
#define MAC_CONFIGURATION_SARC_MASK                                (0x7 << MAC_CONFIGURATION_SARC_SHIFT)
#define MAC_CONFIGURATION_ARPEN                                    BIT31

#define MAC_PACKET_FILTER_OFFSET                                   0x8
#define MAC_PACKET_FILTER_PR                                       BIT0
#define MAC_PACKET_FILTER_HUC                                      BIT1
#define MAC_PACKET_FILTER_HMC                                      BIT2
#define MAC_PACKET_FILTER_DAIF                                     BIT3
#define MAC_PACKET_FILTER_PM                                       BIT4
#define MAC_PACKET_FILTER_DBF                                      BIT5
#define MAC_PACKET_FILTER_PCF_SHIFT                                6
#define MAC_PACKET_FILTER_PCF_MASK                                 (BIT6|BIT7)
#define MAC_PACKET_FILTER_PCF_FILTER_ALL                           (0)
#define MAC_PACKET_FILTER_PCF_FORWARD_ALL_NO_PAUSE                 (BIT6)
#define MAC_PACKET_FILTER_PCF_FORWARD_ALL                          (BIT7)
#define MAC_PACKET_FILTER_PCF_FORWARD_ALL_PASS_ADDRESS             (BIT6|BIT7)
#define MAC_PACKET_FILTER_SAIF                                     BIT8
#define MAC_PACKET_FILTER_SAF                                      BIT9
#define MAC_PACKET_FILTER_HPF                                      BIT10
#define MAC_PACKET_FILTER_VTFE                                     BIT16
#define MAC_PACKET_FILTER_IPFE                                     BIT20
#define MAC_PACKET_FILTER_DNTU                                     BIT21
#define MAC_PACKET_FILTER_RA                                       BIT31

#define MAC_RXQ_CTRL0_OFFSET                                       0xa0
#define MAC_RXQ_CTRL0_RXQ0EN_SHIFT                                 0
#define MAC_RXQ_CTRL0_RXQ0EN_MASK                                  (BIT0|BIT1)
#define MAC_RXQ_CTRL0_FIELD_NOT_ENABLED                           (0)
#define MAC_RXQ_CTRL0_FIELD_AV                                    (BIT0)
#define MAC_RXQ_CTRL0_FIELD_DCB                                   (BIT1)
#define MAC_RXQ_CTRL0_RXQ1EN_SHIFT                                 2
#define MAC_RXQ_CTRL0_RXQ1EN_MASK                                  (BIT2|BIT3)
#define MAC_RXQ_CTRL0_RXQ2EN_SHIFT                                 4
#define MAC_RXQ_CTRL0_RXQ2EN_MASK                                  (BIT4|BIT5)
#define MAC_RXQ_CTRL0_RXQ3EN_SHIFT                                 6
#define MAC_RXQ_CTRL0_RXQ3EN_MASK                                  (BIT6|BIT7)
#define MAC_RXQ_CTRL0_RXQ4EN_SHIFT                                 8
#define MAC_RXQ_CTRL0_RXQ4EN_MASK                                  (BIT8|BIT9)
#define MAC_RXQ_CTRL0_RXQ5EN_SHIFT                                 10
#define MAC_RXQ_CTRL0_RXQ5EN_MASK                                  (BIT10|BIT11)
#define MAC_RXQ_CTRL0_RXQ6EN_SHIFT                                 12
#define MAC_RXQ_CTRL0_RXQ6EN_MASK                                  (BIT12|BIT13)
#define MAC_RXQ_CTRL0_RXQ7EN_SHIFT                                 14
#define MAC_RXQ_CTRL0_RXQ7EN_MASK                                  (BIT14|BIT15)

#define MAC_RXQ_CTRL1_OFFSET                                       0xa4


#define MAC_HW_FEATURE_1_OFFSET                                    0x120
#define MAC_HW_FEATURE_1_RXFIFOSIZE_SHIFT                          0
#define MAC_HW_FEATURE_1_RXFIFOSIZE_MASK                           (0x1f << MAC_HW_FEATURE_1_RXFIFOSIZE_SHIFT)
#define MAC_HW_FEATURE_1_TXFIFOSIZE_SHIFT                          6
#define MAC_HW_FEATURE_1_TXFIFOSIZE_MASK                           (0x1f << MAC_HW_FEATURE_1_TXFIFOSIZE_SHIFT)
#define MAC_HW_FEATURE_1_OSTEN                                     BIT11
#define MAC_HW_FEATURE_1_PTOEN                                     BIT12
#define MAC_HW_FEATURE_1_ADVTHWORD                                 BIT13
#define MAC_HW_FEATURE_1_ADDR64_SHIFT                              14
#define MAC_HW_FEATURE_1_ADDR64_MASK                               (BIT14|BIT15)
#define MAC_HW_FEATURE_1_ADDR64_32                                 (0)
#define MAC_HW_FEATURE_1_ADDR64_40                                 (BIT14)
#define MAC_HW_FEATURE_1_ADDR64_48                                 (BIT15)
#define MAC_HW_FEATURE_1_DCBEN                                     BIT16
#define MAC_HW_FEATURE_1_SPHEN                                     BIT17
#define MAC_HW_FEATURE_1_TSOEN                                     BIT18
#define MAC_HW_FEATURE_1_DBGMEMA                                   BIT19
#define MAC_HW_FEATURE_1_AVSEL                                     BIT20
#define MAC_HW_FEATURE_1_LPMODEEN                                  BIT23
#define MAC_HW_FEATURE_1_HASHTBLSZ_SHIFT                           24
#define MAC_HW_FEATURE_1_HASHTBLSZ_MASK                            (BIT24|BIT25)
#define MAC_HW_FEATURE_1_HASHTBLSZ_NONE                            0
#define MAC_HW_FEATURE_1_HASHTBLSZ_64                              (BIT24)
#define MAC_HW_FEATURE_1_HASHTBLSZ_128                             (BIT25)
#define MAC_HW_FEATURE_1_HASHTBLSZ_256                             (BIT24|BIT25)
#define MAC_HW_FEATURE_1_L3L4FNUM_SHIFT                            27
#define MAC_HW_FEATURE_1_L3L4FNUM_MASK                             (BIT27|BIT28|BIT29|BIT30)

#define MAC_ADDRESS0_HIGH_OFFSET                                   0x300
#define MAC_ADDRESS0_LOW_OFFSET                                    0x304

#define MTL_OPERATION_MODE_OFFSET                                  0xc00
#define MTL_OPERATION_MODE_DTXSTS                                  BIT1
#define MTL_OPERATION_MODE_RAA                                     BIT2
#define MTL_OPERATION_MODE_SCHALG_MASK                             (BIT5|BIT6)
#define MTL_OPERATION_MODE_SCHALG_WRR                              (0)
#define MTL_OPERATION_MODE_SCHALG_WFQ                              (BIT5)
#define MTL_OPERATION_MODE_SCHALG_DWRR                             (BIT6)
#define MTL_OPERATION_MODE_SCHALG_STRICT                           (BIT5|BIT6)
#define MTL_OPERATION_MODE_CNTPRST                                 BIT8
#define MTL_OPERATION_MODE_CNTCLR                                  BIT9

#define MTL_RXQ_DMA_MAP0_OFFSET                                    0xC30
#define MTL_RXQ_DMA_MAP0_Q3MDMACH_SHIFT                            24
#define MTL_RXQ_DMA_MAP0_Q3MDMACH_MASK                             (0xF << MTL_RXQ_DMA_MAP0_Q3MDMACH_SHIFT)
#define MTL_RXQ_DMA_MAP0_Q2MDMACH_SHIFT                            16
#define MTL_RXQ_DMA_MAP0_Q2MDMACH_MASK                             (0xF << MTL_RXQ_DMA_MAP0_Q2MDMACH_SHIFT)
#define MTL_RXQ_DMA_MAP0_Q1MDMACH_SHIFT                            8
#define MTL_RXQ_DMA_MAP0_Q1MDMACH_MASK                             (0xF << MTL_RXQ_DMA_MAP0_Q1MDMACH_SHIFT)
#define MTL_RXQ_DMA_MAP0_Q0MDMACH_SHIFT                            0
#define MTL_RXQ_DMA_MAP0_Q0MDMACH_MASK                             (0xF << MTL_RXQ_DMA_MAP0_Q0MDMACH_SHIFT)
#define MTL_RXQ_DMA_MAP0_QxMDMACH_DMA_CHANNEL_3                    3
#define MTL_RXQ_DMA_MAP0_QxMDMACH_DMA_CHANNEL_2                    2
#define MTL_RXQ_DMA_MAP0_QxMDMACH_DMA_CHANNEL_1                    1
#define MTL_RXQ_DMA_MAP0_QxMDMACH_DMA_CHANNEL_0                    0


#define TX_OCTET_COUNT_GOOD_BAD_OFFSET                             0x714
#define TX_PACKETS_COUNT_GOOD_BAD_OFFSET                           0x718
#define TX_BROADCAST_PACKETS_GOOD_OFFSET                           0x71c
#define TX_MULTICAST_PACKETS_GOOD_OFFSET                           0x720
#define TX_UNICAST_PACKETS_GOOD_OFFSET                             0x73c
#define TX_LATE_COLLISION_PACKETS_OFFSET                           0x758
#define TX_EXCESSIVE_COLLISION_PACKETS_OFFSET                      0x75c
#define TX_PACKET_COUNT_GOOD_OFFSET                                0x768
#define TX_OVERSIZE_PACKETS_GOOD_OFFSET                            0x778

#define RX_PACKETS_COUNT_GOOD_BAD_OFFSET                           0x780
#define RX_OCTET_COUNT_GOOD_BAD_OFFSET                             0x784
#define RX_BROADCAST_PACKETS_GOOD_OFFSET                           0x78c
#define RX_MULTICAST_PACKETS_GOOD_OFFSET                           0x790
#define RX_CRC_ERROR_PACKETS_OFFSET                                0x794
#define RX_UNDERSIZE_PACKETS_GOOD_OFFSET                           0x7a4
#define RX_OVERSIZE_PACKETS_GOOD_OFFSET                            0x7a8
#define RX_UNICAST_PACKETS_GOOD_OFFSET                             0x7c4



#define MTL_TXQ0_OPERATION_MODE_OFFSET                             0xd00
#define MTL_TXQ0_OPERATION_MODE_FTQ                                BIT0
#define MTL_TXQ0_OPERATION_MODE_TSF                                BIT1
#define MTL_TXQ0_OPERATION_MODE_TXQEN_SHIFT                        2
#define MTL_TXQ0_OPERATION_MODE_TXQEN_MASK                         (BIT2|BIT3)
#define MTL_TXQ0_OPERATION_MODE_TXQEN_NOT_ENABLED                  (0)
#define MTL_TXQ0_OPERATION_MODE_TXQEN_ENABLED                      (BIT3)
#define MTL_TXQ0_OPERATION_MODE_TTC_SHIFT                          4
#define MTL_TXQ0_OPERATION_MODE_TTC_MASK                           (BIT4|BIT5|BIT6)
#define MTL_TXQ0_OPERATION_MODE_TQS_SHIFT                          16
#define MTL_TXQ0_OPERATION_MODE_TQS_MASK                           (0xFFFF << MTL_TXQ0_OPERATION_MODE_TQS_SHIFT)

#define MTL_TXQ0_UNDERFLOW_OFFSET                                  0xd04
#define MTL_TXQ0_DEBUG_OFFSET                                      0xd08
#define MTL_TXQ0_DEBUG_TXQPAUSED                                   BIT0
#define MTL_TXQ0_DEBUG_TRCSTS_SHIFT                                1
#define MTL_TXQ0_DEBUG_TRCSTS_MASK                                 (0x3 << MTL_TXQ0_DEBUG_TRCSTS_SHIFT)
#define MTL_TXQ0_DEBUG_TWCSTS                                      BIT3
#define MTL_TXQ0_DEBUG_TXQSTS                                      BIT4
#define MTL_TXQ0_DEBUG_TXSTSFSTS                                   BIT5
#define MTL_TXQ0_DEBUG_PTXQ_SHIFT                                  16
#define MTL_TXQ0_DEBUG_PTXQ_MASK                                   (0x7 << MTL_TXQ0_DEBUG_PTXQ_SHIFT)
#define MTL_TXQ0_DEBUG_STXSTSF_SHIFT                               20
#define MTL_TXQ0_DEBUG_STXSTSF_MASK                                (0x7 << MTL_TXQ0_DEBUG_STXSTSF_SHIFT)

#define MTL_TXQ0_ETS_STATUS                                        0xd14
#define MTL_TXQ0_QUANTUM_WEIGHT_OFFSET                             0xd18
#define MTL_Q0_INTERUPT_CONTROL_STATUS_OFFSET                      0xd2c
#define MTL_RXQ0_OPERATION_MODE_OFFSET                             0xd30
#define MTL_RXQ0_OPERATION_MODE_RTC_SHIFT                          0
#define MTL_RXQ0_OPERATION_MODE_RTC_MASK                           (BIT0|BIT1)
#define MTL_RXQ0_OPERATION_MODE_RTC_64                             (0)
#define MTL_RXQ0_OPERATION_MODE_RTC_32                             (BIT0)
#define MTL_RXQ0_OPERATION_MODE_RTC_96                             (BIT1)
#define MTL_RXQ0_OPERATION_MODE_RTC_128                            (BIT0|BIT1)
#define MTL_RXQ0_OPERATION_MODE_FUP                                BIT3
#define MTL_RXQ0_OPERATION_MODE_FEP                                BIT4
#define MTL_RXQ0_OPERATION_MODE_RSF                                BIT5
#define MTL_RXQ0_OPERATION_MODE_DIS_TCP_EF                         BIT6
#define MTL_RXQ0_OPERATION_MODE_EHFC                               BIT7
#define MTL_RXQ0_OPERATION_MODE_RFA_SHIFT                          8
#define MTL_RXQ0_OPERATION_MODE_RFA_MASK                           (0x3F << MTL_RXQ0_OPERATION_MODE_RFA_SHIFT)
#define MTL_RXQ0_OPERATION_MODE_RFD_SHIFT                          14
#define MTL_RXQ0_OPERATION_MODE_RFD_MASK                           (0x3F << MTL_RXQ0_OPERATION_MODE_RFD_SHIFT)
#define MTL_RXQ0_OPERATION_MODE_RQS_SHIFT                          20
#define MTL_RXQ0_OPERATION_MODE_RQS_MASK                           (0xFFF << MTL_RXQ0_OPERATION_MODE_RQS_SHIFT)

#define MTL_RXQ0_MISSED_PACKET_OVERFLOW_CNT_OFFSET                 0xd34
#define MTL_RXQ0_DEBUG_OFFSET                                      0xd38
#define MTL_RXQ0_DEBUG_RWCSTS                                      BIT0
#define MTL_RXQ0_DEBUG_RRCSTS_SHIFT                                1
#define MTL_RXQ0_DEBUG_RRCSTS_MASK                                 (0x3 << MTL_RXQ0_DEBUG_RRCSTS_SHIFT)
#define MTL_RXQ0_DEBUG_RXQSTS_SHIFT                                4
#define MTL_RXQ0_DEBUG_RXQSTS_MASK                                 (0x3 << MTL_RXQ0_DEBUG_RXQSTS_SHIFT)
#define MTL_RXQ0_DEBUG_PRXQ_SHIFT                                  16
#define MTL_RXQ0_DEBUG_PRXQ_MASK                                   (0x3f << MTL_RXQ0_DEBUG_PRXQ_SHIFT)

#define MTL_RXQ0_CONTROL_OFFSET                                    0xd3c

#define DMA_MODE_OFFSET                                            0x1000
#define DMA_MODE_SWR                                               BIT0
#define DMA_MODE_DA                                                BIT1
#define DMA_MODE_TAA_SHIFT                                         2
#define DMA_MODE_TAA_MASK                                          (0x7 << DMA_MODE_TAA_SHIFT)
#define DMA_MODE_TXPR                                              BIT11
#define DMA_MODE_PR_SHIFT                                          12
#define DMA_MODE_PR_MASK                                           (0x7 << DMA_MODE_PR_SHIFT)
#define DMA_MODE_INTM_SHIFT                                        16
#define DMA_MODE_INTM_MASK                                         (0x3 << DMA_MODE_INTM_SHIFT)

#define DMA_SYSBUS_MODE_OFFSET                                     0x1004
#define DMA_SYSBUS_MODE_FB                                         BIT0
#define DMA_SYSBUS_MODE_BLEN4                                      BIT1
#define DMA_SYSBUS_MODE_BLEN8                                      BIT2
#define DMA_SYSBUS_MODE_BLEN16                                     BIT3
#define DMA_SYSBUS_MODE_BLEN32                                     BIT4
#define DMA_SYSBUS_MODE_BLEN64                                     BIT5
#define DMA_SYSBUS_MODE_BLEN128                                    BIT6
#define DMA_SYSBUS_MODE_BLEN256                                    BIT7
#define DMA_SYSBUS_MODE_EAME                                       BIT11
#define DMA_SYSBUS_MODE_AAL                                        BIT12
#define DMA_SYSBUS_MODE_ONEKBBE                                    BIT13
#define DMA_SYSBUS_MODE_MB                                         BIT14
#define DMA_SYSBUS_MODE_RB                                         BIT15
#define DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT                           16
#define DMA_SYSBUS_MODE_RD_OSR_LMT_MASK                            (0xFF << DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT)
#define DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT                           24
#define DMA_SYSBUS_MODE_WR_OSR_LMT_MASK                            (0x3F << DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT)
#define DMA_SYSBUS_MODE_LPI_XIT_PKT                                BIT30
#define DMA_SYSBUS_MODE_EN_LPI                                     BIT31


#define DMA_CH0_CONTROL_OFFSET                                     0x1100
#define DMA_CH0_TX_CONTROL_OFFSET                                  0x1104
#define DMA_CH0_TX_CONTROL_ST                                      BIT0
#define DMA_CH0_TX_CONTROL_TCW_SHIFT                               1
#define DMA_CH0_TX_CONTROL_TCW_MASK                                (0x7 << DMA_CH0_TX_CONTROL_TCW_SHIFT)
#define DMA_CH0_TX_CONTROL_OSF                                     BIT4
#define DMA_CH0_TX_CONTROL_RTS                                     BIT5
#define DMA_CH0_TX_CONTROL_TSE                                     BIT12
#define DMA_CH0_TX_CONTROL_IPBL                                    BIT15
#define DMA_CH0_TX_CONTROL_TXPBL_SHIFT                             16
#define DMA_CH0_TX_CONTROL_TXPBL_MASK                              (0x3f << DMA_CH0_TX_CONTROL_TXPBL_SHIFT)
#define DMA_CH0_TX_CONTROL_TQOS_SHIFT                              24
#define DMA_CH0_TX_CONTROL_TQOS_MASK                               (0xf << DMA_CH0_TX_CONTROL_TQOS_SHIFT)

#define DMA_CH0_RX_CONTROL_OFFSET                                  0x1108
#define DMA_CH0_RX_CONTROL_SR                                      BIT0
#define DMA_CH0_RX_CONTROL_RBSZ_SHIFT                              1
#define DMA_CH0_RX_CONTROL_RBSZ_MASK                               (0x3f << DMA_CH0_RX_CONTROL_RBSZ_SHIFT)
#define DMA_CH0_RX_CONTROL_RXPBL_SHIFT                             16
#define DMA_CH0_RX_CONTROL_RXPBL_MASK                              (0x3f << DMA_CH0_RX_CONTROL_RXPBL_SHIFT)
#define DMA_CH0_RX_CONTROL_RQOS_SHIFT                              24
#define DMA_CH0_RX_CONTROL_RQOS_MASK                               (0xf << DMA_CH0_RX_CONTROL_RQOS_SHIFT)
#define DMA_CH0_RX_CONTROL_RPF                                     BIT31

#define DMA_CH0_TXDESC_LIST_HADDRESS_OFFSET                        0x1110
#define DMA_CH0_TXDESC_LIST_ADDRESS_OFFSET                         0x1114
#define DMA_CH0_RXDESC_LIST_HADDRESS_OFFSET                        0x1118
#define DMA_CH0_RXDESC_LIST_ADDRESS_OFFSET                         0x111c
#define DMA_CH0_TXDESC_TAIL_POINTER_OFFSET                         0x1120
#define DMA_CH0_RXDESC_TAIL_POINTER_OFFSET                         0x1128
#define DMA_CH0_TXDESC_RING_LENGTH_OFFSET                          0x112c
#define DMA_CH0_RXDESC_RING_LENGTH_OFFSET                          0x1130
#define DMA_CH0_INTERRUPT_ENABLE_OFFSET                            0x1134
#define DMA_CH0_INTERRUPT_ENABLE_TIE                               BIT0
#define DMA_CH0_INTERRUPT_ENABLE_TXSE                              BIT1
#define DMA_CH0_INTERRUPT_ENABLE_TBUE                              BIT2
#define DMA_CH0_INTERRUPT_ENABLE_RIE                               BIT6
#define DMA_CH0_INTERRUPT_ENABLE_RBUE                              BIT7
#define DMA_CH0_INTERRUPT_ENABLE_RSE                               BIT8
#define DMA_CH0_INTERRUPT_ENABLE_RWTE                              BIT9
#define DMA_CH0_INTERRUPT_ENABLE_ETIE                              BIT1
#define DMA_CH0_INTERRUPT_ENABLE_ERIE                              BIT11
#define DMA_CH0_INTERRUPT_ENABLE_FBEE                              BIT12
#define DMA_CH0_INTERRUPT_ENABLE_CDEE                              BIT13
#define DMA_CH0_INTERRUPT_ENABLE_AIE                               BIT14
#define DMA_CH0_INTERRUPT_ENABLE_NIE                               BIT15

#define DMA_CH0_INTERRUPT_WATCHDOG_TIMER_OFFSET                    0x1138
#define DMA_CH0_SLOT_FUNCTION_CONTROL_STATUS_OFFSET                0x113c
#define DMA_CH0_CURRENT_APP_TXDESC_OFFSET                          0x1144
#define DMA_CH0_CURRENT_APP_RXDESC_OFFSET                          0x114c
#define DMA_CH0_CURRENT_APP_TXBUFFER_H_OFFSET                      0x1150
#define DMA_CH0_CURRENT_APP_TXBUFFER_OFFSET                        0x1154
#define DMA_CH0_CURRENT_APP_RXBUFFER_H_OFFSET                      0x1158
#define DMA_CH0_CURRENT_APP_RXBUFFER_OFFSET                        0x115c

#define DMA_CH0_STATUS_OFFSET                                      0x1160
#define DMA_CH0_STATUS_TI                                          BIT0
#define DMA_CH0_STATUS_TPS                                         BIT1
#define DMA_CH0_STATUS_TBU                                         BIT2
#define DMA_CH0_STATUS_RI                                          BIT6
#define DMA_CH0_STATUS_RBU                                         BIT7
#define DMA_CH0_STATUS_RPS                                         BIT8
#define DMA_CH0_STATUS_RWT                                         BIT9
#define DMA_CH0_STATUS_ETI                                         BIT10
#define DMA_CH0_STATUS_ERI                                         BIT11
#define DMA_CH0_STATUS_FBE                                         BIT12
#define DMA_CH0_STATUS_CDE                                         BIT13
#define DMA_CH0_STATUS_AIS                                         BIT14
#define DMA_CH0_STATUS_NIS                                         BIT15
#define DMA_CH0_STATUS_TEB_SHIFT                                   16
#define DMA_CH0_STATUS_TEB_MASK                                    (BIT16|BIT17|BIT18)
#define DMA_CH0_STATUS_REB_SHIFT                                   19
#define DMA_CH0_STATUS_REB_MASK                                    (BIT19|BIT20|BIT21)





#define DMA_CH0_MISS_FRAME_CNT_OFFSET                              0x116c


typedef PACKED struct {
  UINT32 Des0;
  UINT32 Des1;
  UINT32 Des2;
  UINT32 Des3;
} DESIGNWARE_HW_DESCRIPTOR;

#define RDES_3_READ_OWN                                          BIT31
#define RDES_3_READ_IOC                                          BIT30
#define RDES_3_READ_BUF2V                                        BIT25
#define RDES_3_READ_BUF1V                                        BIT24

#define RDES_2_WB_DAF                                            BIT17
#define RDES_2_WB_SAF                                            BIT16

#define RDES_3_WB_OWN                                            BIT31
#define RDES_3_WB_CTXT                                           BIT30
#define RDES_3_WB_FD                                             BIT29
#define RDES_3_WB_LD                                             BIT28
#define RDES_3_WB_RS2V                                           BIT27
#define RDES_3_WB_RS1V                                           BIT26
#define RDES_3_WB_RS0V                                           BIT25
#define RDES_3_WB_CE                                             BIT24
#define RDES_3_WB_GP                                             BIT23
#define RDES_3_WB_RWT                                            BIT22
#define RDES_3_WB_OE                                             BIT21
#define RDES_3_WB_RE                                             BIT20
#define RDES_3_WB_DE                                             BIT19
#define RDES_3_WB_LT_MASK                                        (BIT16|BIT17|BIT18)
#define RDES_3_WB_LT_LENGTH_PACKET                               (0)
#define RDES_3_WB_LT_TYPE_PACKET                                 (BIT16)
#define RDES_3_WB_LT_ARP_PACKET                                  (BIT16|BIT17)
#define RDES_3_WB_LT_VLAN_PACKET                                 (BIT18)
#define RDES_3_WB_LT_DOUBLE_VLAN_PACKET                          (BIT16|BIT18)
#define RDES_3_WB_LT_MAC_CONTROL_PACKET                          (BIT17|BIT18)
#define RDES_3_WB_LT_OAM_PACKET                                  (BIT16|BIT17|BIT18)
#define RDES_3_WB_ES                                             BIT15
#define RDES_3_WB_PL_SHIFT                                        (0)
#define RDES_3_WB_PL_MASK                                        (0x7fff)


#define TDES_2_READ_IOC                                          BIT31
#define TDES_2_READ_TTSE                                         BIT30
#define TDES_2_READ_B2L_SHIFT                                    16
#define TDES_2_READ_B2L_MASK                                     (0x3F << TDES_2_READ_B2L_SHIFT)
#define TDES_2_READ_VTIR_MASK                                    (BIT14|BIT15)
#define TDES_2_READ_VTIR_NO_TAG                                  (0)
#define TDES_2_READ_VTIR_REMOVE_TAG                              (BIT14)
#define TDES_2_READ_VTIR_INSERT_TAG                              (BIT15)
#define TDES_2_READ_VTIR_REPLACE_TAG                             (BIT14|BIT15)
#define TDES_2_READ_B1L_SHIFT                                    0
#define TDES_2_READ_B1L_MASK                                     (0x3ff << TDES_2_READ_B1L_SHIFT)

#define TDES_3_READ_OWN                                          BIT31
#define TDES_3_READ_CTXT                                         BIT30
#define TDES_3_READ_FD                                           BIT29
#define TDES_3_READ_LD                                           BIT28
#define TDES_3_READ_CPC_SHIFT                                    26
#define TDES_3_READ_CPC_MASK                                     (BIT26|BIT27)
#define TDES_3_READ_CPC_CRC_PAD_INSERT                           (0)
#define TDES_3_READ_CPC_CRC_INSERT                               (BIT26)
#define TDES_3_READ_CPC_DISABLE_CRC_INSERT                       (BIT27)
#define TDES_3_READ_CPC_CRC_REPLACEMENT                          (BIT26|BIT27)
#define TDES_3_READ_SAIC_SHIFT                                   23
#define TDES_3_READ_SAIC_MASK                                    (BIT23|BIT24|BIT25)
#define TDES_3_READ_SLOTNUM_THL_SHIFT                            19
#define TDES_3_READ_SLOTNUM_THL_MASK                             (BIT19|BIT20|BIT21|BIT22)
#define TDES_3_READ_TSE                                          BIT18
#define TDES_3_READ_CIC_TPL_SHIFT                                16
#define TDES_3_READ_CIC_TPL_MASK                                 (BIT16|BIT17)
#define TDES_3_READ_TPL                                          BIT15
#define TDES_3_READ_FL_TPL_SHIFT                                 0
#define TDES_3_READ_FL_TPL_MASK                                  0x7fff

#define TDES_3_WB_OWN                                            BIT31


typedef struct {
  struct osi_core_priv_data   *osi_core;
  struct osi_dma_priv_data    *osi_dma;
  struct osi_hw_features      hw_feat;
  void                        *tx_ring_dma_mapping;
  void                        *rx_ring_dma_mapping;
  void                        *rx_buffer_dma_mapping[RX_DESC_CNT];
  void                        *tx_buffer_dma_mapping[TX_DESC_CNT];
  void                        *tx_buffers[TX_DESC_CNT];
  void                        *tx_buffers_phy_addr[TX_DESC_CNT];
} EMAC_DRIVER;

VOID
EFIAPI
EmacSetMacAddress (
  IN  EFI_MAC_ADDRESS         *MacAddress,
  IN  UINTN                   MacBaseAddress
  );

VOID
EFIAPI
EmacReadMacAddress (
  OUT EFI_MAC_ADDRESS         *MacAddress,
  IN  UINTN                   MacBaseAddress
  );

EFI_STATUS
EFIAPI
EmacDxeInitialization (
  IN  EMAC_DRIVER             *EmacDriver,
  IN  UINTN                   MacBaseAddress
  );

EFI_STATUS
EFIAPI
EmacRxFilters (
  IN  UINT32                  ReceiveFilterSetting,
  IN  BOOLEAN                 Reset,
  IN  UINTN                   NumMfilter             OPTIONAL,
  IN  EFI_MAC_ADDRESS         *Mfilter               OPTIONAL,
  IN  UINTN                   MacBaseAddress
  );

UINT32
EFIAPI
GenEtherCrc32 (
  IN  EFI_MAC_ADDRESS         *Mac,
  IN  UINT32 AddrLen
  );

UINT8
EFIAPI
BitReverse (
  UINT8                       Value
  );

VOID
EFIAPI
EmacStopTxRx (
  IN  EMAC_DRIVER             *MacDriver
  );

VOID
EFIAPI
EmacGetDmaStatus (
  OUT UINT32                  *IrqStat  OPTIONAL,
  IN  UINTN                   MacBaseAddress
  );

VOID
EFIAPI
EmacGetStatistic (
  IN  EFI_NETWORK_STATISTICS *Stats,
  IN  UINTN                  MacBaseAddress
  );

VOID
EFIAPI
EmacConfigAdjust (
  IN  UINT32                 Speed,
  IN  UINT32                 Duplex,
  IN  UINTN                  MacBaseAddress
  );

#endif // EMAC_DXE_UTIL_H__
