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
#include "mgbe_desc.h"

/**
 * @brief mgbe_get_rx_vlan - Get Rx VLAN from descriptor
 *
 * Algorithm:
 *      1) Check if the descriptor has CVLAN set
 *      2) If set, set a per packet context flag indicating packet is VLAN
 *      tagged.
 *      3) Extract VLAN tag ID from the descriptor
 *
 * @param[in] rx_desc: Rx descriptor
 * @param[in] rx_pkt_cx: Per-Rx packet context structure
 */
static inline void mgbe_get_rx_vlan(struct osi_rx_desc *rx_desc,
				    struct osi_rx_pkt_cx *rx_pkt_cx)
{
	unsigned int ellt = rx_desc->rdes3 & RDES3_ELLT;

	if ((ellt & RDES3_ELLT_CVLAN) == RDES3_ELLT_CVLAN) {
		rx_pkt_cx->flags |= OSI_PKT_CX_VLAN;
		rx_pkt_cx->vlan_tag = rx_desc->rdes0 & RDES0_OVT;
	}
}

/**
 * @brief mgbe_get_rx_err_stats - Detect Errors from Rx Descriptor
 *
 * Algorithm: This routine will be invoked by OSI layer itself which
 *	checks for the Last Descriptor and updates the receive status errors
 *	accordingly.
 *
 * @param[in] rx_desc: Rx Descriptor.
 * @param[in] pkt_err_stats: Packet error stats which stores the errors reported
 */
static inline void mgbe_update_rx_err_stats(struct osi_rx_desc *rx_desc,
					    struct osi_pkt_err_stats
					    pkt_err_stats)
{
	unsigned int frpsm = 0;
	unsigned int frpsl = 0;

	/* increment rx crc if we see CE bit set */
	if ((rx_desc->rdes3 & RDES3_ERR_MGBE_CRC) == RDES3_ERR_MGBE_CRC) {
		pkt_err_stats.rx_crc_error =
			osi_update_stats_counter(pkt_err_stats.rx_crc_error,
						 1UL);
	}

	/* Update FRP Counters */
	frpsm = rx_desc->rdes2 & MGBE_RDES2_FRPSM;
	frpsl = rx_desc->rdes3 & MGBE_RDES3_FRPSL;
	/* Increment FRP parsed count */
	if ((frpsm == OSI_NONE) && (frpsl == OSI_NONE)) {
		pkt_err_stats.frp_parsed =
			osi_update_stats_counter(pkt_err_stats.frp_parsed, 1UL);
	}
	/* Increment FRP dropped count */
	if ((frpsm == OSI_NONE) && (frpsl == MGBE_RDES3_FRPSL)) {
		pkt_err_stats.frp_dropped =
			osi_update_stats_counter(pkt_err_stats.frp_dropped,
						 1UL);
	}
	/* Increment FRP Parsing Error count */
	if ((frpsm == MGBE_RDES2_FRPSM) && (frpsl == OSI_NONE)) {
		pkt_err_stats.frp_err =
			osi_update_stats_counter(pkt_err_stats.frp_err, 1UL);
	}
	/* Increment FRP Incomplete Parsing count */
	if ((frpsm == MGBE_RDES2_FRPSM) && (frpsl == MGBE_RDES3_FRPSL)) {
		pkt_err_stats.frp_incomplete =
			osi_update_stats_counter(pkt_err_stats.frp_incomplete,
						 1UL);
	}
}

/**
 * @brief mgbe_get_rx_csum - Get the Rx checksum from descriptor if valid
 *
 * Algorithm:
 *      1) Check if the descriptor has any checksum validation errors.
 *      2) If none, set a per packet context flag indicating no err in
 *              Rx checksum
 *      3) The OSD layer will mark the packet appropriately to skip
 *              IP/TCP/UDP checksum validation in software based on whether
 *              COE is enabled for the device.
 *
 * @param[in] rx_desc: Rx descriptor
 * @param[in] rx_pkt_cx: Per-Rx packet context structure
 */
static void mgbe_get_rx_csum(struct osi_rx_desc *rx_desc,
			     struct osi_rx_pkt_cx *rx_pkt_cx)
{
	unsigned int ellt = rx_desc->rdes3 & RDES3_ELLT;

	/* Always include either checksum none/unnecessary
	 * depending on status fields in desc.
	 * Hence no need to explicitly add OSI_PKT_CX_CSUM flag.
	 */
	if ((ellt != RDES3_ELLT_IPHE) && (ellt != RDES3_ELLT_CSUM_ERR)) {
		rx_pkt_cx->rxcsum |= OSI_CHECKSUM_UNNECESSARY;
	}
}

/**
 * @brief mgbe_get_rx_hash - Get Rx packet hash from descriptor if valid
 *
 * Algorithm: This routine will be invoked by OSI layer itself to get received
 * packet Hash from descriptor if RSS hash is valid and it also sets the type
 * of RSS hash.
 *
 * @param[in] rx_desc: Rx Descriptor.
 * @param[in] rx_pkt_cx: Per-Rx packet context structure
 */
static void mgbe_get_rx_hash(struct osi_rx_desc *rx_desc,
			     struct osi_rx_pkt_cx *rx_pkt_cx)
{
	unsigned int pkt_type = rx_desc->rdes3 & RDES3_L34T;

	if ((rx_desc->rdes3 & RDES3_RSV) != RDES3_RSV) {
		return;
	}

	switch (pkt_type) {
	case RDES3_L34T_IPV4_TCP:
	case RDES3_L34T_IPV4_UDP:
	case RDES3_L34T_IPV6_TCP:
	case RDES3_L34T_IPV6_UDP:
		rx_pkt_cx->rx_hash_type = OSI_RX_PKT_HASH_TYPE_L4;
		break;
	default:
		rx_pkt_cx->rx_hash_type = OSI_RX_PKT_HASH_TYPE_L3;
		break;
	}

	/* Get Rx hash from RDES1 RSSH */
	rx_pkt_cx->rx_hash = rx_desc->rdes1;
	rx_pkt_cx->flags |= OSI_PKT_CX_RSS;
}

/** 
 * @brief mgbe_get_rx_hwstamp - Get Rx HW Time stamp
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
static int mgbe_get_rx_hwstamp(struct osi_dma_priv_data *osi_dma,
			       struct osi_rx_desc *rx_desc,
			       struct osi_rx_desc *context_desc,
			       struct osi_rx_pkt_cx *rx_pkt_cx)
{
	int retry;

	if ((rx_desc->rdes3 & RDES3_CDA) != RDES3_CDA) {
		return -1;
	}

	for (retry = 0; retry < 10; retry++) {
		if (((context_desc->rdes3 & RDES3_OWN) == 0U) &&
		    ((context_desc->rdes3 & RDES3_CTXT) == RDES3_CTXT) &&
		    ((context_desc->rdes3 & RDES3_TSA) == RDES3_TSA) &&
		    ((context_desc->rdes3 & RDES3_TSD) != RDES3_TSD)) {
			if ((context_desc->rdes0 == OSI_INVALID_VALUE) &&
			    (context_desc->rdes1 == OSI_INVALID_VALUE)) {
				/* Invalid time stamp */
				return -1;
			}
			/* Update rx pkt context flags to indicate PTP */
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

	return 0;
}

void mgbe_init_desc_ops(struct desc_ops *d_ops)
{
        d_ops->get_rx_csum = mgbe_get_rx_csum;
	d_ops->update_rx_err_stats = mgbe_update_rx_err_stats;
	d_ops->get_rx_vlan = mgbe_get_rx_vlan;
	d_ops->get_rx_hash = mgbe_get_rx_hash;
	d_ops->get_rx_hwstamp = mgbe_get_rx_hwstamp;
}
