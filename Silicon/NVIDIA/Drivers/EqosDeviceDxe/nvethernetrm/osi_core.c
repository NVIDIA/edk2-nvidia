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
 *
 *  Portions provided under the following terms:
 *  Copyright (c) 2018-2019 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 *  property and proprietary rights in and to this material, related
 *  documentation and any modifications thereto. Any use, reproduction,
 *  disclosure or distribution of this material and related documentation
 *  without an express license agreement from NVIDIA CORPORATION or
 *  its affiliates is strictly prohibited.
 *
 *  SPDX-FileCopyrightText: Copyright (c) 2018-2019 NVIDIA CORPORATION & AFFILIATES
 *  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 */

#include <osi_core.h>
#include <osd.h>

/**
 * @addtogroup MDIO Macros
 * @brief Helper MACROS for MDIO
 * @{
 */
#define MAC_MDIO_ADDRESS	0x200
#define MAC_GMII_BUSY		0x00000001U

#define MAC_MDIO_DATA		0x204

#define MAC_GMIIDR_GD_WR_MASK	0xffff0000U
#define MAC_GMIIDR_GD_MASK	0xffffU

#define MDIO_PHY_ADDR_SHIFT	21U
#define MDIO_PHY_REG_SHIFT	16U
#define MDIO_MII_WRITE		OSI_BIT(2)
/** @} */

/**
 * @brief poll_for_mii_idle Query the status of an ongoing DMA transfer
 *
 * @param[in] osi_core: OSI Core private data structure.
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline int poll_for_mii_idle(struct osi_core_priv_data *osi_core)
{
	unsigned int retry = 1000;
	unsigned int mac_gmiiar;
	unsigned int count;
	int cond = 1;

	count = 0;
	while (cond == 1) {
		if (count > retry) {
			osd_err(osi_core->osd, "MII operation timed out\n");
			return -1;
		}

		count++;
		osd_msleep(1U);

		mac_gmiiar = osi_readl((unsigned char *)osi_core->base +
				       MAC_MDIO_ADDRESS);

		if ((mac_gmiiar & MAC_GMII_BUSY) == 0U) {
			cond = 0;
		}
	}

	return 0;
}

int osi_write_phy_reg(struct osi_core_priv_data *osi_core, unsigned int phyaddr,
		      unsigned int phyreg, unsigned short phydata)
{
	unsigned int mac_gmiiar;
	unsigned int mac_gmiidr;
	int ret = 0;

	if (osi_core == OSI_NULL) {
		return -1;
	}

	/* wait for any previous MII read/write operation to complete */
	ret = poll_for_mii_idle(osi_core);
	if (ret < 0) {
		return ret;
	}

	mac_gmiidr = osi_readl((unsigned char *)osi_core->base + MAC_MDIO_DATA);

	mac_gmiidr = ((mac_gmiidr & MAC_GMIIDR_GD_WR_MASK) |
		      (((phydata) & MAC_GMIIDR_GD_MASK) << 0));

	osi_writel(mac_gmiidr, (unsigned char *)osi_core->base + MAC_MDIO_DATA);

	/* initiate the MII write operation by updating desired */
	/* phy address/id (0 - 31) */
	/* phy register offset */
	/* CSR Clock Range (20 - 35MHz) */
	/* Select write operation */
	/* set busy bit */
	mac_gmiiar = osi_readl((unsigned char *)osi_core->base + MAC_MDIO_ADDRESS);
	mac_gmiiar = (mac_gmiiar & 0x12U);
	mac_gmiiar = (mac_gmiiar | ((phyaddr) << MDIO_PHY_ADDR_SHIFT) |
		     ((phyreg) << MDIO_PHY_REG_SHIFT) |
		     ((osi_core->mdc_cr) << 8U) |
		     MDIO_MII_WRITE | MAC_GMII_BUSY);

	osi_writel(mac_gmiiar, (unsigned char *)osi_core->base + MAC_MDIO_ADDRESS);

	osd_usleep_range(9, 11);

	/* wait for MII write operation to complete */
	ret = poll_for_mii_idle(osi_core);
	if (ret < 0) {
		return ret;
	}

	return ret;
}

int osi_read_phy_reg(struct osi_core_priv_data *osi_core, unsigned int phyaddr,
		     unsigned int phyreg)
{
	unsigned int mac_gmiiar;
	unsigned int mac_gmiidr;
	unsigned int data;
	int ret = 0;

	if (osi_core == OSI_NULL) {
		return -1;
	}

	/* wait for any previous MII read/write operation to complete */
	ret = poll_for_mii_idle(osi_core);
	if (ret < 0) {
		return ret;
	}

	mac_gmiiar = osi_readl((unsigned char *)osi_core->base + MAC_MDIO_ADDRESS);
	/* initiate the MII read operation by updating desired */
	/* phy address/id (0 - 31) */
	/* phy register offset */
	/* CSR Clock Range (20 - 35MHz) */
	/* Select read operation */
	/* set busy bit */
	mac_gmiiar = (mac_gmiiar & 0x12U);
	mac_gmiiar = mac_gmiiar | ((phyaddr) << MDIO_PHY_ADDR_SHIFT) |
		     ((phyreg) << MDIO_PHY_REG_SHIFT) |
		     (osi_core->mdc_cr) << 8U | ((0x3U) << 2U) | MAC_GMII_BUSY;
	osi_writel(mac_gmiiar, (unsigned char *)osi_core->base + MAC_MDIO_ADDRESS);

	osd_usleep_range(9, 11);

	/* wait for MII write operation to complete */
	ret = poll_for_mii_idle(osi_core);
	if (ret < 0) {
		return ret;
	}

	mac_gmiidr = osi_readl((unsigned char *)osi_core->base + MAC_MDIO_DATA);
	data = (mac_gmiidr & 0x0000FFFFU);

	return (int)data;
}

int osi_init_core_ops(struct osi_core_priv_data *osi_core)
{
	if (osi_core->mac == OSI_MAC_HW_EQOS) {
		/* Get EQOS HW ops */
		osi_core->ops = eqos_get_hw_core_ops();
		/* Explicitly set osi_core->safety_config = OSI_NULL if
		 * a particular MAC version does not need SW safety mechanisms
		 * like periodic read-verify.
		 */
		osi_core->safety_config = (void *)eqos_get_core_safety_config();
		return 0;
	}

	return -1;
}

int osi_poll_for_swr(struct osi_core_priv_data *osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->poll_for_swr != OSI_NULL)) {
		return osi_core->ops->poll_for_swr(osi_core->base);
	}
	return -1;
}

int osi_set_mdc_clk_rate(struct osi_core_priv_data *osi_core,
			 unsigned long csr_clk_rate)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->set_mdc_clk_rate != OSI_NULL)) {
		osi_core->ops->set_mdc_clk_rate(osi_core, csr_clk_rate);
		return 0;
	}

	return -1;
}

int osi_hw_core_init(struct osi_core_priv_data *osi_core,
		     unsigned int tx_fifo_size,
		     unsigned int rx_fifo_size)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->core_init != OSI_NULL)) {
		return osi_core->ops->core_init(osi_core, tx_fifo_size,
						rx_fifo_size);
	}

	return -1;
}

int osi_hw_core_deinit(struct osi_core_priv_data *osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->core_deinit != OSI_NULL)) {
		osi_core->ops->core_deinit(osi_core);
		return 0;
	}

	return -1;
}

int osi_validate_core_regs(struct osi_core_priv_data *osi_core)
{
	int ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->validate_regs != OSI_NULL) &&
	    (osi_core->safety_config != OSI_NULL)) {
		ret = osi_core->ops->validate_regs(osi_core);
	}

	return ret;
}

int osi_start_mac(struct osi_core_priv_data *osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->start_mac != OSI_NULL)) {
		osi_core->ops->start_mac(osi_core->base);
		return 0;
	}

	return -1;
}

int osi_stop_mac(struct osi_core_priv_data *osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->stop_mac != OSI_NULL)) {
		osi_core->ops->stop_mac(osi_core->base);
		return 0;
	}

	return -1;
}

int osi_common_isr(struct osi_core_priv_data *osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->handle_common_intr != OSI_NULL)) {
		osi_core->ops->handle_common_intr(osi_core);
		return 0;
	}

	return -1;
}

int osi_set_mode(struct osi_core_priv_data *osi_core, int mode)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->set_mode != OSI_NULL)) {
		osi_core->ops->set_mode(osi_core->base, mode);
		return 0;
	}

	return -1;
}

int osi_set_speed(struct osi_core_priv_data *osi_core, int speed)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->set_speed != OSI_NULL)) {
		osi_core->ops->set_speed(osi_core->base, speed);
		return 0;
	}

	return -1;
}

int osi_pad_calibrate(struct osi_core_priv_data *osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->pad_calibrate != OSI_NULL)) {
		return osi_core->ops->pad_calibrate(osi_core->base);
	}

	return -1;
}

int osi_flush_mtl_tx_queue(struct osi_core_priv_data *osi_core,
			   unsigned int qinx)
{
        if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
            (osi_core->ops->flush_mtl_tx_queue != OSI_NULL)) {
		return osi_core->ops->flush_mtl_tx_queue(osi_core->base, qinx);
        }

	return -1;
}

int osi_config_mac_loopback(struct osi_core_priv_data *osi_core,
			    unsigned int lb_mode)
{
	/* Configure MAC LoopBack */
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_mac_loopback != OSI_NULL)) {
		return osi_core->ops->config_mac_loopback(osi_core->base,
							  lb_mode);
	}

	return -1;
}

int osi_set_avb(struct osi_core_priv_data *osi_core,
		struct osi_core_avb_algorithm *avb)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->set_avb_algorithm != OSI_NULL)) {
		return osi_core->ops->set_avb_algorithm(osi_core, avb);
	}

	return -1;
}

int osi_get_avb(struct osi_core_priv_data *osi_core,
		struct osi_core_avb_algorithm *avb)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->get_avb_algorithm != OSI_NULL)) {
		return osi_core->ops->get_avb_algorithm(osi_core, avb);
	}

	return -1;
}

int osi_configure_txstatus(struct osi_core_priv_data *osi_core,
			   unsigned int tx_status)
{
	/* Configure Drop Transmit Status */
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_tx_status != OSI_NULL)) {
		return osi_core->ops->config_tx_status(osi_core->base,
						       tx_status);
	}

	return -1;
}

int osi_config_fw_err_pkts(struct osi_core_priv_data *osi_core,
			   unsigned int qinx, unsigned int fw_err)
{
	/* Configure Forwarding of Error packets */
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_fw_err_pkts != OSI_NULL)) {
		return osi_core->ops->config_fw_err_pkts(osi_core->base,
							 qinx, fw_err);
	}

	return -1;
}

int osi_config_rx_crc_check(struct osi_core_priv_data *osi_core,
			    unsigned int crc_chk)
{
	/* Configure CRC Checking for Received Packets */
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_rx_crc_check != OSI_NULL)) {
		return osi_core->ops->config_rx_crc_check(osi_core->base,
							  crc_chk);
	}

	return -1;
}

int osi_configure_flow_control(struct osi_core_priv_data *osi_core,
			       unsigned int flw_ctrl)
{
	/* Configure Flow control settings */
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_flow_control != OSI_NULL)) {
		return osi_core->ops->config_flow_control(osi_core->base,
							  flw_ctrl);
	}

	return -1;
}

int osi_config_arp_offload(struct osi_core_priv_data *osi_core,
			   unsigned int flags,
			   unsigned char *ip_addr)
{
	if (osi_core != OSI_NULL && osi_core->ops != OSI_NULL &&
	    osi_core->ops->config_arp_offload != OSI_NULL) {
		return osi_core->ops->config_arp_offload(osi_core->mac_ver,
							 osi_core->base,
							 flags, ip_addr);
	}

	return -1;
}

int osi_config_mac_pkt_filter_reg(struct osi_core_priv_data *osi_core,
				  struct osi_filter pfilter)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_mac_pkt_filter_reg != OSI_NULL)) {
		osi_core->ops->config_mac_pkt_filter_reg(osi_core,
							 pfilter);
		return 0;
	}

	return -1;
}

int osi_update_mac_addr_low_high_reg(struct osi_core_priv_data *osi_core,
				     unsigned int index, unsigned char value[],
				     unsigned int dma_routing_enable,
				     unsigned int dma_chan,
				     unsigned int addr_mask,
				     unsigned int src_dest)
{
	int ret = -1;

	if (osi_core == OSI_NULL) {
		return ret;
	}

	if ((dma_routing_enable == OSI_ENABLE) &&
	    (osi_core->dcs_en != OSI_ENABLE)) {
		osd_err(osi_core->osd, "dma routing enabled but dcs disabled in DT\n");
		return ret;
	}

	if ((osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->update_mac_addr_low_high_reg != OSI_NULL)) {
		return osi_core->ops->update_mac_addr_low_high_reg(
							    osi_core,
							    index,
							    value,
							    dma_routing_enable,
							    dma_chan,
							    addr_mask,
							    src_dest);
	}

	return -1;
}

int osi_config_l3_l4_filter_enable(struct osi_core_priv_data *osi_core,
				   unsigned int enable)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_l3_l4_filter_enable != OSI_NULL)) {
		return osi_core->ops->config_l3_l4_filter_enable(osi_core->base,
								 enable);
	}

	return -1;
}

int osi_config_l3_filters(struct osi_core_priv_data *osi_core,
			  unsigned int filter_no,
			  unsigned int enb_dis,
			  unsigned int ipv4_ipv6_match,
			  unsigned int src_dst_addr_match,
			  unsigned int perfect_inverse_match,
			  unsigned int dma_routing_enable,
			  unsigned int dma_chan)
{
	int ret = -1;

	if (osi_core == OSI_NULL) {
		return ret;
	}

	if ((dma_routing_enable == OSI_ENABLE) &&
	    (osi_core->dcs_en != OSI_ENABLE)) {
		osd_err(osi_core->osd, "dma routing enabled but dcs disabled in DT\n");
		return ret;
	}

	if ((osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_l3_filters != OSI_NULL)) {
		return osi_core->ops->config_l3_filters(osi_core, filter_no,
							enb_dis,
							ipv4_ipv6_match,
							src_dst_addr_match,
							perfect_inverse_match,
							dma_routing_enable,
							dma_chan);
	}

	return -1;
}

int osi_update_ip4_addr(struct osi_core_priv_data *osi_core,
			unsigned int filter_no,
			unsigned char addr[],
			unsigned int src_dst_addr_match)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->update_ip4_addr != OSI_NULL)) {
		return osi_core->ops->update_ip4_addr(osi_core, filter_no,
						      addr, src_dst_addr_match);
	}

	return -1;
}

int osi_update_ip6_addr(struct osi_core_priv_data *osi_core,
			unsigned int filter_no,
			unsigned short addr[])
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->update_ip6_addr != OSI_NULL)) {
		return osi_core->ops->update_ip6_addr(osi_core, filter_no,
						      addr);
	}
	return -1;
}

int osi_config_l4_filters(struct osi_core_priv_data *osi_core,
			  unsigned int filter_no,
			  unsigned int enb_dis,
			  unsigned int tcp_udp_match,
			  unsigned int src_dst_port_match,
			  unsigned int perfect_inverse_match,
			  unsigned int dma_routing_enable,
			  unsigned int dma_chan)
{
	int ret = -1;

	if (osi_core == OSI_NULL) {
		return ret;
	}

	if ((dma_routing_enable == OSI_ENABLE) &&
	    (osi_core->dcs_en != OSI_ENABLE)) {
		osd_err(osi_core->osd, "dma routing enabled but dcs disabled in DT\n");
		return ret;
	}

	if ((osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_l4_filters != OSI_NULL)) {
		return osi_core->ops->config_l4_filters(osi_core, filter_no,
							enb_dis, tcp_udp_match,
							src_dst_port_match,
							perfect_inverse_match,
							dma_routing_enable,
							dma_chan);
	}

	return -1;
}

int osi_update_l4_port_no(struct osi_core_priv_data *osi_core,
			  unsigned int filter_no, unsigned short port_no,
			  unsigned int src_dst_port_match)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->update_l4_port_no != OSI_NULL)) {
		return osi_core->ops->update_l4_port_no(osi_core, filter_no,
							port_no,
							src_dst_port_match);
	}

	return -1;
}

int osi_config_vlan_filtering(struct osi_core_priv_data *osi_core,
			      unsigned int filter_enb_dis,
			      unsigned int perfect_hash_filtering,
			      unsigned int perfect_inverse_match)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_vlan_filtering != OSI_NULL)) {
		return osi_core->ops->config_vlan_filtering(
							osi_core,
							filter_enb_dis,
							perfect_hash_filtering,
							perfect_inverse_match);
	}

	return -1;
}

int  osi_config_l2_da_perfect_inverse_match(struct osi_core_priv_data *osi_core,
					    unsigned int perfect_inverse_match)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_l2_da_perfect_inverse_match != OSI_NULL)) {
		return osi_core->ops->config_l2_da_perfect_inverse_match(
							osi_core->base,
							perfect_inverse_match);
	}

	return -1;
}

int osi_config_rxcsum_offload(struct osi_core_priv_data *osi_core,
			      unsigned int enable)
{
	if (osi_core != OSI_NULL && osi_core->ops != OSI_NULL &&
	    osi_core->ops->config_rxcsum_offload != OSI_NULL) {
		return osi_core->ops->config_rxcsum_offload(osi_core->base,
							    enable);
	}

	return -1;
}

int  osi_update_vlan_id(struct osi_core_priv_data *osi_core,
			unsigned int vid)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->update_vlan_id != OSI_NULL && vid != 0U)) {
		return osi_core->ops->update_vlan_id(osi_core->base,
						    vid);
	}

	return -1;
}

int osi_set_systime_to_mac(struct osi_core_priv_data *osi_core,
			   unsigned int sec, unsigned int nsec)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->set_systime_to_mac != OSI_NULL)) {
		return osi_core->ops->set_systime_to_mac(osi_core->base,
							 sec,
							 nsec);
	}

	return -1;
}

/**
 *@brief div_u64_rem - updates remainder and returns Quotient
 *
 * Algorithm: Dividend will be divided by divisor and stores the
 *	 remainder value and returns quotient
 *
 * @param[in] dividend: Dividend value
 * @param[in] divisor: Divisor value
 * @param[out] remain: Remainder
 *
 * @note MAC IP should be out of reset and need to be initialized as the
 *	 requirements
 *
 * @returns Quotient
 */
static inline unsigned long div_u64_rem(unsigned long dividend,
					unsigned long divisor,
					unsigned long *remain)
{
	unsigned long ret = 0;

	if (divisor != 0U) {
		*remain = dividend % divisor;
		ret = dividend / divisor;
	} else {
		ret = 0;
	}
	return ret;
}

/**
 * @brief div_u64 - Calls a function which returns quotient
 *
 * @param[in] dividend: Dividend
 * @param[in] divisor: Divisor
 *
 * @note MAC IP should be out of reset and need to be initialized as the
 *	requirements.
 *
 * @returns Quotient
 */
static inline unsigned long div_u64(unsigned long dividend,
				    unsigned long divisor)
{
	unsigned long remain;

	return div_u64_rem(dividend, divisor, &remain);
}

int osi_adjust_freq(struct osi_core_priv_data *osi_core, int ppb)
{
	unsigned long adj;
	unsigned long temp;
	unsigned int diff = 0;
	unsigned int addend;
	unsigned int neg_adj = 0;
	int ret = -1;

	if (ppb < 0) {
		neg_adj = 1U;
		ppb = -ppb;
	}

	if (osi_core == OSI_NULL) {
		return ret;
	}

	addend = osi_core->default_addend;
	adj = (unsigned long)addend * (unsigned int)ppb;

	/*
	 * div_u64 will divide the "adj" by "1000000000ULL"
	 * and return the quotient.
	 */
	temp = div_u64(adj, OSI_NSEC_PER_SEC);
	if (temp < UINT_MAX) {
		diff = (unsigned int)temp;
	} else {
		/* do nothing here */
	}

	if (neg_adj == 0U) {
		if (addend <= UINT_MAX - diff) {
			addend = (addend + diff);
		} else {
			/* do nothing here */
		}
	} else {
		if (addend > diff) {
			addend = addend - diff;
		} else if (addend < diff) {
			addend = diff - addend;
		} else {
			/* do nothing here */
		}
	}

	if ((osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_addend != OSI_NULL)) {
		ret = osi_core->ops->config_addend(osi_core->base, addend);
	}

	return ret;
}

int osi_adjust_time(struct osi_core_priv_data *osi_core, long delta)
{
	unsigned int neg_adj = 0;
	unsigned int sec = 0, nsec = 0;
	unsigned long quotient;
	unsigned long reminder = 0;
	unsigned long udelta = 0;
	int ret = -1;

	if (delta < 0) {
		neg_adj = 1;
		delta = -delta;
	}
	udelta = (unsigned long) delta;
	quotient = div_u64_rem(udelta, OSI_NSEC_PER_SEC, &reminder);
	if (quotient <= UINT_MAX) {
		sec = (unsigned int)quotient;
	} else {
		/* do nothing */
	}
	if (reminder <= UINT_MAX) {
		nsec = (unsigned int)reminder;
	} else {
		/* do nothing here */
	}

	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->adjust_systime != OSI_NULL)) {
		ret = osi_core->ops->adjust_systime(osi_core->base, sec, nsec,
					neg_adj,
					osi_core->ptp_config.one_nsec_accuracy);
	}

	return ret;
}

int osi_get_systime_from_mac(struct osi_core_priv_data *osi_core,
			     unsigned int *sec,
			     unsigned int *nsec)
{
	unsigned long long ns = 0;
	unsigned long long temp = 0;
	unsigned long remain = 0;

	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->get_systime_from_mac != OSI_NULL)) {
		ns = osi_core->ops->get_systime_from_mac(osi_core->base);
	} else {
		return -1;
	}

	temp = div_u64_rem((unsigned long)ns, OSI_NSEC_PER_SEC, &remain);
	if (temp < UINT_MAX) {
		*sec = (unsigned int) temp;
	} else {
		/* do nothing here */
	}
	if (remain < UINT_MAX) {
		*nsec = (unsigned int)remain;
	} else {
		/* do nothing here */
	}

	return 0;
}

int osi_ptp_configuration(struct osi_core_priv_data *osi_core,
			  unsigned int enable)
{
	int ret = 0;
	unsigned long temp = 0, temp1 = 0;

	if ((osi_core == OSI_NULL) || (osi_core->ops == OSI_NULL) ||
	    (osi_core->ops->config_tscr == OSI_NULL) ||
	    (osi_core->ops->config_ssir == OSI_NULL) ||
	    (osi_core->ops->config_addend == OSI_NULL) ||
	    (osi_core->ops->set_systime_to_mac == OSI_NULL)) {
		return -1;
	}

	if (enable == OSI_DISABLE) {
		/* disable hw time stamping */
		/* Program MAC_Timestamp_Control Register */
		osi_core->ops->config_tscr(osi_core->base, OSI_DISABLE);
	} else {
		/* Program MAC_Timestamp_Control Register */
		osi_core->ops->config_tscr(osi_core->base,
					   osi_core->ptp_config.ptp_filter);

		/* Program Sub Second Increment Register */
		osi_core->ops->config_ssir(osi_core->base,
					   osi_core->ptp_config.ptp_clock);

		/* formula for calculating addend value is
		 * addend = 2^32/freq_div_ratio;
		 * where, freq_div_ratio = EQOS_SYSCLOCK/50MHz
		 * hence, addend = ((2^32) * 50MHz)/EQOS_SYSCLOCK;
		 * NOTE: EQOS_SYSCLOCK must be >= 50MHz to achive 20ns accuracy.
		 * 2^x * y == (y << x), hence
		 * 2^32 * 6250000 ==> (6250000 << 32)
		 */
		temp = ((unsigned long)OSI_ETHER_SYSCLOCK << 32);
		temp1 = div_u64(temp,
				(unsigned long)osi_core->ptp_config.ptp_ref_clk_rate);
		if (temp1 < UINT_MAX) {
			osi_core->default_addend = (unsigned int) temp1;
		} else {
			/* do nothing here */
		}
		/* Program addend value */
		ret = osi_core->ops->config_addend(osi_core->base,
					osi_core->default_addend);

		/* Set current time */
		ret = osi_core->ops->set_systime_to_mac(osi_core->base,
						osi_core->ptp_config.sec,
						osi_core->ptp_config.nsec);
	}

	return ret;
}

int osi_read_mmc(struct osi_core_priv_data *osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->read_mmc != OSI_NULL)) {
		osi_core->ops->read_mmc(osi_core);
		return 0;
	}

	return -1;
}

int osi_reset_mmc(struct osi_core_priv_data *osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->reset_mmc != OSI_NULL)) {
		osi_core->ops->reset_mmc(osi_core);
		return 0;
	}

	return -1;
}
