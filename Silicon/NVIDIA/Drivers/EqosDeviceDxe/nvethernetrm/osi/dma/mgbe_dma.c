/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
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
#include <osi_common.h>
#include "mgbe_dma.h"
#include "dma_local.h"

/**
 * @brief mgbe_disable_chan_tx_intr - Disables DMA Tx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	 3) Mapping of physical IRQ line to DMA channel need to be maintained at
 *	 OSDependent layer and pass corresponding channel number.
 */
static void mgbe_disable_chan_tx_intr(void *addr, nveu32_t chan)
{
	nveu32_t cntrl;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	cntrl = osi_readl((nveu8_t *)addr +
			  MGBE_VIRT_INTR_CHX_CNTRL(chan));
	cntrl &= ~MGBE_VIRT_INTR_CHX_CNTRL_TX;
	osi_writel(cntrl, (nveu8_t *)addr +
		   MGBE_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief mgbe_enable_chan_tx_intr - Enable Tx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	 3) Mapping of physical IRQ line to DMA channel need to be maintained at
 *	 OSDependent layer and pass corresponding channel number.
 */
static void mgbe_enable_chan_tx_intr(void *addr, nveu32_t chan)
{
	nveu32_t cntrl;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	cntrl = osi_readl((nveu8_t *)addr +
			  MGBE_VIRT_INTR_CHX_CNTRL(chan));
	cntrl |= MGBE_VIRT_INTR_CHX_CNTRL_TX;
	osi_writel(cntrl, (nveu8_t *)addr +
		   MGBE_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief mgbe_disable_chan_rx_intr - Disable Rx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	 3) Mapping of physical IRQ line to DMA channel need to be maintained at
 *	 OSDependent layer and pass corresponding channel number.
 */
static void mgbe_disable_chan_rx_intr(void *addr, nveu32_t chan)
{
	nveu32_t cntrl;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	cntrl = osi_readl((nveu8_t *)addr +
			  MGBE_VIRT_INTR_CHX_CNTRL(chan));
	cntrl &= ~MGBE_VIRT_INTR_CHX_CNTRL_RX;
	osi_writel(cntrl, (nveu8_t *)addr +
		   MGBE_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief mgbe_enable_chan_rx_intr - Enable Rx channel interrupts.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 */
static void mgbe_enable_chan_rx_intr(void *addr, nveu32_t chan)
{
	nveu32_t cntrl;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	cntrl = osi_readl((nveu8_t *)addr +
			  MGBE_VIRT_INTR_CHX_CNTRL(chan));
	cntrl |= MGBE_VIRT_INTR_CHX_CNTRL_RX;
	osi_writel(cntrl, (nveu8_t *)addr +
		   MGBE_VIRT_INTR_CHX_CNTRL(chan));
}

/**
 * @brief mgbe_set_tx_ring_len - Set DMA Tx ring length.
 *
 * Algorithm: Set DMA Tx channel ring length for specific channel.
 *
 * @param[in] osi_dma: OSI DMA data structure.
 * @param[in] chan: DMA Tx channel number.
 * @param[in] len: Length.
 */
static void mgbe_set_tx_ring_len(struct osi_dma_priv_data *osi_dma,
				 nveu32_t chan,
				 nveu32_t len)
{
	void *addr = osi_dma->base;
	nveu32_t value;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	value = osi_readl((nveu8_t *)addr + MGBE_DMA_CHX_TX_CNTRL2(chan));
	value |= (len & MGBE_DMA_RING_LENGTH_MASK);
	osi_writel(value, (nveu8_t *)addr + MGBE_DMA_CHX_TX_CNTRL2(chan));
}

/**
 * @brief mgbe_set_tx_ring_start_addr - Set DMA Tx ring base address.
 *
 * Algorithm: Sets DMA Tx ring base address for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 * @param[in] tx_desc: Tx desc base addess.
 */
static void mgbe_set_tx_ring_start_addr(void *addr, nveu32_t chan,
					nveu64_t tx_desc)
{
	nveu64_t temp;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	temp = H32(tx_desc);
	if (temp < UINT_MAX) {
		osi_writel((nveu32_t)temp, (nveu8_t *)addr +
			   MGBE_DMA_CHX_TDLH(chan));
	}

	temp = L32(tx_desc);
	if (temp < UINT_MAX) {
		osi_writel((nveu32_t)temp, (nveu8_t *)addr +
			   MGBE_DMA_CHX_TDLA(chan));
	}
}

/**
 * @brief mgbe_update_tx_tailptr - Updates DMA Tx ring tail pointer.
 *
 * Algorithm: Updates DMA Tx ring tail pointer for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Tx channel number.
 * @param[in] tailptr: DMA Tx ring tail pointer.
 *
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 */
static void mgbe_update_tx_tailptr(void *addr, nveu32_t chan,
				   nveu64_t tailptr)
{
	nveu64_t temp;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	temp = L32(tailptr);
	if (temp < UINT_MAX) {
		osi_writel((nveu32_t)temp, (nveu8_t *)addr +
			   MGBE_DMA_CHX_TDTLP(chan));
	}
}

/**
 * @brief mgbe_set_rx_ring_len - Set Rx channel ring length.
 *
 * Algorithm: Sets DMA Rx channel ring length for specific DMA channel.
 *
 * @param[in] osi_dma: OSI DMA data structure.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] len: Length
 */
static void mgbe_set_rx_ring_len(struct osi_dma_priv_data *osi_dma,
				 nveu32_t chan,
				 nveu32_t len)
{
	void *addr = osi_dma->base;
	nveu32_t value;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	value = osi_readl((nveu8_t *)addr + MGBE_DMA_CHX_RX_CNTRL2(chan));
	value |= (len & MGBE_DMA_RING_LENGTH_MASK);
	osi_writel(value, (nveu8_t *)addr + MGBE_DMA_CHX_RX_CNTRL2(chan));
}

/**
 * @brief mgbe_set_rx_ring_start_addr - Set DMA Rx ring base address.
 *
 * Algorithm: Sets DMA Rx channel ring base address.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] tx_desc: DMA Rx desc base address.
 */
static void mgbe_set_rx_ring_start_addr(void *addr, nveu32_t chan,
					nveu64_t tx_desc)
{
	nveu64_t temp;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	temp = H32(tx_desc);
	if (temp < UINT_MAX) {
		osi_writel((nveu32_t)temp, (nveu8_t *)addr +
			   MGBE_DMA_CHX_RDLH(chan));
	}

	temp = L32(tx_desc);
	if (temp < UINT_MAX) {
		osi_writel((nveu32_t)temp, (nveu8_t *)addr +
			   MGBE_DMA_CHX_RDLA(chan));
	}
}

/**
 * @brief mgbe_update_rx_tailptr - Update Rx ring tail pointer
 *
 * Algorithm: Updates DMA Rx channel tail pointer for specific channel.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] tailptr: Tail pointer
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 */
static void mgbe_update_rx_tailptr(void *addr, nveu32_t chan,
				   nveu64_t tailptr)
{
	nveu64_t temp;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	temp = H32(tailptr);
	if (temp < UINT_MAX) {
		osi_writel((nveu32_t)temp, (nveu8_t *)addr +
			   MGBE_DMA_CHX_RDTHP(chan));
	}

	temp = L32(tailptr);
	if (temp < UINT_MAX) {
		osi_writel((nveu32_t)temp, (nveu8_t *)addr +
			   MGBE_DMA_CHX_RDTLP(chan));
	}
}

/**
 * @brief mgbe_start_dma - Start DMA.
 *
 * Algorithm: Start Tx and Rx DMA for specific channel.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA Tx/Rx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 */
static void mgbe_start_dma(struct osi_dma_priv_data *osi_dma, nveu32_t chan)
{
	nveu32_t val;
	void *addr = osi_dma->base;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	/* start Tx DMA */
	val = osi_readl((nveu8_t *)addr + MGBE_DMA_CHX_TX_CTRL(chan));
	val |= OSI_BIT(0);
	osi_writel(val, (nveu8_t *)addr + MGBE_DMA_CHX_TX_CTRL(chan));

	/* start Rx DMA */
	val = osi_readl((nveu8_t *)addr + MGBE_DMA_CHX_RX_CTRL(chan));
	val |= OSI_BIT(0);
	val &= ~OSI_BIT(31);
	osi_writel(val, (nveu8_t *)addr + MGBE_DMA_CHX_RX_CTRL(chan));
}

/**
 * @brief mgbe_stop_dma - Stop DMA.
 *
 * Algorithm: Start Tx and Rx DMA for specific channel.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA Tx/Rx channel number.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 */
static void mgbe_stop_dma(struct osi_dma_priv_data *osi_dma, nveu32_t chan)
{
	nveu32_t val;
	void *addr = osi_dma->base;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	/* stop Tx DMA */
	val = osi_readl((nveu8_t *)addr + MGBE_DMA_CHX_TX_CTRL(chan));
	val &= ~OSI_BIT(0);
	osi_writel(val, (nveu8_t *)addr + MGBE_DMA_CHX_TX_CTRL(chan));

	/* stop Rx DMA */
	val = osi_readl((nveu8_t *)addr + MGBE_DMA_CHX_RX_CTRL(chan));
	val &= ~OSI_BIT(0);
	val |= OSI_BIT(31);
	osi_writel(val, (nveu8_t *)addr + MGBE_DMA_CHX_RX_CTRL(chan));
}

/**
 * @brief mgbe_configure_dma_channel - Configure DMA channel
 *
 * Algorithm: This takes care of configuring the  below
 *	parameters for the DMA channel
 *	1) Enabling DMA channel interrupts
 *	2) Enable 8xPBL mode
 *	3) Program Tx, Rx PBL
 *	4) Enable TSO if HW supports
 *	5) Program Rx Watchdog timer
 *	6) Program Out Standing DMA Read Requests
 *	7) Program Out Standing DMA write Requests
 *
 * @param[in] chan: DMA channel number that need to be configured.
 * @param[in] owrq: out standing write dma requests
 * @param[in] orrq: out standing read dma requests
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note MAC has to be out of reset.
 */
static void mgbe_configure_dma_channel(nveu32_t chan,
				       nveu32_t owrq,
				       nveu32_t orrq,
				       struct osi_dma_priv_data *osi_dma)
{
	nveu32_t value;
	nveu32_t txpbl;
	nveu32_t rxpbl;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
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
			  MGBE_DMA_CHX_INTR_ENA(chan));
	value |= MGBE_DMA_CHX_INTR_TIE | MGBE_DMA_CHX_INTR_TBUE |
		 MGBE_DMA_CHX_INTR_RIE | MGBE_DMA_CHX_INTR_RBUE |
		 MGBE_DMA_CHX_INTR_FBEE | MGBE_DMA_CHX_INTR_AIE |
		 MGBE_DMA_CHX_INTR_NIE;

	/* For multi-irqs to work nie needs to be disabled */
	/* TODO: do we need this ? */
	value &= ~(MGBE_DMA_CHX_INTR_NIE);
	osi_writel(value, (nveu8_t *)osi_dma->base +
		   MGBE_DMA_CHX_INTR_ENA(chan));

	/* Enable 8xPBL mode */
	value = osi_readl((nveu8_t *)osi_dma->base +
			  MGBE_DMA_CHX_CTRL(chan));
	value |= MGBE_DMA_CHX_CTRL_PBLX8;
	osi_writel(value, (nveu8_t *)osi_dma->base +
		   MGBE_DMA_CHX_CTRL(chan));

	/* Configure DMA channel Transmit control register */
	value = osi_readl((nveu8_t *)osi_dma->base +
			  MGBE_DMA_CHX_TX_CTRL(chan));
	/* Enable OSF mode */
	value |= MGBE_DMA_CHX_TX_CTRL_OSP;

	/*
	 * Formula for TxPBL calculation is
	 * (TxPBL) < ((TXQSize - MTU)/(DATAWIDTH/8)) - 5
	 * if TxPBL exceeds the value of 256 then we need to make use of 256
	 * as the TxPBL else we should be using the value whcih we get after
	 * calculation by using above formula
	 */
	if (osi_dma->pre_si == OSI_ENABLE) {
		txpbl = ((((MGBE_TXQ_RXQ_SIZE_FPGA / osi_dma->num_dma_chans) -
			 osi_dma->mtu) / (MGBE_AXI_DATAWIDTH / 8U)) - 5U);
	} else {
		txpbl = ((((MGBE_TXQ_SIZE / osi_dma->num_dma_chans) -
			 osi_dma->mtu) / (MGBE_AXI_DATAWIDTH / 8U)) - 5U);
	}

	/* Since PBLx8 is set, so txpbl/8 will be the value that
	 * need to be programmed
	 */
	if (txpbl >= MGBE_DMA_CHX_MAX_PBL) {
		value |= ((MGBE_DMA_CHX_MAX_PBL / 8U) <<
			  MGBE_DMA_CHX_CTRL_PBL_SHIFT);
	} else {
		value |= ((txpbl / 8U) << MGBE_DMA_CHX_CTRL_PBL_SHIFT);
	}

	/* enable TSO by default if HW supports */
	value |= MGBE_DMA_CHX_TX_CTRL_TSE;

	osi_writel(value, (nveu8_t *)osi_dma->base +
		   MGBE_DMA_CHX_TX_CTRL(chan));

	/* Configure DMA channel Receive control register */
	/* Select Rx Buffer size.  Needs to be rounded up to next multiple of
	 * bus width
	 */
	value = osi_readl((nveu8_t *)osi_dma->base +
			  MGBE_DMA_CHX_RX_CTRL(chan));

	/* clear previous Rx buffer size */
	value &= ~MGBE_DMA_CHX_RBSZ_MASK;
	value |= (osi_dma->rx_buf_len << MGBE_DMA_CHX_RBSZ_SHIFT);
	/* RxPBL calculation is
	 * RxPBL  <= Rx Queue Size/2
	 */
	if (osi_dma->pre_si == OSI_ENABLE) {
		rxpbl = (((MGBE_TXQ_RXQ_SIZE_FPGA / osi_dma->num_dma_chans) /
			 2U) << MGBE_DMA_CHX_CTRL_PBL_SHIFT);
	} else {
		rxpbl = (((MGBE_RXQ_SIZE / osi_dma->num_dma_chans) / 2U) <<
			 MGBE_DMA_CHX_CTRL_PBL_SHIFT);
	}
	/* Since PBLx8 is set, so rxpbl/8 will be the value that
	 * need to be programmed
	 */
	if (rxpbl >= MGBE_DMA_CHX_MAX_PBL) {
		value |= ((MGBE_DMA_CHX_MAX_PBL / 8) <<
			  MGBE_DMA_CHX_CTRL_PBL_SHIFT);
	} else {
		value |= ((rxpbl / 8) << MGBE_DMA_CHX_CTRL_PBL_SHIFT);
	}

	osi_writel(value, (nveu8_t *)osi_dma->base +
		   MGBE_DMA_CHX_RX_CTRL(chan));

	/* Set Receive Interrupt Watchdog Timer Count */
	/* conversion of usec to RWIT value
	 * Eg:System clock is 62.5MHz, each clock cycle would then be 16ns
	 * For value 0x1 in watchdog timer,device would wait for 256 clk cycles,
	 * ie, (16ns x 256) => 4.096us (rounding off to 4us)
	 * So formula with above values is,ret = usec/4
	 */
	/* NOTE: Bug 3287883: If RWTU value programmed then driver needs
	 * to follow below order -
	 * 1. First write RWT field with non-zero value.
	 * 2. Program RWTU field of register
	 * DMA_CH(#i)_Rx_Interrupt_Watchdog_Time.
	 */
	if ((osi_dma->use_riwt == OSI_ENABLE) &&
	    (osi_dma->rx_riwt < UINT_MAX)) {
		value = osi_readl((nveu8_t *)osi_dma->base +
				  MGBE_DMA_CHX_RX_WDT(chan));
		/* Mask the RWT value */
		value &= ~MGBE_DMA_CHX_RX_WDT_RWT_MASK;
		/* Conversion of usec to Rx Interrupt Watchdog Timer Count */
		/* TODO: Need to fix AXI clock for silicon */
		value |= ((osi_dma->rx_riwt *
			 ((nveu32_t)MGBE_AXI_CLK_FREQ / OSI_ONE_MEGA_HZ)) /
			 MGBE_DMA_CHX_RX_WDT_RWTU) &
			 MGBE_DMA_CHX_RX_WDT_RWT_MASK;
		osi_writel(value, (nveu8_t *)osi_dma->base +
			   MGBE_DMA_CHX_RX_WDT(chan));

		value = osi_readl((nveu8_t *)osi_dma->base +
				  MGBE_DMA_CHX_RX_WDT(chan));
		value &= ~(MGBE_DMA_CHX_RX_WDT_RWTU_MASK <<
			   MGBE_DMA_CHX_RX_WDT_RWTU_SHIFT);
		value |= (MGBE_DMA_CHX_RX_WDT_RWTU_2048_CYCLE <<
			  MGBE_DMA_CHX_RX_WDT_RWTU_SHIFT);
		osi_writel(value, (nveu8_t *)osi_dma->base +
			   MGBE_DMA_CHX_RX_WDT(chan));
	}

	/* Update ORRQ in DMA_CH(#i)_Tx_Control2 register */
	value = osi_readl((nveu8_t *)osi_dma->base +
			  MGBE_DMA_CHX_TX_CNTRL2(chan));
	value |= (orrq << MGBE_DMA_CHX_TX_CNTRL2_ORRQ_SHIFT);
	osi_writel(value, (nveu8_t *)osi_dma->base +
		   MGBE_DMA_CHX_TX_CNTRL2(chan));

	/* Update OWRQ in DMA_CH(#i)_Rx_Control2 register */
	value = osi_readl((nveu8_t *)osi_dma->base +
			  MGBE_DMA_CHX_RX_CNTRL2(chan));
	value |= (owrq << MGBE_DMA_CHX_RX_CNTRL2_OWRQ_SHIFT);
	osi_writel(value, (nveu8_t *)osi_dma->base +
		   MGBE_DMA_CHX_RX_CNTRL2(chan));
}

/**
 * @brief mgbe_init_dma_channel - DMA channel INIT
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 */
static nve32_t mgbe_init_dma_channel(struct osi_dma_priv_data *osi_dma)
{
	nveu32_t chinx;
	nveu32_t owrq;
	nveu32_t orrq;

	/* DMA Read Out Standing Requests */
	/* For Presi ORRQ is 16 in case of schannel and 64 in case of mchannel.
	 * For Si ORRQ is 64 in case of single and multi channel
	 */
	orrq = (MGBE_DMA_CHX_TX_CNTRL2_ORRQ_RECOMMENDED /
		osi_dma->num_dma_chans);
	if ((osi_dma->num_dma_chans == 1U) && (osi_dma->pre_si == OSI_ENABLE)) {
		/* For Presi ORRQ is 16 in a single channel configuration
		 * so overwrite only for this configuration
		 */
		orrq = MGBE_DMA_CHX_RX_CNTRL2_ORRQ_SCHAN_PRESI;
	}

	/* DMA Write Out Standing Requests */
	/* For Presi OWRQ is 8 and for Si it is 32 in case of single channel.
	 * For Multi Channel OWRQ is 64 for both si and presi
	 */
	if (osi_dma->num_dma_chans == 1U) {
		if (osi_dma->pre_si == OSI_ENABLE) {
			owrq = MGBE_DMA_CHX_RX_CNTRL2_OWRQ_SCHAN_PRESI;
		} else {
			owrq = MGBE_DMA_CHX_RX_CNTRL2_OWRQ_SCHAN;
		}
	} else {
		owrq = (MGBE_DMA_CHX_RX_CNTRL2_OWRQ_MCHAN /
			osi_dma->num_dma_chans);
	}

	/* configure MGBE DMA channels */
	for (chinx = 0; chinx < osi_dma->num_dma_chans; chinx++) {
		mgbe_configure_dma_channel(osi_dma->dma_chans[chinx],
					   owrq, orrq, osi_dma);
	}

	return 0;
}

/**
 * @brief mgbe_set_rx_buf_len - Set Rx buffer length
 *	  Sets the Rx buffer length based on the new MTU size set.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note 1) MAC needs to be out of reset and proper clocks need to be configured
 *	 2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	 3) osi_dma->mtu need to be filled with current MTU size <= 9K
 */
static void mgbe_set_rx_buf_len(struct osi_dma_priv_data *osi_dma)
{
	nveu32_t rx_buf_len;

	/* Add Ethernet header + FCS + NET IP align size to MTU */
	rx_buf_len = osi_dma->mtu + OSI_ETH_HLEN +
		     NV_VLAN_HLEN + OSI_NET_IP_ALIGN;
	/* Buffer alignment */
	osi_dma->rx_buf_len = ((rx_buf_len + (MGBE_AXI_BUS_WIDTH - 1U)) &
			       ~(MGBE_AXI_BUS_WIDTH - 1U));
}

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
static nve32_t mgbe_validate_dma_regs(struct osi_dma_priv_data *osi_dma)
{
	/* TODO: for mgbe */
	return 0;
}

/**
 * @brief mgbe_get_global_dma_status - Gets DMA status.
 *
 * Algorithm: Returns global DMA Tx/Rx interrupt status
 *
 * @param[in] addr: MAC base address.
 */
static nveu32_t mgbe_get_global_dma_status(void *addr)
{
	return osi_readl((nveu8_t *)addr + MGBE_GLOBAL_DMA_STATUS);
}

/**
 * @brief mgbe_clear_vm_tx_intr - Clear VM Tx interrupt
 *
 * Algorithm: Clear Tx interrupt source at DMA and wrapper level.
 *
 * @param[in] addr: MAC base address.
 * @param[in] chan: DMA Tx channel number.
 */
static void mgbe_clear_vm_tx_intr(void *addr, nveu32_t chan)
{
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	osi_writel(MGBE_DMA_CHX_STATUS_CLEAR_TX,
		   (nveu8_t *)addr + MGBE_DMA_CHX_STATUS(chan));
	osi_writel(MGBE_VIRT_INTR_CHX_STATUS_TX,
		   (nveu8_t *)addr + MGBE_VIRT_INTR_CHX_STATUS(chan));

	mgbe_disable_chan_tx_intr(addr, chan);
}

/**
 * @brief mgbe_clear_vm_rx_intr - Clear VM Rx interrupt
 *
 * @param[in] addr: MAC base address.
 * @param[in] chan: DMA Tx channel number.
 *
 * Algorithm: Clear Rx interrupt source at DMA and wrapper level.
 */
static void mgbe_clear_vm_rx_intr(void *addr, nveu32_t chan)
{
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	osi_writel(MGBE_DMA_CHX_STATUS_CLEAR_RX,
		   (nveu8_t *)addr + MGBE_DMA_CHX_STATUS(chan));
	osi_writel(MGBE_VIRT_INTR_CHX_STATUS_RX,
		   (nveu8_t *)addr + MGBE_VIRT_INTR_CHX_STATUS(chan));

	mgbe_disable_chan_rx_intr(addr, chan);
}

/**
 * @brief mgbe_config_slot - Configure slot Checking for DMA channel
 *
 * Algorithm: Set/Reset the slot function of DMA channel based on given inputs
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA channel number to enable slot function
 * @param[in] set: flag for set/reset with value OSI_ENABLE/OSI_DISABLE
 * @param[in] interval: slot interval fixed for MGBE - its 125usec
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) OSD should be initialized
 *
 */
static void mgbe_config_slot(struct osi_dma_priv_data *osi_dma,
			     unsigned int chan,
			     unsigned int set,
			     unsigned int interval)
{
	unsigned int value;
#if 0
	MGBE_CHECK_CHAN_BOUND(chan);
#endif
	if (set == OSI_ENABLE) {
		/* Program SLOT CTRL register SIV and set ESC bit */
		value = osi_readl((unsigned char *)osi_dma->base +
				  MGBE_DMA_CHX_SLOT_CTRL(chan));
		/* Set ESC bit */
		value |= MGBE_DMA_CHX_SLOT_ESC;
		osi_writel(value, (unsigned char *)osi_dma->base +
			   MGBE_DMA_CHX_SLOT_CTRL(chan));

	} else {
		/* Clear ESC bit of SLOT CTRL register */
		value = osi_readl((unsigned char *)osi_dma->base +
				  MGBE_DMA_CHX_SLOT_CTRL(chan));
		value &= ~MGBE_DMA_CHX_SLOT_ESC;
		osi_writel(value, (unsigned char *)osi_dma->base +
			   MGBE_DMA_CHX_SLOT_CTRL(chan));
	}
}

void mgbe_init_dma_chan_ops(struct dma_chan_ops *ops)
{
	ops->set_tx_ring_len = mgbe_set_tx_ring_len;
	ops->set_rx_ring_len = mgbe_set_rx_ring_len;
	ops->set_tx_ring_start_addr = mgbe_set_tx_ring_start_addr;
	ops->set_rx_ring_start_addr = mgbe_set_rx_ring_start_addr;
	ops->update_tx_tailptr = mgbe_update_tx_tailptr;
	ops->update_rx_tailptr = mgbe_update_rx_tailptr;
	ops->disable_chan_tx_intr = mgbe_disable_chan_tx_intr;
	ops->enable_chan_tx_intr = mgbe_enable_chan_tx_intr;
	ops->disable_chan_rx_intr = mgbe_disable_chan_rx_intr;
	ops->enable_chan_rx_intr = mgbe_enable_chan_rx_intr;
	ops->start_dma = mgbe_start_dma;
	ops->stop_dma = mgbe_stop_dma;
	ops->init_dma_channel = mgbe_init_dma_channel;
	ops->set_rx_buf_len = mgbe_set_rx_buf_len;
	ops->validate_regs = mgbe_validate_dma_regs;
	ops->get_global_dma_status = mgbe_get_global_dma_status;
	ops->clear_vm_tx_intr = mgbe_clear_vm_tx_intr;
	ops->clear_vm_rx_intr = mgbe_clear_vm_rx_intr;
	ops->config_slot = mgbe_config_slot;
};
