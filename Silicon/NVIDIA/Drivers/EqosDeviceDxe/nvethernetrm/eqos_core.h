/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef EQOS_CORE_H_
#define EQOS_CORE_H_

/**
 * @addtogroup EQOS-FC Flow Control Threshold Macros
 *
 * @brief These bits control the threshold (fill-level of Rx queue) at which
 * the flow control is asserted or de-asserted
 * @{
 */
#define FULL_MINUS_1_5K		(unsigned int)1
#define FULL_MINUS_2_K		(unsigned int)2
#define FULL_MINUS_2_5K		(unsigned int)3
#define FULL_MINUS_3_K		(unsigned int)4
#define FULL_MINUS_4_K		(unsigned int)6
#define FULL_MINUS_6_K		(unsigned int)10
#define FULL_MINUS_10_K		(unsigned int)18
#define FULL_MINUS_16_K		(unsigned int)30
/** @} */

/**
 * @addtogroup EQOS-MTLQ MTL queue operation mode
 *
 * @brief MTL queue operation mode options
 * @{
 */
#define EQOS_MTL_QUEUE_DISABLED	0x0U
#define EQOS_MTL_QUEUE_AVB	0x1U
#define EQOS_MTL_QUEUE_DCB	0x2U
#define EQOS_MTL_QUEUE_GENERIC	0x3U
/** @} */

/**
 * @addtogroup EQOS-MDC MDC Clock Selection defines
 *
 * @brief MDC Clock defines
 * @{
 */
#define EQOS_CSR_60_100M	0x0	/* MDC = clk_csr/42 */
#define EQOS_CSR_100_150M	0x1	/* MDC = clk_csr/62 */
#define EQOS_CSR_20_35M		0x2	/* MDC = clk_csr/16 */
#define EQOS_CSR_35_60M		0x3	/* MDC = clk_csr/26 */
#define EQOS_CSR_150_250M	0x4	/* MDC = clk_csr/102 */
#define EQOS_CSR_250_300M	0x5	/* MDC = clk_csr/124 */
#define EQOS_CSR_300_500M	0x6	/* MDC = clk_csr/204 */
#define EQOS_CSR_500_800M	0x7	/* MDC = clk_csr/324 */
/** @} */

/**
 * @addtogroup EQOS-SIZE SIZE calculation helper Macros
 *
 * @brief SIZE calculation defines
 * @{
 */
#define FIFO_SIZE_B(x) (x)
#define FIFO_SIZE_KB(x) ((x) * 1024U)
/** @} */

/**
 * @addtogroup EQOS-QUEUE QUEUE fifo size programmable values
 *
 * @brief Queue FIFO size programmable values
 * @{
 */
#define EQOS_256	0x00U
#define EQOS_512	0x01U
#define EQOS_1K		0x03U
#define EQOS_2K		0x07U
#define EQOS_4K		0x0FU
#define EQOS_8K		0x1FU
#define EQOS_9K		0x23U
#define EQOS_16K	0x3FU
#define EQOS_32K	0x7FU
#define EQOS_36K	0x8FU
/** @} */

/**
 * @addtogroup EQOS-HW Hardware Register offsets
 *
 * @brief EQOS HW register offsets
 * @{
 */
#define EQOS_5_00_MAC_ARPPA	0x0210
#define EQOS_4_10_MAC_ARPPA	0x0AE0
#define EQOS_DMA_SBUS		0x1004
#define EQOS_DMA_BMR		0x1000
#define EQOS_MMC_CNTRL		0x0700
#define EQOS_MAC_MA0HR		0x0300
#define EQOS_MAC_MA0LR		0x0304
#define EQOS_MAC_MCR		0x0000
#define EQOS_MAC_VLAN_TAG	0x0050
#define EQOS_MAC_VLANTIR	0x0060
#define EQOS_MAC_IMR		0x00B4
#define EQOS_DMA_ISR		0x1008
#define EQOS_MAC_ISR		0x00B0
#define EQOS_MAC_RQC1R		0x00A4
#define EQOS_MAC_RQC2R		0x00A8
#define EQOS_MMC_TX_INTR_MASK	0x0710
#define EQOS_MMC_RX_INTR_MASK	0x070C
#define EQOS_MMC_IPC_RX_INTR_MASK	0x0800
#define EQOS_MAC_RQC0R		0x00A0
#define EQOS_MAC_PMTCSR		0x00C0
#define EQOS_MAC_PCS		0x00F8
#define EQOS_MAC_ANS		0x00E4
/* Enable for DA-based or L2/L4 based  DMA Channel selection*/
#define EQOS_RXQ_TO_DMA_CHAN_MAP	0x03020100U
#define EQOS_RXQ_TO_DMA_CHAN_MAP_DCS_EN	0x13121110U
#define EQOS_MAC_EXTR		0x0004
#define EQOS_MAC_RX_FLW_CTRL	0x0090
#define EQOS_MAC_QX_TX_FLW_CTRL(x)	((0x0004U * (x)) + 0x0070U)
/*MAC Filtering*/
#define EQOS_MAC_HTR_REG(x)		((0x0004U * (x)) + 0x0010U)
/* x will be index from 0 to 127 to write*/
#define EQOS_MAC_ADDRH(x)		((0x0008U * (x)) + 0x0300U)
#define EQOS_MAC_ADDRL(x)		((0x0008U * (x)) + 0x0304U)
#define EQOS_MAC_L3L4_CTR(x)		((0x0030U * (x)) + 0x0900U)
#define EQOS_MAC_L4_ADR(x)		((0x0030U * (x)) + 0x0904U)
#define EQOS_MAC_L3_AD0R(x)		((0x0030U * (x)) + 0x0910U)
#define EQOS_MAC_L3_AD1R(x)		((0x0030U * (x)) + 0x0914U)
#define EQOS_MAC_L3_AD2R(x)		((0x0030U * (x)) + 0x0918U)
#define EQOS_MAC_L3_AD3R(x)		((0x0030U * (x)) + 0x091CU)
#define EQOS_MAC_PFR			0x0008U
#define EQOS_MAC_TCR			0x0B00
#define EQOS_MAC_SSIR			0x0B04
#define EQOS_MAC_STSR			0x0B08
#define EQOS_MAC_STNSR			0x0B0C
#define EQOS_MAC_STSUR			0x0B10
#define EQOS_MAC_STNSUR			0x0B14
#define EQOS_MAC_TAR			0x0B18
/** @} */

/**
 * @addtogroup EQOS-MTL MTL HW Register offsets
 *
 * @brief EQOS MTL HW Register offsets
 * @{
 */
#define EQOS_MTL_CHX_TX_OP_MODE(x)	((0x0040U * (x)) + 0x0D00U)
#define EQOS_MTL_TXQ_QW(x)		((0x0040U * (x)) + 0x0D18U)
#define EQOS_MTL_CHX_RX_OP_MODE(x)	((0x0040U * (x)) + 0x0D30U)
#define EQOS_MTL_TXQ_ETS_CR(x)		((0x0040U * (x)) + 0x0D10U)
#define EQOS_MTL_TXQ_ETS_SSCR(x)	((0x0040U * (x)) + 0x0D1CU)
#define EQOS_MTL_TXQ_ETS_HCR(x)		((0x0040U * (x)) + 0x0D20U)
#define EQOS_MTL_TXQ_ETS_LCR(x)		((0x0040U * (x)) + 0x0D24U)
#define EQOS_MTL_RXQ_DMA_MAP0		0x0C30
#define EQOS_MTL_OP_MODE		0x0C00
/** @} */

/**
 * @addtogroup EQOS-Wrapper EQOS Wrapper HW Register offsets
 *
 * @brief EQOS Wrapper register offsets
 * @{
 */
#define EQOS_PAD_AUTO_CAL_CFG		0x8804U
#define EQOS_PAD_AUTO_CAL_STAT		0x880CU
#define EQOS_PAD_CRTL			0x8800U
#define EQOS_CLOCK_CTRL_0		0x8000U
/** @} */

/**
 * @addtogroup HW Register BIT values
 *
 * @brief consists of corresponding EQOS MAC, MTL register bit values
 * @{
 */
#define EQOS_PAD_AUTO_CAL_CFG_ENABLE		OSI_BIT(29)
#define EQOS_PAD_AUTO_CAL_CFG_START		OSI_BIT(31)
#define EQOS_PAD_AUTO_CAL_STAT_ACTIVE		OSI_BIT(31)
#define EQOS_PAD_CRTL_E_INPUT_OR_E_PWRD		OSI_BIT(31)
#define EQOS_MCR_ARPEN				OSI_BIT(31)
#define EQOS_MCR_IPC				OSI_BIT(27)
#define EQOS_MMC_CNTRL_CNTRST			OSI_BIT(0)
#define EQOS_MMC_CNTRL_RSTONRD			OSI_BIT(2)
#define EQOS_MMC_CNTRL_CNTPRST			OSI_BIT(4)
#define EQOS_MMC_CNTRL_CNTPRSTLVL		OSI_BIT(5)
#define EQOS_MTL_QTOMR_FTQ			OSI_BIT(0)
#define EQOS_MTL_TSF				OSI_BIT(1)
#define EQOS_MTL_TXQEN				OSI_BIT(3)
#define EQOS_MTL_RSF				OSI_BIT(5)
#define EQOS_MCR_TE				OSI_BIT(0)
#define EQOS_MCR_RE				OSI_BIT(1)
#define EQOS_MCR_DM				OSI_BIT(13)
#define EQOS_MCR_FES				OSI_BIT(14)
#define EQOS_MCR_PS				OSI_BIT(15)
#define EQOS_MCR_JE				OSI_BIT(16)
#define EQOS_MCR_JD				OSI_BIT(17)
#define EQOS_MCR_ACS				OSI_BIT(20)
#define EQOS_MCR_CST				OSI_BIT(21)
#define EQOS_MCR_S2KP				OSI_BIT(22)
#define EQOS_IMR_RGSMIIIE			OSI_BIT(0)
#define EQOS_IMR_PCSLCHGIE			OSI_BIT(1)
#define EQOS_IMR_PCSANCIE			OSI_BIT(2)
#define EQOS_IMR_PMTIE				OSI_BIT(4)
#define EQOS_IMR_LPIIE				OSI_BIT(5)
#define EQOS_MAC_PCS_LNKSTS			OSI_BIT(19)
#define EQOS_MAC_PCS_LNKMOD			OSI_BIT(16)
#define EQOS_MAC_PCS_LNKSPEED			(OSI_BIT(17) | OSI_BIT(18))
#define EQOS_MAC_PCS_LNKSPEED_10		0x0U
#define EQOS_MAC_PCS_LNKSPEED_100		OSI_BIT(17)
#define EQOS_MAC_PCS_LNKSPEED_1000		OSI_BIT(18)
#define EQOS_MAC_VLANTIR_VLTI			OSI_BIT(20)
#define EQOS_MAC_VLANTR_EVLS_ALWAYS_STRIP	((unsigned int)0x3 << 21U)
#define EQOS_MAC_VLANTR_EVLRXS			OSI_BIT(24)
#define EQOS_MAC_VLANTR_DOVLTC			OSI_BIT(20)
#define EQOS_MAC_VLANTR_ERIVLT			OSI_BIT(27)
#define EQOS_MAC_VLANTIRR_VLTI			OSI_BIT(20)
#define EQOS_MAC_VLANTIRR_CSVL			OSI_BIT(19)
#define EQOS_DMA_SBUS_BLEN4			OSI_BIT(1)
#define EQOS_DMA_SBUS_BLEN8			OSI_BIT(2)
#define EQOS_DMA_SBUS_BLEN16			OSI_BIT(3)
#define EQOS_DMA_SBUS_EAME			OSI_BIT(11)
#define EQOS_DMA_BMR_SWR			OSI_BIT(0)
#define EQOS_DMA_BMR_DPSW			OSI_BIT(8)
#define EQOS_MAC_RQC1R_MCBCQ1			OSI_BIT(16)
#define EQOS_MAC_RQC1R_MCBCQEN			OSI_BIT(20)
#define EQOS_MTL_QTOMR_FTQ_LPOS			OSI_BIT(0)
#define EQOS_DMA_ISR_MACIS			OSI_BIT(17)
#define EQOS_MAC_ISR_RGSMIIS			OSI_BIT(0)
#define EQOS_MTL_TXQ_QW_ISCQW			OSI_BIT(4)
#define EQOS_DMA_SBUS_RD_OSR_LMT		0x001F0000U
#define EQOS_DMA_SBUS_WR_OSR_LMT		0x1F000000U
#define EQOS_MTL_TXQ_SIZE_SHIFT			16U
#define EQOS_MTL_RXQ_SIZE_SHIFT			20U
#define EQOS_MAC_ENABLE_LM			OSI_BIT(12)
#define EQOS_RX_CLK_SEL				OSI_BIT(8)
#define EQOS_MAX_TC				8U
#define EQOS_MTL_TXQ_ETS_CR_SLC_MASK		(OSI_BIT(6) | OSI_BIT(5) | \
						 OSI_BIT(4))
#define EQOS_MTL_TXQ_ETS_CR_CC			OSI_BIT(3)
#define EQOS_MTL_TXQ_ETS_CR_AVALG		OSI_BIT(2)
#define EQOS_MTL_TXQ_ETS_CR_CC_SHIFT		3U
#define EQOS_MTL_TXQ_ETS_CR_AVALG_SHIFT		2U
#define EQOS_MTL_TXQ_ETS_QW_ISCQW_MASK		0x000FFFFFU
#define EQOS_MTL_TXQ_ETS_SSCR_SSC_MASK		0x00003FFFU
#define EQOS_MTL_TXQ_ETS_HCR_HC_MASK		0x1FFFFFFFU
#define EQOS_MTL_TXQ_ETS_LCR_LC_MASK		0x1FFFFFFFU
#define EQOS_MTL_TXQEN_MASK			(OSI_BIT(3) | OSI_BIT(2))
#define EQOS_MTL_TXQEN_MASK_SHIFT		2U
#define EQOS_MTL_RXQ_OP_MODE_FEP		OSI_BIT(4)
#define EQOS_MTL_OP_MODE_DTXSTS			OSI_BIT(1)
#define EQOS_MAC_EXTR_DCRCC			OSI_BIT(16)
#define EQOS_MAC_QX_TX_FLW_CTRL_TFE		OSI_BIT(1)
#define EQOS_MAC_RX_FLW_CTRL_RFE		OSI_BIT(0)
#define EQOS_MAC_PAUSE_TIME			0xFFFF0000U
#define EQOS_MAC_PAUSE_TIME_MASK		0xFFFF0000U
#define EQOS_MTL_RXQ_OP_MODE_EHFC		OSI_BIT(7)
#define EQOS_MTL_RXQ_OP_MODE_RFA_SHIFT		8U
#define EQOS_MTL_RXQ_OP_MODE_RFA_MASK		0x00003F00U
#define EQOS_MTL_RXQ_OP_MODE_RFD_SHIFT		14U
#define EQOS_MTL_RXQ_OP_MODE_RFD_MASK		0x000FC000U
#define EQOS_MAC_PFR_PR				OSI_BIT(0)
#define EQOS_MAC_PFR_HUC			OSI_BIT(1)
#define EQOS_MAC_PFR_HMC			OSI_BIT(2)
#define EQOS_MAC_PFR_DAIF			OSI_BIT(3)
#define EQOS_MAC_PFR_PM				OSI_BIT(4)
#define EQOS_MAC_PFR_DBF			OSI_BIT(5)
#define EQOS_MAC_PFR_PCF			(OSI_BIT(6) | OSI_BIT(7))
#define EQOS_MAC_PFR_SAIF			OSI_BIT(8)
#define EQOS_MAC_PFR_SAF			OSI_BIT(9)
#define EQOS_MAC_PFR_HPF			OSI_BIT(10)
#define EQOS_MAC_PFR_VTFE			OSI_BIT(16)
#define EQOS_MAC_PFR_SHIFT			16
#define EQOS_MAC_PFR_IPFE			OSI_BIT(20)
#define EQOS_MAC_PFR_DNTU			OSI_BIT(21)
#define EQOS_MAC_PFR_RA				OSI_BIT(31)
#define EQOS_MAC_PFR_HUC_SHIFT			1
#define EQOS_MAC_PFR_HMC_SHIFT			2
#define EQOS_MAC_PFR_DAIF_SHIFT			3
#define EQOS_MAC_PFR_PM_SHIFT			4
#define EQOS_MAC_PFR_HPF_SHIFT			10
#define EQOS_MAC_L4_SP_MASK			0x0000FFFFU
#define EQOS_MAC_L4_DP_MASK			0xFFFF0000U
#define EQOS_MAC_L4_DP_SHIFT			16
#define EQOS_MAC_L3L4_CTR_L4SPM0		OSI_BIT(18)
#define EQOS_MAC_L3L4_CTR_L4SPIM0		OSI_BIT(19)
#define EQOS_MAC_L3L4_CTR_L4SPI_SHIFT		19
#define EQOS_MAC_L3L4_CTR_L4DPM0		OSI_BIT(20)
#define EQOS_MAC_L3L4_CTR_L4DPIM0		OSI_BIT(21)
#define EQOS_MAC_L3L4_CTR_L4DPI_SHIFT		21
#define EQOS_MAC_L3L4_CTR_L4PEN0		OSI_BIT(16)
#define EQOS_MAC_L3L4_CTR_L3PEN0		OSI_BIT(0)
#define EQOS_MAC_L3L4_CTR_L3SAM0		OSI_BIT(2)
#define EQOS_MAC_L3L4_CTR_L3SAIM0		OSI_BIT(3)
#define EQOS_MAC_L3L4_CTR_L3SAI_SHIFT		3
#define EQOS_MAC_L3L4_CTR_L3DAM0		OSI_BIT(4)
#define EQOS_MAC_L3L4_CTR_L3DAIM0		OSI_BIT(5)
#define EQOS_MAC_L3L4_CTR_L3DAI_SHIFT		5
#define EQOS_MAC_L3L4_CTR_L3HSBM0		(OSI_BIT(6) | OSI_BIT(7) | \
						 OSI_BIT(8) | OSI_BIT(9) | \
						 OSI_BIT(10))
#define EQOS_MAC_L3L4_CTR_L3HDBM0		(OSI_BIT(11) | OSI_BIT(12) | \
						 OSI_BIT(13) | OSI_BIT(14) | \
						 OSI_BIT(15))
#define EQOS_MAC_L3L4_CTR_DMCHEN0		OSI_BIT(28)
#define EQOS_MAC_L3L4_CTR_DMCHEN0_SHIFT		28
#define EQOS_MAC_L3L4_CTR_DMCHN0		(OSI_BIT(24) | OSI_BIT(25) | \
						 OSI_BIT(26) | OSI_BIT(27))
#define EQOS_MAC_L3L4_CTR_DMCHN0_SHIFT		24
#define EQOS_MAC_L3_IP6_CTRL_CLEAR		(EQOS_MAC_L3L4_CTR_L3SAM0  | \
						 EQOS_MAC_L3L4_CTR_L3SAIM0 | \
						 EQOS_MAC_L3L4_CTR_L3DAM0  | \
						 EQOS_MAC_L3L4_CTR_L3DAIM0 | \
						 EQOS_MAC_L3L4_CTR_DMCHEN0 | \
						 EQOS_MAC_L3L4_CTR_DMCHN0)
#define EQOS_MAC_L3_IP4_SA_CTRL_CLEAR		(EQOS_MAC_L3L4_CTR_L3SAM0  | \
						 EQOS_MAC_L3L4_CTR_L3SAIM0 | \
						 EQOS_MAC_L3L4_CTR_DMCHEN0 | \
						 EQOS_MAC_L3L4_CTR_DMCHN0)
#define EQOS_MAC_L3_IP4_DA_CTRL_CLEAR		(EQOS_MAC_L3L4_CTR_L3DAM0  | \
						 EQOS_MAC_L3L4_CTR_L3DAIM0 | \
						 EQOS_MAC_L3L4_CTR_DMCHEN0 | \
						 EQOS_MAC_L3L4_CTR_DMCHN0)
#define EQOS_MAC_L4_SP_CTRL_CLEAR		(EQOS_MAC_L3L4_CTR_L4SPM0  | \
						 EQOS_MAC_L3L4_CTR_L4SPIM0 | \
						 EQOS_MAC_L3L4_CTR_DMCHEN0 | \
						 EQOS_MAC_L3L4_CTR_DMCHN0)
#define EQOS_MAC_L4_DP_CTRL_CLEAR		(EQOS_MAC_L3L4_CTR_L4DPM0  | \
						 EQOS_MAC_L3L4_CTR_L4DPIM0 | \
						 EQOS_MAC_L3L4_CTR_DMCHEN0 | \
						 EQOS_MAC_L3L4_CTR_DMCHN0)
#define EQOS_MAC_ADDRH_DCS			(OSI_BIT(23) | OSI_BIT(22) | \
						 OSI_BIT(21) | OSI_BIT(20) | \
						 OSI_BIT(19) | OSI_BIT(18) | \
						 OSI_BIT(17) | OSI_BIT(16))
#define EQOS_MAC_ADDRH_DCS_SHIFT		16
#define EQOS_MAC_ADDRH_MBC			(OSI_BIT(29) | OSI_BIT(28) | \
						 OSI_BIT(27) | OSI_BIT(26) | \
						 OSI_BIT(25) | OSI_BIT(24))
#define EQOS_MAC_ADDRH_MBC_SHIFT		24
#define EQOS_MAX_MASK_BYTE			0x3FU
#define EQOS_MAX_MAC_ADDR_REG			32U
#define EQOS_MAC_ADDRH_SA			OSI_BIT(30)
#define EQOS_MAC_ADDRH_SA_SHIFT			30
#define EQOS_MAC_VLAN_TR			0x0050U
#define EQOS_MAC_VLAN_TFR			0x0054U
#define EQOS_MAC_VLAN_HTR			0x0058U
#define EQOS_MAC_VLAN_TR_ETV			OSI_BIT(16)
#define EQOS_MAC_VLAN_TR_VTIM			OSI_BIT(17)
#define EQOS_MAC_VLAN_TR_VTIM_SHIFT		17
#define EQOS_MAC_VLAN_TR_VTHM			OSI_BIT(25)
#define EQOS_MAC_VLAN_TR_VL			0xFFFFU
#define EQOS_MAC_VLAN_HTR_VLHT			0xFFFFU
#define EQOS_MAC_RQC2_PSRQ_MASK			((unsigned int)0xFF)
#define EQOS_MAC_RQC2_PSRQ_SHIFT		8U
#define EQOS_MAC_VLAN_TR_ETV_SHIFT		16U
#define EQOS_MAC_MAX_HTR_REG_LEN		8U
#define EQOS_MAC_TCR_TSADDREG			OSI_BIT(5)
#define EQOS_MAC_TCR_TSINIT			OSI_BIT(2)
#define EQOS_MAC_TCR_TSUPDT			OSI_BIT(3)
#define EQOS_MAC_STNSUR_ADDSUB_SHIFT		31U
#define EQOS_MAC_TCR_TSCFUPDT			OSI_BIT(1)
#define EQOS_MAC_TCR_TSCTRLSSR			OSI_BIT(9)
#define EQOS_MAC_SSIR_SSINC_SHIFT		16U
#define EQOS_MAC_STNSR_TSSS_MASK		0x7FFFFFFFU

#define EQOS_DMA_CHX_STATUS_TPS			OSI_BIT(1)
#define EQOS_DMA_CHX_STATUS_TBU			OSI_BIT(2)
#define EQOS_DMA_CHX_STATUS_RBU			OSI_BIT(7)
#define EQOS_DMA_CHX_STATUS_RPS			OSI_BIT(8)
#define EQOS_DMA_CHX_STATUS_RWT			OSI_BIT(9)
#define EQOS_DMA_CHX_STATUS_FBE			OSI_BIT(10)
/** @} */

/**
 * @brief update_ehfc_rfa_rfd - Update EHFC, RFD and RSA values
 *
 * Algorithm: Calculates and stores the RSD (Threshold for Deactivating
 *	  Flow control) and RSA (Threshold for Activating Flow Control) values
 *	  based on the Rx FIFO size and also enables HW flow control
 *
 * @param[in] rx_fifo: Rx FIFO size.
 * @param[in] value: Stores RFD and RSA values
 */
void update_ehfc_rfa_rfd(unsigned int rx_fifo, unsigned int *value);

/**
 * @addtogroup EQOS-Safety-Register EQOS Safety Register Mask
 *
 * @brief EQOS HW register masks and index
 * @{
 */
#define EQOS_MAC_MCR_MASK			0xFFFFFF7FU
#define EQOS_MAC_PFR_MASK			0x803107FFU
#define EQOS_MAC_HTR_MASK			0xFFFFFFFFU
#define EQOS_MAC_QX_TXFC_MASK			0xFFFF00F2U
#define EQOS_MAC_RQC0R_MASK			0xFFU
#define EQOS_MAC_RQC1R_MASK			0xF77077U
#define EQOS_MAC_RQC2R_MASK			0xFFFFFFFFU
#define EQOS_MAC_IMR_MASK			0x47039U
#define EQOS_MAC_MA0HR_MASK			0xFFFFFU
#define EQOS_MAC_MA0LR_MASK			0xFFFFFFFFU
#define EQOS_MAC_TCR_MASK			0x1107FF03U
#define EQOS_MAC_SSIR_MASK			0xFFFF00U
#define EQOS_MAC_TAR_MASK			0xFFFFFFFFU
#define EQOS_RXQ_DMA_MAP0_MASK			0x13131313U

#define EQOS_MTL_TXQ_OP_MODE_MASK		0xFF007EU
#define EQOS_MTL_TXQ_QW_MASK			0x1FFFFFU
#define EQOS_MTL_RXQ_OP_MODE_MASK		0xFFFFFFBU
#define EQOS_PAD_AUTO_CAL_CFG_MASK		0x7FFFFFFFU
#define EQOS_DMA_SBUS_MASK			0xDF1F3CFFU

/* To add new registers to validate,append at end of this list and increment
 * EQOS_MAX_CORE_SAFETY_REGS.
 * Using macro instead of enum due to misra error.
 */
#define EQOS_MAC_MCR_IDX			0U
#define EQOS_MAC_PFR_IDX			1U
#define EQOS_MAC_HTR0_IDX			2U
#define EQOS_MAC_HTR1_IDX			3U
#define EQOS_MAC_HTR2_IDX			4U
#define EQOS_MAC_HTR3_IDX			5U
#define EQOS_MAC_Q0_TXFC_IDX			6U
#define EQOS_MAC_RQC0R_IDX			7U
#define EQOS_MAC_RQC1R_IDX			8U
#define EQOS_MAC_RQC2R_IDX			9U
#define EQOS_MAC_IMR_IDX			10U
#define EQOS_MAC_MA0HR_IDX			11U
#define EQOS_MAC_MA0LR_IDX			12U
#define EQOS_MAC_TCR_IDX			13U
#define EQOS_MAC_SSIR_IDX			14U
#define EQOS_MAC_TAR_IDX			15U
#define EQOS_PAD_AUTO_CAL_CFG_IDX		16U
#define EQOS_MTL_RXQ_DMA_MAP0_IDX		17U
#define EQOS_MTL_CH0_TX_OP_MODE_IDX		18U
#define EQOS_MTL_CH1_TX_OP_MODE_IDX		19U
#define EQOS_MTL_CH2_TX_OP_MODE_IDX		20U
#define EQOS_MTL_CH3_TX_OP_MODE_IDX		21U
#define EQOS_MTL_TXQ0_QW_IDX			22U
#define EQOS_MTL_TXQ1_QW_IDX			23U
#define EQOS_MTL_TXQ2_QW_IDX			24U
#define EQOS_MTL_TXQ3_QW_IDX			25U
#define EQOS_MTL_CH0_RX_OP_MODE_IDX		26U
#define EQOS_MTL_CH1_RX_OP_MODE_IDX		27U
#define EQOS_MTL_CH2_RX_OP_MODE_IDX		28U
#define EQOS_MTL_CH3_RX_OP_MODE_IDX		29U
#define EQOS_DMA_SBUS_IDX			30U
#define EQOS_MAX_CORE_SAFETY_REGS		31U
/** @} */

/**
 * @brief core_func_safety - Struct used to store last written values of
 *	critical core HW registers.
 */
struct core_func_safety {
	/** Array of reg MMIO addresses (base of EQoS + offset of reg) */
	void *reg_addr[EQOS_MAX_CORE_SAFETY_REGS];
	/** Array of bit-mask value of each corresponding reg
	 * (used to ignore self-clearing/reserved bits in reg) */
	unsigned int reg_mask[EQOS_MAX_CORE_SAFETY_REGS];
	/** Array of value stored in each corresponding register */
	unsigned int reg_val[EQOS_MAX_CORE_SAFETY_REGS];
	/** OSI lock variable used to protect writes to reg while
	 * validation is in-progress */
	unsigned int core_safety_lock;
};
#endif
