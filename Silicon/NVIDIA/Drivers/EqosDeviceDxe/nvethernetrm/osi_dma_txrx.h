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

#ifndef OSI_DMA_TXRX_H
#define OSI_DMA_TXRX_H

/**
 * @addtogroup EQOS_Help Descriptor Helper MACROS
 *
 * @brief Helper macros for defining Tx/Rx descriptor count
 * @{
 */
#define TX_DESC_CNT	256U
#define RX_DESC_CNT	256U
/** @} */

/** TSO Header length divisor */
#define OSI_TSO_HDR_LEN_DIVISOR	4U

/**
 * @addtogroup EQOS_Help1 Helper MACROS for descriptor index operations
 *
 * @brief Helper macros for incrementing or decrementing Tx/Rx descriptor index
 * @{
 */
/** Increment the tx descriptor index */
#define INCR_TX_DESC_INDEX(idx, i) ((idx) = ((idx) + (i)) & (TX_DESC_CNT - 1U))
/** Decrement the tx descriptor index */
#define DECR_TX_DESC_INDEX(idx, i) ((idx) = ((idx) - (i)) & (TX_DESC_CNT - 1U))
/** Increment the rx descriptor index */
#define INCR_RX_DESC_INDEX(idx, i) ((idx) = ((idx) + (i)) & (RX_DESC_CNT - 1U))
/** Decrement the rx descriptor index */
#define DECR_RX_DESC_INDEX(idx, i) ((idx) = ((idx) - (i)) & (RX_DESC_CNT - 1U))
/** @} */

/**
 * @addtogroup EQOS_RxDesc Receive Descriptors bit fields
 *
 * @brief These macros are used to check the value in specific bit fields of
 * the descriptor. The fields in the descriptor are mapped as
 * defined in the HW manual
 * @{
 */
#define RDES3_OWN		OSI_BIT(31)
#define RDES3_CTXT		OSI_BIT(30)
#define RDES3_IOC		OSI_BIT(30)
#define RDES3_B1V		OSI_BIT(24)
#define RDES3_LD		OSI_BIT(28)
#define RDES3_ERR_CRC		OSI_BIT(24)
#define RDES3_ERR_GP		OSI_BIT(23)
#define RDES3_ERR_WD		OSI_BIT(22)
#define RDES3_ERR_ORUN		OSI_BIT(21)
#define RDES3_ERR_RE		OSI_BIT(20)
#define RDES3_ERR_DRIB		OSI_BIT(19)
#define RDES3_PKT_LEN		0x00007fffU
#define RDES3_LT		(OSI_BIT(16) | OSI_BIT(17) | OSI_BIT(18))
#define RDES3_LT_VT		OSI_BIT(18)
#define RDES3_LT_DVT		(OSI_BIT(16) | OSI_BIT(18))
#define RDES3_RS0V		OSI_BIT(25)
#define RDES3_RS1V		OSI_BIT(26)
#define RDES0_OVT		0x0000FFFFU
#define RDES1_TSA		OSI_BIT(14)
#define RDES1_TD		OSI_BIT(15)

#define RDES1_IPCE		OSI_BIT(7)
#define RDES1_IPCB		OSI_BIT(6)
#define RDES1_IPHE		OSI_BIT(3)
/** @} */

/** Error Summary bits for Received packet */
#define RDES3_ES_BITS \
	(RDES3_ERR_CRC | RDES3_ERR_GP | RDES3_ERR_WD | \
	RDES3_ERR_ORUN | RDES3_ERR_RE | RDES3_ERR_DRIB)

/**
 * @addtogroup EQOS_TxDesc Transmit Descriptors bit fields
 *
 * @brief These macros are used to check the value in specific bit fields of
 * the descriptor. The fields in the descriptor are mapped as
 * defined in the HW manual
 * @{
 */
#define TDES2_IOC		OSI_BIT(31)
#define TDES2_MSS_MASK		0x3FFFU
#define TDES3_OWN		OSI_BIT(31)
#define TDES3_CTXT		OSI_BIT(30)
#define TDES3_TCMSSV		OSI_BIT(26)
#define TDES3_FD		OSI_BIT(29)
#define TDES3_LD		OSI_BIT(28)
#define TDES3_TSE		OSI_BIT(18)
#define TDES3_HW_CIC		(OSI_BIT(16) | OSI_BIT(17))
#define TDES3_VT_MASK		0xFFFFU
#define TDES3_THL_MASK		0xFU
#define TDES3_TPL_MASK		0x3FFFFU
#define TDES3_THL_SHIFT		19U
#define TDES3_VLTV		OSI_BIT(16)
#define TDES3_TTSS		OSI_BIT(17)

/* Tx Errors */
#define TDES3_IP_HEADER_ERR	OSI_BIT(0)
#define TDES3_UNDER_FLOW_ERR	OSI_BIT(2)
#define TDES3_EXCESSIVE_DEF_ERR	OSI_BIT(3)
#define TDES3_EXCESSIVE_COL_ERR	OSI_BIT(8)
#define TDES3_LATE_COL_ERR	OSI_BIT(9)
#define TDES3_NO_CARRIER_ERR	OSI_BIT(10)
#define TDES3_LOSS_CARRIER_ERR	OSI_BIT(11)
#define TDES3_PL_CHK_SUM_ERR	OSI_BIT(12)
#define TDES3_PKT_FLUSH_ERR	OSI_BIT(13)
#define TDES3_JABBER_TIMEO_ERR	OSI_BIT(14)

/* VTIR = 0x2 (Insert a VLAN tag with the tag value programmed in the
 * MAC_VLAN_Incl register or context descriptor.)
*/
#define TDES2_VTIR		((unsigned int)0x2 << 14U)
#define TDES2_TTSE		((unsigned int)0x1 << 30U)
/** @} */

/** Error Summary bits for Transmitted packet */
#define TDES3_ES_BITS		(TDES3_IP_HEADER_ERR     | \
				 TDES3_UNDER_FLOW_ERR    | \
				 TDES3_EXCESSIVE_DEF_ERR | \
				 TDES3_EXCESSIVE_COL_ERR | \
				 TDES3_LATE_COL_ERR      | \
				 TDES3_NO_CARRIER_ERR    | \
				 TDES3_LOSS_CARRIER_ERR  | \
				 TDES3_PL_CHK_SUM_ERR    | \
				 TDES3_PKT_FLUSH_ERR     | \
				 TDES3_JABBER_TIMEO_ERR)
#endif /* OSI_DMA_TXRX_H */
