/*
 * Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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

#ifdef OSI_DEBUG
#include "debug.h"

/**
 * @brief dump_struct - Dumps a given structure.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] ptr: Pointer to structure.
 * @param[in] size: Size of structure to dump.
 *
 */
static void dump_struct(struct osi_dma_priv_data *osi_dma,
			unsigned char *ptr,
			unsigned long size)
{
	nveu32_t i = 0, rem, j;
	unsigned long temp;

	if (ptr == OSI_NULL) {
		osi_dma->osd_ops.printf(osi_dma, OSI_DEBUG_TYPE_STRUCTS,
					"Pointer is NULL\n");
		return;
	}

	rem = i % 4;
	temp = size - rem;

	for (i = 0; i < temp; i += 4) {
		osi_dma->osd_ops.printf(osi_dma, OSI_DEBUG_TYPE_STRUCTS,
					"%02x%02x%02x%02x", ptr[i], ptr[i + 1],
					ptr[i + 2], ptr[i + 3]);
		j = i;
	}

	for (i = j; i < size; i++) {
		osi_dma->osd_ops.printf(osi_dma, OSI_DEBUG_TYPE_STRUCTS, "%x",
					ptr[i]);
	}
}

/**
 * @brief structs_dump - Dumps OSI DMA structure.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 */
void structs_dump(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	nveu32_t i = 0;

	osi_dma->osd_ops.printf(osi_dma, OSI_DEBUG_TYPE_STRUCTS,
				"OSI DMA struct size: %lu",
				sizeof(struct osi_dma_priv_data));
	dump_struct(osi_dma, (unsigned char *)osi_dma,
		    sizeof(struct osi_dma_priv_data));

	osi_dma->osd_ops.printf(osi_dma, OSI_DEBUG_TYPE_STRUCTS,
				"OSI DMA Tx/Rx Ring struct sizes: %lu %lu",
				sizeof(struct osi_tx_ring),
				sizeof(struct osi_rx_ring));
	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		dump_struct(osi_dma, (unsigned char *)osi_dma->tx_ring[i],
			    sizeof(struct osi_tx_ring));
		dump_struct(osi_dma, (unsigned char *)osi_dma->rx_ring[i],
			    sizeof(struct osi_rx_ring));
	}


	osi_dma->osd_ops.printf(osi_dma, OSI_DEBUG_TYPE_STRUCTS,
				"OSD DMA ops struct size: %lu",
				sizeof(struct osd_dma_ops));
	dump_struct(osi_dma, (unsigned char *)(&osi_dma->osd_ops),
		    sizeof(struct osd_dma_ops));

	osi_dma->osd_ops.printf(osi_dma, OSI_DEBUG_TYPE_STRUCTS,
				"OSI local DMA struct size: %lu",
				sizeof(struct dma_local));
	dump_struct(osi_dma, (unsigned char *)l_dma,
		    sizeof(struct dma_local));

	osi_dma->osd_ops.printf(osi_dma, OSI_DEBUG_TYPE_STRUCTS,
				"OSI local ops DMA struct size: %lu",
				sizeof(struct dma_chan_ops));
	dump_struct(osi_dma, (unsigned char *)l_dma->ops_p,
		    sizeof(struct dma_chan_ops));
}

/**
 * @brief reg_dump - Dumps MAC DMA registers
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 */
void reg_dump(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	unsigned int max_addr;
	unsigned int addr;
	unsigned int reg_val;

	switch (l_dma->mac_ver) {
	case OSI_EQOS_MAC_5_00:
		addr = 0x1100;
		max_addr = 0x12E4;
		break;
	case OSI_EQOS_MAC_5_30:
		addr = 0x116C;
		max_addr = 0x14EC;
		break;
	case OSI_MGBE_MAC_3_10:
	case OSI_MGBE_MAC_4_00:
		addr = 0x3100;
		max_addr = 0x35FC;
		break;
	default:
		return;
	}

	while (1) {
		if (addr > max_addr)
			break;

		reg_val = osi_readl((nveu8_t *)osi_dma->base + addr);
		osi_dma->osd_ops.printf(osi_dma, OSI_DEBUG_TYPE_REG,
					"%x: %x\n", addr, reg_val);
		addr += 4;
	}
}

/**
 * @brief rx_desc_dump - Function to dump Rx descriptors
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] idx: Index to be dumped in Rx ring.
 * @param[in] chan: DMA channel number
 */
static void rx_desc_dump(struct osi_dma_priv_data *osi_dma, unsigned int idx,
			 unsigned int chan)
{
	struct osi_rx_ring *rx_ring = osi_dma->rx_ring[chan];
	struct osi_rx_desc *rx_desc = rx_ring->rx_desc + idx;
	struct osd_dma_ops *ops = &osi_dma->osd_ops;

	ops->printf(osi_dma, OSI_DEBUG_TYPE_DESC,
		    "N [%02d %4p %04d %lx R_D] = %#x:%#x:%#x:%#x\n",
		    chan, rx_desc, idx,
		    (rx_ring->rx_desc_phy_addr + (idx * sizeof(struct osi_rx_desc))),
		    rx_desc->rdes3, rx_desc->rdes2,
		    rx_desc->rdes1, rx_desc->rdes0);

}

/**
 * @brief tx_desc_dump - Function to dump Tx descriptors
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] f_idx: First index to be dumped in Tx ring.
 * @param[in] l_idx: Last index to be dumped in Tx ring.
 * @param[in] tx: Represents whether packet queued for tx done.
 * @param[in] chan: DMA channel number
 */
static void tx_desc_dump(struct osi_dma_priv_data *osi_dma, unsigned int f_idx,
			 unsigned int l_idx, unsigned int tx,
			 unsigned int chan)
{
	struct osi_tx_ring *tx_ring = osi_dma->tx_ring[chan];
	struct osi_tx_desc *tx_desc = OSI_NULL;
	struct osd_dma_ops *ops = &osi_dma->osd_ops;
	unsigned int ctxt = 0, i = 0;

	if (f_idx == l_idx) {
		tx_desc = tx_ring->tx_desc + f_idx;
		ctxt = tx_desc->tdes3 & TDES3_CTXT;

		ops->printf(osi_dma, OSI_DEBUG_TYPE_DESC,
			    "%s [%02d %4p %04d %lx %s] = %#x:%#x:%#x:%#x\n",
			    (ctxt  == TDES3_CTXT) ? "C" : "N",
			    chan, tx_desc, f_idx,
			    (tx_ring->tx_desc_phy_addr + (f_idx * sizeof(struct osi_tx_desc))),
			    (tx == TX_DESC_DUMP_TX) ? "T_Q" : "T_D",
			    tx_desc->tdes3, tx_desc->tdes2,
			    tx_desc->tdes1, tx_desc->tdes0);
	} else {
		int cnt;

		if (f_idx > l_idx) {
			cnt = l_idx + TX_DESC_CNT - f_idx;
		} else {
			cnt = l_idx - f_idx;
		}

		for (i = f_idx; cnt >= 0; cnt--) {
			tx_desc = tx_ring->tx_desc + i;
			ctxt = tx_desc->tdes3 & TDES3_CTXT;

			ops->printf(osi_dma, OSI_DEBUG_TYPE_DESC,
				    "%s [%02d %4p %04d %lx %s] = %#x:%#x:%#x:%#x\n",
				    (ctxt  == TDES3_CTXT) ? "C" : "N",
				    chan, tx_desc, i,
				    (tx_ring->tx_desc_phy_addr + (i * sizeof(struct osi_tx_desc))),
				    (tx == TX_DESC_DUMP_TX) ? "T_Q" : "T_D",
				    tx_desc->tdes3, tx_desc->tdes2,
				    tx_desc->tdes1, tx_desc->tdes0);

			INCR_TX_DESC_INDEX(i, 1U);
		}
	}
}

/**
 * @brief desc_dump - Function to dump Tx/Rx descriptors
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] f_idx: First index to be dumped in Tx/Rx ring.
 * @param[in] l_idx: Last index to be dumped in Tx/Rx ring.
 * @param[in] flag: Flags to indicate Tx/Tx done or Rx.
 * @param[in] chan: DMA channel number
 *
 */
void desc_dump(struct osi_dma_priv_data *osi_dma, unsigned int f_idx,
	       unsigned int l_idx, unsigned int flag, unsigned int chan)
{
	switch (flag & TXRX_DESC_DUMP_MASK) {
	case TX_DESC_DUMP:
		tx_desc_dump(osi_dma, f_idx, l_idx,
			     (flag & TX_DESC_DUMP_MASK), chan);
		break;
	case RX_DESC_DUMP:
		rx_desc_dump(osi_dma, f_idx, chan);
		break;
	default:
		break;
	}
}
#endif /* OSI_DEBUG */
