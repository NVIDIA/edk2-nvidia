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

#include <osi_common.h>
#include "osi_dma_local.h"
#include "eqos_dma.h"

/**
 * @brief eqos_dma_safety_config - EQOS MAC DMA safety configuration
 */
static struct dma_func_safety eqos_dma_safety_config;

/**
 * @brief Write to safety critical register.
 *
 * Algorithm:
 *	1) Acquire RW lock, so that eqos_validate_dma_regs does not run while
 *	updating the safety critical register.
 *	2) call osi_writel() to actually update the memory mapped register.
 *	3) Store the same value in eqos_dma_safety_config->reg_val[idx], so that
 *	this latest value will be compared when eqos_validate_dma_regs is
 *	scheduled.
 *
 * @param[in] val: Value to be written.
 * @param[in] addr: memory mapped register address to be written to.
 * @param[in] idx: Index of register corresponding to enum func_safety_dma_regs.
 *
 * @note MAC has to be out of reset, and clocks supplied.
 */
static inline void eqos_dma_safety_writel(unsigned int val, void *addr,
					  unsigned int idx)
{
	struct dma_func_safety *config = &eqos_dma_safety_config;

	osi_lock_irq_enabled(&config->dma_safety_lock);
	osi_writel(val, addr);
	config->reg_val[idx] = (val & config->reg_mask[idx]);
	osi_unlock_irq_enabled(&config->dma_safety_lock);
}

/**
 * @brief Initialize the eqos_dma_safety_config.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * Algorithm: Populate the list of safety critical registers and provide
 *	1) the address of the register
 *	2) Register mask (to ignore reserved/self-critical bits in the reg).
 *	See eqos_validate_dma_regs which can be ivoked periodically to compare
 *	the last written value to this register vs the actual value read when
 *	eqos_validate_dma_regs is scheduled.
 */
static void eqos_dma_safety_init(struct osi_dma_priv_data *osi_dma)
{
	struct dma_func_safety *config = &eqos_dma_safety_config;
	unsigned char *base = (unsigned char *)osi_dma->base;
	unsigned int val;
	unsigned int i, idx;

	/* Initialize all reg address to NULL, since we may not use
	 * some regs depending on the number of DMA chans enabled.
	 */
	for (i = EQOS_DMA_CH0_CTRL_IDX; i < EQOS_MAX_DMA_SAFETY_REGS; i++) {
		config->reg_addr[i] = OSI_NULL;
	}

	for (i = 0U; i < osi_dma->num_dma_chans; i++) {
		idx = osi_dma->dma_chans[i];
		CHECK_CHAN_BOUND(idx);
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
		val = osi_readl((unsigned char *)config->reg_addr[i]);
		config->reg_val[i] = val & config->reg_mask[i];
	}

	osi_lock_init(&config->dma_safety_lock);
}

/**
 * @brief Read-validate HW registers for functional safety.
 *
 * Algorithm: Reads pre-configured list of MAC/MTL configuration registers
 *	and compares with last written value for any modifications.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note
 *	1) MAC has to be out of reset.
 *	2) osi_hw_dma_init has to be called. Internally this would initialize
 *	the safety_config (see osi_dma_priv_data) based on MAC version and
 *	which specific registers needs to be validated periodically.
 *	3) Invoke this call iff (osi_dma_priv_data->safety_config != OSI_NULL)
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int eqos_validate_dma_regs(struct osi_dma_priv_data *osi_dma)
{
	struct dma_func_safety *config =
		(struct dma_func_safety *)osi_dma->safety_config;
	unsigned int cur_val;
	unsigned int i;

	osi_lock_irq_enabled(&config->dma_safety_lock);
	for (i = EQOS_DMA_CH0_CTRL_IDX; i < EQOS_MAX_DMA_SAFETY_REGS; i++) {
		if (config->reg_addr[i] == OSI_NULL) {
			continue;
		}

		/* FIXME
		 * QNX OSD currently overwrites following registers and
		 * therefore validation fails using this API. Add an
		 * exception for following registers until QNX OSD completely
		 * moves to common library.
		 */
		if ((i == EQOS_DMA_CH0_TDRL_IDX) ||
			(i == EQOS_DMA_CH0_RDRL_IDX))
		{
			continue;
		}

		cur_val = osi_readl((unsigned char *)config->reg_addr[i]);
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
 * @brief eqos_disable_chan_tx_intr - Disables DMA Tx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	 3) Mapping of physical IRQ line to DMA channel need to be maintained at
 *	 OSDependent layer and pass corresponding channel number.
 */
static void eqos_disable_chan_tx_intr(void *addr, unsigned int chan)
{
	unsigned int cntrl;

	CHECK_CHAN_BOUND(chan);

	cntrl = osi_readl((unsigned char *)addr +
			  EQOS_VIRT_INTR_CHX_CNTRL(chan));
	cntrl &= ~EQOS_VIRT_INTR_CHX_CNTRL_TX;
	osi_writel(cntrl, (unsigned char *)addr +
		   EQOS_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief eqos_enable_chan_tx_intr - Enable Tx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	 3) Mapping of physical IRQ line to DMA channel need to be maintained at
 *	 OSDependent layer and pass corresponding channel number.
 */
static void eqos_enable_chan_tx_intr(void *addr, unsigned int chan)
{
	unsigned int cntrl;

	CHECK_CHAN_BOUND(chan);

	cntrl = osi_readl((unsigned char *)addr +
			  EQOS_VIRT_INTR_CHX_CNTRL(chan));
	cntrl |= EQOS_VIRT_INTR_CHX_CNTRL_TX;
	osi_writel(cntrl, (unsigned char *)addr +
		   EQOS_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief eqos_disable_chan_rx_intr - Disable Rx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	 3) Mapping of physical IRQ line to DMA channel need to be maintained at
 *	 OSDependent layer and pass corresponding channel number.
 */
static void eqos_disable_chan_rx_intr(void *addr, unsigned int chan)
{
	unsigned int cntrl;

	CHECK_CHAN_BOUND(chan);

	cntrl = osi_readl((unsigned char *)addr +
			  EQOS_VIRT_INTR_CHX_CNTRL(chan));
	cntrl &= ~EQOS_VIRT_INTR_CHX_CNTRL_RX;
	osi_writel(cntrl, (unsigned char *)addr +
		   EQOS_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief eqos_enable_chan_rx_intr - Enable Rx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 */
static void eqos_enable_chan_rx_intr(void *addr, unsigned int chan)
{
	unsigned int cntrl;

	CHECK_CHAN_BOUND(chan);

	cntrl = osi_readl((unsigned char *)addr +
			  EQOS_VIRT_INTR_CHX_CNTRL(chan));
	cntrl |= EQOS_VIRT_INTR_CHX_CNTRL_RX;
	osi_writel(cntrl, (unsigned char *)addr +
		   EQOS_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief eqos_clear_tx_intr - Handle EQOS DMA Tx channel interrupts.
 *
 * Algorithm: Clear DMA Tx interrupt source at wrapper and DMA level.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	3) Mapping of physical IRQ line to DMA channel need to be maintained at
 *	OSDependent layer and pass corresponding channel number.
 */
static void eqos_clear_tx_intr(void *addr, unsigned int chan)
{
	unsigned int status;

	CHECK_CHAN_BOUND(chan);

	status = osi_readl((unsigned char *)addr +
			   EQOS_VIRT_INTR_CHX_STATUS(chan));
	if ((status & EQOS_VIRT_INTR_CHX_STATUS_TX) == 1U) {
		osi_writel(EQOS_DMA_CHX_STATUS_CLEAR_TX,
			   (unsigned char *)addr + EQOS_DMA_CHX_STATUS(chan));
		osi_writel(EQOS_VIRT_INTR_CHX_STATUS_TX,
			   (unsigned char *)addr +
			   EQOS_VIRT_INTR_CHX_STATUS(chan));
	}
}

/**
 * @brief eqos_clear_rx_intr - Handles DMA Rx channel interrupts.
 *
 * Algorithm: Clear DMA Rx interrupt source at wrapper and DMA level.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	3) Mapping of physical IRQ line to DMA channel need to be maintained at
 *	OSDependent layer and pass corresponding channel number.
 */
static void eqos_clear_rx_intr(void *addr, unsigned int chan)
{
	unsigned int status;

	CHECK_CHAN_BOUND(chan);

	status = osi_readl((unsigned char *)addr +
			   EQOS_VIRT_INTR_CHX_STATUS(chan));
	if ((status & EQOS_VIRT_INTR_CHX_STATUS_RX) == 2U) {
		osi_writel(EQOS_DMA_CHX_STATUS_CLEAR_RX,
			   (unsigned char *)addr + EQOS_DMA_CHX_STATUS(chan));
		osi_writel(EQOS_VIRT_INTR_CHX_STATUS_RX,
			   (unsigned char *)addr +
			   EQOS_VIRT_INTR_CHX_STATUS(chan));
	}
}

/**
 * @brief eqos_set_tx_ring_len - Set DMA Tx ring length.
 *
 * Algorithm: Set DMA Tx channel ring length for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 * @param[in] len: Length.
 */
static void eqos_set_tx_ring_len(void *addr, unsigned int chan,
				 unsigned int len)
{
	CHECK_CHAN_BOUND(chan);
	eqos_dma_safety_writel(len, (unsigned char *)addr +
			       EQOS_DMA_CHX_TDRL(chan),
			       EQOS_DMA_CH0_TDRL_IDX + chan);
}

/**
 * @brief eqos_set_tx_ring_start_addr - Set DMA Tx ring base address.
 *
 * Algorithm: Sets DMA Tx ring base address for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 * @param[in] tx_desc: Tx desc base addess.
 */
static void eqos_set_tx_ring_start_addr(void *addr, unsigned int chan,
					unsigned long tx_desc)
{
	unsigned long tmp;

	CHECK_CHAN_BOUND(chan);

	tmp = H32(tx_desc);
	if (tmp < UINT_MAX) {
		osi_writel((unsigned int)tmp, (unsigned char *)addr +
			   EQOS_DMA_CHX_TDLH(chan));
	}

	tmp = L32(tx_desc);
	if (tmp < UINT_MAX) {
		osi_writel((unsigned int)tmp, (unsigned char *)addr +
			   EQOS_DMA_CHX_TDLA(chan));
	}
}

/**
 * @brief eqos_update_tx_tailptr - Updates DMA Tx ring tail pointer.
 *
 * Algorithm: Updates DMA Tx ring tail pointer for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 * @param[in] tailptr: DMA Tx ring tail pointer.
 *
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 */
static void eqos_update_tx_tailptr(void *addr, unsigned int chan,
				   unsigned long tailptr)
{
	unsigned long tmp;

	CHECK_CHAN_BOUND(chan);

	tmp = L32(tailptr);
	if (tmp < UINT_MAX) {
		osi_writel((unsigned int)tmp, (unsigned char *)addr +
			   EQOS_DMA_CHX_TDTP(chan));
	}
}

/**
 * @brief eqos_set_rx_ring_len - Set Rx channel ring length.
 *
 * Algorithm: Sets DMA Rx channel ring length for specific DMA channel.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] len: Length
 */
static void eqos_set_rx_ring_len(void *addr, unsigned int chan,
				 unsigned int len)
{
	CHECK_CHAN_BOUND(chan);
	eqos_dma_safety_writel(len, (unsigned char *)addr +
			       EQOS_DMA_CHX_RDRL(chan),
			       EQOS_DMA_CH0_RDRL_IDX + chan);
}

/**
 * @brief eqos_set_rx_ring_start_addr - Set DMA Rx ring base address.
 *
 * Algorithm: Sets DMA Rx channel ring base address.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] tx_desc: DMA Rx desc base address.
 */
static void eqos_set_rx_ring_start_addr(void *addr, unsigned int chan,
					unsigned long tx_desc)
{
	unsigned long tmp;

	CHECK_CHAN_BOUND(chan);

	tmp = H32(tx_desc);
	if (tmp < UINT_MAX) {
		osi_writel((unsigned int)tmp, (unsigned char *)addr +
			   EQOS_DMA_CHX_RDLH(chan));
	}

	tmp = L32(tx_desc);
	if (tmp < UINT_MAX) {
		osi_writel((unsigned int)tmp, (unsigned char *)addr +
			   EQOS_DMA_CHX_RDLA(chan));
	}
}

/**
 * @brief eqos_update_rx_tailptr - Update Rx ring tail pointer
 *
 * Algorithm: Updates DMA Rx channel tail pointer for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] tailptr: Tail pointer
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 */
static void eqos_update_rx_tailptr(void *addr, unsigned int chan,
				   unsigned long tailptr)
{
	unsigned long tmp;

	CHECK_CHAN_BOUND(chan);

	tmp = L32(tailptr);
	if (tmp < UINT_MAX) {
		osi_writel((unsigned int)tmp, (unsigned char *)addr +
			   EQOS_DMA_CHX_RDTP(chan));
	}
}

/**
 * @brief eqos_start_dma - Start DMA.
 *
 * Algorithm: Start Tx and Rx DMA for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx/Rx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 */
static void eqos_start_dma(void *addr, unsigned int chan)
{
	unsigned int val;

	CHECK_CHAN_BOUND(chan);

	/* start Tx DMA */
	val = osi_readl((unsigned char *)addr + EQOS_DMA_CHX_TX_CTRL(chan));
	val |= OSI_BIT(0);
	eqos_dma_safety_writel(val, (unsigned char *)addr +
			       EQOS_DMA_CHX_TX_CTRL(chan),
			       EQOS_DMA_CH0_TX_CTRL_IDX + chan);

	/* start Rx DMA */
	val = osi_readl((unsigned char *)addr + EQOS_DMA_CHX_RX_CTRL(chan));
	val |= OSI_BIT(0);
	eqos_dma_safety_writel(val, (unsigned char *)addr +
			       EQOS_DMA_CHX_RX_CTRL(chan),
			       EQOS_DMA_CH0_RX_CTRL_IDX + chan);
}

/**
 * @brief eqos_stop_dma - Stop DMA.
 *
 * Algorithm: Start Tx and Rx DMA for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 * 	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx/Rx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 */
static void eqos_stop_dma(void *addr, unsigned int chan)
{
	unsigned int val;

	CHECK_CHAN_BOUND(chan);

	/* stop Tx DMA */
	val = osi_readl((unsigned char *)addr + EQOS_DMA_CHX_TX_CTRL(chan));
	val &= ~OSI_BIT(0);
	eqos_dma_safety_writel(val, (unsigned char *)addr +
			       EQOS_DMA_CHX_TX_CTRL(chan),
			       EQOS_DMA_CH0_TX_CTRL_IDX + chan);

	/* stop Rx DMA */
	val = osi_readl((unsigned char *)addr + EQOS_DMA_CHX_RX_CTRL(chan));
	val &= ~OSI_BIT(0);
	eqos_dma_safety_writel(val, (unsigned char *)addr +
			       EQOS_DMA_CHX_RX_CTRL(chan),
			       EQOS_DMA_CH0_RX_CTRL_IDX + chan);
}

/**
 * @brief eqos_configure_dma_channel - Configure DMA channel
 *
 * Algorithm: This takes care of configuring the  below
 *	parameters for the DMA channel
 *	1) Enabling DMA channel interrupts
 *	2) Enable 8xPBL mode
 *	3) Program Tx, Rx PBL
 *	4) Enable TSO if HW supports
 *	5) Program Rx Watchdog timer
 *
 * @param[in] chan: DMA channel number that need to be configured.
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note MAC has to be out of reset.
 */
static void eqos_configure_dma_channel(unsigned int chan,
				       struct osi_dma_priv_data *osi_dma)
{
	unsigned int value;

	CHECK_CHAN_BOUND(chan);

	/* enable DMA channel interrupts */
	/* Enable TIE and TBUE */
	/* TIE - Transmit Interrupt Enable */
	/* TBUE - Transmit Buffer Unavailable Enable */
	/* RIE - Receive Interrupt Enable */
	/* RBUE - Receive Buffer Unavailable Enable  */
	/* AIE - Abnormal Interrupt Summary Enable */
	/* NIE - Normal Interrupt Summary Enable */
	/* FBE - Fatal Bus Error Enable */
	value = osi_readl((unsigned char *)osi_dma->base +
			  EQOS_DMA_CHX_INTR_ENA(chan));
	value |= EQOS_DMA_CHX_INTR_TIE | EQOS_DMA_CHX_INTR_TBUE |
		 EQOS_DMA_CHX_INTR_RIE | EQOS_DMA_CHX_INTR_RBUE |
		 EQOS_DMA_CHX_INTR_FBEE | EQOS_DMA_CHX_INTR_AIE |
		 EQOS_DMA_CHX_INTR_NIE;

	/* For multi-irqs to work nie needs to be disabled */
	value &= ~(EQOS_DMA_CHX_INTR_NIE);
	eqos_dma_safety_writel(value, (unsigned char *)osi_dma->base +
			       EQOS_DMA_CHX_INTR_ENA(chan),
			       EQOS_DMA_CH0_INTR_ENA_IDX + chan);

	/* Enable 8xPBL mode */
	value = osi_readl((unsigned char *)osi_dma->base +
			  EQOS_DMA_CHX_CTRL(chan));
	value |= EQOS_DMA_CHX_CTRL_PBLX8;
	eqos_dma_safety_writel(value, (unsigned char *)osi_dma->base +
			       EQOS_DMA_CHX_CTRL(chan),
			       EQOS_DMA_CH0_CTRL_IDX + chan);

	/* Configure DMA channel Transmit control register */
	value = osi_readl((unsigned char *)osi_dma->base +
			  EQOS_DMA_CHX_TX_CTRL(chan));
	/* Enable OSF mode */
	value |= EQOS_DMA_CHX_TX_CTRL_OSF;
	/* TxPBL = 32*/
	value |= EQOS_DMA_CHX_TX_CTRL_TXPBL_RECOMMENDED;
	/* enable TSO by default if HW supports */
	value |= EQOS_DMA_CHX_TX_CTRL_TSE;

	eqos_dma_safety_writel(value, (unsigned char *)osi_dma->base +
			       EQOS_DMA_CHX_TX_CTRL(chan),
			       EQOS_DMA_CH0_TX_CTRL_IDX + chan);

	/* Configure DMA channel Receive control register */
	/* Select Rx Buffer size.  Needs to be rounded up to next multiple of
	 * bus width
	 */
	value = osi_readl((unsigned char *)osi_dma->base +
			  EQOS_DMA_CHX_RX_CTRL(chan));

	value |= (osi_dma->rx_buf_len << EQOS_DMA_CHX_RBSZ_SHIFT);
	/* RXPBL = 12 */
	value |= EQOS_DMA_CHX_RX_CTRL_RXPBL_RECOMMENDED;
	eqos_dma_safety_writel(value, (unsigned char *)osi_dma->base +
			       EQOS_DMA_CHX_RX_CTRL(chan),
			       EQOS_DMA_CH0_RX_CTRL_IDX + chan);

	/* Set Receive Interrupt Watchdog Timer Count */
	/* conversion of usec to RWIT value
	 * Eg:System clock is 62.5MHz, each clock cycle would then be 16ns
	 * For value 0x1 in watchdog timer,device would wait for 256 clk cycles,
	 * ie, (16ns x 256) => 4.096us (rounding off to 4us)
	 * So formula with above values is,ret = usec/4
	 */
	if (osi_dma->use_riwt == OSI_ENABLE && osi_dma->rx_riwt < UINT_MAX) {
		value = osi_readl((unsigned char *)osi_dma->base +
				  EQOS_DMA_CHX_RX_WDT(chan));
		/* Mask the RWT value */
		value &= ~EQOS_DMA_CHX_RX_WDT_RWT_MASK;
		/* Conversion of usec to Rx Interrupt Watchdog Timer Count */
		value |= ((osi_dma->rx_riwt *
			 (OSI_ETHER_SYSCLOCK / OSI_ONE_MEGA_HZ)) /
			 EQOS_DMA_CHX_RX_WDT_RWTU) &
			 EQOS_DMA_CHX_RX_WDT_RWT_MASK;
		osi_writel(value, (unsigned char *)osi_dma->base +
			   EQOS_DMA_CHX_RX_WDT(chan));
	}
}

/**
 * @brief eqos_init_dma_channel - DMA channel INIT
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 */
static void eqos_init_dma_channel(struct osi_dma_priv_data *osi_dma)
{
	unsigned int chinx;

	eqos_dma_safety_init(osi_dma);

	/* configure EQOS DMA channels */
	for (chinx = 0; chinx < osi_dma->num_dma_chans; chinx++) {
		eqos_configure_dma_channel(osi_dma->dma_chans[chinx], osi_dma);
	}
}

/**
 * @brief eqos_set_rx_buf_len - Set Rx buffer length
 *	  Sets the Rx buffer length based on the new MTU size set.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	 3) osi_dma->mtu need to be filled with current MTU size <= 9K
 */
static void eqos_set_rx_buf_len(struct osi_dma_priv_data *osi_dma)
{
	unsigned int rx_buf_len;

	if (osi_dma->mtu >= OSI_MTU_SIZE_8K) {
		rx_buf_len = OSI_MTU_SIZE_16K;
	} else if (osi_dma->mtu >= OSI_MTU_SIZE_4K) {
		rx_buf_len = OSI_MTU_SIZE_8K;
	} else if (osi_dma->mtu >= OSI_MTU_SIZE_2K) {
		rx_buf_len = OSI_MTU_SIZE_4K;
	} else if (osi_dma->mtu > MAX_ETH_FRAME_LEN_DEFAULT) {
		rx_buf_len = OSI_MTU_SIZE_2K;
	} else {
		rx_buf_len = MAX_ETH_FRAME_LEN_DEFAULT;
	}

	/* Buffer alignment */
	osi_dma->rx_buf_len = ((rx_buf_len + (EQOS_AXI_BUS_WIDTH - 1U)) &
			       ~(EQOS_AXI_BUS_WIDTH - 1U));
}

/**
 * @brief eqos_dma_chan_ops - EQOS DMA operations
 *
 */
static struct osi_dma_chan_ops eqos_dma_chan_ops = {
	.set_tx_ring_len = eqos_set_tx_ring_len,
	.set_rx_ring_len = eqos_set_rx_ring_len,
	.set_tx_ring_start_addr = eqos_set_tx_ring_start_addr,
	.set_rx_ring_start_addr = eqos_set_rx_ring_start_addr,
	.update_tx_tailptr = eqos_update_tx_tailptr,
	.update_rx_tailptr = eqos_update_rx_tailptr,
	.clear_tx_intr = eqos_clear_tx_intr,
	.clear_rx_intr = eqos_clear_rx_intr,
	.disable_chan_tx_intr = eqos_disable_chan_tx_intr,
	.enable_chan_tx_intr = eqos_enable_chan_tx_intr,
	.disable_chan_rx_intr = eqos_disable_chan_rx_intr,
	.enable_chan_rx_intr = eqos_enable_chan_rx_intr,
	.start_dma = eqos_start_dma,
	.stop_dma = eqos_stop_dma,
	.init_dma_channel = eqos_init_dma_channel,
	.set_rx_buf_len = eqos_set_rx_buf_len,
	.validate_regs = eqos_validate_dma_regs,
};

/**
 * @brief eqos_get_dma_safety_config - EQOS get DMA safety configuration
 */
void *eqos_get_dma_safety_config(void)
{
	return &eqos_dma_safety_config;
}

/**
 * @brief eqos_get_dma_chan_ops - EQOS get DMA channel operations
 */
struct osi_dma_chan_ops *eqos_get_dma_chan_ops(void)
{
	return &eqos_dma_chan_ops;
}
