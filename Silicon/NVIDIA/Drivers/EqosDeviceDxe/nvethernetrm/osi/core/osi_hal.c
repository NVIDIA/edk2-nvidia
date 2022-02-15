/*
 * Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <local_common.h>
#include <ivc_core.h>
#include "core_local.h"
#include "../osi/common/common.h"
#include "vlan_filter.h"
#include "frp.h"
#ifdef OSI_DEBUG
#include "debug.h"
#endif /* OSI_DEBUG */

/**
 * @brief g_ops - Static core operations array.
 */
static struct core_ops g_ops[MAX_MAC_IP_TYPES];

/**
 * @brief Function to validate input arguments of API.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] l_core: OSI local core data structure.
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
static inline nve32_t validate_args(struct osi_core_priv_data *const osi_core,
				    struct core_local *l_core)
{
	if ((osi_core == OSI_NULL) || (osi_core->base == OSI_NULL) ||
	    (l_core->init_done == OSI_DISABLE) ||
	    (l_core->magic_num != (nveu64_t)osi_core)) {
		return -1;
	}

	return 0;
}

/**
 * @brief Function to validate function pointers.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] ops_p: OSI Core operations structure.
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
static nve32_t validate_func_ptrs(struct osi_core_priv_data *const osi_core,
				  struct core_ops *ops_p)
{
	nveu32_t i = 0;
	void *temp_ops = (void *)ops_p;
#if __SIZEOF_POINTER__ == 8
	nveu64_t *l_ops = (nveu64_t *)temp_ops;
#elif __SIZEOF_POINTER__ == 4
	nveu32_t *l_ops = (nveu32_t *)temp_ops;
#else
	OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
		     "Undefined architecture\n", 0ULL);
	return -1;
#endif

	for (i = 0; i < (sizeof(*ops_p) / (nveu64_t)__SIZEOF_POINTER__); i++) {
		if (*l_ops == 0U) {
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				     "core: fn ptr validation failed at\n",
				     (nveu64_t)i);
			return -1;
		}

		l_ops++;
	}

	return 0;
}

nve32_t osi_hal_write_phy_reg(struct osi_core_priv_data *const osi_core,
			      const nveu32_t phyaddr, const nveu32_t phyreg,
			      const nveu16_t phydata)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	return l_core->ops_p->write_phy_reg(osi_core, phyaddr, phyreg, phydata);
}

nve32_t osi_hal_read_phy_reg(struct osi_core_priv_data *const osi_core,
			     const nveu32_t phyaddr, const nveu32_t phyreg)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	return l_core->ops_p->read_phy_reg(osi_core, phyaddr, phyreg);
}

static nve32_t osi_hal_init_core_ops(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	typedef void (*init_ops_arr)(struct core_ops *);
	typedef void *(*safety_init)(void);

	init_ops_arr i_ops[MAX_MAC_IP_TYPES][MAX_MAC_IP_TYPES] = {
		{ eqos_init_core_ops, OSI_NULL },
		{ mgbe_init_core_ops, OSI_NULL }
	};

	safety_init s_init[MAX_MAC_IP_TYPES][MAX_MAC_IP_TYPES] = {
		{ eqos_get_core_safety_config, ivc_get_core_safety_config },
		{ OSI_NULL, OSI_NULL }
	};

	if (osi_core == OSI_NULL) {
		return -1;
	}

	if ((l_core->magic_num != (nveu64_t)osi_core) ||
	    (l_core->init_done == OSI_ENABLE)) {
		return -1;
	}

	if ((osi_core->osd_ops.ops_log == OSI_NULL) ||
	    (osi_core->osd_ops.udelay == OSI_NULL) ||
	    (osi_core->osd_ops.msleep == OSI_NULL) ||
#ifdef OSI_DEBUG
	    (osi_core->osd_ops.printf == OSI_NULL) ||
#endif /* OSI_DEBUG */
	    (osi_core->osd_ops.usleep_range == OSI_NULL)) {
		return -1;
	}

	if (osi_core->mac > OSI_MAC_HW_MGBE) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "Invalid MAC HW type\n", 0ULL);
		return -1;
	}

	if (osi_core->use_virtualization > OSI_ENABLE) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "Invalid use_virtualization value\n", 0ULL);
		return -1;
	}

	if (i_ops[osi_core->mac][osi_core->use_virtualization] != OSI_NULL) {
		i_ops[osi_core->mac][osi_core->use_virtualization](&g_ops[osi_core->mac]);
	}

	if (s_init[osi_core->mac][osi_core->use_virtualization] != OSI_NULL) {
		osi_core->safety_config =
			s_init[osi_core->mac][osi_core->use_virtualization]();
	}

	if (validate_func_ptrs(osi_core, &g_ops[osi_core->mac]) < 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "core: function ptrs validation failed\n", 0ULL);
		return -1;
	}

	l_core->ops_p = &g_ops[osi_core->mac];
	l_core->init_done = OSI_ENABLE;

	return 0;
}

nve32_t osi_poll_for_mac_reset_complete(
				      struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	return l_core->ops_p->poll_for_swr(osi_core);
}

/**
 * @brief init_vlan_filters - Helper function to init all VLAN SW information.
 *
 * Algorithm: Initialize VLAN filtering information.
 *
 * @param[in] osi_core: OSI Core private data structure.
 */
static inline void init_vlan_filters(struct osi_core_priv_data *const osi_core)
{
	unsigned int i = 0U;

	for (i = 0; i < VLAN_NUM_VID; i++) {
		osi_core->vid[i] = VLAN_ID_INVALID;
	}

	osi_core->vf_bitmap = 0U;
	osi_core->vlan_filter_cnt = 0U;
}

nve32_t osi_hal_hw_core_init(struct osi_core_priv_data *const osi_core,
			     nveu32_t tx_fifo_size, nveu32_t rx_fifo_size)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	init_vlan_filters(osi_core);

	/* Init FRP */
	init_frp(osi_core);

	return l_core->ops_p->core_init(osi_core, tx_fifo_size, rx_fifo_size);
}

nve32_t osi_hal_hw_core_deinit(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	l_core->ops_p->core_deinit(osi_core);

	/* FIXME: Should be fixed */
	//l_core->init_done = OSI_DISABLE;
	//l_core->magic_num = 0;

	return 0;
}

nve32_t osi_start_mac(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	l_core->ops_p->start_mac(osi_core);

	return 0;
}

nve32_t osi_stop_mac(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	l_core->ops_p->stop_mac(osi_core);

	return 0;
}

nve32_t osi_common_isr(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	l_core->ops_p->handle_common_intr(osi_core);

	return 0;
}

nve32_t osi_set_mode(struct osi_core_priv_data *const osi_core,
		     const nve32_t mode)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	return l_core->ops_p->set_mode(osi_core, mode);
}

nve32_t osi_set_speed(struct osi_core_priv_data *const osi_core,
		      const nve32_t speed)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	return l_core->ops_p->set_speed(osi_core, speed);
}

nve32_t osi_pad_calibrate(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	return l_core->ops_p->pad_calibrate(osi_core);
}

nve32_t osi_config_fw_err_pkts(struct osi_core_priv_data *const osi_core,
			       const nveu32_t qinx, const nveu32_t fw_err)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	/* Configure Forwarding of Error packets */
	return l_core->ops_p->config_fw_err_pkts(osi_core, qinx, fw_err);
}

static nve32_t conf_ptp_offload(struct osi_core_priv_data *const osi_core,
				struct osi_pto_config *const pto_config)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	int ret = -1;

	/* Validate input arguments */
	if (pto_config == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "pto_config is NULL\n", 0ULL);
		return ret;
	}

	if (pto_config->mc_uc != OSI_ENABLE &&
	    pto_config->mc_uc != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid mc_uc flag value\n",
			     (nveul64_t)pto_config->mc_uc);
		return ret;
	}

	if (pto_config->en_dis != OSI_ENABLE &&
	    pto_config->en_dis != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid enable flag value\n",
			     (nveul64_t)pto_config->en_dis);
		return ret;
	}

	if (pto_config->snap_type != OSI_PTP_SNAP_ORDINARY &&
	    pto_config->snap_type != OSI_PTP_SNAP_TRANSPORT &&
	    pto_config->snap_type != OSI_PTP_SNAP_P2P) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid SNAP type value\n",
			     (nveul64_t)pto_config->snap_type);
		return ret;
	}

	if (pto_config->master != OSI_ENABLE &&
	    pto_config->master != OSI_DISABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid master flag value\n",
			     (nveul64_t)pto_config->master);
		return ret;
	}

	if (pto_config->domain_num >= OSI_PTP_MAX_DOMAIN) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid ptp domain\n",
			     (nveul64_t)pto_config->domain_num);
		return ret;
	}

	if (pto_config->portid >= OSI_PTP_MAX_PORTID) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid ptp port ID\n",
			     (nveul64_t)pto_config->portid);
		return ret;
	}

	ret = l_core->ops_p->config_ptp_offload(osi_core, pto_config);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Fail to configure PTO\n",
			     (nveul64_t)pto_config->en_dis);
		return ret;
	}

	/* Configure PTP */
	ret = osi_ptp_configuration(osi_core, pto_config->en_dis);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Fail to configure PTP\n",
			     (nveul64_t)pto_config->en_dis);
		return ret;
	}

	return ret;
}

nve32_t osi_l2_filter(struct osi_core_priv_data *const osi_core,
		      const struct osi_filter *filter)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	nve32_t ret;

	if ((validate_args(osi_core, l_core) < 0) || (filter == OSI_NULL)) {
		return -1;
	}

	if (filter == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "CORE: filter is NULL\n", 0ULL);
		return -1;
	}

	ret = l_core->ops_p->config_mac_pkt_filter_reg(osi_core, filter);
	if (ret < 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
			     "failed to configure MAC packet filter register\n",
			     0ULL);
		return ret;
	}

	if (((filter->oper_mode & OSI_OPER_ADDR_UPDATE) != OSI_NONE) ||
	    ((filter->oper_mode & OSI_OPER_ADDR_DEL) != OSI_NONE)) {
		ret = -1;

		if ((filter->dma_routing == OSI_ENABLE) &&
		    (osi_core->dcs_en != OSI_ENABLE)) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "DCS requested. Conflicts with DT config\n",
				     0ULL);
			return ret;
		}

		ret = l_core->ops_p->update_mac_addr_low_high_reg(osi_core,
								  filter);
	}

	return ret;
}

/**
 * @brief helper_l4_filter helper function for l4 filtering
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] l_filter: filter structure
 * @param[in] type: filter type l3 or l4
 * @param[in] dma_routing_enable: dma routing enable (1) or disable (0)
 * @param[in] dma_chan: dma channel
 *
 * @pre MAC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t helper_l4_filter(
				   struct osi_core_priv_data *const osi_core,
				   struct core_ops *ops_p,
				   struct osi_l3_l4_filter l_filter,
				   nveu32_t type,
				   nveu32_t dma_routing_enable,
				   nveu32_t dma_chan)
{
	nve32_t ret = 0;

	ret = ops_p->config_l4_filters(osi_core,
				    l_filter.filter_no,
				    l_filter.filter_enb_dis,
				    type,
				    l_filter.src_dst_addr_match,
				    l_filter.perfect_inverse_match,
				    dma_routing_enable,
				    dma_chan);
	if (ret < 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
			     "failed to configure L4 filters\n", 0ULL);
		return ret;
	}

	return ops_p->update_l4_port_no(osi_core,
				     l_filter.filter_no,
				     l_filter.port_no,
				     l_filter.src_dst_addr_match);
}

/**
 * @brief helper_l3_filter helper function for l3 filtering
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] l_filter: filter structure
 * @param[in] type: filter type l3 or l4
 * @param[in] dma_routing_enable: dma routing enable (1) or disable (0)
 * @param[in] dma_chan: dma channel
 *
 * @pre MAC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t helper_l3_filter(
				   struct osi_core_priv_data *const osi_core,
				   struct core_ops *ops_p,
				   struct osi_l3_l4_filter l_filter,
				   nveu32_t type,
				   nveu32_t dma_routing_enable,
				   nveu32_t dma_chan)
{
	nve32_t ret = 0;

	ret = ops_p->config_l3_filters(osi_core,
				    l_filter.filter_no,
				    l_filter.filter_enb_dis,
				    type,
				    l_filter.src_dst_addr_match,
				    l_filter.perfect_inverse_match,
				    dma_routing_enable,
				    dma_chan);
	if (ret < 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
			     "failed to configure L3 filters\n", 0ULL);
		return ret;
	}

	if (type == OSI_IP6_FILTER) {
		ret = ops_p->update_ip6_addr(osi_core, l_filter.filter_no,
					  l_filter.ip6_addr);
	} else if (type == OSI_IP4_FILTER) {
		ret = ops_p->update_ip4_addr(osi_core, l_filter.filter_no,
					  l_filter.ip4_addr,
					  l_filter.src_dst_addr_match);
	} else {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "Invalid L3 filter type\n", 0ULL);
		return -1;
	}

	return ret;
}

nve32_t osi_l3l4_filter(struct osi_core_priv_data *const osi_core,
			const struct osi_l3_l4_filter l_filter,
			const nveu32_t type, const nveu32_t dma_routing_enable,
			const nveu32_t dma_chan, const nveu32_t is_l4_filter)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	nve32_t ret = -1;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	if ((dma_routing_enable == OSI_ENABLE) &&
	    (osi_core->dcs_en != OSI_ENABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "dma routing enabled but dcs disabled in DT\n",
			     0ULL);
		return ret;
	}

	if (is_l4_filter == OSI_ENABLE) {
		ret = helper_l4_filter(osi_core, l_core->ops_p, l_filter, type,
				       dma_routing_enable, dma_chan);
	} else {
		ret = helper_l3_filter(osi_core, l_core->ops_p, l_filter, type,
				       dma_routing_enable, dma_chan);
	}

	if (ret < 0) {
		OSI_CORE_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
			      "L3/L4 helper function failed\n", 0ULL);
		return ret;
	}

	if (osi_core->l3l4_filter_bitmask != OSI_DISABLE) {
		ret = l_core->ops_p->config_l3_l4_filter_enable(osi_core,
								OSI_ENABLE);
	} else {
		ret = l_core->ops_p->config_l3_l4_filter_enable(osi_core,
								OSI_DISABLE);
	}

	return ret;
}

nve32_t osi_config_rxcsum_offload(struct osi_core_priv_data *const osi_core,
				  const nveu32_t enable)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	return l_core->ops_p->config_rxcsum_offload(osi_core, enable);
}

nve32_t osi_set_systime_to_mac(struct osi_core_priv_data *const osi_core,
			       const nveu32_t sec, const nveu32_t nsec)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	return l_core->ops_p->set_systime_to_mac(osi_core, sec, nsec);
}

/**
 * @brief div_u64 - Calls a function which returns quotient
 *
 * @param[in] dividend: Dividend
 * @param[in] divisor: Divisor
 *
 * @pre MAC IP should be out of reset and need to be initialized as the
 *      requirements.
 *
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 * @returns Quotient
 */
static inline nveu64_t div_u64(nveu64_t dividend,
			       nveu64_t divisor)
{
	nveu64_t remain;

	return div_u64_rem(dividend, divisor, &remain);
}

nve32_t osi_adjust_freq(struct osi_core_priv_data *const osi_core, nve32_t ppb)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	nveu64_t adj;
	nveu64_t temp;
	nveu32_t diff = 0;
	nveu32_t addend;
	nveu32_t neg_adj = 0;
	nve32_t ret = -1;
	nve32_t ppb1 = ppb;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	addend = osi_core->default_addend;
	if (ppb1 < 0) {
		neg_adj = 1U;
		ppb1 = -ppb1;
		adj = (nveu64_t)addend * (nveu32_t)ppb1;
	} else {
		adj = (nveu64_t)addend * (nveu32_t)ppb1;
	}

	/*
	 * div_u64 will divide the "adj" by "1000000000ULL"
	 * and return the quotient.
	 */
	temp = div_u64(adj, OSI_NSEC_PER_SEC);
	if (temp < UINT_MAX) {
		diff = (nveu32_t)temp;
	} else {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID, "temp > UINT_MAX\n",
			     0ULL);
		return ret;
	}

	if (neg_adj == 0U) {
		if (addend <= (UINT_MAX - diff)) {
			addend = (addend + diff);
		} else {
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				     "addend > UINT_MAX\n", 0ULL);
			return ret;
		}
	} else {
		if (addend > diff) {
			addend = addend - diff;
		} else if (addend < diff) {
			addend = diff - addend;
		} else {
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				     "addend = diff\n", 0ULL);
		}
	}

	return l_core->ops_p->config_addend(osi_core, addend);
}

nve32_t osi_adjust_time(struct osi_core_priv_data *const osi_core,
			nvel64_t nsec_delta)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	nveu32_t neg_adj = 0;
	nveu32_t sec = 0, nsec = 0;
	nveu64_t quotient;
	nveu64_t reminder = 0;
	nveu64_t udelta = 0;
	nve32_t ret = -1;
	nvel64_t nsec_delta1 = nsec_delta;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	if (nsec_delta1 < 0) {
		neg_adj = 1;
		nsec_delta1 = -nsec_delta1;
		udelta = (nveul64_t)nsec_delta1;
	} else {
		udelta = (nveul64_t)nsec_delta1;
	}

	quotient = div_u64_rem(udelta, OSI_NSEC_PER_SEC, &reminder);
	if (quotient <= UINT_MAX) {
		sec = (nveu32_t)quotient;
	} else {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "quotient > UINT_MAX\n", 0ULL);
		return ret;
	}

	if (reminder <= UINT_MAX) {
		nsec = (nveu32_t)reminder;
	} else {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "reminder > UINT_MAX\n", 0ULL);
		return ret;
	}

	return l_core->ops_p->adjust_mactime(osi_core, sec, nsec, neg_adj,
					     osi_core->ptp_config.one_nsec_accuracy);
}

nve32_t osi_ptp_configuration(struct osi_core_priv_data *const osi_core,
			      const nveu32_t enable)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	nve32_t ret = 0;
	nveu64_t temp = 0, temp1 = 0, temp2 = 0;
	nveu64_t ssinc = 0;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	if (enable == OSI_DISABLE) {
		/* disable hw time stamping */
		/* Program MAC_Timestamp_Control Register */
		l_core->ops_p->config_tscr(osi_core, OSI_DISABLE);
		/* Disable PTP RX Queue routing */
		ret = l_core->ops_p->config_ptp_rxq(osi_core,
					    osi_core->ptp_config.ptp_rx_queue,
					    OSI_DISABLE);
	} else {
		/* Program MAC_Timestamp_Control Register */
		l_core->ops_p->config_tscr(osi_core,
					   osi_core->ptp_config.ptp_filter);

		if (osi_core->pre_si == OSI_ENABLE) {
			if (osi_core->mac == OSI_MAC_HW_MGBE) {
				/* FIXME: Pass it from OSD */
				osi_core->ptp_config.ptp_clock = 78125000U;
				osi_core->ptp_config.ptp_ref_clk_rate =
								 78125000U;
			} else {
				/* FIXME: Pass it from OSD */
				osi_core->ptp_config.ptp_clock = 312500000U;
				osi_core->ptp_config.ptp_ref_clk_rate =
								 312500000U;
			}
		}
		/* Program Sub Second Increment Register */
		l_core->ops_p->config_ssir(osi_core,
					   osi_core->ptp_config.ptp_clock);

		/* formula for calculating addend value is
		 * TSAR = (2^32 * 1000) / (ptp_ref_clk_rate in MHz * SSINC)
		 * 2^x * y == (y << x), hence
		 * 2^32 * 1000 == (1000 << 32)
		 * so addend = (2^32 * 1000)/(ptp_ref_clk_rate in MHZ * SSINC);
		 */
		if ((osi_core->pre_si == OSI_ENABLE) &&
		    ((osi_core->mac == OSI_MAC_HW_MGBE) ||
		    (osi_core->mac_ver <= OSI_EQOS_MAC_4_10))) {
			ssinc = OSI_PTP_SSINC_16;
		} else {
			ssinc = OSI_PTP_SSINC_4;
			if (osi_core->mac_ver == OSI_EQOS_MAC_5_30) {
				ssinc = OSI_PTP_SSINC_6;
			}
		}

		temp = ((nveu64_t)1000 << 32);
		temp = (nveu64_t)temp * 1000000U;

		temp1 = div_u64(temp,
			(nveu64_t)osi_core->ptp_config.ptp_ref_clk_rate);

		temp2 = div_u64(temp1, (nveu64_t)ssinc);

		if (temp2 < UINT_MAX) {
			osi_core->default_addend = (nveu32_t)temp2;
		} else {
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				     "core: temp2 >= UINT_MAX\n", 0ULL);
			return -1;
		}

		/* Program addend value */
		ret = l_core->ops_p->config_addend(osi_core,
						   osi_core->default_addend);

		/* Set current time */
		if (ret == 0) {
			ret = l_core->ops_p->set_systime_to_mac(osi_core,
						     osi_core->ptp_config.sec,
						     osi_core->ptp_config.nsec);
			if (ret == 0) {
				/* Enable PTP RX Queue routing */
				ret = l_core->ops_p->config_ptp_rxq(osi_core,
					osi_core->ptp_config.ptp_rx_queue,
					OSI_ENABLE);
			}
		}
	}

	return ret;
}

/**
 * @brief rxq_route_config - Enable PTP RX packets routing
 *
 * Algorithm: Program PTP RX queue index
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] rx_route: Pointer to the osi_rxq_route structure, which
 * contains RXQ routing parameters.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t rxq_route_config(struct osi_core_priv_data *const osi_core,
				const struct osi_rxq_route *rxq_route)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (rxq_route->route_type != OSI_RXQ_ROUTE_PTP) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid route_type\n",
			     rxq_route->route_type);
		return -1;
	}

	return l_core->ops_p->config_ptp_rxq(osi_core,
				     rxq_route->idx,
				     rxq_route->enable);
}

nve32_t osi_read_mmc(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	l_core->ops_p->read_mmc(osi_core);

	return 0;
}

nve32_t osi_get_mac_version(struct osi_core_priv_data *const osi_core,
			    nveu32_t *mac_ver)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	if (mac_ver == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "mac_ver is NULL\n", 0ULL);
		return -1;
	}

	*mac_ver = ((l_core->ops_p->read_reg(osi_core, (nve32_t)MAC_VERSION)) &
		    MAC_VERSION_SNVER_MASK);

	if (validate_mac_ver_update_chans(*mac_ver, &l_core->max_chans) == 0) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "Invalid MAC version\n", (nveu64_t)*mac_ver)
		return -1;
	}

	return 0;
}

#ifndef OSI_STRIPPED_LIB
/**
 * @brief validate_core_regs - Read-validate HW registers for func safety.
 *
 * @note
 * Algorithm:
 *  - Reads pre-configured list of MAC/MTL configuration registers
 *    and compares with last written value for any modifications.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre
 *  - MAC has to be out of reset.
 *  - osi_hal_hw_core_init has to be called. Internally this would initialize
 *    the safety_config (see osi_core_priv_data) based on MAC version and
 *    which specific registers needs to be validated periodically.
 *  - Invoke this call if (osi_core_priv_data->safety_config != OSI_NULL)
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
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
static nve32_t validate_core_regs(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (osi_core->safety_config == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "CORE: Safety config is NULL\n", 0ULL);
		return -1;
	}

	return l_core->ops_p->validate_regs(osi_core);
}

/**
 * @brief vlan_id_update - invoke osi call to update VLAN ID
 *
 * @note
 * Algorithm:
 *  - return 16 bit VLAN ID
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] vid: VLAN ID
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t vlan_id_update(struct osi_core_priv_data *const osi_core,
			      const nveu32_t vid)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	unsigned int action = vid & VLAN_ACTION_MASK;
	unsigned short vlan_id = vid & VLAN_VID_MASK;

	if ((osi_core->mac_ver == OSI_EQOS_MAC_4_10) ||
	    (osi_core->mac_ver == OSI_EQOS_MAC_5_00)) {
		/* No VLAN ID filtering */
		return 0;
	}

	if (((action != OSI_VLAN_ACTION_ADD) &&
	    (action != OSI_VLAN_ACTION_DEL)) ||
	    (vlan_id >= VLAN_NUM_VID)) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "CORE: Invalid action/vlan_id\n", 0ULL);
		/* Unsupported action */
		return -1;
	}

	return update_vlan_id(osi_core, l_core->ops_p, vid);
}

/**
 * @brief conf_eee - Configure EEE LPI in MAC.
 *
 * @note
 * Algorithm:
 *  - This routine invokes configuration of EEE LPI in the MAC.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] tx_lpi_enabled: Enable (1)/disable (0) tx lpi
 * @param[in] tx_lpi_timer: Tx LPI entry timer in usecs upto
 *            OSI_MAX_TX_LPI_TIMER (in steps of 8usec)
 *
 * @pre
 *  - MAC and PHY should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t conf_eee(struct osi_core_priv_data *const osi_core,
			nveu32_t tx_lpi_enabled, nveu32_t tx_lpi_timer)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if ((tx_lpi_timer >= OSI_MAX_TX_LPI_TIMER) ||
	    (tx_lpi_timer <= OSI_MIN_TX_LPI_TIMER) ||
	    ((tx_lpi_timer % OSI_MIN_TX_LPI_TIMER) != OSI_NONE)) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "Invalid Tx LPI timer value\n",
			     (nveul64_t)tx_lpi_timer);
		return -1;
	}

	l_core->ops_p->configure_eee(osi_core, tx_lpi_enabled, tx_lpi_timer);

	return 0;
}

/**
 * @brief configure_frp - Configure the FRP offload entry in the
 * Instruction Table.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] cmd: FRP command data structure.
 *
 * @pre
 *  - MAC and PHY should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
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
static int configure_frp(struct osi_core_priv_data *const osi_core,
			 struct osi_core_frp_cmd *const cmd)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (cmd == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			"Invalid argment\n", OSI_NONE);
		return -1;
	}

	/* Check for supported MAC version */
	if ((osi_core->mac == OSI_MAC_HW_EQOS) &&
	    (osi_core->mac_ver < OSI_EQOS_MAC_5_10)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "MAC doesn't support FRP\n", OSI_NONE);
		return -1;
	}

	return setup_frp(osi_core, l_core->ops_p, cmd);
}

/**
 * @brief config_arp_offload - Configure ARP offload in MAC.
 *
 * @note
 * Algorithm:
 *  - Invokes EQOS config ARP offload routine.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] flags: Enable/disable flag.
 * @param[in] ip_addr: Char array representation of IP address
 *
 * @pre
 *  - MAC should be init and started. see osi_start_mac()
 *  - Valid 4 byte IP address as argument ip_addr
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
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
static nve32_t conf_arp_offload(struct osi_core_priv_data *const osi_core,
				const nveu32_t flags,
				const nveu8_t *ip_addr)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (ip_addr == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "CORE: ip_addr is NULL\n", 0ULL);
		return -1;
	}

	if ((flags != OSI_ENABLE) && (flags != OSI_DISABLE)) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "Invalid ARP offload enable/disable flag\n", 0ULL);
		return -1;
	}

	return l_core->ops_p->config_arp_offload(osi_core, flags, ip_addr);
}

/**
 * @brief conf_mac_loopback - Configure MAC loopback
 *
 * @note
 * Algorithm:
 *  - Configure the MAC to support the loopback.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] lb_mode: Enable or disable MAC loopback
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
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
static nve32_t conf_mac_loopback(struct osi_core_priv_data *const osi_core,
				 const nveu32_t lb_mode)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	/* don't allow only if loopback mode is other than 0 or 1 */
	if (lb_mode != OSI_ENABLE && lb_mode != OSI_DISABLE) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "Invalid loopback mode\n", 0ULL);
		return -1;
	}

	return l_core->ops_p->config_mac_loopback(osi_core, lb_mode);
}
#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief config_est - Read Setting for GCL from input and update
 * registers.
 *
 * Algorithm:
 * - Write  TER, LLR and EST control register
 * - Update GCL to sw own GCL (MTL_EST_Status bit SWOL will tell which is
 *   owned by SW) and store which GCL is in use currently in sw.
 * - TODO set DBGB and DBGM for debugging
 * - EST_data and GCRR to 1, update entry sno in ADDR and put data at
 *   est_gcl_data enable GCL MTL_EST_SSWL and wait for self clear or use
 *   SWLC in MTL_EST_Status. Please note new GCL will be pushed for each entry.
 * - Configure btr. Update btr based on current time (current time
 *   should be updated based on PTP by this time)
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est: Configuration osi_est_config structure.
 *
 * @pre
 * - MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
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
static nve32_t config_est(struct osi_core_priv_data *osi_core,
			  struct osi_est_config *est)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (est == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "EST data is NULL", 0ULL);
		return -1;
	}

	if ((osi_core->flow_ctrl & OSI_FLOW_CTRL_TX) ==
	     OSI_FLOW_CTRL_TX) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "TX Flow control enabled, please disable it",
			      0ULL);
		return -1;
	}

	return l_core->ops_p->hw_config_est(osi_core, est);
}

/**
 * @brief config_fep - Read Setting for preemption and express for TC
 * and update registers.
 *
 * Algorithm:
 * - Check for TC enable and TC has masked for setting to preemptable.
 * - Update FPE control status register
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] fpe: Configuration osi_fpe_config structure.
 *
 * @pre
 * - MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
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
static nve32_t config_fpe(struct osi_core_priv_data *osi_core,
			  struct osi_fpe_config *fpe)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (fpe == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "FPE data is NULL", 0ULL);
		return -1;
	}

	return l_core->ops_p->hw_config_fpe(osi_core, fpe);
}

/**
 * @brief Free stale timestamps for channel
 *
 * Algorithm:
 * - Check for if any timestamp for input channel id
 * - if yes, reset node to 0x0 for reuse.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] chan: 1 for DMA channel 0, 2 for dma channel 1,...
 *		    0 is used for onestep.
 */
static inline void free_tx_ts(struct osi_core_priv_data *osi_core,
			   nveu32_t chan)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	struct osi_core_tx_ts *head = &l_core->tx_ts_head;
	struct osi_core_tx_ts *temp = l_core->tx_ts_head.next;
	nveu32_t count = 0U;

	while ((temp != head) && (count < MAX_TX_TS_CNT)) {
		if (((temp->pkt_id >> CHAN_START_POSITION) & chan) == chan) {
			temp->next->prev = temp->prev;
			temp->prev->next = temp->next;
			/* reset in_use for temp node from the link */
			temp->in_use = OSI_DISABLE;
		}
		count++;
		temp = temp->next;
	}
}

/**
 * @brief Parses internal ts structure array and update time stamp if packet
 *   id matches.
 * Algorithm:
 * - Check for if any timestamp with packet ID in list and valid
 * - update sec and nsec in timestamp structure
 * - reset node to reuse for next call
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in,out] ts: osi core ts structure, pkt_id is input and time is output.
 *
 * @retval 0 on success
 * @retval -1 any other failure.
 */
static inline nve32_t get_tx_ts(struct osi_core_priv_data *osi_core,
				struct osi_core_tx_ts *ts)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	struct osi_core_tx_ts *temp = l_core->tx_ts_head.next;
	struct osi_core_tx_ts *head = &l_core->tx_ts_head;
	nve32_t ret = -1;
	nveu32_t count = 0U;

	if (__sync_fetch_and_add(&l_core->ts_lock, 1) == 1U) {
		/* mask return as initial value is returned always */
		(void)__sync_fetch_and_sub(&l_core->ts_lock, 1);
		osi_core->xstats.ts_lock_del_fail =
				osi_update_stats_counter(
				osi_core->xstats.ts_lock_del_fail, 1U);
		goto done;
	}

	while ((temp != head) && (count < MAX_TX_TS_CNT)) {
		if ((temp->pkt_id == ts->pkt_id) &&
		    (temp->in_use != OSI_NONE)) {
			ts->sec = temp->sec;
			ts->nsec = temp->nsec;
			/* remove temp node from the link */
			temp->next->prev = temp->prev;
			temp->prev->next =  temp->next;
			/* Clear in_use fields */
			temp->in_use = OSI_DISABLE;
			ret = 0;
			break;
		}
		count++;
		temp = temp->next;
	}

	/* mask return as initial value is returned always */
	(void)__sync_fetch_and_sub(&l_core->ts_lock, 1);
done:
	return ret;
}

nve32_t osi_hal_handle_ioctl(struct osi_core_priv_data *osi_core,
			     struct osi_ioctl *data)
{
	struct core_local *l_core = (struct core_local *)osi_core;
	struct core_ops *ops_p;
	nve32_t ret = -1;

	if (validate_args(osi_core, l_core) < 0) {
		return ret;
	}

	ops_p = l_core->ops_p;

	if (data == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "CORE: Invalid argument\n", 0ULL);
		return -1;
	}

	switch (data->cmd) {
#ifndef OSI_STRIPPED_LIB
	case OSI_CMD_RESTORE_REGISTER:
		ret = ops_p->restore_registers(osi_core);
		break;

	case OSI_CMD_L3L4_FILTER:
		ret = osi_l3l4_filter(osi_core, data->l3l4_filter,
				      data->arg1_u32, data->arg2_u32,
				      data->arg3_u32, data->arg4_u32);
		break;

	case OSI_CMD_MDC_CONFIG:
		ops_p->set_mdc_clk_rate(osi_core, data->arg5_u64);
		ret = 0;
		break;

	case OSI_CMD_VALIDATE_CORE_REG:
		ret = validate_core_regs(osi_core);
		break;

	case OSI_CMD_RESET_MMC:
		ops_p->reset_mmc(osi_core);
		ret = 0;
		break;

	case OSI_CMD_SAVE_REGISTER:
		ret = ops_p->save_registers(osi_core);
		break;

	case OSI_CMD_MAC_LB:
		ret = conf_mac_loopback(osi_core, data->arg1_u32);
		break;

	case OSI_CMD_FLOW_CTRL:
		ret = ops_p->config_flow_control(osi_core, data->arg1_u32);
		break;

	case OSI_CMD_GET_AVB:
		ret = ops_p->get_avb_algorithm(osi_core, &data->avb);
		break;

	case OSI_CMD_SET_AVB:
		ret = ops_p->set_avb_algorithm(osi_core, &data->avb);
		break;

	case OSI_CMD_CONFIG_RX_CRC_CHECK:
		ret = ops_p->config_rx_crc_check(osi_core, data->arg1_u32);
		break;

	case OSI_CMD_UPDATE_VLAN_ID:
		ret = vlan_id_update(osi_core, data->arg1_u32);
		break;

	case OSI_CMD_CONFIG_TXSTATUS:
		ret = ops_p->config_tx_status(osi_core, data->arg1_u32);
		break;

	case OSI_CMD_CONFIG_FW_ERR:
		ret = ops_p->config_fw_err_pkts(osi_core, data->arg1_u32,
						data->arg2_u32);
		break;

	case OSI_CMD_ARP_OFFLOAD:
		ret = conf_arp_offload(osi_core, data->arg1_u32,
				       data->arg7_u8_p);
		break;

	case OSI_CMD_VLAN_FILTER:
		ret = ops_p->config_vlan_filtering(osi_core,
				data->vlan_filter.filter_enb_dis,
				data->vlan_filter.perfect_hash,
				data->vlan_filter.perfect_inverse_match);
		break;

	case OSI_CMD_CONFIG_EEE:
		ret = conf_eee(osi_core, data->arg1_u32, data->arg2_u32);
		break;

#endif /* !OSI_STRIPPED_LIB */
	case OSI_CMD_POLL_FOR_MAC_RST:
		ret = ops_p->poll_for_swr(osi_core);
		break;

	case OSI_CMD_START_MAC:
		ops_p->start_mac(osi_core);
		ret = 0;
		break;

	case OSI_CMD_STOP_MAC:
		ops_p->stop_mac(osi_core);
		ret = 0;
		break;

	case OSI_CMD_COMMON_ISR:
		ops_p->handle_common_intr(osi_core);
		ret = 0;
		break;

	case OSI_CMD_PAD_CALIBRATION:
		ret = ops_p->pad_calibrate(osi_core);
		break;

	case OSI_CMD_READ_MMC:
		ops_p->read_mmc(osi_core);
		ret = 0;
		break;

	case OSI_CMD_GET_MAC_VER:
		ret = osi_get_mac_version(osi_core, &data->arg1_u32);
		break;

	case OSI_CMD_SET_MODE:
		ret = ops_p->set_mode(osi_core, data->arg6_32);
		break;

	case OSI_CMD_SET_SPEED:
		ret = ops_p->set_speed(osi_core, data->arg6_32);
		break;

	case OSI_CMD_L2_FILTER:
		ret = osi_l2_filter(osi_core, &data->l2_filter);
		break;

	case OSI_CMD_RXCSUM_OFFLOAD:
		ret = ops_p->config_rxcsum_offload(osi_core, data->arg1_u32);
		break;

	case OSI_CMD_ADJ_FREQ:
		ret = osi_adjust_freq(osi_core, data->arg6_32);
		break;

	case OSI_CMD_ADJ_TIME:
		ret = osi_adjust_time(osi_core, data->arg8_64);
		break;

	case OSI_CMD_CONFIG_PTP:
		ret = osi_ptp_configuration(osi_core, data->arg1_u32);
		break;

	case OSI_CMD_GET_HW_FEAT:
		ret = ops_p->get_hw_features(osi_core, &data->hw_feat);
		break;

	case OSI_CMD_SET_SYSTOHW_TIME:
		ret = ops_p->set_systime_to_mac(osi_core, data->arg1_u32,
						data->arg2_u32);
		break;
	case OSI_CMD_CONFIG_PTP_OFFLOAD:
		ret = conf_ptp_offload(osi_core, &data->pto_config);
		break;

	case OSI_CMD_PTP_RXQ_ROUTE:
		ret = rxq_route_config(osi_core, &data->rxq_route);
		break;

	case OSI_CMD_CONFIG_FRP:
		ret = configure_frp(osi_core, &data->frp_cmd);
		break;

	case OSI_CMD_CONFIG_RSS:
		ret = ops_p->config_rss(osi_core);
		break;

	case OSI_CMD_CONFIG_EST:
		ret = config_est(osi_core, &data->est);
		break;

	case OSI_CMD_CONFIG_FPE:
		ret = config_fpe(osi_core, &data->fpe);
		break;

	case OSI_CMD_READ_REG:
		ret = ops_p->read_reg(osi_core, (nve32_t) data->arg1_u32);
		break;

	case OSI_CMD_WRITE_REG:
		ret = ops_p->write_reg(osi_core, (nve32_t) data->arg1_u32,
				       (nve32_t) data->arg2_u32);
		break;

	case OSI_CMD_GET_TX_TS:
		ret = get_tx_ts(osi_core, &data->tx_ts);
		break;

	case OSI_CMD_FREE_TS:
		free_tx_ts(osi_core, data->arg1_u32);
		ret = 0;
		break;

#ifdef OSI_DEBUG
	case OSI_CMD_REG_DUMP:
		core_reg_dump(osi_core);
		ret = 0;
		break;
	case OSI_CMD_STRUCTS_DUMP:
		core_structs_dump(osi_core);
		ret = 0;
		break;
#endif /* OSI_DEBUG */
	case OSI_CMD_CAP_TSC_PTP:
		ret = ops_p->ptp_tsc_capture(osi_core, &data->ptp_tsc);
		break;

	default:
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "CORE: Incorrect command\n",
			     (nveul64_t)data->cmd);
		break;
	}

	return ret;
}

nve32_t osi_get_hw_features(struct osi_core_priv_data *const osi_core,
			    struct osi_hw_features *hw_feat)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	if (validate_args(osi_core, l_core) < 0) {
		return -1;
	}

	if (hw_feat == OSI_NULL) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "CORE: Invalid hw_feat\n", 0ULL);
		return -1;
	}

	return l_core->ops_p->get_hw_features(osi_core, hw_feat);
}

void hw_interface_init_core_ops(struct if_core_ops *if_ops_p)
{
	if_ops_p->if_core_init = osi_hal_hw_core_init;
	if_ops_p->if_core_deinit = osi_hal_hw_core_deinit;
	if_ops_p->if_write_phy_reg = osi_hal_write_phy_reg;
	if_ops_p->if_read_phy_reg = osi_hal_read_phy_reg;
	if_ops_p->if_init_core_ops = osi_hal_init_core_ops;
	if_ops_p->if_handle_ioctl = osi_hal_handle_ioctl;
}
