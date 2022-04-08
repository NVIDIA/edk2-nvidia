/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 */

#include "osd.h"
#include "EmacDxeUtil.h"
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseMemoryLib.h>

/**
 * @brief osd_usleep_range - sleep in micro seconds
 *
 * @param[in] umin: Minimum time in usecs to sleep
 * @param[in] umax: Maximum time in usecs to sleep
 */
void osd_usleep_range(unsigned long umin, unsigned long umax) {
	MicroSecondDelay (umin);
}

/**
 * @brief osd_msleep - sleep in milli seconds
 *
 * @param[in] msec: time in milli seconds
 */
void osd_msleep(unsigned int msec) {
	MicroSecondDelay (((unsigned long)msec) * 1000);
}

/**
 * @brief osd_udelay - delay in micro seconds
 *
 * @param[in] usec: time in usec
 */
void osd_udelay(unsigned long usec) {
	MicroSecondDelay (usec);
}

/**
 * @brief osd_log - logging function
 *
 * @param[in] priv: OSD private data
 * @param[in] func: function name
 * @param[in] line: line number
 * @param[in] level: log level
 * @param[in] type: error type
 * @param[in] err: error string
 * @param[in] loga: error data
 */
void osd_log(void *priv, const char *func, unsigned int line,
		unsigned int level, unsigned int type, const char *err,
		unsigned long long loga) {
	if (level == OSI_LOG_ERR) {
		DEBUG ((DEBUG_ERROR, "Osd: Error: Function: %a Line: %d Type: %d Err: %a Loga:0x%lx \r\n",
				func, line, type, err, loga));
	} else if (level == OSI_LOG_WARN) {
		DEBUG ((DEBUG_ERROR, "Osd: Warning: Function: %a Line: %d Type: %d Err: %a Loga:0x%lx \r\n",
				func, line, type, err, loga));
	} else if (level == OSI_LOG_INFO) {
		DEBUG ((DEBUG_ERROR, "Osd: Info: Function: %a Line: %d Type: %d Err: %a Loga:0x%lx \r\n",
				func, line, type, err, loga));
	}
}

/**
 * @brief osd_receive_packet - Handover received packet to network stack.
 *
 * Algorithm:
 *	  1) Unmap the DMA buffer address.
 *	  2) Updates socket buffer with len and ether type and handover to
 *	  OS network stack.
 *	  3) Refill the Rx ring based on threshold.
 *	  4) Fills the rxpkt_cx->flags with the below bit fields accordingly
 *	  OSI_PKT_CX_VLAN
 *	  OSI_PKT_CX_VALID
 *	  OSI_PKT_CX_CSUM
 *	  OSI_PKT_CX_TSO
 *	  OSI_PKT_CX_PTP
 *
 * @param[in] priv: OSD private data structure.
 * @param[in] rxring: Pointer to DMA channel Rx ring.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] dma_buf_len: Rx DMA buffer length.
 * @param[in] rxpkt_cx: Received packet context.
 * @param[in] rx_pkt_swcx: Received packet sw context.
 *
 * @note Rx completion need to make sure that Rx descriptors processed properly.
 */
void osd_receive_packet(void *priv, struct osi_rx_ring *rxring, unsigned int chan,
			unsigned int dma_buf_len, struct osi_rx_pkt_cx *rxpkt_cx,
			struct osi_rx_swcx *rx_pkt_swcx){
	EMAC_DRIVER *EmacDriver = (EMAC_DRIVER *)priv;
	EmacDriver->rx_pkt_swcx = rx_pkt_swcx;
	rx_pkt_swcx->flags |= OSI_RX_SWCX_PROCESSED;
	EmacDriver->rxpkt_cx = rxpkt_cx;
}

/**
 * @brief osd_transmit_complete - Transmit completion routine.
 *
 * Algorithm:
 *	  1) Updates stats for Linux network stack.
 *	  2) unmap and free the buffer DMA address and buffer.
 *	  3) Time stamp will be updated to stack if available.
 *
 * @param[in] priv: OSD private data structure.
 * @param[in] buffer: Buffer address to free.
 * @param[in] dmaaddr: DMA address to unmap.
 * @param[in] len: Length of data.
 * @param[in] txdone_pkt_cx: Pointer to struct which has tx done status info.
 *		This struct has flags to indicate tx error, whether DMA address
 *		is mapped from paged/linear buffer, Time stamp availability,
 *		if TS available txdone_pkt_cx->ns stores the time stamp.
 *		Below are the valid bit maps set for txdone_pkt_cx->flags
 *		OSI_TXDONE_CX_PAGED_BUF         OSI_BIT(0)
 *		OSI_TXDONE_CX_ERROR             OSI_BIT(1)
 *		OSI_TXDONE_CX_TS                OSI_BIT(2)
 *
 * @note Tx completion need to make sure that Tx descriptors processed properly.
 */
void osd_transmit_complete(void *priv, void *buffer, unsigned long dmaaddr,
			   unsigned int len, struct osi_txdone_pkt_cx *txdone_pkt_cx){
	EMAC_DRIVER *EmacDriver = (EMAC_DRIVER *)priv;
	EmacDriver->tx_completed_buffer = buffer;
}

/**.printf function callback */
void osd_core_printf(struct osi_core_priv_data *priv,
					 nveu32_t type,
					 const char *fmt, ...)
{
	VA_LIST  Marker;

	VA_START (Marker, fmt);
	DebugVPrint (DEBUG_ERROR, fmt, Marker);
	VA_END (Marker);
}

/**.printf function callback */
void osd_dma_printf(struct osi_dma_priv_data *priv,
					nveu32_t type,
					const char *fmt, ...)
{
	VA_LIST  Marker;

	VA_START (Marker, fmt);
	DebugVPrint (DEBUG_ERROR, fmt, Marker);
	VA_END (Marker);
}
