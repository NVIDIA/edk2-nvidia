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
 */

#include "dma_local.h"
#include <local_common.h>
#include "hw_desc.h"
#include "../osi/common/common.h"
#ifdef OSI_DEBUG
#include "debug.h"
#endif /* OSI_DEBUG */

/**
 * @brief g_dma - DMA local data array.
 */
static struct dma_local g_dma[MAX_DMA_INSTANCES];

/**
 * @brief g_ops - local DMA HW operations array.
 */
static struct dma_chan_ops g_ops[MAX_MAC_IP_TYPES];

struct osi_dma_priv_data *osi_get_dma(void)
{
	nveu32_t i;

	for (i = 0U; i < MAX_DMA_INSTANCES; i++) {
		if (g_dma[i].init_done == OSI_ENABLE) {
			continue;
		}

		break;
	}

	if (i == MAX_DMA_INSTANCES) {
		return OSI_NULL;
	}

	g_dma[i].magic_num = (nveu64_t)&g_dma[i].osi_dma;

	return &g_dma[i].osi_dma;
}

/**
 * @brief Function to validate input arguments of API.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] l_dma: Local OSI DMA data structure.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t validate_args(struct osi_dma_priv_data *osi_dma,
				    struct dma_local *l_dma)
{
	if ((osi_dma == OSI_NULL) || (osi_dma->base == OSI_NULL) ||
	    (l_dma->init_done == OSI_DISABLE)) {
		return -1;
	}

	return 0;
}

/**
 * @brief Function to validate input arguments of API.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA channel number.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t validate_dma_chan_num(struct osi_dma_priv_data *osi_dma,
					    nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (chan >= l_dma->max_chans) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				"Invalid DMA channel number\n", chan);
		return -1;
	}

	return 0;
}

/**
 * @brief Function to validate array of DMA channels.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t validate_dma_chans(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	nveu32_t i = 0;

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		if (osi_dma->dma_chans[i] > l_dma->max_chans) {
			OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				    "Invalid DMA channel number:\n",
				    osi_dma->dma_chans[i]);
			return -1;
		}
	}

	return 0;
}

/**
 * @brief Function to validate function pointers.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] ops_p: Pointer to OSI DMA channel operations.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t validate_func_ptrs(struct osi_dma_priv_data *osi_dma,
				  struct dma_chan_ops *ops_p)
{
	nveu32_t i = 0;
	void *temp_ops = (void *)ops_p;
#if __SIZEOF_POINTER__ == 8
	nveu64_t *l_ops = (nveu64_t *)temp_ops;
#elif __SIZEOF_POINTER__ == 4
	nveu32_t *l_ops = (nveu32_t *)temp_ops;
#else
	OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
		    "DMA: Undefined architecture\n", 0ULL);
	return -1;
#endif

	for (i = 0; i < (sizeof(*ops_p) / (nveu64_t)__SIZEOF_POINTER__); i++) {
		if (*l_ops == 0U) {
			OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				    "dma: fn ptr validation failed at\n",
				    (nveu64_t)i);
			return -1;
		}

		l_ops++;
	}

	return 0;
}

nve32_t osi_init_dma_ops(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	typedef void (*init_ops_arr)(struct dma_chan_ops *temp);
	typedef void *(*safety_init)(void);

	init_ops_arr i_ops[MAX_MAC_IP_TYPES] = {
		eqos_init_dma_chan_ops, mgbe_init_dma_chan_ops
	};

	safety_init s_init[MAX_MAC_IP_TYPES] = {
		eqos_get_dma_safety_config, OSI_NULL
	};

	if (osi_dma == OSI_NULL) {
		return -1;
	}

	if ((l_dma->magic_num != (nveu64_t)osi_dma) ||
	    (l_dma->init_done == OSI_ENABLE)) {
		return -1;
	}

	if ((osi_dma->osd_ops.transmit_complete == OSI_NULL) ||
	    (osi_dma->osd_ops.receive_packet == OSI_NULL) ||
	    (osi_dma->osd_ops.ops_log == OSI_NULL) ||
#ifdef OSI_DEBUG
	    (osi_dma->osd_ops.printf == OSI_NULL) ||
#endif /* OSI_DEBUG */
	    (osi_dma->osd_ops.udelay == OSI_NULL)) {
		return -1;
	}

	if (osi_dma->mac > OSI_MAC_HW_MGBE) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "DMA: Invalid MAC HW type\n", 0ULL);
		return -1;
	}

	i_ops[osi_dma->mac](&g_ops[osi_dma->mac]);

	if (s_init[osi_dma->mac] != OSI_NULL) {
		osi_dma->safety_config = s_init[osi_dma->mac]();
	}

	if (init_desc_ops(osi_dma) < 0) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "DMA desc ops init failed\n", 0ULL);
		return -1;
	}

	if (validate_func_ptrs(osi_dma, &g_ops[osi_dma->mac]) < 0) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "DMA ops validation failed\n", 0ULL);
		return -1;
	}

	l_dma->ops_p = &g_ops[osi_dma->mac];
	l_dma->init_done = OSI_ENABLE;

	return 0;
}

nve32_t osi_hw_dma_init(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	nveu32_t i, chan;
	nve32_t ret;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	l_dma->mac_ver = osi_readl((nveu8_t *)osi_dma->base + MAC_VERSION) &
				   MAC_VERSION_SNVER_MASK;
	if (validate_mac_ver_update_chans(l_dma->mac_ver,
					  &l_dma->max_chans) == 0) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "Invalid MAC version\n", (nveu64_t)l_dma->mac_ver);
		return -1;
	}

	if (osi_dma->num_dma_chans > l_dma->max_chans) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "Invalid number of DMA channels\n", 0ULL);
		return -1;
	}

	if (validate_dma_chans(osi_dma) < 0) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "DMA channels validation failed\n", 0ULL);
		return -1;
	}

	ret = l_dma->ops_p->init_dma_channel(osi_dma);
	if (ret < 0) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "dma: init dma channel failed\n", 0ULL);
		return ret;
	}

	ret = dma_desc_init(osi_dma, l_dma->ops_p);
	if (ret != 0) {
		return ret;
	}

	if ((l_dma->mac_ver !=  OSI_EQOS_MAC_4_10) &&
	    (l_dma->mac_ver != OSI_EQOS_MAC_5_00)) {
		l_dma->vm_intr = OSI_ENABLE;
	}

	/* Enable channel interrupts at wrapper level and start DMA */
	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		chan = osi_dma->dma_chans[i];

		l_dma->ops_p->enable_chan_tx_intr(osi_dma->base, chan);
		l_dma->ops_p->enable_chan_rx_intr(osi_dma->base, chan);
		l_dma->ops_p->start_dma(osi_dma, chan);
	}

	/**
	 * OSD will update this if PTP needs to be run in diffrent modes.
	 * Default configuration is PTP sync in two step sync with slave mode.
	 */
	if (osi_dma->ptp_flag == 0U) {
		osi_dma->ptp_flag = (OSI_PTP_SYNC_SLAVE | OSI_PTP_SYNC_TWOSTEP);
	}

	return 0;
}

nve32_t osi_hw_dma_deinit(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	nveu32_t i;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (osi_dma->num_dma_chans > l_dma->max_chans) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "Invalid number of DMA channels\n", 0ULL);
		return -1;
	}

	if (validate_dma_chans(osi_dma) < 0) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "DMA channels validation failed\n", 0ULL);
		return -1;
	}

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		l_dma->ops_p->stop_dma(osi_dma, osi_dma->dma_chans[i]);
	}

	/* FIXME: Need to fix */
//	l_dma->magic_num = 0;
//	l_dma->init_done = OSI_DISABLE;

	return 0;
}

nve32_t osi_disable_chan_tx_intr(struct osi_dma_priv_data *osi_dma,
				 nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		return -1;
	}

	l_dma->ops_p->disable_chan_tx_intr(osi_dma->base, chan);

	return 0;
}

nve32_t osi_enable_chan_tx_intr(struct osi_dma_priv_data *osi_dma,
				nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		return -1;
	}

	l_dma->ops_p->enable_chan_tx_intr(osi_dma->base, chan);

	return 0;
}

nve32_t osi_disable_chan_rx_intr(struct osi_dma_priv_data *osi_dma,
				 nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		return -1;
	}

	l_dma->ops_p->disable_chan_rx_intr(osi_dma->base, chan);

	return 0;
}

nve32_t osi_enable_chan_rx_intr(struct osi_dma_priv_data *osi_dma,
				nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		return -1;
	}

	l_dma->ops_p->enable_chan_rx_intr(osi_dma->base, chan);

	return 0;
}

nve32_t osi_clear_vm_tx_intr(struct osi_dma_priv_data *osi_dma,
			     nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		return -1;
	}

	l_dma->ops_p->clear_vm_tx_intr(osi_dma->base, chan);

	return 0;
}

nve32_t osi_clear_vm_rx_intr(struct osi_dma_priv_data *osi_dma,
			     nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		return -1;
	}

	l_dma->ops_p->clear_vm_rx_intr(osi_dma->base, chan);

	return 0;
}

nveu32_t osi_get_global_dma_status(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return 0;
	}

	return l_dma->ops_p->get_global_dma_status(osi_dma->base);
}

nve32_t osi_handle_dma_intr(struct osi_dma_priv_data *osi_dma,
			    nveu32_t chan,
			    nveu32_t tx_rx,
			    nveu32_t en_dis)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	typedef void (*dma_intr_fn)(void *base, nveu32_t ch);
	dma_intr_fn fn[2][2][2] = {
		{ { l_dma->ops_p->disable_chan_tx_intr, l_dma->ops_p->enable_chan_tx_intr },
		  { l_dma->ops_p->disable_chan_rx_intr, l_dma->ops_p->enable_chan_rx_intr } },
		{ { l_dma->ops_p->clear_vm_tx_intr, l_dma->ops_p->enable_chan_tx_intr },
		  { l_dma->ops_p->clear_vm_rx_intr, l_dma->ops_p->enable_chan_rx_intr } }
	};

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		return -1;
	}

	if ((tx_rx > OSI_DMA_CH_RX_INTR) ||
	    (en_dis > OSI_DMA_INTR_ENABLE)) {
		return -1;
	}

	fn[l_dma->vm_intr][tx_rx][en_dis](osi_dma->base, chan);

	return 0;
}

nve32_t osi_start_dma(struct osi_dma_priv_data *osi_dma,
		      nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		return -1;
	}

	l_dma->ops_p->start_dma(osi_dma, chan);

	return 0;
}

nve32_t osi_stop_dma(struct osi_dma_priv_data *osi_dma,
		     nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		return -1;
	}

	l_dma->ops_p->stop_dma(osi_dma, chan);

	return 0;
}

nveu32_t osi_get_refill_rx_desc_cnt(struct osi_rx_ring *rx_ring)
{
	if ((rx_ring == OSI_NULL) ||
	    (rx_ring->cur_rx_idx >= RX_DESC_CNT) ||
	    (rx_ring->refill_idx >= RX_DESC_CNT)) {
		return 0;
	}

	return (rx_ring->cur_rx_idx - rx_ring->refill_idx) & (RX_DESC_CNT - 1U);
}

/**
 * @brief rx_dma_desc_validate_args - DMA Rx descriptor init args Validate
 *
 * Algorithm: Validates DMA Rx descriptor init argments.
 *
 * @param[in] osi_dma: OSI DMA private data struture.
 * @param[in] rx_ring: HW ring corresponding to Rx DMA channel.
 * @param[in] chan: Rx DMA channel number
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t rx_dma_desc_validate_args(
					    struct osi_dma_priv_data *osi_dma,
					    struct dma_local *l_dma,
					    struct osi_rx_ring *rx_ring,
					    nveu32_t chan)
{
	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	if (!((rx_ring != OSI_NULL) && (rx_ring->rx_swcx != OSI_NULL) &&
	      (rx_ring->rx_desc != OSI_NULL))) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "dma: Invalid pointers\n", 0ULL);
		return -1;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "dma: Invalid channel\n", 0ULL);
		return -1;
	}

	return 0;
}

/**
 * @brief rx_dma_handle_ioc - DMA Rx descriptor RWIT Handler
 *
 * Algorithm:
 * 1) Check RWIT enable and reset IOC bit
 * 2) Check rx_frames enable and update IOC bit
 *
 * @param[in] osi_dma: OSI DMA private data struture.
 * @param[in] rx_ring: HW ring corresponding to Rx DMA channel.
 * @param[in, out] rx_desc: Rx Rx descriptor.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 */
static inline void rx_dma_handle_ioc(struct osi_dma_priv_data *osi_dma,
				     struct osi_rx_ring *rx_ring,
				     struct osi_rx_desc *rx_desc)
{
	/* reset IOC bit if RWIT is enabled */
	if (osi_dma->use_riwt == OSI_ENABLE) {
		rx_desc->rdes3 &= ~RDES3_IOC;
		/* update IOC bit if rx_frames is enabled. Rx_frames
		 * can be enabled only along with RWIT.
		 */
		if (osi_dma->use_rx_frames == OSI_ENABLE) {
			if ((rx_ring->refill_idx %
			    osi_dma->rx_frames) == OSI_NONE) {
				rx_desc->rdes3 |= RDES3_IOC;
			}
		}
	}
}

nve32_t osi_rx_dma_desc_init(struct osi_dma_priv_data *osi_dma,
			     struct osi_rx_ring *rx_ring, nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	nveu64_t tailptr = 0;
	struct osi_rx_swcx *rx_swcx = OSI_NULL;
	struct osi_rx_desc *rx_desc = OSI_NULL;

	if (rx_dma_desc_validate_args(osi_dma, l_dma, rx_ring, chan) < 0) {
		/* Return on arguments validation failure */
		return -1;
	}

	/* Refill buffers */
	while ((rx_ring->refill_idx != rx_ring->cur_rx_idx) &&
	       (rx_ring->refill_idx < RX_DESC_CNT)) {
		rx_swcx = rx_ring->rx_swcx + rx_ring->refill_idx;
		rx_desc = rx_ring->rx_desc + rx_ring->refill_idx;

		if ((rx_swcx->flags & OSI_RX_SWCX_BUF_VALID) !=
		    OSI_RX_SWCX_BUF_VALID) {
			break;
		}

		rx_swcx->flags = 0;

		/* Populate the newly allocated buffer address */
		rx_desc->rdes0 = L32(rx_swcx->buf_phy_addr);
		rx_desc->rdes1 = H32(rx_swcx->buf_phy_addr);

		rx_desc->rdes2 = 0;
		rx_desc->rdes3 = RDES3_IOC;

		if (osi_dma->mac == OSI_MAC_HW_EQOS) {
			rx_desc->rdes3 |= RDES3_B1V;
		}

		/* Reset IOC bit if RWIT is enabled */
		rx_dma_handle_ioc(osi_dma, rx_ring, rx_desc);
		rx_desc->rdes3 |= RDES3_OWN;

		INCR_RX_DESC_INDEX(rx_ring->refill_idx, 1U);
	}

	/* Update the Rx tail ptr  whenever buffer is replenished to
	 * kick the Rx DMA to resume if it is in suspend. Always set
	 * Rx tailptr to 1 greater than last descriptor in the ring since HW
	 * knows to loop over to start of ring.
	 */
	tailptr = rx_ring->rx_desc_phy_addr +
		  (sizeof(struct osi_rx_desc) * (RX_DESC_CNT));

	if (osi_unlikely(tailptr < rx_ring->rx_desc_phy_addr)) {
		/* Will not hit this case, used for CERT-C compliance */
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "dma: Invalid tailptr\n", 0ULL);
		return -1;
	}

	l_dma->ops_p->update_rx_tailptr(osi_dma->base, chan, tailptr);

	return 0;
}

nve32_t osi_set_rx_buf_len(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	l_dma->ops_p->set_rx_buf_len(osi_dma);

	return 0;
}

nve32_t osi_dma_get_systime_from_mac(struct osi_dma_priv_data *const osi_dma,
				     nveu32_t *sec, nveu32_t *nsec)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	common_get_systime_from_mac(osi_dma->base, osi_dma->mac, sec, nsec);

	return 0;
}

nveu32_t osi_is_mac_enabled(struct osi_dma_priv_data *const osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return OSI_DISABLE;
	}

	return common_is_mac_enabled(osi_dma->base, osi_dma->mac);
}

nve32_t osi_hw_transmit(struct osi_dma_priv_data *osi_dma, nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (osi_unlikely(validate_args(osi_dma, l_dma) < 0)) {
		return -1;
	}

	if (osi_unlikely(validate_dma_chan_num(osi_dma, chan) < 0)) {
		return -1;
	}

	if (osi_unlikely(osi_dma->tx_ring[chan] == OSI_NULL)) {
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "DMA: Invalid Tx ring\n", 0ULL);
		return -1;
	}

	return hw_transmit(osi_dma, osi_dma->tx_ring[chan], l_dma->ops_p, chan);
}

nve32_t osi_dma_ioctl(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	struct osi_dma_ioctl_data *data;

	if (osi_unlikely(validate_args(osi_dma, l_dma) < 0)) {
		return -1;
	}

	data = &osi_dma->ioctl_data;

	switch (data->cmd) {
#ifdef OSI_DEBUG
	case OSI_DMA_IOCTL_CMD_REG_DUMP:
		reg_dump(osi_dma);
		break;
	case OSI_DMA_IOCTL_CMD_STRUCTS_DUMP:
		structs_dump(osi_dma);
		break;
#endif /* OSI_DEBUG */
	default:
		OSI_DMA_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			    "DMA: Invalid IOCTL command", 0ULL);
		return -1;
	}

	return 0;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief osi_slot_args_validate - Validate slot function arguments
 *
 * @note
 * Algorithm:
 *  - Check set argument and return error.
 *  - Validate osi_dma structure pointers.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] set: Flag to set with OSI_ENABLE and reset with OSI_DISABLE
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t osi_slot_args_validate(struct osi_dma_priv_data *osi_dma,
					     struct dma_local *l_dma,
					     nveu32_t set)
{
	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	/* return on invalid set argument */
	if ((set != OSI_ENABLE) && (set != OSI_DISABLE)) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "dma: Invalid set argument\n", set);
		return -1;
	}

	return 0;
}

nve32_t osi_config_slot_function(struct osi_dma_priv_data *osi_dma,
				 nveu32_t set)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	nveu32_t i = 0U, chan = 0U, interval = 0U;
	struct osi_tx_ring *tx_ring = OSI_NULL;

	/* Validate arguments */
	if (osi_slot_args_validate(osi_dma, l_dma, set) < 0) {
		return -1;
	}

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		/* Get DMA channel and validate */
		chan = osi_dma->dma_chans[i];

		if ((chan == 0x0U) ||
		    (chan >= l_dma->max_chans)) {
			/* Ignore 0 and invalid channels */
			continue;
		}

		/* Check for slot enable */
		if (osi_dma->slot_enabled[chan] == OSI_ENABLE) {
			/* Get DMA slot interval and validate */
			interval = osi_dma->slot_interval[chan];
			if (interval > OSI_SLOT_INTVL_MAX) {
				OSI_DMA_ERR(osi_dma->osd,
					    OSI_LOG_ARG_INVALID,
					    "dma: Invalid interval arguments\n",
					    interval);
				return -1;
			}

			tx_ring = osi_dma->tx_ring[chan];
			if (tx_ring == OSI_NULL) {
				OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
					    "tx_ring is null\n", chan);
				return -1;
			}
			tx_ring->slot_check = set;
			l_dma->ops_p->config_slot(osi_dma, chan, set, interval);
		}
	}

	return 0;
}

nve32_t osi_validate_dma_regs(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;

	if (validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	return l_dma->ops_p->validate_regs(osi_dma);
}

nve32_t osi_txring_empty(struct osi_dma_priv_data *osi_dma, nveu32_t chan)
{
	struct osi_tx_ring *tx_ring = osi_dma->tx_ring[chan];

	return (tx_ring->clean_idx == tx_ring->cur_tx_idx) ? 1 : 0;
}
#endif /* !OSI_STRIPPED_LIB */
