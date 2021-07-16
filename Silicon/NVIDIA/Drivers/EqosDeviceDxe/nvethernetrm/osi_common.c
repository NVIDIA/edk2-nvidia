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

void osi_get_hw_features(void *base, struct osi_hw_features *hw_feat)
{
	unsigned int mac_hfr0;
	unsigned int mac_hfr1;
	unsigned int mac_hfr2;

	/* TODO: need to add HFR3 */
	mac_hfr0 = osi_readl((unsigned char *)base + EQOS_MAC_HFR0);
	mac_hfr1 = osi_readl((unsigned char *)base + EQOS_MAC_HFR1);
	mac_hfr2 = osi_readl((unsigned char *)base + EQOS_MAC_HFR2);

	hw_feat->mii_sel =
		((mac_hfr0 >> 0) & EQOS_MAC_HFR0_MIISEL_MASK);
	hw_feat->gmii_sel =
		((mac_hfr0 >> 1U) & EQOS_MAC_HFR0_GMIISEL_MASK);
	hw_feat->hd_sel =
		((mac_hfr0 >> 2U) & EQOS_MAC_HFR0_HDSEL_MASK);
	hw_feat->pcs_sel =
		((mac_hfr0 >> 3U) & EQOS_MAC_HFR0_PCSSEL_MASK);
	hw_feat->sma_sel =
		((mac_hfr0 >> 5U) & EQOS_MAC_HFR0_SMASEL_MASK);
	hw_feat->rwk_sel =
		((mac_hfr0 >> 6U) & EQOS_MAC_HFR0_RWKSEL_MASK);
	hw_feat->mgk_sel =
		((mac_hfr0 >> 7U) & EQOS_MAC_HFR0_MGKSEL_MASK);
	hw_feat->mmc_sel =
		((mac_hfr0 >> 8U) & EQOS_MAC_HFR0_MMCSEL_MASK);
	hw_feat->arp_offld_en =
		((mac_hfr0 >> 9U) & EQOS_MAC_HFR0_ARPOFFLDEN_MASK);
	hw_feat->ts_sel =
		((mac_hfr0 >> 12U) & EQOS_MAC_HFR0_TSSSEL_MASK);
	hw_feat->eee_sel =
		((mac_hfr0 >> 13U) & EQOS_MAC_HFR0_EEESEL_MASK);
	hw_feat->tx_coe_sel =
		((mac_hfr0 >> 14U) & EQOS_MAC_HFR0_TXCOESEL_MASK);
	hw_feat->rx_coe_sel =
		((mac_hfr0 >> 16U) & EQOS_MAC_HFR0_RXCOE_MASK);
	hw_feat->mac_addr16_sel =
		((mac_hfr0 >> 18U) & EQOS_MAC_HFR0_ADDMACADRSEL_MASK);
	hw_feat->mac_addr32_sel =
		((mac_hfr0 >> 23U) & EQOS_MAC_HFR0_MACADR32SEL_MASK);
	hw_feat->mac_addr64_sel =
		((mac_hfr0 >> 24U) & EQOS_MAC_HFR0_MACADR64SEL_MASK);
	hw_feat->tsstssel =
		((mac_hfr0 >> 25U) & EQOS_MAC_HFR0_TSINTSEL_MASK);
	hw_feat->sa_vlan_ins =
		((mac_hfr0 >> 27U) & EQOS_MAC_HFR0_SAVLANINS_MASK);
	hw_feat->act_phy_sel =
		((mac_hfr0 >> 28U) & EQOS_MAC_HFR0_ACTPHYSEL_MASK);
	hw_feat->rx_fifo_size =
		((mac_hfr1 >> 0) & EQOS_MAC_HFR1_RXFIFOSIZE_MASK);
	hw_feat->tx_fifo_size =
		((mac_hfr1 >> 6U) & EQOS_MAC_HFR1_TXFIFOSIZE_MASK);
	hw_feat->adv_ts_hword =
		((mac_hfr1 >> 13U) & EQOS_MAC_HFR1_ADVTHWORD_MASK);
	hw_feat->addr_64 =
		((mac_hfr1 >> 14U) & EQOS_MAC_HFR1_ADDR64_MASK);
	hw_feat->dcb_en =
		((mac_hfr1 >> 16U) & EQOS_MAC_HFR1_DCBEN_MASK);
	hw_feat->sph_en =
		((mac_hfr1 >> 17U) & EQOS_MAC_HFR1_SPHEN_MASK);
	hw_feat->tso_en =
		((mac_hfr1 >> 18U) & EQOS_MAC_HFR1_TSOEN_MASK);
	hw_feat->dma_debug_gen =
		((mac_hfr1 >> 19U) & EQOS_MAC_HFR1_DMADEBUGEN_MASK);
	hw_feat->av_sel =
		((mac_hfr1 >> 20U) & EQOS_MAC_HFR1_AVSEL_MASK);
	hw_feat->hash_tbl_sz =
		((mac_hfr1 >> 24U) & EQOS_MAC_HFR1_HASHTBLSZ_MASK);
	hw_feat->l3l4_filter_num =
		((mac_hfr1 >> 27U) & EQOS_MAC_HFR1_L3L4FILTERNUM_MASK);
	hw_feat->rx_q_cnt =
		((mac_hfr2 >> 0) & EQOS_MAC_HFR2_RXQCNT_MASK);
	hw_feat->tx_q_cnt =
		((mac_hfr2 >> 6U) & EQOS_MAC_HFR2_TXQCNT_MASK);
	hw_feat->rx_ch_cnt =
		((mac_hfr2 >> 12U) & EQOS_MAC_HFR2_RXCHCNT_MASK);
	hw_feat->tx_ch_cnt =
		((mac_hfr2 >> 18U) & EQOS_MAC_HFR2_TXCHCNT_MASK);
	hw_feat->pps_out_num =
		((mac_hfr2 >> 24U) & EQOS_MAC_HFR2_PPSOUTNUM_MASK);
	hw_feat->aux_snap_num =
		((mac_hfr2 >> 28U) & EQOS_MAC_HFR2_AUXSNAPNUM_MASK);
}

int osi_get_mac_version(void *addr, unsigned int *mac_ver)
{
	unsigned int macver;
	int ret = 0;

	macver = osi_readl((unsigned char *)addr + MAC_VERSION) &
		MAC_VERSION_SNVER_MASK;
	if (is_valid_mac_version(macver) == 0) {
		return -1;
	}

	*mac_ver = macver;
	return ret;
}

void osi_memset(void *s, unsigned int c, unsigned long count)
{
	unsigned char *xs = s;

	while (count != 0UL) {
		if (c < OSI_UCHAR_MAX) {
			*xs++ = (unsigned char)c;
		}
		count--;
	}
}
