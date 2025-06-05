/*
 * SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INCLUDED_EQOS_CORE_H
#define INCLUDED_EQOS_CORE_H

#ifndef OSI_STRIPPED_LIB
#define EQOS_MAC_PFR             0x0008
#define EQOS_MAC_LPI_CSR         0x00D0
#define EQOS_MAC_LPI_TIMER_CTRL  0x00D4
#define EQOS_MAC_LPI_EN_TIMER    0x00D8
#define EQOS_MAC_RX_FLW_CTRL     0x0090
#define EQOS_MAC_MA0LR           0x0304
#define EQOS_MAC_PIDR0           0x0BC4
#define EQOS_MAC_PTO_CR          0x0BC0
#define EQOS_MAC_PIDR1           0x0BC8
#define EQOS_MAC_PIDR2           0x0BCC
#define EQOS_MAC_PMTCSR          0x00C0
#define EQOS_MAC_QX_TX_FLW_CTRL(x)  ((0x0004U * (x)) + 0x0070U)
#define EQOS_MAC_MA0HR       0x0300
#define EQOS_4_10_MAC_ARPPA  0x0AE0
#define EQOS_5_00_MAC_ARPPA  0x0210
#define EQOS_CLOCK_CTRL_0    0x8000U
#define EQOS_APB_ERR_STATUS  0x8214U

#define EQOS_MAC_PFR_VTFE         OSI_BIT(16)
#define EQOS_MAC_PFR_IPFE         OSI_BIT(20)
#define EQOS_MAC_PFR_IPFE_SHIFT   20U
#define EQOS_MAC_MA0HR_IDX        11U
#define EQOS_5_30_SID             0x3U
#define EQOS_5_30_SID_CH3         ((EQOS_5_30_SID) << EQOS_ASID_CTRL_SHIFT_24)
#define EQOS_5_30_SID_CH2         ((EQOS_5_30_SID) << EQOS_ASID_CTRL_SHIFT_16)
#define EQOS_5_30_SID_CH1         ((EQOS_5_30_SID) << EQOS_ASID_CTRL_SHIFT_8)
#define EQOS_5_30_SID_CH7         ((EQOS_5_30_SID) << EQOS_ASID_CTRL_SHIFT_24)
#define EQOS_5_30_SID_CH6         ((EQOS_5_30_SID) << EQOS_ASID_CTRL_SHIFT_16)
#define EQOS_5_30_SID_CH5         ((EQOS_5_30_SID) << EQOS_ASID_CTRL_SHIFT_8)
#define EQOS_5_30_ASID_CTRL_VAL   ((EQOS_5_30_SID_CH3) |\
                                 (EQOS_5_30_SID_CH2) |\
                                 (EQOS_5_30_SID_CH1) |\
                                 (EQOS_5_30_SID))
#define EQOS_5_30_ASID1_CTRL_VAL  ((EQOS_5_30_SID_CH7) |\
                                 (EQOS_5_30_SID_CH6) |\
                                 (EQOS_5_30_SID_CH5) |\
                                 (EQOS_5_30_SID))
#define EQOS_MAC_MA0HR_MASK       0xFFFFFU
#define EQOS_MAC_IMR_MASK         0x67039U
#define EQOS_MAC_HTR_MASK         0xFFFFFFFFU
#define EQOS_MAC_HTR0_IDX         2U
#define EQOS_MAC_HTR_REG(x)  ((0x0004U * (x)) + 0x0010U)
#define EQOS_DMA_SBUS_MASK           0xDF1F3CFFU
#define EQOS_DMA_CHX_STATUS_FBE      OSI_BIT(10)
#define EQOS_DMA_CHX_STATUS_TBU      OSI_BIT(2)
#define EQOS_DMA_CHX_STATUS_RBU      OSI_BIT(7)
#define EQOS_DMA_CHX_STATUS_RPS      OSI_BIT(8)
#define EQOS_DMA_CHX_STATUS_RWT      OSI_BIT(9)
#define EQOS_DMA_CHX_STATUS_TPS      OSI_BIT(1)
#define EQOS_MAC_RQC0R_MASK          0xFFU
#define EQOS_MAC_QX_TX_FLW_CTRL_TFE  OSI_BIT(1)
#define EQOS_MAC_QX_TXFC_MASK        0xFFFF00F2U
#define EQOS_MAC_Q0_TXFC_IDX         6U
#define EQOS_MAC_PTO_CR_ASYNCEN      OSI_BIT(1)
#define EQOS_MAC_RQC1R_OMCBCQ        OSI_BIT(28)
#define EQOS_MAC_PIDR_PID_MASK       0XFFFFU
#define EQOS_MAC_PFR_MASK            0x803107FFU
#define EQOS_MAC_PAUSE_TIME          0xFFFF0000U
#define EQOS_MAC_PAUSE_TIME_MASK     0xFFFF0000U
#define EQOS_MAC_MCR_MASK            0xFFFFFF7FU
#define EQOS_MAC_MA0LR_IDX           12U
#define EQOS_MAC_MA0LR_MASK          0xFFFFFFFFU
#define EQOS_MAC_PTO_CR_DN           (OSI_BIT(15) | OSI_BIT(14) |            \
                                                 OSI_BIT(13) | OSI_BIT(12) | \
                                                 OSI_BIT(11) | OSI_BIT(10) | \
                                                 OSI_BIT(9) | OSI_BIT(8))
#define EQOS_MAC_PTO_CR_DN_SHIFT     8U
#define EQOS_MAC_PTO_CR_APDREQEN     OSI_BIT(2)
#define EQOS_MAC_PTO_CR_PTOEN        OSI_BIT(0)

#define EQOS_MAC_TCR_TSENMACADDR       OSI_BIT(18)
#define EQOS_MAC_TCR_SNAPTYPSEL_SHIFT  16U
#define EQOS_MAC_TAR_IDX               15U
#define EQOS_MAC_SSIR_IDX              14U
#define EQOS_MAC_RX_FLW_CTRL_RFE       OSI_BIT(0)
#define EQOS_MAC_TCR_MASK              0x1107FF03U
#define EQOS_MAC_TAR_MASK              0xFFFFFFFFU
#define EQOS_MAC_SSIR_MASK             0xFFFF00U
#define EQOS_MAC_RQC2R_MASK            0xFFFFFFFFU
#define EQOS_MAC_RQC1R_TPQC            (OSI_BIT(22) | OSI_BIT(23))
#define EQOS_MAC_RQC1R_TPQC0           OSI_BIT(22)
#define EQOS_MAC_RQC1R_PTPQ            (OSI_BIT(6) | OSI_BIT(5) |          \
                                                 OSI_BIT(4))
#define EQOS_MAC_RQC1R_PTPQ_SHIFT      4U
#define EQOS_MAC_LPI_CSR_LPITE         OSI_BIT(20)
#define EQOS_MAC_LPI_CSR_LPITXA        OSI_BIT(19)
#define EQOS_MAC_LPI_CSR_PLS           OSI_BIT(17)
#define EQOS_MAC_LPI_CSR_LPIEN         OSI_BIT(16)
#endif /* !OSI_STRIPPED_LIB */

#define EQOS_CORE_MAC_STSR        0x0B08
#define EQOS_CORE_MAC_STNSR       0x0B0C
#define EQOS_MCR_IPG_MASK         0x7000000U
#define EQOS_MCR_IPG_SHIFT        24U
#define EQOS_MCR_IPG              0x7U
#define EQOS_MTL_EST_CONTROL      0x0C50
#define EQOS_MTL_EST_OVERHEAD     0x0C54
#define EQOS_MTL_EST_STATUS       0x0C58
#define EQOS_MTL_EST_SCH_ERR      0x0C60
#define EQOS_MTL_EST_FRMS_ERR     0x0C64
#define EQOS_MTL_EST_ITRE         0x0C70
#define EQOS_MTL_EST_GCL_CONTROL  0x0C80
#define EQOS_MTL_EST_DATA         0x0C84
#define EQOS_MTL_FPE_CTS          0x0C90
#define EQOS_MTL_FPE_ADV          0x0C94
#define EQOS_MTL_RXP_CS           0x0CA0
#define EQOS_MTL_RXP_INTR_CS      0x0CA4
#define EQOS_MTL_RXP_IND_CS       0x0CB0
#define EQOS_MTL_RXP_IND_DATA     0x0CB4
#define EQOS_MTL_TXQ_ETS_CR(x)    ((0x0040U * (x)) + 0x0D10U)
#define EQOS_MTL_TXQ_ETS_SSCR(x)  ((0x0040U * (x)) + 0x0D1CU)
#define EQOS_MTL_TXQ_ETS_HCR(x)   ((0x0040U * (x)) + 0x0D20U)
#define EQOS_MTL_TXQ_ETS_LCR(x)   ((0x0040U * (x)) + 0x0D24U)
#define EQOS_MTL_INTR_STATUS        0x0C20
#define EQOS_MTL_OP_MODE            0x0C00
#define EQOS_MAC_FPE_CTS            0x0234
#define EQOS_IMR_FPEIE              OSI_BIT(17)
#define EQOS_MTL_FRP_IE2_DCH_SHIFT  24U
#define EQOS_DMA_ISR_MTLIS          OSI_BIT(16)

/**
 * @addtogroup EQOS-MTL-FRP FRP Indirect Access register defines
 *
 * @brief EQOS MTL FRP register defines
 * @{
 */
#define EQOS_MTL_FRP_READ_UDELAY  1U
#define EQOS_MTL_FRP_READ_RETRY   10000U

/* FRP Control and Status register defines */
#define EQOS_MTL_RXP_CS_RXPI       OSI_BIT(31)
#define EQOS_MTL_RXP_CS_NPE        (OSI_BIT(23) | OSI_BIT(22) |              \
                                                 OSI_BIT(21) | OSI_BIT(20) | \
                                                 OSI_BIT(19) | OSI_BIT(18) | \
                                                 OSI_BIT(17) | OSI_BIT(16))
#define EQOS_MTL_RXP_CS_NPE_SHIFT  16U
#define EQOS_MTL_RXP_CS_NVE        (OSI_BIT(7) | OSI_BIT(6) |              \
                                                 OSI_BIT(5) | OSI_BIT(4) | \
                                                 OSI_BIT(3) | OSI_BIT(2) | \
                                                 OSI_BIT(1) | OSI_BIT(0))
/* Indirect register defines */
#define EQOS_MTL_RXP_IND_CS_BUSY   OSI_BIT(31)
#define EQOS_MTL_RXP_IND_CS_WRRDN  OSI_BIT(16)
#define EQOS_MTL_RXP_IND_CS_ADDR   (OSI_BIT(9) | OSI_BIT(8) |              \
                                                 OSI_BIT(7) | OSI_BIT(6) | \
                                                 OSI_BIT(5) | OSI_BIT(4) | \
                                                 OSI_BIT(3) | OSI_BIT(2) | \
                                                 OSI_BIT(1) | OSI_BIT(0))
/** @} */

/* FRP Interrupt Control and Status register */
#define EQOS_MTL_RXP_INTR_CS_PDRFIE   OSI_BIT(19)
#define EQOS_MTL_RXP_INTR_CS_FOOVIE   OSI_BIT(18)
#define EQOS_MTL_RXP_INTR_CS_NPEOVIE  OSI_BIT(17)
#define EQOS_MTL_RXP_INTR_CS_NVEOVIE  OSI_BIT(16)
#define EQOS_MTL_RXP_INTR_CS_PDRFIS   OSI_BIT(3)
#define EQOS_MTL_RXP_INTR_CS_FOOVIS   OSI_BIT(2)
#define EQOS_MTL_RXP_INTR_CS_NPEOVIS  OSI_BIT(1)
#define EQOS_MTL_RXP_INTR_CS_NVEOVIS  OSI_BIT(0)

#ifndef OSI_STRIPPED_LIB
#define EQOS_RXQ_DMA_MAP0_MASK      0x13131313U
#define EQOS_MTL_TXQ_QW_MASK        0x1FFFFFU
#define EQOS_PAD_AUTO_CAL_CFG_MASK  0x7FFFFFFFU
#define EQOS_MTL_TXQ_OP_MODE_MASK   0xFF007EU
#define EQOS_MTL_RXQ_OP_MODE_MASK   0xFFFFFFBU
#define EQOS_MAC_RQC1R_MASK         0xF77077U
#endif /* !OSI_STRIPPED_LIB */
#define EQOS_MTL_TXQ_ETS_SSCR_SSC_MASK   0x00003FFFU
#define EQOS_MTL_TXQ_ETS_QW_ISCQW_MASK   0x000FFFFFU
#define EQOS_MTL_TXQ_ETS_HCR_HC_MASK     0x1FFFFFFFU
#define EQOS_MTL_TXQ_ETS_LCR_LC_MASK     0x1FFFFFFFU
#define EQOS_MTL_TXQ_ETS_CR_CC           OSI_BIT(3)
#define EQOS_MTL_TXQ_ETS_CR_AVALG        OSI_BIT(2)
#define EQOS_MTL_TXQ_ETS_CR_CC_SHIFT     3U
#define EQOS_MTL_TXQ_ETS_CR_AVALG_SHIFT  2U
#define EQOS_MAC_RQC1R_FPRQ              (OSI_BIT(26) | OSI_BIT(25) |        \
                                                 OSI_BIT(24))
#define EQOS_MAC_RQC1R_FPRQ_SHIFT        24U
/* Indirect Instruction Table defines */
#define EQOS_MTL_FRP_IE0(x)  (((x) * 0x4U) + 0x0U)
#define EQOS_MTL_FRP_IE1(x)  (((x) * 0x4U) + 0x1U)
#define EQOS_MTL_FRP_IE2(x)  (((x) * 0x4U) + 0x2U)
#define EQOS_MTL_FRP_IE3(x)  (((x) * 0x4U) + 0x3U)
#define EQOS_MTL_FRP_IE2_DCH        (OSI_BIT(31) | OSI_BIT(30) |             \
                                                 OSI_BIT(29) | OSI_BIT(28) | \
                                                 OSI_BIT(27) | OSI_BIT(26) | \
                                                 OSI_BIT(25) | OSI_BIT(24))
#define EQOS_MTL_FRP_IE2_OKI        (OSI_BIT(23) | OSI_BIT(22) |             \
                                                 OSI_BIT(21) | OSI_BIT(20) | \
                                                 OSI_BIT(19) | OSI_BIT(18) | \
                                                 OSI_BIT(17) | OSI_BIT(16))
#define EQOS_MTL_FRP_IE2_OKI_SHIFT  16U
#define EQOS_MTL_FRP_IE2_FO         (OSI_BIT(13) | OSI_BIT(12) |             \
                                                 OSI_BIT(11) | OSI_BIT(10) | \
                                                 OSI_BIT(9) | OSI_BIT(8))
#define EQOS_MTL_FRP_IE2_FO_SHIFT   8U
#define EQOS_MTL_FRP_IE2_NC         OSI_BIT(3)
#define EQOS_MTL_FRP_IE2_IM         OSI_BIT(2)
#define EQOS_MTL_FRP_IE2_RF         OSI_BIT(1)
#define EQOS_MTL_FRP_IE2_AF         OSI_BIT(0)

/**
 * @addtogroup EQOS-HW Hardware Register offsets
 *
 * @brief EQOS HW register offsets
 * @{
 */
#define EQOS_MAC_MCR       0x0000
#define EQOS_MAC_EXTR      0x0004
#define EQOS_MAC_VLAN_TAG  0x0050
#define EQOS_MAC_VLANTIR   0x0060
#define EQOS_MAC_RQC0R     0x00A0
#define EQOS_MAC_RQC1R     0x00A4
#define EQOS_MAC_RQC2R     0x00A8
#define EQOS_MAC_ISR       0x00B0
#define EQOS_MAC_IMR       0x00B4
#ifndef OSI_STRIPPED_LIB
#define EQOS_MAC_1US_TIC_CNTR  0x00DC
#define EQOS_MAC_ANS           0x00E4
#endif /* !OSI_STRIPPED_LIB */
#define EQOS_MAC_PCS  0x00F8

#define EQOS_MAC_DEBUG         0x0114
#define EQOS_MAC_DEBUG_RPESTS  OSI_BIT(0)
#define EQOS_MAC_DEBUG_TPESTS  OSI_BIT(16)

#define EQOS_MAC_MDIO_ADDRESS  0x0200
#define EQOS_MAC_MDIO_DATA     0x0204
#define EQOS_MAC_ADDRH(x)  ((0x0008U * (x)) + 0x0300U)
#define EQOS_MAC_ADDRL(x)  ((0x0008U * (x)) + 0x0304U)
#define EQOS_MMC_CNTRL             0x0700
#define EQOS_MMC_TX_INTR_MASK      0x0710
#define EQOS_MMC_RX_INTR_MASK      0x070C
#define EQOS_MMC_IPC_RX_INTR_MASK  0x0800
#define EQOS_MAC_L3L4_CTR(x)  ((0x0030U * (x)) + 0x0900U)
#define EQOS_MAC_L3_AD1R(x)   ((0x0030U * (x)) + 0x0914U)
#ifndef OSI_STRIPPED_LIB
#define EQOS_MAC_L3_AD0R(x)  ((0x0030U * (x)) + 0x0910U)
#define EQOS_MAC_L3_AD2R(x)  ((0x0030U * (x)) + 0x0918U)
#define EQOS_MAC_L3_AD3R(x)  ((0x0030U * (x)) + 0x091CU)
#define EQOS_MAC_L4_ADR(x)   ((0x0030U * (x)) + 0x0904U)
#endif /* !OSI_STRIPPED_LIB */
#define EQOS_MAC_TCR           0x0B00
#define EQOS_MAC_SSIR          0x0B04
#define EQOS_MAC_STSUR         0x0B10
#define EQOS_MAC_STNSUR        0x0B14
#define EQOS_MAC_TAR           0x0B18
#define EQOS_MAC_PPS_CTL       0x0B70
#define EQOS_MAC_PPS_TT_SEC    0x0B80
#define EQOS_MAC_PPS_TT_NSEC   0x0B84
#define EQOS_MAC_PPS_INTERVAL  0x0B88
#define EQOS_MAC_PPS_WIDTH     0x0B8C
#define EQOS_DMA_BMR           0x1000
#define EQOS_DMA_SBUS          0x1004
#define EQOS_DMA_ISR           0x1008
#define EQOS_PTP_CLK_SPEED     208333334U
#define EQOS_X_PTP_CLK_SPEED   312500000U
/** @} */

/**
 * @addtogroup EQOS-MTL MTL HW Register offsets
 *
 * @brief EQOS MTL HW Register offsets
 * @{
 */
#define EQOS_MTL_RXQ_DMA_MAP0  0x0C30
#define EQOS_MTL_RXQ_DMA_MAP1  0x0C34
#define EQOS_MTL_CHX_TX_OP_MODE(x)  ((0x0040U * (x)) + 0x0D00U)
#define EQOS_MTL_TXQ_QW(x)          ((0x0040U * (x)) + 0x0D18U)
#define EQOS_MTL_CHX_RX_OP_MODE(x)  ((0x0040U * (x)) + 0x0D30U)
/** @} */

/**
 * @addtogroup EQOS-Wrapper EQOS Wrapper HW Register offsets
 *
 * @brief EQOS Wrapper register offsets
 * @{
 */
#define EQOS_AXI_ASID_CTRL      0x8400U
#define EQOS_AXI_ASID1_CTRL     0x8404U
#define EQOS_PAD_CRTL           0x8800U
#define EQOS_PAD_AUTO_CAL_CFG   0x8804U
#define EQOS_PAD_AUTO_CAL_STAT  0x880CU
#define EQOS_VIRT_INTR_APB_CHX_CNTRL(x)  (0x8200U + ((x) * 4U))
#define VIRTUAL_APB_ERR_CTRL          0x8300
#define EQOS_WRAP_COMMON_INTR_ENABLE  0x8704

#ifdef HSI_SUPPORT
#define EQOS_REGISTER_PARITY_ERR     OSI_BIT(5)
#define EQOS_CORE_CORRECTABLE_ERR    OSI_BIT(4)
#define EQOS_CORE_UNCORRECTABLE_ERR  OSI_BIT(3)
#endif

#define EQOS_MAC_SBD_INTR             OSI_BIT(2)
#define EQOS_WRAP_COMMON_INTR_STATUS  0x8708

/** @} */

/**
 * @addtogroup HW Register BIT values
 *
 * @brief consists of corresponding EQOS MAC, MTL register bit values
 * @{
 */
/* Enable for DA-based or L2/L4 based  DMA Channel selection*/
#define EQOS_RXQ_TO_DMA_CHAN_MAP           0x03020100U
#define EQOS_RXQ_TO_DMA_CHAN_MAP1          0x07060504U
#define EQOS_RXQ_TO_DMA_CHAN_MAP_DCS_EN    0x13121110U
#define EQOS_RXQ_TO_DMA_CHAN_MAP1_DCS_EN   0x17161514U
#define EQOS_PAD_AUTO_CAL_CFG_ENABLE       OSI_BIT(29)
#define EQOS_PAD_AUTO_CAL_CFG_START        OSI_BIT(31)
#define EQOS_PAD_AUTO_CAL_STAT_ACTIVE      OSI_BIT(31)
#define EQOS_PAD_CRTL_E_INPUT_OR_E_PWRD    OSI_BIT(31)
#define EQOS_PAD_CRTL_PD_OFFSET_MASK       0x1F00U
#define EQOS_PAD_CRTL_PU_OFFSET_MASK       0x1FU
#define EQOS_MCR_IPC                       OSI_BIT(27)
#define EQOS_MMC_CNTRL_CNTRST              OSI_BIT(0)
#define EQOS_MMC_CNTRL_RSTONRD             OSI_BIT(2)
#define EQOS_MMC_CNTRL_CNTPRST             OSI_BIT(4)
#define EQOS_MMC_CNTRL_CNTPRSTLVL          OSI_BIT(5)
#define EQOS_MTL_TSF                       OSI_BIT(1)
#define EQOS_MTL_TXQEN                     OSI_BIT(3)
#define EQOS_MTL_RSF                       OSI_BIT(5)
#define EQOS_MCR_RE                        OSI_BIT(0)
#define EQOS_MCR_TE                        OSI_BIT(1)
#define EQOS_MCR_DO                        OSI_BIT(10)
#define EQOS_MCR_DM                        OSI_BIT(13)
#define EQOS_MCR_FES                       OSI_BIT(14)
#define EQOS_MCR_PS                        OSI_BIT(15)
#define EQOS_MCR_JE                        OSI_BIT(16)
#define EQOS_MCR_JD                        OSI_BIT(17)
#define EQOS_MCR_WD                        OSI_BIT(19)
#define EQOS_MCR_ACS                       OSI_BIT(20)
#define EQOS_MCR_CST                       OSI_BIT(21)
#define EQOS_MCR_GPSLCE                    OSI_BIT(23)
#define EQOS_IMR_RGSMIIIE                  OSI_BIT(0)
#define EQOS_MAC_PCS_LNKSTS                OSI_BIT(19)
#define EQOS_MAC_PCS_LNKMOD                OSI_BIT(16)
#define EQOS_MAC_PCS_LNKSPEED              (OSI_BIT(17) | OSI_BIT(18))
#define EQOS_MAC_PCS_LNKSPEED_10           0x0U
#define EQOS_MAC_PCS_LNKSPEED_100          OSI_BIT(17)
#define EQOS_MAC_PCS_LNKSPEED_1000         OSI_BIT(18)
#define EQOS_MAC_VLANTIR_VLTI              OSI_BIT(20)
#define EQOS_MAC_VLANTR_EVLS_ALWAYS_STRIP  ((nveu32_t)0x3 << 21U)
#define EQOS_MAC_VLANTR_EVLRXS             OSI_BIT(24)
#define EQOS_MAC_VLANTR_DOVLTC             OSI_BIT(20)
#define EQOS_MAC_VLANTR_ERIVLT             OSI_BIT(27)
#define EQOS_MAC_VLANTIRR_CSVL             OSI_BIT(19)
#define EQOS_DMA_SBUS_BLEN8                OSI_BIT(2)
#define EQOS_DMA_SBUS_BLEN16               OSI_BIT(3)
#define EQOS_DMA_SBUS_EAME                 OSI_BIT(11)
#define EQOS_DMA_BMR_DPSW                  OSI_BIT(8)
#define EQOS_MAC_RQC1R_MCBCQ               (OSI_BIT(18) | OSI_BIT(17) |     \
                                                 OSI_BIT(16))
#define EQOS_MAC_RQC1R_MCBCQ_SHIFT         16U
#define EQOS_MAC_RQC1R_MCBCQ3              0x3U
#define EQOS_MAC_RQC1R_MCBCQ7              0x7U
#define EQOS_MAC_RQC1R_MCBCQEN             OSI_BIT(20)

#define EQOS_DMA_ISR_MACIS  OSI_BIT(17)

#ifdef HSI_SUPPORT
#define EQOS_DMA_ISR_TXSTSIS  OSI_BIT(13)
#define EQOS_IMR_TXESIE       OSI_BIT(13)
#endif

#define EQOS_MAC_ISR_RGSMIIS      OSI_BIT(0)
#define EQOS_MAC_IMR_FPEIS        OSI_BIT(17)
#define EQOS_MTL_TXQ_QW_ISCQW     OSI_BIT(4)
#define EQOS_RXQ_EN_MASK          (OSI_BIT(0) | OSI_BIT(1))
#define EQOS_DMA_SBUS_RD_OSR_LMT  0x001F0000U
#define EQOS_DMA_SBUS_WR_OSR_LMT  0x1F000000U
#define EQOS_MTL_TXQ_SIZE_SHIFT   16U
#define EQOS_MTL_RXQ_SIZE_SHIFT   20U
#ifndef OSI_STRIPPED_LIB
#define EQOS_MAC_ENABLE_LM               OSI_BIT(12)
#define EQOS_MCR_ARPEN                   OSI_BIT(31)
#define EQOS_RX_CLK_SEL                  OSI_BIT(8)
#define EQOS_MTL_OP_MODE_DTXSTS          OSI_BIT(1)
#define EQOS_MAC_VLAN_TR                 0x0050U
#define EQOS_MAC_VLAN_TR_VTIM            OSI_BIT(17)
#define EQOS_MAC_VLAN_TR_VTIM_SHIFT      17
#define EQOS_MAC_VLAN_TR_VTHM            OSI_BIT(25)
#define EQOS_MAC_PFR_SHIFT               16
#define EQOS_MTL_OP_MODE_DTXSTS          OSI_BIT(1)
#define EQOS_MAC_EXTR_DCRCC              OSI_BIT(16)
#define EQOS_MTL_TXQ_ETS_SSCR_SSC_MASK   0x00003FFFU
#define EQOS_MTL_TXQ_ETS_QW_ISCQW_MASK   0x000FFFFFU
#define EQOS_MTL_TXQ_ETS_HCR_HC_MASK     0x1FFFFFFFU
#define EQOS_MTL_TXQ_ETS_LCR_LC_MASK     0x1FFFFFFFU
#define EQOS_MTL_TXQ_ETS_CR_AVALG        OSI_BIT(2)
#define EQOS_MTL_TXQ_ETS_CR_AVALG_SHIFT  2U
#define EQOS_MTL_TXQ_ETS_CR_CC           OSI_BIT(3)
#define EQOS_MTL_TXQ_ETS_CR_CC_SHIFT     3U
#define EQOS_MAC_EXTR_PDC                OSI_BIT(19)
#endif /* !OSI_STRIPPED_LIB */
#define EQOS_CORE_MAC_STNSR_TSSS_MASK   0x7FFFFFFFU
#define EQOS_MAC_EXTR_EIPG              0x3U
#define EQOS_MAC_EXTR_EIPG_MASK         0x3E000000U
#define EQOS_MAC_EXTR_EIPG_SHIFT        25U
#define EQOS_MAC_EXTR_EIPGEN            OSI_BIT(24)
#define EQOS_MTL_TXQEN_MASK             (OSI_BIT(3) | OSI_BIT(2))
#define EQOS_MTL_TXQEN_MASK_SHIFT       2U
#define EQOS_MTL_OP_MODE_FRPE           OSI_BIT(15)
#define EQOS_MAC_EXTR_PDC               OSI_BIT(19)
#define EQOS_MTL_RXQ_OP_MODE_EHFC       OSI_BIT(7)
#define EQOS_MTL_RXQ_OP_MODE_RFA_SHIFT  8U
#define EQOS_MTL_RXQ_OP_MODE_RFA_MASK   0x00003F00U
#define EQOS_MTL_RXQ_OP_MODE_RFD_SHIFT  14U
#define EQOS_MTL_RXQ_OP_MODE_RFD_MASK   0x000FC000U
#ifndef OSI_STRIPPED_LIB
#define EQOS_MAC_L4_SP_MASK   0x0000FFFFU
#define EQOS_MAC_L4_DP_MASK   0xFFFF0000U
#define EQOS_MAC_L4_DP_SHIFT  16
#endif /* !OSI_STRIPPED_LIB */
#define EQOS_MAC_ADDRH_DCS                 (OSI_BIT(23) | OSI_BIT(22) |      \
                                                 OSI_BIT(21) | OSI_BIT(20) | \
                                                 OSI_BIT(19) | OSI_BIT(18) | \
                                                 OSI_BIT(17) | OSI_BIT(16))
#define EQOS_MAC_ADDRH_DCS_SHIFT           16
#define EQOS_MAC_ADDRH_MBC                 (OSI_BIT(29) | OSI_BIT(28) |      \
                                                 OSI_BIT(27) | OSI_BIT(26) | \
                                                 OSI_BIT(25) | OSI_BIT(24))
#define EQOS_MAC_ADDRH_MBC_SHIFT           24
#define EQOS_MAX_MASK_BYTE                 0x3FU
#define EQOS_MAX_MAC_ADDR_REG              32U
#define EQOS_MAC_ADDRH_SA                  OSI_BIT(30)
#define EQOS_MAC_ADDRH_SA_SHIFT            30
#define EQOS_MAC_ADDRH_AE                  OSI_BIT(31)
#define EQOS_MAC_RQC2_PSRQ_MASK            ((nveu32_t)0xFF)
#define EQOS_MAC_RQC2_PSRQ_SHIFT           8U
#define EQOS_MAC_TCR_TSUPDT                OSI_BIT(3)
#define EQOS_MAC_STNSUR_ADDSUB_SHIFT       31U
#define EQOS_MAC_GMIIDR_GD_WR_MASK         0xFFFF0000U
#define EQOS_MAC_GMIIDR_GD_MASK            0xFFFFU
#define EQOS_MDIO_PHY_ADDR_SHIFT           21U
#define EQOS_MDIO_PHY_REG_SHIFT            16U
#define EQOS_MDIO_PHY_REG_CR_SHIF          8U
#define EQOS_MDIO_PHY_REG_WRITE            OSI_BIT(2)
#define EQOS_MDIO_PHY_REG_GOC_READ         (OSI_BIT(2) | OSI_BIT(3))
#define EQOS_MDIO_PHY_REG_SKAP             OSI_BIT(4)
#define EQOS_MDIO_PHY_REG_C45E             OSI_BIT(1)
#define EQOS_MAC_GMII_BUSY                 0x00000001U
#define EQOS_MAC_EXTR_GPSL_MSK             0x00003FFFU
#define EQOS_MDIO_DATA_REG_PHYREG_MASK     0xFFFFU
#define EQOS_MDIO_DATA_REG_PHYREG_SHIFT    16U
#define EQOS_MDIO_DATA_REG_DEV_ADDR_MASK   0x1FU
#define EQOS_MDIO_DATA_REG_DEV_ADDR_SHIFT  16U

#define EQOS_DMA_CHAN_INTR_STATUS  0xFU

#define EQOS_ASID_CTRL_SHIFT_24  24U
#define EQOS_ASID_CTRL_SHIFT_16  16U
#define EQOS_ASID_CTRL_SHIFT_8   8U

#define TEGRA_SID_EQOS           (nveu32_t)20
#define TEGRA_SID_EQOS_CH3       ((TEGRA_SID_EQOS) << EQOS_ASID_CTRL_SHIFT_24)
#define TEGRA_SID_EQOS_CH2       ((TEGRA_SID_EQOS) << EQOS_ASID_CTRL_SHIFT_16)
#define TEGRA_SID_EQOS_CH1       ((TEGRA_SID_EQOS) << EQOS_ASID_CTRL_SHIFT_8)
#define EQOS_AXI_ASID_CTRL_VAL   ((TEGRA_SID_EQOS_CH3) |\
                                 (TEGRA_SID_EQOS_CH2) |\
                                 (TEGRA_SID_EQOS_CH1) |\
                                 (TEGRA_SID_EQOS))
#define TEGRA_SID_EQOS_CH7       ((TEGRA_SID_EQOS) << EQOS_ASID_CTRL_SHIFT_24)
#define TEGRA_SID_EQOS_CH6       ((TEGRA_SID_EQOS) << EQOS_ASID_CTRL_SHIFT_16)
#define TEGRA_SID_EQOS_CH5       ((TEGRA_SID_EQOS) << EQOS_ASID_CTRL_SHIFT_8)
#define EQOS_AXI_ASID1_CTRL_VAL  ((TEGRA_SID_EQOS_CH7) |\
                                 (TEGRA_SID_EQOS_CH6) |\
                                 (TEGRA_SID_EQOS_CH5) |\
                                 (TEGRA_SID_EQOS))
#define EQOS_MMC_INTR_DISABLE    0xFFFFFFFFU

/* MAC FPE control/statusOSI_BITmap */
#define EQOS_MAC_FPE_CTS_EFPE  OSI_BIT(0)
#define EQOS_MAC_FPE_CTS_TRSP  OSI_BIT(19)
#define EQOS_MAC_FPE_CTS_TVER  OSI_BIT(18)
#define EQOS_MAC_FPE_CTS_RRSP  OSI_BIT(17)
#define EQOS_MAC_FPE_CTS_RVER  OSI_BIT(16)
#define EQOS_MAC_FPE_CTS_SRSP  OSI_BIT(2)

/* MTL_EST_CONTROL */
#define EQOS_MTL_EST_CONTROL_PTOV        (OSI_BIT(24) | OSI_BIT(25) |        \
                                                 OSI_BIT(26) | OSI_BIT(27) | \
                                                 OSI_BIT(28) | OSI_BIT(29) | \
                                                 OSI_BIT(30) | OSI_BIT(31))
#define EQOS_MTL_EST_CONTROL_PTOV_SHIFT  24U
#define EQOS_MTL_EST_PTOV_RECOMMEND      32U
#define EQOS_MTL_EST_CONTROL_CTOV        (OSI_BIT(12) | OSI_BIT(13) |        \
                                                 OSI_BIT(14) | OSI_BIT(15) | \
                                                 OSI_BIT(16) | OSI_BIT(17) | \
                                                 OSI_BIT(18) | OSI_BIT(19) | \
                                                 OSI_BIT(20) | OSI_BIT(21) | \
                                                 OSI_BIT(22) | OSI_BIT(23))
#define EQOS_MTL_EST_CONTROL_CTOV_SHIFT  12U
#define EQOS_MTL_EST_CTOV_RECOMMEND      94U
#define EQOS_8PTP_CYCLE                  40U
#define EQOS_MTL_EST_CONTROL_LCSE        (OSI_BIT(6) | OSI_BIT(5))
#define EQOS_MTL_EST_CONTROL_LCSE_VAL    0U
#define EQOS_MTL_EST_CONTROL_DFBS        OSI_BIT(5)
#define EQOS_MTL_EST_CONTROL_DDBF        OSI_BIT(4)
#define EQOS_MTL_EST_CONTROL_EEST        OSI_BIT(0)
#define EQOS_MTL_EST_OVERHEAD_OVHD       (OSI_BIT(5) | OSI_BIT(4) |        \
                                                 OSI_BIT(3) | OSI_BIT(2) | \
                                                 OSI_BIT(1) | OSI_BIT(0))
#define EQOS_MTL_EST_OVERHEAD_RECOMMEND  0x17U
/* EST GCL controlOSI_BITmap */
#define EQOS_MTL_EST_ADDR_SHIFT  8U
/* EST GCRA addresses */
#define EQOS_MTL_EST_BTR_LOW   ((nveu32_t)0x0 <<                  \
                                                 EQOS_MTL_EST_ADDR_SHIFT)
#define EQOS_MTL_EST_BTR_HIGH  ((nveu32_t)0x1 <<                  \
                                                 EQOS_MTL_EST_ADDR_SHIFT)
#define EQOS_MTL_EST_CTR_LOW   ((nveu32_t)0x2 <<                  \
                                                 EQOS_MTL_EST_ADDR_SHIFT)
#define EQOS_MTL_EST_CTR_HIGH  ((nveu32_t)0x3 <<                  \
                                                 EQOS_MTL_EST_ADDR_SHIFT)
#define EQOS_MTL_EST_TER       ((nveu32_t)0x4 <<                  \
                                                 EQOS_MTL_EST_ADDR_SHIFT)
#define EQOS_MTL_EST_LLR       ((nveu32_t)0x5 <<                  \
                                                 EQOS_MTL_EST_ADDR_SHIFT)
/*EST MTL interrupt STATUS and ERR*/
#define EQOS_MTL_IS_ESTIS  OSI_BIT(18)
/* MTL_EST_STATUS*/
#define EQOS_MTL_EST_STATUS_CGCE  OSI_BIT(4)
#define EQOS_MTL_EST_STATUS_HLBS  OSI_BIT(3)
#define EQOS_MTL_EST_STATUS_HLBF  OSI_BIT(2)
#define EQOS_MTL_EST_STATUS_BTRE  OSI_BIT(1)
#define EQOS_MTL_EST_STATUS_SWLC  OSI_BIT(0)
#if defined (MACSEC_SUPPORT)
/* MACSEC Recommended value*/
#define EQOS_MTL_EST_CTOV_MACSEC_RECOMMEND  758U
#endif /*  MACSEC_SUPPORT */
/* EQOS RGMII Rx padctrl registers E_INPUT bit */
#define EQOS_PADCTL_EQOS_E_INPUT  OSI_BIT(6)
/** @} */

void
update_ehfc_rfa_rfd (
  nveu32_t  rx_fifo,
  nveu32_t  *value
  );

/**
 * @addtogroup EQOS-MAC-Feature EQOS MAC HW feature registers bit fields
 *
 * @brief HW feature register bit masks and bit shifts.
 * @{
 */
#define EQOS_MAC_HFR0_MIISEL_MASK   0x1U
#define EQOS_MAC_HFR0_MIISEL_SHIFT  0U

#define EQOS_MAC_HFR0_GMIISEL_MASK   0x1U
#define EQOS_MAC_HFR0_GMIISEL_SHIFT  1U

#define EQOS_MAC_HFR0_HDSEL_MASK   0x1U
#define EQOS_MAC_HFR0_HDSEL_SHIFT  2U

#define EQOS_MAC_HFR0_PCSSEL_MASK   0x1U
#define EQOS_MAC_HFR0_PCSSEL_SHIFT  3U

#define EQOS_MAC_HFR0_VLHASH_MASK   0x1U
#define EQOS_MAC_HFR0_VLHASH_SHIFT  4U

#define EQOS_MAC_HFR0_SMASEL_MASK   0x1U
#define EQOS_MAC_HFR0_SMASEL_SHIFT  5U

#define EQOS_MAC_HFR0_RWKSEL_MASK   0x1U
#define EQOS_MAC_HFR0_RWKSEL_SHIFT  6U

#define EQOS_MAC_HFR0_MGKSEL_MASK   0x1U
#define EQOS_MAC_HFR0_MGKSEL_SHIFT  7U

#define EQOS_MAC_HFR0_MMCSEL_MASK   0x1U
#define EQOS_MAC_HFR0_MMCSEL_SHIFT  8U

#define EQOS_MAC_HFR0_ARPOFFLDEN_MASK   0x1U
#define EQOS_MAC_HFR0_ARPOFFLDEN_SHIFT  9U

#define EQOS_MAC_HFR0_TSSSEL_MASK   0x1U
#define EQOS_MAC_HFR0_TSSSEL_SHIFT  12U

#define EQOS_MAC_HFR0_EEESEL_MASK   0x1U
#define EQOS_MAC_HFR0_EEESEL_SHIFT  13U

#define EQOS_MAC_HFR0_TXCOESEL_MASK   0x1U
#define EQOS_MAC_HFR0_TXCOESEL_SHIFT  14U

#define EQOS_MAC_HFR0_RXCOE_MASK   0x1U
#define EQOS_MAC_HFR0_RXCOE_SHIFT  16U

#define EQOS_MAC_HFR0_ADDMACADRSEL_MASK   0x1FU
#define EQOS_MAC_HFR0_ADDMACADRSEL_SHIFT  18U

#define EQOS_MAC_HFR0_MACADR32SEL_MASK   0x1U
#define EQOS_MAC_HFR0_MACADR32SEL_SHIFT  23U

#define EQOS_MAC_HFR0_MACADR64SEL_MASK   0x1U
#define EQOS_MAC_HFR0_MACADR64SEL_SHIFT  24U

#define EQOS_MAC_HFR0_TSINTSEL_MASK   0x3U
#define EQOS_MAC_HFR0_TSINTSEL_SHIFT  25U

#define EQOS_MAC_HFR0_SAVLANINS_MASK   0x1U
#define EQOS_MAC_HFR0_SAVLANINS_SHIFT  27U

#define EQOS_MAC_HFR0_ACTPHYSEL_MASK   0x7U
#define EQOS_MAC_HFR0_ACTPHYSEL_SHIFT  28U

#define EQOS_MAC_HFR1_RXFIFOSIZE_MASK   0x1FU
#define EQOS_MAC_HFR1_RXFIFOSIZE_SHIFT  0U

#define EQOS_MAC_HFR1_TXFIFOSIZE_MASK   0x1FU
#define EQOS_MAC_HFR1_TXFIFOSIZE_SHIFT  6U

#define EQOS_MAC_HFR1_OSTEN_MASK   0x1U
#define EQOS_MAC_HFR1_OSTEN_SHIFT  11U

#define EQOS_MAC_HFR1_PTOEN_MASK   0x1U
#define EQOS_MAC_HFR1_PTOEN_SHIFT  12U

#define EQOS_MAC_HFR1_ADVTHWORD_MASK   0x1U
#define EQOS_MAC_HFR1_ADVTHWORD_SHIFT  13U

#define EQOS_MAC_HFR1_ADDR64_MASK   0x3U
#define EQOS_MAC_HFR1_ADDR64_SHIFT  14U

#define EQOS_MAC_HFR1_DCBEN_MASK   0x1U
#define EQOS_MAC_HFR1_DCBEN_SHIFT  16U

#define EQOS_MAC_HFR1_SPHEN_MASK   0x1U
#define EQOS_MAC_HFR1_SPHEN_SHIFT  17U

#define EQOS_MAC_HFR1_TSOEN_MASK   0x1U
#define EQOS_MAC_HFR1_TSOEN_SHIFT  18U

#define EQOS_MAC_HFR1_DMADEBUGEN_MASK   0x1U
#define EQOS_MAC_HFR1_DMADEBUGEN_SHIFT  19U

#define EQOS_MAC_HFR1_AVSEL_MASK   0x1U
#define EQOS_MAC_HFR1_AVSEL_SHIFT  20U

#define EQOS_MAC_HFR1_RAVSEL_MASK   0x1U
#define EQOS_MAC_HFR1_RAVSEL_SHIFT  21U

#define EQOS_MAC_HFR1_POUOST_MASK   0x1U
#define EQOS_MAC_HFR1_POUOST_SHIFT  23U

#define EQOS_MAC_HFR1_HASHTBLSZ_MASK   0x3U
#define EQOS_MAC_HFR1_HASHTBLSZ_SHIFT  24U

#define EQOS_MAC_HFR1_L3L4FILTERNUM_MASK   0xFU
#define EQOS_MAC_HFR1_L3L4FILTERNUM_SHIFT  27U

#define EQOS_MAC_HFR2_RXQCNT_MASK   0xFU
#define EQOS_MAC_HFR2_RXQCNT_SHIFT  0U

#define EQOS_MAC_HFR2_TXQCNT_MASK   0xFU
#define EQOS_MAC_HFR2_TXQCNT_SHIFT  6U

#define EQOS_MAC_HFR2_RXCHCNT_MASK   0xFU
#define EQOS_MAC_HFR2_RXCHCNT_SHIFT  12U

#define EQOS_MAC_HFR2_TXCHCNT_MASK   0xFU
#define EQOS_MAC_HFR2_TXCHCNT_SHIFT  18U

#define EQOS_MAC_HFR2_PPSOUTNUM_MASK   0x7U
#define EQOS_MAC_HFR2_PPSOUTNUM_SHIFT  24U

#define EQOS_MAC_HFR2_AUXSNAPNUM_MASK   0x7U
#define EQOS_MAC_HFR2_AUXSNAPNUM_SHIFT  28U

#define EQOS_MAC_HFR3_NRVF_MASK   0x7U
#define EQOS_MAC_HFR3_NRVF_SHIFT  0U

#define EQOS_MAC_HFR3_CBTISEL_MASK   0x1U
#define EQOS_MAC_HFR3_CBTISEL_SHIFT  4U

#define EQOS_MAC_HFR3_DVLAN_MASK   0x1U
#define EQOS_MAC_HFR3_DVLAN_SHIFT  5U

#define EQOS_MAC_HFR3_FRPSEL_MASK   0x1U
#define EQOS_MAC_HFR3_FRPSEL_SHIFT  10U

#define EQOS_MAC_HFR3_FRPPB_MASK   0x3U
#define EQOS_MAC_HFR3_FRPPB_SHIFT  11U

#define EQOS_MAC_HFR3_FRPES_MASK   0x3U
#define EQOS_MAC_HFR3_FRPES_SHIFT  13U

#define EQOS_MAC_HFR3_ESTSEL_MASK   0x1U
#define EQOS_MAC_HFR3_ESTSEL_SHIFT  16U

#define EQOS_MAC_HFR3_GCLDEP_MASK   0x7U
#define EQOS_MAC_HFR3_GCLDEP_SHIFT  17U

#define EQOS_MAC_HFR3_GCLWID_MASK   0x3U
#define EQOS_MAC_HFR3_GCLWID_SHIFT  20U

#define EQOS_MAC_HFR3_FPESEL_MASK   0x1U
#define EQOS_MAC_HFR3_FPESEL_SHIFT  26U

#define EQOS_MAC_HFR3_TBSSEL_MASK   0x1U
#define EQOS_MAC_HFR3_TBSSEL_SHIFT  27U

#define EQOS_MAC_HFR3_ASP_MASK   0x3U
#define EQOS_MAC_HFR3_ASP_SHIFT  28U
/** @} */

/**
 * @addtogroup EQOS-MAC EQOS MAC HW supported features
 *
 * @brief Helps in identifying the features that are set in MAC HW
 * @{
 */
#define EQOS_MAC_HFR0  0x11C
#define EQOS_MAC_HFR1  0x120
#define EQOS_MAC_HFR2  0x124
#define EQOS_MAC_HFR3  0x128
/** @} */

#ifdef HSI_SUPPORT

/**
 * @addtogroup EQOS-HSI
 *
 * @brief EQOS HSI related registers and bitmap
 * @{
 */
#define EQOS_MTL_ECC_INTERRUPT_ENABLE      0xCC8U
#define EQOS_MTL_TXCEIE                    OSI_BIT(0)
#define EQOS_MTL_RXCEIE                    OSI_BIT(4)
#define EQOS_MTL_ECEIE                     OSI_BIT(8)
#define EQOS_MTL_RPCEIE                    OSI_BIT(12)
#define EQOS_DMA_ECC_INTERRUPT_ENABLE      0x1084U
#define EQOS_DMA_TCEIE                     OSI_BIT(0)
#define EQOS_DMA_DCEIE                     OSI_BIT(1)
#define EQOS_MTL_ECC_INTERRUPT_STATUS      0xCCCU
#define EQOS_DMA_ECC_INTERRUPT_STATUS      0x1088U
#define EQOS_MTL_ECC_CONTROL               0xCC0U
#define EQOS_MTL_ECC_MTXEE                 OSI_BIT(0)
#define EQOS_MTL_ECC_MRXEE                 OSI_BIT(1)
#define EQOS_MTL_ECC_MESTEE                OSI_BIT(2)
#define EQOS_MTL_ECC_MRXPEE                OSI_BIT(3)
#define EQOS_MTL_ECC_TSOEE                 OSI_BIT(4)
#define EQOS_MTL_ECC_DSCEE                 OSI_BIT(5)
#define EQOS_MAC_FSM_ACT_TIMER             0x014CU
#define EQOS_LTMRMD_SHIFT                  20U
#define EQOS_LTMRMD_MASK                   0xF00000U
#define EQOS_NTMRMD_SHIFT                  16U
#define EQOS_NTMRMD_MASK                   0xF0000U
#define EQOS_TMR_SHIFT                     0U
#define EQOS_TMR_MASK                      0x3FFU
#define EQOS_MAC_FSM_CONTROL               0x148U
#define EQOS_PRTYEN                        OSI_BIT(1)
#define EQOS_TMOUTEN                       OSI_BIT(0)
#define EQOS_MAC_DPP_FSM_INTERRUPT_STATUS  0x140U
#define EQOS_MTL_DPP_CONTROL               0xCE0U
#define EQOS_EDPP                          OSI_BIT(0)
#define EQOS_OPE                           OSI_BIT(1)
#define EQOS_MTL_DBG_CTL                   0xC08U
#define EQOS_MTL_DBG_CTL_EIEC              OSI_BIT(18)
#define EQOS_MTL_DBG_CTL_EIEE              OSI_BIT(16)
#define EQOS_MTL_DPP_ECC_EIC               0xCE4U
#define EQOS_MTL_DPP_ECC_EIC_BLEI          OSI_BIT(0)
/** @} */
#endif

#endif /* INCLUDED_EQOS_CORE_H */
