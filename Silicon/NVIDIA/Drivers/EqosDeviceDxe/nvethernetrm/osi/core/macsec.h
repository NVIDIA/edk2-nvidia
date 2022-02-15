/*
 * Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INCLUDED_MACSEC_H
#define INCLUDED_MACSEC_H

/**
 * @addtogroup MACsec AMAP
 *
 * @brief MACsec controller register offsets
 * @{
 */
#define MACSEC_GCM_KEYTABLE_CONFIG		0x0000
#define MACSEC_GCM_KEYTABLE_DATA(x)		(0x0004 + (x * 4))
#define MACSEC_RX_ICV_ERR_CNTRL 		0x4000
#define MACSEC_INTERRUPT_COMMON_SR		0x4004
#define MACSEC_TX_IMR				0x4008
#define MACSEC_TX_ISR				0x400C
#define MACSEC_RX_IMR				0x4048
#define MACSEC_RX_ISR				0x404C
#define MACSEC_INTERRUPT_MASK1_0		0x40A0
#define MACSEC_TX_SC_PN_THRESHOLD_STATUS0_0	0x4018
#define MACSEC_TX_SC_PN_THRESHOLD_STATUS1_0	0x401C
#define MACSEC_TX_SC_PN_EXHAUSTED_STATUS0_0	0x4024
#define MACSEC_TX_SC_PN_EXHAUSTED_STATUS1_0	0x4028
#define MACSEC_TX_SC_ERROR_INTERRUPT_STATUS_0	0x402C
#define MACSEC_RX_SC_PN_EXHAUSTED_STATUS0_0	0x405C
#define MACSEC_RX_SC_PN_EXHAUSTED_STATUS1_0	0x4060
#define MACSEC_RX_SC_REPLAY_ERROR_STATUS0_0	0x4090
#define MACSEC_RX_SC_REPLAY_ERROR_STATUS1_0	0x4094
#define MACSEC_STATS_CONFIG			0x9000
#define MACSEC_STATS_CONTROL_0			0x900C
#define MACSEC_TX_PKTS_UNTG_LO_0		0x9010
#define MACSEC_TX_PKTS_UNTG_HI_0		0x9014
#define MACSEC_TX_OCTETS_PRTCTD_LO_0		0x9018
#define MACSEC_TX_OCTETS_PRTCTD_HI_0		0x901C
#define MACSEC_TX_PKTS_TOO_LONG_LO_0		0x9020
#define MACSEC_TX_PKTS_TOO_LONG_HI_0		0x9024
#define MACSEC_TX_PKTS_PROTECTED_SCx_LO_0(x)	(0x9028 + (x * 8))
#define MACSEC_TX_PKTS_PROTECTED_SCx_HI_0(x)	(0x902C  + (x * 8))
#define MACSEC_RX_PKTS_NOTG_LO_0		0x90B0
#define MACSEC_RX_PKTS_NOTG_HI_0		0x90B4
#define MACSEC_RX_PKTS_UNTG_LO_0		0x90A8
#define MACSEC_RX_PKTS_UNTG_HI_0		0x90AC
#define MACSEC_RX_PKTS_BADTAG_LO_0		0x90B8
#define MACSEC_RX_PKTS_BADTAG_HI_0		0x90BC
#define MACSEC_RX_PKTS_NOSA_LO_0		0x90C0
#define MACSEC_RX_PKTS_NOSA_HI_0		0x90C4
#define MACSEC_RX_PKTS_NOSAERROR_LO_0		0x90C8
#define MACSEC_RX_PKTS_NOSAERROR_HI_0		0x90CC
#define MACSEC_RX_PKTS_OVRRUN_LO_0		0x90D0
#define MACSEC_RX_PKTS_OVRRUN_HI_0		0x90D4
#define MACSEC_RX_OCTETS_VLDTD_LO_0		0x90D8
#define MACSEC_RX_OCTETS_VLDTD_HI_0		0x90DC
#define MACSEC_RX_PKTS_LATE_SCx_LO_0(x) 	(0x90E0 + (x * 8))
#define MACSEC_RX_PKTS_LATE_SCx_HI_0(x) 	(0x90E4 + (x * 8))
#define MACSEC_RX_PKTS_NOTVALID_SCx_LO_0(x)	(0x9160 + (x * 8))
#define MACSEC_RX_PKTS_NOTVALID_SCx_HI_0(x)	(0x9164 + (x * 8))
#define MACSEC_RX_PKTS_OK_SCx_LO_0(x)		(0x91E0 + (x * 8))
#define MACSEC_RX_PKTS_OK_SCx_HI_0(x)		(0x91E4 + (x * 8))

#define MACSEC_TX_INPKTS_CRCIN_NOTVALID_LO_0	0x9260
#define MACSEC_TX_INPKTS_CRCIN_NOTVALID_HI_0	0x9264
#define MACSEC_RX_INPKTS_CRCIN_NOTVALID_LO_0	0x9268
#define MACSEC_RX_INPKTS_CRCIN_NOTVALID_HI_0	0x926C

#define MACSEC_CONTROL0 		0xD000
#define MACSEC_LUT_CONFIG		0xD004
#define MACSEC_LUT_DATA(x)		(0xD008 + (x * 4))
#define MACSEC_TX_BYP_LUT_VALID 	0xD024
#define MACSEC_TX_SCI_LUT_VALID 	0xD028
#define MACSEC_RX_BYP_LUT_VALID 	0xD02C
#define MACSEC_RX_SCI_LUT_VALID 	0xD030

#define MACSEC_COMMON_IMR		0xD054
#define MACSEC_COMMON_ISR		0xD058
#define MACSEC_TX_SC_KEY_INVALID_STS0_0	0xD064
#define MACSEC_TX_SC_KEY_INVALID_STS1_0	0xD068
#define MACSEC_RX_SC_KEY_INVALID_STS0_0	0xD080
#define MACSEC_RX_SC_KEY_INVALID_STS1_0	0xD084

#define MACSEC_TX_DEBUG_CONTROL_0	0xD098
#define MACSEC_TX_DEBUG_TRIGGER_EN_0	0xD09C
#define MACSEC_TX_DEBUG_STATUS_0	0xD0C4
#define MACSEC_DEBUG_BUF_CONFIG_0	0xD0C8
#define MACSEC_DEBUG_BUF_DATA_0(x)	(0xD0CC + (x * 4))
#define MACSEC_RX_DEBUG_CONTROL_0	0xD0DC
#define MACSEC_RX_DEBUG_TRIGGER_EN_0	0xD0E0
#define MACSEC_RX_DEBUG_STATUS_0	0xD0F8

#define MACSEC_CONTROL1			0xE000
#define MACSEC_GCM_AES_CONTROL_0	0xE004
#define MACSEC_TX_MTU_LEN		0xE008
#define MACSEC_TX_SOT_DELAY		0xE010
#define MACSEC_RX_MTU_LEN		0xE014
#define MACSEC_RX_SOT_DELAY		0xE01C
#define MACSEC_TX_DVLAN_CONTROL_0	0xE00C
#define MACSEC_RX_DVLAN_CONTROL_0	0xE018
/** @} */

/**
 * @addtogroup MACSEC_GCM_KEYTABLE_CONFIG register
 *
 * @brief Bit definitions of MACSEC_GCM_KEYTABLE_CONFIG register
 * @{
 */
#define MACSEC_KT_CONFIG_UPDATE 	OSI_BIT(31)
#define MACSEC_KT_CONFIG_CTLR_SEL	OSI_BIT(25)
#define MACSEC_KT_CONFIG_RW		OSI_BIT(24)
#define MACSEC_KT_CONFIG_INDEX_MASK	(OSI_BIT(4) | OSI_BIT(3) | OSI_BIT(2) |\
					 OSI_BIT(1) | OSI_BIT(0))
#define MACSEC_KT_ENTRY_VALID		OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_GCM_KEYTABLE_DATA registers
 *
 * @brief Bit definitions of MACSEC_GCM_KEYTABLE_DATA register & helpful macros
 * @{
 */
#define MACSEC_KT_DATA_REG_CNT		13
#define MACSEC_KT_DATA_REG_SAK_CNT	8
#define MACSEC_KT_DATA_REG_H_CNT	4
/** @} */

/**
 * @addtogroup MACSEC_LUT_CONFIG register
 *
 * @brief Bit definitions of MACSEC_LUT_CONFIG register
 * @{
 */
#define MACSEC_LUT_CONFIG_UPDATE	OSI_BIT(31)
#define MACSEC_LUT_CONFIG_CTLR_SEL	OSI_BIT(25)
#define MACSEC_LUT_CONFIG_RW		OSI_BIT(24)
#define MACSEC_LUT_CONFIG_LUT_SEL_MASK	(OSI_BIT(18) | OSI_BIT(17) |\
					 OSI_BIT(16))
#define MACSEC_LUT_CONFIG_LUT_SEL_SHIFT	16
#define MACSEC_LUT_CONFIG_INDEX_MASK	(OSI_BIT(4) | OSI_BIT(3) | OSI_BIT(2) |\
					 OSI_BIT(1) | OSI_BIT(0))
/** @} */
/**
 * @addtogroup INTERRUPT_COMMON_STATUS register
 *
 * @brief Bit definitions of MACSEC_INTERRUPT_COMMON_STATUS register
 * @{
 */
#define MACSEC_COMMON_SR_SFTY_ERR		OSI_BIT(2)
#define MACSEC_COMMON_SR_RX			OSI_BIT(1)
#define MACSEC_COMMON_SR_TX			OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_CONTROL0 register
 *
 * @brief Bit definitions of MACSEC_CONTROL0 register
 * @{
 */
#define MACSEC_TX_LKUP_MISS_NS_INTR		OSI_BIT(24)
#define MACSEC_RX_LKUP_MISS_NS_INTR		OSI_BIT(23)
#define MACSEC_VALIDATE_FRAMES_MASK		(OSI_BIT(22) | OSI_BIT(21))
#define MACSEC_VALIDATE_FRAMES_DIS		0x0
#define MACSEC_VALIDATE_FRAMES_STRICT		OSI_BIT(22)
#define MACSEC_VALIDATE_FRAMES_CHECK		OSI_BIT(21)
#define MACSEC_RX_REPLAY_PROT_EN		OSI_BIT(20)
#define MACSEC_RX_LKUP_MISS_BYPASS		OSI_BIT(19)
#define MACSEC_RX_EN				OSI_BIT(16)
#define MACSEC_TX_LKUP_MISS_BYPASS		OSI_BIT(3)
#define MACSEC_TX_EN				OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_CONTROL1 register
 *
 * @brief Bit definitions of MACSEC_CONTROL1 register
 * @{
 */
#define MACSEC_LOOPBACK_MODE_EN 		OSI_BIT(31)
#define MACSEC_RX_MTU_CHECK_EN			OSI_BIT(16)
#define MACSEC_TX_LUT_PRIO_BYP			OSI_BIT(2)
#define MACSEC_TX_MTU_CHECK_EN			OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_GCM_AES_CONTROL_0 register
 *
 * @brief Bit definitions of MACSEC_GCM_AES_CONTROL_0 register
 * @{
 */
#define MACSEC_RX_AES_MODE_MASK 	(OSI_BIT(17) | OSI_BIT(16))
#define MACSEC_RX_AES_MODE_AES128	0x0
#define MACSEC_RX_AES_MODE_AES256	OSI_BIT(17)
#define MACSEC_TX_AES_MODE_MASK 	(OSI_BIT(1) | OSI_BIT(0))
#define MACSEC_TX_AES_MODE_AES128	0x0
#define MACSEC_TX_AES_MODE_AES256	OSI_BIT(1)
/** @} */

/**
 * @addtogroup MACSEC_COMMON_IMR register
 *
 * @brief Bit definitions of MACSEC_INTERRUPT_MASK register
 * @{
 */
#define MACSEC_SECURE_REG_VIOL_INT_EN		OSI_BIT(31)
#define MACSEC_RX_UNINIT_KEY_SLOT_INT_EN	OSI_BIT(17)
#define MACSEC_RX_LKUP_MISS_INT_EN		OSI_BIT(16)
#define MACSEC_TX_UNINIT_KEY_SLOT_INT_EN	OSI_BIT(1)
#define MACSEC_TX_LKUP_MISS_INT_EN		OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_TX_IMR register
 *
 * @brief Bit definitions of TX_INTERRUPT_MASK register
 * @{
 */
#define MACSEC_TX_DBG_BUF_CAPTURE_DONE_INT_EN	OSI_BIT(22)
#define MACSEC_TX_MTU_CHECK_FAIL_INT_EN 	OSI_BIT(19)
#define MACSEC_TX_AES_GCM_BUF_OVF_INT_EN	OSI_BIT(18)
#define MACSEC_TX_SC_AN_NOT_VALID_INT_EN	OSI_BIT(17)
#define MACSEC_TX_MAC_CRC_ERROR_INT_EN		OSI_BIT(16)
#define MACSEC_TX_PN_EXHAUSTED_INT_EN		OSI_BIT(1)
#define MACSEC_TX_PN_THRSHLD_RCHD_INT_EN	OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_RX_IMR register
 *
 * @brief Bit definitions of RX_INTERRUPT_MASK register
 * @{
 */
#define MACSEC_RX_DBG_BUF_CAPTURE_DONE_INT_EN	OSI_BIT(22)
#define MACSEC_RX_ICV_ERROR_INT_EN		OSI_BIT(21)
#define RX_REPLAY_ERROR_INT_EN  		OSI_BIT(20)
#define MACSEC_RX_MTU_CHECK_FAIL_INT_EN 	OSI_BIT(19)
#define MACSEC_RX_AES_GCM_BUF_OVF_INT_EN	OSI_BIT(18)
#define MACSEC_RX_MAC_CRC_ERROR_INT_EN		OSI_BIT(16)
#define MACSEC_RX_PN_EXHAUSTED_INT_EN		OSI_BIT(1)
/** @} */

/**
 * @addtogroup MACSEC_INTERRUPT_MASK1_0 register
 *
 * @brief Bit definitions of MACSEC_INTERRUPT_MASK1_0 register
 * @{
 */
#define MACSEC_SFTY_ERR_UNCORR_INT_EN		OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_COMMON_ISR register
 *
 * @brief Bit definitions of MACSEC_INTERRUPT_STATUS register
 * @{
 */
#define MACSEC_SECURE_REG_VIOL		OSI_BIT(31)
#define MACSEC_RX_UNINIT_KEY_SLOT	OSI_BIT(17)
#define MACSEC_RX_LKUP_MISS		OSI_BIT(16)
#define MACSEC_TX_UNINIT_KEY_SLOT	OSI_BIT(1)
#define MACSEC_TX_LKUP_MISS		OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_TX_ISR register
 *
 * @brief Bit definitions of TX_INTERRUPT_STATUS register
 * @{
 */
#define MACSEC_TX_DBG_BUF_CAPTURE_DONE	OSI_BIT(22)
#define MACSEC_TX_MTU_CHECK_FAIL 	OSI_BIT(19)
#define MACSEC_TX_AES_GCM_BUF_OVF	OSI_BIT(18)
#define MACSEC_TX_SC_AN_NOT_VALID	OSI_BIT(17)
#define MACSEC_TX_MAC_CRC_ERROR 	OSI_BIT(16)
#define MACSEC_TX_PN_EXHAUSTED		OSI_BIT(1)
#define MACSEC_TX_PN_THRSHLD_RCHD	OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_RX_ISR register
 *
 * @brief Bit definitions of RX_INTERRUPT_STATUS register
 * @{
 */
#define MACSEC_RX_DBG_BUF_CAPTURE_DONE	OSI_BIT(22)
#define MACSEC_RX_ICV_ERROR		OSI_BIT(21)
#define MACSEC_RX_REPLAY_ERROR		OSI_BIT(20)
#define MACSEC_RX_MTU_CHECK_FAIL	OSI_BIT(19)
#define MACSEC_RX_AES_GCM_BUF_OVF	OSI_BIT(18)
#define MACSEC_RX_MAC_CRC_ERROR 	OSI_BIT(16)
#define MACSEC_RX_PN_EXHAUSTED		OSI_BIT(1)
/** @} */

/**
 * @addtogroup MACSEC_STATS_CONTROL_0 register
 *
 * @brief Bit definitions of MACSEC_STATS_CONTROL_0 register
 * @{
 */
#define MACSEC_STATS_CONTROL0_RD_CPY			OSI_BIT(3)
#define MACSEC_STATS_CONTROL0_TK_CPY			OSI_BIT(2)
#define MACSEC_STATS_CONTROL0_CNT_RL_OVR_CPY		OSI_BIT(1)
#define MACSEC_STATS_CONTROL0_CNT_CLR			OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_DEBUG_BUF_CONFIG_0 register
 *
 * @brief Bit definitions of MACSEC_DEBUG_BUF_CONFIG_0 register
 * @{
 */
#define MACSEC_DEBUG_BUF_CONFIG_0_UPDATE	OSI_BIT(31)
#define MACSEC_DEBUG_BUF_CONFIG_0_CTLR_SEL	OSI_BIT(25)
#define MACSEC_DEBUG_BUF_CONFIG_0_RW		OSI_BIT(24)
#define MACSEC_DEBUG_BUF_CONFIG_0_IDX_MASK	(OSI_BIT(0) | OSI_BIT(1) | \
						OSI_BIT(2) | OSI_BIT(3))
/** @} */

/**
 * @addtogroup MACSEC_TX_DEBUG_TRIGGER_EN_0 register
 *
 * @brief Bit definitions of MACSEC_TX_DEBUG_TRIGGER_EN_0 register
 * @{
 */
#define MACSEC_TX_DBG_CAPTURE		OSI_BIT(10)
#define MACSEC_TX_DBG_ICV_CORRUPT	OSI_BIT(9)
#define MACSEC_TX_DBG_CRC_CORRUPT	OSI_BIT(8)
#define MACSEC_TX_DBG_DATA_MATCH	OSI_BIT(7)
#define MACSEC_TX_DBG_LKUP_MATCH	OSI_BIT(6)
#define MACSEC_TX_DBG_CRCOUT_MATCH	OSI_BIT(5)
#define MACSEC_TX_DBG_CRCIN_MATCH	OSI_BIT(4)
#define MACSEC_TX_DBG_ICV_MATCH 	OSI_BIT(3)
#define MACSEC_TX_DBG_KEY_NOT_VALID	OSI_BIT(2)
#define MACSEC_TX_DBG_AN_NOT_VALID	OSI_BIT(1)
#define MACSEC_TX_DBG_LKUP_MISS 	OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_TX_DEBUG_STATUS_0 register
 *
 * @brief Bit definitions of MACSEC_TX_DEBUG_STATUS_0 register
 * @{
 */
#define MACSEC_TX_DBG_STS_CAPTURE		OSI_BIT(10)
#define MACSEC_TX_DBG_STS_ICV_CORRUPT		OSI_BIT(9)
#define MACSEC_TX_DBG_STS_CRC_CORRUPT		OSI_BIT(8)
#define MACSEC_TX_DBG_STS_DATA_MATCH		OSI_BIT(7)
#define MACSEC_TX_DBG_STS_LKUP_MATCH		OSI_BIT(6)
#define MACSEC_TX_DBG_STS_CRCOUT_MATCH		OSI_BIT(5)
#define MACSEC_TX_DBG_STS_CRCIN_MATCH		OSI_BIT(4)
#define MACSEC_TX_DBG_STS_ICV_MATCH		OSI_BIT(3)
#define MACSEC_TX_DBG_STS_KEY_NOT_VALID 	OSI_BIT(2)
#define MACSEC_TX_DBG_STS_AN_NOT_VALID		OSI_BIT(1)
#define MACSEC_TX_DBG_STS_LKUP_MISS		OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_RX_DEBUG_TRIGGER_EN_0 register
 *
 * @brief Bit definitions of MACSEC_RX_DEBUG_TRIGGER_EN_0 register
 * @{
 */
#define MACSEC_RX_DBG_CAPTURE		OSI_BIT(10)
#define MACSEC_RX_DBG_ICV_ERROR 	OSI_BIT(9)
#define MACSEC_RX_DBG_CRC_CORRUPT	OSI_BIT(8)
#define MACSEC_RX_DBG_DATA_MATCH	OSI_BIT(7)
#define MACSEC_RX_DBG_BYP_LKUP_MATCH	OSI_BIT(6)
#define MACSEC_RX_DBG_CRCOUT_MATCH	OSI_BIT(5)
#define MACSEC_RX_DBG_CRCIN_MATCH	OSI_BIT(4)
#define MACSEC_RX_DBG_REPLAY_ERR	OSI_BIT(3)
#define MACSEC_RX_DBG_KEY_NOT_VALID	OSI_BIT(2)
#define MACSEC_RX_DBG_LKUP_MISS 	OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_RX_DEBUG_STATUS_0 register
 *
 * @brief Bit definitions of MACSEC_RX_DEBUG_STATUS_0 register
 * @{
 */
#define MACSEC_RX_DBG_STS_CAPTURE		OSI_BIT(10)
#define MACSEC_RX_DBG_STS_ICV_ERROR		OSI_BIT(9)
#define MACSEC_RX_DBG_STS_CRC_CORRUPT		OSI_BIT(8)
#define MACSEC_RX_DBG_STS_DATA_MATCH		OSI_BIT(7)
#define MACSEC_RX_DBG_STS_BYP_LKUP_MATCH	OSI_BIT(6)
#define MACSEC_RX_DBG_STS_CRCOUT_MATCH		OSI_BIT(5)
#define MACSEC_RX_DBG_STS_CRCIN_MATCH		OSI_BIT(4)
#define MACSEC_RX_DBG_STS_REPLAY_ERR		OSI_BIT(3)
#define MACSEC_RX_DBG_STS_KEY_NOT_VALID 	OSI_BIT(2)
#define MACSEC_RX_DBG_STS_LKUP_MISS		OSI_BIT(0)
/** @} */

/**
 * @addtogroup MACSEC_TX_DEBUG_CONTROL_0 register
 *
 * @brief Bit definitions of MACSEC_TX_DEBUG_CONTROL_0 register
 * @{
 */
#define MACSEC_TX_DEBUG_CONTROL_0_START_CAP	OSI_BIT(31)
/** @} */

/**
 * @addtogroup MACSEC_RX_DEBUG_CONTROL_0 register
 *
 * @brief Bit definitions of MACSEC_RX_DEBUG_CONTROL_0 register
 * @{
 */
#define MACSEC_RX_DEBUG_CONTROL_0_START_CAP	OSI_BIT(31)
/** @} */

#define MTU_LENGTH_MASK			0xFFFF
#define MTU_ADDONS			(8 + 14 + 4)
#define DVLAN_TAG_ETHERTYPE	0x88A8
#define SOT_LENGTH_MASK		0xFF
#define EQOS_MACSEC_SOT_DELAY	0x4E

/**
 * @addtogroup TX/RX_BYP/SCI_LUT_VALID register
 *
 * @brief Bit definitions of LUT_VALID registers
 * @{
 */
#define MACSEC_TX_BYP_LUT_VALID_ENTRY(x)	OSI_BIT(x)
#define MACSEC_TX_BYP_LUT_VALID_NONE		0x0
#define MACSEC_TX_SCI_LUT_VALID_ENTRY(x)	OSI_BIT(x)
#define MACSEC_TX_SCI_LUT_VALID_NONE		0x0
#define MACSEC_RX_BYP_LUT_VALID_ENTRY(x)	OSI_BIT(x)
#define MACSEC_RX_BYP_LUT_VALID_NONE		0x0
#define MACSEC_RX_SCI_LUT_VALID_ENTRY(x)	OSI_BIT(x)
#define MACSEC_RX_SCI_LUT_VALID_NONE		0x0
/** @} */

/**
 * @addtogroup TX/RX LUT bit fields in LUT_DATA registers
 *
 * @brief Helper macros for LUT data programming
 * @{
 */
#define MACSEC_LUT_DATA_REG_CNT		7
/* Bit Offsets for LUT DATA[x] registers for various lookup field masks */
/* DA mask bits in LUT_DATA[1] register */
#define MACSEC_LUT_DA_BYTE0_INACTIVE		OSI_BIT(16)
#define MACSEC_LUT_DA_BYTE1_INACTIVE		OSI_BIT(17)
#define MACSEC_LUT_DA_BYTE2_INACTIVE		OSI_BIT(18)
#define MACSEC_LUT_DA_BYTE3_INACTIVE		OSI_BIT(19)
#define MACSEC_LUT_DA_BYTE4_INACTIVE		OSI_BIT(20)
#define MACSEC_LUT_DA_BYTE5_INACTIVE		OSI_BIT(21)
/* SA mask bits in LUT_DATA[3] register */
#define MACSEC_LUT_SA_BYTE0_INACTIVE		OSI_BIT(6)
#define MACSEC_LUT_SA_BYTE1_INACTIVE		OSI_BIT(7)
#define MACSEC_LUT_SA_BYTE2_INACTIVE		OSI_BIT(8)
#define MACSEC_LUT_SA_BYTE3_INACTIVE		OSI_BIT(9)
#define MACSEC_LUT_SA_BYTE4_INACTIVE		OSI_BIT(10)
#define MACSEC_LUT_SA_BYTE5_INACTIVE		OSI_BIT(11)
/* Ether type mask in LUT_DATA[3] register */
#define MACSEC_LUT_ETHTYPE_INACTIVE		OSI_BIT(28)
/* VLAN PCP mask in LUT_DATA[4] register */
#define MACSEC_LUT_VLAN_PCP_INACTIVE		OSI_BIT(0)
/* VLAN ID mask in LUT_DATA[4] register */
#define MACSEC_LUT_VLAN_ID_INACTIVE		OSI_BIT(13)
/* VLAN mask in LUT_DATA[4] register */
#define MACSEC_LUT_VLAN_ACTIVE			OSI_BIT(14)
/* Byte pattern masks in LUT_DATA[4] register */
#define MACSEC_LUT_BYTE0_PATTERN_INACTIVE	OSI_BIT(29)
/* Byte pattern masks in LUT_DATA[5] register */
#define MACSEC_LUT_BYTE1_PATTERN_INACTIVE	OSI_BIT(12)
#define MACSEC_LUT_BYTE2_PATTERN_INACTIVE	OSI_BIT(27)
/* Byte pattern masks in LUT_DATA[6] register */
#define MACSEC_LUT_BYTE3_PATTERN_INACTIVE	OSI_BIT(10)
/* Preemptable packet in LUT_DATA[6] register */
#define MACSEC_LUT_PREEMPT			OSI_BIT(11)
/* Preempt mask in LUT_DATA[6] register */
#define MACSEC_LUT_PREEMPT_INACTIVE		OSI_BIT(12)
/* Controlled port mask in LUT_DATA[6] register */
#define MACSEC_LUT_CONTROLLED_PORT		OSI_BIT(13)
/* DVLAN packet in LUT_DATA[6] register */
#define MACSEC_BYP_LUT_DVLAN_PKT		OSI_BIT(14)
/* DVLAN outer/inner tag select in LUT_DATA[6] register */
#define BYP_LUT_DVLAN_OUTER_INNER_TAG_SEL	OSI_BIT(15)
/* AN valid bits for SCI LUT in LUT_DATA[6] register */
#define MACSEC_LUT_AN0_VALID			OSI_BIT(13)
#define MACSEC_LUT_AN1_VALID			OSI_BIT(14)
#define MACSEC_LUT_AN2_VALID			OSI_BIT(15)
#define MACSEC_LUT_AN3_VALID			OSI_BIT(16)
/* DVLAN packet in LUT_DATA[6] register */
#define MACSEC_TX_SCI_LUT_DVLAN_PKT		OSI_BIT(21)
/* DVLAN outer/inner tag select in LUT_DATA[6] register */
#define MACSEC_TX_SCI_LUT_DVLAN_OUTER_INNER_TAG_SEL	OSI_BIT(22)
/* SA State LUT entry valid in LUT_DATA[0] register */
#define MACSEC_SA_STATE_LUT_ENTRY_VALID 	OSI_BIT(0)

/* Preemptable packet in LUT_DATA[2] register for Rx SCI */
#define MACSEC_RX_SCI_LUT_PREEMPT		OSI_BIT(8)
/* Preempt mask in LUT_DATA[2] register for Rx SCI */
#define MACSEC_RX_SCI_LUT_PREEMPT_INACTIVE	OSI_BIT(9)
/** @} */

/* debug buffer data read/write length */
#define DBG_BUF_LEN		4U
#define INTEGER_LEN		4U

#endif /* INCLUDED_MACSEC_H */
