/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
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

#ifndef INCLUDED_HW_DESC_H
#define INCLUDED_HW_DESC_H

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
#define RDES3_CDA		OSI_BIT(27)
#define RDES3_LD		OSI_BIT(28)
#define RDES3_FD		OSI_BIT(29)
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
#define RDES3_RSV		OSI_BIT(26)
#define RDES0_OVT		0x0000FFFFU
#define RDES3_TSD		OSI_BIT(6)
#define RDES3_TSA		OSI_BIT(4)
#define RDES1_TSA		OSI_BIT(14)
#define RDES1_TD		OSI_BIT(15)
#define RDES3_L34T		0x00F00000U
#define RDES3_L34T_IPV4_TCP	OSI_BIT(20)
#define RDES3_L34T_IPV4_UDP	OSI_BIT(21)
#define RDES3_L34T_IPV6_TCP	(OSI_BIT(23) | OSI_BIT(20))
#define RDES3_L34T_IPV6_UDP	(OSI_BIT(23) | OSI_BIT(21))

#define RDES1_IPCE		OSI_BIT(7)
#define RDES1_IPCB		OSI_BIT(6)
#define RDES1_IPV6		OSI_BIT(5)
#define RDES1_IPV4		OSI_BIT(4)
#define RDES1_IPHE		OSI_BIT(3)
#define RDES1_PT_MASK		(OSI_BIT(2) | OSI_BIT(1) | OSI_BIT(0))
#define RDES1_PT_TCP		OSI_BIT(1)
#define RDES1_PT_UDP		OSI_BIT(0)
#define RDES3_ELLT		0xF0000U
#define RDES3_ELLT_IPHE		0x50000U
#define RDES3_ELLT_CSUM_ERR	0x60000U
#define RDES3_ELLT_CVLAN	0x90000U
/** @} */

/** Error Summary bits for Received packet */
#define RDES3_ES_BITS \
	(RDES3_ERR_CRC | RDES3_ERR_GP | RDES3_ERR_WD | \
	RDES3_ERR_ORUN | RDES3_ERR_RE | RDES3_ERR_DRIB)

/** MGBE error summary bits for Received packet */
#define RDES3_ES_MGBE		0x8000U
#define RDES3_ERR_MGBE_CRC	(OSI_BIT(16) | OSI_BIT(17))
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
#define TDES3_OSTC              OSI_BIT(27)
#define TDES3_TSE		OSI_BIT(18)
#define TDES3_HW_CIC_ALL	(OSI_BIT(16) | OSI_BIT(17))
#define TDES3_HW_CIC_IP_ONLY	(OSI_BIT(16))
#define TDES3_VT_MASK		0xFFFFU
#define TDES3_THL_MASK		0xFU
#define TDES3_TPL_MASK		0x3FFFFU
#define TDES3_PL_MASK		0x7FFFU
#define TDES3_THL_SHIFT		19U
#define TDES3_VLTV		OSI_BIT(16)
#define TDES3_TTSS		OSI_BIT(17)
#define TDES3_PIDV		OSI_BIT(25)

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
#define TDES2_VTIR		((nveu32_t)0x2 << 14U)
#define TDES2_TTSE		((nveu32_t)0x1 << 30U)
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
#endif /* INCLUDED_HW_DESC_H */
