/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
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
 *  Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 *  property and proprietary rights in and to this material, related
 *  documentation and any modifications thereto. Any use, reproduction,
 *  disclosure or distribution of this material and related documentation
 *  without an express license agreement from NVIDIA CORPORATION or
 *  its affiliates is strictly prohibited.
 *
 *  SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
 *  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 */

#ifndef OSD_H
#define OSD_H

#include "osi_core.h"
#include "osi_dma.h"

/**
 * @brief osd_usleep_range - sleep in micro seconds
 *
 * @param[in] umin: Minimum time in usecs to sleep
 * @param[in] umax: Maximum time in usecs to sleep
 */
void osd_usleep_range(unsigned long umin, unsigned long umax);

/**
 * @brief osd_msleep - sleep in milli seconds
 *
 * @param[in] msec: time in milli seconds
 */
void osd_msleep(unsigned int msec);

/**
 * @brief osd_udelay - delay in micro seconds
 *
 * @param[in] usec: time in usec
 */
void osd_udelay(unsigned long usec);

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
		unsigned long long loga);

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
			struct osi_rx_swcx *rx_pkt_swcx);

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
			   unsigned int len, struct osi_txdone_pkt_cx *txdone_pkt_cx);

/**.printf function callback */
void osd_core_printf(struct osi_core_priv_data *priv,
					nveu32_t type,
					const char *fmt, ...);

/**.printf function callback */
void osd_dma_printf(struct osi_dma_priv_data *priv,
					nveu32_t type,
					const char *fmt, ...);
#endif
