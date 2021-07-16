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
 *
 *  Portions provided under the following terms:
 *  Copyright (c) 2018-2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 *  property and proprietary rights in and to this material, related
 *  documentation and any modifications thereto. Any use, reproduction,
 *  disclosure or distribution of this material and related documentation
 *  without an express license agreement from NVIDIA CORPORATION or
 *  its affiliates is strictly prohibited.
 *
 *  SPDX-FileCopyrightText: Copyright (c) 2018-2020 NVIDIA CORPORATION & AFFILIATES
 *  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 */

#include <osd.h>
#include "osi_dma_local.h"
#include <osi_dma_txrx.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DmaLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
/**
 * @brief get_rx_csum - Get the Rx checksum from descriptor if valid
 *
 * Algorithm:
 *	1) Check if the descriptor has any checksum validation errors.
 *	2) If none, set a per packet context flag indicating no err in
 *		Rx checksum
 *	3) The OSD layer will mark the packet appropriately to skip
 *		IP/TCP/UDP checksum validation in software based on whether
 *		COE is enabled for the device.
 *
 * @param[in] rx_desc: Rx descriptor
 * @param[in] rx_pkt_cx: Per-Rx packet context structure
 */
static inline void get_rx_csum(struct osi_rx_desc *rx_desc,
			       struct osi_rx_pkt_cx *rx_pkt_cx)
{
	/* Always include either checksum none/unnecessary
	 * depending on status fields in desc.
	 * Hence no need to explicitly add OSI_PKT_CX_CSUM flag.
	 */
	if ((rx_desc->rdes3 & RDES3_RS1V) == RDES3_RS1V) {
		/* Check if no checksum errors reported in status */
		if ((rx_desc->rdes1 &
		    (RDES1_IPCE | RDES1_IPCB | RDES1_IPHE)) == 0U) {
			rx_pkt_cx->rxcsum |= OSI_CHECKSUM_UNNECESSARY;
		}
	}
}

/**
 * @brief get_rx_vlan_from_desc - Get Rx VLAN from descriptor
 *
 * Algorithm:
 *	1) Check if the descriptor has any type set.
 *	2) If set, set a per packet context flag indicating packet is VLAN
 *	tagged.
 *	3) Extract VLAN tag ID from the descriptor
 *
 * @param[in] rx_desc: Rx descriptor
 * @param[in] rx_pkt_cx: Per-Rx packet context structure
 */
static inline void get_rx_vlan_from_desc(struct osi_rx_desc *rx_desc,
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
 * @brief get_rx_tstamp_status - Get Tx Time stamp status
 *
 * Algorithm:
 *	1) Check if the received descriptor is a context descriptor.
 *	2) If yes, check whether the time stamp is valid or not.
 *
 * @param[in] context_desc: Rx context descriptor
 *
 * @retval -1 if TimeStamp is not valid
 * @retval 0 if TimeStamp is valid.
 */
static inline int get_rx_tstamp_status(struct osi_rx_desc *context_desc)
{
	if (((context_desc->rdes3 & RDES3_OWN) == 0U) &&
			((context_desc->rdes3 & RDES3_CTXT) == RDES3_CTXT)) {
		if (((context_desc->rdes0 == OSI_INVALID_VALUE) &&
		    (context_desc->rdes1 == OSI_INVALID_VALUE))) {
			/* Invalid time stamp */
			return -1;
		}
		/* tstamp can be read */
		return 0;
	}
	/* Busy */
	return -2;
}

/**
 * @brief get_rx_hwstamp - Get Rx HW Time stamp
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
static int get_rx_hwstamp(struct osi_rx_desc *rx_desc,
			  struct osi_rx_desc *context_desc,
			  struct osi_rx_pkt_cx *rx_pkt_cx)
{
	int retry, ret = -1;

	/* Check for RS1V/TSA/TD valid */
	if (((rx_desc->rdes3 & RDES3_RS1V) == RDES3_RS1V) ||
	    ((rx_desc->rdes1 & RDES1_TSA) == RDES1_TSA) ||
	    ((rx_desc->rdes1 & RDES1_TD) == 0U)) {

		for (retry = 0; retry < 10; retry++) {
			ret = get_rx_tstamp_status(context_desc);
			if (ret == 0) {
				/* Update rx pkt context flags to indicate PTP */
				rx_pkt_cx->flags |= OSI_PKT_CX_PTP;
				/* Time Stamp can be read */
				break;
			} else if (ret != -2) {
				/* Failed to get Rx timestamp */
				return ret;
			} else {
				/* Do nothing here */
			}
			/* TS not available yet, so retrying */
		}
		if (ret != 0) {
			/* Timed out waiting for Rx timestamp */
			return ret;
		}

		rx_pkt_cx->ns = context_desc->rdes0 +
				(OSI_NSEC_PER_SEC * context_desc->rdes1);
		if (rx_pkt_cx->ns < context_desc->rdes0) {
			/* Will not hit this case */
			return -1;
		}
	}
	return ret;
}


/**
 * @brief get_rx_err_stats - Detect Errors from Rx Descriptor
 *
 * Algorithm: This routine will be invoked by OSI layer itself which
 *	checks for the Last Descriptor and updates the receive status errors
 *	accordingly.
 *
 * @param[in] rx_desc: Rx Descriptor.
 * @param[in] pkt_err_stats: Packet error stats which stores the errors reported
 */
static inline void get_rx_err_stats(struct osi_rx_desc *rx_desc,
				    struct osi_pkt_err_stats pkt_err_stats)
{
	/* increment rx crc if we see CE bit set */
	if ((rx_desc->rdes3 & RDES3_ERR_CRC) == RDES3_ERR_CRC) {
		if (pkt_err_stats.rx_crc_error < ULONG_MAX) {
			pkt_err_stats.rx_crc_error++;
		}
	}
}

int osi_process_rx_completions(struct osi_dma_priv_data *osi,
			       unsigned int chan, int budget)
{
	struct osi_rx_ring *rx_ring = osi->rx_ring[chan];
	struct osi_rx_pkt_cx *rx_pkt_cx = &rx_ring->rx_pkt_cx;
	struct osi_rx_desc *rx_desc = OSI_NULL;
	struct osi_rx_swcx *rx_swcx = OSI_NULL;
	struct osi_rx_desc *context_desc = OSI_NULL;
	int received = 0;
	int ret = 0;

	if (rx_ring->cur_rx_idx >= RX_DESC_CNT) {
		return -1;
	}

	while (received < budget) {
		osi_memset(rx_pkt_cx, 0U, sizeof(*rx_pkt_cx));
		rx_desc = rx_ring->rx_desc + rx_ring->cur_rx_idx;
		rx_swcx = rx_ring->rx_swcx + rx_ring->cur_rx_idx;

		/* check for data availability */
		if ((rx_desc->rdes3 & RDES3_OWN) == RDES3_OWN) {
			break;
		}
		INCR_RX_DESC_INDEX(rx_ring->cur_rx_idx, 1U);

		/* get the length of the packet */
		rx_pkt_cx->pkt_len = rx_desc->rdes3 & RDES3_PKT_LEN;

		/* Mark pkt as valid by default */
		rx_pkt_cx->flags |= OSI_PKT_CX_VALID;

		if ((rx_desc->rdes3 & RDES3_LD) == RDES3_LD) {
			if ((rx_desc->rdes3 & RDES3_ES_BITS) != 0U) {
				/* reset validity if any of the error bits
				 * are set
				 */
				rx_pkt_cx->flags &= ~OSI_PKT_CX_VALID;
				get_rx_err_stats(rx_desc, osi->pkt_err_stats);
			}

			/* Check if COE Rx checksum is valid */
			get_rx_csum(rx_desc, rx_pkt_cx);

			get_rx_vlan_from_desc(rx_desc, rx_pkt_cx);
			context_desc = rx_ring->rx_desc + rx_ring->cur_rx_idx;
			/* Get rx time stamp */
			ret = get_rx_hwstamp(rx_desc, context_desc, rx_pkt_cx);
			/* Rx timestamping not used in UEFI. */
			if (ret == 0) {
				/* Context descriptor was consumed. Its skb
				 * and DMA mapping will be recycled
				 */
				INCR_RX_DESC_INDEX(rx_ring->cur_rx_idx, 1U);
			}
			osd_receive_packet(osi->osd, rx_ring, chan,
					   osi->rx_buf_len, rx_pkt_cx, rx_swcx);
		}
		osi->dstats.q_rx_pkt_n[chan] =
			osi_update_stats_counter(osi->dstats.q_rx_pkt_n[chan],
						 1UL);
		osi->dstats.rx_pkt_n =
			osi_update_stats_counter(osi->dstats.rx_pkt_n, 1UL);
		received++;
	}

	return received;
}

/**
 * @brief get_tx_err_stats - Detect Errors from Tx Status
 *
 * Algorithm: This routine will be invoked by OSI layer itself which
 *	checks for the Last Descriptor and updates the transmit status errors
 *	accordingly.
 *
 * @param[in] tx_desc: Tx Descriptor.
 * @param[in] pkt_err_stats: Pakcet error stats which stores the errors reported
 */
static inline void get_tx_err_stats(struct osi_tx_desc *tx_desc,
				    struct osi_pkt_err_stats pkt_err_stats)
{
	/* IP Header Error */
	if ((tx_desc->tdes3 & TDES3_IP_HEADER_ERR) == TDES3_IP_HEADER_ERR) {
		if (pkt_err_stats.ip_header_error < ULONG_MAX) {
			pkt_err_stats.ip_header_error++;
		}
	}

	/* Jabber timeout Error */
	if ((tx_desc->tdes3 & TDES3_JABBER_TIMEO_ERR) ==
	    TDES3_JABBER_TIMEO_ERR) {
		if (pkt_err_stats.jabber_timeout_error < ULONG_MAX) {
			pkt_err_stats.jabber_timeout_error++;
		}
	}

	/* Packet Flush Error */
	if ((tx_desc->tdes3 & TDES3_PKT_FLUSH_ERR) == TDES3_PKT_FLUSH_ERR) {
		if (pkt_err_stats.pkt_flush_error < ULONG_MAX) {
			pkt_err_stats.pkt_flush_error++;
		}
	}

	/* Payload Checksum Error */
	if ((tx_desc->tdes3 & TDES3_PL_CHK_SUM_ERR) == TDES3_PL_CHK_SUM_ERR) {
		if (pkt_err_stats.payload_cs_error < ULONG_MAX) {
			pkt_err_stats.payload_cs_error++;
		}
	}

	/* Loss of Carrier Error */
	if ((tx_desc->tdes3 & TDES3_LOSS_CARRIER_ERR) ==
	    TDES3_LOSS_CARRIER_ERR) {
		if (pkt_err_stats.loss_of_carrier_error < ULONG_MAX) {
			pkt_err_stats.loss_of_carrier_error++;
		}
	}

	/* No Carrier Error */
	if ((tx_desc->tdes3 & TDES3_NO_CARRIER_ERR) == TDES3_NO_CARRIER_ERR) {
		if (pkt_err_stats.no_carrier_error < ULONG_MAX) {
			pkt_err_stats.no_carrier_error++;
		}
	}

	/* Late Collision Error */
	if ((tx_desc->tdes3 & TDES3_LATE_COL_ERR) == TDES3_LATE_COL_ERR) {
		if (pkt_err_stats.late_collision_error < ULONG_MAX) {
			pkt_err_stats.late_collision_error++;
		}
	}

	/* Execessive Collision Error */
	if ((tx_desc->tdes3 & TDES3_EXCESSIVE_COL_ERR) ==
	    TDES3_EXCESSIVE_COL_ERR) {
		if (pkt_err_stats.excessive_collision_error < ULONG_MAX) {
			pkt_err_stats.excessive_collision_error++;
		}
	}

	/* Excessive Deferal Error */
	if ((tx_desc->tdes3 & TDES3_EXCESSIVE_DEF_ERR) ==
	    TDES3_EXCESSIVE_DEF_ERR) {
		if (pkt_err_stats.excessive_deferal_error < ULONG_MAX) {
			pkt_err_stats.excessive_deferal_error++;
		}
	}

	/* Under Flow Error */
	if ((tx_desc->tdes3 & TDES3_UNDER_FLOW_ERR) == TDES3_UNDER_FLOW_ERR) {
		if (pkt_err_stats.underflow_error < ULONG_MAX) {
			pkt_err_stats.underflow_error++;
		}
	}
}

int osi_clear_tx_pkt_err_stats(struct osi_dma_priv_data *osi_dma)
{
	int ret = -1;

	if (osi_dma != OSI_NULL) {
		/* Reset tx packet errors */
		osi_dma->pkt_err_stats.ip_header_error = 0U;
		osi_dma->pkt_err_stats.jabber_timeout_error = 0U;
		osi_dma->pkt_err_stats.pkt_flush_error = 0U;
		osi_dma->pkt_err_stats.payload_cs_error = 0U;
		osi_dma->pkt_err_stats.loss_of_carrier_error = 0U;
		osi_dma->pkt_err_stats.no_carrier_error = 0U;
		osi_dma->pkt_err_stats.late_collision_error = 0U;
		osi_dma->pkt_err_stats.excessive_collision_error = 0U;
		osi_dma->pkt_err_stats.excessive_deferal_error = 0U;
		osi_dma->pkt_err_stats.underflow_error = 0U;
		ret = 0;
	}

	return ret;
}

int osi_clear_rx_pkt_err_stats(struct osi_dma_priv_data *osi_dma)
{
	int ret = -1;

	if (osi_dma != OSI_NULL) {
		/* Reset Rx packet errors */
		osi_dma->pkt_err_stats.rx_crc_error = 0U;
		ret = 0;
	}

	return ret;
}

int osi_process_tx_completions(struct osi_dma_priv_data *osi,
			       unsigned int chan, unsigned int budget)
{
	struct osi_tx_ring *tx_ring = osi->tx_ring[chan];
	struct osi_txdone_pkt_cx *txdone_pkt_cx = &tx_ring->txdone_pkt_cx;
	struct osi_tx_swcx *tx_swcx = OSI_NULL;
	struct osi_tx_desc *tx_desc = OSI_NULL;
	unsigned int entry = tx_ring->clean_idx;
	unsigned long vartdes1;
	unsigned long long ns;
	int processed = 0;

	osi->dstats.tx_clean_n[chan] =
		osi_update_stats_counter(osi->dstats.tx_clean_n[chan], 1U);

	while (entry != tx_ring->cur_tx_idx && processed < budget) {
		osi_memset(txdone_pkt_cx, 0U, sizeof(*txdone_pkt_cx));

		tx_desc = tx_ring->tx_desc + entry;
		tx_swcx = tx_ring->tx_swcx + entry;

		if ((tx_desc->tdes3 & TDES3_OWN) == TDES3_OWN) {
			break;
		}

		/* check for Last Descriptor */
		if ((tx_desc->tdes3 & TDES3_LD) == TDES3_LD) {
			if ((tx_desc->tdes3 & TDES3_ES_BITS) != 0U) {
				txdone_pkt_cx->flags |= OSI_TXDONE_CX_ERROR;
				/* fill packet error stats */
				get_tx_err_stats(tx_desc, osi->pkt_err_stats);
			}
		}

		if (((tx_desc->tdes3 & TDES3_LD) == TDES3_LD) &&
		    ((tx_desc->tdes3 & TDES3_CTXT) == 0U)) {
			/* check tx tstamp status */
			if ((tx_desc->tdes3 & TDES3_TTSS) != 0U) {
				/* tx timestamp captured for this packet */
				ns = tx_desc->tdes0;
				vartdes1 = tx_desc->tdes1;
				if (OSI_NSEC_PER_SEC >
						(OSI_ULLONG_MAX / vartdes1)) {
					/* Will not hit this case */
				} else if (OSI_ULLONG_MAX -
					(vartdes1 * OSI_NSEC_PER_SEC) < ns) {
					/* Will not hit this case */
				} else {
					txdone_pkt_cx->flags |=
						OSI_TXDONE_CX_TS;
					txdone_pkt_cx->ns = ns +
						(vartdes1 * OSI_NSEC_PER_SEC);
				}
			} else {
				/* Do nothing here */
			}
		}

		if (tx_swcx->is_paged_buf == 1U) {
			txdone_pkt_cx->flags |= OSI_TXDONE_CX_PAGED_BUF;
		}

		osd_transmit_complete(osi->osd, tx_swcx->buf_virt_addr,
				      tx_swcx->buf_phy_addr, tx_swcx->len,
				      txdone_pkt_cx);

		tx_desc->tdes3 = 0;
		tx_desc->tdes2 = 0;
		tx_desc->tdes1 = 0;
		tx_desc->tdes0 = 0;
		tx_swcx->len = 0;

		tx_swcx->buf_virt_addr = OSI_NULL;
		tx_swcx->buf_phy_addr = 0;
		tx_swcx->is_paged_buf = 0;
		INCR_TX_DESC_INDEX(entry, 1U);
		if (processed < INT_MAX) {
			processed++;
		}

		/* Don't wait to update tx_ring->clean-idx. It will
		 * be used by OSD layer to determine the num. of available
		 * descriptors in the ring, which will inturn be used to
		 * wake the corresponding transmit queue in OS layer.
		 */
		tx_ring->clean_idx = entry;
		osi->dstats.q_tx_pkt_n[chan] =
			osi_update_stats_counter(osi->dstats.q_tx_pkt_n[chan],
						 1UL);
		osi->dstats.tx_pkt_n =
			osi_update_stats_counter(osi->dstats.tx_pkt_n, 1UL);
	}

	return processed;
}

/**
 * @brief need_cntx_desc - Helper function to check if context desc is needed.
 *
 * Algorithm:
 *	1) Check if transmit packet context flags are set
 *	2) If set, set the context descriptor bit along
 *	with other context information in the transmit descriptor.
 *
 * @param[in] tx_pkt_cx: Pointer to transmit packet context structure
 * @param[in] tx_desc: Pointer to tranmit descriptor to be filled.
 *
 * @retval 0 - cntx desc not used
 * @retval 1 - cntx desc used.
 */
static inline int need_cntx_desc(struct osi_tx_pkt_cx *tx_pkt_cx,
				 struct osi_tx_desc *tx_desc)
{
	int ret = 0;

	if (((tx_pkt_cx->flags & OSI_PKT_CX_VLAN) == OSI_PKT_CX_VLAN) ||
	    ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO)) {
		/* Set context type */
		tx_desc->tdes3 |= TDES3_CTXT;

		if ((tx_pkt_cx->flags & OSI_PKT_CX_VLAN) == OSI_PKT_CX_VLAN) {
			/* Remove any overflow bits. VT field is 16bit field */
			tx_pkt_cx->vtag_id &= TDES3_VT_MASK;
			/* Fill VLAN Tag ID */
			tx_desc->tdes3 |= tx_pkt_cx->vtag_id;
			/* Set VLAN TAG Valid */
			tx_desc->tdes3 |= TDES3_VLTV;
		}

		if ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO) {
			/* Remove any overflow bits. MSS is 13bit field */
			tx_pkt_cx->mss &= TDES2_MSS_MASK;
			/* Fill MSS */
			tx_desc->tdes2 |= tx_pkt_cx->mss;
			/* Set MSS valid */
			tx_desc->tdes3 |= TDES3_TCMSSV;
		}

		ret = 1;
	}

	return ret;
}

/**
 * @brief fill_first_desc - Helper function to fill the first transmit
 *	descriptor.
 *
 * Algorithm:
 *	  1) Update the buffer address and length of buffer in first desc.
 *	  2) Check if any features like HW checksum offload, TSO, VLAN insertion
 *	  etc. are flagged in transmit packet context. If so, set the fiels in
 *	  first desc corresponding to those features.
 *
 * @param[in] tx_pkt_cx: Pointer to transmit packet context structure
 * @param[in] tx_desc: Pointer to tranmit descriptor to be filled.
 * @param[in] tx_swcx: Pointer to corresponding tx descriptor software context.
 */
static inline void fill_first_desc(struct osi_tx_pkt_cx *tx_pkt_cx,
				   struct osi_tx_desc *tx_desc,
				   struct osi_tx_swcx *tx_swcx)
{
	unsigned long tmp;

	/* update the first buffer pointer and length */
	tmp = L32(tx_swcx->buf_phy_addr);
	if (tmp < UINT_MAX) {
		tx_desc->tdes0 = (unsigned int)tmp;
	}

	tmp = H32(tx_swcx->buf_phy_addr);
	if (tmp < UINT_MAX) {
		tx_desc->tdes1 = (unsigned int)tmp;
	}

	tx_desc->tdes2 = tx_swcx->len;
	/* Mark it as First descriptor */
	tx_desc->tdes3 |= TDES3_FD;

	/* If HW checksum offload enabled, mark CIC bits of FD */
	if ((tx_pkt_cx->flags & OSI_PKT_CX_CSUM) == OSI_PKT_CX_CSUM) {
		tx_desc->tdes3 |= TDES3_HW_CIC;
	}

	/* Enable VTIR in normal descriptor for VLAN packet */
	if ((tx_pkt_cx->flags & OSI_PKT_CX_VLAN) == OSI_PKT_CX_VLAN) {
		tx_desc->tdes2 |= TDES2_VTIR;
	}

	/* if TS is set enable timestamping */
	if ((tx_pkt_cx->flags & OSI_PKT_CX_PTP) == OSI_PKT_CX_PTP) {
		tx_desc->tdes2 |= TDES2_TTSE;
	}

	/* Enable TSE bit and update TCP hdr, payload len */
	if ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO) {
		tx_desc->tdes3 |= TDES3_TSE;

		/* Minimum value for THL field is 5 for TSO
		 * So divide L4 hdr len by 4
		 * Typical TCP hdr len = 20B / 4 = 5
		 */
		tx_pkt_cx->tcp_udp_hdrlen /= OSI_TSO_HDR_LEN_DIVISOR;
		/* Remove any overflow bits. THL field is only 4 bit wide */
		tx_pkt_cx->tcp_udp_hdrlen &= TDES3_THL_MASK;
		/* Update hdr len in desc */
		tx_desc->tdes3 |= (tx_pkt_cx->tcp_udp_hdrlen <<
				   TDES3_THL_SHIFT);
		/* Remove any overflow bits. TPL field is 18 bit wide */
		tx_pkt_cx->payload_len &= TDES3_TPL_MASK;
		/* Update TCP payload len in desc */
		tx_desc->tdes3 |= tx_pkt_cx->payload_len;
	}
}

void osi_hw_transmit(struct osi_dma_priv_data *osi, unsigned int chan)
{
	struct osi_tx_ring *tx_ring = osi->tx_ring[chan];
	struct osi_dma_chan_ops *ops = osi->ops;
	unsigned int entry = tx_ring->cur_tx_idx;
	struct osi_tx_desc *tx_desc = tx_ring->tx_desc + entry;
	struct osi_tx_swcx *tx_swcx = tx_ring->tx_swcx + entry;
	struct osi_tx_pkt_cx *tx_pkt_cx = &tx_ring->tx_pkt_cx;
	unsigned int desc_cnt = tx_pkt_cx->desc_cnt;
	struct osi_tx_desc *last_desc = OSI_NULL;
	struct osi_tx_desc *first_desc = OSI_NULL;
	struct osi_tx_desc *cx_desc = OSI_NULL;
	unsigned long tailptr, tmp;
	unsigned int i;
	int cntx_desc_consumed;

	if (entry >= TX_DESC_CNT) {
		return;
	}
	if (desc_cnt == 0U) {
		/* Will not hit this case */
		return;
	}

	/* Context decriptor for VLAN/TSO */
	if ((tx_pkt_cx->flags & OSI_PKT_CX_VLAN) == OSI_PKT_CX_VLAN) {
		osi->dstats.tx_vlan_pkt_n =
			osi_update_stats_counter(osi->dstats.tx_vlan_pkt_n,
						 1UL);
	}

	if ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO) {
		osi->dstats.tx_tso_pkt_n =
			osi_update_stats_counter(osi->dstats.tx_tso_pkt_n, 1UL);
	}

	cntx_desc_consumed = need_cntx_desc(tx_pkt_cx, tx_desc);
	if (cntx_desc_consumed == 1) {
		INCR_TX_DESC_INDEX(entry, 1U);

		/* Storing context descriptor to set DMA_OWN at last */
		cx_desc = tx_desc;
		tx_desc = tx_ring->tx_desc + entry;
		tx_swcx = tx_ring->tx_swcx + entry;
		desc_cnt--;
	}

	/* Fill first descriptor */
	fill_first_desc(tx_pkt_cx, tx_desc, tx_swcx);
	INCR_TX_DESC_INDEX(entry, 1U);

	first_desc = tx_desc;
	last_desc = tx_desc;
	tx_desc = tx_ring->tx_desc + entry;
	tx_swcx = tx_ring->tx_swcx + entry;
	desc_cnt--;

	/* Fill remaining descriptors */
	for (i = 0; i < desc_cnt; i++) {
		tmp = L32(tx_swcx->buf_phy_addr);
		if (tmp < UINT_MAX) {
			tx_desc->tdes0 = (unsigned int)tmp;
		}

		tmp = H32(tx_swcx->buf_phy_addr);
		if (tmp < UINT_MAX) {
			tx_desc->tdes1 = (unsigned int)tmp;
		}
		tx_desc->tdes2 = tx_swcx->len;
		/* set HW OWN bit for descriptor*/
		tx_desc->tdes3 |= TDES3_OWN;

		INCR_TX_DESC_INDEX(entry, 1U);
		last_desc = tx_desc;
		tx_desc = tx_ring->tx_desc + entry;
		tx_swcx = tx_ring->tx_swcx + entry;
	}


	/* Mark it as LAST descriptor */
	last_desc->tdes3 |= TDES3_LD;
	/* set Interrupt on Completion*/
	last_desc->tdes2 |= TDES2_IOC;


	/* Set OWN bit for first and context descriptors
	 * at the end to avoid race condition
	 */
	first_desc->tdes3 |= TDES3_OWN;
	if (cntx_desc_consumed == 1) {
		cx_desc->tdes3 |= TDES3_OWN;
	}

	tailptr = tx_ring->tx_desc_phy_addr +
		  (TX_DESC_CNT * sizeof(struct osi_tx_desc));
	if (tailptr < tx_ring->tx_desc_phy_addr) {
		/* Will not hit this case */
		return;
	}

	//Make sure descriptors are updated before starting DMA again
	MemoryFence ();

	ops->update_tx_tailptr(osi->base, chan, tailptr);
	tx_ring->cur_tx_idx = entry;
}

/**
 * @brief rx_dma_desc_initialization - Initialize DMA Receive descriptors for Rx
 *
 * Algorithm: Initialize Receive descriptors with DMA mappable buffers,
 *	set OWN bit, Rx ring length and set starting address of Rx DMA channel.
 *	Tx ring base address in Tx DMA registers.
 *
 * @param[in] osi:	OSI private data structure.
 * @param[in] chan:	Rx channel number.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int rx_dma_desc_initialization(struct osi_dma_priv_data *osi,
				      unsigned int chan)
{
	struct osi_rx_ring *rx_ring = osi->rx_ring[chan];
	struct osi_rx_desc *rx_desc = OSI_NULL;
	struct osi_rx_swcx *rx_swcx = OSI_NULL;
	struct osi_dma_chan_ops *ops = osi->ops;
	unsigned long tailptr = 0, tmp;
	unsigned int i;
	int ret = 0;

	rx_ring->cur_rx_idx = 0;
	rx_ring->refill_idx = 0;

	for (i = 0; i < RX_DESC_CNT; i++) {
		rx_swcx = rx_ring->rx_swcx + i;
		rx_desc = rx_ring->rx_desc + i;

		tmp = L32(rx_swcx->buf_phy_addr);
		if (tmp < UINT_MAX) {
			rx_desc->rdes0 = (unsigned int)tmp;
		}

		tmp = H32(rx_swcx->buf_phy_addr);
		if (tmp < UINT_MAX) {
			rx_desc->rdes1 = (unsigned int)tmp;
		}

		rx_desc->rdes2 = 0;
		rx_desc->rdes3 = (RDES3_OWN | RDES3_IOC | RDES3_B1V);
		/* reconfigure INTE bit if RX watchdog timer is enabled */
		if (osi->use_riwt == OSI_ENABLE) {
			rx_desc->rdes3 &= ~RDES3_IOC;
		}
	}

	tailptr = rx_ring->rx_desc_phy_addr +
		  sizeof(struct osi_rx_desc) * (RX_DESC_CNT - 1U);
	if (tailptr < rx_ring->rx_desc_phy_addr) {
		/* Will not hit this case */
		return -1;
	}

	ops->set_rx_ring_len(osi->base, chan, (RX_DESC_CNT - 1U));
	ops->update_rx_tailptr(osi->base, chan, tailptr);
	ops->set_rx_ring_start_addr(osi->base, chan, rx_ring->rx_desc_phy_addr);

	return ret;
}

/**
 * @brief rx_dma_desc_init - Initialize DMA Receive descriptors for Rx channel.
 *
 * Algorithm: Initialize Receive descriptors with DMA mappabled buffers,
 *	set OWN bit, Rx ring length and set starting address of Rx DMA channel.
 *	Tx ring base address in Tx DMA registers.
 *
 * @param[in] osi: OSI private data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int rx_dma_desc_init(struct osi_dma_priv_data *osi)
{
	unsigned int chan = 0;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < osi->num_dma_chans; i++) {
		chan = osi->dma_chans[i];

		ret = rx_dma_desc_initialization(osi, chan);
		if (ret != 0) {
			return ret;
		}
	}

	return ret;
}

/**
 * @brief tx_dma_desc_init - Initialize DMA Transmit descriptors.
 *
 * Algorithm: Initialize Trannsmit descriptors and set Tx ring length,
 *	Tx ring base address in Tx DMA registers.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 */
static void tx_dma_desc_init(struct osi_dma_priv_data *osi_dma)
{
	struct osi_tx_ring *tx_ring = OSI_NULL;
	struct osi_tx_desc *tx_desc = OSI_NULL;
	struct osi_dma_chan_ops *ops = osi_dma->ops;
	unsigned int chan = 0;
	unsigned int i, j;

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		chan = osi_dma->dma_chans[i];

		tx_ring = osi_dma->tx_ring[chan];

		for (j = 0; j < TX_DESC_CNT; j++) {
			tx_desc = tx_ring->tx_desc + j;

			tx_desc->tdes0 = 0;
			tx_desc->tdes1 = 0;
			tx_desc->tdes2 = 0;
			tx_desc->tdes3 = 0;
		}

		tx_ring->cur_tx_idx = 0;
		tx_ring->clean_idx = 0;

		ops->set_tx_ring_len(osi_dma->base, chan,
				(TX_DESC_CNT - 1U));
		ops->set_tx_ring_start_addr(osi_dma->base, chan,
				tx_ring->tx_desc_phy_addr);
	}
}

int dma_desc_init(struct osi_dma_priv_data *osi_dma)
{
	int ret = 0;

	tx_dma_desc_init(osi_dma);

	ret = rx_dma_desc_init(osi_dma);
	if (ret != 0) {
		return ret;
	}

	return ret;
}

