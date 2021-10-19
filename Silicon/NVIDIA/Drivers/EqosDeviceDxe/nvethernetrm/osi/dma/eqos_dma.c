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

#include "../osi/common/common.h"
#include "dma_local.h"
#include "eqos_dma.h"
#include "../osi/common/type.h"

/**
 * @brief eqos_dma_safety_config - EQOS MAC DMA safety configuration
 */
static struct dma_func_safety eqos_dma_safety_config;

/**
 * @brief Write to safety critical register.
 *
 * @note
 * Algorithm:
 *  - Acquire RW lock, so that eqos_validate_dma_regs does not run while
 *    updating the safety critical register.
 *  - call osi_writel() to actually update the memory mapped register.
 *  - Store the same value in eqos_dma_safety_config->reg_val[idx], so that
 *    this latest value will be compared when eqos_validate_dma_regs is
 *    scheduled.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] val: Value to be written.
 * @param[in] addr: memory mapped register address to be written to.
 * @param[in] idx: Index of register corresponding to enum func_safety_dma_regs.
 *
 * @pre MAC has to be out of reset, and clocks supplied.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 */
static inline void eqos_dma_safety_writel(struct osi_dma_priv_data *osi_dma,
					  nveu32_t val, void *addr,
					  nveu32_t idx)
{
	struct dma_func_safety *config = &eqos_dma_safety_config;

	osi_lock_irq_enabled(&config->dma_safety_lock);
	osi_writela(osi_dma->osd, val, addr);
	config->reg_val[idx] = (val & config->reg_mask[idx]);
	osi_unlock_irq_enabled(&config->dma_safety_lock);
}

/**
 * @brief Initialize the eqos_dma_safety_config.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note
 * Algorithm:
 *  - Populate the list of safety critical registers and provide
 *  - the address of the register
 *  - Register mask (to ignore reserved/self-critical bits in the reg).
 *    See eqos_validate_dma_regs which can be invoked periodically to compare
 *    the last written value to this register vs the actual value read when
 *    eqos_validate_dma_regs is scheduled.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static void eqos_dma_safety_init(struct osi_dma_priv_data *osi_dma)
{
	struct dma_func_safety *config = &eqos_dma_safety_config;
	nveu8_t *base = (nveu8_t *)osi_dma->base;
	nveu32_t val;
	nveu32_t i, idx;

	/* Initialize all reg address to NULL, since we may not use
	 * some regs depending on the number of DMA chans enabled.
	 */
	for (i = EQOS_DMA_CH0_CTRL_IDX; i < EQOS_MAX_DMA_SAFETY_REGS; i++) {
		config->reg_addr[i] = OSI_NULL;
	}

	for (i = 0U; i < osi_dma->num_dma_chans; i++) {
		idx = osi_dma->dma_chans[i];
#if 0
		CHECK_CHAN_BOUND(idx);
#endif
		config->reg_addr[EQOS_DMA_CH0_CTRL_IDX + idx] = base +
						EQOS_DMA_CHX_CTRL(idx);
		config->reg_addr[EQOS_DMA_CH0_TX_CTRL_IDX + idx] = base +
						EQOS_DMA_CHX_TX_CTRL(idx);
		config->reg_addr[EQOS_DMA_CH0_RX_CTRL_IDX + idx] = base +
						EQOS_DMA_CHX_RX_CTRL(idx);
		config->reg_addr[EQOS_DMA_CH0_TDRL_IDX + idx] = base +
						EQOS_DMA_CHX_TDRL(idx);
		config->reg_addr[EQOS_DMA_CH0_RDRL_IDX + idx] = base +
						EQOS_DMA_CHX_RDRL(idx);
		config->reg_addr[EQOS_DMA_CH0_INTR_ENA_IDX + idx] = base +
						EQOS_DMA_CHX_INTR_ENA(idx);

		config->reg_mask[EQOS_DMA_CH0_CTRL_IDX + idx] =
						EQOS_DMA_CHX_CTRL_MASK;
		config->reg_mask[EQOS_DMA_CH0_TX_CTRL_IDX + idx] =
						EQOS_DMA_CHX_TX_CTRL_MASK;
		config->reg_mask[EQOS_DMA_CH0_RX_CTRL_IDX + idx] =
						EQOS_DMA_CHX_RX_CTRL_MASK;
		config->reg_mask[EQOS_DMA_CH0_TDRL_IDX + idx] =
						EQOS_DMA_CHX_TDRL_MASK;
		config->reg_mask[EQOS_DMA_CH0_RDRL_IDX + idx] =
						EQOS_DMA_CHX_RDRL_MASK;
		config->reg_mask[EQOS_DMA_CH0_INTR_ENA_IDX + idx] =
						EQOS_DMA_CHX_INTR_ENA_MASK;
	}

	/* Initialize current power-on-reset values of these registers. */
	for (i = EQOS_DMA_CH0_CTRL_IDX; i < EQOS_MAX_DMA_SAFETY_REGS; i++) {
		if (config->reg_addr[i] == OSI_NULL) {
			continue;
		}
		val = osi_readl((nveu8_t *)config->reg_addr[i]);
		config->reg_val[i] = val & config->reg_mask[i];
	}

	osi_lock_init(&config->dma_safety_lock);
}

/**
 * @brief eqos_disable_chan_tx_intr - Disables DMA Tx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 *                  memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - Mapping of physical IRQ line to DMA channel need to be maintained at
 *    OSDependent layer and pass corresponding channel number.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: Yes
 */
static void eqos_disable_chan_tx_intr(void *addr, nveu32_t chan)
{
	nveu32_t cntrl, status;

#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	/* Clear irq before disabling */
	status = osi_readl((nveu8_t *)addr +
			   EQOS_VIRT_INTR_CHX_STATUS(chan));
	if ((status & EQOS_VIRT_INTR_CHX_STATUS_TX) ==
	    EQOS_VIRT_INTR_CHX_STATUS_TX) {
		osi_writel(EQOS_DMA_CHX_STATUS_CLEAR_TX,
			   (nveu8_t *)addr + EQOS_DMA_CHX_STATUS(chan));
		osi_writel(EQOS_VIRT_INTR_CHX_STATUS_TX,
			   (nveu8_t *)addr +
			   EQOS_VIRT_INTR_CHX_STATUS(chan));
	}

	/* Disable the irq */
	cntrl = osi_readl((nveu8_t *)addr +
			  EQOS_VIRT_INTR_CHX_CNTRL(chan));
	cntrl &= ~EQOS_VIRT_INTR_CHX_CNTRL_TX;
	osi_writel(cntrl, (nveu8_t *)addr +
		   EQOS_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief eqos_enable_chan_tx_intr - Enable Tx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - Mapping of physical IRQ line to DMA channel need to be maintained at
 *    OSDependent layer and pass corresponding channel number.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 */
static void eqos_enable_chan_tx_intr(void *addr, nveu32_t chan)
{
	nveu32_t cntrl;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	cntrl = osi_readl((nveu8_t *)addr +
			  EQOS_VIRT_INTR_CHX_CNTRL(chan));
	cntrl |= EQOS_VIRT_INTR_CHX_CNTRL_TX;
	osi_writel(cntrl, (nveu8_t *)addr +
		   EQOS_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief eqos_disable_chan_rx_intr - Disable Rx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - Mapping of physical IRQ line to DMA channel need to be maintained at
 *    OSDependent layer and pass corresponding channel number.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: Yes
 */
static void eqos_disable_chan_rx_intr(void *addr, nveu32_t chan)
{
	nveu32_t cntrl, status;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	/* Clear irq before disabling */
	status = osi_readl((nveu8_t *)addr +
			   EQOS_VIRT_INTR_CHX_STATUS(chan));
	if ((status & EQOS_VIRT_INTR_CHX_STATUS_RX) ==
	     EQOS_VIRT_INTR_CHX_STATUS_RX) {
		osi_writel(EQOS_DMA_CHX_STATUS_CLEAR_RX,
			   (nveu8_t *)addr + EQOS_DMA_CHX_STATUS(chan));
		osi_writel(EQOS_VIRT_INTR_CHX_STATUS_RX,
			   (nveu8_t *)addr +
			   EQOS_VIRT_INTR_CHX_STATUS(chan));
	}

	/* Disable irq */
	cntrl = osi_readl((nveu8_t *)addr +
			  EQOS_VIRT_INTR_CHX_CNTRL(chan));
	cntrl &= ~EQOS_VIRT_INTR_CHX_CNTRL_RX;
	osi_writel(cntrl, (nveu8_t *)addr +
		   EQOS_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief eqos_enable_chan_rx_intr - Enable Rx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 */
static void eqos_enable_chan_rx_intr(void *addr, nveu32_t chan)
{
	nveu32_t cntrl;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	cntrl = osi_readl((nveu8_t *)addr +
			  EQOS_VIRT_INTR_CHX_CNTRL(chan));
	cntrl |= EQOS_VIRT_INTR_CHX_CNTRL_RX;
	osi_writel(cntrl, (nveu8_t *)addr +
		   EQOS_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief eqos_set_tx_ring_len - Set DMA Tx ring length.
 *
 * @note
 * Algorithm:
 *  - Set DMA Tx channel ring length for specific channel.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA Tx channel number.
 * @param[in] len: Length.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static void eqos_set_tx_ring_len(struct osi_dma_priv_data *osi_dma,
				 nveu32_t chan,
				 nveu32_t len)
{
	void *addr = osi_dma->base;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	eqos_dma_safety_writel(osi_dma, len, (nveu8_t *)addr +
			       EQOS_DMA_CHX_TDRL(chan),
			       EQOS_DMA_CH0_TDRL_IDX + chan);
}

/**
 * @brief eqos_set_tx_ring_start_addr - Set DMA Tx ring base address.
 *
 * @note
 * Algorithm:
 *  - Sets DMA Tx ring base address for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 * @param[in] tx_desc: Tx desc base address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static void eqos_set_tx_ring_start_addr(void *addr, nveu32_t chan,
					nveu64_t tx_desc)
{
	nveu64_t tmp;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	tmp = H32(tx_desc);
	if (tmp < UINT_MAX) {
		osi_writel((nveu32_t)tmp, (nveu8_t *)addr +
			   EQOS_DMA_CHX_TDLH(chan));
	}

	tmp = L32(tx_desc);
	if (tmp < UINT_MAX) {
		osi_writel((nveu32_t)tmp, (nveu8_t *)addr +
			   EQOS_DMA_CHX_TDLA(chan));
	}
}

/**
 * @brief eqos_update_tx_tailptr - Updates DMA Tx ring tail pointer.
 *
 * @note
 * Algorithm:
 *  - Updates DMA Tx ring tail pointer for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 * @param[in] tailptr: DMA Tx ring tail pointer.
 *
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void eqos_update_tx_tailptr(void *addr, nveu32_t chan,
				   nveu64_t tailptr)
{
	nveu64_t tmp;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	tmp = L32(tailptr);
	if (tmp < UINT_MAX) {
		osi_writel((nveu32_t)tmp, (nveu8_t *)addr +
			   EQOS_DMA_CHX_TDTP(chan));
	}
}

/**
 * @brief eqos_set_rx_ring_len - Set Rx channel ring length.
 *
 * @note
 * Algorithm:
 *  - Sets DMA Rx channel ring length for specific DMA channel.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] len: Length
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static void eqos_set_rx_ring_len(struct osi_dma_priv_data *osi_dma,
				 nveu32_t chan,
				 nveu32_t len)
{
	void *addr = osi_dma->base;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	eqos_dma_safety_writel(osi_dma, len, (nveu8_t *)addr +
			       EQOS_DMA_CHX_RDRL(chan),
			       EQOS_DMA_CH0_RDRL_IDX + chan);
}

/**
 * @brief eqos_set_rx_ring_start_addr - Set DMA Rx ring base address.
 *
 * @note
 * Algorithm:
 *  - Sets DMA Rx channel ring base address.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] rx_desc: DMA Rx desc base address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static void eqos_set_rx_ring_start_addr(void *addr, nveu32_t chan,
					nveu64_t rx_desc)
{
	nveu64_t tmp;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	tmp = H32(rx_desc);
	if (tmp < UINT_MAX) {
		osi_writel((nveu32_t)tmp, (nveu8_t *)addr +
			   EQOS_DMA_CHX_RDLH(chan));
	}

	tmp = L32(rx_desc);
	if (tmp < UINT_MAX) {
		osi_writel((nveu32_t)tmp, (nveu8_t *)addr +
			   EQOS_DMA_CHX_RDLA(chan));
	}
}

/**
 * @brief eqos_update_rx_tailptr - Update Rx ring tail pointer
 *
 * @note
 * Algorithm:
 *  - Updates DMA Rx channel tail pointer for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] tailptr: Tail pointer
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 */
static void eqos_update_rx_tailptr(void *addr, nveu32_t chan,
				   nveu64_t tailptr)
{
	nveu64_t tmp;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	tmp = L32(tailptr);
	if (tmp < UINT_MAX) {
		osi_writel((nveu32_t)tmp, (nveu8_t *)addr +
			   EQOS_DMA_CHX_RDTP(chan));
	}
}

/**
 * @brief eqos_start_dma - Start DMA.
 *
 * @note
 * Algorithm:
 *  - Start Tx and Rx DMA for specific channel.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA Tx/Rx channel number.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static void eqos_start_dma(struct osi_dma_priv_data *osi_dma, nveu32_t chan)
{
	nveu32_t val;
	void *addr = osi_dma->base;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	/* start Tx DMA */
	val = osi_readla(osi_dma->osd,
			 (nveu8_t *)addr + EQOS_DMA_CHX_TX_CTRL(chan));
	val |= OSI_BIT(0);
	eqos_dma_safety_writel(osi_dma, val, (nveu8_t *)addr +
			       EQOS_DMA_CHX_TX_CTRL(chan),
			       EQOS_DMA_CH0_TX_CTRL_IDX + chan);

	/* start Rx DMA */
	val = osi_readla(osi_dma->osd,
			 (nveu8_t *)addr + EQOS_DMA_CHX_RX_CTRL(chan));
	val |= OSI_BIT(0);
	val &= ~OSI_BIT(31);
	eqos_dma_safety_writel(osi_dma, val, (nveu8_t *)addr +
			       EQOS_DMA_CHX_RX_CTRL(chan),
			       EQOS_DMA_CH0_RX_CTRL_IDX + chan);
}

/**
 * @brief eqos_stop_dma - Stop DMA.
 *
 * @note
 * Algorithm:
 *  - Start Tx and Rx DMA for specific channel.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA Tx/Rx channel number.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: No
 * - De-initialization: Yes
 */
static void eqos_stop_dma(struct osi_dma_priv_data *osi_dma, nveu32_t chan)
{
	nveu32_t val;
	void *addr = osi_dma->base;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	/* stop Tx DMA */
	val = osi_readla(osi_dma->osd,
			 (nveu8_t *)addr + EQOS_DMA_CHX_TX_CTRL(chan));
	val &= ~OSI_BIT(0);
	eqos_dma_safety_writel(osi_dma, val, (nveu8_t *)addr +
			       EQOS_DMA_CHX_TX_CTRL(chan),
			       EQOS_DMA_CH0_TX_CTRL_IDX + chan);

	/* stop Rx DMA */
	val = osi_readla(osi_dma->osd,
			 (nveu8_t *)addr + EQOS_DMA_CHX_RX_CTRL(chan));
	val &= ~OSI_BIT(0);
	val |= OSI_BIT(31);
	eqos_dma_safety_writel(osi_dma, val, (nveu8_t *)addr +
			       EQOS_DMA_CHX_RX_CTRL(chan),
			       EQOS_DMA_CH0_RX_CTRL_IDX + chan);
}

/**
 * @brief eqos_configure_dma_channel - Configure DMA channel
 *
 * @note
 * Algorithm:
 *  - This takes care of configuring the  below
 *    parameters for the DMA channel
 *   - Enabling DMA channel interrupts
 *   - Enable 8xPBL mode
 *   - Program Tx, Rx PBL
 *   - Enable TSO if HW supports
 *   - Program Rx Watchdog timer
 *
 * @param[in] chan: DMA channel number that need to be configured.
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @pre MAC has to be out of reset.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static void eqos_configure_dma_channel(nveu32_t chan,
				       struct osi_dma_priv_data *osi_dma)
{
	nveu32_t value;
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	/* enable DMA channel interrupts */
	/* Enable TIE and TBUE */
	/* TIE - Transmit Interrupt Enable */
	/* TBUE - Transmit Buffer Unavailable Enable */
	/* RIE - Receive Interrupt Enable */
	/* RBUE - Receive Buffer Unavailable Enable  */
	/* AIE - Abnormal Interrupt Summary Enable */
	/* NIE - Normal Interrupt Summary Enable */
	/* FBE - Fatal Bus Error Enable */
	value = osi_readl((nveu8_t *)osi_dma->base +
			  EQOS_DMA_CHX_INTR_ENA(chan));
	if (osi_dma->use_virtualization == OSI_DISABLE) {
		value |= EQOS_DMA_CHX_INTR_TBUE |
			 EQOS_DMA_CHX_INTR_RBUE;
	}

	value |= EQOS_DMA_CHX_INTR_TIE | EQOS_DMA_CHX_INTR_RIE |
		 EQOS_DMA_CHX_INTR_FBEE | EQOS_DMA_CHX_INTR_AIE |
		 EQOS_DMA_CHX_INTR_NIE;
	/* For multi-irqs to work nie needs to be disabled */
	value &= ~(EQOS_DMA_CHX_INTR_NIE);
	eqos_dma_safety_writel(osi_dma, value, (nveu8_t *)osi_dma->base +
			       EQOS_DMA_CHX_INTR_ENA(chan),
			       EQOS_DMA_CH0_INTR_ENA_IDX + chan);

	/* Enable 8xPBL mode */
	value = osi_readl((nveu8_t *)osi_dma->base +
			  EQOS_DMA_CHX_CTRL(chan));
	value |= EQOS_DMA_CHX_CTRL_PBLX8;
	eqos_dma_safety_writel(osi_dma, value, (nveu8_t *)osi_dma->base +
			       EQOS_DMA_CHX_CTRL(chan),
			       EQOS_DMA_CH0_CTRL_IDX + chan);

	/* Configure DMA channel Transmit control register */
	value = osi_readl((nveu8_t *)osi_dma->base +
			  EQOS_DMA_CHX_TX_CTRL(chan));
	/* Enable OSF mode */
	value |= EQOS_DMA_CHX_TX_CTRL_OSF;
	/* TxPBL = 32*/
	value |= EQOS_DMA_CHX_TX_CTRL_TXPBL_RECOMMENDED;
	/* enable TSO by default if HW supports */
	value |= EQOS_DMA_CHX_TX_CTRL_TSE;

	eqos_dma_safety_writel(osi_dma, value, (nveu8_t *)osi_dma->base +
			       EQOS_DMA_CHX_TX_CTRL(chan),
			       EQOS_DMA_CH0_TX_CTRL_IDX + chan);

	/* Configure DMA channel Receive control register */
	/* Select Rx Buffer size.  Needs to be rounded up to next multiple of
	 * bus width
	 */
	value = osi_readl((nveu8_t *)osi_dma->base +
			  EQOS_DMA_CHX_RX_CTRL(chan));

	/* clear previous Rx buffer size */
	value &= ~EQOS_DMA_CHX_RBSZ_MASK;

	value |= (osi_dma->rx_buf_len << EQOS_DMA_CHX_RBSZ_SHIFT);
	/* RXPBL = 12 */
	value |= EQOS_DMA_CHX_RX_CTRL_RXPBL_RECOMMENDED;
	eqos_dma_safety_writel(osi_dma, value, (nveu8_t *)osi_dma->base +
			       EQOS_DMA_CHX_RX_CTRL(chan),
			       EQOS_DMA_CH0_RX_CTRL_IDX + chan);

	/* Set Receive Interrupt Watchdog Timer Count */
	/* conversion of usec to RWIT value
	 * Eg: System clock is 125MHz, each clock cycle would then be 8ns
	 * For value 0x1 in RWT, device would wait for 512 clk cycles with
	 * RWTU as 0x1,
	 * ie, (8ns x 512) => 4.096us (rounding off to 4us)
	 * So formula with above values is,ret = usec/4
	 */
	if ((osi_dma->use_riwt == OSI_ENABLE) &&
	    (osi_dma->rx_riwt < UINT_MAX)) {
		value = osi_readl((nveu8_t *)osi_dma->base +
				  EQOS_DMA_CHX_RX_WDT(chan));
		/* Mask the RWT and RWTU value */
		value &= ~(EQOS_DMA_CHX_RX_WDT_RWT_MASK |
			   EQOS_DMA_CHX_RX_WDT_RWTU_MASK);
		/* Conversion of usec to Rx Interrupt Watchdog Timer Count */
		value |= ((osi_dma->rx_riwt *
			 (EQOS_AXI_CLK_FREQ / OSI_ONE_MEGA_HZ)) /
			 EQOS_DMA_CHX_RX_WDT_RWTU) &
			 EQOS_DMA_CHX_RX_WDT_RWT_MASK;
		value |= EQOS_DMA_CHX_RX_WDT_RWTU_512_CYCLE;
		osi_writel(value, (nveu8_t *)osi_dma->base +
			   EQOS_DMA_CHX_RX_WDT(chan));
	}
}

/**
 * @brief eqos_init_dma_channel - DMA channel INIT
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t eqos_init_dma_channel(struct osi_dma_priv_data *osi_dma)
{
	nveu32_t chinx;

	eqos_dma_safety_init(osi_dma);

	/* configure EQOS DMA channels */
	for (chinx = 0; chinx < osi_dma->num_dma_chans; chinx++) {
		eqos_configure_dma_channel(osi_dma->dma_chans[chinx], osi_dma);
	}

	return 0;
}

/**
 * @brief eqos_set_rx_buf_len - Set Rx buffer length
 *        Sets the Rx buffer length based on the new MTU size set.
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - osi_dma->mtu need to be filled with current MTU size <= 9K
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void eqos_set_rx_buf_len(struct osi_dma_priv_data *osi_dma)
{
	nveu32_t rx_buf_len = 0U;

	/* Add Ethernet header + VLAN header + NET IP align size to MTU */
	if (osi_dma->mtu <= OSI_MAX_MTU_SIZE) {
		rx_buf_len = osi_dma->mtu + OSI_ETH_HLEN + NV_VLAN_HLEN +
			     OSI_NET_IP_ALIGN;
	} else {
		rx_buf_len = OSI_MAX_MTU_SIZE + OSI_ETH_HLEN + NV_VLAN_HLEN +
			     OSI_NET_IP_ALIGN;
	}

	/* Buffer alignment */
	osi_dma->rx_buf_len = ((rx_buf_len + (EQOS_AXI_BUS_WIDTH - 1U)) &
			       ~(EQOS_AXI_BUS_WIDTH - 1U));
}

#ifndef OSI_STRIPPED_LIB
/**
 * @brief Read-validate HW registers for functional safety.
 *
 * @note
 * Algorithm:
 *  - Reads pre-configured list of MAC/MTL configuration registers
 *    and compares with last written value for any modifications.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @pre
 *  - MAC has to be out of reset.
 *  - osi_hw_dma_init has to be called. Internally this would initialize
 *    the safety_config (see osi_dma_priv_data) based on MAC version and
 *    which specific registers needs to be validated periodically.
 *  - Invoke this call if (osi_dma_priv_data->safety_config != OSI_NULL)
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
static nve32_t eqos_validate_dma_regs(struct osi_dma_priv_data *osi_dma)
{
	struct dma_func_safety *config =
		(struct dma_func_safety *)osi_dma->safety_config;
	nveu32_t cur_val;
	nveu32_t i;

	osi_lock_irq_enabled(&config->dma_safety_lock);
	for (i = EQOS_DMA_CH0_CTRL_IDX; i < EQOS_MAX_DMA_SAFETY_REGS; i++) {
		if (config->reg_addr[i] == OSI_NULL) {
			continue;
		}

		cur_val = osi_readl((nveu8_t *)config->reg_addr[i]);
		cur_val &= config->reg_mask[i];

		if (cur_val == config->reg_val[i]) {
			continue;
		} else {
			/* Register content differs from what was written.
			 * Return error and let safety manager (NVGaurd etc.)
			 * take care of corrective action.
			 */
			osi_unlock_irq_enabled(&config->dma_safety_lock);
			return -1;
		}
	}
	osi_unlock_irq_enabled(&config->dma_safety_lock);

	return 0;
}

/**
 * @brief eqos_config_slot - Configure slot Checking for DMA channel
 *
 * @note
 * Algorithm:
 *  - Set/Reset the slot function of DMA channel based on given inputs
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA channel number to enable slot function
 * @param[in] set: flag for set/reset with value OSI_ENABLE/OSI_DISABLE
 * @param[in] interval: slot interval from 0usec to 4095usec
 *
 * @pre
 *  - MAC should be init and started. see osi_start_mac()
 *  - OSD should be initialized
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 */
static void eqos_config_slot(struct osi_dma_priv_data *osi_dma,
			     nveu32_t chan,
			     nveu32_t set,
			     nveu32_t interval)
{
	nveu32_t value;
	nveu32_t intr;

#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	if (set == OSI_ENABLE) {
		/* Program SLOT CTRL register SIV and set ESC bit */
		value = osi_readl((nveu8_t *)osi_dma->base +
				  EQOS_DMA_CHX_SLOT_CTRL(chan));
		value &= ~EQOS_DMA_CHX_SLOT_SIV_MASK;
		/* remove overflow bits of interval */
		intr = interval & EQOS_DMA_CHX_SLOT_SIV_MASK;
		value |= (intr << EQOS_DMA_CHX_SLOT_SIV_SHIFT);
		/* Set ESC bit */
		value |= EQOS_DMA_CHX_SLOT_ESC;
		osi_writel(value, (nveu8_t *)osi_dma->base +
			   EQOS_DMA_CHX_SLOT_CTRL(chan));

	} else {
		/* Clear ESC bit of SLOT CTRL register */
		value = osi_readl((nveu8_t *)osi_dma->base +
				  EQOS_DMA_CHX_SLOT_CTRL(chan));
		value &= ~EQOS_DMA_CHX_SLOT_ESC;
		osi_writel(value, (nveu8_t *)osi_dma->base +
			   EQOS_DMA_CHX_SLOT_CTRL(chan));
	}
}
#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief eqos_get_global_dma_status - Gets DMA status.
 *
 * Algorithm: Returns global DMA Tx/Rx interrupt status
 *
 * @param[in] addr: MAC base address.
 *
 * @note
 *	Dependencies: None.
 *	Protection: None.
 *
 * @retval status
 */
static nveu32_t eqos_get_global_dma_status(void *addr)
{
	return osi_readl((nveu8_t *)addr + EQOS_GLOBAL_DMA_STATUS);
}

/**
 * @brief eqos_clear_vm_tx_intr - Handle VM Tx interrupt
 *
 * @param[in] addr: MAC base address.
 * @param[in] chan: DMA Tx channel number.
 *
 * Algorithm: Clear Tx interrupt source at DMA and wrapper level.
 *
 * @note
 *	Dependencies: None.
 *	Protection: None.
 * @retval None.
 */
static void eqos_clear_vm_tx_intr(void *addr, nveu32_t chan)
{
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	osi_writel(EQOS_DMA_CHX_STATUS_CLEAR_TX,
		   (nveu8_t *)addr + EQOS_DMA_CHX_STATUS(chan));
	osi_writel(EQOS_VIRT_INTR_CHX_STATUS_TX,
		   (nveu8_t *)addr + EQOS_VIRT_INTR_CHX_STATUS(chan));

	eqos_disable_chan_tx_intr(addr, chan);
}

/**
 * @brief eqos_clear_vm_rx_intr - Handle VM Rx interrupt
 *
 * @param[in] addr: MAC base address.
 * @param[in] chan: DMA Rx channel number.
 *
 * Algorithm: Clear Rx interrupt source at DMA and wrapper level.
 *
 * @note
 *	Dependencies: None.
 *	Protection: None.
 *
 * @retval None.
 */
static void eqos_clear_vm_rx_intr(void *addr, nveu32_t chan)
{
#if 0
	CHECK_CHAN_BOUND(chan);
#endif
	osi_writel(EQOS_DMA_CHX_STATUS_CLEAR_RX,
		   (nveu8_t *)addr + EQOS_DMA_CHX_STATUS(chan));
	osi_writel(EQOS_VIRT_INTR_CHX_STATUS_RX,
		   (nveu8_t *)addr + EQOS_VIRT_INTR_CHX_STATUS(chan));

	eqos_disable_chan_rx_intr(addr, chan);
}

/**
 * @brief eqos_get_dma_safety_config - EQOS get DMA safety configuration
 */
void *eqos_get_dma_safety_config(void)
{
	return &eqos_dma_safety_config;
}

/**
 * @brief eqos_init_dma_chan_ops - Initialize EQOS DMA operations.
 *
 * @param[in] ops: DMA channel operations pointer.
 */
void eqos_init_dma_chan_ops(struct dma_chan_ops *ops)
{
	ops->set_tx_ring_len = eqos_set_tx_ring_len;
	ops->set_rx_ring_len = eqos_set_rx_ring_len;
	ops->set_tx_ring_start_addr = eqos_set_tx_ring_start_addr;
	ops->set_rx_ring_start_addr = eqos_set_rx_ring_start_addr;
	ops->update_tx_tailptr = eqos_update_tx_tailptr;
	ops->update_rx_tailptr = eqos_update_rx_tailptr;
	ops->disable_chan_tx_intr = eqos_disable_chan_tx_intr;
	ops->enable_chan_tx_intr = eqos_enable_chan_tx_intr;
	ops->disable_chan_rx_intr = eqos_disable_chan_rx_intr;
	ops->enable_chan_rx_intr = eqos_enable_chan_rx_intr;
	ops->start_dma = eqos_start_dma;
	ops->stop_dma = eqos_stop_dma;
	ops->init_dma_channel = eqos_init_dma_channel;
	ops->set_rx_buf_len = eqos_set_rx_buf_len;
#ifndef OSI_STRIPPED_LIB
	ops->validate_regs = eqos_validate_dma_regs;
	ops->config_slot = eqos_config_slot;
#endif /* !OSI_STRIPPED_LIB */
	ops->get_global_dma_status = eqos_get_global_dma_status;
	ops->clear_vm_tx_intr = eqos_clear_vm_tx_intr;
	ops->clear_vm_rx_intr = eqos_clear_vm_rx_intr;
}
