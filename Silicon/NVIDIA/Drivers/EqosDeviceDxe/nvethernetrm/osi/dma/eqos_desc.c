/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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

#include "dma_local.h"
#include "hw_desc.h"

/**
 * @brief eqos_get_rx_vlan - Get Rx VLAN from descriptor
 *
 * Algorithm:
 *      1) Check if the descriptor has any type set.
 *      2) If set, set a per packet context flag indicating packet is VLAN
 *      tagged.
 *      3) Extract VLAN tag ID from the descriptor
 *
 * @param[in] rx_desc: Rx descriptor
 * @param[in] rx_pkt_cx: Per-Rx packet context structure
 */
static inline void eqos_get_rx_vlan(struct osi_rx_desc *rx_desc,
				    struct osi_rx_pkt_cx *rx_pkt_cx)
{
	unsigned int lt;

	/* Check for Receive Status rdes0 */
	if ((rx_desc->rdes3 & RDES3_RS0V) == RDES3_RS0V) {
		/* get length or type */
		lt = rx_desc->rdes3 & RDES3_LT;
		if (lt == RDES3_LT_VT || lt == RDES3_LT_DVT) {
			rx_pkt_cx->flags |= OSI_PKT_CX_VLAN;
			rx_pkt_cx->vlan_tag = rx_desc->rdes0 & RDES0_OVT;
		}
	}
}

/**
 * @brief eqos_update_rx_err_stats - Detect Errors from Rx Descriptor
 *
 * Algorithm: This routine will be invoked by OSI layer itself which
 *	checks for the Last Descriptor and updates the receive status errors
 *	accordingly.
 *
 * @param[in] rx_desc: Rx Descriptor.
 * @param[in] pkt_err_stats: Packet error stats which stores the errors reported
 */
static inline void eqos_update_rx_err_stats(struct osi_rx_desc *rx_desc,
					    struct osi_pkt_err_stats
					    pkt_err_stats)
{
	/* increment rx crc if we see CE bit set */
	if ((rx_desc->rdes3 & RDES3_ERR_CRC) == RDES3_ERR_CRC) {
		pkt_err_stats.rx_crc_error =
			osi_update_stats_counter(
					pkt_err_stats.rx_crc_error,
					1UL);
	}

	/* increment rx frame error if we see RE bit set */
	if ((rx_desc->rdes3 & RDES3_ERR_RE) == RDES3_ERR_RE) {
		pkt_err_stats.rx_frame_error =
			osi_update_stats_counter(
					pkt_err_stats.rx_frame_error,
					1UL);
	}
}

/**
 * @brief eqos_get_rx_csum - Get the Rx checksum from descriptor if valid
 *
 * @note
 * Algorithm:
 *  - Check if the descriptor has any checksum validation errors.
 *  - If none, set a per packet context flag indicating no err in
 *    Rx checksum
 *  - The OSD layer will mark the packet appropriately to skip
 *    IP/TCP/UDP checksum validation in software based on whether
 *    COE is enabled for the device.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @param[in, out] rx_desc: Rx descriptor
 * @param[in, out] rx_pkt_cx: Per-Rx packet context structure
 */
static void eqos_get_rx_csum(struct osi_rx_desc *rx_desc,
			     struct osi_rx_pkt_cx *rx_pkt_cx)
{
	nveu32_t pkt_type;

	/* Set rxcsum flags based on RDES1 values. These are required
	 * for QNX as it requires more granularity.
	 * Set none/unnecessary bit as well for other OS to check and
	 * take proper actions.
	 */
	if ((rx_desc->rdes3 & RDES3_RS1V) != RDES3_RS1V) {
		return;
	}

	if ((rx_desc->rdes1 &
	    (RDES1_IPCE | RDES1_IPCB | RDES1_IPHE)) == OSI_DISABLE) {
		rx_pkt_cx->rxcsum |= OSI_CHECKSUM_UNNECESSARY;
	}

	if ((rx_desc->rdes1 & RDES1_IPCB) != OSI_DISABLE) {
		return;
	}

	rx_pkt_cx->rxcsum |= OSI_CHECKSUM_IPv4;
	if ((rx_desc->rdes1 & RDES1_IPHE) == RDES1_IPHE) {
		rx_pkt_cx->rxcsum |= OSI_CHECKSUM_IPv4_BAD;
	}

	pkt_type = rx_desc->rdes1 & RDES1_PT_MASK;
	if ((rx_desc->rdes1 & RDES1_IPV4) == RDES1_IPV4) {
		if (pkt_type == RDES1_PT_UDP) {
			rx_pkt_cx->rxcsum |= OSI_CHECKSUM_UDPv4;
		} else if (pkt_type == RDES1_PT_TCP) {
			rx_pkt_cx->rxcsum |= OSI_CHECKSUM_TCPv4;

		} else {
			/* Do nothing */
		}
	} else if ((rx_desc->rdes1 & RDES1_IPV6) == RDES1_IPV6) {
		if (pkt_type == RDES1_PT_UDP) {
			rx_pkt_cx->rxcsum |= OSI_CHECKSUM_UDPv6;
		} else if (pkt_type == RDES1_PT_TCP) {
			rx_pkt_cx->rxcsum |= OSI_CHECKSUM_TCPv6;

		} else {
			/* Do nothing */
		}

	} else {
		/* Do nothing */
	}

	if ((rx_desc->rdes1 & RDES1_IPCE) == RDES1_IPCE) {
		rx_pkt_cx->rxcsum |= OSI_CHECKSUM_TCP_UDP_BAD;
	}
}

/**
 * @brief eqos_get_rx_hash - Get Rx packet hash from descriptor if valid
 *
 * Algorithm: This routine will be invoked by OSI layer itself to get received
 * packet Hash from descriptor if RSS hash is valid and it also sets the type
 * of RSS hash.
 *
 * @param[in] rx_desc: Rx Descriptor.
 * @param[in] rx_pkt_cx: Per-Rx packet context structure
 */
static void eqos_get_rx_hash(struct osi_rx_desc *rx_desc,
			     struct osi_rx_pkt_cx *rx_pkt_cx)
{
}

/**
 * @brief eqos_get_rx_hwstamp - Get Rx HW Time stamp
 *
 * Algorithm:
 *	1) Check for TS availability.
 *	2) call get_tx_tstamp_status if TS is valid or not.
 *	3) If yes, set a bit and update nano seconds in rx_pkt_cx so that OSD
 *	layer can extract the time by checking this bit.
 *
 * @param[in] rx_desc: Rx descriptor
 * @param[in] context_desc: Rx context descriptor
 * @param[in] rx_pkt_cx: Rx packet context
 *
 * @retval -1 if TimeStamp is not available
 * @retval 0 if TimeStamp is available.
 */
static int eqos_get_rx_hwstamp(struct osi_dma_priv_data *osi_dma,
			       struct osi_rx_desc *rx_desc,
			       struct osi_rx_desc *context_desc,
			       struct osi_rx_pkt_cx *rx_pkt_cx)
{
	int retry;

	/* Check for RS1V/TSA/TD valid */
	if (((rx_desc->rdes3 & RDES3_RS1V) == RDES3_RS1V) &&
	    ((rx_desc->rdes1 & RDES1_TSA) == RDES1_TSA) &&
	    ((rx_desc->rdes1 & RDES1_TD) == 0U)) {
		for (retry = 0; retry < 10; retry++) {
			if (((context_desc->rdes3 & RDES3_OWN) == 0U) &&
			    ((context_desc->rdes3 & RDES3_CTXT) ==
			     RDES3_CTXT)) {
				if ((context_desc->rdes0 ==
				     OSI_INVALID_VALUE) &&
				    (context_desc->rdes1 ==
				     OSI_INVALID_VALUE)) {
					return -1;
				}
				/* Update rx pkt context flags to indicate
				 * PTP */
				rx_pkt_cx->flags |= OSI_PKT_CX_PTP;
				/* Time Stamp can be read */
				break;
			} else {
				/* TS not available yet, so retrying */
				osi_dma->osd_ops.udelay(OSI_DELAY_1US);
			}
		}
		if (retry == 10) {
			/* Timed out waiting for Rx timestamp */
			return -1;
		}

		rx_pkt_cx->ns = context_desc->rdes0 +
				(OSI_NSEC_PER_SEC * context_desc->rdes1);
		if (rx_pkt_cx->ns < context_desc->rdes0) {
			/* Will not hit this case */
			return -1;
		}
	} else {
		return -1;
	}

	return 0;
}

void eqos_init_desc_ops(struct desc_ops *d_ops)
{
	d_ops->get_rx_csum = eqos_get_rx_csum;
	d_ops->update_rx_err_stats = eqos_update_rx_err_stats;
	d_ops->get_rx_vlan = eqos_get_rx_vlan;
	d_ops->get_rx_hash = eqos_get_rx_hash;
	d_ops->get_rx_hwstamp = eqos_get_rx_hwstamp;
}
