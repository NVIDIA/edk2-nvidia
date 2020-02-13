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

#include "osi_dma_local.h"

int osi_init_dma_ops(struct osi_dma_priv_data *osi_dma)
{
	if (osi_dma->mac == OSI_MAC_HW_EQOS) {
		/* Get EQOS HW ops */
		osi_dma->ops = eqos_get_dma_chan_ops();
		/* Explicitly set osi_dma->safety_config = OSI_NULL if
		 * a particular MAC version does not need SW safety mechanisms
		 * like periodic read-verify.
		 */
		osi_dma->safety_config = (void *)eqos_get_dma_safety_config();
		return 0;
	}

	return -1;
}

int osi_hw_dma_init(struct osi_dma_priv_data *osi_dma)
{
	unsigned int i, chan;
	int ret;

	if ((osi_dma != OSI_NULL) && (osi_dma->ops != OSI_NULL) &&
	    osi_dma->ops->init_dma_channel != OSI_NULL) {
		osi_dma->ops->init_dma_channel(osi_dma);
	} else {
		return -1;
	}

	ret = dma_desc_init(osi_dma);
	if (ret != 0) {
		return ret;
	}

	/* Enable channel interrupts at wrapper level and start DMA */
	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		chan = osi_dma->dma_chans[i];

		ret = osi_enable_chan_tx_intr(osi_dma, chan);
		if (ret != 0) {
			return ret;
		}

		ret = osi_enable_chan_rx_intr(osi_dma, chan);
		if (ret != 0) {
			return ret;
		}

		ret = osi_start_dma(osi_dma, chan);
		if (ret != 0) {
			return ret;
		}
	}

	return ret;
}

int  osi_hw_dma_deinit(struct osi_dma_priv_data *osi_dma)
{
	unsigned int i;
	int ret = 0;

	if (osi_dma == OSI_NULL) {
		return -1;
	}

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		ret = osi_stop_dma(osi_dma, osi_dma->dma_chans[i]);
		if (ret != 0) {
			return ret;
		}
	}

	return ret;
}

int osi_disable_chan_tx_intr(struct osi_dma_priv_data *osi_dma,
			     unsigned int chan)
{
	if ((osi_dma != OSI_NULL) && (osi_dma->ops != OSI_NULL) &&
	    (osi_dma->ops->disable_chan_tx_intr != OSI_NULL)) {
		osi_dma->ops->disable_chan_tx_intr(osi_dma->base, chan);
		return 0;
	}

	return -1;
}

int osi_enable_chan_tx_intr(struct osi_dma_priv_data *osi_dma,
			    unsigned int chan)
{
	if ((osi_dma != OSI_NULL) && (osi_dma->ops != OSI_NULL) &&
	    (osi_dma->ops->enable_chan_tx_intr != OSI_NULL)) {
		osi_dma->ops->enable_chan_tx_intr(osi_dma->base, chan);
		return 0;
	}

	return -1;
}

int osi_disable_chan_rx_intr(struct osi_dma_priv_data *osi_dma,
			     unsigned int chan)
{
	if ((osi_dma != OSI_NULL) && (osi_dma->ops != OSI_NULL) &&
	    (osi_dma->ops->disable_chan_rx_intr != OSI_NULL)) {
		osi_dma->ops->disable_chan_rx_intr(osi_dma->base, chan);
		return 0;
	}

	return -1;
}

int osi_enable_chan_rx_intr(struct osi_dma_priv_data *osi_dma,
			    unsigned int chan)
{
	if ((osi_dma != OSI_NULL) && (osi_dma->ops != OSI_NULL) &&
	    (osi_dma->ops->enable_chan_rx_intr != OSI_NULL)) {
		osi_dma->ops->enable_chan_rx_intr(osi_dma->base, chan);
		return 0;
	}

	return -1;
}

int osi_clear_tx_intr(struct osi_dma_priv_data *osi_dma,
		      unsigned int chan)
{
	if ((osi_dma != OSI_NULL) && (osi_dma->ops != OSI_NULL) &&
	    (osi_dma->ops->clear_tx_intr != OSI_NULL)) {
		osi_dma->ops->clear_tx_intr(osi_dma->base, chan);
		return 0;
	}

	return -1;
}

int osi_clear_rx_intr(struct osi_dma_priv_data *osi_dma,
		      unsigned int chan)
{
	if ((osi_dma != OSI_NULL) && (osi_dma->ops != OSI_NULL) &&
	    (osi_dma->ops->clear_rx_intr != OSI_NULL)) {
		osi_dma->ops->clear_rx_intr(osi_dma->base, chan);
		return 0;
	}

	return -1;
}

int  osi_start_dma(struct osi_dma_priv_data *osi_dma,
		   unsigned int chan)
{
	if ((osi_dma != OSI_NULL) && (osi_dma->ops != OSI_NULL) &&
	    (osi_dma->ops->start_dma != OSI_NULL)) {
		osi_dma->ops->start_dma(osi_dma->base, chan);
		return 0;
	}

	return -1;
}

int osi_stop_dma(struct osi_dma_priv_data *osi_dma,
		 unsigned int chan)
{
	if ((osi_dma != OSI_NULL) && (osi_dma->ops != OSI_NULL) &&
	    (osi_dma->ops->stop_dma != OSI_NULL)) {
		osi_dma->ops->stop_dma(osi_dma->base, chan);
		return 0;
	}

	return -1;
}

unsigned int osi_get_refill_rx_desc_cnt(struct osi_rx_ring *rx_ring)
{
	if (rx_ring->cur_rx_idx >= RX_DESC_CNT ||
	    rx_ring->refill_idx >= RX_DESC_CNT) {
		return 0;
	}

	return (rx_ring->cur_rx_idx - rx_ring->refill_idx) & (RX_DESC_CNT - 1U);
}

int osi_rx_dma_desc_init(struct osi_rx_swcx *rx_swcx,
			 struct osi_rx_desc *rx_desc,
			 unsigned int use_riwt)
{
	/* for CERT-C error */
	unsigned long temp;

	if (rx_swcx != OSI_NULL && rx_desc != OSI_NULL) {
		temp = L32(rx_swcx->buf_phy_addr);
		if (temp > UINT_MAX) {
			/* error case do nothing */
		} else {
			rx_desc->rdes0 = (unsigned int)temp;
		}

		temp = H32(rx_swcx->buf_phy_addr);
		if (temp > UINT_MAX) {
			/* error case do nothing */
		} else {
			rx_desc->rdes1 = (unsigned int)temp;
		}
		rx_desc->rdes2 = 0;
		rx_desc->rdes3 = (RDES3_OWN | RDES3_IOC | RDES3_B1V);
	} else {
		return -1;
	}
	/* reset IOC bit if RWIT is enabled */
	if (use_riwt == OSI_ENABLE) {
		rx_desc->rdes3 &= ~RDES3_IOC;
	}

	return 0;
}

int osi_update_rx_tailptr(struct osi_dma_priv_data *osi_dma,
			  struct osi_rx_ring *rx_ring,
			  unsigned int chan)
{
	unsigned long tailptr = 0;
	unsigned int refill_idx = rx_ring->refill_idx;

	if (refill_idx >= RX_DESC_CNT) {
		return -1;
	}

	DECR_RX_DESC_INDEX(refill_idx, 1U);
	tailptr = rx_ring->rx_desc_phy_addr +
		  (RX_DESC_CNT * sizeof(struct osi_rx_desc));
	if (tailptr < rx_ring->rx_desc_phy_addr) {
		return -1;
	}

	if (osi_dma != OSI_NULL && osi_dma->ops != OSI_NULL &&
	    osi_dma->ops->update_rx_tailptr != OSI_NULL) {
		osi_dma->ops->update_rx_tailptr(osi_dma->base, chan, tailptr);
	} else {
		return -1;
	}

	return 0;
}

int osi_set_rx_buf_len(struct osi_dma_priv_data *osi_dma)
{
	if (osi_dma != OSI_NULL && osi_dma->ops != OSI_NULL &&
	    osi_dma->ops->set_rx_buf_len != OSI_NULL) {
		osi_dma->ops->set_rx_buf_len(osi_dma);
	} else {
		return -1;
	}

	return 0;
}

int osi_validate_dma_regs(struct osi_dma_priv_data *osi_dma)
{
	int ret = -1;
	if (osi_dma != OSI_NULL && osi_dma->ops != OSI_NULL &&
	    osi_dma->ops->validate_regs != OSI_NULL &&
	    osi_dma->safety_config != OSI_NULL) {
		ret = osi_dma->ops->validate_regs(osi_dma);
	}

	return ret;
}
