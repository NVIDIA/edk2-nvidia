/*
 * SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef INCLUDED_MGBE_CORE_H_
#define INCLUDED_MGBE_CORE_H_

#ifndef OSI_STRIPPED_LIB
#define MGBE_MAC_RX_FLW_CTRL  0x0090
#define MGBE_MAC_RQC2R        0x00A8
#define MGBE_MAC_QX_TX_FLW_CTRL(x)  ((0x0004U * (x)) + 0x0070U)
#define MGBE_MAC_ARPPA           0x0C10
#define MGBE_MAC_LPI_CSR         0x00D0
#define MGBE_MAC_LPI_TIMER_CTRL  0x00D4
#define MGBE_MAC_LPI_EN_TIMER    0x00D8
#define MGBE_MAC_RSS_CTRL        0x0C80
#define MGBE_MAC_RSS_ADDR        0x0C88
#define MGBE_MAC_RSS_DATA        0x0C8C
#define MGBE_MAC_PTO_CR          0x0DC0
#define MGBE_MAC_PIDR0           0x0DC4
#define MGBE_MAC_PIDR1           0x0DC8
#define MGBE_MAC_PIDR2           0x0DCC
#define MGBE_MAC_PMTCSR          0x00C0
#define MGBE_MAC_HTR_REG(x)  ((0x0004U * (x)) + 0x0010U)
#define MGBE_WRAP_AXI_ASID0_CTRL  0x8400
#define MGBE_WRAP_AXI_ASID1_CTRL  0x8404
#define MGBE_WRAP_AXI_ASID2_CTRL  0x8408
#define MGBE_MAC_PFR_DHLFRS       OSI_BIT(12)
#define MGBE_MAC_PFR_DHLFRS_MASK  (OSI_BIT(12) | OSI_BIT(11))
#define MGBE_MAC_PFR_VTFE         OSI_BIT(16)
#define MGBE_MAC_PFR_IPFE         OSI_BIT(20)
#define MGBE_MAC_PFR_IPFE_SHIFT   20
#define MGBE_SID_VAL1(x)  (((x) << 24U) |                      \
                                                 ((x) << 16U) |\
                                                 ((x) << 8U) |\
                                                 (x))
#define MGBE_SID_VAL2(x)  (((x) << 8U) |                      \
                                                 (x))
#define MGBE0_SID                    ((nveu32_t)0x6U)
#define MGBE1_SID                    ((nveu32_t)0x49U)
#define MGBE2_SID                    ((nveu32_t)0x4AU)
#define MGBE3_SID                    ((nveu32_t)0x4BU)
#define MGBE0_SID_T264               ((nveu32_t)0x0U)
#define MGBE1_SID_T264               ((nveu32_t)0x0U)
#define MGBE2_SID_T264               ((nveu32_t)0x0U)
#define MGBE3_SID_T264               ((nveu32_t)0x0U)
#define MGBE_MAC_PAUSE_TIME          0xFFFF0000U
#define MGBE_MAC_PAUSE_TIME_MASK     0xFFFF0000U
#define MGBE_MAC_VLAN_TR_VTHM        OSI_BIT(25)
#define MGBE_MAC_VLAN_TR_VTIM        OSI_BIT(17)
#define MGBE_MAC_VLAN_TR_VTIM_SHIFT  17

/**
 * @addtogroup MGBE MAC hash table defines
 *
 * @brief MGBE MAC hash table Control register
 * filed type defines.
 * @{
 */
#define MGBE_MAX_HTR_REGS  4U
/** @} */

#define MGBE_MAX_VLAN_FILTER           32U
#define MGBE_MAC_RX_FLW_CTRL_RFE       OSI_BIT(0)
#define MGBE_MAC_TCR_SNAPTYPSEL_SHIFT  16U
#define MGBE_MAC_TCR_TSENMACADDR       OSI_BIT(18)
#define MGBE_MAC_RQC1R_PTPQ_SHIFT      24U
#define MGBE_MAC_RQC1R_PTPQ            (OSI_BIT(27) | OSI_BIT(26) |          \
                                                 OSI_BIT(25) | OSI_BIT(24))
#define MGBE_PKTID_MASK                (OSI_BIT(9) | OSI_BIT(8) |          \
                                                 OSI_BIT(7) | OSI_BIT(6) | \
                                                 OSI_BIT(5) | OSI_BIT(4) | \
                                                 OSI_BIT(3) | OSI_BIT(2) | \
                                                 OSI_BIT(1) | OSI_BIT(0))
/* T264 VDMA ID bits */
#define MGBE_VDMAID_MASK               (OSI_BIT(23) | OSI_BIT(22) |          \
                                                 OSI_BIT(21) | OSI_BIT(20) | \
                                                 OSI_BIT(19) | OSI_BIT(18) | \
                                                 OSI_BIT(17) | OSI_BIT(16))
#define MGBE_MAC_RMCR_LM               OSI_BIT(10)
#define MGBE_MAC_RMCR_ARPEN            OSI_BIT(31)
#define MGBE_MAC_QX_TX_FLW_CTRL_TFE    OSI_BIT(1)
#define MGBE_MAC_TMCR_IFP              OSI_BIT(11)
#define MGBE_MAC_TMCR_IPG              OSI_BIT(8) | OSI_BIT(9)
#define MGBE_MAC_RQC1R_TPQC0           OSI_BIT(21)
#define MGBE_MAC_RQC1R_OMCBCQ          OSI_BIT(20)
#define MGBE_MAC_RSS_CTRL_RSSE         OSI_BIT(0)
#define MGBE_MAC_RSS_CTRL_IP2TE        OSI_BIT(1)
#define MGBE_MAC_RSS_CTRL_TCP4TE       OSI_BIT(2)
#define MGBE_MAC_RSS_CTRL_UDP4TE       OSI_BIT(3)
#define MGBE_MAC_RSS_ADDR_ADDRT        OSI_BIT(2)
#define MGBE_MAC_RSS_ADDR_RSSIA_SHIFT  8U
#define MGBE_MAC_RSS_ADDR_OB           OSI_BIT(0)
#define MGBE_MAC_RSS_ADDR_CT           OSI_BIT(1)

/**
 * @addtogroup - MGBE-LPI LPI configuration macros
 *
 * @brief LPI timers and config register field masks.
 * @{
 */
/* LPI LS timer - minimum time (in milliseconds) for which the link status from
 * PHY should be up before the LPI pattern can be transmitted to the PHY.
 * Default 1sec.
 */
#define MGBE_DEFAULT_LPI_LS_TIMER  ((nveu32_t)1000)
#define MGBE_LPI_LS_TIMER_MASK     0x3FFU
#define MGBE_LPI_LS_TIMER_SHIFT    16U

/* LPI TW timer - minimum time (in microseconds) for which MAC wait after it
 * stops transmitting LPI pattern before resuming normal tx.
 * Default 21us
 */
#define MGBE_DEFAULT_LPI_TW_TIMER  0x15U
#define MGBE_LPI_TW_TIMER_MASK     0xFFFFU

/* LPI entry timer - Time in microseconds that MAC will wait to enter LPI mode
 * after all tx is complete.
 * Default 1sec.
 */
#define MGBE_LPI_ENTRY_TIMER_MASK  0xFFFF8U

/* 1US TIC counter - This counter should be programmed with the number of clock
 * cycles of CSR clock that constitutes a period of 1us.
 * it should be APB clock in MHZ i.e 480-1 for silicon and 13MHZ-1 for uFPGA
 */
#define MGBE_1US_TIC_COUNTER    0x1DF
#define MGBE_MAC_1US_TIC_COUNT  0x00DC
/** @} */
#define MGBE_MAC_PTO_CR_PTOEN     OSI_BIT(0)
#define MGBE_MAC_PTO_CR_ASYNCEN   OSI_BIT(1)
#define MGBE_MAC_PTO_CR_APDREQEN  OSI_BIT(2)
#define MGBE_MAC_PTO_CR_DN        (OSI_BIT(15) | OSI_BIT(14) |               \
                                                 OSI_BIT(13) | OSI_BIT(12) | \
                                                 OSI_BIT(11) | OSI_BIT(10) | \
                                                 OSI_BIT(9) | OSI_BIT(8))
#define MGBE_MAC_PTO_CR_DN_SHIFT  8U
#define MGBE_DMA_CHX_STATUS_RPS   OSI_BIT(8)
#define MGBE_DMA_CHX_STATUS_TPS   OSI_BIT(1)
#define MGBE_DMA_CHX_STATUS_TBU   OSI_BIT(2)
#define MGBE_DMA_CHX_STATUS_RBU   OSI_BIT(7)
#define MGBE_DMA_CHX_STATUS_FBE   OSI_BIT(12)

#define MGBE_MAC_LPI_CSR_LPITE    OSI_BIT(20)
#define MGBE_MAC_LPI_CSR_LPITXA   OSI_BIT(19)
#define MGBE_MAC_LPI_CSR_PLS      OSI_BIT(17)
#define MGBE_MAC_LPI_CSR_LPIEN    OSI_BIT(16)
#define MGBE_MAC_LPI_STATUS_MASK  0xF0FU
#define MGBE_MAC_PFR_VTFE_SHIFT   16
#define MGBE_MAC_PIDR_PID_MASK    0XFFFFU

#define MGBE_MTL_RXP_BYPASS_CNT  2U
#define MGBE_MAC_FPE_CTS_SVER    OSI_BIT(1)

#endif /* !OSI_STRIPPED_LIB */

#define MGBE_PKTID_MASK  (OSI_BIT(9) | OSI_BIT(8) |                        \
                                                 OSI_BIT(7) | OSI_BIT(6) | \
                                                 OSI_BIT(5) | OSI_BIT(4) | \
                                                 OSI_BIT(3) | OSI_BIT(2) | \
                                                 OSI_BIT(1) | OSI_BIT(0))
/* T264 VDMA ID bits */
#define MGBE_VDMAID_MASK  (OSI_BIT(23) | OSI_BIT(22) |                       \
                                                 OSI_BIT(21) | OSI_BIT(20) | \
                                                 OSI_BIT(19) | OSI_BIT(18) | \
                                                 OSI_BIT(17) | OSI_BIT(16))

#define MGBE_VDMAID_OFFSET        16U
#define MGBE_MAC_PFR_DHLFRS       OSI_BIT(12)
#define MGBE_MAC_PFR_DHLFRS_MASK  (OSI_BIT(12) | OSI_BIT(11))

#define MGBE_CORE_MAC_STSR             0x0D08
#define MGBE_CORE_MAC_STNSR            0x0D0C
#define MGBE_CORE_MAC_STNSR_TSSS_MASK  0x7FFFFFFFU
#define MGBE_MAC_TMCR_IPG_MASK         0x700U
#define MGBE_MAC_TMCR_IFP              OSI_BIT(11)
#define MGBE_MAC_TMCR_IPG              OSI_BIT(8) | OSI_BIT(9)
#define MGBE_MAC_RX_TX_STS             0x00B8
#define MGBE_MTL_EST_CONTROL           0x1050
#define MGBE_MTL_EST_OVERHEAD          0x1054
#define MGBE_MTL_EST_STATUS            0x1058
#define MGBE_MTL_EST_SCH_ERR           0x1060
#define MGBE_MTL_EST_FRMS_ERR          0x1064
#define MGBE_MTL_EST_ITRE              0x1070
#define MGBE_MTL_EST_GCL_CONTROL       0x1080
#define MGBE_MTL_EST_DATA              0x1084
#define MGBE_MAC_RQC4R                 0x0094
#define MGBE_MAC_FPE_CTS               0x0280
#define MGBE_MTL_RXP_CS                0x10A0
#define MGBE_MTL_RXP_INTR_CS           0x10A4
#define MGBE_MTL_RXP_IND_CS            0x10B0
#define MGBE_MTL_RXP_IND_DATA          0x10B4

#define MGBE_MAC_TX_PCE  OSI_BIT(13)
#define MGBE_MAC_TX_IHE  OSI_BIT(12)
#define MGBE_MAC_TX_TJT  OSI_BIT(0)
#define MGBE_MTL_TCQ_ETS_HCR(x)   ((0x0080U * (x)) + 0x1120U)
#define MGBE_MTL_TCQ_ETS_LCR(x)   ((0x0080U * (x)) + 0x1124U)
#define MGBE_MTL_TCQ_ETS_SSCR(x)  ((0x0080U * (x)) + 0x111CU)
#define MGBE_MTL_OP_MODE      0x1000
#define MGBE_MTL_INTR_STATUS  0x1020
#define MGBE_MTL_FPE_CTS      0x1090
#define MGBE_MTL_FPE_ADV      0x1094

#define MGBE_MTL_QINT_STATUS(x)  ((0x0080U * (x)) + 0x1174U)
#define MGBE_MTL_TCQ_ETS_CR_AVALG_SHIFT    0U
#define MGBE_MTL_QINT_TXUNIFS              OSI_BIT(0)
#define MGBE_MTL_TX_OP_MODE_Q2TCMAP        (OSI_BIT(10) | OSI_BIT(9) |     \
                                                 OSI_BIT(8))
#define MGBE_MTL_TX_OP_MODE_Q2TCMAP_SHIFT  8U
#define MGBE_MTL_TX_OP_MODE_TXQEN          (OSI_BIT(3) | OSI_BIT(2))
#define MGBE_MTL_TX_OP_MODE_TXQEN_SHIFT    2U
#define MGBE_MTL_TCQ_ETS_CR_CC             OSI_BIT(3)
#define MGBE_MTL_TCQ_ETS_CR_CC_SHIFT       3U
#define MGBE_MTL_TCQ_ETS_QW_ISCQW_MASK     0x001FFFFFU
#define MGBE_MTL_TCQ_ETS_SSCR_SSC_MASK     0x0000FFFFU
#define MGBE_MTL_TCQ_ETS_HCR_HC_MASK       0x1FFFFFFFU
#define MGBE_MTL_TCQ_ETS_LCR_LC_MASK       0x1FFFFFFFU

#define MGBE_8PTP_CYCLE              26U
#define MGBE_PTP_CLK_SPEED           312500000U
#define MGBE_DMA_ISR_MTLIS           OSI_BIT(16)
#define MGBE_IMR_TXESIE              OSI_BIT(13)
#define MGBE_IMR_FPEIE               OSI_BIT(15)
#define MGBE_MAC_EXT_CNF_EIPG        0x1U
#define MGBE_MAC_EXT_CNF_EIPG_MASK   0x7FU
#define MGBE_MAC_RQC4R_PMCBCQ        (OSI_BIT(27) | OSI_BIT(26) |            \
                                                 OSI_BIT(25) | OSI_BIT(24))
#define MGBE_MAC_RQC4R_PMCBCQ_SHIFT  24U
#define MGBE_MAC_RQC1R_RQ_SHIFT      4U
#define MGBE_MTL_EST_EEST            OSI_BIT(0)
/* EST GCL controlOSI_BITmap */
#define MGBE_MTL_EST_ADDR_SHIFT  8
/*EST MTL interrupt STATUS and ERR*/
#define MGBE_MTL_IS_ESTIS  OSI_BIT(18)
/* MTL_EST_STATUS*/
#define MGBE_MTL_EST_STATUS_CGCE  OSI_BIT(4)
#define MGBE_MTL_EST_STATUS_HLBS  OSI_BIT(3)
#define MGBE_MTL_EST_STATUS_HLBF  OSI_BIT(2)
#define MGBE_MTL_EST_STATUS_BTRE  OSI_BIT(1)
#define MGBE_MTL_EST_STATUS_SWLC  OSI_BIT(0)
/* MAC FPE control/statusOSI_BITmap */
#define MGBE_MAC_FPE_CTS_EFPE  OSI_BIT(0)
#define MGBE_MAC_FPE_CTS_TRSP  OSI_BIT(19)
#define MGBE_MAC_FPE_CTS_TVER  OSI_BIT(18)
#define MGBE_MAC_FPE_CTS_RVER  OSI_BIT(16)
#define MGBE_MAC_FPE_CTS_SRSP  OSI_BIT(2)
/* MTL FPE adv registers */
#define MGBE_MAC_IMR_FPEIS     OSI_BIT(16)
#define MGBE_MAC_FPE_CTS_RRSP  OSI_BIT(17)
/* MTL_EST_CONTROL */
#define MGBE_MTL_EST_CONTROL_PTOV        (OSI_BIT(23) | OSI_BIT(24) |        \
                                                 OSI_BIT(25) | OSI_BIT(26) | \
                                                 OSI_BIT(27) | OSI_BIT(28) | \
                                                 OSI_BIT(29) | OSI_BIT(30) | \
                                                 OSI_BIT(31))
#define MGBE_MTL_EST_CONTROL_PTOV_SHIFT  23U
#define MGBE_MTL_EST_PTOV_RECOMMEND      32U
#define MGBE_MTL_EST_CONTROL_CTOV        (OSI_BIT(11) | OSI_BIT(12) |        \
                                                 OSI_BIT(13) | OSI_BIT(14) | \
                                                 OSI_BIT(15) | OSI_BIT(16) | \
                                                 OSI_BIT(17) | OSI_BIT(18) | \
                                                 OSI_BIT(19) | OSI_BIT(20) | \
                                                 OSI_BIT(21) | OSI_BIT(22))
#define MGBE_MTL_EST_CONTROL_CTOV_SHIFT  11U
#define MGBE_MTL_EST_CTOV_RECOMMEND      42U
#define MGBE_MAC_RQC1R_RQ                (OSI_BIT(7) | OSI_BIT(6) |        \
                                                 OSI_BIT(5) | OSI_BIT(4))

/**
 * @addtogroup MGBE-MTL-FRP FRP Indirect Access register defines
 *
 * @brief MGBE MTL FRP register defines
 * @{
 */
#define MGBE_MTL_FRP_READ_UDELAY  1U
#define MGBE_MTL_FRP_READ_RETRY   1000U

#define MGBE_MTL_OP_MODE_FRPE  OSI_BIT(15)
/* FRP Control and Status register defines */
#define MGBE_MTL_RXP_CS_RXPI       OSI_BIT(31)
#define MGBE_MTL_RXP_CS_NPE        (OSI_BIT(23) | OSI_BIT(22) |              \
                                                 OSI_BIT(21) | OSI_BIT(20) | \
                                                 OSI_BIT(19) | OSI_BIT(18) | \
                                                 OSI_BIT(17) | OSI_BIT(16))
#define MGBE_MTL_RXP_CS_NPE_SHIFT  16U
#define MGBE_MTL_RXP_CS_NVE        (OSI_BIT(7) | OSI_BIT(6) |              \
                                                 OSI_BIT(5) | OSI_BIT(4) | \
                                                 OSI_BIT(3) | OSI_BIT(2) | \
                                                 OSI_BIT(1) | OSI_BIT(0))

#define MGBE_MTL_RXP_CS_CLR_ANP       ~(OSI_BIT(25) | OSI_BIT(26) | OSI_BIT(27))
#define MGBE_MTL_RXP_CS_ANP           (OSI_BIT(25) | OSI_BIT(26))
#define MGBE_MTL_RCHlist_READ_UDELAY  1U
#define MGBE_MTL_RCHlist_READ_RETRY   1000U

/* FRP Interrupt Control and Status register */
#define MGBE_MTL_RXP_INTR_CS_PDRFIE   OSI_BIT(19)
#define MGBE_MTL_RXP_INTR_CS_FOOVIE   OSI_BIT(18)
#define MGBE_MTL_RXP_INTR_CS_NPEOVIE  OSI_BIT(17)
#define MGBE_MTL_RXP_INTR_CS_NVEOVIE  OSI_BIT(16)
#define MGBE_MTL_RXP_INTR_CS_PDRFIS   OSI_BIT(3)
#define MGBE_MTL_RXP_INTR_CS_FOOVIS   OSI_BIT(2)
#define MGBE_MTL_RXP_INTR_CS_NPEOVIS  OSI_BIT(1)
#define MGBE_MTL_RXP_INTR_CS_NVEOVIS  OSI_BIT(0)
/* Indirect Instruction Table defines */
#define MGBE_MTL_FRP_IE0(x)  (((x) * 0x4U) + 0x0U)
#define MGBE_MTL_FRP_IE1(x)  (((x) * 0x4U) + 0x1U)
#define MGBE_MTL_FRP_IE2(x)  (((x) * 0x4U) + 0x2U)
#define MGBE_MTL_FRP_IE3(x)  (((x) * 0x4U) + 0x3U)
#define MGBE_MTL_FRP_IE2_DCH        (OSI_BIT(31) | OSI_BIT(30) |             \
                                                 OSI_BIT(29) | OSI_BIT(28) | \
                                                 OSI_BIT(27) | OSI_BIT(26) | \
                                                 OSI_BIT(25) | OSI_BIT(24))
#define MGBE_MTL_FRP_IE2_DCH_SHIFT  24U
#define MGBE_MTL_FRP_IE2_OKI        (OSI_BIT(23) | OSI_BIT(22) |             \
                                                 OSI_BIT(21) | OSI_BIT(20) | \
                                                 OSI_BIT(19) | OSI_BIT(18) | \
                                                 OSI_BIT(17) | OSI_BIT(16))
#define MGBE_MTL_FRP_IE2_OKI_SHIFT  16U
#define MGBE_MTL_FRP_IE2_FO         (OSI_BIT(13) | OSI_BIT(12) |             \
                                                 OSI_BIT(11) | OSI_BIT(10) | \
                                                 OSI_BIT(9) | OSI_BIT(8))
#define MGBE_MTL_FRP_IE2_FO_SHIFT   8U
#define MGBE_MTL_FRP_IE2_DCHT       OSI_BIT(7)
#define MGBE_MTL_FRP_IE2_NC         OSI_BIT(3)
#define MGBE_MTL_FRP_IE2_IM         OSI_BIT(2)
#define MGBE_MTL_FRP_IE2_RF         OSI_BIT(1)
#define MGBE_MTL_FRP_IE2_AF         OSI_BIT(0)
#define MGBE_MTL_FRP_IE3_DCH_MASK   0xFFFFU
/* Indirect register defines */
#define MGBE_MTL_RXP_IND_CS_BUSY     OSI_BIT(31)
#define MGBE_MTL_RXP_IND_RCH_ACCSEL  OSI_BIT(25)
#define MGBE_MTL_RXP_IND_CS_ACCSEL   OSI_BIT(24)
#define MGBE_MTL_RXP_IND_CS_CRWEN    OSI_BIT(18)
#define MGBE_MTL_RXP_IND_CS_CRWSEL   OSI_BIT(17)
#define MGBE_MTL_RXP_IND_CS_WRRDN    OSI_BIT(16)
#define MGBE_MTL_RXP_IND_CS_ADDR     (OSI_BIT(9) | OSI_BIT(8) |            \
                                                 OSI_BIT(7) | OSI_BIT(6) | \
                                                 OSI_BIT(5) | OSI_BIT(4) | \
                                                 OSI_BIT(3) | OSI_BIT(2) | \
                                                 OSI_BIT(1) | OSI_BIT(0))
/** @} */

/**
 * @addtogroup MGBE MTL queue ETS algorithm mode
 *
 * @brief MTL queue algorithm type
 * @{
 */
#define OSI_MGBE_TXQ_AVALG_ETS     2U
#define MGBE_MTL_TCQ_ETS_CR_AVALG  (OSI_BIT(1) | OSI_BIT(0))
/** @} */

/**
 * @addtogroup MGBE-MAC MAC register offsets
 *
 * @brief MGBE MAC register offsets
 * @{
 */
#define MGBE_MAC_TMCR           0x0000
#define MGBE_MAC_RMCR           0x0004
#define MGBE_MAC_VLAN_TR        0x0050
#define MGBE_MAC_VLANTIR        0x0060
#define MGBE_MAC_RQC0R          0x00A0
#define MGBE_MAC_RQC1R          0x00A4
#define MGBE_MAC_ISR            0x00B0
#define MGBE_MAC_IER            0x00B4
#define MGBE_MAC_EXT_CNF        0x0140
#define MGBE_MDIO_SCCD          0x0204
#define MGBE_MDIO_SCCA          0x0200
#define MGBE_MAC_MDIO_INTR_STS  0x0214
#define MGBE_MAC_ADDRH(x)  ((0x0008U * (x)) + 0x0300U)
#define MGBE_MAC_ADDRL(x)  ((0x0008U * (x)) + 0x0304U)
#define MGBE_MAC_INDIR_AC         0x0700
#define MGBE_MAC_INDIR_DATA       0x0704
#define MGBE_MAC_PCTH_INTR_STS    0x070C
#define MGBE_MAC_PCTW_INTR_STS    0x0734
#define MGBE_MMC_TX_INTR_EN       0x0810
#define MGBE_MMC_RX_INTR_EN       0x080C
#define MGBE_MMC_CNTRL            0x0800
#define MGBE_MMC_IPC_RX_INT_MASK  0x0A5C
#define MGBE_MAC_L3L4_ADDR_CTR    0x0C00
#define MGBE_MAC_L3L4_DATA        0x0C04
#define MGBE_MAC_TCR              0x0D00
#define MGBE_MAC_SSIR             0x0D04
#define MGBE_MAC_STSUR            0x0D10
#define MGBE_MAC_STNSUR           0x0D14
#define MGBE_MAC_TAR              0x0D18
#define MGBE_MAC_TSS              0x0D20
#define MGBE_MAC_TSNSSEC          0x0D30
#define MGBE_MAC_TSSEC            0x0D34
#define MGBE_MAC_TSPKID           0x0D38
#define MGBE_MAC_PPS_CTL          0x0D70
#define MGBE_MAC_PPS_TT_SEC       0x0D80
#define MGBE_MAC_PPS_TT_NSEC      0x0D84
#define MGBE_MAC_PPS_INTERVAL     0x0D88
#define MGBE_MAC_PPS_WIDTH        0x0D8C
/** @} */

/**
 * @addtogroup MGBE-WRAPPER MGBE Wrapper register offsets
 *
 * @brief MGBE Wrapper register offsets
 * @{
 */
#define MGBE_WRAP_COMMON_INTR_ENABLE       0x8704
#define MGBE_T26X_WRAP_COMMON_INTR_ENABLE  0x880C

#ifdef HSI_SUPPORT
#define MGBE_REGISTER_PARITY_ERR     OSI_BIT(5)
#define MGBE_CORE_CORRECTABLE_ERR    OSI_BIT(4)
#define MGBE_CORE_UNCORRECTABLE_ERR  OSI_BIT(3)

#define MGBE_MTL_DEBUG_CONTROL           0x1008U
#define MGBE_MTL_DEBUG_CONTROL_FDBGEN    OSI_BIT(0)
#define MGBE_MTL_DEBUG_CONTROL_DBGMOD    OSI_BIT(1)
#define MGBE_MTL_DEBUG_CONTROL_FIFORDEN  OSI_BIT(10)
#define MGBE_MTL_DEBUG_CONTROL_EIEE      OSI_BIT(16)
#define MGBE_MTL_DEBUG_CONTROL_EIEC      OSI_BIT(18)

#endif
#define MGBE_MAC_SBD_INTR                  OSI_BIT(2)
#define MGBE_WRAP_COMMON_INTR_STATUS       0x8708
#define MGBE_T26X_WRAP_COMMON_INTR_STATUS  0x8810
#define MGBE_VIRT_INTR_APB_CHX_CNTRL(x)  (0x8200U + ((x) * 4U))
#define MGBE_VIRTUAL_APB_ERR_CTRL  0x8300
/** @} */

/**
 * @addtogroup MGBE-MAC-MODE MAC Mode Select Group
 *
 * @brief MGBE MAC Indirect Access control and status for
 * Mode Select type defines.
 * @{
 */
#define MGBE_MAC_XDCS_DMA_MAX       0x3FFU
#define MGBE_MAC_XDCS_DMA_MAX_T26X  0xFFFFFFFFFFFFU
#define MGBE_MAC_XDCST_DMA_MAX      OSI_BIT(16)
#define MGBE_MAC_INDIR_AC_OB_WAIT   10U
#define MGBE_MAC_INDIR_AC_OB_RETRY  10U

#define MGBE_MAC_INDIR_AC_MSEL_T26X  (OSI_BIT(26) | OSI_BIT(27) |    \
                                         OSI_BIT(28) | OSI_BIT(29))
#define MGBE_MAC_DCHSEL              0U
#define MGBE_MAC_DPCSEL              0x3U
#define MGBE_MAC_DPCSEL_DDS          OSI_BIT(1)

/* MGBE_MAC_INDIR_AC register defines */
#define MGBE_MAC_INDIR_AC_MSEL             (OSI_BIT(19) | OSI_BIT(18) |\
                                         OSI_BIT(17) | OSI_BIT(16))
#define MGBE_MAC_INDIR_AC_MSEL_SHIFT       16U
#define MGBE_MAC_INDIR_AC_MSEL_SHIFT_T264  26U
#define MGBE_MAC_INDIR_AC_AOFF             (OSI_BIT(15) | OSI_BIT(14) |\
                                         OSI_BIT(13) | OSI_BIT(12) | \
                                         OSI_BIT(11) | OSI_BIT(10) | \
                                         OSI_BIT(9) | OSI_BIT(8))
#define MGBE_MAC_INDIR_AC_AOFF_SHIFT       8U
#define MGBE_MAC_INDIR_AC_CMD              OSI_BIT(1)
#define MGBE_MAC_INDIR_AC_OB               OSI_BIT(0)
/** @} */

/**
 * @addtogroup MGBE-L3L4 MAC L3L4 defines
 *
 * @brief MGBE L3L4 Address Control register
 * IDDR filter filed type defines
 * @{
 */
#define MGBE_MAC_XB_WAIT   10U
#define MGBE_MAC_L3L4_CTR  0x0
#define MGBE_MAC_L3_AD1R   0x5
#ifndef OSI_STRIPPED_LIB
#define MGBE_MAC_L3_AD0R           0x4
#define MGBE_MAC_L3_AD2R           0x6
#define MGBE_MAC_L3_AD3R           0x7
#define MGBE_MAC_L4_ADDR           0x1
#define MGBE_MAC_L4_ADDR_SP_MASK   0x0000FFFFU
#define MGBE_MAC_L4_ADDR_DP_MASK   0xFFFF0000U
#define MGBE_MAC_L4_ADDR_DP_SHIFT  16
#endif /* !OSI_STRIPPED_LIB */
/** @} */

#define MGBE_DMA_CHX_TX_CNTRL2_ORRQ_RECOMMENDED  64U
#define MGBE_DMA_CHX_TX_CNTRL2_ORRQ_SHIFT        24U
#define MGBE_DMA_CHX_RX_CNTRL2_OWRQ_SCHAN        32U
#define MGBE_DMA_CHX_RX_CNTRL2_OWRQ_MCHAN        64U
#define MGBE_DMA_CHX_RX_CNTRL2_OWRQ_SHIFT        24U
#define MGBE_DMA_CHX_CTRL_PBL_SHIFT              16U

/**
 * @addtogroup MGBE-DMA DMA register offsets
 *
 * @brief MGBE DMA register offsets
 * @{
 */
#define MGBE_DMA_MODE          0x3000
#define MGBE_DMA_SBUS          0x3004
#define MGBE_DMA_ISR_CH0_15    0x3008
#define MGBE_DMA_TX_EDMA_CTRL  0x3040
#define MGBE_DMA_RX_EDMA_CTRL  0x3044
#define MGBE_DMA_INDIR_CTRL    0x3080
#define MGBE_DMA_INDIR_DATA    0x3084
#define MGBE_DMA_ISR_CH16_47   0x3090
#define MGBE_DMA_CHX_STATUS(x)  ((0x0080U * (x)) + 0x3160U)
#define MGBE_DMA_CHX_IER(x)     ((0x0080U * (x)) + 0x3138U)
/** @} */

/**
 * @addtogroup MGBE-MTL MTL register offsets
 *
 * @brief MGBE MTL register offsets
 * @{
 */
#define MGBE_MTL_RXQ_DMA_MAP0  0x1030
#define MGBE_MTL_RXQ_DMA_MAP1  0x1034
#define MGBE_MTL_RXQ_DMA_MAP2  0x1038
#define MGBE_MTL_CHX_TX_OP_MODE(x)  ((0x0080U * (x)) + 0x1100U)
#define MGBE_MTL_TCQ_ETS_CR(x)      ((0x0080U * (x)) + 0x1110U)
#define MGBE_MTL_TCQ_QW(x)          ((0x0080U * (x)) + 0x1118U)
#define MGBE_MTL_CHX_RX_OP_MODE(x)  ((0x0080U * (x)) + 0x1140U)
#define MGBE_MTL_RXQ_FLOW_CTRL(x)   ((0x0080U * (x)) + 0x1150U)
/** @} */

/**
 * @addtogroup HW Register BIT values
 *
 * @brief consists of corresponding MGBE MAC, MTL register bit values
 * @{
 */
#define MGBE_MTL_CHX_TX_OP_MODE_Q2TC_SH  8U
#define MGBE_MTL_TSF                     OSI_BIT(1)
#define MGBE_MTL_TXQEN                   OSI_BIT(3)
#define MGBE_MTL_RSF                     OSI_BIT(5)
#define MGBE_MTL_TCQ_QW_ISCQW            OSI_BIT(4)
#define MGBE_MAC_RMCR_ACS                OSI_BIT(1)
#define MGBE_MAC_RMCR_CST                OSI_BIT(2)
#define MGBE_MAC_RMCR_IPC                OSI_BIT(9)
#define MGBE_MAC_RXQC0_RXQEN_MASK        0x3U
#define MGBE_MAC_RXQC0_RXQEN_SHIFT(x)  ((x) * 2U)
#define MGBE_MDIO_SCCD_SBUSY                     OSI_BIT(22)
#define MGBE_MDIO_SCCA_DA_SHIFT                  21U
#define MGBE_MDIO_SCCA_DA_MASK                   0x1FU
#define MGBE_MDIO_C45_DA_SHIFT                   16U
#define MGBE_MDIO_SCCA_PA_SHIFT                  16U
#define MGBE_MDIO_SCCA_RA_MASK                   0xFFFFU
#define MGBE_MDIO_SCCD_CMD_WR                    1U
#define MGBE_MDIO_SCCD_CMD_RD                    3U
#define MGBE_MDIO_SCCD_CMD_SHIFT                 16U
#define MGBE_MDIO_SCCD_CR_SHIFT                  19U
#define MGBE_MDIO_SCCD_CR_MASK                   0x7U
#define MGBE_MDIO_SCCD_SDATA_MASK                0xFFFFU
#define MGBE_MDIO_SCCD_CRS                       OSI_BIT(31)
#define MGBE_MAC_RMCR_GPSLCE                     OSI_BIT(6)
#define MGBE_MAC_RMCR_WD                         OSI_BIT(7)
#define MGBE_MAC_RMCR_JE                         OSI_BIT(8)
#define MGBE_MAC_TMCR_DDIC                       OSI_BIT(1)
#define MGBE_MAC_TMCR_JD                         OSI_BIT(16)
#define MGBE_MMC_CNTRL_CNTRST                    OSI_BIT(0)
#define MGBE_MMC_CNTRL_RSTONRD                   OSI_BIT(2)
#define MGBE_MMC_CNTRL_CNTMCT                    (OSI_BIT(4) | OSI_BIT(5))
#define MGBE_MMC_CNTRL_CNTPRST                   OSI_BIT(7)
#define MGBE_MMC_CNTRL_DRCHM                     OSI_BIT(31)
#define MGBE_MMC_IPC_RX_INT_MASK_VALUE           0x3FFF3FFFU
#define MGBE_MAC_RQC1R_MCBCQEN                   OSI_BIT(15)
#define MGBE_MAC_RQC1R_MCBCQ                     (OSI_BIT(11) | OSI_BIT(10) |\
                                                 OSI_BIT(9) | OSI_BIT(8))
#define MGBE_MAC_RQC1R_MCBCQ_SHIFT               8U
#define MGBE_IMR_RGSMIIIE                        OSI_BIT(0)
#define MGBE_IMR_TSIE                            OSI_BIT(12)
#define MGBE_ISR_TSIS                            OSI_BIT(12)
#define MGBE_DMA_ISR_MACIS                       OSI_BIT(17)
#define MGBE_DMA_ISR_DCH0_DCH15_MASK             0x3FFU
#define MGBE_DMA_ISR_DCH16_DCH47_MASK            0xFFFFU
#define MGBE_DMA_CHX_STATUS_TI                   OSI_BIT(0)
#define MGBE_DMA_CHX_STATUS_RI                   OSI_BIT(6)
#define MGBE_MAC_ADDRH_AE                        OSI_BIT(31)
#define MGBE_MAC_ADDRH_SA                        OSI_BIT(30)
#define MGBE_MAC_ADDRH_SA_SHIFT                  30
#define MGBE_MAB_ADDRH_MBC_MAX_MASK              0x3FU
#define MGBE_MAC_ADDRH_MBC                       (OSI_BIT(29) | OSI_BIT(28) |\
                                                 OSI_BIT(27) | OSI_BIT(26) | \
                                                 OSI_BIT(25) | OSI_BIT(24))
#define MGBE_MAC_ADDRH_MBC_SHIFT                 24
#define MGBE_MAC_ADDRH_DCS                       (OSI_BIT(23) | OSI_BIT(22) |\
                                                 OSI_BIT(21) | OSI_BIT(20) | \
                                                 OSI_BIT(19) | OSI_BIT(18) | \
                                                 OSI_BIT(17) | OSI_BIT(16))
#define MGBE_MAC_ADDRH_DCS_SHIFT                 16
#define MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM_T264    (OSI_BIT(12) | OSI_BIT(13) |\
                                                 OSI_BIT(14) | OSI_BIT(15) | \
                                                 OSI_BIT(16) | OSI_BIT(17))
#define MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM         (OSI_BIT(12) | OSI_BIT(13) |\
                                                 OSI_BIT(14) | OSI_BIT(15))
#define MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM_SHIFT   12
#define MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE        (OSI_BIT(8) | OSI_BIT(9) |\
                                                 OSI_BIT(10) | OSI_BIT(11))
#define MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE_SHIFT  8
#define MGBE_MAC_L3L4_ADDR_CTR_TT                OSI_BIT(1)
#define MGBE_MAC_L3L4_ADDR_CTR_XB                OSI_BIT(0)
#define MGBE_MAC_VLANTR_EVLS_ALWAYS_STRIP        ((nveu32_t)0x3 << 21U)
#define MGBE_MAC_VLANTR_EVLRXS                   OSI_BIT(24)
#define MGBE_MAC_VLANTR_DOVLTC                   OSI_BIT(20)
#define MGBE_MAC_VLANTIR_VLTI                    OSI_BIT(20)
#define MGBE_MAC_VLANTIRR_CSVL                   OSI_BIT(19)
#define MGBE_MAC_ISR_LSI                         OSI_BIT(0)
#define MGBE_MAC_ISR_LS_MASK                     (OSI_BIT(25) | OSI_BIT(24))
#define MGBE_MAC_ISR_LS_LOCAL_FAULT              OSI_BIT(25)
#define MGBE_MAC_ISR_LS_LINK_OK                  0U
/* DMA SBUS */
#define MGBE_DMA_SBUS_UNDEF             OSI_BIT(0)
#define MGBE_DMA_SBUS_BLEN256           OSI_BIT(7)
#define MGBE_DMA_SBUS_EAME              OSI_BIT(11)
#define MGBE_DMA_SBUS_RD_OSR_LMT        0x003F0000U
#define MGBE_DMA_SBUS_WR_OSR_LMT        0x3F000000U
#define MGBE_DMA_TX_EDMA_CTRL_TDPS      0x00000005U
#define MGBE_DMA_RX_EDMA_CTRL_RDPS      0x00000005U
#define MGBE_MAC_TMCR_SS_2_5G           (OSI_BIT(31) | OSI_BIT(30))
#define MGBE_MAC_TMCR_SS_5G             (OSI_BIT(31) | OSI_BIT(29))
#define MGBE_MAC_TMCR_SS_10G            (OSI_BIT(31) | OSI_BIT(30) | OSI_BIT(29))
#define MGBE_MAC_TMCR_SS_SPEED_25G      OSI_BIT(29)
#define MGBE_MAC_TMCR_TE                OSI_BIT(0)
#define MGBE_MAC_RMCR_RE                OSI_BIT(0)
#define MGBE_MTL_TXQ_SIZE_SHIFT         16U
#define MGBE_MTL_RXQ_SIZE_SHIFT         16U
#define MGBE_MTL_Q_SIZE_MASK            (OSI_BIT(21) | OSI_BIT(20) | OSI_BIT(19) |         \
                                                OSI_BIT(18) | OSI_BIT(17) | OSI_BIT(16))
#define MGBE_RXQ_TO_DMA_CHAN_MAP0       0x03020100U
#define MGBE_RXQ_TO_DMA_CHAN_MAP1       0x07060504U
#define MGBE_RXQ_TO_DMA_CHAN_MAP2       0x0B0A0908U
#define MGBE_RXQ_TO_DMA_MAP_DDMACH      0x80808080U
#define MGBE_MAC_RMCR_GPSL_MSK          0x3FFF0000U
#define MGBE_MAC_TCR_TSUPDT             OSI_BIT(3)
#define MGBE_MAC_STNSUR_ADDSUB_SHIFT    31U
#define MGBE_MTL_RXQ_OP_MODE_EHFC       OSI_BIT(7)
#define MGBE_MTL_RXQ_OP_MODE_RFA_SHIFT  1U
#define MGBE_MTL_RXQ_OP_MODE_RFA_MASK   0x0000007EU
#define MGBE_MTL_RXQ_OP_MODE_RFD_SHIFT  17U
#define MGBE_MTL_RXQ_OP_MODE_RFD_MASK   0x007E0000U
#define MGBE_DMA_MODE_DSCB              OSI_BIT(16)
#if defined (MACSEC_SUPPORT)

/**
 * MACSEC Recommended value
 * By default PCS and UPHY are present
 */
#define MGBE_MTL_EST_CTOV_MACSEC_RECOMMEND  295U
#endif /*  MACSEC_SUPPORT */
#define MGBE_MTL_EST_CONTROL_LCSE        (OSI_BIT(7) | OSI_BIT(6))
#define MGBE_MTL_EST_CONTROL_LCSE_VAL    0U
#define MGBE_MTL_EST_CONTROL_DDBF        OSI_BIT(4)
#define MGBE_MTL_EST_OVERHEAD_OVHD       (OSI_BIT(0) | OSI_BIT(1) |        \
                                                 OSI_BIT(2) | OSI_BIT(3) | \
                                                 OSI_BIT(4) | OSI_BIT(5))
#define MGBE_MTL_EST_OVERHEAD_RECOMMEND  56U
/* EST GCL controlOSI_BITmap */
#define MGBE_MTL_EST_ADDR_SHIFT  8
/* EST GCRA addresses */
#define MGBE_MTL_EST_BTR_LOW   ((nveu32_t)0x0 <<                  \
                                                 MGBE_MTL_EST_ADDR_SHIFT)
#define MGBE_MTL_EST_BTR_HIGH  ((nveu32_t)0x1 <<                  \
                                                 MGBE_MTL_EST_ADDR_SHIFT)
#define MGBE_MTL_EST_CTR_LOW   ((nveu32_t)0x2 <<                  \
                                                 MGBE_MTL_EST_ADDR_SHIFT)
#define MGBE_MTL_EST_CTR_HIGH  ((nveu32_t)0x3 <<                  \
                                                 MGBE_MTL_EST_ADDR_SHIFT)
#define MGBE_MTL_EST_TER       ((nveu32_t)0x4 <<                  \
                                                 MGBE_MTL_EST_ADDR_SHIFT)
#define MGBE_MTL_EST_LLR       ((nveu32_t)0x5 <<                  \
                                                 MGBE_MTL_EST_ADDR_SHIFT)
/*EST MTL interrupt STATUS and ERR*/
#define MGBE_MTL_IS_ESTIS  OSI_BIT(18)

#define MGBE_MAC_EXT_CNF_DDS  OSI_BIT(7)
/* TX timestamp */
#define MGBE_MAC_TSS_TXTSC  OSI_BIT(15)
/* MGBE DMA IND CTRL register field masks */
#define MGBE_DMA_INDIR_CTRL_MSEL_MASK   (OSI_BIT(24) | OSI_BIT(25) |         \
                                                OSI_BIT(26) | OSI_BIT(27) | \
                                                OSI_BIT(28))
#define MGBE_DMA_INDIR_CTRL_MSEL_SHIFT  24
#define MGBE_DMA_INDIR_CTRL_AOFF_MASK   (OSI_BIT(8) | OSI_BIT(9) |         \
                                                OSI_BIT(10) | OSI_BIT(11) | \
                                                OSI_BIT(12) | OSI_BIT(13) | \
                                                OSI_BIT(14))
#define MGBE_DMA_INDIR_CTRL_AOFF_SHIFT  8
#define MGBE_DMA_INDIR_CTRL_CT          OSI_BIT(1)
#define MGBE_DMA_INDIR_CTRL_OB          OSI_BIT(0)
/* MGBE PDMA_CH(#i)_Tx/RxExtCfg register field masks */
#define MGBE_PDMA_CHX_TX_EXTCFG                 0U
#define MGBE_PDMA_CHX_RX_EXTCFG                 1U
#define MGBE_PDMA_CHX_TXRX_EXTCFG_ORRQ_SHIFT    8
#define MGBE_PDMA_CHX_TXRX_EXTCFG_P2TCMP_SHIFT  16
#define MGBE_PDMA_CHX_TXRX_EXTCFG_P2TCMP_MASK   (OSI_BIT(16) | \
                                                OSI_BIT(17) | OSI_BIT(18))
#define MGBE_PDMA_CHX_TXRX_EXTCFG_PBLX8         OSI_BIT(19)
#define MGBE_PDMA_CHX_EXTCFG_PBL_SHIFT          24U

#define MGBE_PDMA_CHX_RX_EXTCFG_RXPEN  OSI_BIT(31)

/* MGBE PDMA_CH(#i)_Tx/RxDescCtrl register field masks */
#define MGBE_VDMA_CHX_TX_DESC_CTRL               4U
#define MGBE_VDMA_CHX_RX_DESC_CTRL               5U
#define MGBE_VDMA_CHX_TXRX_DESC_CTRL_DCSZ        5U
#define MGBE_VDMA_CHX_TXRX_DESC_CTRL_DCSZ_UFPGA  3U
#define MGBE_VDMA_CHX_TXRX_DESC_CTRL_DCSZ_MASK   (OSI_BIT(0) | OSI_BIT(1) |\
                                                OSI_BIT(2))
#define MGBE_VDMA_CHX_TXRX_DESC_CTRL_DPS         3U
#define MGBE_VDMA_CHX_TXRX_DESC_CTRL_DPS_MASK    (OSI_BIT(3)  | OSI_BIT(4) |\
                                                OSI_BIT(5))
#define MGBE_VDMA_CHX_TXRX_DESC_CTRL_DPS_SHIFT   3
/** @} */

#ifndef OSI_STRIPPED_LIB

/**
 * @addtogroup MGBE-HW-BACKUP
 *
 * @brief Definitions related to taking backup of MGBE core registers.
 * @{
 */

/* Hardware Register offsets to be backed up during suspend.
 *
 * Do not change the order of these macros. To add new registers to be
 * backed up, append to end of list before MGBE_MAX_MAC_BAK_IDX, and
 * update MGBE_MAX_MAC_BAK_IDX based on new macro.
 */
#define MGBE_MAC_TMCR_BAK_IDX            0U
#define MGBE_MAC_RMCR_BAK_IDX            ((MGBE_MAC_TMCR_BAK_IDX + 1U))
#define MGBE_MAC_PFR_BAK_IDX             ((MGBE_MAC_RMCR_BAK_IDX + 1U))
#define MGBE_MAC_VLAN_TAG_BAK_IDX        ((MGBE_MAC_PFR_BAK_IDX + 1U))
#define MGBE_MAC_VLANTIR_BAK_IDX         ((MGBE_MAC_VLAN_TAG_BAK_IDX + 1U))
#define MGBE_MAC_RX_FLW_CTRL_BAK_IDX     ((MGBE_MAC_VLANTIR_BAK_IDX + 1U))
#define MGBE_MAC_RQC0R_BAK_IDX           ((MGBE_MAC_RX_FLW_CTRL_BAK_IDX + 1U))
#define MGBE_MAC_RQC1R_BAK_IDX           ((MGBE_MAC_RQC0R_BAK_IDX + 1U))
#define MGBE_MAC_RQC2R_BAK_IDX           ((MGBE_MAC_RQC1R_BAK_IDX + 1U))
#define MGBE_MAC_ISR_BAK_IDX             ((MGBE_MAC_RQC2R_BAK_IDX + 1U))
#define MGBE_MAC_IER_BAK_IDX             ((MGBE_MAC_ISR_BAK_IDX + 1U))
#define MGBE_MAC_PMTCSR_BAK_IDX          ((MGBE_MAC_IER_BAK_IDX + 1U))
#define MGBE_MAC_LPI_CSR_BAK_IDX         ((MGBE_MAC_PMTCSR_BAK_IDX + 1U))
#define MGBE_MAC_LPI_TIMER_CTRL_BAK_IDX  ((MGBE_MAC_LPI_CSR_BAK_IDX + 1U))
#define MGBE_MAC_LPI_EN_TIMER_BAK_IDX    ((MGBE_MAC_LPI_TIMER_CTRL_BAK_IDX + 1U))
#define MGBE_MAC_EXT_CNF_BAK_IDX         ((MGBE_MAC_LPI_EN_TIMER_BAK_IDX + 1U))
#define MGBE_MAC_TCR_BAK_IDX             ((MGBE_MAC_EXT_CNF_BAK_IDX + 1U))
#define MGBE_MAC_SSIR_BAK_IDX            ((MGBE_MAC_TCR_BAK_IDX + 1U))
#define MGBE_MAC_STSR_BAK_IDX            ((MGBE_MAC_SSIR_BAK_IDX + 1U))
#define MGBE_MAC_STNSR_BAK_IDX           ((MGBE_MAC_STSR_BAK_IDX + 1U))
#define MGBE_MAC_STSUR_BAK_IDX           ((MGBE_MAC_STNSR_BAK_IDX + 1U))
#define MGBE_MAC_STNSUR_BAK_IDX          ((MGBE_MAC_STSUR_BAK_IDX + 1U))
#define MGBE_MAC_TAR_BAK_IDX             ((MGBE_MAC_STNSUR_BAK_IDX + 1U))
#define MGBE_DMA_BMR_BAK_IDX             ((MGBE_MAC_TAR_BAK_IDX + 1U))
#define MGBE_DMA_SBUS_BAK_IDX            ((MGBE_DMA_BMR_BAK_IDX + 1U))
#define MGBE_DMA_ISR_BAK_IDX             ((MGBE_DMA_SBUS_BAK_IDX + 1U))
#define MGBE_MTL_OP_MODE_BAK_IDX         ((MGBE_DMA_ISR_BAK_IDX + 1U))
#define MGBE_MTL_RXQ_DMA_MAP0_BAK_IDX    ((MGBE_MTL_OP_MODE_BAK_IDX + 1U))
/* x varies from 0-3, 4 HTR registers total */
#define MGBE_MAC_HTR_REG_BAK_IDX(x)  ((MGBE_MTL_RXQ_DMA_MAP0_BAK_IDX + 1U +    \
                                        (x)))
/* x varies from 0-9, 10 queues total */
#define MGBE_MAC_QX_TX_FLW_CTRL_BAK_IDX(x)  ((MGBE_MAC_HTR_REG_BAK_IDX(0U)     \
                                                + MGBE_MAX_HTR_REGS + (x)))
/* x varies from 0-31, 32 L2 DA/SA filters total */
#define MGBE_MAC_ADDRH_BAK_IDX(x)  ((MGBE_MAC_QX_TX_FLW_CTRL_BAK_IDX(0U)      \
                                        + OSI_MGBE_MAX_NUM_QUEUES + (x)))
#define MGBE_MAC_ADDRL_BAK_IDX(x)  ((MGBE_MAC_ADDRH_BAK_IDX(0U) +      \
                                        OSI_MGBE_MAX_MAC_ADDRESS_FILTER + (x)))

/* MTL HW Register offsets
 *
 * Do not change the order of these macros. To add new registers to be
 * backed up, append to end of list before MGBE_MAX_MTL_BAK_IDX, and
 * update MGBE_MAX_MTL_BAK_IDX based on new macro.
 */
/* x varies from 0-9, 10 queues total */
#define MGBE_MTL_CHX_TX_OP_MODE_BAK_IDX(x)  ((MGBE_MAC_ADDRL_BAK_IDX(0U) +\
                                           OSI_MGBE_MAX_MAC_ADDRESS_FILTER + \
                                           (x)))
#define MGBE_MTL_TXQ_ETS_CR_BAK_IDX(x)      ((MGBE_MTL_CHX_TX_OP_MODE_BAK_IDX(0U)\
                                        + OSI_MGBE_MAX_NUM_QUEUES + (x)))
#define MGBE_MTL_TXQ_QW_BAK_IDX(x)          ((MGBE_MTL_TXQ_ETS_CR_BAK_IDX(0U) +\
                                        OSI_MGBE_MAX_NUM_QUEUES + (x)))
#define MGBE_MTL_TXQ_ETS_SSCR_BAK_IDX(x)    ((MGBE_MTL_TXQ_QW_BAK_IDX(0U)     \
                                                + OSI_MGBE_MAX_NUM_QUEUES + \
                                                (x)))
#define MGBE_MTL_TXQ_ETS_HCR_BAK_IDX(x)     ((MGBE_MTL_TXQ_ETS_SSCR_BAK_IDX(0U) +\
                                        OSI_MGBE_MAX_NUM_QUEUES + (x)))
#define MGBE_MTL_TXQ_ETS_LCR_BAK_IDX(x)     ((MGBE_MTL_TXQ_ETS_HCR_BAK_IDX(0U) +\
                                        OSI_MGBE_MAX_NUM_QUEUES + (x)))
#define MGBE_MTL_CHX_RX_OP_MODE_BAK_IDX(x)      \
                                        ((MGBE_MTL_TXQ_ETS_LCR_BAK_IDX(0U) + \
                                        OSI_MGBE_MAX_NUM_QUEUES + (x)))

/* MGBE Wrapper register offsets to be saved during suspend
 *
 * Do not change the order of these macros. To add new registers to be
 * backed up, append to end of list before MGBE_MAX_WRAPPER_BAK_IDX,
 * and update MGBE_MAX_WRAPPER_BAK_IDX based on new macro.
 */
#define MGBE_CLOCK_CTRL_0_BAK_IDX      ((MGBE_MTL_CHX_RX_OP_MODE_BAK_IDX(0U)  \
                                        + OSI_MGBE_MAX_NUM_QUEUES))
#define MGBE_AXI_ASID_CTRL_BAK_IDX     ((MGBE_CLOCK_CTRL_0_BAK_IDX + 1U))
#define MGBE_PAD_CRTL_BAK_IDX          ((MGBE_AXI_ASID_CTRL_BAK_IDX + 1U))
#define MGBE_PAD_AUTO_CAL_CFG_BAK_IDX  ((MGBE_PAD_CRTL_BAK_IDX + 1U))
/* MGBE_PAD_AUTO_CAL_STAT is Read-only. Skip backup/restore */

/* To add new direct access registers to backup during suspend,
 * and restore during resume add it before this line, and increment
 * MGBE_DIRECT_MAX_BAK_IDX accordingly.
 */
#define MGBE_DIRECT_MAX_BAK_IDX  ((MGBE_PAD_AUTO_CAL_CFG_BAK_IDX + 1U))

/**
 * Start indirect addressing registers
 **/
/* x varies from 0-7, 8 L3/L4 filters total */
#define MGBE_MAC_L3L4_CTR_BAK_IDX(x)  (MGBE_DIRECT_MAX_BAK_IDX + (x))
#define MGBE_MAC_L4_ADR_BAK_IDX(x)    ((MGBE_MAC_L3L4_CTR_BAK_IDX(0U) +   \
                                        OSI_MGBE_MAX_L3_L4_FILTER + (x)))
#define MGBE_MAC_L3_AD0R_BAK_IDX(x)   ((MGBE_MAC_L4_ADR_BAK_IDX(0U) +   \
                                        OSI_MGBE_MAX_L3_L4_FILTER + (x)))
#define MGBE_MAC_L3_AD1R_BAK_IDX(x)   ((MGBE_MAC_L3_AD0R_BAK_IDX(0U) +   \
                                        OSI_MGBE_MAX_L3_L4_FILTER + (x)))
#define MGBE_MAC_L3_AD2R_BAK_IDX(x)   ((MGBE_MAC_L3_AD1R_BAK_IDX(0U) +   \
                                        OSI_MGBE_MAX_L3_L4_FILTER + (x)))
#define MGBE_MAC_L3_AD3R_BAK_IDX(x)   ((MGBE_MAC_L3_AD2R_BAK_IDX(0U) +   \
                                        OSI_MGBE_MAX_L3_L4_FILTER + (x)))

/* x varies from 0-31, 32 VLAN tag filters total */
#define MGBE_MAC_VLAN_BAK_IDX(x)  ((MGBE_MAC_L3_AD3R_BAK_IDX(0U) +       \
                                        OSI_MGBE_MAX_L3_L4_FILTER + (x)))
/* Add MAC_DChSel_IndReg */
#define MGBE_MAC_DCHSEL_BAK_IDX(x)  ((MGBE_MAC_VLAN_BAK_IDX(0U) +     \
                                         MGBE_MAX_VLAN_FILTER + 1U))

#define MGBE_MAX_BAK_IDX  ((MGBE_MAC_DCHSEL_BAK_IDX(0U) +               \
                                         OSI_MGBE_MAX_MAC_ADDRESS_FILTER + 1U))
/** @} */
#endif /* !OSI_STRIPPED_LIB */

/* TXQ Size 128KB is divided equally across 10 MTL Queues*/
#define TX_FIFO_SZ        (((((MGBE_TXQ_SIZE)/OSI_MGBE_MAX_NUM_QUEUES)) / 256U) - 1U)
#define TX_FIFO_SZ_UFPGA  (((((MGBE_TXQ_SIZE_UFPGA)/OSI_MGBE_MAX_NUM_QUEUES)) / 256U) - 1U)

/**
 * @addtogroup MGBE-MAC-HWFR MGBE MAC HW feature registers
 *
 * @brief Helps in identifying the features that are set in MAC HW
 * @{
 */
#define MGBE_MAC_HFR0  0x11C
#define MGBE_MAC_HFR1  0x120
#define MGBE_MAC_HFR2  0x124
#define MGBE_MAC_HFR3  0x128
/** @} */

/**
 * @addtogroup MGBE-MAC-Feature MGBE MAC HW feature registers bit fields
 *
 * @brief HW feature register bit masks and bit shifts.
 * @{
 */
#define MGBE_MAC_HFR0_RGMIISEL_MASK   0x1U
#define MGBE_MAC_HFR0_RGMIISEL_SHIFT  0U

#define MGBE_MAC_HFR0_GMIISEL_MASK   0x1U
#define MGBE_MAC_HFR0_GMIISEL_SHIFT  1U

#define MGBE_MAC_HFR0_RMIISEL_MASK   0x1U
#define MGBE_MAC_HFR0_RMIISEL_SHIFT  2U

#define MGBE_MAC_HFR0_HDSEL_MASK   0x1U
#define MGBE_MAC_HFR0_HDSEL_SHIFT  3U

#define MGBE_MAC_HFR0_VLHASH_MASK   0x1U
#define MGBE_MAC_HFR0_VLHASH_SHIFT  4U

#define MGBE_MAC_HFR0_SMASEL_MASK   0x1U
#define MGBE_MAC_HFR0_SMASEL_SHIFT  5U

#define MGBE_MAC_HFR0_RWKSEL_MASK   0x1U
#define MGBE_MAC_HFR0_RWKSEL_SHIFT  6U

#define MGBE_MAC_HFR0_MGKSEL_MASK   0x1U
#define MGBE_MAC_HFR0_MGKSEL_SHIFT  7U

#define MGBE_MAC_HFR0_MMCSEL_MASK   0x1U
#define MGBE_MAC_HFR0_MMCSEL_SHIFT  8U

#define MGBE_MAC_HFR0_ARPOFFLDEN_MASK   0x1U
#define MGBE_MAC_HFR0_ARPOFFLDEN_SHIFT  9U

#define MGBE_MAC_HFR0_RAVSEL_MASK   0x1U
#define MGBE_MAC_HFR0_RAVSEL_SHIFT  10U

#define MGBE_MAC_HFR0_AVSEL_MASK   0x1U
#define MGBE_MAC_HFR0_AVSEL_SHIFT  11U

#define MGBE_MAC_HFR0_TSSSEL_MASK   0x1U
#define MGBE_MAC_HFR0_TSSSEL_SHIFT  12U

#define MGBE_MAC_HFR0_EEESEL_MASK   0x1U
#define MGBE_MAC_HFR0_EEESEL_SHIFT  13U

#define MGBE_MAC_HFR0_TXCOESEL_MASK   0x1U
#define MGBE_MAC_HFR0_TXCOESEL_SHIFT  14U

#define MGBE_MAC_HFR0_RXCOESEL_MASK   0x1U
#define MGBE_MAC_HFR0_RXCOESEL_SHIFT  16U

#define MGBE_MAC_HFR0_ADDMACADRSEL_MASK        0x1FU
#define MGBE_T26X_MAC_HFR0_ADDMACADRSEL_MASK   0x3FU
#define MGBE_MAC_HFR0_ADDMACADRSEL_SHIFT       18U
#define MGBE_T26X_MAC_HFR0_ADDMACADRSEL_SHIFT  17U

#define MGBE_MAC_HFR0_PHYSEL_MASK   0x3U
#define MGBE_MAC_HFR0_PHYSEL_SHIFT  23U

#define MGBE_MAC_HFR0_TSSTSSEL_MASK   0x3U
#define MGBE_MAC_HFR0_TSSTSSEL_SHIFT  25U

#define MGBE_MAC_HFR0_SAVLANINS_SHIFT  27U

#define MGBE_MAC_HFR0_VXN_MASK   0x1U
#define MGBE_MAC_HFR0_VXN_SHIFT  29U

#define MGBE_MAC_HFR0_EDIFFC_MASK   0x1U
#define MGBE_MAC_HFR0_EDIFFC_SHIFT  30U

#define MGBE_MAC_HFR0_EDMA_MASK   0x1U
#define MGBE_MAC_HFR0_EDMA_SHIFT  31U

#define MGBE_MAC_HFR1_RXFIFOSIZE_MASK   0x1FU
#define MGBE_MAC_HFR1_RXFIFOSIZE_SHIFT  0U

#define MGBE_MAC_HFR1_PFCEN_MASK   0x1U
#define MGBE_MAC_HFR1_PFCEN_SHIFT  5U

#define MGBE_MAC_HFR1_TXFIFOSIZE_MASK   0x1FU
#define MGBE_MAC_HFR1_TXFIFOSIZE_SHIFT  6U

#define MGBE_MAC_HFR1_OSTEN_MASK   0x1U
#define MGBE_MAC_HFR1_OSTEN_SHIFT  11U

#define MGBE_MAC_HFR1_PTOEN_MASK   0x1U
#define MGBE_MAC_HFR1_PTOEN_SHIFT  12U

#define MGBE_MAC_HFR1_ADVTHWORD_MASK   0x1U
#define MGBE_MAC_HFR1_ADVTHWORD_SHIFT  13U

#define MGBE_MAC_HFR1_ADDR64_MASK   0x3U
#define MGBE_MAC_HFR1_ADDR64_SHIFT  14U

#define MGBE_MAC_HFR1_DCBEN_MASK   0x1U
#define MGBE_MAC_HFR1_DCBEN_SHIFT  16U

#define MGBE_MAC_HFR1_SPHEN_MASK   0x1U
#define MGBE_MAC_HFR1_SPHEN_SHIFT  17U

#define MGBE_MAC_HFR1_TSOEN_MASK   0x1U
#define MGBE_MAC_HFR1_TSOEN_SHIFT  18U

#define MGBE_MAC_HFR1_DBGMEMA_MASK   0x1U
#define MGBE_MAC_HFR1_DBGMEMA_SHIFT  19U

#define MGBE_MAC_HFR1_RSSEN_MASK   0x1U
#define MGBE_MAC_HFR1_RSSEN_SHIFT  20U

#define MGBE_MAC_HFR1_NUMTC_MASK   0x7U
#define MGBE_MAC_HFR1_NUMTC_SHIFT  21U

#define MGBE_MAC_HFR1_HASHTBLSZ_MASK   0x3U
#define MGBE_MAC_HFR1_HASHTBLSZ_SHIFT  24U

#define MGBE_MAC_HFR1_L3L4FNUM_MASK   0xFU
#define MGBE_MAC_HFR1_L3L4FNUM_SHIFT  27U

#define MGBE_MAC_HFR2_RXQCNT_MASK   0xFU
#define MGBE_MAC_HFR2_RXQCNT_SHIFT  0U

#define MGBE_MAC_HFR2_TXQCNT_MASK   0xFU
#define MGBE_MAC_HFR2_TXQCNT_SHIFT  6U

#define MGBE_MAC_HFR2_RXCHCNT_MASK   0xFU
#define MGBE_MAC_HFR2_RXCHCNT_SHIFT  12U

#define MGBE_MAC_HFR2_TXCHCNT_MASK   0xFU
#define MGBE_MAC_HFR2_TXCHCNT_SHIFT  18U

#define MGBE_MAC_HFR2_PPSOUTNUM_MASK   0x7U
#define MGBE_MAC_HFR2_PPSOUTNUM_SHIFT  24U

#define MGBE_MAC_HFR2_AUXSNAPNUM_MASK   0x7U
#define MGBE_MAC_HFR2_AUXSNAPNUM_SHIFT  28U

#define MGBE_MAC_HFR3_NRVF_MASK   0x7U
#define MGBE_MAC_HFR3_NRVF_SHIFT  0U

#define MGBE_MAC_HFR3_FRPSEL_MASK   0x1U
#define MGBE_MAC_HFR3_FRPSEL_SHIFT  3U

#define MGBE_MAC_HFR3_CBTISEL_MASK   0x1U
#define MGBE_MAC_HFR3_CBTISEL_SHIFT  4U

#define MGBE_MAC_HFR3_FRPPIPE_MASK   0x7U
#define MGBE_MAC_HFR3_FRPPIPE_SHIFT  5U

#define MGBE_MAC_HFR3_POUOST_MASK   0x1U
#define MGBE_MAC_HFR3_POUOST_SHIFT  8U

#define MGBE_MAC_HFR3_FRPPB_MASK   0x3U
#define MGBE_MAC_HFR3_FRPPB_SHIFT  9U

#define MGBE_MAC_HFR3_FRPES_MASK   0x3U
#define MGBE_MAC_HFR3_FRPES_SHIFT  11U

#define MGBE_MAC_HFR3_DVLAN_MASK   0x1U
#define MGBE_MAC_HFR3_DVLAN_SHIFT  13U

#define MGBE_MAC_HFR3_ASP_MASK   0x3U
#define MGBE_MAC_HFR3_ASP_SHIFT  14U

#define MGBE_MAC_HFR3_TTSFD_MASK   0x7U
#define MGBE_MAC_HFR3_TTSFD_SHIFT  16U

#define MGBE_MAC_HFR3_ESTSEL_MASK   0x1U
#define MGBE_MAC_HFR3_ESTSEL_SHIFT  19U

#define MGBE_MAC_HFR3_GCLDEP_MASK   0x7U
#define MGBE_MAC_HFR3_GCLDEP_SHIFT  20U

#define MGBE_MAC_HFR3_GCLWID_MASK   0x3U
#define MGBE_MAC_HFR3_GCLWID_SHIFT  23U

#define MGBE_MAC_HFR3_FPESEL_MASK   0x1U
#define MGBE_MAC_HFR3_FPESEL_SHIFT  26U

#define MGBE_MAC_HFR3_TBSSEL_MASK   0x1U
#define MGBE_MAC_HFR3_TBSSEL_SHIFT  27U

#define MGBE_MAC_HFR3_TBS_CH_MASK   0xFU
#define MGBE_MAC_HFR3_TBS_CH_SHIFT  28U

/* FRP defines */
#define MGBE_MAC_FRPPB_64   0U
#define MGBE_MAC_FRPPB_128  1U
#define MGBE_MAC_FRPPB_256  2U
#define MGBE_MAC_FRPES_64   0U
#define MGBE_MAC_FRPES_128  1U
#define MGBE_MAC_FRPES_256  2U

#define MGBE_MAC_FRP_BYTES64   64U
#define MGBE_MAC_FRP_BYTES128  128U
#define MGBE_MAC_FRP_BYTES256  256U
/** @} */

#ifdef HSI_SUPPORT

/**
 * @addtogroup MGBE-HSI
 *
 * @brief HSI feature related registers and bitmap
 * @{
 */
#define MGBE_MTL_ECC_INTERRUPT_ENABLE      0x10C8U
#define MGBE_MTL_TXCEIE                    OSI_BIT(0)
#define MGBE_MTL_RXCEIE                    OSI_BIT(4)
#define MGBE_MTL_GCEIE                     OSI_BIT(8)
#define MGBE_MTL_RPCEIE                    OSI_BIT(12)
#define MGBE_DMA_ECC_INTERRUPT_ENABLE      0x3068U
#define MGBE_DMA_TCEIE                     OSI_BIT(0)
#define MGBE_DMA_DCEIE                     OSI_BIT(1)
#define MGBE_MAC_SCSR_CONTROL              0x164U
#define MGBE_CPEN                          OSI_BIT(0)
#define MGBE_MTL_ECC_INTERRUPT_STATUS      0x10CCU
#define MGBE_DMA_ECC_INTERRUPT_STATUS      0x306CU
#define MGBE_DWCXG_CORE_MAC_FSM_ACT_TIMER  0x15CU
#define MGBE_SNPS_SCS_REG1                 0x160U
#define MGBE_SNPS_SCS_REG1_TRCFSM          OSI_BIT(0)
#define MGBE_SNPS_SCS_REG1_RPERXLPIFSM     OSI_BIT(16)
#define MGBE_CTMR_SHIFT                    28U
#define MGBE_CTMR_MASK                     0x70000000U
#define MGBE_LTMRMD_SHIFT                  20U
#define MGBE_LTMRMD_MASK                   0xF00000U
#define MGBE_NTMRMD_SHIFT                  16U
#define MGBE_NTMRMD_MASK                   0xF0000U
#define MGBE_TMR_SHIFT                     0U
#define MGBE_TMR_MASK                      0x3FFU
#define MGBE_MTL_ECC_CONTROL               0x10C0U
#define MGBE_MTL_ECC_MTXED                 OSI_BIT(0)
#define MGBE_MTL_ECC_MRXED                 OSI_BIT(1)
#define MGBE_MTL_ECC_MGCLED                OSI_BIT(2)
#define MGBE_MTL_ECC_MRXPED                OSI_BIT(3)
#define MGBE_MTL_ECC_TSOED                 OSI_BIT(4)
#define MGBE_MTL_ECC_DESCED                OSI_BIT(5)
#define MGBE_MAC_FSM_CONTROL               0x158U
#define MGBE_PRTYEN                        OSI_BIT(1)
#define MGBE_TMOUTEN                       OSI_BIT(0)
#define MGBE_RXCRCERPIE                    OSI_BIT(5)
#define MGBE_MAC_DPP_FSM_INTERRUPT_STATUS  0x150U
#define MGBE_MTL_DPP_CONTROL               0x10E0U
#define MGBE_DDPP                          OSI_BIT(0)
#define MGBE_MAC_DPP_FSM_INTERRUPT_STATUS  0x150U
/** @} */
#endif

#endif /* INCLUDED_MGBE_CORE_H_ */
