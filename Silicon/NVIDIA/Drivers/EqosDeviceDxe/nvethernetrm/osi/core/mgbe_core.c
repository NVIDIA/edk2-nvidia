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
#include "../osi/common/type.h"
#include <local_common.h>
#include <osi_common.h>
#include <osi_core.h>
#include "mgbe_core.h"
#include "core_local.h"
#include "xpcs.h"
#include "mgbe_mmc.h"
#include "vlan_filter.h"

/**
 * @brief mgbe_ptp_tsc_capture - read PTP and TSC registers
 *
 * Algorithm:
 * - write 1 to MGBE_WRAP_SYNC_TSC_PTP_CAPTURE_0
 * - wait till MGBE_WRAP_SYNC_TSC_PTP_CAPTURE_0 is 0x0
 * - read and return following registers
 *   MGBE_WRAP _TSC_CAPTURE_LOW_0
 *   MGBE_WRAP _TSC_CAPTURE_HIGH_0
 *   MGBE_WRAP _PTP_CAPTURE_LOW_0
 *   MGBE_WRAP _PTP_CAPTURE_HIGH_0
 *
 * @param[in] base: MGBE virtual base address.
 * @param[out]: osi_core_ptp_tsc_data register
 *
 * @note MAC needs to be out of reset and proper clock configured. TSC and PTP
 * registers should be configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t mgbe_ptp_tsc_capture(struct osi_core_priv_data *const osi_core,
				    struct osi_core_ptp_tsc_data *data)
{
	nveu32_t retry = 1000U;
	nveu32_t count = 0U, val = 0U;
	nve32_t cond = COND_NOT_MET;
	nve32_t ret = -1;

	osi_writela(osi_core, OSI_ENABLE, (nveu8_t *)osi_core->base +
		    MGBE_WRAP_SYNC_TSC_PTP_CAPTURE);

	/* Poll Until Poll Condition */
	while (cond == COND_NOT_MET) {
		if (count > retry) {
			/* Max retries reached */
			goto done;
		}

		count++;

		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MGBE_WRAP_SYNC_TSC_PTP_CAPTURE);
		if ((val & OSI_ENABLE) == OSI_NONE) {
			cond = COND_MET;
		} else {
			/* sleep if SWR is set */
			osi_core->osd_ops.msleep(1U);
		}
	}

	data->tsc_low_bits = osi_readla(osi_core, (nveu8_t *)osi_core->base +
					MGBE_WRAP_TSC_CAPTURE_LOW);
	data->tsc_high_bits =  osi_readla(osi_core, (nveu8_t *)osi_core->base +
					 MGBE_WRAP_TSC_CAPTURE_HIGH);
	data->ptp_low_bits =  osi_readla(osi_core, (nveu8_t *)osi_core->base +
					 MGBE_WRAP_PTP_CAPTURE_LOW);
	data->ptp_high_bits =  osi_readla(osi_core, (nveu8_t *)osi_core->base +
					 MGBE_WRAP_PTP_CAPTURE_HIGH);
	ret = 0;
done:
	return ret;
}

/**
 * @brief mgbe_config_fw_err_pkts - Configure forwarding of error packets
 *
 * Algorithm: When FEP bit is reset, the Rx queue drops packets with
 *	  error status (CRC error, GMII_ER, watchdog timeout, or overflow).
 *	  When FEP bit is set, all packets except the runt error packets
 *	  are forwarded to the application or DMA.
 *
 * @param[in] addr: Base address indicating the start of memory mapped IO
 * region of the MAC.
 * @param[in] qinx: Q index
 * @param[in] enable_fw_err_pkts: Enable or Disable the forwarding of error
 * packets
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_fw_err_pkts(struct osi_core_priv_data *osi_core,
				   const unsigned int qinx,
				   const unsigned int enable_fw_err_pkts)
{
	unsigned int val;

	/* Check for valid enable_fw_err_pkts and qinx values */
	if ((enable_fw_err_pkts!= OSI_ENABLE &&
	    enable_fw_err_pkts != OSI_DISABLE) ||
	    (qinx >= OSI_MGBE_MAX_NUM_CHANS)) {
		return -1;
	}

	/* Read MTL RXQ Operation_Mode Register */
	val = osi_readla(osi_core, (unsigned char *)osi_core->base +
			MGBE_MTL_CHX_RX_OP_MODE(qinx));

	/* enable_fw_err_pkts, 1 is for enable and 0 is for disable */
	if (enable_fw_err_pkts == OSI_ENABLE) {
		/* When enable_fw_err_pkts bit is set, all packets except
		 * the runt error packets are forwarded to the application
		 * or DMA.
		 */
		val |= MGBE_MTL_RXQ_OP_MODE_FEP;
	} else {
		/* When this bit is reset, the Rx queue drops packets with error
		 * status (CRC error, GMII_ER, watchdog timeout, or overflow)
		 */
		val &= ~MGBE_MTL_RXQ_OP_MODE_FEP;
	}

	/* Write to FEP bit of MTL RXQ Operation Mode Register to enable or
	 * disable the forwarding of error packets to DMA or application.
	 */
	osi_writela(osi_core, val, (unsigned char *)osi_core->base +
		   MGBE_MTL_CHX_RX_OP_MODE(qinx));

	return 0;
}

/**
 * @brief mgbe_poll_for_swr - Poll for software reset (SWR bit in DMA Mode)
 *
 * Algorithm: CAR reset will be issued through MAC reset pin.
 *	  Waits for SWR reset to be cleared in DMA Mode register.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t mgbe_poll_for_swr(struct osi_core_priv_data *const osi_core)
{
	void *addr = osi_core->base;
	nveu32_t retry = 1000;
	nveu32_t count;
	nveu32_t dma_bmr = 0;
	nve32_t cond = 1;
	nveu32_t pre_si = osi_core->pre_si;

	/* Performing software reset */
	if (pre_si == OSI_ENABLE) {
		osi_writela(osi_core, OSI_ENABLE,
			    (nveu8_t *)addr + MGBE_DMA_MODE);
	}

	/* Poll Until Poll Condition */
	count = 0;
	while (cond == 1) {
		if (count > retry) {
			return -1;
		}

		count++;

		dma_bmr = osi_readla(osi_core, (nveu8_t *)addr + MGBE_DMA_MODE);
		if ((dma_bmr & MGBE_DMA_MODE_SWR) == OSI_NONE) {
			cond = 0;
		} else {
			/* sleep if SWR is set */
			osi_core->osd_ops.msleep(1U);
		}
	}

	return 0;
}

/**
 * @brief mgbe_calculate_per_queue_fifo - Calculate per queue FIFO size
 *
 * Algorithm: Total Tx/Rx FIFO size which is read from
 *	MAC HW is being shared equally among the queues that are
 *	configured.
 *
 * @param[in] fifo_size: Total Tx/RX HW FIFO size.
 * @param[in] queue_count: Total number of Queues configured.
 *
 * @note MAC has to be out of reset.
 *
 * @retval Queue size that need to be programmed.
 */
static nveu32_t mgbe_calculate_per_queue_fifo(nveu32_t fifo_size,
						  nveu32_t queue_count)
{
	nveu32_t q_fifo_size = 0;  /* calculated fifo size per queue */
	nveu32_t p_fifo = 0; /* per queue fifo size program value */

	if (queue_count == 0U) {
		return 0U;
	}

	/* calculate Tx/Rx fifo share per queue */
	switch (fifo_size) {
	case 0:
	case 1:
	case 2:
	case 3:
		q_fifo_size = FIFO_SIZE_KB(1U);
		break;
	case 4:
		q_fifo_size = FIFO_SIZE_KB(2U);
		break;
	case 5:
		q_fifo_size = FIFO_SIZE_KB(4U);
		break;
	case 6:
		q_fifo_size = FIFO_SIZE_KB(8U);
		break;
	case 7:
		q_fifo_size = FIFO_SIZE_KB(16U);
		break;
	case 8:
		q_fifo_size = FIFO_SIZE_KB(32U);
		break;
	case 9:
		q_fifo_size = FIFO_SIZE_KB(64U);
		break;
	case 10:
		q_fifo_size = FIFO_SIZE_KB(128U);
		break;
	case 11:
		q_fifo_size = FIFO_SIZE_KB(256U);
		break;
	case 12:
		/* Size mapping not found for 192KB, so assigned 12 */
		q_fifo_size = FIFO_SIZE_KB(192U);
		break;
	default:
		q_fifo_size = FIFO_SIZE_KB(1U);
		break;
	}

	q_fifo_size = q_fifo_size / queue_count;

	if (q_fifo_size < UINT_MAX) {
		p_fifo = (q_fifo_size / 256U) - 1U;
	}

	return p_fifo;
}

/**
 * @brief mgbe_poll_for_mac_accrtl - Poll for Indirect Access control and status
 * register operations complete.
 *
 * Algorithm: Waits for waits for transfer busy bit to be cleared in
 * MAC Indirect address control register to complete operations.
 *
 * @param[in] addr: MGBE virtual base address.
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_poll_for_mac_acrtl(struct osi_core_priv_data *osi_core)
{
	nveu32_t count = 0U;
	nveu32_t mac_indir_addr_ctrl = 0U;

	/* Poll Until MAC_Indir_Access_Ctrl OB is clear */
	while (count < MGBE_MAC_INDIR_AC_OB_RETRY) {
		mac_indir_addr_ctrl = osi_readla(osi_core,
						 (nveu8_t *)osi_core->base +
						 MGBE_MAC_INDIR_AC);
		if ((mac_indir_addr_ctrl & MGBE_MAC_INDIR_AC_OB) == OSI_NONE) {
			/* OB is clear exit the loop */
			return 0;
		}

		/* wait for 10 usec for OB clear and retry */
		osi_core->osd_ops.udelay(MGBE_MAC_INDIR_AC_OB_WAIT);
		count++;
	}

	return -1;
}

/**
 * @brief mgbe_mac_indir_addr_write - MAC Indirect AC register write.
 *
 * Algorithm: writes MAC Indirect AC register
 *
 * @param[in] base: MGBE virtual base address.
 * @param[in] mc_no: MAC AC Mode Select number
 * @param[in] addr_offset: MAC AC Address Offset.
 * @param[in] value: MAC AC register value
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_mac_indir_addr_write(struct osi_core_priv_data *osi_core,
				     nveu32_t mc_no,
				     nveu32_t addr_offset,
				     nveu32_t value)
{
	void *base = osi_core->base;
	nveu32_t addr = 0;

	/* Write MAC_Indir_Access_Data register value */
	osi_writela(osi_core, value, (nveu8_t *)base + MGBE_MAC_INDIR_DATA);

	/* Program MAC_Indir_Access_Ctrl */
	addr = osi_readla(osi_core, (nveu8_t *)base + MGBE_MAC_INDIR_AC);

	/* update Mode Select */
	addr &= ~(MGBE_MAC_INDIR_AC_MSEL);
	addr |= ((mc_no << MGBE_MAC_INDIR_AC_MSEL_SHIFT) &
		  MGBE_MAC_INDIR_AC_MSEL);

	/* update Address Offset */
	addr &= ~(MGBE_MAC_INDIR_AC_AOFF);
	addr |= ((addr_offset << MGBE_MAC_INDIR_AC_AOFF_SHIFT) &
		  MGBE_MAC_INDIR_AC_AOFF);

	/* Set CMD filed bit 0 for write */
	addr &= ~(MGBE_MAC_INDIR_AC_CMD);

	/* Set OB bit to initiate write */
	addr |= MGBE_MAC_INDIR_AC_OB;

	/* Write MGBE_MAC_L3L4_ADDR_CTR */
	osi_writela(osi_core, addr, (nveu8_t *)base + MGBE_MAC_INDIR_AC);

	/* Wait until OB bit reset */
	if (mgbe_poll_for_mac_acrtl(osi_core) < 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
			     "Fail to write MAC_Indir_Access_Ctrl\n", mc_no);
		return -1;
	}

	return 0;
}

/**
 * @brief mgbe_mac_indir_addr_read - MAC Indirect AC register read.
 *
 * Algorithm: Reads MAC Indirect AC register
 *
 * @param[in] base: MGBE virtual base address.
 * @param[in] mc_no: MAC AC Mode Select number
 * @param[in] addr_offset: MAC AC Address Offset.
 * @param[in] value: Pointer MAC AC register value
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_mac_indir_addr_read(struct osi_core_priv_data *osi_core,
				    nveu32_t mc_no,
				    nveu32_t addr_offset,
				    nveu32_t *value)
{
	void *base = osi_core->base;
	nveu32_t addr = 0;

	/* Program MAC_Indir_Access_Ctrl */
	addr = osi_readla(osi_core, (nveu8_t *)base + MGBE_MAC_INDIR_AC);

	/* update Mode Select */
	addr &= ~(MGBE_MAC_INDIR_AC_MSEL);
	addr |= ((mc_no << MGBE_MAC_INDIR_AC_MSEL_SHIFT) &
		  MGBE_MAC_INDIR_AC_MSEL);

	/* update Address Offset */
	addr &= ~(MGBE_MAC_INDIR_AC_AOFF);
	addr |= ((addr_offset << MGBE_MAC_INDIR_AC_AOFF_SHIFT) &
		  MGBE_MAC_INDIR_AC_AOFF);

	/* Set CMD filed bit to 1 for read */
	addr |= MGBE_MAC_INDIR_AC_CMD;

	/* Set OB bit to initiate write */
	addr |= MGBE_MAC_INDIR_AC_OB;

	/* Write MGBE_MAC_L3L4_ADDR_CTR */
	osi_writela(osi_core, addr, (nveu8_t *)base + MGBE_MAC_INDIR_AC);

	/* Wait until OB bit reset */
	if (mgbe_poll_for_mac_acrtl(osi_core) < 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
			     "Fail to write MAC_Indir_Access_Ctrl\n", mc_no);
		return -1;
	}

	/* Read MAC_Indir_Access_Data register value */
	*value = osi_readla(osi_core, (nveu8_t *)base + MGBE_MAC_INDIR_DATA);
	return 0;
}

/**
 * @brief mgbe_config_l2_da_perfect_inverse_match - configure register for
 *	inverse or perfect match.
 *
 * Algorithm: This sequence is used to select perfect/inverse matching
 *	for L2 DA
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] perfect_inverse_match: 1 - inverse mode 0- perfect mode
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static inline void mgbe_config_l2_da_perfect_inverse_match(
					struct osi_core_priv_data *osi_core,
					unsigned int perfect_inverse_match)
{
	unsigned int value = 0U;

	value = osi_readla(osi_core,
			   (unsigned char *)osi_core->base + MGBE_MAC_PFR);
	value &= ~MGBE_MAC_PFR_DAIF;
	if (perfect_inverse_match == OSI_INV_MATCH) {
		/* Set DA Inverse Filtering */
		value |= MGBE_MAC_PFR_DAIF;
	}
	osi_writela(osi_core, value,
		    (unsigned char *)osi_core->base + MGBE_MAC_PFR);
}

/**
 * @brief mgbe_config_mac_pkt_filter_reg - configure mac filter register.
 *
 * Algorithm: This sequence is used to configure MAC in differnet pkt
 *	processing modes like promiscuous, multicast, unicast,
 *	hash unicast/multicast.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter: OSI filter structure.
 *
 * @note 1) MAC should be initialized and started. see osi_start_mac()
 *
 * @retval 0 always
 */
static int mgbe_config_mac_pkt_filter_reg(struct osi_core_priv_data *osi_core,
					  const struct osi_filter *filter)
{
	unsigned int value = 0U;
	int ret = 0;

	value = osi_readla(osi_core,
			   (unsigned char *)osi_core->base + MGBE_MAC_PFR);

	/* Retain all other values */
	value &= (MGBE_MAC_PFR_DAIF | MGBE_MAC_PFR_DBF | MGBE_MAC_PFR_SAIF |
		  MGBE_MAC_PFR_SAF | MGBE_MAC_PFR_PCF | MGBE_MAC_PFR_VTFE |
		  MGBE_MAC_PFR_IPFE | MGBE_MAC_PFR_DNTU | MGBE_MAC_PFR_RA);

	if ((filter->oper_mode & OSI_OPER_EN_PROMISC) != OSI_DISABLE) {
		/* Set Promiscuous Mode Bit */
		value |= MGBE_MAC_PFR_PR;
	}

	if ((filter->oper_mode & OSI_OPER_DIS_PROMISC) != OSI_DISABLE) {
		/* Reset Promiscuous Mode Bit */
		value &= ~MGBE_MAC_PFR_PR;
	}

	if ((filter->oper_mode & OSI_OPER_EN_ALLMULTI) != OSI_DISABLE) {
		/* Set Pass All Multicast Bit */
		value |= MGBE_MAC_PFR_PM;
	}

	if ((filter->oper_mode & OSI_OPER_DIS_ALLMULTI) != OSI_DISABLE) {
		/* Reset Pass All Multicast Bit */
		value &= ~MGBE_MAC_PFR_PM;
	}

	if ((filter->oper_mode & OSI_OPER_EN_PERFECT) != OSI_DISABLE) {
		/* Set Hash or Perfect Filter Bit */
		value |= MGBE_MAC_PFR_HPF;
	}

	if ((filter->oper_mode & OSI_OPER_DIS_PERFECT) != OSI_DISABLE) {
		/* Reset Hash or Perfect Filter Bit */
		value &= ~MGBE_MAC_PFR_HPF;
	}

	osi_writela(osi_core, value,
		    (unsigned char *)osi_core->base + MGBE_MAC_PFR);

	if ((filter->oper_mode & OSI_OPER_EN_L2_DA_INV) != OSI_DISABLE) {
		mgbe_config_l2_da_perfect_inverse_match(osi_core,
							OSI_INV_MATCH);
	}

	if ((filter->oper_mode & OSI_OPER_DIS_L2_DA_INV) != OSI_DISABLE) {
		mgbe_config_l2_da_perfect_inverse_match(osi_core,
							OSI_PFT_MATCH);
	}

	return ret;
}

/**
 * @brief mgbe_filter_args_validate - Validates the filter arguments
 *
 * Algorithm: This function just validates all arguments provided by
 * the osi_filter structure variable.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter: OSI filter structure.
 *
 * @note 1) MAC should be initialized and stated. see osi_start_mac()
 *	 2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_filter_args_validate(struct osi_core_priv_data *const osi_core,
				     const struct osi_filter *filter)
{
	nveu32_t idx = filter->index;
	nveu32_t dma_routing_enable = filter->dma_routing;
	nveu32_t dma_chan = filter->dma_chan;
	nveu32_t addr_mask = filter->addr_mask;
	nveu32_t src_dest = filter->src_dest;
	nveu32_t dma_chansel = filter->dma_chansel;

	/* check for valid index (0 to 31) */
	if (idx >= OSI_MGBE_MAX_MAC_ADDRESS_FILTER) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"invalid MAC filter index\n",
			idx);
		return -1;
	}

	/* check for DMA channel index (0 to 9) */
	if ((dma_chan > OSI_MGBE_MAX_NUM_CHANS - 0x1U) &&
	    (dma_chan != OSI_CHAN_ANY)){
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OUTOFBOUND,
			"invalid dma channel\n",
			(nveul64_t)dma_chan);
		return -1;
	}

	/* validate dma_chansel argument */
	if (dma_chansel > MGBE_MAC_XDCS_DMA_MAX) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OUTOFBOUND,
			"invalid dma_chansel value\n",
			dma_chansel);
		return -1;
	}

	/* validate addr_mask argument */
	if (addr_mask > MGBE_MAB_ADDRH_MBC_MAX_MASK) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			"Invalid addr_mask value\n",
			addr_mask);
		return -1;
	}

	/* validate src_dest argument */
	if (src_dest != OSI_SA_MATCH && src_dest != OSI_DA_MATCH) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			"Invalid src_dest value\n",
			src_dest);
		return -1;
	}

	/* validate dma_routing_enable argument */
	if (dma_routing_enable != OSI_ENABLE &&
		dma_routing_enable != OSI_DISABLE) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			"Invalid dma_routing value\n",
			dma_routing_enable);
		return -1;
	}

	return 0;
}

/**
 * @brief mgbe_update_mac_addr_low_high_reg- Update L2 address in filter
 *	  register
 *
 * Algorithm: This routine update MAC address to register for filtering
 *	based on dma_routing_enable, addr_mask and src_dest. Validation of
 *	dma_chan as well as DCS bit enabled in RXQ to DMA mapping register
 *	performed before updating DCS bits.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter: OSI filter structure.
 *
 * @note 1) MAC should be initialized and stated. see osi_start_mac()
 *	 2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_update_mac_addr_low_high_reg(
				struct osi_core_priv_data *const osi_core,
				const struct osi_filter *filter)
{
	nveu32_t idx = filter->index;
	nveu32_t dma_chan = filter->dma_chan;
	nveu32_t addr_mask = filter->addr_mask;
	nveu32_t src_dest = filter->src_dest;
	const nveu8_t *addr = filter->mac_address;
	nveu32_t dma_chansel = filter->dma_chansel;
	nveu32_t xdcs_check;
	nveu32_t value = 0x0U;
	nve32_t ret = 0;

	/* Validate filter values */
	if (mgbe_filter_args_validate(osi_core, filter) < 0) {
		/* Filter argments validation got failed */
		return -1;
	}

	value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			   MGBE_MAC_ADDRH((idx)));

	/* read current value at index preserve XDCS current value */
	ret = mgbe_mac_indir_addr_read(osi_core,
				       MGBE_MAC_DCHSEL,
				       idx,
				       &xdcs_check);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "indirect register read failed\n", 0ULL);
		return -1;
	}

	/* preserve last XDCS bits */
	xdcs_check &= MGBE_MAC_XDCS_DMA_MAX;

	/* High address reset DCS and AE bits  and XDCS in MAC_DChSel_IndReg */
	if ((filter->oper_mode & OSI_OPER_ADDR_DEL) != OSI_NONE) {
		xdcs_check &= ~OSI_BIT(dma_chan);
		ret = mgbe_mac_indir_addr_write(osi_core, MGBE_MAC_DCHSEL,
						idx, xdcs_check);
		value &= ~(MGBE_MAC_ADDRH_DCS);

		/* XDCS values is always maintained */
		if (xdcs_check == OSI_DISABLE) {
			value &= ~(MGBE_MAC_ADDRH_AE);
		}

		osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
			    MGBE_MAC_ADDRH((idx)));
		return 0;
	}

	/* Add DMA channel to value in binary */
	value = OSI_NONE;
	value |= ((dma_chan << MGBE_MAC_ADDRH_DCS_SHIFT) &
			 MGBE_MAC_ADDRH_DCS);

	if (idx != 0U) {
		/* Add Address mask */
		value |= ((addr_mask << MGBE_MAC_ADDRH_MBC_SHIFT) &
			   MGBE_MAC_ADDRH_MBC);

		/* Setting Source/Destination Address match valid */
		value |= ((src_dest << MGBE_MAC_ADDRH_SA_SHIFT) &
			  MGBE_MAC_ADDRH_SA);
	}

	osi_writela(osi_core, ((unsigned int)addr[4] |
		   ((unsigned int)addr[5] << 8) |
		   MGBE_MAC_ADDRH_AE |
		   value),
		   (unsigned char *)osi_core->base + MGBE_MAC_ADDRH((idx)));

	osi_writela(osi_core, ((unsigned int)addr[0] |
		   ((unsigned int)addr[1] << 8) |
		   ((unsigned int)addr[2] << 16) |
		   ((unsigned int)addr[3] << 24)),
		   (unsigned char *)osi_core->base +  MGBE_MAC_ADDRL((idx)));

	/* Write XDCS configuration into MAC_DChSel_IndReg(x) */
	/* Append DCS DMA channel to XDCS hot bit selection */
	xdcs_check |= (OSI_BIT(dma_chan) | dma_chansel);
	ret = mgbe_mac_indir_addr_write(osi_core,
					MGBE_MAC_DCHSEL,
					idx,
					xdcs_check);

	return ret;
}

/**
 * @brief mgbe_poll_for_l3l4crtl - Poll for L3_L4 filter register operations.
 *
 * Algorithm: Waits for waits for transfer busy bit to be cleared in
 * L3_L4 address control register to complete filter register operations.
 *
 * @param[in] addr: MGBE virtual base address.
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_poll_for_l3l4crtl(struct osi_core_priv_data *osi_core)
{
	unsigned int retry = 10;
	unsigned int count;
	unsigned int l3l4_addr_ctrl = 0;
	int cond = 1;

	/* Poll Until L3_L4_Address_Control XB is clear */
	count = 0;
	while (cond == 1) {
		if (count > retry) {
			/* Return error after max retries */
			return -1;
		}

		count++;

		l3l4_addr_ctrl = osi_readla(osi_core,
					    (unsigned char *)osi_core->base +
					    MGBE_MAC_L3L4_ADDR_CTR);
		if ((l3l4_addr_ctrl & MGBE_MAC_L3L4_ADDR_CTR_XB) == OSI_NONE) {
			/* Set cond to 0 to exit loop */
			cond = 0;
		} else {
			/* wait for 10 usec for XB clear */
			osi_core->osd_ops.udelay(MGBE_MAC_XB_WAIT);
		}
	}

	return 0;
}

/**
 * @brief mgbe_l3l4_filter_write - L3_L4 filter register write.
 *
 * Algorithm: writes L3_L4 filter register
 *
 * @param[in] base: MGBE virtual base address.
 * @param[in] filter_no: MGBE  L3_L4 filter number
 * @param[in] filter_type: MGBE L3_L4 filter register type.
 * @param[in] value: MGBE  L3_L4 filter register value
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_l3l4_filter_write(struct osi_core_priv_data *osi_core,
				  unsigned int filter_no,
				  unsigned int filter_type,
				  unsigned int value)
{
	void *base = osi_core->base;
	unsigned int addr = 0;

	/* Write MAC_L3_L4_Data register value */
	osi_writela(osi_core, value,
		    (unsigned char *)base + MGBE_MAC_L3L4_DATA);

	/* Program MAC_L3_L4_Address_Control */
	addr = osi_readla(osi_core,
			  (unsigned char *)base + MGBE_MAC_L3L4_ADDR_CTR);

	/* update filter number */
	addr &= ~(MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM);
	addr |= ((filter_no << MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM_SHIFT) &
		  MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM);

	/* update filter type */
	addr &= ~(MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE);
	addr |= ((filter_type << MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE_SHIFT) &
		  MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE);

	/* Set TT filed 0 for write */
	addr &= ~(MGBE_MAC_L3L4_ADDR_CTR_TT);

	/* Set XB bit to initiate write */
	addr |= MGBE_MAC_L3L4_ADDR_CTR_XB;

	/* Write MGBE_MAC_L3L4_ADDR_CTR */
	osi_writela(osi_core, addr,
		    (unsigned char *)base + MGBE_MAC_L3L4_ADDR_CTR);

	/* Wait untile XB bit reset */
	if (mgbe_poll_for_l3l4crtl(osi_core) < 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
			     "Fail to write L3_L4_Address_Control\n",
			     filter_type);
		return -1;
	}

	return 0;
}

/**
 * @brief mgbe_l3l4_filter_read - L3_L4 filter register read.
 *
 * Algorithm: writes L3_L4 filter register
 *
 * @param[in] base: MGBE virtual base address.
 * @param[in] filter_no: MGBE  L3_L4 filter number
 * @param[in] filter_type: MGBE L3_L4 filter register type.
 * @param[in] *value: Pointer MGBE L3_L4 filter register value
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_l3l4_filter_read(struct osi_core_priv_data *osi_core,
				 unsigned int filter_no,
				 unsigned int filter_type,
				 unsigned int *value)
{
	void *base = osi_core->base;
	unsigned int addr = 0;

	/* Program MAC_L3_L4_Address_Control */
	addr = osi_readla(osi_core,
			  (unsigned char *)base + MGBE_MAC_L3L4_ADDR_CTR);

	/* update filter number */
	addr &= ~(MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM);
	addr |= ((filter_no << MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM_SHIFT) &
		  MGBE_MAC_L3L4_ADDR_CTR_IDDR_FNUM);

	/* update filter type */
	addr &= ~(MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE);
	addr |= ((filter_type << MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE_SHIFT) &
		  MGBE_MAC_L3L4_ADDR_CTR_IDDR_FTYPE);

	/* Set TT field 1 for read */
	addr |= MGBE_MAC_L3L4_ADDR_CTR_TT;

	/* Set XB bit to initiate write */
	addr |= MGBE_MAC_L3L4_ADDR_CTR_XB;

	/* Write MGBE_MAC_L3L4_ADDR_CTR */
	osi_writela(osi_core, addr,
		    (unsigned char *)base + MGBE_MAC_L3L4_ADDR_CTR);

	/* Wait untile XB bit reset */
	if (mgbe_poll_for_l3l4crtl(osi_core) < 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
			    "Fail to read L3L4 Address\n",
			    filter_type);
		return -1;
	}

	/* Read the MGBE_MAC_L3L4_DATA for filter register data */
	*value = osi_readla(osi_core,
			    (unsigned char *)base + MGBE_MAC_L3L4_DATA);
	return 0;
}

/**
 * @brief mgbe_update_ip4_addr - configure register for IPV4 address filtering
 *
 * Algorithm:  This sequence is used to update IPv4 source/destination
 *	Address for L3 layer filtering
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] addr: ipv4 address
 * @param[in] src_dst_addr_match: 0 - source addr otherwise - dest addr
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_update_ip4_addr(struct osi_core_priv_data *osi_core,
				const unsigned int filter_no,
				const unsigned char addr[],
				const unsigned int src_dst_addr_match)
{
	unsigned int value = 0U;
	unsigned int temp = 0U;
	int ret = 0;

	if (addr == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"invalid address\n",
			0ULL);
		return -1;
	}

	if (filter_no >= OSI_MGBE_MAX_L3_L4_FILTER) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OUTOFBOUND,
			"invalid filter index for L3/L4 filter\n",
			(unsigned long long)filter_no);
		return -1;
	}

	/* validate src_dst_addr_match argument */
	if (src_dst_addr_match != OSI_SOURCE_MATCH &&
	    src_dst_addr_match != OSI_INV_MATCH) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid src_dst_addr_match value\n",
			src_dst_addr_match);
		return -1;
	}

	value = addr[3];
	temp = (unsigned int)addr[2] << 8;
	value |= temp;
	temp = (unsigned int)addr[1] << 16;
	value |= temp;
	temp = (unsigned int)addr[0] << 24;
	value |= temp;
	if (src_dst_addr_match == OSI_SOURCE_MATCH) {
		ret = mgbe_l3l4_filter_write(osi_core,
					     filter_no,
					     MGBE_MAC_L3_AD0R,
					     value);
	} else {
		ret = mgbe_l3l4_filter_write(osi_core,
					     filter_no,
					     MGBE_MAC_L3_AD1R,
					     value);
	}

	return ret;
}

/**
 * @brief mgbe_update_ip6_addr - add ipv6 address in register
 *
 * Algorithm: This sequence is used to update IPv6 source/destination
 *	      Address for L3 layer filtering
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] addr: ipv6 adderss
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_update_ip6_addr(struct osi_core_priv_data *osi_core,
				const unsigned int filter_no,
				const unsigned short addr[])
{
	unsigned int value = 0U;
	unsigned int temp = 0U;
	int ret = 0;

	if (addr == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"invalid address\n",
			0ULL);
		return -1;
	}

	if (filter_no >= OSI_MGBE_MAX_L3_L4_FILTER) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"invalid filter index for L3/L4 filter\n",
			(unsigned long long)filter_no);
		return -1;
	}

	/* update Bits[31:0] of 128-bit IP addr */
	value = addr[7];
	temp = (unsigned int)addr[6] << 16;
	value |= temp;

	ret = mgbe_l3l4_filter_write(osi_core, filter_no,
				     MGBE_MAC_L3_AD0R, value);
	if (ret < 0) {
		/* Write MGBE_MAC_L3_AD0R fail return error */
		return ret;
	}
	/* update Bits[63:32] of 128-bit IP addr */
	value = addr[5];
	temp = (unsigned int)addr[4] << 16;
	value |= temp;

	ret = mgbe_l3l4_filter_write(osi_core, filter_no,
				     MGBE_MAC_L3_AD1R, value);
	if (ret < 0) {
		/* Write MGBE_MAC_L3_AD1R fail return error */
		return ret;
	}
	/* update Bits[95:64] of 128-bit IP addr */
	value = addr[3];
	temp = (unsigned int)addr[2] << 16;
	value |= temp;

	ret = mgbe_l3l4_filter_write(osi_core, filter_no,
				     MGBE_MAC_L3_AD2R, value);
	if (ret < 0) {
		/* Write MGBE_MAC_L3_AD2R fail return error */
		return ret;
	}

	/* update Bits[127:96] of 128-bit IP addr */
	value = addr[1];
	temp = (unsigned int)addr[0] << 16;
	value |= temp;

	return mgbe_l3l4_filter_write(osi_core, filter_no,
				      MGBE_MAC_L3_AD3R, value);
}

/**
 * @brief mgbe_config_l3_l4_filter_enable - register write to enable L3/L4
 *	filters.
 *
 * Algorithm: This routine to enable/disable L3/l4 filter
 *
 * @param[in] osi_core: OSI core private data structure.
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_l3_l4_filter_enable(
				struct osi_core_priv_data *const osi_core,
				unsigned int filter_enb_dis)
{
	unsigned int value = 0U;
	void *base = osi_core->base;

	/* validate filter_enb_dis argument */
	if (filter_enb_dis != OSI_ENABLE && filter_enb_dis != OSI_DISABLE) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			"Invalid filter_enb_dis value\n",
			filter_enb_dis);
		return -1;
	}

	value = osi_readla(osi_core, (unsigned char *)base + MGBE_MAC_PFR);
	value &= ~(MGBE_MAC_PFR_IPFE);
	value |= ((filter_enb_dis << MGBE_MAC_PFR_IPFE_SHIFT) &
		  MGBE_MAC_PFR_IPFE);
	osi_writela(osi_core, value, (unsigned char *)base + MGBE_MAC_PFR);

	return 0;
}

/**
 * @brief mgbe_update_l4_port_no -program source  port no
 *
 * Algorithm: sequence is used to update Source Port Number for
 *	L4(TCP/UDP) layer filtering.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] port_no: port number
 * @param[in] src_dst_port_match: 0 - source port, otherwise - dest port
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) osi_core->osd should be populated
 *	 3) DCS bits should be enabled in RXQ to DMA mapping register
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_update_l4_port_no(struct osi_core_priv_data *osi_core,
				  unsigned int filter_no,
				  unsigned short port_no,
				  unsigned int src_dst_port_match)
{
	unsigned int value = 0U;
	unsigned int temp = 0U;
	int ret = 0;

	if (filter_no >= OSI_MGBE_MAX_L3_L4_FILTER) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OUTOFBOUND,
			"invalid filter index for L3/L4 filter\n",
			(unsigned long long)filter_no);
		return -1;
	}

	ret = mgbe_l3l4_filter_read(osi_core, filter_no,
				    MGBE_MAC_L4_ADDR, &value);
	if (ret < 0) {
		/* Read MGBE_MAC_L4_ADDR fail return error */
		return ret;
	}

	if (src_dst_port_match == OSI_SOURCE_MATCH) {
		value &= ~MGBE_MAC_L4_ADDR_SP_MASK;
		value |= ((unsigned int)port_no  & MGBE_MAC_L4_ADDR_SP_MASK);
	} else {
		value &= ~MGBE_MAC_L4_ADDR_DP_MASK;
		temp = port_no;
		value |= ((temp << MGBE_MAC_L4_ADDR_DP_SHIFT) &
			  MGBE_MAC_L4_ADDR_DP_MASK);
	}

	return mgbe_l3l4_filter_write(osi_core, filter_no,
				      MGBE_MAC_L4_ADDR, value);
}

/**
 * @brief mgbe_set_dcs - check and update dma routing register
 *
 * Algorithm: Check for request for DCS_enable as well as validate chan
 *	number and dcs_enable is set. After validation, this sequence is used
 *	to configure L3((IPv4/IPv6) filters for address matching.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] value: unsigned int value for caller
 * @param[in] dma_routing_enable: filter based dma routing enable(1)
 * @param[in] dma_chan: dma channel for routing based on filter
 *
 * @note 1) MAC IP should be out of reset and need to be initialized
 *	 as the requirements.
 *	 2) DCS bit of RxQ should be enabled for dynamic channel selection
 *	    in filter support
 *
 * @retval updated unsigned int value param
 */
static inline unsigned int mgbe_set_dcs(struct osi_core_priv_data *osi_core,
					unsigned int value,
					unsigned int dma_routing_enable,
					unsigned int dma_chan)
{
	if ((dma_routing_enable == OSI_ENABLE) && (dma_chan <
	    OSI_MGBE_MAX_NUM_CHANS) && (osi_core->dcs_en ==
	    OSI_ENABLE)) {
		value |= ((dma_routing_enable <<
			  MGBE_MAC_L3L4_CTR_DMCHEN0_SHIFT) &
			  MGBE_MAC_L3L4_CTR_DMCHEN0);
		value |= ((dma_chan <<
			  MGBE_MAC_L3L4_CTR_DMCHN0_SHIFT) &
			  MGBE_MAC_L3L4_CTR_DMCHN0);
	}

	return value;
}

/**
 * @brief mgbe_helper_l3l4_bitmask - helper function to set L3L4
 * bitmask.
 *
 * Algorithm: set bit corresponding to L3l4 filter index
 *
 * @param[in] bitmask: bit mask OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] value:  0 - disable  otherwise - l3/l4 filter enabled
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 */
static inline void mgbe_helper_l3l4_bitmask(unsigned int *bitmask,
					    unsigned int filter_no,
					    unsigned int value)
{
	unsigned int temp;

	temp = OSI_ENABLE;
	temp = temp << filter_no;

	/* check against all bit fields for L3L4 filter enable */
	if ((value & MGBE_MAC_L3L4_CTRL_ALL) != OSI_DISABLE) {
		/* Set bit mask for index */
		*bitmask |= temp;
	} else {
		/* Reset bit mask for index */
		*bitmask &= ~temp;
	}
}

/**
 * @brief mgbe_config_l3_filters - config L3 filters.
 *
 * Algorithm: Check for DCS_enable as well as validate channel
 *	number and if dcs_enable is set. After validation, code flow
 *	is used to configure L3((IPv4/IPv6) filters resister
 *	for address matching.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] enb_dis:  1 - enable otherwise - disable L3 filter
 * @param[in] ipv4_ipv6_match: 1 - IPv6, otherwise - IPv4
 * @param[in] src_dst_addr_match: 0 - source, otherwise - destination
 * @param[in] perfect_inverse_match: normal match(0) or inverse map(1)
 * @param[in] dma_routing_enable: filter based dma routing enable(1)
 * @param[in] dma_chan: dma channel for routing based on filter
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) osi_core->osd should be populated
 *	 3) DCS bit of RxQ should be enabled for dynamic channel selection
 *	    in filter support
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_l3_filters(struct osi_core_priv_data *osi_core,
				  unsigned int filter_no,
				  unsigned int enb_dis,
				  unsigned int ipv4_ipv6_match,
				  unsigned int src_dst_addr_match,
				  unsigned int perfect_inverse_match,
				  unsigned int dma_routing_enable,
				  unsigned int dma_chan)
{
	unsigned int value = 0U;
	int ret = 0;

	if (filter_no >= OSI_MGBE_MAX_L3_L4_FILTER) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OUTOFBOUND,
			"invalid filter index for L3/L4 filter\n",
			(unsigned long long)filter_no);
		return -1;
	}
	/* validate enb_dis argument */
	if (enb_dis != OSI_ENABLE && enb_dis != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid filter_enb_dis value\n",
			enb_dis);
		return -1;
	}
	/* validate ipv4_ipv6_match argument */
	if (ipv4_ipv6_match != OSI_IPV6_MATCH &&
	    ipv4_ipv6_match != OSI_IPV4_MATCH) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid ipv4_ipv6_match value\n",
			ipv4_ipv6_match);
		return -1;
	}
	/* validate src_dst_addr_match argument */
	if (src_dst_addr_match != OSI_SOURCE_MATCH &&
	    src_dst_addr_match != OSI_INV_MATCH) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid src_dst_addr_match value\n",
			src_dst_addr_match);
		return -1;
	}
	/* validate perfect_inverse_match argument */
	if (perfect_inverse_match != OSI_ENABLE &&
	    perfect_inverse_match != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid perfect_inverse_match value\n",
			perfect_inverse_match);
		return -1;
	}
	if ((dma_routing_enable == OSI_ENABLE) &&
	    (dma_chan > OSI_MGBE_MAX_NUM_CHANS - 1U)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OUTOFBOUND,
			"Wrong DMA channel\n",
			(unsigned long long)dma_chan);
		return -1;
	}

	ret = mgbe_l3l4_filter_read(osi_core, filter_no,
				    MGBE_MAC_L3L4_CTR, &value);
	if (ret < 0) {
		/* MGBE_MAC_L3L4_CTR read fail return here */
		return ret;
	}

	value &= ~MGBE_MAC_L3L4_CTR_L3PEN0;
	value |= (ipv4_ipv6_match  & MGBE_MAC_L3L4_CTR_L3PEN0);

	/* For IPv6 either SA/DA can be checked not both */
	if (ipv4_ipv6_match == OSI_IPV6_MATCH) {
		if (enb_dis == OSI_ENABLE) {
			if (src_dst_addr_match == OSI_SOURCE_MATCH) {
				/* Enable L3 filters for IPv6 SOURCE addr
				 *  matching
				 */
				value &= ~MGBE_MAC_L3_IP6_CTRL_CLEAR;
				value |= ((MGBE_MAC_L3L4_CTR_L3SAM0 |
					  perfect_inverse_match <<
					  MGBE_MAC_L3L4_CTR_L3SAIM0_SHIFT) &
					  ((MGBE_MAC_L3L4_CTR_L3SAM0 |
					  MGBE_MAC_L3L4_CTR_L3SAIM0)));
				value |= mgbe_set_dcs(osi_core, value,
						      dma_routing_enable,
						      dma_chan);

			} else {
				/* Enable L3 filters for IPv6 DESTINATION addr
				 * matching
				 */
				value &= ~MGBE_MAC_L3_IP6_CTRL_CLEAR;
				value |= ((MGBE_MAC_L3L4_CTR_L3DAM0 |
					  perfect_inverse_match <<
					  MGBE_MAC_L3L4_CTR_L3DAIM0_SHIFT) &
					  ((MGBE_MAC_L3L4_CTR_L3DAM0 |
					  MGBE_MAC_L3L4_CTR_L3DAIM0)));
				value |= mgbe_set_dcs(osi_core, value,
						      dma_routing_enable,
						      dma_chan);
			}
		} else {
			/* Disable L3 filters for IPv6 SOURCE/DESTINATION addr
			 * matching
			 */
			value &= ~(MGBE_MAC_L3_IP6_CTRL_CLEAR |
				   MGBE_MAC_L3L4_CTR_L3PEN0);
		}
	} else {
		if (src_dst_addr_match == OSI_SOURCE_MATCH) {
			if (enb_dis == OSI_ENABLE) {
				/* Enable L3 filters for IPv4 SOURCE addr
				 * matching
				 */
				value &= ~MGBE_MAC_L3_IP4_SA_CTRL_CLEAR;
				value |= ((MGBE_MAC_L3L4_CTR_L3SAM0 |
					  perfect_inverse_match <<
					  MGBE_MAC_L3L4_CTR_L3SAIM0_SHIFT) &
					  ((MGBE_MAC_L3L4_CTR_L3SAM0 |
					  MGBE_MAC_L3L4_CTR_L3SAIM0)));
				value |= mgbe_set_dcs(osi_core, value,
						      dma_routing_enable,
						      dma_chan);
			} else {
				/* Disable L3 filters for IPv4 SOURCE addr
				 * matching
				 */
				value &= ~MGBE_MAC_L3_IP4_SA_CTRL_CLEAR;
			}
		} else {
			if (enb_dis == OSI_ENABLE) {
				/* Enable L3 filters for IPv4 DESTINATION addr
				 * matching
				 */
				value &= ~MGBE_MAC_L3_IP4_DA_CTRL_CLEAR;
				value |= ((MGBE_MAC_L3L4_CTR_L3DAM0 |
					  perfect_inverse_match <<
					  MGBE_MAC_L3L4_CTR_L3DAIM0_SHIFT) &
					  ((MGBE_MAC_L3L4_CTR_L3DAM0 |
					  MGBE_MAC_L3L4_CTR_L3DAIM0)));
				value |= mgbe_set_dcs(osi_core, value,
						      dma_routing_enable,
						      dma_chan);
			} else {
				/* Disable L3 filters for IPv4 DESTINATION addr
				 * matching
				 */
				value &= ~MGBE_MAC_L3_IP4_DA_CTRL_CLEAR;
			}
		}
	}

	ret = mgbe_l3l4_filter_write(osi_core, filter_no,
				     MGBE_MAC_L3L4_CTR, value);
	if (ret < 0) {
		/* Write MGBE_MAC_L3L4_CTR fail return error */
		return ret;
	}

	/* Set bit corresponding to filter index if value is non-zero */
	mgbe_helper_l3l4_bitmask(&osi_core->l3l4_filter_bitmask,
				 filter_no, value);

	return ret;
}

/**
 * @brief mgbe_config_l4_filters - Config L4 filters.
 *
 * Algorithm: This sequence is used to configure L4(TCP/UDP) filters for
 *	SA and DA Port Number matching
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] enb_dis: 1 - enable, otherwise - disable L4 filter
 * @param[in] tcp_udp_match: 1 - udp, 0 - tcp
 * @param[in] src_dst_port_match: 0 - source port, otherwise - dest port
 * @param[in] perfect_inverse_match: normal match(0) or inverse map(1)
 * @param[in] dma_routing_enable: filter based dma routing enable(1)
 * @param[in] dma_chan: dma channel for routing based on filter
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_l4_filters(struct osi_core_priv_data *osi_core,
				  unsigned int filter_no,
				  unsigned int enb_dis,
				  unsigned int tcp_udp_match,
				  unsigned int src_dst_port_match,
				  unsigned int perfect_inverse_match,
				  unsigned int dma_routing_enable,
				  unsigned int dma_chan)
{
	unsigned int value = 0U;
	int ret = 0;

	if (filter_no >= OSI_MGBE_MAX_L3_L4_FILTER) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OUTOFBOUND,
			"invalid filter index for L3/L4 filter\n",
			(unsigned long long)filter_no);
		return -1;
	}
	/* validate enb_dis argument */
	if (enb_dis != OSI_ENABLE && enb_dis != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid filter_enb_dis value\n",
			enb_dis);
		return -1;
	}
	/* validate tcp_udp_match argument */
	if (tcp_udp_match != OSI_ENABLE && tcp_udp_match != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid tcp_udp_match value\n",
			tcp_udp_match);
		return -1;
	}
	/* validate src_dst_port_match argument */
	if (src_dst_port_match != OSI_SOURCE_MATCH &&
	    src_dst_port_match != OSI_INV_MATCH) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid src_dst_port_match value\n",
			src_dst_port_match);
		return -1;
	}
	/* validate perfect_inverse_match argument */
	if (perfect_inverse_match != OSI_ENABLE &&
	    perfect_inverse_match != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid perfect_inverse_match value\n",
			perfect_inverse_match);
		return -1;
	}
	if ((dma_routing_enable == OSI_ENABLE) &&
	    (dma_chan > OSI_MGBE_MAX_NUM_CHANS - 1U)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OUTOFBOUND,
			"Wrong DMA channel\n",
			(unsigned int)dma_chan);
		return -1;
	}

	ret = mgbe_l3l4_filter_read(osi_core, filter_no,
				    MGBE_MAC_L3L4_CTR, &value);
	if (ret < 0) {
		/* MGBE_MAC_L3L4_CTR read fail return here */
		return ret;
	}

	value &= ~MGBE_MAC_L3L4_CTR_L4PEN0;
	value |= ((tcp_udp_match << 16) & MGBE_MAC_L3L4_CTR_L4PEN0);

	if (src_dst_port_match == OSI_SOURCE_MATCH) {
		if (enb_dis == OSI_ENABLE) {
			/* Enable L4 filters for SOURCE Port No matching */
			value &= ~MGBE_MAC_L4_SP_CTRL_CLEAR;
			value |= ((MGBE_MAC_L3L4_CTR_L4SPM0 |
				  perfect_inverse_match <<
				  MGBE_MAC_L3L4_CTR_L4SPIM0_SHIFT) &
				  (MGBE_MAC_L3L4_CTR_L4SPM0 |
				  MGBE_MAC_L3L4_CTR_L4SPIM0));
			value |= mgbe_set_dcs(osi_core, value,
					      dma_routing_enable,
					      dma_chan);
		} else {
			/* Disable L4 filters for SOURCE Port No matching  */
			value &= ~MGBE_MAC_L4_SP_CTRL_CLEAR;
		}
	} else {
		if (enb_dis == OSI_ENABLE) {
			/* Enable L4 filters for DESTINATION port No
			 * matching
			 */
			value &= ~MGBE_MAC_L4_DP_CTRL_CLEAR;
			value |= ((MGBE_MAC_L3L4_CTR_L4DPM0 |
				  perfect_inverse_match <<
				  MGBE_MAC_L3L4_CTR_L4DPIM0_SHIFT) &
				  (MGBE_MAC_L3L4_CTR_L4DPM0 |
				  MGBE_MAC_L3L4_CTR_L4DPIM0));
			value |= mgbe_set_dcs(osi_core, value,
					      dma_routing_enable,
					      dma_chan);
		} else {
			/* Disable L4 filters for DESTINATION port No
			 * matching
			 */
			value &= ~MGBE_MAC_L4_DP_CTRL_CLEAR;
		}
	}

	ret = mgbe_l3l4_filter_write(osi_core, filter_no,
				     MGBE_MAC_L3L4_CTR, value);
	if (ret < 0) {
		/* Write MGBE_MAC_L3L4_CTR fail return error */
		return ret;
	}

	/* Set bit corresponding to filter index if value is non-zero */
	mgbe_helper_l3l4_bitmask(&osi_core->l3l4_filter_bitmask,
				 filter_no, value);

	return ret;
}

/**
 * @brief mgbe_config_vlan_filter_reg - config vlan filter register
 *
 * Algorithm: This sequence is used to enable/disable VLAN filtering and
 *	also selects VLAN filtering mode- perfect/hash
 *
 * @param[in] osi_core: Base address  from OSI core private data structure.
 * @param[in] filter_enb_dis: vlan filter enable/disable
 * @param[in] perfect_hash_filtering: perfect or hash filter
 * @param[in] perfect_inverse_match: normal or inverse filter
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_vlan_filtering(struct osi_core_priv_data *osi_core,
				      unsigned int filter_enb_dis,
				      unsigned int perfect_hash_filtering,
				      unsigned int perfect_inverse_match)
{
	unsigned int value;
	unsigned char *base = osi_core->base;

	/* validate perfect_inverse_match argument */
	if (perfect_hash_filtering == OSI_HASH_FILTER_MODE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OPNOTSUPP,
			"VLAN hash filter is not supported, VTHM not updated\n",
			0ULL);
		return -1;
	}
	if (perfect_hash_filtering != OSI_PERFECT_FILTER_MODE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid perfect_hash_filtering value\n",
			perfect_hash_filtering);
		return -1;
	}

	/* validate filter_enb_dis argument */
	if (filter_enb_dis != OSI_ENABLE && filter_enb_dis != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid filter_enb_dis value\n",
			filter_enb_dis);
		return -1;
	}

	/* validate perfect_inverse_match argument */
	if (perfect_inverse_match != OSI_ENABLE &&
	    perfect_inverse_match != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid perfect_inverse_match value\n",
			perfect_inverse_match);
		return -1;
	}

	/* Read MAC PFR value set VTFE bit */
	value = osi_readla(osi_core, base + MGBE_MAC_PFR);
	value &= ~(MGBE_MAC_PFR_VTFE);
	value |= ((filter_enb_dis << MGBE_MAC_PFR_VTFE_SHIFT) &
		  MGBE_MAC_PFR_VTFE);
	osi_writela(osi_core, value, base + MGBE_MAC_PFR);

	/* Read MAC VLAN TR register value set VTIM bit */
	value = osi_readla(osi_core, base + MGBE_MAC_VLAN_TR);
	value &= ~(MGBE_MAC_VLAN_TR_VTIM | MGBE_MAC_VLAN_TR_VTHM);
	value |= ((perfect_inverse_match << MGBE_MAC_VLAN_TR_VTIM_SHIFT) &
		  MGBE_MAC_VLAN_TR_VTIM);
	osi_writela(osi_core, value, base + MGBE_MAC_VLAN_TR);

	return 0;
}

/**
 * @brief mgbe_update_vlan_id - update VLAN ID in Tag register
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] vid: VLAN ID to be programmed.
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static inline int mgbe_update_vlan_id(struct osi_core_priv_data *const osi_core,
				      unsigned int vid)
{
	return 0;
}

/**
 * @brief mgbe_config_ptp_rxq - Config PTP RX packets queue route
 *
 * Algorithm: This function is used to program the PTP RX packets queue.
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] rxq_idx: PTP RXQ index.
 * @param[in] enable: PTP RXQ route enable(1) or disable(0).
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_ptp_rxq(struct osi_core_priv_data *const osi_core,
			       const unsigned int rxq_idx,
			       const unsigned int enable)
{
	unsigned char *base = osi_core->base;
	unsigned int value = 0U;
	unsigned int i = 0U;

	/* Validate the RX queue index argument */
	if (rxq_idx >= OSI_MGBE_MAX_NUM_QUEUES) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid PTP RX queue index\n",
			rxq_idx);
		return -1;
	}

	/* Validate enable argument */
	if (enable != OSI_ENABLE && enable != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid enable input\n",
			enable);
		return -1;
	}

	/* Validate PTP RX queue enable */
	for (i = 0; i < osi_core->num_mtl_queues; i++) {
		if (osi_core->mtl_queues[i] == rxq_idx) {
			/* Given PTP RX queue is enabled */
			break;
		}
	}
	if (i == osi_core->num_mtl_queues) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"PTP RX queue not enabled\n",
			rxq_idx);
		return -1;
	}

	/* Read MAC_RxQ_Ctrl1 */
	value = osi_readla(osi_core, base + MGBE_MAC_RQC1R);
	/* Check for enable or disable */
	if (enable == OSI_DISABLE) {
		/** Reset OMCBCQ bit to disable over-riding the MCBC Queue
		 * priority for the PTP RX queue.
		 **/
		value &= ~MGBE_MAC_RQC1R_OMCBCQ;
	} else {
		/* Store PTP RX queue into OSI private data */
		osi_core->ptp_config.ptp_rx_queue = rxq_idx;
		/* Program PTPQ with ptp_rxq */
		value &= ~MGBE_MAC_RQC1R_PTPQ;
		value |= (rxq_idx << MGBE_MAC_RQC1R_PTPQ_SHIFT);
		/** Set TPQC to 0x1 for VLAN Tagged PTP over
		 * ethernet packets are routed to Rx Queue specified
		 * by PTPQ field
		 **/
		value |= MGBE_MAC_RQC1R_TPQC0;
		/** Set OMCBCQ bit to enable over-riding the MCBC Queue
		 * priority for the PTP RX queue.
		 **/
		value |= MGBE_MAC_RQC1R_OMCBCQ;
	}
	/* Write MAC_RxQ_Ctrl1 */
	osi_writela(osi_core, value, base + MGBE_MAC_RQC1R);

	return 0;
}

/**
 * @brief mgbe_flush_mtl_tx_queue - Flush MTL Tx queue
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] qinx: MTL queue index.
 *
 * @note 1) MAC should out of reset and clocks enabled.
 *	 2) hw core initialized. see osi_hw_core_init().
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t mgbe_flush_mtl_tx_queue(
				struct osi_core_priv_data *const osi_core,
				const nveu32_t qinx)
{
	void *addr = osi_core->base;
	nveu32_t retry = 1000;
	nveu32_t count;
	nveu32_t value;
	nve32_t cond = 1;

	if (qinx >= OSI_MGBE_MAX_NUM_QUEUES) {
		return -1;
	}

	/* Read Tx Q Operating Mode Register and flush TxQ */
	value = osi_readla(osi_core, (nveu8_t *)addr +
			  MGBE_MTL_CHX_TX_OP_MODE(qinx));
	value |= MGBE_MTL_QTOMR_FTQ;
	osi_writela(osi_core, value, (nveu8_t *)addr +
		   MGBE_MTL_CHX_TX_OP_MODE(qinx));

	/* Poll Until FTQ bit resets for Successful Tx Q flush */
	count = 0;
	while (cond == 1) {
		if (count > retry) {
			return -1;
		}

		count++;

		value = osi_readla(osi_core, (nveu8_t *)addr +
				  MGBE_MTL_CHX_TX_OP_MODE(qinx));
		if ((value & MGBE_MTL_QTOMR_FTQ_LPOS) == OSI_NONE) {
			cond = 0;
		} else {
			osi_core->osd_ops.msleep(1);
		}
	}

	return 0;
}

/**
 * @brief mgbe_config_mac_loopback - Configure MAC to support loopback
 *
 * @param[in] addr: Base address indicating the start of
 *            memory mapped IO region of the MAC.
 * @param[in] lb_mode: Enable or Disable MAC loopback mode
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_mac_loopback(struct osi_core_priv_data *const osi_core,
				    unsigned int lb_mode)
{
	unsigned int value;
	void *addr = osi_core->base;

	/* don't allow only if loopback mode is other than 0 or 1 */
	if (lb_mode != OSI_ENABLE && lb_mode != OSI_DISABLE) {
		return -1;
	}

	/* Read MAC Configuration Register */
	value = osi_readla(osi_core, (unsigned char *)addr + MGBE_MAC_RMCR);
	if (lb_mode == OSI_ENABLE) {
		/* Enable Loopback Mode */
		value |= MGBE_MAC_RMCR_LM;
	} else {
		value &= ~MGBE_MAC_RMCR_LM;
	}

	osi_writela(osi_core, value, (unsigned char *)addr + MGBE_MAC_RMCR);

	return 0;
}

/**
 * @brief mgbe_config_arp_offload - Enable/Disable ARP offload
 *
 * Algorithm:
 *      1) Read the MAC configuration register
 *      2) If ARP offload is to be enabled, program the IP address in
 *      ARPPA register
 *      3) Enable/disable the ARPEN bit in MCR and write back to the MCR.
 *
 * @param[in] addr: MGBE virtual base address.
 * @param[in] enable: Flag variable to enable/disable ARP offload
 * @param[in] ip_addr: IP address of device to be programmed in HW.
 *            HW will use this IP address to respond to ARP requests.
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *       2) Valid 4 byte IP address as argument ip_addr
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_arp_offload(struct osi_core_priv_data *const osi_core,
				   const unsigned int enable,
				   const unsigned char *ip_addr)
{
	unsigned int mac_rmcr;
	unsigned int val;
	void *addr = osi_core->base;

	if (enable != OSI_ENABLE && enable != OSI_DISABLE) {
		return -1;
	}

	mac_rmcr = osi_readla(osi_core, (unsigned char *)addr + MGBE_MAC_RMCR);

	if (enable == OSI_ENABLE) {
		val = (((unsigned int)ip_addr[0]) << 24) |
		       (((unsigned int)ip_addr[1]) << 16) |
		       (((unsigned int)ip_addr[2]) << 8) |
		       (((unsigned int)ip_addr[3]));

		osi_writela(osi_core, val,
			    (unsigned char *)addr + MGBE_MAC_ARPPA);

		mac_rmcr |= MGBE_MAC_RMCR_ARPEN;
	} else {
		mac_rmcr &= ~MGBE_MAC_RMCR_ARPEN;
	}

	osi_writela(osi_core, mac_rmcr, (unsigned char *)addr + MGBE_MAC_RMCR);

	return 0;
}

/**
 * @brief mgbe_config_rxcsum_offload - Enable/Disale rx checksum offload in HW
 *
 * Algorithm:
 *      1) Read the MAC configuration register.
 *      2) Enable the IP checksum offload engine COE in MAC receiver.
 *      3) Update the MAC configuration register.
 *
 * @param[in] addr: MGBE virtual base address.
 * @param[in] enabled: Flag to indicate feature is to be enabled/disabled.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_rxcsum_offload(
				struct osi_core_priv_data *const osi_core,
				unsigned int enabled)
{
	void *addr = osi_core->base;
	unsigned int mac_rmcr;

	if (enabled != OSI_ENABLE && enabled != OSI_DISABLE) {
		return -1;
	}

	mac_rmcr = osi_readla(osi_core, (unsigned char *)addr + MGBE_MAC_RMCR);
	if (enabled == OSI_ENABLE) {
		mac_rmcr |= MGBE_MAC_RMCR_IPC;
	} else {
		mac_rmcr &= ~MGBE_MAC_RMCR_IPC;
	}

	osi_writela(osi_core, mac_rmcr, (unsigned char *)addr + MGBE_MAC_RMCR);

	return 0;
}

/**
 * @brief mgbe_config_frp - Enable/Disale RX Flexible Receive Parser in HW
 *
 * Algorithm:
 *      1) Read the MTL OP Mode configuration register.
 *      2) Enable/Disable FRPE bit based on the input.
 *      3) Write the MTL OP Mode configuration register.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enabled: Flag to indicate feature is to be enabled/disabled.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_frp(struct osi_core_priv_data *const osi_core,
			   const unsigned int enabled)
{
	unsigned char *base = osi_core->base;
	unsigned int op_mode = 0U, val = 0U;
	int ret = -1;

	if (enabled != OSI_ENABLE && enabled != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid enable input\n",
			enabled);
		return -1;
	}

	op_mode = osi_readla(osi_core, base + MGBE_MTL_OP_MODE);
	if (enabled == OSI_ENABLE) {
		/* Set FRPE bit of MTL_Operation_Mode register */
		op_mode |= MGBE_MTL_OP_MODE_FRPE;
		osi_writela(osi_core, op_mode, base + MGBE_MTL_OP_MODE);

		/* Verify RXPI bit set in MTL_RXP_Control_Status */
		ret = osi_readl_poll_timeout((base + MGBE_MTL_RXP_CS),
					     (osi_core->osd_ops.udelay),
					     (val),
					     ((val & MGBE_MTL_RXP_CS_RXPI) ==
					      MGBE_MTL_RXP_CS_RXPI),
					     (MGBE_MTL_FRP_READ_UDELAY),
					     (MGBE_MTL_FRP_READ_RETRY));
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				"Fail to enable FRP\n",
				val);
			return -1;
		}

		/* Enable FRP Interrupts in MTL_RXP_Interrupt_Control_Status */
		val = osi_readla(osi_core, base + MGBE_MTL_RXP_INTR_CS);
		val |= (MGBE_MTL_RXP_INTR_CS_NVEOVIE |
			MGBE_MTL_RXP_INTR_CS_NPEOVIE |
			MGBE_MTL_RXP_INTR_CS_FOOVIE |
			MGBE_MTL_RXP_INTR_CS_PDRFIE);
		osi_writela(osi_core, val, base + MGBE_MTL_RXP_INTR_CS);
	} else {
		/* Reset FRPE bit of MTL_Operation_Mode register */
		op_mode &= ~MGBE_MTL_OP_MODE_FRPE;
		osi_writela(osi_core, op_mode, base + MGBE_MTL_OP_MODE);

		/* Verify RXPI bit reset in MTL_RXP_Control_Status */
		ret = osi_readl_poll_timeout((base + MGBE_MTL_RXP_CS),
					     (osi_core->osd_ops.udelay),
					     (val),
					     ((val & MGBE_MTL_RXP_CS_RXPI) ==
					      OSI_NONE),
					     (MGBE_MTL_FRP_READ_UDELAY),
					     (MGBE_MTL_FRP_READ_RETRY));
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				"Fail to disable FRP\n",
				val);
			return -1;
		}

		/* Disable FRP Interrupts in MTL_RXP_Interrupt_Control_Status */
		val = osi_readla(osi_core, base + MGBE_MTL_RXP_INTR_CS);
		val &= ~(MGBE_MTL_RXP_INTR_CS_NVEOVIE |
			 MGBE_MTL_RXP_INTR_CS_NPEOVIE |
			 MGBE_MTL_RXP_INTR_CS_FOOVIE |
			 MGBE_MTL_RXP_INTR_CS_PDRFIE);
		osi_writela(osi_core, val, base + MGBE_MTL_RXP_INTR_CS);
	}

	return 0;
}

/**
 * @brief mgbe_frp_write - Write FRP entry into HW
 *
 * Algorithm: This function will write FRP entry registers into HW.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] acc_sel: FRP Indirect Access Selection.
 *		       0x0 : Access FRP Instruction Table.
 *		       0x1 : Access Indirect FRP Register block.
 * @param[in] addr: FRP register address.
 * @param[in] data: FRP register data.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_frp_write(struct osi_core_priv_data *osi_core,
			  unsigned int acc_sel,
			  unsigned int addr,
			  unsigned int data)
{
	int ret = 0;
	unsigned char *base = osi_core->base;
	unsigned int val = 0U;

	if (acc_sel != OSI_ENABLE && acc_sel != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid acc_sel argment\n",
			acc_sel);
		return -1;
	}

	/* Wait for ready */
	ret = osi_readl_poll_timeout((base + MGBE_MTL_RXP_IND_CS),
				     (osi_core->osd_ops.udelay),
				     (val),
				     ((val & MGBE_MTL_RXP_IND_CS_BUSY) ==
				      OSI_NONE),
				     (MGBE_MTL_FRP_READ_UDELAY),
				     (MGBE_MTL_FRP_READ_RETRY));
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Fail to write\n",
			val);
		return -1;
	}

	/* Write data into MTL_RXP_Indirect_Acc_Data */
	osi_writela(osi_core, data, base + MGBE_MTL_RXP_IND_DATA);

	/* Program MTL_RXP_Indirect_Acc_Control_Status */
	val = osi_readla(osi_core, base + MGBE_MTL_RXP_IND_CS);
	/* Set/Reset ACCSEL for FRP Register block/Instruction Table */
	if (acc_sel == OSI_ENABLE) {
		/* Set ACCSEL bit */
		val |= MGBE_MTL_RXP_IND_CS_ACCSEL;
	} else {
		/* Reset ACCSEL bit */
		val &= ~MGBE_MTL_RXP_IND_CS_ACCSEL;
	}
	/* Set WRRDN for write */
	val |= MGBE_MTL_RXP_IND_CS_WRRDN;
	/* Clear and add ADDR */
	val &= ~MGBE_MTL_RXP_IND_CS_ADDR;
	val |= (addr & MGBE_MTL_RXP_IND_CS_ADDR);
	/* Start write */
	val |= MGBE_MTL_RXP_IND_CS_BUSY;
	osi_writela(osi_core, val, base + MGBE_MTL_RXP_IND_CS);

	/* Wait for complete */
	ret = osi_readl_poll_timeout((base + MGBE_MTL_RXP_IND_CS),
				     (osi_core->osd_ops.udelay),
				     (val),
				     ((val & MGBE_MTL_RXP_IND_CS_BUSY) ==
				      OSI_NONE),
				     (MGBE_MTL_FRP_READ_UDELAY),
				     (MGBE_MTL_FRP_READ_RETRY));
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			"Fail to write\n",
			val);
		return -1;
	}

	return ret;
}

/**
 * @brief mgbe_update_frp_entry - Update FRP Instruction Table entry in HW
 *
 * Algorithm:
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] pos: FRP Instruction Table entry location.
 * @param[in] data: FRP entry data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_update_frp_entry(struct osi_core_priv_data *const osi_core,
				 const unsigned int pos,
				 struct osi_core_frp_data *const data)
{
	unsigned int val = 0U, tmp = 0U;
	int ret = -1;

	/* Validate pos value */
	if (pos >= OSI_FRP_MAX_ENTRY) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid FRP table entry\n",
			pos);
		return -1;
	}

	/** Write Match Data into IE0 **/
	val = data->match_data;
	ret = mgbe_frp_write(osi_core, OSI_DISABLE, MGBE_MTL_FRP_IE0(pos), val);
	if (ret < 0) {
		/* Match Data Write fail */
		return -1;
	}

	/** Write Match Enable into IE1 **/
	val = data->match_en;
	ret = mgbe_frp_write(osi_core, OSI_DISABLE, MGBE_MTL_FRP_IE1(pos), val);
	if (ret < 0) {
		/* Match Enable Write fail */
		return -1;
	}

	/** Write AF, RF, IM, NIC, FO and OKI into IE2 **/
	val = 0;
	if (data->accept_frame == OSI_ENABLE) {
		/* Set AF Bit */
		val |= MGBE_MTL_FRP_IE2_AF;
	}
	if (data->reject_frame == OSI_ENABLE) {
		/* Set RF Bit */
		val |= MGBE_MTL_FRP_IE2_RF;
	}
	if (data->inverse_match == OSI_ENABLE) {
		/* Set IM Bit */
		val |= MGBE_MTL_FRP_IE2_IM;
	}
	if (data->next_ins_ctrl == OSI_ENABLE) {
		/* Set NIC Bit */
		val |= MGBE_MTL_FRP_IE2_NC;
	}
	tmp = data->frame_offset;
	val |= ((tmp << MGBE_MTL_FRP_IE2_FO_SHIFT) & MGBE_MTL_FRP_IE2_FO);
	tmp = data->ok_index;
	val |= ((tmp << MGBE_MTL_FRP_IE2_OKI_SHIFT) & MGBE_MTL_FRP_IE2_OKI);
	tmp = data->dma_chsel;
	val |= ((tmp << MGBE_MTL_FRP_IE2_DCH_SHIFT) & MGBE_MTL_FRP_IE2_DCH);
	ret = mgbe_frp_write(osi_core, OSI_DISABLE, MGBE_MTL_FRP_IE2(pos), val);
	if (ret < 0) {
		/* FRP IE2 Write fail */
		return -1;
	}

	/** Write DCH into IE3 **/
	/* TODO: The DCH position will be updated in the final RTL.
	 * We need to update this on the final RTL.
	 */
	tmp = (data->dma_chsel >> MGBE_MTL_FRP_IE3_DCH_SHIFT);
	val = (tmp & MGBE_MTL_FRP_IE3_DCH_MASK);
	ret = mgbe_frp_write(osi_core, OSI_DISABLE, MGBE_MTL_FRP_IE3(pos), val);
	if (ret < 0) {
		/* DCH Write fail */
		return -1;
	}

	return ret;
}

/**
 * @brief mgbe_update_frp_nve - Update FRP NVE into HW
 *
 * Algorithm:
 *
 * @param[in] addr: MGBE virtual base address.
 * @param[in] enabled: Flag to indicate feature is to be enabled/disabled.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_update_frp_nve(struct osi_core_priv_data *const osi_core,
			       const unsigned int nve)
{
	unsigned int val;
	unsigned char *base = osi_core->base;

	/* Validate the NVE value */
	if (nve >= OSI_FRP_MAX_ENTRY) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid NVE value\n",
			nve);
		return -1;
	}

	/* Update NVE and NPE in MTL_RXP_Control_Status register */
	val = osi_readla(osi_core, base + MGBE_MTL_RXP_CS);
	/* Clear old NVE and NPE */
	val &= ~(MGBE_MTL_RXP_CS_NVE | MGBE_MTL_RXP_CS_NPE);
	/* Add new NVE and NPE */
	val |= (nve & MGBE_MTL_RXP_CS_NVE);
	val |= ((nve << MGBE_MTL_RXP_CS_NPE_SHIFT) & MGBE_MTL_RXP_CS_NPE);
	osi_writela(osi_core, val, base + MGBE_MTL_RXP_CS);

	return 0;
}

/**
 * @brief update_rfa_rfd - Update RFD and RSA values
 *
 * Algorithm: Calulates and stores the RSD (Threshold for Dectivating
 *	  Flow control) and RSA (Threshold for Activating Flow Control) values
 *	  based on the Rx FIFO size
 *
 * @param[in] rx_fifo: Rx FIFO size.
 * @param[in] value: Stores RFD and RSA values
 */
static void update_rfa_rfd(unsigned int rx_fifo, unsigned int *value)
{
	switch (rx_fifo) {
		case MGBE_21K:
			/* Update RFD */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			*value |= (FULL_MINUS_4_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFD_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			/* Update RFA */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			*value |= (FULL_MINUS_18_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFA_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			break;
		case MGBE_24K:
			/* Update RFD */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			*value |= (FULL_MINUS_4_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFD_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			/* Update RFA */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			*value |= (FULL_MINUS_21_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFA_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			break;
		case MGBE_27K:
			/* Update RFD */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			*value |= (FULL_MINUS_4_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFD_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			/* Update RFA */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			*value |= (FULL_MINUS_24_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFA_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			break;
		case MGBE_32K:
			/* Update RFD */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			*value |= (FULL_MINUS_4_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFD_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			/* Update RFA */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			*value |= (FULL_MINUS_29_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFA_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			break;
		case MGBE_38K:
		case MGBE_48K:
		case MGBE_64K:
		case MGBE_96K:
		case MGBE_192K:
			/* Update RFD */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			*value |= (FULL_MINUS_4_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFD_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			/* Update RFA */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			*value |= (FULL_MINUS_32_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFA_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			break;
		case MGBE_19K:
		default:
			/* Update RFD */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			*value |= (FULL_MINUS_4_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFD_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFD_MASK;
			/* Update RFA */
			*value &= ~MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			*value |= (FULL_MINUS_16_K <<
				   MGBE_MTL_RXQ_OP_MODE_RFA_SHIFT) &
				   MGBE_MTL_RXQ_OP_MODE_RFA_MASK;
			break;
	}
}

/**
 * @brief mgbe_configure_mtl_queue - Configure MTL Queue
 *
 * Algorithm: This takes care of configuring the  below
 *	parameters for the MTL Queue
 *	1) Mapping MTL Rx queue and DMA Rx channel
 *	2) Flush TxQ
 *	3) Enable Store and Forward mode for Tx, Rx
 *	4) Configure Tx and Rx MTL Queue sizes
 *	5) Configure TxQ weight
 *	6) Enable Rx Queues
 *	7) Enable TX Underflow Interrupt for MTL Q
 *
 * @param[in] qinx: Queue number that need to be configured.
 * @param[in] osi_core: OSI core private data.
 * @param[in] tx_fifo: MTL TX queue size for a MTL queue.
 * @param[in] rx_fifo: MTL RX queue size for a MTL queue.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t mgbe_configure_mtl_queue(nveu32_t qinx,
				    struct osi_core_priv_data *osi_core,
				    nveu32_t tx_fifo,
				    nveu32_t rx_fifo)
{
	nveu32_t value = 0;
	nve32_t ret = 0;

	/* Program ETSALG (802.1Qaz) and RAA in MTL_Operation_Mode
	 * register to initialize the MTL operation in case
	 * of multiple Tx and Rx queues default : ETSALG WRR RAA SP
	 */

	/* Program the priorities mapped to the Selected Traffic
	 * Classes in MTL_TC_Prty_Map0-3 registers. This register is
	 * to tell traffic class x should be blocked from transmitting
	 * for the specified pause time when a PFC packet is received
	 * with priorities matching the priorities programmed in this field
	 * default: 0x0
	 */

	/* Program the Transmit Selection Algorithm (TSA) in
	 * MTL_TC[n]_ETS_Control register for all the Selected
	 * Traffic Classes (n=0, 1, â€¦, Max selected number of TCs - 1)
	 * Setting related to CBS will come here for TC.
	 * default: 0x0 SP
	 */
	ret = mgbe_flush_mtl_tx_queue(osi_core, qinx);
	if (ret < 0) {
		return ret;
	}

	value = (tx_fifo << MGBE_MTL_TXQ_SIZE_SHIFT);
	/* Enable Store and Forward mode */
	value |= MGBE_MTL_TSF;
	/*TTC  not applicable for TX*/
	/* Enable TxQ */
	value |= MGBE_MTL_TXQEN;
	value |= (osi_core->tc[qinx] << MGBE_MTL_CHX_TX_OP_MODE_Q2TC_SH);
	osi_writela(osi_core, value, (unsigned char *)
		   osi_core->base + MGBE_MTL_CHX_TX_OP_MODE(qinx));

	/* read RX Q0 Operating Mode Register */
	value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			  MGBE_MTL_CHX_RX_OP_MODE(qinx));
	value |= (rx_fifo << MGBE_MTL_RXQ_SIZE_SHIFT);
	/* Enable Store and Forward mode */
	value |= MGBE_MTL_RSF;
	/* Enable HW flow control */
	value |= MGBE_MTL_RXQ_OP_MODE_EHFC;

	osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
		   MGBE_MTL_CHX_RX_OP_MODE(qinx));

	/* Update RFA and RFD
	 * RFA: Threshold for Activating Flow Control
	 * RFD: Threshold for Deactivating Flow Control
	 */
	value = osi_readla(osi_core, (unsigned char *)osi_core->base +
			  MGBE_MTL_RXQ_FLOW_CTRL(qinx));
	update_rfa_rfd(rx_fifo, &value);
	osi_writela(osi_core, value, (unsigned char *)osi_core->base +
		   MGBE_MTL_RXQ_FLOW_CTRL(qinx));

	/* Transmit Queue weight */
	value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			  MGBE_MTL_TCQ_QW(qinx));
	value |= (MGBE_MTL_TCQ_QW_ISCQW + qinx);
	osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
		   MGBE_MTL_TCQ_QW(qinx));
	/* Enable Rx Queue Control */
	value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			  MGBE_MAC_RQC0R);
	value |= ((osi_core->rxq_ctrl[qinx] & MGBE_MAC_RXQC0_RXQEN_MASK) <<
		  (MGBE_MAC_RXQC0_RXQEN_SHIFT(qinx)));
	osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
		   MGBE_MAC_RQC0R);

	/* Enable TX Underflow Interrupt for MTL Q */
	value = osi_readl((unsigned char *)osi_core->base +
			  MGBE_MTL_QINT_ENABLE(qinx));
	value |= MGBE_MTL_QINT_TXUIE;
	osi_writel(value, (unsigned char *)osi_core->base +
		   MGBE_MTL_QINT_ENABLE(qinx));
	return 0;
}

/**
 * @brief mgbe_rss_write_reg - Write into RSS registers
 *
 * Algorithm: Programes RSS hash table or RSS hash key.
 *
 * @param[in] addr: MAC base address
 * @param[in] idx: Hash table or key index
 * @param[in] value: Value to be programmed in RSS data register.
 * @param[in] is_key: To represent passed value key or table data.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_rss_write_reg(struct osi_core_priv_data *osi_core,
			      unsigned int idx,
			      unsigned int value,
			      unsigned int is_key)
{
	unsigned char *addr = (unsigned char *)osi_core->base;
	unsigned int retry = 100;
	unsigned int ctrl = 0;
	unsigned int count = 0;
	int cond = 1;

	/* data into RSS Lookup Table or RSS Hash Key */
	osi_writela(osi_core, value, addr + MGBE_MAC_RSS_DATA);

	if (is_key == OSI_ENABLE) {
		ctrl |= MGBE_MAC_RSS_ADDR_ADDRT;
	}

	ctrl |= idx << MGBE_MAC_RSS_ADDR_RSSIA_SHIFT;
	ctrl |= MGBE_MAC_RSS_ADDR_OB;
	ctrl &= ~MGBE_MAC_RSS_ADDR_CT;
	osi_writela(osi_core, ctrl, addr + MGBE_MAC_RSS_ADDR);

	/* poll for write operation to complete */
	while (cond == 1) {
		if (count > retry) {
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
				     "Failed to update RSS Hash key or table\n",
				     0ULL);
			return -1;
		}

		count++;

		value = osi_readla(osi_core, addr + MGBE_MAC_RSS_ADDR);
		if ((value & MGBE_MAC_RSS_ADDR_OB) == OSI_NONE) {
			cond = 0;
		} else {
			osi_core->osd_ops.udelay(100);
		}
	}

	return 0;
}

/**
 * @brief mgbe_config_rss - Configure RSS
 *
 * Algorithm: Programes RSS hash table or RSS hash key.
 *
 * @param[in] osi_core: OSI core private data.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_rss(struct osi_core_priv_data *osi_core)
{
	unsigned char *addr = (unsigned char *)osi_core->base;
	unsigned int value = 0;
	unsigned int i = 0, j = 0;
	int ret = 0;

	if (osi_core->rss.enable == OSI_DISABLE) {
		/* RSS not supported */
		return 0;
	}

	/* No need to enable RSS for single Queue */
	if (osi_core->num_mtl_queues == 1U) {
		return 0;
	}

	/* Program the hash key */
	for (i = 0; i < OSI_RSS_HASH_KEY_SIZE; i += 4U) {
		value = ((unsigned int)osi_core->rss.key[i] |
			 (unsigned int)osi_core->rss.key[i + 1U] << 8U |
			 (unsigned int)osi_core->rss.key[i + 2U] << 16U |
			 (unsigned int)osi_core->rss.key[i + 3U] << 24U);
		ret = mgbe_rss_write_reg(osi_core, j, value, OSI_ENABLE);
		if (ret < 0) {
			return ret;
		}
		j++;
	}

	/* Program Hash table */
	for (i = 0; i < OSI_RSS_MAX_TABLE_SIZE; i++) {
		ret = mgbe_rss_write_reg(osi_core, i, osi_core->rss.table[i],
					 OSI_NONE);
		if (ret < 0) {
			return ret;
		}
	}

	/* Enable RSS */
	value = osi_readla(osi_core, addr + MGBE_MAC_RSS_CTRL);
	value |= MGBE_MAC_RSS_CTRL_UDP4TE | MGBE_MAC_RSS_CTRL_TCP4TE |
		 MGBE_MAC_RSS_CTRL_IP2TE | MGBE_MAC_RSS_CTRL_RSSE;
	osi_writela(osi_core, value, addr + MGBE_MAC_RSS_CTRL);

	return 0;
}

/**
 * @brief mgbe_config_flow_control - Configure MAC flow control settings
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] flw_ctrl: flw_ctrl settings
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_flow_control(struct osi_core_priv_data *const osi_core,
				    const nveu32_t flw_ctrl)
{
	unsigned int val;
	void *addr = osi_core->base;

	/* return on invalid argument */
	if (flw_ctrl > (OSI_FLOW_CTRL_RX | OSI_FLOW_CTRL_TX)) {
		return -1;
	}

	/* Configure MAC Tx Flow control */
	/* Read MAC Tx Flow control Register of Q0 */
	val = osi_readla(osi_core,
			 (unsigned char *)addr + MGBE_MAC_QX_TX_FLW_CTRL(0U));

	/* flw_ctrl BIT0: 1 is for tx flow ctrl enable
	 * flw_ctrl BIT0: 0 is for tx flow ctrl disable
	 */
	if ((flw_ctrl & OSI_FLOW_CTRL_TX) == OSI_FLOW_CTRL_TX) {
		/* Enable Tx Flow Control */
		val |= MGBE_MAC_QX_TX_FLW_CTRL_TFE;
		/* Mask and set Pause Time */
		val &= ~MGBE_MAC_PAUSE_TIME_MASK;
		val |= MGBE_MAC_PAUSE_TIME & MGBE_MAC_PAUSE_TIME_MASK;
	} else {
		/* Disable Tx Flow Control */
		val &= ~MGBE_MAC_QX_TX_FLW_CTRL_TFE;
	}

	/* Write to MAC Tx Flow control Register of Q0 */
	osi_writela(osi_core, val,
		    (unsigned char *)addr + MGBE_MAC_QX_TX_FLW_CTRL(0U));

	/* Configure MAC Rx Flow control*/
	/* Read MAC Rx Flow control Register */
	val = osi_readla(osi_core,
			 (unsigned char *)addr + MGBE_MAC_RX_FLW_CTRL);

	/* flw_ctrl BIT1: 1 is for rx flow ctrl enable
	 * flw_ctrl BIT1: 0 is for rx flow ctrl disable
	 */
	if ((flw_ctrl & OSI_FLOW_CTRL_RX) == OSI_FLOW_CTRL_RX) {
		/* Enable Rx Flow Control */
		val |= MGBE_MAC_RX_FLW_CTRL_RFE;
	} else {
		/* Disable Rx Flow Control */
		val &= ~MGBE_MAC_RX_FLW_CTRL_RFE;
	}

	/* Write to MAC Rx Flow control Register */
	osi_writela(osi_core, val,
		    (unsigned char *)addr + MGBE_MAC_RX_FLW_CTRL);

	return 0;
}

/**
 * @brief mgbe_configure_mac - Configure MAC
 *
 * Algorithm: This takes care of configuring the  below
 *	parameters for the MAC
 *	1) Programming the MAC address
 *	2) Enable required MAC control fields in MCR
 *	3) Enable Multicast and Broadcast Queue
 *	4) Disable MMC nve32_terrupts and Configure the MMC counters
 *	5) Enable required MAC nve32_terrupts
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_configure_mac(struct osi_core_priv_data *osi_core)
{
	unsigned int value = 0U, max_queue = 0U, i = 0U;

	/* TODO: Need to check if we need to enable anything in Tx configuration
	 * value = osi_readla(osi_core,
			      (nveu8_t *)osi_core->base + MGBE_MAC_TMCR);
	 */
	/* Read MAC Rx Configuration Register */
	value = osi_readla(osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_RMCR);
	/* Enable Automatic Pad or CRC Stripping */
	/* Enable CRC stripping for Type packets */
	/* Enable Rx checksum offload engine by default */
	value |= MGBE_MAC_RMCR_ACS | MGBE_MAC_RMCR_CST | MGBE_MAC_RMCR_IPC;

	/* Jumbo Packet Enable */
	if (osi_core->mtu > OSI_DFLT_MTU_SIZE &&
	    osi_core->mtu <= OSI_MTU_SIZE_9000) {
		value |= MGBE_MAC_RMCR_JE;
	} else if (osi_core->mtu > OSI_MTU_SIZE_9000){
		/* if MTU greater 9K use GPSLCE */
		value |= MGBE_MAC_RMCR_GPSLCE | MGBE_MAC_RMCR_WD;
		value &= ~MGBE_MAC_RMCR_GPSL_MSK;
		value |=  ((OSI_MAX_MTU_SIZE << 16) & MGBE_MAC_RMCR_GPSL_MSK);
	} else {
		value &= ~MGBE_MAC_RMCR_JE;
		value &= ~MGBE_MAC_RMCR_GPSLCE;
		value &= ~MGBE_MAC_RMCR_WD;
	}

	osi_writela(osi_core, value,
		    (unsigned char *)osi_core->base + MGBE_MAC_RMCR);

	value = osi_readla(osi_core,
			   (unsigned char *)osi_core->base + MGBE_MAC_TMCR);
	/* Jabber Disable */
	if (osi_core->mtu > OSI_DFLT_MTU_SIZE) {
		value |= MGBE_MAC_TMCR_JD;
	}
	osi_writela(osi_core, value,
		    (unsigned char *)osi_core->base + MGBE_MAC_TMCR);

	/* Enable Multicast and Broadcast Queue */
	value = osi_readla(osi_core,
			   (unsigned char *)osi_core->base + MGBE_MAC_RQC1R);
	value |= MGBE_MAC_RQC1R_MCBCQEN;
	/* Set MCBCQ to highest enabled RX queue index */
	for (i = 0; i < osi_core->num_mtl_queues; i++) {
		if ((max_queue < osi_core->mtl_queues[i]) &&
		    (osi_core->mtl_queues[i] < OSI_MGBE_MAX_NUM_QUEUES)) {
			/* Update max queue number */
			max_queue = osi_core->mtl_queues[i];
		}
	}
	value &= ~(MGBE_MAC_RQC1R_MCBCQ);
	value |= (max_queue << MGBE_MAC_RQC1R_MCBCQ_SHIFT);
	osi_writela(osi_core, value,
		    (unsigned char *)osi_core->base + MGBE_MAC_RQC1R);

	/* Disable all MMC nve32_terrupts */
	/* Disable all MMC Tx nve32_terrupts */
	osi_writela(osi_core, OSI_NONE, (nveu8_t *)osi_core->base +
		   MGBE_MMC_TX_INTR_EN);
	/* Disable all MMC RX nve32_terrupts */
	osi_writela(osi_core, OSI_NONE, (nveu8_t *)osi_core->base +
		   MGBE_MMC_RX_INTR_EN);

	/* Configure MMC counters */
	value = osi_readla(osi_core,
			   (nveu8_t *)osi_core->base + MGBE_MMC_CNTRL);
	value |= MGBE_MMC_CNTRL_CNTRST | MGBE_MMC_CNTRL_RSTONRD |
		 MGBE_MMC_CNTRL_CNTMCT | MGBE_MMC_CNTRL_CNTPRST;
	osi_writela(osi_core, value,
		    (nveu8_t *)osi_core->base + MGBE_MMC_CNTRL);

	/* Enable MAC nve32_terrupts */
	/* Read MAC IMR Register */
	value = osi_readla(osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_IER);
	/* RGSMIIIM - RGMII/SMII interrupt and TSIE Enable */
	/* TXESIE - Transmit Error Status Interrupt Enable */
	/* TODO: LPI need to be enabled during EEE implementation */
	value |= (MGBE_IMR_RGSMIIIE | MGBE_IMR_TSIE | MGBE_IMR_TXESIE);
	osi_writela(osi_core, value, (nveu8_t *)osi_core->base + MGBE_MAC_IER);

	/* Enable common interrupt at wrapper level */
	value = osi_readla(osi_core, (unsigned char *)osi_core->base +
			  MGBE_WRAP_COMMON_INTR_ENABLE);
	value |= MGBE_MAC_SBD_INTR;
	osi_writela(osi_core, value, (unsigned char *)osi_core->base +
		   MGBE_WRAP_COMMON_INTR_ENABLE);

	/* Enable VLAN configuration */
	value = osi_readla(osi_core,
			   (unsigned char *)osi_core->base + MGBE_MAC_VLAN_TR);
	/* Enable VLAN Tag in RX Status
	 * Disable double VLAN Tag processing on TX and RX
	 */
	if (osi_core->strip_vlan_tag == OSI_ENABLE) {
		/* Enable VLAN Tag stripping always */
		value |= MGBE_MAC_VLANTR_EVLS_ALWAYS_STRIP;
	}
	value |= MGBE_MAC_VLANTR_EVLRXS | MGBE_MAC_VLANTR_DOVLTC;
	osi_writela(osi_core, value,
		    (unsigned char *)osi_core->base + MGBE_MAC_VLAN_TR);

	value = osi_readla(osi_core,
			   (unsigned char *)osi_core->base + MGBE_MAC_VLANTIR);
	/* Enable VLAN tagging through context descriptor */
	value |= MGBE_MAC_VLANTIR_VLTI;
	/* insert/replace C_VLAN in 13th & 14th bytes of transmitted frames */
	value &= ~MGBE_MAC_VLANTIRR_CSVL;
	osi_writela(osi_core, value,
		    (unsigned char *)osi_core->base + MGBE_MAC_VLANTIR);

	/* Configure default flow control settings */
	if (osi_core->pause_frames == OSI_PAUSE_FRAMES_ENABLE) {
		osi_core->flow_ctrl = (OSI_FLOW_CTRL_TX | OSI_FLOW_CTRL_RX);
		if (mgbe_config_flow_control(osi_core,
					     osi_core->flow_ctrl) != 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				"Failed to set flow control configuration\n",
				0ULL);
		}
	}
	/* TODO: USP (user Priority) to RxQ Mapping */

	/* RSS cofiguration */
	return mgbe_config_rss(osi_core);
}

/**
 * @brief mgbe_configure_dma - Configure DMA
 *
 * Algorithm: This takes care of configuring the  below
 *	parameters for the DMA
 *	1) Programming different burst length for the DMA
 *	2) Enable enhanced Address mode
 *	3) Programming max read outstanding request limit
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC has to be out of reset.
 */
static void mgbe_configure_dma(struct osi_core_priv_data *osi_core,
			       nveu32_t pre_si)
{
	nveu32_t value = 0;

	/* Set AXI Undefined Burst Length */
	value |= MGBE_DMA_SBUS_UNDEF;
	/* AXI Burst Length 256*/
	value |= MGBE_DMA_SBUS_BLEN256;
	/* Enhanced Address Mode Enable */
	value |= MGBE_DMA_SBUS_EAME;
	/* AXI Maximum Read Outstanding Request Limit = 63 */
	value |= MGBE_DMA_SBUS_RD_OSR_LMT;
	/* AXI Maximum Write Outstanding Request Limit = 63 */
	value |= MGBE_DMA_SBUS_WR_OSR_LMT;

	osi_writela(osi_core, value,
		    (nveu8_t *)osi_core->base + MGBE_DMA_SBUS);

	/* Configure TDPS to 5 */
	value = osi_readla(osi_core,
			   (nveu8_t *)osi_core->base + MGBE_DMA_TX_EDMA_CTRL);
	if (pre_si == OSI_ENABLE) {
		/* For Pre silicon TDPS Value is 3 */
		value |= MGBE_DMA_TX_EDMA_CTRL_TDPS_PRESI;
	} else {
		value |= MGBE_DMA_TX_EDMA_CTRL_TDPS;
	}
	osi_writela(osi_core, value,
		    (nveu8_t *)osi_core->base + MGBE_DMA_TX_EDMA_CTRL);

	/* Configure RDPS to 5 */
	value = osi_readla(osi_core,
			   (nveu8_t *)osi_core->base + MGBE_DMA_RX_EDMA_CTRL);
	if (pre_si == OSI_ENABLE) {
		/* For Pre silicon RDPS Value is 3 */
		value |= MGBE_DMA_RX_EDMA_CTRL_RDPS_PRESI;
	} else {
		value |= MGBE_DMA_RX_EDMA_CTRL_RDPS;
	}
	osi_writela(osi_core, value,
		    (nveu8_t *)osi_core->base + MGBE_DMA_RX_EDMA_CTRL);
}

/**
 * @brief Initialize the osi_core->backup_config.
 *
 * Algorithm: Populate the list of core registers to be saved during suspend.
 *	Fill the address of each register in structure.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @retval none
 */
static void mgbe_core_backup_init(struct osi_core_priv_data *const osi_core)
{
	struct core_backup *config = &osi_core->backup_config;
	unsigned char *base = (unsigned char *)osi_core->base;
	unsigned int i;

	/* MAC registers backup */
	config->reg_addr[MGBE_MAC_TMCR_BAK_IDX] = base + MGBE_MAC_TMCR;
	config->reg_addr[MGBE_MAC_RMCR_BAK_IDX] = base + MGBE_MAC_RMCR;
	config->reg_addr[MGBE_MAC_PFR_BAK_IDX] = base + MGBE_MAC_PFR;
	config->reg_addr[MGBE_MAC_VLAN_TAG_BAK_IDX] = base +
						MGBE_MAC_VLAN_TR;
	config->reg_addr[MGBE_MAC_VLANTIR_BAK_IDX] = base + MGBE_MAC_VLANTIR;
	config->reg_addr[MGBE_MAC_RX_FLW_CTRL_BAK_IDX] = base +
						MGBE_MAC_RX_FLW_CTRL;
	config->reg_addr[MGBE_MAC_RQC0R_BAK_IDX] = base + MGBE_MAC_RQC0R;
	config->reg_addr[MGBE_MAC_RQC1R_BAK_IDX] = base + MGBE_MAC_RQC1R;
	config->reg_addr[MGBE_MAC_RQC2R_BAK_IDX] = base + MGBE_MAC_RQC2R;
	config->reg_addr[MGBE_MAC_ISR_BAK_IDX] = base + MGBE_MAC_ISR;
	config->reg_addr[MGBE_MAC_IER_BAK_IDX] = base + MGBE_MAC_IER;
	config->reg_addr[MGBE_MAC_PMTCSR_BAK_IDX] = base + MGBE_MAC_PMTCSR;
	config->reg_addr[MGBE_MAC_LPI_CSR_BAK_IDX] = base + MGBE_MAC_LPI_CSR;
	config->reg_addr[MGBE_MAC_LPI_TIMER_CTRL_BAK_IDX] = base +
						MGBE_MAC_LPI_TIMER_CTRL;
	config->reg_addr[MGBE_MAC_LPI_EN_TIMER_BAK_IDX] = base +
						MGBE_MAC_LPI_EN_TIMER;
	config->reg_addr[MGBE_MAC_TCR_BAK_IDX] = base + MGBE_MAC_TCR;
	config->reg_addr[MGBE_MAC_SSIR_BAK_IDX] = base + MGBE_MAC_SSIR;
	config->reg_addr[MGBE_MAC_STSR_BAK_IDX] = base + MGBE_MAC_STSR;
	config->reg_addr[MGBE_MAC_STNSR_BAK_IDX] = base + MGBE_MAC_STNSR;
	config->reg_addr[MGBE_MAC_STSUR_BAK_IDX] = base + MGBE_MAC_STSUR;
	config->reg_addr[MGBE_MAC_STNSUR_BAK_IDX] = base + MGBE_MAC_STNSUR;
	config->reg_addr[MGBE_MAC_TAR_BAK_IDX] = base + MGBE_MAC_TAR;
	config->reg_addr[MGBE_DMA_BMR_BAK_IDX] = base + MGBE_DMA_MODE;
	config->reg_addr[MGBE_DMA_SBUS_BAK_IDX] = base + MGBE_DMA_SBUS;
	config->reg_addr[MGBE_DMA_ISR_BAK_IDX] = base + MGBE_DMA_ISR;
	config->reg_addr[MGBE_MTL_OP_MODE_BAK_IDX] = base + MGBE_MTL_OP_MODE;
	config->reg_addr[MGBE_MTL_RXQ_DMA_MAP0_BAK_IDX] = base +
						MGBE_MTL_RXQ_DMA_MAP0;

	for (i = 0; i < MGBE_MAX_HTR_REGS; i++) {
		config->reg_addr[MGBE_MAC_HTR_REG_BAK_IDX(i)] = base +
						MGBE_MAC_HTR_REG(i);
	}
	for (i = 0; i < OSI_MGBE_MAX_NUM_QUEUES; i++) {
		config->reg_addr[MGBE_MAC_QX_TX_FLW_CTRL_BAK_IDX(i)] = base +
						MGBE_MAC_QX_TX_FLW_CTRL(i);
	}
	for (i = 0; i < OSI_MGBE_MAX_MAC_ADDRESS_FILTER; i++) {
		config->reg_addr[MGBE_MAC_ADDRH_BAK_IDX(i)] = base +
						MGBE_MAC_ADDRH(i);
		config->reg_addr[MGBE_MAC_ADDRL_BAK_IDX(i)] = base +
						MGBE_MAC_ADDRL(i);
	}
	for (i = 0; i < OSI_MGBE_MAX_NUM_QUEUES; i++) {
		config->reg_addr[MGBE_MTL_CHX_TX_OP_MODE_BAK_IDX(i)] = base +
						MGBE_MTL_CHX_TX_OP_MODE(i);
		config->reg_addr[MGBE_MTL_CHX_RX_OP_MODE_BAK_IDX(i)] = base +
						MGBE_MTL_CHX_RX_OP_MODE(i);
	}
	for (i = 0; i < OSI_MAX_TC_NUM; i++) {
		config->reg_addr[MGBE_MTL_TXQ_ETS_CR_BAK_IDX(i)] = base +
						MGBE_MTL_TCQ_ETS_CR(i);
		config->reg_addr[MGBE_MTL_TXQ_QW_BAK_IDX(i)] = base +
						MGBE_MTL_TCQ_QW(i);
		config->reg_addr[MGBE_MTL_TXQ_ETS_SSCR_BAK_IDX(i)] = base +
						MGBE_MTL_TCQ_ETS_SSCR(i);
		config->reg_addr[MGBE_MTL_TXQ_ETS_HCR_BAK_IDX(i)] = base +
						MGBE_MTL_TCQ_ETS_HCR(i);
		config->reg_addr[MGBE_MTL_TXQ_ETS_LCR_BAK_IDX(i)] = base +
						MGBE_MTL_TCQ_ETS_LCR(i);
	}

	/* TODO: Add wrapper register backup */
}

/**
 * @brief mgbe_enable_mtl_interrupts - Enable MTL interrupts
 *
 * Algorithm: enable MTL interrupts for EST
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static inline void mgbe_enable_mtl_interrupts(
				struct osi_core_priv_data *osi_core)
{
	unsigned int  mtl_est_ir = OSI_DISABLE;

	mtl_est_ir = osi_readla(osi_core, (unsigned char *)
				osi_core->base + MGBE_MTL_EST_ITRE);
	/* enable only MTL interrupt realted to
	 * Constant Gate Control Error
	 * Head-Of-Line Blocking due to Scheduling
	 * Head-Of-Line Blocking due to Frame Size
	 * BTR Error
	 * Switch to S/W owned list Complete
	 */
	mtl_est_ir |= (MGBE_MTL_EST_ITRE_CGCE | MGBE_MTL_EST_ITRE_IEHS |
		       MGBE_MTL_EST_ITRE_IEHF | MGBE_MTL_EST_ITRE_IEBE |
		       MGBE_MTL_EST_ITRE_IECC);
	osi_writela(osi_core, mtl_est_ir,
		    (unsigned char *)osi_core->base + MGBE_MTL_EST_ITRE);
}

/**
 * @brief mgbe_enable_fpe_interrupts - Enable MTL interrupts
 *
 * Algorithm: enable FPE interrupts
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static inline void mgbe_enable_fpe_interrupts(
				struct osi_core_priv_data *osi_core)
{
	unsigned int  value = OSI_DISABLE;

	/* Read MAC IER Register and enable Frame Preemption Interrupt
	 * Enable */
	value = osi_readla(osi_core, (unsigned char *)
			   osi_core->base + MGBE_MAC_IER);
	value |= MGBE_IMR_FPEIE;
	osi_writela(osi_core, value, (unsigned char *)
		    osi_core->base + MGBE_MAC_IER);
}

/**
 * @brief mgbe_save_gcl_params - save GCL configs in local core structure
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static inline void mgbe_save_gcl_params(struct osi_core_priv_data *osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	unsigned int gcl_widhth[4] = {0, OSI_MAX_24BITS, OSI_MAX_28BITS,
				      OSI_MAX_32BITS};
	unsigned int gcl_depthth[6] = {0, OSI_GCL_SIZE_64, OSI_GCL_SIZE_128,
				       OSI_GCL_SIZE_256, OSI_GCL_SIZE_512,
				       OSI_GCL_SIZE_1024};

	if (osi_core->hw_feature->gcl_width == 0 ||
	    osi_core->hw_feature->gcl_width > 3) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "Wrong HW feature GCL width\n",
			   (unsigned long long)osi_core->hw_feature->gcl_width);
	} else {
		l_core->gcl_width_val =
				    gcl_widhth[osi_core->hw_feature->gcl_width];
	}

	if (osi_core->hw_feature->gcl_depth == 0 ||
	    osi_core->hw_feature->gcl_depth > 5) {
		/* Do Nothing */
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Wrong HW feature GCL depth\n",
			   (unsigned long long)osi_core->hw_feature->gcl_depth);
	} else {
		l_core->gcl_dep = gcl_depthth[osi_core->hw_feature->gcl_depth];
	}
}

/**
 * @brief mgbe_tsn_init - initialize TSN feature
 *
 * Algorithm:
 * 1) If hardware support EST,
 *   a) Set default EST configuration
 *   b) Set enable interrupts
 * 2) If hardware supports FPE
 *   a) Set default FPE configuration
 *   b) enable interrupts
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est_sel: EST HW support present or not
 * @param[in] fpe_sel: FPE HW support present or not
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static void mgbe_tsn_init(struct osi_core_priv_data *osi_core,
			  unsigned int est_sel, unsigned int fpe_sel)
{
	unsigned int val = 0x0;
	unsigned int temp = 0U;

	if (est_sel == OSI_ENABLE) {
		mgbe_save_gcl_params(osi_core);
		val = osi_readla(osi_core, (unsigned char *)osi_core->base +
				MGBE_MTL_EST_CONTROL);

		/*
		 * PTOV PTP clock period * 6
		 * dual-port RAM based asynchronous FIFO controllers or
		 * Single-port RAM based synchronous FIFO controllers
		 * CTOV 96 x Tx clock period
		 * :
		 * :
		 * set other default value
		 */
		val &= ~MGBE_MTL_EST_CONTROL_PTOV;
		if (osi_core->pre_si == OSI_ENABLE) {
			/* 6*1/(78.6 MHz) in ns*/
			temp = (6U * 13U);
		} else {
			temp = MGBE_MTL_EST_PTOV_RECOMMEND;
		}
		temp = temp << MGBE_MTL_EST_CONTROL_PTOV_SHIFT;
		val |= temp;

		val &= ~MGBE_MTL_EST_CONTROL_CTOV;
		temp = MGBE_MTL_EST_CTOV_RECOMMEND;
		temp = temp << MGBE_MTL_EST_CONTROL_CTOV_SHIFT;
		val |= temp;

		/*Loop Count to report Scheduling Error*/
		val &= ~MGBE_MTL_EST_CONTROL_LCSE;
		val |= MGBE_MTL_EST_CONTROL_LCSE_VAL;

		val &= ~MGBE_MTL_EST_CONTROL_DDBF;
		val |= MGBE_MTL_EST_CONTROL_DDBF;
		osi_writela(osi_core, val, (unsigned char *)osi_core->base +
			   MGBE_MTL_EST_CONTROL);

		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				MGBE_MTL_EST_OVERHEAD);
		val &= ~MGBE_MTL_EST_OVERHEAD_OVHD;
		/* As per hardware programming info */
		val |= MGBE_MTL_EST_OVERHEAD_RECOMMEND;
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			   MGBE_MTL_EST_OVERHEAD);

		mgbe_enable_mtl_interrupts(osi_core);
	}

	if (fpe_sel == OSI_ENABLE) {
		val = osi_readla(osi_core, (unsigned char *)osi_core->base +
				MGBE_MAC_RQC1R);
		val &= ~MGBE_MAC_RQC1R_RQ;
		temp = osi_core->residual_queue;
		temp = temp << MGBE_MAC_RQC1R_RQ_SHIFT;
		temp = (temp & MGBE_MAC_RQC1R_RQ);
		val |= temp;
		osi_writela(osi_core, val, (unsigned char *)osi_core->base +
			   MGBE_MAC_RQC1R);

		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				MGBE_MAC_RQC4R);
		val &= ~MGBE_MAC_RQC4R_PMCBCQ;
		temp = osi_core->residual_queue;
		temp = temp << MGBE_MAC_RQC4R_PMCBCQ_SHIFT;
		temp = (temp & MGBE_MAC_RQC4R_PMCBCQ);
		val |= temp;
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			   MGBE_MAC_RQC4R);

		mgbe_enable_fpe_interrupts(osi_core);
	}

	/* CBS setting for TC or TXQ for default configuration
	   user application should use IOCTL to set CBS as per requirement
	 */
}

/**
 * @brief Map DMA channels to a specific VM IRQ.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note OSD layer needs to update number of VM channels and
 *	DMA channel list in osi_vm_irq_data.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t mgbe_dma_chan_to_vmirq_map(struct osi_core_priv_data *osi_core)
{
	nveu32_t sid[4] = { MGBE0_SID, MGBE1_SID, MGBE2_SID, MGBE3_SID };
	struct osi_vm_irq_data *irq_data;
	nveu32_t i, j;
	nveu32_t chan;

	for (i = 0; i < osi_core->num_vm_irqs; i++) {
		irq_data = &osi_core->irq_data[i];

		for (j = 0; j < irq_data->num_vm_chans; j++) {
			chan = irq_data->vm_chans[j];

			if (chan >= OSI_MGBE_MAX_NUM_CHANS) {
				continue;
			}

			osi_writel(OSI_BIT(irq_data->vm_num),
				   (nveu8_t *)osi_core->base +
				   MGBE_VIRT_INTR_APB_CHX_CNTRL(chan));
		}
	}

	if ((osi_core->use_virtualization == OSI_DISABLE) &&
	    (osi_core->hv_base != OSI_NULL)) {
		if (osi_core->instance_id > 3U) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Wrong MAC instance-ID\n",
				     osi_core->instance_id);
			return -1;
		}

		osi_writela(osi_core, MGBE_SID_VAL1(sid[osi_core->instance_id]),
			    (nveu8_t *)osi_core->hv_base +
			    MGBE_WRAP_AXI_ASID0_CTRL);

		osi_writela(osi_core, MGBE_SID_VAL1(sid[osi_core->instance_id]),
			    (nveu8_t *)osi_core->hv_base +
			    MGBE_WRAP_AXI_ASID1_CTRL);

		osi_writela(osi_core, MGBE_SID_VAL2(sid[osi_core->instance_id]),
			    (nveu8_t *)osi_core->hv_base +
			    MGBE_WRAP_AXI_ASID2_CTRL);
	}

	return 0;
}


/**
 * @brief mgbe_core_init - MGBE MAC, MTL and common DMA Initialization
 *
 * Algorithm: This function will take care of initializing MAC, MTL and
 *	common DMA registers.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] tx_fifo_size: MTL TX FIFO size
 * @param[in] rx_fifo_size: MTL RX FIFO size
 *
 * @note 1) MAC should be out of reset. See osi_poll_for_swr() for details.
 *	 2) osi_core->base needs to be filled based on ioremap.
 *	 3) osi_core->num_mtl_queues needs to be filled.
 *	 4) osi_core->mtl_queues[qinx] need to be filled.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t mgbe_core_init(struct osi_core_priv_data *osi_core,
			  nveu32_t tx_fifo_size,
			  nveu32_t rx_fifo_size)
{
	nve32_t ret = 0;
	nveu32_t qinx = 0;
	nveu32_t value = 0;
	nveu32_t tx_fifo = 0;
	nveu32_t rx_fifo = 0;

	mgbe_core_backup_init(osi_core);

	/* reset mmc counters */
	osi_writela(osi_core, MGBE_MMC_CNTRL_CNTRST, (nveu8_t *)osi_core->base +
		   MGBE_MMC_CNTRL);

	/* Mapping MTL Rx queue and DMA Rx channel */
	value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			  MGBE_MTL_RXQ_DMA_MAP0);
	value |= MGBE_RXQ_TO_DMA_CHAN_MAP0;
	value |= MGBE_RXQ_TO_DMA_MAP_DDMACH;
	osi_writela(osi_core, value, (unsigned char *)osi_core->base +
		   MGBE_MTL_RXQ_DMA_MAP0);

	value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			  MGBE_MTL_RXQ_DMA_MAP1);
	value |= MGBE_RXQ_TO_DMA_CHAN_MAP1;
	value |= MGBE_RXQ_TO_DMA_MAP_DDMACH;
	osi_writela(osi_core, value, (unsigned char *)osi_core->base +
		   MGBE_MTL_RXQ_DMA_MAP1);

	value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			  MGBE_MTL_RXQ_DMA_MAP2);
	value |= MGBE_RXQ_TO_DMA_CHAN_MAP2;
	value |= MGBE_RXQ_TO_DMA_MAP_DDMACH;
	osi_writela(osi_core, value, (unsigned char *)osi_core->base +
		   MGBE_MTL_RXQ_DMA_MAP2);

	/* Enable XDCS in MAC_Extended_Configuration */
	value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			  MGBE_MAC_EXT_CNF);
	value |= MGBE_MAC_EXT_CNF_DDS;
	osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
		   MGBE_MAC_EXT_CNF);

	if (osi_core->pre_si == OSI_ENABLE) {
		/* For pre silicon Tx and Rx Queue sizes are 64KB */
		tx_fifo_size = MGBE_TX_FIFO_SIZE_64KB;
		rx_fifo_size = MGBE_RX_FIFO_SIZE_64KB;
	} else {
		/* Actual HW RAM size for Tx is 128KB and Rx is 192KB */
		tx_fifo_size = MGBE_TX_FIFO_SIZE_128KB;
		rx_fifo_size = MGBE_RX_FIFO_SIZE_192KB;
	}

	/* Calculate value of Transmit queue fifo size to be programmed */
	tx_fifo = mgbe_calculate_per_queue_fifo(tx_fifo_size,
						osi_core->num_mtl_queues);

	/* Calculate value of Receive queue fifo size to be programmed */
	rx_fifo = mgbe_calculate_per_queue_fifo(rx_fifo_size,
						osi_core->num_mtl_queues);

	/* Configure MTL Queues */
	/* TODO: Iterate over Number MTL queues need to be removed */
	for (qinx = 0; qinx < osi_core->num_mtl_queues; qinx++) {
		ret = mgbe_configure_mtl_queue(osi_core->mtl_queues[qinx],
					       osi_core, tx_fifo, rx_fifo);
		if (ret < 0) {
			return ret;
		}
	}

	/* configure MGBE MAC HW */
	ret = mgbe_configure_mac(osi_core);
	if (ret < 0) {
		return ret;
	}

	/* configure MGBE DMA */
	mgbe_configure_dma(osi_core, osi_core->pre_si);

	/* tsn initialization */
	if (osi_core->hw_feature != OSI_NULL) {
		mgbe_tsn_init(osi_core, osi_core->hw_feature->est_sel,
			      osi_core->hw_feature->fpe_sel);
	}

	return mgbe_dma_chan_to_vmirq_map(osi_core);
}

/**
 * @brief mgbe_handle_mac_fpe_intrs
 *
 * Algorithm: This function takes care of handling the
 *	MAC FPE interrupts.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC interrupts need to be enabled
 *
 */
static void mgbe_handle_mac_fpe_intrs(struct osi_core_priv_data *osi_core)
{
	unsigned int val = 0;

	/* interrupt bit clear on read as CSR_SW is reset */
	val = osi_readla(osi_core, (unsigned char *)
			 osi_core->base + MGBE_MAC_FPE_CTS);

	if ((val & MGBE_MAC_FPE_CTS_RVER) == MGBE_MAC_FPE_CTS_RVER) {
		val &= ~MGBE_MAC_FPE_CTS_RVER;
		val |= MGBE_MAC_FPE_CTS_SRSP;
	}

	if ((val & MGBE_MAC_FPE_CTS_RRSP) == MGBE_MAC_FPE_CTS_RRSP) {
		/* received respose packet  Nothing to be done, it means other
		 * IP also support FPE
		 */
		val &= ~MGBE_MAC_FPE_CTS_RRSP;
		val &= ~MGBE_MAC_FPE_CTS_TVER;
		osi_core->fpe_ready = OSI_ENABLE;
		val |= MGBE_MAC_FPE_CTS_EFPE;
	}

	if ((val & MGBE_MAC_FPE_CTS_TRSP) == MGBE_MAC_FPE_CTS_TRSP) {
		/* TX response packet sucessful */
		osi_core->fpe_ready = OSI_ENABLE;
		/* Enable frame preemption */
		val &= ~MGBE_MAC_FPE_CTS_TRSP;
		val &= ~MGBE_MAC_FPE_CTS_TVER;
		val |= MGBE_MAC_FPE_CTS_EFPE;
	}

	if ((val & MGBE_MAC_FPE_CTS_TVER) == MGBE_MAC_FPE_CTS_TVER) {
		/*Transmit verif packet sucessful*/
		osi_core->fpe_ready = OSI_DISABLE;
		val &= ~MGBE_MAC_FPE_CTS_TVER;
		val &= ~MGBE_MAC_FPE_CTS_EFPE;
	}

	osi_writela(osi_core, val, (unsigned char *)
		    osi_core->base + MGBE_MAC_FPE_CTS);
}

/**
 * @brief Get free timestamp index from TS array by validating in_use param
 *
 * @param[in] l_core: Core local private data structure.
 *
 * @retval Free timestamp index.
 *
 * @post If timestamp index is MAX_TX_TS_CNT, then its no free count index
 *       is available.
 */
static inline nveu32_t get_free_ts_idx(struct core_local *l_core)
{
	nveu32_t i;

	for (i = 0; i < MAX_TX_TS_CNT; i++) {
		if (l_core->ts[i].in_use == OSI_NONE) {
			break;
		}
	}
	return i;
}

/**
 * @brief mgbe_handle_mac_intrs - Handle MAC interrupts
 *
 * Algorithm: This function takes care of handling the
 *	MAC nve32_terrupts which includes speed, mode detection.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] dma_isr: DMA ISR register read value.
 *
 * @note MAC nve32_terrupts need to be enabled
 */
static void mgbe_handle_mac_intrs(struct osi_core_priv_data *osi_core,
				  nveu32_t dma_isr)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	nveu32_t mac_isr = 0;
	nveu32_t mac_ier = 0;
	nveu32_t tx_errors = 0;

	mac_isr = osi_readla(osi_core,
			     (unsigned char *)osi_core->base + MGBE_MAC_ISR);
	/* Handle MAC interrupts */
	if ((dma_isr & MGBE_DMA_ISR_MACIS) != MGBE_DMA_ISR_MACIS) {
		return;
	}

	mac_ier = osi_readla(osi_core,
			     (unsigned char *)osi_core->base + MGBE_MAC_IER);
	if (((mac_isr & MGBE_MAC_IMR_FPEIS) == MGBE_MAC_IMR_FPEIS) &&
	    ((mac_ier & MGBE_IMR_FPEIE) == MGBE_IMR_FPEIE)) {
		mgbe_handle_mac_fpe_intrs(osi_core);
		mac_isr &= ~MGBE_MAC_IMR_FPEIS;
	}
	/* Check for any MAC Transmit Error Status Interrupt */
	if ((mac_isr & MGBE_IMR_TXESIE) == MGBE_IMR_TXESIE) {
		/* Check for the type of Tx error by reading  MAC_Rx_Tx_Status
		 * register
		 */
		tx_errors = osi_readl((unsigned char *)osi_core->base +
				      MGBE_MAC_RX_TX_STS);
		if ((tx_errors & MGBE_MAC_TX_TJT) == MGBE_MAC_TX_TJT) {
			/* increment Tx Jabber timeout stats */
			osi_core->pkt_err_stats.mgbe_jabber_timeout_err =
				osi_update_stats_counter(
				osi_core->pkt_err_stats.mgbe_jabber_timeout_err,
				1UL);
		}
		if ((tx_errors & MGBE_MAC_TX_IHE) == MGBE_MAC_TX_IHE) {
			/* IP Header Error */
			osi_core->pkt_err_stats.mgbe_ip_header_err =
				osi_update_stats_counter(
				osi_core->pkt_err_stats.mgbe_ip_header_err,
				1UL);
		}
		if ((tx_errors & MGBE_MAC_TX_PCE) == MGBE_MAC_TX_PCE) {
			/* Payload Checksum error */
			osi_core->pkt_err_stats.mgbe_payload_cs_err =
				osi_update_stats_counter(
				osi_core->pkt_err_stats.mgbe_payload_cs_err,
				1UL);
		}
	}

	osi_writela(osi_core, mac_isr,
		    (unsigned char *)osi_core->base + MGBE_MAC_ISR);
	if ((mac_isr & MGBE_ISR_TSIS) == MGBE_ISR_TSIS) {
		struct osi_core_tx_ts *head = &l_core->tx_ts_head;

		if (__sync_fetch_and_add(&l_core->ts_lock, 1) == 1U) {
			/* mask return as initial value is returned always */
			(void)__sync_fetch_and_sub(&l_core->ts_lock, 1);
			osi_core->xstats.ts_lock_add_fail =
					osi_update_stats_counter(
					osi_core->xstats.ts_lock_add_fail, 1U);
			goto done;
		}

		/* TXTSC bit should get reset when all timestamp read */
		while (((osi_readla(osi_core, (nveu8_t *)osi_core->base +
					       MGBE_MAC_TSS) &
			MGBE_MAC_TSS_TXTSC) == MGBE_MAC_TSS_TXTSC)) {
			nveu32_t i = get_free_ts_idx(l_core);

			if (i == MAX_TX_TS_CNT) {
				OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
					     "TS queue is full\n", i);
				break;
			}

			l_core->ts[i].nsec = osi_readla(osi_core,
						    (nveu8_t *)osi_core->base +
						     MGBE_MAC_TSNSSEC);

			l_core->ts[i].in_use = OSI_ENABLE;
			l_core->ts[i].pkt_id = osi_readla(osi_core,
						    (nveu8_t *)osi_core->base +
						     MGBE_MAC_TSPKID);
			l_core->ts[i].sec = osi_readla(osi_core,
						    (nveu8_t *)osi_core->base +
						     MGBE_MAC_TSSEC);
			/* Add time stamp to end of list */
			l_core->ts[i].next = head->prev->next;
			head->prev->next = &l_core->ts[i];
			l_core->ts[i].prev = head->prev;
			head->prev = &l_core->ts[i];
		}

		/* mask return as initial value is returned always */
		(void)__sync_fetch_and_sub(&l_core->ts_lock, 1);
	}
done:
	mac_isr &= ~MGBE_ISR_TSIS;

	osi_writela(osi_core, mac_isr,
		    (unsigned char *)osi_core->base + MGBE_MAC_ISR);
	/* TODO: Duplex/speed settigs - Its not same as EQOS for MGBE */
}

/**
 * @brief mgbe_update_dma_sr_stats - stats for dma_status error
 *
 * Algorithm: increament error stats based on corresponding bit filed.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] dma_sr: Dma status register read value
 * @param[in] qinx: Queue index
 */
static inline void mgbe_update_dma_sr_stats(struct osi_core_priv_data *osi_core,
					    nveu32_t dma_sr, nveu32_t qinx)
{
	nveu64_t val;

	if ((dma_sr & MGBE_DMA_CHX_STATUS_RBU) == MGBE_DMA_CHX_STATUS_RBU) {
		val = osi_core->xstats.rx_buf_unavail_irq_n[qinx];
		osi_core->xstats.rx_buf_unavail_irq_n[qinx] =
			osi_update_stats_counter(val, 1U);
	}
	if ((dma_sr & MGBE_DMA_CHX_STATUS_TPS) == MGBE_DMA_CHX_STATUS_TPS) {
		val = osi_core->xstats.tx_proc_stopped_irq_n[qinx];
		osi_core->xstats.tx_proc_stopped_irq_n[qinx] =
			osi_update_stats_counter(val, 1U);
	}
	if ((dma_sr & MGBE_DMA_CHX_STATUS_TBU) == MGBE_DMA_CHX_STATUS_TBU) {
		val = osi_core->xstats.tx_buf_unavail_irq_n[qinx];
		osi_core->xstats.tx_buf_unavail_irq_n[qinx] =
			osi_update_stats_counter(val, 1U);
	}
	if ((dma_sr & MGBE_DMA_CHX_STATUS_RPS) == MGBE_DMA_CHX_STATUS_RPS) {
		val = osi_core->xstats.rx_proc_stopped_irq_n[qinx];
		osi_core->xstats.rx_proc_stopped_irq_n[qinx] =
			osi_update_stats_counter(val, 1U);
	}
	if ((dma_sr & MGBE_DMA_CHX_STATUS_FBE) == MGBE_DMA_CHX_STATUS_FBE) {
		val = osi_core->xstats.fatal_bus_error_irq_n;
		osi_core->xstats.fatal_bus_error_irq_n =
			osi_update_stats_counter(val, 1U);
	}
}

/**
 * @brief mgbe_set_avb_algorithm - Set TxQ/TC avb config
 *
 * Algorithm:
 *	1) Check if queue index is valid
 *	2) Update operation mode of TxQ/TC
 *	 2a) Set TxQ operation mode
 *	 2b) Set Algo and Credit contro
 *	 2c) Set Send slope credit
 *	 2d) Set Idle slope credit
 *	 2e) Set Hi credit
 *	 2f) Set low credit
 *	3) Update register values
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[in] avb: structure having configuration for avb algorithm
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_set_avb_algorithm(
				struct osi_core_priv_data *const osi_core,
				const struct osi_core_avb_algorithm *const avb)
{
	unsigned int value;
	int ret = -1;
	unsigned int qinx = 0U;
	unsigned int tcinx = 0U;

	if (avb == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"avb structure is NULL\n",
			0ULL);
		return ret;
	}

	/* queue index in range */
	if (avb->qindex >= OSI_MGBE_MAX_NUM_QUEUES) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid Queue index\n",
			(unsigned long long)avb->qindex);
		return ret;
	}

	/* queue oper_mode in range check*/
	if (avb->oper_mode >= OSI_MTL_QUEUE_MODEMAX) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid Queue mode\n",
			(unsigned long long)avb->qindex);
		return ret;
	}

	/* Validate algo is valid  */
	if (avb->algo > OSI_MTL_TXQ_AVALG_CBS) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid Algo input\n",
			(unsigned long long)avb->tcindex);
		return ret;
	}

	/* can't set AVB mode for queue 0 */
	if ((avb->qindex == 0U) && (avb->oper_mode == OSI_MTL_QUEUE_AVB)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_OPNOTSUPP,
			"Not allowed to set AVB for Q0\n",
			(unsigned long long)avb->qindex);
		return ret;
	}

	/* TC index range check */
	if ((avb->tcindex == 0U) || (avb->tcindex >= OSI_MAX_TC_NUM)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid Queue TC mapping\n",
			(unsigned long long)avb->tcindex);
		return ret;
	}

	qinx = avb->qindex;
	tcinx = avb->tcindex;
	value = osi_readla(osi_core, (unsigned char *)osi_core->base +
			  MGBE_MTL_CHX_TX_OP_MODE(qinx));
	value &= ~MGBE_MTL_TX_OP_MODE_TXQEN;
	/* Set TXQEN mode as per input struct after masking 3 bit */
	value |= ((avb->oper_mode << MGBE_MTL_TX_OP_MODE_TXQEN_SHIFT) &
		  MGBE_MTL_TX_OP_MODE_TXQEN);
	/* Set TC mapping */
	value &= ~MGBE_MTL_TX_OP_MODE_Q2TCMAP;
	value |= ((tcinx << MGBE_MTL_TX_OP_MODE_Q2TCMAP_SHIFT) &
		  MGBE_MTL_TX_OP_MODE_Q2TCMAP);
	osi_writela(osi_core, value, (unsigned char *)osi_core->base +
		   MGBE_MTL_CHX_TX_OP_MODE(qinx));

	/* Set Algo and Credit control */
	value = osi_readla(osi_core, (unsigned char *)osi_core->base +
			  MGBE_MTL_TCQ_ETS_CR(tcinx));
	if (avb->algo == OSI_MTL_TXQ_AVALG_CBS) {
		value &= ~MGBE_MTL_TCQ_ETS_CR_CC;
		value |= (avb->credit_control << MGBE_MTL_TCQ_ETS_CR_CC_SHIFT) &
			 MGBE_MTL_TCQ_ETS_CR_CC;
	}
	value &= ~MGBE_MTL_TCQ_ETS_CR_AVALG;
	value |= (avb->algo << MGBE_MTL_TCQ_ETS_CR_AVALG_SHIFT) &
		  MGBE_MTL_TCQ_ETS_CR_AVALG;
	osi_writela(osi_core, value, (unsigned char *)osi_core->base +
		   MGBE_MTL_TCQ_ETS_CR(tcinx));

	if (avb->algo == OSI_MTL_TXQ_AVALG_CBS) {
		/* Set Idle slope credit*/
		value = osi_readla(osi_core, (unsigned char *)osi_core->base +
				  MGBE_MTL_TCQ_QW(tcinx));
		value &= ~MGBE_MTL_TCQ_ETS_QW_ISCQW_MASK;
		value |= avb->idle_slope & MGBE_MTL_TCQ_ETS_QW_ISCQW_MASK;
		osi_writela(osi_core, value, (unsigned char *)osi_core->base +
			   MGBE_MTL_TCQ_QW(tcinx));

		/* Set Send slope credit */
		value = osi_readla(osi_core, (unsigned char *)osi_core->base +
				  MGBE_MTL_TCQ_ETS_SSCR(tcinx));
		value &= ~MGBE_MTL_TCQ_ETS_SSCR_SSC_MASK;
		value |= avb->send_slope & MGBE_MTL_TCQ_ETS_SSCR_SSC_MASK;
		osi_writela(osi_core, value, (unsigned char *)osi_core->base +
			   MGBE_MTL_TCQ_ETS_SSCR(tcinx));

		/* Set Hi credit */
		value = avb->hi_credit & MGBE_MTL_TCQ_ETS_HCR_HC_MASK;
		osi_writela(osi_core, value, (unsigned char *)osi_core->base +
			   MGBE_MTL_TCQ_ETS_HCR(tcinx));

		/* low credit  is -ve number, osi_write need a unsigned int
		 * take only 28:0 bits from avb->low_credit
		 */
		value = avb->low_credit & MGBE_MTL_TCQ_ETS_LCR_LC_MASK;
		osi_writela(osi_core, value, (unsigned char *)osi_core->base +
			   MGBE_MTL_TCQ_ETS_LCR(tcinx));
	}

	return 0;
}

/**
 * @brief mgbe_get_avb_algorithm - Get TxQ/TC avb config
 *
 * Algorithm:
 *	1) Check if queue index is valid
 *	2) read operation mode of TxQ/TC
 *	 2a) read TxQ operation mode
 *	 2b) read Algo and Credit contro
 *	 2c) read Send slope credit
 *	 2d) read Idle slope credit
 *	 2e) read Hi credit
 *	 2f) read low credit
 *	3) updated pointer
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[out] avb: structure pointer having configuration for avb algorithm
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_get_avb_algorithm(struct osi_core_priv_data *const osi_core,
				  struct osi_core_avb_algorithm *const avb)
{
	unsigned int value;
	int ret = -1;
	unsigned int qinx = 0U;
	unsigned int tcinx = 0U;

	if (avb == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"avb structure is NULL\n",
			0ULL);
		return ret;
	}

	if (avb->qindex >= OSI_MGBE_MAX_NUM_QUEUES) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"Invalid Queue index\n",
			(unsigned long long)avb->qindex);
		return ret;
	}

	qinx = avb->qindex;
	value = osi_readla(osi_core, (unsigned char *)osi_core->base +
			  MGBE_MTL_CHX_TX_OP_MODE(qinx));

	/* Get TxQ/TC mode as per input struct after masking 3:2 bit */
	avb->oper_mode = (value & MGBE_MTL_TX_OP_MODE_TXQEN) >>
			  MGBE_MTL_TX_OP_MODE_TXQEN_SHIFT;

	/* Get Queue Traffic Class Mapping */
	avb->tcindex = ((value & MGBE_MTL_TX_OP_MODE_Q2TCMAP) >>
		 MGBE_MTL_TX_OP_MODE_Q2TCMAP_SHIFT);
	tcinx = avb->tcindex;

	/* Get Algo and Credit control */
	value = osi_readla(osi_core, (unsigned char *)osi_core->base +
			  MGBE_MTL_TCQ_ETS_CR(tcinx));
	avb->credit_control = (value & MGBE_MTL_TCQ_ETS_CR_CC) >>
			       MGBE_MTL_TCQ_ETS_CR_CC_SHIFT;
	avb->algo = (value & MGBE_MTL_TCQ_ETS_CR_AVALG) >>
		     MGBE_MTL_TCQ_ETS_CR_AVALG_SHIFT;

	if (avb->algo == OSI_MTL_TXQ_AVALG_CBS) {
		/* Get Idle slope credit*/
		value = osi_readla(osi_core, (unsigned char *)osi_core->base +
				  MGBE_MTL_TCQ_QW(tcinx));
		avb->idle_slope = value & MGBE_MTL_TCQ_ETS_QW_ISCQW_MASK;

		/* Get Send slope credit */
		value = osi_readla(osi_core, (unsigned char *)osi_core->base +
				  MGBE_MTL_TCQ_ETS_SSCR(tcinx));
		avb->send_slope = value & MGBE_MTL_TCQ_ETS_SSCR_SSC_MASK;

		/* Get Hi credit */
		value = osi_readla(osi_core, (unsigned char *)osi_core->base +
				  MGBE_MTL_TCQ_ETS_HCR(tcinx));
		avb->hi_credit = value & MGBE_MTL_TCQ_ETS_HCR_HC_MASK;

		/* Get Low credit for which bit 31:29 are unknown
		 * return 28:0 valid bits to application
		 */
		value = osi_readla(osi_core, (unsigned char *)osi_core->base +
				  MGBE_MTL_TCQ_ETS_LCR(tcinx));
		avb->low_credit = value & MGBE_MTL_TCQ_ETS_LCR_LC_MASK;
	}

	return 0;
}

/**
 * @brief mgbe_handle_mtl_intrs - Handle MTL interrupts
 *
 * Algorithm: Code to handle interrupt for MTL EST error and status.
 * There are possible 4 errors which can be part of common interrupt in case of
 * MTL_EST_SCH_ERR (sheduling error)- HLBS
 * MTL_EST_FRMS_ERR (Frame size error) - HLBF
 * MTL_EST_FRMC_ERR (frame check error) - HLBF
 * Constant Gate Control Error - when time interval in less
 * than or equal to cycle time, llr = 1
 * There is one status interrupt which says swich to SWOL complete.
 *
 * @param[in] osi_core: osi core priv data structure
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static void mgbe_handle_mtl_intrs(struct osi_core_priv_data *osi_core,
				  unsigned int mtl_isr)
{
	unsigned int val = 0U;
	unsigned int sch_err = 0U;
	unsigned int frm_err = 0U;
	unsigned int temp = 0U;
	unsigned int i = 0;
	unsigned long stat_val = 0U;
	unsigned int value = 0U;
	unsigned int qstatus = 0U;
	unsigned int qinx = 0U;

	/* Check for all MTL queues */
	for (i = 0; i < osi_core->num_mtl_queues; i++) {
		qinx = osi_core->mtl_queues[i];
		if (mtl_isr & OSI_BIT(qinx)) {
			/* check if Q has underflow error */
			qstatus = osi_readl((unsigned char *)osi_core->base +
					    MGBE_MTL_QINT_STATUS(qinx));
			/* Transmit Queue Underflow Interrupt Status */
			if (qstatus & MGBE_MTL_QINT_TXUNIFS) {
				osi_core->pkt_err_stats.mgbe_tx_underflow_err =
				osi_update_stats_counter(
				osi_core->pkt_err_stats.mgbe_tx_underflow_err,
				1UL);
			}
			/* Clear interrupt status by writing back with 1 */
			osi_writel(1U, (unsigned char *)osi_core->base +
				   MGBE_MTL_QINT_STATUS(qinx));
		}
	}

	if ((mtl_isr & MGBE_MTL_IS_ESTIS) != MGBE_MTL_IS_ESTIS) {
		return;
	}

	val = osi_readla(osi_core,
			 (nveu8_t *)osi_core->base + MGBE_MTL_EST_STATUS);
	val &= (MGBE_MTL_EST_STATUS_CGCE | MGBE_MTL_EST_STATUS_HLBS |
		MGBE_MTL_EST_STATUS_HLBF | MGBE_MTL_EST_STATUS_BTRE |
		MGBE_MTL_EST_STATUS_SWLC);

	/* return if interrupt is not related to EST */
	if (val == OSI_DISABLE) {
		return;
	}

	/* increase counter write 1 back will clear */
	if ((val & MGBE_MTL_EST_STATUS_CGCE) == MGBE_MTL_EST_STATUS_CGCE) {
		osi_core->est_ready = OSI_DISABLE;
		stat_val = osi_core->tsn_stats.const_gate_ctr_err;
		osi_core->tsn_stats.const_gate_ctr_err =
				osi_update_stats_counter(stat_val, 1U);
	}

	if ((val & MGBE_MTL_EST_STATUS_HLBS) == MGBE_MTL_EST_STATUS_HLBS) {
		osi_core->est_ready = OSI_DISABLE;
		stat_val = osi_core->tsn_stats.head_of_line_blk_sch;
		osi_core->tsn_stats.head_of_line_blk_sch =
				osi_update_stats_counter(stat_val, 1U);
		/* Need to read MTL_EST_Sch_Error register and cleared */
		sch_err = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				    MGBE_MTL_EST_SCH_ERR);
		for (i = 0U; i < OSI_MAX_TC_NUM; i++) {
			temp = OSI_ENABLE;
			temp = temp << i;
			if ((sch_err & temp) == temp) {
				stat_val = osi_core->tsn_stats.hlbs_q[i];
				osi_core->tsn_stats.hlbs_q[i] =
					osi_update_stats_counter(stat_val, 1U);
			}
		}
		sch_err &= 0xFFU; //only 8 TC allowed so clearing all
		osi_writela(osi_core, sch_err, (nveu8_t *)osi_core->base +
			   MGBE_MTL_EST_SCH_ERR);
		/* Reset EST with print to configure it properly */
		value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MGBE_MTL_EST_CONTROL);
		value &= ~MGBE_MTL_EST_EEST;
		osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
			    MGBE_MTL_EST_CONTROL);
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "Disabling EST due to HLBS, correct GCL\n", OSI_NONE);
	}

	if ((val & MGBE_MTL_EST_STATUS_HLBF) == MGBE_MTL_EST_STATUS_HLBF) {
		osi_core->est_ready = OSI_DISABLE;
		stat_val = osi_core->tsn_stats.head_of_line_blk_frm;
		osi_core->tsn_stats.head_of_line_blk_frm =
				osi_update_stats_counter(stat_val, 1U);
		/* Need to read MTL_EST_Frm_Size_Error register and cleared */
		frm_err = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				    MGBE_MTL_EST_FRMS_ERR);
		for (i = 0U; i < OSI_MAX_TC_NUM; i++) {
			temp = OSI_ENABLE;
			temp = temp << i;
			if ((frm_err & temp) == temp) {
				stat_val = osi_core->tsn_stats.hlbf_q[i];
				osi_core->tsn_stats.hlbf_q[i] =
					osi_update_stats_counter(stat_val, 1U);
			}
		}
		frm_err &= 0xFFU; //only 8 TC allowed so clearing all
		osi_writela(osi_core, frm_err, (nveu8_t *)osi_core->base +
			   MGBE_MTL_EST_FRMS_ERR);

		/* Reset EST with print to configure it properly */
		value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				   MGBE_MTL_EST_CONTROL);
		/* DDBF 1 means don't drop packets */
		if ((value & MGBE_MTL_EST_CONTROL_DDBF) ==
		    MGBE_MTL_EST_CONTROL_DDBF) {
			value &= ~MGBE_MTL_EST_EEST;
			osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
				    MGBE_MTL_EST_CONTROL);
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				     "Disabling EST due to HLBF, correct GCL\n",
				     OSI_NONE);
		}
	}

	if ((val & MGBE_MTL_EST_STATUS_SWLC) == MGBE_MTL_EST_STATUS_SWLC) {
		if ((val & MGBE_MTL_EST_STATUS_BTRE) !=
		    MGBE_MTL_EST_STATUS_BTRE) {
			osi_core->est_ready = OSI_ENABLE;
		}
		stat_val = osi_core->tsn_stats.sw_own_list_complete;
		osi_core->tsn_stats.sw_own_list_complete =
				osi_update_stats_counter(stat_val, 1U);
	}

	if ((val & MGBE_MTL_EST_STATUS_BTRE) == MGBE_MTL_EST_STATUS_BTRE) {
		osi_core->est_ready = OSI_DISABLE;
		stat_val = osi_core->tsn_stats.base_time_reg_err;
		osi_core->tsn_stats.base_time_reg_err =
				osi_update_stats_counter(stat_val, 1U);
		osi_core->est_ready = OSI_DISABLE;
	}
	/* clear EST status register as interrupt is handled */
	osi_writela(osi_core, val,
		    (nveu8_t *)osi_core->base + MGBE_MTL_EST_STATUS);

	mtl_isr &= ~MGBE_MTL_IS_ESTIS;
	osi_writela(osi_core, mtl_isr, (unsigned char *)osi_core->base +
		    MGBE_MTL_INTR_STATUS);
}

/**
 * @brief mgbe_config_ptp_offload - Enable/Disable PTP offload
 *
 * Algorithm: Based on input argument, update PTO and TSCR registers.
 * Update ptp_filter for TSCR register.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) configure_ptp() should be called after this API
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */

static int mgbe_config_ptp_offload(struct osi_core_priv_data *const osi_core,
				   struct osi_pto_config *const pto_config)
{
	unsigned char *addr = (unsigned char *)osi_core->base;
	int ret = 0;
	unsigned int value = 0x0U;
	unsigned int ptc_value = 0x0U;
	unsigned int port_id = 0x0U;

	/* Read MAC TCR */
	value = osi_readla(osi_core, (unsigned char *)addr + MGBE_MAC_TCR);
	/* clear old configuration */

	value &= ~(MGBE_MAC_TCR_TSENMACADDR | OSI_MAC_TCR_SNAPTYPSEL_3 |
		   OSI_MAC_TCR_TSMASTERENA | OSI_MAC_TCR_TSEVENTENA |
		   OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT |
		   OSI_MAC_TCR_TSCTRLSSR | OSI_MAC_TCR_TSVER2ENA |
		   OSI_MAC_TCR_TSIPENA);

	/** Handle PTO disable */
	if (pto_config->en_dis == OSI_DISABLE) {
		/* update global setting in ptp_filter */
		osi_core->ptp_config.ptp_filter = value;
		osi_writela(osi_core, ptc_value, addr + MGBE_MAC_PTO_CR);
		osi_writela(osi_core, value, addr + MGBE_MAC_TCR);
		/* Setting PORT ID as 0 */
		osi_writela(osi_core, OSI_NONE, addr + MGBE_MAC_PIDR0);
		osi_writela(osi_core, OSI_NONE, addr + MGBE_MAC_PIDR1);
		osi_writela(osi_core, OSI_NONE, addr + MGBE_MAC_PIDR2);
		return 0;
	}

	/** Handle PTO enable */
	/* Set PTOEN bit */
	ptc_value |= MGBE_MAC_PTO_CR_PTOEN;
	ptc_value |= ((pto_config->domain_num << MGBE_MAC_PTO_CR_DN_SHIFT)
		      & MGBE_MAC_PTO_CR_DN);

	/* Set TSCR register flag */
	value |= (OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT |
		  OSI_MAC_TCR_TSCTRLSSR | OSI_MAC_TCR_TSVER2ENA |
		  OSI_MAC_TCR_TSIPENA);

	if (pto_config->snap_type > 0U) {
		/* Set APDREQEN bit if snap_type > 0 */
		ptc_value |= MGBE_MAC_PTO_CR_APDREQEN;
	}

	/* Set SNAPTYPSEL for Taking Snapshots mode */
	value |= ((pto_config->snap_type << MGBE_MAC_TCR_SNAPTYPSEL_SHIFT) &
			OSI_MAC_TCR_SNAPTYPSEL_3);
	/* Set/Reset TSMSTRENA bit for Master/Slave */
	if (pto_config->master == OSI_ENABLE) {
		/* Set TSMSTRENA bit for master */
		value |= OSI_MAC_TCR_TSMASTERENA;
		if (pto_config->snap_type != OSI_PTP_SNAP_P2P) {
			/* Set ASYNCEN bit on PTO Control Register */
			ptc_value |= MGBE_MAC_PTO_CR_ASYNCEN;
		}
	} else {
		/* Reset TSMSTRENA bit for slave */
		value &= ~OSI_MAC_TCR_TSMASTERENA;
	}

	/* Set/Reset TSENMACADDR bit for UC/MC MAC */
	if (pto_config->mc_uc == OSI_ENABLE) {
		/* Set TSENMACADDR bit for MC/UC MAC PTP filter */
		value |= MGBE_MAC_TCR_TSENMACADDR;
	} else {
		/* Reset TSENMACADDR bit */
		value &= ~MGBE_MAC_TCR_TSENMACADDR;
	}

	/* Set TSEVNTENA bit for PTP events */
	value |= OSI_MAC_TCR_TSEVENTENA;

	/* update global setting in ptp_filter */
	osi_core->ptp_config.ptp_filter = value;
	/** Write PTO_CR and TCR registers */
	osi_writela(osi_core, ptc_value, addr + MGBE_MAC_PTO_CR);
	osi_writela(osi_core, value, addr + MGBE_MAC_TCR);
	/* Port ID for PTP offload packet created */
	port_id = pto_config->portid & MGBE_MAC_PIDR_PID_MASK;
	osi_writela(osi_core, port_id, addr + MGBE_MAC_PIDR0);
	osi_writela(osi_core, OSI_NONE, addr + MGBE_MAC_PIDR1);
	osi_writela(osi_core, OSI_NONE, addr + MGBE_MAC_PIDR2);

	return ret;
}

/**
 * @brief mgbe_handle_common_intr - Handles common interrupt.
 *
 * Algorithm: Clear common nve32_terrupt source.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static void mgbe_handle_common_intr(struct osi_core_priv_data *osi_core)
{
	void *base = osi_core->base;
	unsigned int dma_isr = 0;
	unsigned int qinx = 0;
	unsigned int i = 0;
	unsigned int dma_sr = 0;
	unsigned int dma_ier = 0;
	unsigned int mtl_isr = 0;
	unsigned int val = 0;

	/* FIXME: Disabling common interrupt. Needs to be fixed once
	 * RTL issue http://nvbugs/200517360 resolved.
	 */
	val = osi_readla(osi_core, (unsigned char *)osi_core->base +
			MGBE_WRAP_COMMON_INTR_ENABLE);
	val &= ~MGBE_MAC_SBD_INTR;
	osi_writela(osi_core, val, (unsigned char *)osi_core->base +
		   MGBE_WRAP_COMMON_INTR_ENABLE);

	dma_isr = osi_readla(osi_core, (nveu8_t *)base + MGBE_DMA_ISR);
	if (dma_isr == OSI_NONE) {
		return;
	}

	//FIXME Need to check how we can get the DMA channel here instead of
	//MTL Queues
	if ((dma_isr & MGBE_DMA_ISR_DCH0_DCH15_MASK) != OSI_NONE) {
		/* Handle Non-TI/RI nve32_terrupts */
		for (i = 0; i < osi_core->num_mtl_queues; i++) {
			qinx = osi_core->mtl_queues[i];

			if (qinx >= OSI_MGBE_MAX_NUM_CHANS) {
				continue;
			}

			/* read dma channel status register */
			dma_sr = osi_readla(osi_core, (nveu8_t *)base +
					   MGBE_DMA_CHX_STATUS(qinx));
			/* read dma channel nve32_terrupt enable register */
			dma_ier = osi_readla(osi_core, (nveu8_t *)base +
					    MGBE_DMA_CHX_IER(qinx));

			/* process only those nve32_terrupts which we
			 * have enabled.
			 */
			dma_sr = (dma_sr & dma_ier);

			/* mask off RI and TI */
			dma_sr &= ~(MGBE_DMA_CHX_STATUS_TI |
				    MGBE_DMA_CHX_STATUS_RI);
			if (dma_sr == OSI_NONE) {
				continue;
			}

			/* ack non ti/ri nve32_ts */
			osi_writela(osi_core, dma_sr, (nveu8_t *)base +
				   MGBE_DMA_CHX_STATUS(qinx));
			mgbe_update_dma_sr_stats(osi_core, dma_sr, qinx);
		}
	}

	mgbe_handle_mac_intrs(osi_core, dma_isr);

	/* Handle MTL inerrupts */
	mtl_isr = osi_readla(osi_core,
			     (unsigned char *)base + MGBE_MTL_INTR_STATUS);
	if ((dma_isr & MGBE_DMA_ISR_MTLIS) == MGBE_DMA_ISR_MTLIS) {
		mgbe_handle_mtl_intrs(osi_core, mtl_isr);
	}

	/* Clear common interrupt status in wrapper register */
	osi_writela(osi_core, MGBE_MAC_SBD_INTR,
		   (unsigned char *)base + MGBE_WRAP_COMMON_INTR_STATUS);
	val = osi_readla(osi_core, (unsigned char *)osi_core->base +
			MGBE_WRAP_COMMON_INTR_ENABLE);
	val |= MGBE_MAC_SBD_INTR;
	osi_writela(osi_core, val, (unsigned char *)osi_core->base +
		   MGBE_WRAP_COMMON_INTR_ENABLE);

	/* Clear FRP Interrupts in MTL_RXP_Interrupt_Control_Status */
	val = osi_readla(osi_core, (nveu8_t *)base + MGBE_MTL_RXP_INTR_CS);
	val |= (MGBE_MTL_RXP_INTR_CS_NVEOVIS |
		MGBE_MTL_RXP_INTR_CS_NPEOVIS |
		MGBE_MTL_RXP_INTR_CS_FOOVIS |
		MGBE_MTL_RXP_INTR_CS_PDRFIS);
	osi_writela(osi_core, val, (nveu8_t *)base + MGBE_MTL_RXP_INTR_CS);
}

/**
 * @brief mgbe_pad_calibrate - PAD calibration
 *
 * Algorithm: Since PAD calibration not applicable for MGBE
 * it returns zero.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @retval zero always
 */
static nve32_t mgbe_pad_calibrate(struct osi_core_priv_data *const osi_core)
{
	return 0;
}

/**
 * @brief mgbe_start_mac - Start MAC Tx/Rx engine
 *
 * Algorithm: Enable MAC Transmitter and Receiver
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note 1) MAC init should be complete. See osi_hw_core_init() and
 *	 osi_hw_dma_init()
 */
static void mgbe_start_mac(struct osi_core_priv_data *const osi_core)
{
	nveu32_t value;
	void *addr = osi_core->base;

	value = osi_readla(osi_core, (nveu8_t *)addr + MGBE_MAC_TMCR);
	/* Enable MAC Transmit */
	value |= MGBE_MAC_TMCR_TE;
	osi_writela(osi_core, value, (nveu8_t *)addr + MGBE_MAC_TMCR);

	value = osi_readla(osi_core, (nveu8_t *)addr + MGBE_MAC_RMCR);
	/* Enable MAC Receive */
	value |= MGBE_MAC_RMCR_RE;
	osi_writela(osi_core, value, (nveu8_t *)addr + MGBE_MAC_RMCR);
}

/**
 * @brief mgbe_stop_mac - Stop MAC Tx/Rx engine
 *
 * Algorithm: Disables MAC Transmitter and Receiver
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC DMA deinit should be complete. See osi_hw_dma_deinit()
 */
static void mgbe_stop_mac(struct osi_core_priv_data *const osi_core)
{
	nveu32_t value;
	void *addr = osi_core->base;

	value = osi_readla(osi_core, (nveu8_t *)addr + MGBE_MAC_TMCR);
	/* Disable MAC Transmit */
	value &= ~MGBE_MAC_TMCR_TE;
	osi_writela(osi_core, value, (nveu8_t *)addr + MGBE_MAC_TMCR);

	value = osi_readla(osi_core, (nveu8_t *)addr + MGBE_MAC_RMCR);
	/* Disable MAC Receive */
	value &= ~MGBE_MAC_RMCR_RE;
	osi_writela(osi_core, value, (nveu8_t *)addr + MGBE_MAC_RMCR);
}

#ifdef MACSEC_SUPPORT
/**
 * @brief mgbe_config_mac_tx - Enable/Disable MAC Tx
 *
 * Algorithm: Enable/Disable MAC Transmitter engine
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enable: Enable or Disable.MAC Tx
 *
 * @note 1) MAC init should be complete. See osi_hw_core_init()
 */
static void mgbe_config_mac_tx(struct osi_core_priv_data *const osi_core,
				const nveu32_t enable)
{
	nveu32_t value;
	void *addr = osi_core->base;

	if (enable == OSI_ENABLE) {
		value = osi_readla(osi_core, (nveu8_t *)addr + MGBE_MAC_TMCR);
		/* Enable MAC Transmit */
		value |= MGBE_MAC_TMCR_TE;
		osi_writela(osi_core, value, (nveu8_t *)addr + MGBE_MAC_TMCR);
	} else {
		value = osi_readla(osi_core, (nveu8_t *)addr + MGBE_MAC_TMCR);
		/* Disable MAC Transmit */
		value &= ~MGBE_MAC_TMCR_TE;
		osi_writela(osi_core, value, (nveu8_t *)addr + MGBE_MAC_TMCR);
	}
}
#endif /*  MACSEC_SUPPORT */

/**
 * @brief mgbe_core_deinit - MGBE MAC core deinitialization
 *
 * Algorithm: This function will take care of deinitializing MAC
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note Required clks and resets has to be enabled
 */
static void mgbe_core_deinit(struct osi_core_priv_data *osi_core)
{
	/* Stop the MAC by disabling both MAC Tx and Rx */
	mgbe_stop_mac(osi_core);
}

/**
 * @brief mgbe_set_speed - Set operating speed
 *
 * Algorithm: Based on the speed (2.5G/5G/10G) MAC will be configured
 *        accordingly.
 *
 * @param[in] osi_core:	OSI core private data.
 * @param[in] speed:    Operating speed.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static int mgbe_set_speed(struct osi_core_priv_data *const osi_core,
			  const int speed)
{
	unsigned int value = 0;

	value = osi_readla(osi_core,
			   (unsigned char *) osi_core->base + MGBE_MAC_TMCR);

	switch (speed) {
	case OSI_SPEED_2500:
		value |= MGBE_MAC_TMCR_SS_2_5G;
		break;
	case OSI_SPEED_5000:
		value |= MGBE_MAC_TMCR_SS_5G;
		break;
	case OSI_SPEED_10000:
		value &= ~MGBE_MAC_TMCR_SS_10G;
		break;
	default:
		/* setting default to 10G */
		value &= ~MGBE_MAC_TMCR_SS_10G;
		break;
	}

	osi_writela(osi_core, value, (unsigned char *)
		    osi_core->base + MGBE_MAC_TMCR);

	if (xpcs_init(osi_core) < 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
			     "xpcs_init failed\n", OSI_NONE);
		return -1;
	}

	return xpcs_start(osi_core);
}

/**
 * @brief mgbe_mdio_busy_wait - MDIO busy wait loop
 *
 * Algorithm: Wait for any previous MII read/write operation to complete
 *
 * @param[in] osi_core: OSI core data struture.
 */
static int mgbe_mdio_busy_wait(struct osi_core_priv_data *const osi_core)
{
	/* half second timeout */
	unsigned int retry = 50000;
	unsigned int mac_gmiiar;
	unsigned int count;
	int cond = 1;

	count = 0;
	while (cond == 1) {
		if (count > retry) {
			return -1;
		}

		count++;

		mac_gmiiar = osi_readla(osi_core, (unsigned char *)
					osi_core->base + MGBE_MDIO_SCCD);
		if ((mac_gmiiar & MGBE_MDIO_SCCD_SBUSY) == 0U) {
			cond = 0;
		} else {
			osi_core->osd_ops.udelay(10U);
		}
	}

	return 0;
}

/*
 * @brief mgbe_save_registers Function to store a backup of
 * MAC register space during SOC suspend.
 *
 * Algorithm: Read registers to be backed up as per struct core_backup and
 * store the register values in memory.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline int mgbe_save_registers(
				struct osi_core_priv_data *const osi_core)
{
	unsigned int i = 0;
	struct core_backup *config = &osi_core->backup_config;
	int ret = 0;

	/* Save direct access registers */
	for (i = 0; i < MGBE_DIRECT_MAX_BAK_IDX; i++) {
		if (config->reg_addr[i] != OSI_NULL) {
			/* Read the register and store into reg_val */
			config->reg_val[i] = osi_readla(osi_core,
							config->reg_addr[i]);
		}
	}

	/* Save L3 and L4 indirect addressing registers */
	for (i = 0; i < OSI_MGBE_MAX_L3_L4_FILTER; i++) {
		ret = mgbe_l3l4_filter_read(osi_core, i, MGBE_MAC_L3L4_CTR,
				&config->reg_val[MGBE_MAC_L3L4_CTR_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L3L4_CTR read fail return here */
			return ret;
		}
		ret = mgbe_l3l4_filter_read(osi_core, i, MGBE_MAC_L4_ADDR,
				&config->reg_val[MGBE_MAC_L4_ADR_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L4_ADDR read fail return here */
			return ret;
		}
		ret = mgbe_l3l4_filter_read(osi_core, i, MGBE_MAC_L3_AD0R,
				&config->reg_val[MGBE_MAC_L3_AD0R_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L3_AD0R read fail return here */
			return ret;
		}
		ret = mgbe_l3l4_filter_read(osi_core, i, MGBE_MAC_L3_AD1R,
				&config->reg_val[MGBE_MAC_L3_AD1R_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L3_AD1R read fail return here */
			return ret;
		}
		ret = mgbe_l3l4_filter_read(osi_core, i, MGBE_MAC_L3_AD2R,
				&config->reg_val[MGBE_MAC_L3_AD2R_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L3_AD2R read fail return here */
			return ret;
		}
		ret = mgbe_l3l4_filter_read(osi_core, i, MGBE_MAC_L3_AD3R,
				&config->reg_val[MGBE_MAC_L3_AD3R_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L3_AD3R read fail return here */
			return ret;
		}
	}

	/* Save MAC_DChSel_IndReg indirect addressing registers */
	for (i = 0; i < OSI_MGBE_MAX_MAC_ADDRESS_FILTER; i++) {
		ret = mgbe_mac_indir_addr_read(osi_core, MGBE_MAC_DCHSEL,
			i, &config->reg_val[MGBE_MAC_DCHSEL_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_DCHSEL read fail return here */
			return ret;
		}
	}

	return ret;
}

/**
 * @brief mgbe_restore_registers Function to restore the backup of
 * MAC registers during SOC resume.
 *
 * Algorithm: Restore the register values from the in memory backup taken using
 * mgbe_save_registers().
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline int mgbe_restore_registers(
				struct osi_core_priv_data *const osi_core)
{
	unsigned int i = 0;
	struct core_backup *config = &osi_core->backup_config;
	int ret = 0;

	/* Restore direct access registers */
	for (i = 0; i < MGBE_MAX_BAK_IDX; i++) {
		if (config->reg_addr[i] != OSI_NULL) {
			/* Write back the saved register value */
			osi_writela(osi_core, config->reg_val[i],
				    config->reg_addr[i]);
		}
	}

	/* Restore L3 and L4 indirect addressing registers */
	for (i = 0; i < OSI_MGBE_MAX_L3_L4_FILTER; i++) {
		ret = mgbe_l3l4_filter_write(osi_core, i, MGBE_MAC_L3L4_CTR,
				config->reg_val[MGBE_MAC_L3L4_CTR_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L3L4_CTR write fail return here */
			return ret;
		}
		ret = mgbe_l3l4_filter_write(osi_core, i, MGBE_MAC_L4_ADDR,
				config->reg_val[MGBE_MAC_L4_ADR_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L4_ADDR write fail return here */
			return ret;
		}
		ret = mgbe_l3l4_filter_write(osi_core, i, MGBE_MAC_L3_AD0R,
				config->reg_val[MGBE_MAC_L3_AD0R_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L3_AD0R write fail return here */
			return ret;
		}
		ret = mgbe_l3l4_filter_write(osi_core, i, MGBE_MAC_L3_AD1R,
				config->reg_val[MGBE_MAC_L3_AD1R_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L3_AD1R write fail return here */
			return ret;
		}
		ret = mgbe_l3l4_filter_write(osi_core, i, MGBE_MAC_L3_AD2R,
				config->reg_val[MGBE_MAC_L3_AD2R_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L3_AD2R write fail return here */
			return ret;
		}
		ret = mgbe_l3l4_filter_write(osi_core, i, MGBE_MAC_L3_AD3R,
				config->reg_val[MGBE_MAC_L3_AD3R_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_L3_AD3R write fail return here */
			return ret;
		}
	}

	/* Restore MAC_DChSel_IndReg indirect addressing registers */
	for (i = 0; i < OSI_MGBE_MAX_MAC_ADDRESS_FILTER; i++) {
		ret = mgbe_mac_indir_addr_write(osi_core, MGBE_MAC_DCHSEL,
			i, config->reg_val[MGBE_MAC_DCHSEL_BAK_IDX(i)]);
		if (ret < 0) {
			/* MGBE_MAC_DCHSEL write fail return here */
			return ret;
		}
	}

	return ret;
}

/**
 * @brief mgbe_write_phy_reg - Write to a PHY register over MDIO bus.
 *
 * Algorithm: Write into a PHY register through MGBE MDIO bus.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be write to PHY.
 * @param[in] phydata: Data to write to a PHY register.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_write_phy_reg(struct osi_core_priv_data *osi_core,
			      unsigned int phyaddr,
			      unsigned int phyreg,
			      unsigned short phydata)
{
	int ret = 0;
	unsigned int reg;

	if (osi_core == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID, "osi_core is NULL\n",
			0ULL);
		return -1;
	}

	/* Wait for any previous MII read/write operation to complete */
	ret = mgbe_mdio_busy_wait(osi_core);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd,
			OSI_LOG_ARG_HW_FAIL,
			"MII operation timed out\n",
			0ULL);
		return ret;
	}

	/* set MDIO address register */
	/* set device address */
	reg = ((phyreg >> MGBE_MDIO_C45_DA_SHIFT) & MGBE_MDIO_SCCA_DA_MASK) <<
	       MGBE_MDIO_SCCA_DA_SHIFT;
	/* set port address and register address */
	reg |= (phyaddr << MGBE_MDIO_SCCA_PA_SHIFT) |
		(phyreg & MGBE_MDIO_SCCA_RA_MASK);
	osi_writela(osi_core, reg, (unsigned char *)
		    osi_core->base + MGBE_MDIO_SCCA);

	/* Program Data register */
	reg = phydata |
	      (MGBE_MDIO_SCCD_CMD_WR << MGBE_MDIO_SCCD_CMD_SHIFT) |
	      MGBE_MDIO_SCCD_SBUSY;

	/**
	 * On FPGA AXI/APB clock is 13MHz. To achive maximum MDC clock
	 * of 2.5MHz need to enable CRS and CR to be set to 1.
	 * On Silicon AXI/APB clock is 408MHz. To achive maximum MDC clock
	 * of 2.5MHz only CR need to be set to 5.
	 */
	if (osi_core->pre_si) {
		reg |= (MGBE_MDIO_SCCD_CRS |
			((0x1U & MGBE_MDIO_SCCD_CR_MASK) <<
			MGBE_MDIO_SCCD_CR_SHIFT));
	} else {
		reg &= ~MGBE_MDIO_SCCD_CRS;
		reg |= ((0x5U & MGBE_MDIO_SCCD_CR_MASK) <<
			MGBE_MDIO_SCCD_CR_SHIFT);
	}

	osi_writela(osi_core, reg, (unsigned char *)
		    osi_core->base + MGBE_MDIO_SCCD);

	/* wait for MII write operation to complete */
	ret = mgbe_mdio_busy_wait(osi_core);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd,
			OSI_LOG_ARG_HW_FAIL,
			"MII operation timed out\n",
			0ULL);
		return ret;
	}

	return 0;
}

/**
 * @brief mgbe_read_phy_reg - Read from a PHY register over MDIO bus.
 *
 * Algorithm: Write into a PHY register through MGBE MDIO bus.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be read from PHY.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_read_phy_reg(struct osi_core_priv_data *osi_core,
			     unsigned int phyaddr,
			     unsigned int phyreg)
{
	unsigned int reg;
	unsigned int data;
	int ret = 0;

	ret = mgbe_mdio_busy_wait(osi_core);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd,
			OSI_LOG_ARG_HW_FAIL,
			"MII operation timed out\n",
			0ULL);
		return ret;
	}

	/* set MDIO address register */
	/* set device address */
	reg = ((phyreg >> MGBE_MDIO_C45_DA_SHIFT) & MGBE_MDIO_SCCA_DA_MASK) <<
	       MGBE_MDIO_SCCA_DA_SHIFT;
	/* set port address and register address */
	reg |= (phyaddr << MGBE_MDIO_SCCA_PA_SHIFT) |
		(phyreg & MGBE_MDIO_SCCA_RA_MASK);
	osi_writela(osi_core, reg, (unsigned char *)
		    osi_core->base + MGBE_MDIO_SCCA);

	/* Program Data register */
	reg = (MGBE_MDIO_SCCD_CMD_RD << MGBE_MDIO_SCCD_CMD_SHIFT) |
	       MGBE_MDIO_SCCD_SBUSY;

	 /**
         * On FPGA AXI/APB clock is 13MHz. To achive maximum MDC clock
         * of 2.5MHz need to enable CRS and CR to be set to 1.
         * On Silicon AXI/APB clock is 408MHz. To achive maximum MDC clock
         * of 2.5MHz only CR need to be set to 5.
         */
        if (osi_core->pre_si) {
                reg |= (MGBE_MDIO_SCCD_CRS |
                        ((0x1U & MGBE_MDIO_SCCD_CR_MASK) <<
                        MGBE_MDIO_SCCD_CR_SHIFT));
        } else {
                reg &= ~MGBE_MDIO_SCCD_CRS;
                reg |= ((0x5U & MGBE_MDIO_SCCD_CR_MASK) <<
                        MGBE_MDIO_SCCD_CR_SHIFT);
        }

	osi_writela(osi_core, reg, (unsigned char *)
		    osi_core->base + MGBE_MDIO_SCCD);

	ret = mgbe_mdio_busy_wait(osi_core);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd,
			OSI_LOG_ARG_HW_FAIL,
			"MII operation timed out\n",
			0ULL);
		return ret;
	}

	reg = osi_readla(osi_core, (unsigned char *)
			 osi_core->base + MGBE_MDIO_SCCD);

	data = (reg & MGBE_MDIO_SCCD_SDATA_MASK);
	return (int)data;
}

/**
 * @brief mgbe_hw_est_write - indirect write the GCL to Software own list
 * (SWOL)
 *
 * @param[in] base: MAC base IOVA address.
 * @param[in] addr_val: Address offset for indirect write.
 * @param[in] data: Data to be written at offset.
 * @param[in] gcla: Gate Control List Address, 0 for ETS register.
 *	      1 for GCL memory.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_hw_est_write(struct osi_core_priv_data *osi_core,
			     unsigned int addr_val, unsigned int data,
			     unsigned int gcla)
{
	int retry = 1000;
	unsigned int val = 0x0;

	osi_writela(osi_core, data, (unsigned char *)osi_core->base +
		   MGBE_MTL_EST_DATA);

	val &= ~MGBE_MTL_EST_ADDR_MASK;
	val |= (gcla == 1U) ? 0x0U : MGBE_MTL_EST_GCRR;
	val |= MGBE_MTL_EST_SRWO;
	val |= addr_val;
	osi_writela(osi_core, val, (unsigned char *)osi_core->base +
		   MGBE_MTL_EST_GCL_CONTROL);

	while (--retry > 0) {
		osi_core->osd_ops.udelay(OSI_DELAY_1US);
		val = osi_readla(osi_core, (unsigned char *)osi_core->base +
				MGBE_MTL_EST_GCL_CONTROL);
		if ((val & MGBE_MTL_EST_SRWO) == MGBE_MTL_EST_SRWO) {
			continue;
		}

		break;
	}

	if ((val & MGBE_MTL_EST_ERR0) == MGBE_MTL_EST_ERR0 ||
	    (retry <= 0)) {
		return -1;
	}

	return 0;
}

/**
 * @brief mgbe_gcl_validate - validate GCL from user
 *
 * Algorithm: validate GCL size and width of time interval value
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est: Configuration input argument.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline int mgbe_gcl_validate(struct osi_core_priv_data *osi_core,
				    struct osi_est_config *est)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	unsigned int i;

	if (est->llr > l_core->gcl_dep) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "input argument more than GCL depth\n",
			     (unsigned long long)est->llr);
		return -1;
	}

	for (i = 0U; i < est->llr; i++) {
		if (est->gcl[i] <= l_core->gcl_width_val) {
			continue;
		}

		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "validation of GCL entry failed\n",
			     (unsigned long long)i);
		return -1;
	}

	return 0;
}

/**
 * @brief mgbe_hw_config_est - Read Setting for GCL from input and update
 * registers.
 *
 * Algorithm:
 * 1) Write  TER, LLR and EST control register
 * 2) Update GCL to sw own GCL (MTL_EST_Status bit SWOL will tell which is
 *    owned by SW) and store which GCL is in use currently in sw.
 * 3) TODO set DBGB and DBGM for debugging
 * 4) EST_data and GCRR to 1, update entry sno in ADDR and put data at
 *    est_gcl_data enable GCL MTL_EST_SSWL and wait for self clear or use
 *    SWLC in MTL_EST_Status. Please note new GCL will be pushed for each entry.
 * 5) Configure btr. Update btr based on current time (current time
 *    should be updated based on PTP by this time)
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est: EST configuration input argument.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_hw_config_est(struct osi_core_priv_data *osi_core,
			      struct osi_est_config *est)
{
	unsigned int btr[2] = {0};
	unsigned int val = 0x0;
	void *base = osi_core->base;
	unsigned int i;
	int ret = 0;
	unsigned int addr = 0x0;

	if ((osi_core->hw_feature != OSI_NULL) &&
	    (osi_core->hw_feature->est_sel == OSI_DISABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "EST not supported in HW\n", 0ULL);
		return -1;
	}

	if (est->en_dis == OSI_DISABLE) {
		val = osi_readla(osi_core, (unsigned char *)
				 base + MGBE_MTL_EST_CONTROL);
		val &= ~MGBE_MTL_EST_EEST;
		osi_writela(osi_core, val, (unsigned char *)
			    base + MGBE_MTL_EST_CONTROL);

		return 0;
	}

	if (mgbe_gcl_validate(osi_core, est) < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "GCL validation failed\n", 0LL);
		return -1;
	}

	ret = mgbe_hw_est_write(osi_core, MGBE_MTL_EST_CTR_LOW, est->ctr[0], 0);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "GCL CTR[0] failed\n", 0LL);
		return ret;
	}
	/* check for est->ctr[i]  not more than FF, TODO as per hw config
	 * parameter we can have max 0x3 as this value in sec */
	est->ctr[1] &= MGBE_MTL_EST_CTR_HIGH_MAX;
	ret = mgbe_hw_est_write(osi_core, MGBE_MTL_EST_CTR_HIGH, est->ctr[1], 0);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "GCL CTR[1] failed\n", 0LL);
		return ret;
	}

	ret = mgbe_hw_est_write(osi_core, MGBE_MTL_EST_TER, est->ter, 0);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "GCL TER failed\n", 0LL);
		return ret;
	}

	ret = mgbe_hw_est_write(osi_core, MGBE_MTL_EST_LLR, est->llr, 0);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "GCL LLR failed\n", 0LL);
		return ret;
	}

	/* Write GCL table */
	for (i = 0U; i < est->llr; i++) {
		addr = i;
		addr = addr << MGBE_MTL_EST_ADDR_SHIFT;
		addr &= MGBE_MTL_EST_ADDR_MASK;
		ret = mgbe_hw_est_write(osi_core, addr, est->gcl[i], 1);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "GCL enties write failed\n",
				     (unsigned long long)i);
			return ret;
		}
	}

	btr[0] = est->btr[0];
	btr[1] = est->btr[1];
	if (btr[0] == 0U && btr[1] == 0U) {
		common_get_systime_from_mac(osi_core->base,
					    osi_core->mac,
					    &btr[1], &btr[0]);
	}
	/* Write parameters */
	ret = mgbe_hw_est_write(osi_core, MGBE_MTL_EST_BTR_LOW,
				btr[0] + est->btr_offset[0], OSI_DISABLE);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "GCL BTR[0] failed\n",
			     (unsigned long long)(btr[0] +
			     est->btr_offset[0]));
		return ret;
	}

	ret = mgbe_hw_est_write(osi_core, MGBE_MTL_EST_BTR_HIGH,
				btr[1] + est->btr_offset[1], OSI_DISABLE);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "GCL BTR[1] failed\n",
			     (unsigned long long)(btr[1] +
			     est->btr_offset[1]));
		return ret;
	}

	val = osi_readla(osi_core, (unsigned char *)
			 base + MGBE_MTL_EST_CONTROL);
	/* Store table */
	val |= MGBE_MTL_EST_SSWL;
	val |= MGBE_MTL_EST_EEST;
	val |= MGBE_MTL_EST_QHLBF;
	osi_writela(osi_core, val, (unsigned char *)
		    base + MGBE_MTL_EST_CONTROL);

	return ret;
}

/**
 * @brief mgbe_hw_config_fep - Read Setting for preemption and express for TC
 * and update registers.
 *
 * Algorithm:
 * 1) Check for TC enable and TC has masked for setting to preemptable.
 * 2) update FPE control status register
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] fpe: FPE configuration input argument.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_hw_config_fpe(struct osi_core_priv_data *osi_core,
			      struct osi_fpe_config *fpe)
{
	unsigned int i = 0U;
	unsigned int val = 0U;
	unsigned int temp = 0U, temp1 = 0U;
	unsigned int temp_shift = 0U;

	if ((osi_core->hw_feature != OSI_NULL) &&
	    (osi_core->hw_feature->fpe_sel == OSI_DISABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"FPE not supported in HW\n", 0ULL);
		return -1;
	}

	osi_core->fpe_ready = OSI_DISABLE;

	if (((fpe->tx_queue_preemption_enable << MGBE_MTL_FPE_CTS_PEC_SHIFT) &
	     MGBE_MTL_FPE_CTS_PEC) == OSI_DISABLE) {
		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MGBE_MTL_FPE_CTS);
		val &= ~MGBE_MTL_FPE_CTS_PEC;
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MGBE_MTL_FPE_CTS);

		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MGBE_MAC_FPE_CTS);
		val &= ~MGBE_MAC_FPE_CTS_EFPE;
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MGBE_MAC_FPE_CTS);

		return 0;
	}

	val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			 MGBE_MTL_FPE_CTS);
	val &= ~MGBE_MTL_FPE_CTS_PEC;
	for (i = 0U; i < OSI_MAX_TC_NUM; i++) {
	/* max 8 bit for this structure fot TC/TXQ. Set the TC for express or
	 * preemption. Default is express for a TC. DWCXG_NUM_TC = 8 */
		temp = OSI_BIT(i);
		if ((fpe->tx_queue_preemption_enable & temp) == temp) {
			temp_shift = i;
			temp_shift += MGBE_MTL_FPE_CTS_PEC_SHIFT;
			/* set queue for preemtable */
			if (temp_shift < MGBE_MTL_FPE_CTS_PEC_MAX_SHIFT) {
				temp1 = OSI_ENABLE;
				temp1 = temp1 << temp_shift;
				val |= temp1;
			} else {
				/* Do nothing */
			}
		}
	}
	osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
		    MGBE_MTL_FPE_CTS);

	if (fpe->rq == 0x0U || fpe->rq >= OSI_MGBE_MAX_NUM_CHANS) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"FPE init failed due to wrong RQ\n", fpe->rq);
		return -1;
	}

	val = osi_readla(osi_core, (unsigned char *)
			 osi_core->base + MGBE_MAC_RQC1R);
	val &= ~MGBE_MAC_RQC1R_RQ;
	temp = fpe->rq;
	temp = temp << MGBE_MAC_RQC1R_RQ_SHIFT;
	temp = (temp & MGBE_MAC_RQC1R_RQ);
	val |= temp;
	osi_core->residual_queue = fpe->rq;
	osi_writela(osi_core, val, (unsigned char *)
		    osi_core->base + MGBE_MAC_RQC1R);

	val = osi_readla(osi_core, (nveu8_t *)osi_core->base + MGBE_MAC_RQC4R);
	val &= ~MGBE_MAC_RQC4R_PMCBCQ;
	temp = fpe->rq;
	temp = temp << MGBE_MAC_RQC4R_PMCBCQ_SHIFT;
	temp = (temp & MGBE_MAC_RQC4R_PMCBCQ);
	val |= temp;
	osi_writela(osi_core, val, (nveu8_t *)osi_core->base + MGBE_MAC_RQC4R);

	/* initiate SVER for SMD-V and SMD-R */
	val = osi_readla(osi_core, (unsigned char *)
			 osi_core->base + MGBE_MTL_FPE_CTS);
	val |= MGBE_MAC_FPE_CTS_SVER;
	osi_writela(osi_core, val, (unsigned char *)
		    osi_core->base + MGBE_MAC_FPE_CTS);

	val = osi_readla(osi_core, (unsigned char *)
			 osi_core->base + MGBE_MTL_FPE_ADV);
	val &= ~MGBE_MTL_FPE_ADV_HADV_MASK;
	//(minimum_fragment_size +IPG/EIPG + Preamble) *.8 ~98ns for10G
	val |= MGBE_MTL_FPE_ADV_HADV_VAL;
	osi_writela(osi_core, val, (unsigned char *)
		    osi_core->base + MGBE_MTL_FPE_ADV);

	return 0;
}

/**
 * @brief mgbe_disable_tx_lpi - Helper function to disable Tx LPI.
 *
 * Algorithm:
 * Clear the bits to enable Tx LPI, Tx LPI automate, LPI Tx Timer and
 * PHY Link status in the LPI control/status register
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC has to be out of reset, and clocks supplied.
 */
static inline void mgbe_disable_tx_lpi(struct osi_core_priv_data *osi_core)
{
	unsigned int lpi_csr = 0;

	/* Disable LPI control bits */
	lpi_csr = osi_readla(osi_core, (unsigned char *)
			     osi_core->base + MGBE_MAC_LPI_CSR);
	lpi_csr &= ~(MGBE_MAC_LPI_CSR_LPITE | MGBE_MAC_LPI_CSR_LPITXA |
		     MGBE_MAC_LPI_CSR_PLS | MGBE_MAC_LPI_CSR_LPIEN);
	osi_writela(osi_core, lpi_csr, (unsigned char *)
		    osi_core->base + MGBE_MAC_LPI_CSR);
}

/**
 * @brief mgbe_configure_eee - Configure the EEE LPI mode
 *
 * Algorithm: This routine configures EEE LPI mode in the MAC.
 * 1) The time (in microsecond) to wait before resuming transmission after
 * exiting from LPI
 * 2) The time (in millisecond) to wait before LPI pattern can be transmitted
 * after PHY link is up) are not configurable. Default values are used in this
 * routine.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] tx_lpi_enable: enable (1)/disable (0) flag for Tx lpi
 * @param[in] tx_lpi_timer: Tx LPI entry timer in usec. Valid upto
 * OSI_MAX_TX_LPI_TIMER in steps of 8usec.
 *
 * @note Required clks and resets has to be enabled.
 * MAC/PHY should be initialized
 *
 */
static void mgbe_configure_eee(struct osi_core_priv_data *osi_core,
			       unsigned int tx_lpi_enabled,
			       unsigned int tx_lpi_timer)
{
	unsigned int lpi_csr = 0;
	unsigned int lpi_timer_ctrl = 0;
	unsigned int lpi_entry_timer = 0;
	unsigned int tic_counter = 0;
	void *addr =  osi_core->base;

	if (xpcs_eee(osi_core->xpcs_base, tx_lpi_enabled) != 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "xpcs_eee call failed\n", 0ULL);
		return;
	}

	if (tx_lpi_enabled != OSI_DISABLE) {
		/** 3. Program LST (bits[25:16]) and TWT (bits[15:0]) in
		 *	MAC_LPI_Timers_Control Register
		 * Configure the following timers.
		 *  a. LPI LS timer - minimum time (in milliseconds) for
		 *     which the link status from PHY should be up before
		 *     the LPI pattern can be transmitted to the PHY. Default
		 *     1sec
		 * b. LPI TW timer - minimum time (in microseconds) for which
		 *     MAC waits after it stops transmitting LPI pattern before
		 *     resuming normal tx. Default 21us
		 */

		lpi_timer_ctrl |= ((MGBE_DEFAULT_LPI_LS_TIMER <<
				   MGBE_LPI_LS_TIMER_SHIFT) &
				   MGBE_LPI_LS_TIMER_MASK);
		lpi_timer_ctrl |= (MGBE_DEFAULT_LPI_TW_TIMER &
				   MGBE_LPI_TW_TIMER_MASK);
		osi_writela(osi_core, lpi_timer_ctrl, (unsigned char *)addr +
			   MGBE_MAC_LPI_TIMER_CTRL);

		/* 4. For GMII, read the link status of the PHY chip by
		 * using the MDIO interface and update Bit 17 of
		 * MAC_LPI_Control_Status register accordingly. This update
		 * should be done whenever the link status in the PHY chip
		 * changes. For XGMII, the update is automatic unless
		 * PLSDIS bit is set." (skip) */
		/* 5. Program the MAC_1US_Tic_Counter as per the frequency
		 *	of the clock used for accessing the CSR slave port.
		 */
		/* Should be same as (ABP clock freq - 1) = 12 = 0xC, currently
		 * from define but we should get it from pdata->clock  TODO */
		tic_counter = MGBE_1US_TIC_COUNTER;
		osi_writela(osi_core, tic_counter, (unsigned char *)addr +
			   MGBE_MAC_1US_TIC_COUNT);

		/* 6. Program the MAC_LPI_Auto_Entry_Timer register (LPIET)
		 *	 with the IDLE time for which the MAC should wait
		 *	 before entering the LPI state on its own.
		 * LPI entry timer - Time in microseconds that MAC will wait
		 * to enter LPI mode after all tx is complete. Default 1sec
		 */
		lpi_entry_timer |= (tx_lpi_timer & MGBE_LPI_ENTRY_TIMER_MASK);
		osi_writela(osi_core, lpi_entry_timer, (unsigned char *)addr +
			   MGBE_MAC_LPI_EN_TIMER);

		/* 7. Set LPIATE and LPITXA (bit[20:19]) of
		 *    MAC_LPI_Control_Status register to enable the auto-entry
		 *    into LPI and auto-exit of MAC from LPI state.
		 * 8. Set LPITXEN (bit[16]) of MAC_LPI_Control_Status register
		 *    to make the MAC Transmitter enter the LPI state. The MAC
		 *    enters the LPI mode after completing all scheduled
		 *    packets and remain IDLE for the time indicated by LPIET.
		 */
		lpi_csr = osi_readla(osi_core, (unsigned char *)
				     addr + MGBE_MAC_LPI_CSR);
		lpi_csr |= (MGBE_MAC_LPI_CSR_LPITE | MGBE_MAC_LPI_CSR_LPITXA |
			    MGBE_MAC_LPI_CSR_PLS | MGBE_MAC_LPI_CSR_LPIEN);
		osi_writela(osi_core, lpi_csr, (unsigned char *)
			    addr + MGBE_MAC_LPI_CSR);
	} else {
		/* Disable LPI control bits */
		mgbe_disable_tx_lpi(osi_core);
	}
}

static int mgbe_get_hw_features(struct osi_core_priv_data *osi_core,
				struct osi_hw_features *hw_feat)
{
	unsigned char *base = (unsigned char *)osi_core->base;
	unsigned int mac_hfr0 = 0;
	unsigned int mac_hfr1 = 0;
	unsigned int mac_hfr2 = 0;
	unsigned int mac_hfr3 = 0;
	unsigned int val = 0;

	mac_hfr0 = osi_readla(osi_core, base + MGBE_MAC_HFR0);
	mac_hfr1 = osi_readla(osi_core, base + MGBE_MAC_HFR1);
	mac_hfr2 = osi_readla(osi_core, base + MGBE_MAC_HFR2);
	mac_hfr3 = osi_readla(osi_core, base + MGBE_MAC_HFR3);

	hw_feat->rgmii_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_RGMIISEL_SHIFT) &
			      MGBE_MAC_HFR0_RGMIISEL_MASK);
	hw_feat->gmii_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_GMIISEL_SHIFT) &
			     MGBE_MAC_HFR0_GMIISEL_MASK);
	hw_feat->rmii_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_RMIISEL_SHIFT) &
			     MGBE_MAC_HFR0_RMIISEL_MASK);
	hw_feat->hd_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_HDSEL_SHIFT) &
			   MGBE_MAC_HFR0_HDSEL_MASK);
	hw_feat->vlan_hash_en = ((mac_hfr0 >> MGBE_MAC_HFR0_VLHASH_SHIFT) &
				 MGBE_MAC_HFR0_VLHASH_MASK);
	hw_feat->sma_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_SMASEL_SHIFT) &
			    MGBE_MAC_HFR0_SMASEL_MASK);
	hw_feat->rwk_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_RWKSEL_SHIFT) &
			    MGBE_MAC_HFR0_RWKSEL_MASK);
	hw_feat->mgk_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_MGKSEL_SHIFT) &
			    MGBE_MAC_HFR0_MGKSEL_MASK);
	hw_feat->mmc_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_MMCSEL_SHIFT) &
			    MGBE_MAC_HFR0_MMCSEL_MASK);
	hw_feat->arp_offld_en = ((mac_hfr0 >> MGBE_MAC_HFR0_ARPOFFLDEN_SHIFT) &
				 MGBE_MAC_HFR0_ARPOFFLDEN_MASK);
	hw_feat->rav_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_RAVSEL_SHIFT) &
			    MGBE_MAC_HFR0_RAVSEL_MASK);
	hw_feat->av_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_AVSEL_SHIFT) &
			   MGBE_MAC_HFR0_AVSEL_MASK);
	hw_feat->ts_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_TSSSEL_SHIFT) &
			   MGBE_MAC_HFR0_TSSSEL_MASK);
	hw_feat->eee_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_EEESEL_SHIFT) &
			    MGBE_MAC_HFR0_EEESEL_MASK);
	hw_feat->tx_coe_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_TXCOESEL_SHIFT) &
			       MGBE_MAC_HFR0_TXCOESEL_MASK);
	hw_feat->rx_coe_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_RXCOESEL_SHIFT) &
			       MGBE_MAC_HFR0_RXCOESEL_MASK);
	hw_feat->mac_addr_sel =
			((mac_hfr0 >> MGBE_MAC_HFR0_ADDMACADRSEL_SHIFT) &
			 MGBE_MAC_HFR0_ADDMACADRSEL_MASK);
	hw_feat->act_phy_sel = ((mac_hfr0 >> MGBE_MAC_HFR0_PHYSEL_SHIFT) &
				MGBE_MAC_HFR0_PHYSEL_MASK);
	hw_feat->tsstssel = ((mac_hfr0 >> MGBE_MAC_HFR0_TSSTSSEL_SHIFT) &
			     MGBE_MAC_HFR0_TSSTSSEL_MASK);
	hw_feat->sa_vlan_ins  = ((mac_hfr0 >> MGBE_MAC_HFR0_SAVLANINS_SHIFT) &
				 MGBE_MAC_HFR0_SAVLANINS_SHIFT);
	hw_feat->vxn = ((mac_hfr0 >> MGBE_MAC_HFR0_VXN_SHIFT) &
			MGBE_MAC_HFR0_VXN_MASK);
	hw_feat->ediffc = ((mac_hfr0 >> MGBE_MAC_HFR0_EDIFFC_SHIFT) &
			   MGBE_MAC_HFR0_EDIFFC_MASK);
	hw_feat->edma = ((mac_hfr0 >> MGBE_MAC_HFR0_EDMA_SHIFT) &
			 MGBE_MAC_HFR0_EDMA_MASK);
	hw_feat->rx_fifo_size = ((mac_hfr1 >> MGBE_MAC_HFR1_RXFIFOSIZE_SHIFT) &
				 MGBE_MAC_HFR1_RXFIFOSIZE_MASK);
	hw_feat->pfc_en = ((mac_hfr1 >> MGBE_MAC_HFR1_PFCEN_SHIFT) &
			   MGBE_MAC_HFR1_PFCEN_MASK);
	hw_feat->tx_fifo_size = ((mac_hfr1 >> MGBE_MAC_HFR1_TXFIFOSIZE_SHIFT) &
				 MGBE_MAC_HFR1_TXFIFOSIZE_MASK);
	hw_feat->ost_en = ((mac_hfr1 >> MGBE_MAC_HFR1_OSTEN_SHIFT) &
			   MGBE_MAC_HFR1_OSTEN_MASK);
	hw_feat->pto_en = ((mac_hfr1 >> MGBE_MAC_HFR1_PTOEN_SHIFT) &
			   MGBE_MAC_HFR1_PTOEN_MASK);
	hw_feat->adv_ts_hword = ((mac_hfr1 >> MGBE_MAC_HFR1_ADVTHWORD_SHIFT) &
				 MGBE_MAC_HFR1_ADVTHWORD_MASK);
	hw_feat->addr_64 = ((mac_hfr1 >> MGBE_MAC_HFR1_ADDR64_SHIFT) &
			    MGBE_MAC_HFR1_ADDR64_MASK);
	hw_feat->dcb_en = ((mac_hfr1 >> MGBE_MAC_HFR1_DCBEN_SHIFT) &
			   MGBE_MAC_HFR1_DCBEN_MASK);
	hw_feat->sph_en = ((mac_hfr1 >> MGBE_MAC_HFR1_SPHEN_SHIFT) &
			   MGBE_MAC_HFR1_SPHEN_MASK);
	hw_feat->tso_en = ((mac_hfr1 >> MGBE_MAC_HFR1_TSOEN_SHIFT) &
			   MGBE_MAC_HFR1_TSOEN_MASK);
	hw_feat->dma_debug_gen = ((mac_hfr1 >> MGBE_MAC_HFR1_DBGMEMA_SHIFT) &
				  MGBE_MAC_HFR1_DBGMEMA_MASK);
	hw_feat->rss_en = ((mac_hfr1 >> MGBE_MAC_HFR1_RSSEN_SHIFT) &
			   MGBE_MAC_HFR1_RSSEN_MASK);
	hw_feat->num_tc = ((mac_hfr1 >> MGBE_MAC_HFR1_NUMTC_SHIFT) &
			   MGBE_MAC_HFR1_NUMTC_MASK);
	hw_feat->hash_tbl_sz = ((mac_hfr1 >> MGBE_MAC_HFR1_HASHTBLSZ_SHIFT) &
				MGBE_MAC_HFR1_HASHTBLSZ_MASK);
	hw_feat->l3l4_filter_num = ((mac_hfr1 >> MGBE_MAC_HFR1_L3L4FNUM_SHIFT) &
				    MGBE_MAC_HFR1_L3L4FNUM_MASK);
	hw_feat->rx_q_cnt = ((mac_hfr2 >> MGBE_MAC_HFR2_RXQCNT_SHIFT) &
			     MGBE_MAC_HFR2_RXQCNT_MASK);
	hw_feat->tx_q_cnt = ((mac_hfr2 >> MGBE_MAC_HFR2_TXQCNT_SHIFT) &
			     MGBE_MAC_HFR2_TXQCNT_MASK);
	hw_feat->rx_ch_cnt = ((mac_hfr2 >> MGBE_MAC_HFR2_RXCHCNT_SHIFT) &
			      MGBE_MAC_HFR2_RXCHCNT_MASK);
	hw_feat->tx_ch_cnt = ((mac_hfr2 >> MGBE_MAC_HFR2_TXCHCNT_SHIFT) &
			      MGBE_MAC_HFR2_TXCHCNT_MASK);
	hw_feat->pps_out_num = ((mac_hfr2 >> MGBE_MAC_HFR2_PPSOUTNUM_SHIFT) &
				MGBE_MAC_HFR2_PPSOUTNUM_MASK);
	hw_feat->aux_snap_num = ((mac_hfr2 >> MGBE_MAC_HFR2_AUXSNAPNUM_SHIFT) &
				 MGBE_MAC_HFR2_AUXSNAPNUM_MASK);
	hw_feat->num_vlan_filters = ((mac_hfr3 >> MGBE_MAC_HFR3_NRVF_SHIFT) &
				     MGBE_MAC_HFR3_NRVF_MASK);
	hw_feat->frp_sel = ((mac_hfr3 >> MGBE_MAC_HFR3_FRPSEL_SHIFT) &
			    MGBE_MAC_HFR3_FRPSEL_MASK);
	hw_feat->cbti_sel = ((mac_hfr3 >> MGBE_MAC_HFR3_CBTISEL_SHIFT) &
			    MGBE_MAC_HFR3_CBTISEL_MASK);
	hw_feat->num_frp_pipes = ((mac_hfr3 >> MGBE_MAC_HFR3_FRPPIPE_SHIFT) &
			    MGBE_MAC_HFR3_FRPPIPE_MASK);
	hw_feat->ost_over_udp = ((mac_hfr3 >> MGBE_MAC_HFR3_POUOST_SHIFT) &
				MGBE_MAC_HFR3_POUOST_MASK);

	val = ((mac_hfr3 >> MGBE_MAC_HFR3_FRPPB_SHIFT) &
		MGBE_MAC_HFR3_FRPPB_MASK);
	switch (val) {
	case MGBE_MAC_FRPPB_64:
		hw_feat->max_frp_bytes = MGBE_MAC_FRP_BYTES64;
		break;
	case MGBE_MAC_FRPPB_128:
		hw_feat->max_frp_bytes = MGBE_MAC_FRP_BYTES128;
		break;
	case MGBE_MAC_FRPPB_256:
	default:
		hw_feat->max_frp_bytes = MGBE_MAC_FRP_BYTES256;
		break;
	}
	val = ((mac_hfr3 >> MGBE_MAC_HFR3_FRPES_SHIFT) &
	       MGBE_MAC_HFR3_FRPES_MASK);
	switch (val) {
	case MGBE_MAC_FRPES_64:
		hw_feat->max_frp_entries = MGBE_MAC_FRP_BYTES64;
		break;
	case MGBE_MAC_FRPES_128:
		hw_feat->max_frp_entries = MGBE_MAC_FRP_BYTES128;
		break;
	case MGBE_MAC_FRPES_256:
	default:
		hw_feat->max_frp_entries = MGBE_MAC_FRP_BYTES256;
		break;
	}

	hw_feat->double_vlan_en = ((mac_hfr3 >> MGBE_MAC_HFR3_DVLAN_SHIFT) &
				   MGBE_MAC_HFR3_DVLAN_MASK);
	hw_feat->auto_safety_pkg = ((mac_hfr3 >> MGBE_MAC_HFR3_ASP_SHIFT) &
				    MGBE_MAC_HFR3_ASP_MASK);
	hw_feat->tts_fifo_depth = ((mac_hfr3 >> MGBE_MAC_HFR3_TTSFD_SHIFT) &
				    MGBE_MAC_HFR3_TTSFD_MASK);
	hw_feat->est_sel = ((mac_hfr3 >> MGBE_MAC_HFR3_ESTSEL_SHIFT) &
			    MGBE_MAC_HFR3_ESTSEL_MASK);
	hw_feat->gcl_depth = ((mac_hfr3 >> MGBE_MAC_HFR3_GCLDEP_SHIFT) &
			      MGBE_MAC_HFR3_GCLDEP_MASK);
	hw_feat->gcl_width = ((mac_hfr3 >> MGBE_MAC_HFR3_GCLWID_SHIFT) &
			      MGBE_MAC_HFR3_GCLWID_MASK);
	hw_feat->fpe_sel = ((mac_hfr3 >> MGBE_MAC_HFR3_FPESEL_SHIFT) &
			    MGBE_MAC_HFR3_FPESEL_MASK);
	hw_feat->tbs_sel = ((mac_hfr3 >> MGBE_MAC_HFR3_TBSSEL_SHIFT) &
			    MGBE_MAC_HFR3_TBSSEL_MASK);
	hw_feat->num_tbs_ch = ((mac_hfr3 >> MGBE_MAC_HFR3_TBS_CH_SHIFT) &
			       MGBE_MAC_HFR3_TBS_CH_MASK);

	return 0;
}

/**
 * @brief mgbe_poll_for_tsinit_complete - Poll for time stamp init complete
 *
 * Algorithm: Read TSINIT value from MAC TCR register until it is
 *	equal to zero.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] mac_tcr: Address to store time stamp control register read value
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline int mgbe_poll_for_tsinit_complete(
					struct osi_core_priv_data *osi_core,
					unsigned int *mac_tcr)
{
	unsigned int retry = 0U;

	while (retry < OSI_POLL_COUNT) {
		/* Read and Check TSINIT in MAC_Timestamp_Control register */
		*mac_tcr = osi_readla(osi_core, (unsigned char *)
				      osi_core->base + MGBE_MAC_TCR);
		if ((*mac_tcr & MGBE_MAC_TCR_TSINIT) == 0U) {
			return 0;
		}

		retry++;
		osi_core->osd_ops.udelay(OSI_DELAY_1000US);
	}

	return -1;
}

/**
 * @brief mgbe_set_systime - Set system time
 *
 * Algorithm: Updates system time (seconds and nano seconds)
 *	in hardware registers
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] sec: Seconds to be configured
 * @param[in] nsec: Nano Seconds to be configured
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_set_systime_to_mac(struct osi_core_priv_data *osi_core,
				   unsigned int sec,
				   unsigned int nsec)
{
	unsigned int mac_tcr;
	void *addr = osi_core->base;
	int ret;

	/* To be sure previous write was flushed (if Any) */
	ret = mgbe_poll_for_tsinit_complete(osi_core, &mac_tcr);
	if (ret == -1) {
		return -1;
	}

	/* write seconds value to MAC_System_Time_Seconds_Update register */
	osi_writela(osi_core, sec, (unsigned char *)addr + MGBE_MAC_STSUR);

	/* write nano seconds value to MAC_System_Time_Nanoseconds_Update
	 * register
	 */
	osi_writela(osi_core, nsec, (unsigned char *)addr + MGBE_MAC_STNSUR);

	/* issue command to update the configured secs and nsecs values */
	mac_tcr |= MGBE_MAC_TCR_TSINIT;
	osi_writela(osi_core, mac_tcr, (unsigned char *)addr + MGBE_MAC_TCR);

	ret = mgbe_poll_for_tsinit_complete(osi_core, &mac_tcr);
	if (ret == -1) {
		return -1;
	}

	return 0;
}

/**
 * @brief mgbe_poll_for_addend_complete - Poll for addend value write complete
 *
 * Algorithm: Read TSADDREG value from MAC TCR register until it is
 *	equal to zero.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] mac_tcr: Address to store time stamp control register read value
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline int mgbe_poll_for_addend_complete(
					struct osi_core_priv_data *osi_core,
					unsigned int *mac_tcr)
{
	unsigned int retry = 0U;

	/* Poll */
	while (retry < OSI_POLL_COUNT) {
		/* Read and Check TSADDREG in MAC_Timestamp_Control register */
		*mac_tcr = osi_readla(osi_core, (unsigned char *)
				      osi_core->base + MGBE_MAC_TCR);
		if ((*mac_tcr & MGBE_MAC_TCR_TSADDREG) == 0U) {
			return 0;
		}

		retry++;
		osi_core->osd_ops.udelay(OSI_DELAY_1000US);
	}

	return -1;
}

/**
 * @brief mgbe_config_addend - Configure addend
 *
 * Algorithm: Updates the Addend value in HW register
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] addend: Addend value to be configured
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_config_addend(struct osi_core_priv_data *osi_core,
			      unsigned int addend)
{
	unsigned int mac_tcr;
	void *addr = osi_core->base;
	int ret;

	/* To be sure previous write was flushed (if Any) */
	ret = mgbe_poll_for_addend_complete(osi_core, &mac_tcr);
	if (ret == -1) {
		return -1;
	}

	/* write addend value to MAC_Timestamp_Addend register */
	osi_writela(osi_core, addend, (unsigned char *)addr + MGBE_MAC_TAR);

	/* issue command to update the configured addend value */
	mac_tcr |= MGBE_MAC_TCR_TSADDREG;
	osi_writela(osi_core, mac_tcr, (unsigned char *)addr + MGBE_MAC_TCR);

	ret = mgbe_poll_for_addend_complete(osi_core, &mac_tcr);
	if (ret == -1) {
		return -1;
	}

	return 0;
}

/**
 * @brief mgbe_poll_for_update_ts_complete - Poll for update time stamp
 *
 * Algorithm: Read time stamp update value from TCR register until it is
 *	equal to zero.
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] mac_tcr: Address to store time stamp control register read value
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline int mgbe_poll_for_update_ts_complete(
				struct osi_core_priv_data *osi_core,
				unsigned int *mac_tcr)
{
	unsigned int retry = 0U;

	while (retry < OSI_POLL_COUNT) {
		/* Read and Check TSUPDT in MAC_Timestamp_Control register */
		*mac_tcr = osi_readla(osi_core, (unsigned char *)
				      osi_core->base + MGBE_MAC_TCR);
		if ((*mac_tcr & MGBE_MAC_TCR_TSUPDT) == 0U) {
			return 0;
		}

		retry++;
		osi_core->osd_ops.udelay(OSI_DELAY_1000US);
	}

	return -1;
}

/**
 * @brief mgbe_adjust_mactime - Adjust MAC time with system time
 *
 * Algorithm: Update MAC time with system time
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] sec: Seconds to be configured
 * @param[in] nsec: Nano seconds to be configured
 * @param[in] add_sub: To decide on add/sub with system time
 * @param[in] one_nsec_accuracy: One nano second accuracy
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) osi_core->ptp_config.one_nsec_accuracy need to be set to 1
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static int mgbe_adjust_mactime(struct osi_core_priv_data *osi_core,
			       unsigned int sec, unsigned int nsec,
			       unsigned int add_sub,
			       unsigned int one_nsec_accuracy)
{
	void *addr = osi_core->base;
	unsigned int mac_tcr;
	unsigned int value = 0;
	unsigned long long temp = 0;
	int ret;

	/* To be sure previous write was flushed (if Any) */
	ret = mgbe_poll_for_update_ts_complete(osi_core, &mac_tcr);
	if (ret == -1) {
		return -1;
	}

	if (add_sub != 0U) {
		/* If the new sec value needs to be subtracted with
		 * the system time, then MAC_STSUR reg should be
		 * programmed with (2^32 – <new_sec_value>)
		 */
		temp = (TWO_POWER_32 - sec);
		if (temp < UINT_MAX) {
			sec = (unsigned int)temp;
		} else {
			/* do nothing here */
		}

		/* If the new nsec value need to be subtracted with
		 * the system time, then MAC_STNSUR.TSSS field should be
		 * programmed with, (10^9 - <new_nsec_value>) if
		 * MAC_TCR.TSCTRLSSR is set or
		 * (2^32 - <new_nsec_value> if MAC_TCR.TSCTRLSSR is reset)
		 */
		if (one_nsec_accuracy == OSI_ENABLE) {
			if (nsec < UINT_MAX) {
				nsec = (TEN_POWER_9 - nsec);
			}
		} else {
			if (nsec < UINT_MAX) {
				nsec = (TWO_POWER_31 - nsec);
			}
		}
	}

	/* write seconds value to MAC_System_Time_Seconds_Update register */
	osi_writela(osi_core, sec, (unsigned char *)addr + MGBE_MAC_STSUR);

	/* write nano seconds value and add_sub to
	 * MAC_System_Time_Nanoseconds_Update register
	 */
	value |= nsec;
	value |= (add_sub << MGBE_MAC_STNSUR_ADDSUB_SHIFT);
	osi_writela(osi_core, value, (unsigned char *)addr + MGBE_MAC_STNSUR);

	/* issue command to initialize system time with the value
	 * specified in MAC_STSUR and MAC_STNSUR
	 */
	mac_tcr |= MGBE_MAC_TCR_TSUPDT;
	osi_writela(osi_core, mac_tcr, (unsigned char *)addr + MGBE_MAC_TCR);

	ret = mgbe_poll_for_update_ts_complete(osi_core, &mac_tcr);
	if (ret == -1) {
		return -1;
	}

	return 0;
}

/**
 * @brief mgbe_config_tscr - Configure Time Stamp Register
 *
 * @param[in] addr: Base address indicating the start of
 *	      memory mapped IO region of the MAC.
 * @param[in] ptp_filter: PTP rx filter parameters
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static void mgbe_config_tscr(struct osi_core_priv_data *osi_core,
			     unsigned int ptp_filter)
{
	unsigned int mac_tcr = 0;
	void *addr = osi_core->base;

	if (ptp_filter != OSI_DISABLE) {
		mac_tcr = (OSI_MAC_TCR_TSENA	|
			   OSI_MAC_TCR_TSCFUPDT |
			   OSI_MAC_TCR_TSCTRLSSR);

		if ((ptp_filter & OSI_MAC_TCR_SNAPTYPSEL_1) ==
		    OSI_MAC_TCR_SNAPTYPSEL_1) {
			mac_tcr |= OSI_MAC_TCR_SNAPTYPSEL_1;
		}
		if ((ptp_filter & OSI_MAC_TCR_SNAPTYPSEL_2) ==
		    OSI_MAC_TCR_SNAPTYPSEL_2) {
			mac_tcr |= OSI_MAC_TCR_SNAPTYPSEL_2;
		}
		if ((ptp_filter & OSI_MAC_TCR_SNAPTYPSEL_3) ==
		    OSI_MAC_TCR_SNAPTYPSEL_3) {
			mac_tcr |= OSI_MAC_TCR_SNAPTYPSEL_3;
		}
		if ((ptp_filter & OSI_MAC_TCR_TSIPV4ENA) ==
		    OSI_MAC_TCR_TSIPV4ENA) {
			mac_tcr |= OSI_MAC_TCR_TSIPV4ENA;
		}
		if ((ptp_filter & OSI_MAC_TCR_TSIPV6ENA) ==
		    OSI_MAC_TCR_TSIPV6ENA) {
			mac_tcr |= OSI_MAC_TCR_TSIPV6ENA;
		}
		if ((ptp_filter & OSI_MAC_TCR_TSEVENTENA) ==
		    OSI_MAC_TCR_TSEVENTENA) {
			mac_tcr |= OSI_MAC_TCR_TSEVENTENA;
		}
		if ((ptp_filter & OSI_MAC_TCR_TSMASTERENA) ==
		    OSI_MAC_TCR_TSMASTERENA) {
			mac_tcr |= OSI_MAC_TCR_TSMASTERENA;
		}
		if ((ptp_filter & OSI_MAC_TCR_TSVER2ENA) ==
		    OSI_MAC_TCR_TSVER2ENA) {
			mac_tcr |= OSI_MAC_TCR_TSVER2ENA;
		}
		if ((ptp_filter & OSI_MAC_TCR_TSIPENA) ==
		    OSI_MAC_TCR_TSIPENA) {
			mac_tcr |= OSI_MAC_TCR_TSIPENA;
		}
		if ((ptp_filter & OSI_MAC_TCR_AV8021ASMEN) ==
		    OSI_MAC_TCR_AV8021ASMEN) {
			mac_tcr |= OSI_MAC_TCR_AV8021ASMEN;
		}
		if ((ptp_filter & OSI_MAC_TCR_TSENALL) ==
		    OSI_MAC_TCR_TSENALL) {
			mac_tcr |= OSI_MAC_TCR_TSENALL;
		}
		if ((ptp_filter & OSI_MAC_TCR_CSC) ==
				OSI_MAC_TCR_CSC) {
			mac_tcr |= OSI_MAC_TCR_CSC;
		}
	} else {
		/* Disabling the MAC time stamping */
		mac_tcr = OSI_DISABLE;
	}

	osi_writela(osi_core, mac_tcr, (unsigned char *)addr + MGBE_MAC_TCR);
}

/**
 * @brief mgbe_config_ssir - Configure SSIR
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] ptp_clock: PTP required clock frequency
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static void mgbe_config_ssir(struct osi_core_priv_data *const osi_core,
			     const unsigned int ptp_clock)
{
	unsigned long long val;
	unsigned int mac_tcr;
	void *addr = osi_core->base;

	mac_tcr = osi_readla(osi_core, (unsigned char *)addr + MGBE_MAC_TCR);

	/* convert the PTP required clock frequency to nano second.
	 * formula is : ((1/ptp_clock) * 1000000000)
	 * where, ptp_clock = OSI_PTP_REQ_CLK_FREQ if FINE correction
	 * and ptp_clock = PTP reference clock if COARSE correction
	 */
	if ((mac_tcr & MGBE_MAC_TCR_TSCFUPDT) == MGBE_MAC_TCR_TSCFUPDT) {
		if (osi_core->pre_si == OSI_ENABLE) {
			val = OSI_PTP_SSINC_16;
		} else {
			/* For silicon */
			val = OSI_PTP_SSINC_4;
		}
	} else {
		val = ((1U * OSI_NSEC_PER_SEC) / ptp_clock);
	}

	/* 0.465ns accurecy */
	if ((mac_tcr & MGBE_MAC_TCR_TSCTRLSSR) == 0U) {
		if (val < UINT_MAX) {
			val = (val * 1000U) / 465U;
		}
	}

	val |= (val << MGBE_MAC_SSIR_SSINC_SHIFT);

	/* update Sub-second Increment Value */
	if (val < UINT_MAX) {
		osi_writela(osi_core, (unsigned int)val,
			   (unsigned char *)addr + MGBE_MAC_SSIR);
	}
}

/**
 * @brief mgbe_set_mode - Setting the mode.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] mode: mode to be set.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nve32_t mgbe_set_mode(struct osi_core_priv_data *const osi_core,
			     const nve32_t mode)
{
	return 0;
}

/**
 * @brief mgbe_read_reg - Read a register
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nveu32_t mgbe_read_reg(struct osi_core_priv_data *const osi_core,
			     const nve32_t reg)
{
	return osi_readla(osi_core, (nveu8_t *)osi_core->base + reg);
}

/**
 * @brief mgbe_write_reg - Write a reg
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] val:  Value to be written.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nveu32_t mgbe_write_reg(struct osi_core_priv_data *const osi_core,
			       const nveu32_t val,
			       const nve32_t reg)
{
	osi_writela(osi_core, val, (nveu8_t *)osi_core->base + reg);
	return 0;
}

/**
 * @brief mgbe_validate_core_regs - Validates MGBE core registers.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nve32_t mgbe_validate_core_regs(
				struct osi_core_priv_data *const osi_core)
{
	return 0;
}

/**
 * @brief eqos_write_reg - Write a reg
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] val:  Value to be written.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nve32_t mgbe_config_tx_status(struct osi_core_priv_data *const osi_core,
				     const nveu32_t tx_status)
{
	return 0;
}

/**
 * @brief eqos_write_reg - Write a reg
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] val:  Value to be written.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static nve32_t mgbe_config_rx_crc_check(struct osi_core_priv_data *const osi_core,
					const nveu32_t crc_chk)
{
	return 0;
}

/**
 * @brief eqos_write_reg - Write a reg
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] val:  Value to be written.
 * @param[in] reg: Register address.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 * @retval 0
 */
static void mgbe_set_mdc_clk_rate(struct osi_core_priv_data *const osi_core,
				  const nveu64_t csr_clk_rate)
{
}

#ifdef MACSEC_SUPPORT
/**
 * @brief mgbe_config_macsec_ipg - Configure MAC IPG according to macsec IAS
 *
 * @note
 * Algorithm:
 *  - Stop MAC Tx
 *  - Update MAC IPG value to accommodate macsec 32 byte SECTAG.
 *  - Start MAC Tx
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] enable: Enable or Disable MAC Tx engine
 *
 * @pre
 *     1) MAC has to be out of reset.
 *     2) Shall not use this ipg value in half duplex mode
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
void mgbe_config_macsec_ipg(struct osi_core_priv_data *const osi_core,
					    const nveu32_t enable)
{
	nveu32_t value = 0U;

	/* stop MAC Tx */
	mgbe_config_mac_tx(osi_core, OSI_DISABLE);
	if (enable == OSI_ENABLE) {
		/* Configure IPG  {EIPG,IPG} value according to macsec IAS in
		 * MAC_Tx_Configuration and MAC_Extended_Configuration
		 * IPG (12 B[default] + 32 B[sectag]) = 352 bits
		 */
		value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				   MGBE_MAC_TMCR);
		value &= ~MGBE_MAC_TMCR_IPG_MASK;
		value |= MGBE_MAC_TMCR_IFP;
		osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
			    MGBE_MAC_TMCR);
		value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				   MGBE_MAC_EXT_CNF);
		value |= MGBE_MAC_EXT_CNF_EIPG;
		osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
			    MGBE_MAC_EXT_CNF);
	} else {
		/* Update MAC IPG to default value 12B */
		value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				   MGBE_MAC_TMCR);
		value &= ~MGBE_MAC_TMCR_IPG_MASK;
		value &= ~MGBE_MAC_TMCR_IFP;
		osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
			    MGBE_MAC_TMCR);
		value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				   MGBE_MAC_EXT_CNF);
		value &= ~MGBE_MAC_EXT_CNF_EIPG_MASK;
		osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
			    MGBE_MAC_EXT_CNF);
	}
	/* start MAC Tx */
	mgbe_config_mac_tx(osi_core, OSI_ENABLE);
}
#endif /*  MACSEC_SUPPORT */

/**
 * @brief mgbe_init_core_ops - Initialize MGBE MAC core operations
 */
void mgbe_init_core_ops(struct core_ops *ops)
{
	ops->poll_for_swr = mgbe_poll_for_swr;
	ops->core_init = mgbe_core_init;
	ops->core_deinit = mgbe_core_deinit;
	ops->validate_regs = mgbe_validate_core_regs;
	ops->start_mac = mgbe_start_mac;
	ops->stop_mac = mgbe_stop_mac;
	ops->handle_common_intr = mgbe_handle_common_intr;
	/* only MGBE supports full duplex */
	ops->set_mode = mgbe_set_mode;
	/* by default speed is 10G */
	ops->set_speed = mgbe_set_speed;
	ops->pad_calibrate = mgbe_pad_calibrate;
	ops->set_mdc_clk_rate = mgbe_set_mdc_clk_rate;
	ops->flush_mtl_tx_queue = mgbe_flush_mtl_tx_queue;
	ops->config_mac_loopback = mgbe_config_mac_loopback;
	ops->set_avb_algorithm = mgbe_set_avb_algorithm;
	ops->get_avb_algorithm = mgbe_get_avb_algorithm,
	ops->config_fw_err_pkts = mgbe_config_fw_err_pkts;
	ops->config_tx_status = mgbe_config_tx_status;
	ops->config_rx_crc_check = mgbe_config_rx_crc_check;
	ops->config_flow_control = mgbe_config_flow_control;
	ops->config_arp_offload = mgbe_config_arp_offload;
	ops->config_ptp_offload = mgbe_config_ptp_offload;
	ops->config_rxcsum_offload = mgbe_config_rxcsum_offload;
	ops->config_mac_pkt_filter_reg = mgbe_config_mac_pkt_filter_reg;
	ops->update_mac_addr_low_high_reg = mgbe_update_mac_addr_low_high_reg;
	ops->config_l3_l4_filter_enable = mgbe_config_l3_l4_filter_enable;
	ops->config_l3_filters = mgbe_config_l3_filters;
	ops->update_ip4_addr = mgbe_update_ip4_addr;
	ops->update_ip6_addr = mgbe_update_ip6_addr;
	ops->config_l4_filters = mgbe_config_l4_filters;
	ops->update_l4_port_no = mgbe_update_l4_port_no;
	ops->config_vlan_filtering = mgbe_config_vlan_filtering;
	ops->update_vlan_id = mgbe_update_vlan_id;
	ops->set_systime_to_mac = mgbe_set_systime_to_mac;
	ops->config_addend = mgbe_config_addend;
	ops->adjust_mactime = mgbe_adjust_mactime;
	ops->config_tscr = mgbe_config_tscr;
	ops->config_ssir = mgbe_config_ssir,
	ops->config_ptp_rxq = mgbe_config_ptp_rxq;
	ops->write_phy_reg = mgbe_write_phy_reg;
	ops->read_phy_reg = mgbe_read_phy_reg;
	ops->save_registers = mgbe_save_registers;
	ops->restore_registers = mgbe_restore_registers;
	ops->read_mmc = mgbe_read_mmc;
	ops->reset_mmc = mgbe_reset_mmc;
	ops->configure_eee = mgbe_configure_eee;
	ops->get_hw_features = mgbe_get_hw_features;
	ops->config_rss = mgbe_config_rss;
	ops->hw_config_est = mgbe_hw_config_est;
	ops->hw_config_fpe = mgbe_hw_config_fpe;
	ops->config_frp = mgbe_config_frp;
	ops->update_frp_entry = mgbe_update_frp_entry;
	ops->update_frp_nve = mgbe_update_frp_nve;
	ops->ptp_tsc_capture = mgbe_ptp_tsc_capture;
	ops->write_reg = mgbe_write_reg;
	ops->read_reg = mgbe_read_reg;
#ifdef MACSEC_SUPPORT
	ops->config_macsec_ipg = mgbe_config_macsec_ipg;
#endif /*  MACSEC_SUPPORT */
};
