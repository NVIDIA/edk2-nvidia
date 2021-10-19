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
#include <osi_core.h>
#include "eqos_mmc.h"
#include "eqos_core.h"

/**
 * @brief update_mmc_val - function to read register and return value to callee
 *
 * @note
 * Algorithm:
 *  - Read the registers, check for boundary, if more, reset
 *    counters else return same to caller.
 *
 * @param[in, out] osi_core: OSI core private data structure.
 * @param[in] last_value: previous value of stats variable.
 * @param[in] offset: HW register offset
 *
 * @pre
 *  - MAC should be init and started. see osi_start_mac()
 *  - osi_core->osd should be populated
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static inline nveu64_t update_mmc_val(struct osi_core_priv_data *const osi_core,
				      nveu64_t last_value,
				      nveu64_t offset)
{
	nveu64_t temp;
	nveu32_t value = osi_readla(osi_core,
				    (nveu8_t *)osi_core->base + offset);

	temp = last_value + value;
	if (temp < last_value) {
		OSI_CORE_ERR(osi_core->osd,
			     OSI_LOG_ARG_OUTOFBOUND,
			     "Value overflow resetting  all counters\n",
			     (nveul64_t)offset);
		eqos_reset_mmc(osi_core);
	} else {
		return temp;
	}

	return 0;
}

/**
 * @brief eqos_reset_mmc - To reset MMC registers and ether_mmc_counter
 *        structure variable
 *
 * @param[in, out] osi_core: OSI core private data structure.
 *
 * @pre
 *  - MAC should be init and started. see osi_start_mac()
 *  - osi_core->osd should be populated
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
void eqos_reset_mmc(struct osi_core_priv_data *const osi_core)
{
	nveu32_t value;

	value = osi_readla(osi_core,
			   (nveu8_t *)osi_core->base + EQOS_MMC_CNTRL);
	/* self-clear bit in one clock cycle */
	value |= EQOS_MMC_CNTRL_CNTRST;
	osi_writela(osi_core, value,
		    (nveu8_t *)osi_core->base + EQOS_MMC_CNTRL);
	osi_memset(&osi_core->mmc, 0U, sizeof(struct osi_mmc_counters));
}

/**
 * @brief eqos_read_mmc - To read MMC registers and ether_mmc_counter structure
 *        variable
 *
 * @note
 * Algorithm:
 *  - Pass register offset and old value to helper function and
 *    update structure.
 *
 * @param[in, out] osi_core: OSI core private data structure.
 *
 * @pre
 *  - MAC should be init and started. see osi_start_mac()
 *  - osi_core->osd should be populated
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
void eqos_read_mmc(struct osi_core_priv_data *const osi_core)
{
	struct osi_mmc_counters *mmc = &osi_core->mmc;

	mmc->mmc_tx_octetcount_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_octetcount_gb,
			       MMC_TXOCTETCOUNT_GB);
	mmc->mmc_tx_framecount_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_framecount_gb,
			       MMC_TXPACKETCOUNT_GB);
	mmc->mmc_tx_broadcastframe_g =
		update_mmc_val(osi_core, mmc->mmc_tx_broadcastframe_g,
			       MMC_TXBROADCASTPACKETS_G);
	mmc->mmc_tx_multicastframe_g =
		update_mmc_val(osi_core, mmc->mmc_tx_multicastframe_g,
			       MMC_TXMULTICASTPACKETS_G);
	mmc->mmc_tx_64_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_64_octets_gb,
			       MMC_TX64OCTETS_GB);
	mmc->mmc_tx_65_to_127_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_65_to_127_octets_gb,
			       MMC_TX65TO127OCTETS_GB);
	mmc->mmc_tx_128_to_255_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_128_to_255_octets_gb,
			       MMC_TX128TO255OCTETS_GB);
	mmc->mmc_tx_256_to_511_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_256_to_511_octets_gb,
			       MMC_TX256TO511OCTETS_GB);
	mmc->mmc_tx_512_to_1023_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_512_to_1023_octets_gb,
			       MMC_TX512TO1023OCTETS_GB);
	mmc->mmc_tx_1024_to_max_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_1024_to_max_octets_gb,
			       MMC_TX1024TOMAXOCTETS_GB);
	mmc->mmc_tx_unicast_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_unicast_gb,
			       MMC_TXUNICASTPACKETS_GB);
	mmc->mmc_tx_multicast_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_multicast_gb,
			       MMC_TXMULTICASTPACKETS_GB);
	mmc->mmc_tx_broadcast_gb =
		update_mmc_val(osi_core, mmc->mmc_tx_broadcast_gb,
			       MMC_TXBROADCASTPACKETS_GB);
	mmc->mmc_tx_underflow_error =
		update_mmc_val(osi_core, mmc->mmc_tx_underflow_error,
			       MMC_TXUNDERFLOWERROR);
	mmc->mmc_tx_singlecol_g =
		update_mmc_val(osi_core, mmc->mmc_tx_singlecol_g,
			       MMC_TXSINGLECOL_G);
	mmc->mmc_tx_multicol_g =
		update_mmc_val(osi_core, mmc->mmc_tx_multicol_g,
			       MMC_TXMULTICOL_G);
	mmc->mmc_tx_deferred =
		update_mmc_val(osi_core, mmc->mmc_tx_deferred,
			       MMC_TXDEFERRED);
	mmc->mmc_tx_latecol =
		update_mmc_val(osi_core, mmc->mmc_tx_latecol,
			       MMC_TXLATECOL);
	mmc->mmc_tx_exesscol =
		update_mmc_val(osi_core, mmc->mmc_tx_exesscol,
			       MMC_TXEXESSCOL);
	mmc->mmc_tx_carrier_error =
		update_mmc_val(osi_core, mmc->mmc_tx_exesscol,
			       MMC_TXCARRIERERROR);
	mmc->mmc_tx_octetcount_g =
		update_mmc_val(osi_core, mmc->mmc_tx_octetcount_g,
			       MMC_TXOCTETCOUNT_G);
	mmc->mmc_tx_framecount_g =
		 update_mmc_val(osi_core, mmc->mmc_tx_framecount_g,
				MMC_TXPACKETSCOUNT_G);
	mmc->mmc_tx_excessdef =
		update_mmc_val(osi_core, mmc->mmc_tx_excessdef,
			       MMC_TXEXCESSDEF);
	mmc->mmc_tx_pause_frame =
		update_mmc_val(osi_core, mmc->mmc_tx_pause_frame,
			       MMC_TXPAUSEPACKETS);
	mmc->mmc_tx_vlan_frame_g =
		update_mmc_val(osi_core, mmc->mmc_tx_vlan_frame_g,
			       MMC_TXVLANPACKETS_G);
	mmc->mmc_tx_osize_frame_g =
		update_mmc_val(osi_core, mmc->mmc_tx_osize_frame_g,
			       MMC_TXOVERSIZE_G);
	mmc->mmc_rx_framecount_gb =
		update_mmc_val(osi_core, mmc->mmc_rx_framecount_gb,
			       MMC_RXPACKETCOUNT_GB);
	mmc->mmc_rx_octetcount_gb =
		update_mmc_val(osi_core, mmc->mmc_rx_octetcount_gb,
			       MMC_RXOCTETCOUNT_GB);
	mmc->mmc_rx_octetcount_g =
		update_mmc_val(osi_core, mmc->mmc_rx_octetcount_g,
			       MMC_RXOCTETCOUNT_G);
	mmc->mmc_rx_broadcastframe_g =
		update_mmc_val(osi_core, mmc->mmc_rx_broadcastframe_g,
			       MMC_RXBROADCASTPACKETS_G);
	mmc->mmc_rx_multicastframe_g =
		update_mmc_val(osi_core, mmc->mmc_rx_multicastframe_g,
			       MMC_RXMULTICASTPACKETS_G);
	mmc->mmc_rx_crc_error =
		update_mmc_val(osi_core, mmc->mmc_rx_crc_error,
			       MMC_RXCRCERROR);
	mmc->mmc_rx_align_error =
		update_mmc_val(osi_core, mmc->mmc_rx_align_error,
			       MMC_RXALIGNMENTERROR);
	mmc->mmc_rx_runt_error =
		update_mmc_val(osi_core, mmc->mmc_rx_runt_error,
			       MMC_RXRUNTERROR);
	mmc->mmc_rx_jabber_error =
		update_mmc_val(osi_core, mmc->mmc_rx_jabber_error,
			       MMC_RXJABBERERROR);
	mmc->mmc_rx_undersize_g =
		update_mmc_val(osi_core, mmc->mmc_rx_undersize_g,
			       MMC_RXUNDERSIZE_G);
	mmc->mmc_rx_oversize_g =
		update_mmc_val(osi_core, mmc->mmc_rx_oversize_g,
			       MMC_RXOVERSIZE_G);
	mmc->mmc_rx_64_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_rx_64_octets_gb,
			       MMC_RX64OCTETS_GB);
	mmc->mmc_rx_65_to_127_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_rx_65_to_127_octets_gb,
			       MMC_RX65TO127OCTETS_GB);
	mmc->mmc_rx_128_to_255_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_rx_128_to_255_octets_gb,
			       MMC_RX128TO255OCTETS_GB);
	mmc->mmc_rx_256_to_511_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_rx_256_to_511_octets_gb,
			       MMC_RX256TO511OCTETS_GB);
	mmc->mmc_rx_512_to_1023_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_rx_512_to_1023_octets_gb,
			       MMC_RX512TO1023OCTETS_GB);
	mmc->mmc_rx_1024_to_max_octets_gb =
		update_mmc_val(osi_core, mmc->mmc_rx_1024_to_max_octets_gb,
			       MMC_RX1024TOMAXOCTETS_GB);
	mmc->mmc_rx_unicast_g =
		update_mmc_val(osi_core, mmc->mmc_rx_unicast_g,
			       MMC_RXUNICASTPACKETS_G);
	mmc->mmc_rx_length_error =
		update_mmc_val(osi_core, mmc->mmc_rx_length_error,
			       MMC_RXLENGTHERROR);
	mmc->mmc_rx_outofrangetype =
		update_mmc_val(osi_core, mmc->mmc_rx_outofrangetype,
			       MMC_RXOUTOFRANGETYPE);
	mmc->mmc_rx_pause_frames =
		update_mmc_val(osi_core, mmc->mmc_rx_pause_frames,
			       MMC_RXPAUSEPACKETS);
	mmc->mmc_rx_fifo_overflow =
		update_mmc_val(osi_core, mmc->mmc_rx_fifo_overflow,
			       MMC_RXFIFOOVERFLOW);
	mmc->mmc_rx_vlan_frames_gb =
		update_mmc_val(osi_core, mmc->mmc_rx_vlan_frames_gb,
			       MMC_RXVLANPACKETS_GB);
	mmc->mmc_rx_watchdog_error =
		update_mmc_val(osi_core, mmc->mmc_rx_watchdog_error,
			       MMC_RXWATCHDOGERROR);
	mmc->mmc_rx_receive_error =
		update_mmc_val(osi_core, mmc->mmc_rx_receive_error,
			       MMC_RXRCVERROR);
	mmc->mmc_rx_ctrl_frames_g =
		update_mmc_val(osi_core, mmc->mmc_rx_ctrl_frames_g,
			       MMC_RXCTRLPACKETS_G);
	mmc->mmc_tx_lpi_usec_cntr =
		update_mmc_val(osi_core, mmc->mmc_tx_lpi_usec_cntr,
			       MMC_TXLPIUSECCNTR);
	mmc->mmc_tx_lpi_tran_cntr =
		update_mmc_val(osi_core, mmc->mmc_tx_lpi_tran_cntr,
			       MMC_TXLPITRANCNTR);
	mmc->mmc_rx_lpi_usec_cntr =
		update_mmc_val(osi_core, mmc->mmc_rx_lpi_usec_cntr,
			       MMC_RXLPIUSECCNTR);
	mmc->mmc_rx_lpi_tran_cntr =
		update_mmc_val(osi_core, mmc->mmc_rx_lpi_tran_cntr,
			       MMC_RXLPITRANCNTR);
	mmc->mmc_rx_ipv4_gd =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv4_gd,
			       MMC_RXIPV4_GD_PKTS);
	mmc->mmc_rx_ipv4_hderr =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv4_hderr,
			       MMC_RXIPV4_HDRERR_PKTS);
	mmc->mmc_rx_ipv4_nopay =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv4_nopay,
			       MMC_RXIPV4_NOPAY_PKTS);
	mmc->mmc_rx_ipv4_frag =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv4_frag,
			       MMC_RXIPV4_FRAG_PKTS);
	mmc->mmc_rx_ipv4_udsbl =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv4_udsbl,
			       MMC_RXIPV4_UBSBL_PKTS);
	mmc->mmc_rx_ipv6_gd =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv6_gd,
			       MMC_RXIPV6_GD_PKTS);
	mmc->mmc_rx_ipv6_hderr =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv6_hderr,
			       MMC_RXIPV6_HDRERR_PKTS);
	mmc->mmc_rx_ipv6_nopay =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv6_nopay,
			       MMC_RXIPV6_NOPAY_PKTS);
	mmc->mmc_rx_udp_gd =
		update_mmc_val(osi_core, mmc->mmc_rx_udp_gd,
			       MMC_RXUDP_GD_PKTS);
	mmc->mmc_rx_udp_err =
		update_mmc_val(osi_core, mmc->mmc_rx_udp_err,
			       MMC_RXUDP_ERR_PKTS);
	mmc->mmc_rx_tcp_gd =
		update_mmc_val(osi_core, mmc->mmc_rx_tcp_gd,
			       MMC_RXTCP_GD_PKTS);
	mmc->mmc_rx_tcp_err =
		update_mmc_val(osi_core, mmc->mmc_rx_tcp_err,
			       MMC_RXTCP_ERR_PKTS);
	mmc->mmc_rx_icmp_gd =
		update_mmc_val(osi_core, mmc->mmc_rx_icmp_gd,
			       MMC_RXICMP_GD_PKTS);
	mmc->mmc_rx_icmp_err =
		update_mmc_val(osi_core, mmc->mmc_rx_icmp_err,
			       MMC_RXICMP_ERR_PKTS);
	mmc->mmc_rx_ipv4_gd_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv4_gd_octets,
			       MMC_RXIPV4_GD_OCTETS);
	mmc->mmc_rx_ipv4_hderr_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv4_hderr_octets,
			       MMC_RXIPV4_HDRERR_OCTETS);
	mmc->mmc_rx_ipv4_nopay_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv4_nopay_octets,
			       MMC_RXIPV4_NOPAY_OCTETS);
	mmc->mmc_rx_ipv4_frag_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv4_frag_octets,
			       MMC_RXIPV4_FRAG_OCTETS);
	mmc->mmc_rx_ipv4_udsbl_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv4_udsbl_octets,
			       MMC_RXIPV4_UDSBL_OCTETS);
	mmc->mmc_rx_udp_gd_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_udp_gd_octets,
			       MMC_RXUDP_GD_OCTETS);
	mmc->mmc_rx_ipv6_gd_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv6_gd_octets,
			       MMC_RXIPV6_GD_OCTETS);
	mmc->mmc_rx_ipv6_hderr_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv6_hderr_octets,
			       MMC_RXIPV6_HDRERR_OCTETS);
	mmc->mmc_rx_ipv6_nopay_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_ipv6_nopay_octets,
			       MMC_RXIPV6_NOPAY_OCTETS);
	mmc->mmc_rx_udp_gd_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_udp_gd_octets,
			       MMC_RXUDP_GD_OCTETS);
	mmc->mmc_rx_udp_err_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_udp_err_octets,
			       MMC_RXUDP_ERR_OCTETS);
	mmc->mmc_rx_tcp_gd_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_tcp_gd_octets,
			       MMC_RXTCP_GD_OCTETS);
	mmc->mmc_rx_tcp_err_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_tcp_err_octets,
			       MMC_RXTCP_ERR_OCTETS);
	mmc->mmc_rx_icmp_gd_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_icmp_gd_octets,
			       MMC_RXICMP_GD_OCTETS);
	mmc->mmc_rx_icmp_err_octets =
		update_mmc_val(osi_core, mmc->mmc_rx_icmp_err_octets,
			       MMC_RXICMP_ERR_OCTETS);
	mmc->mmc_tx_fpe_frag_cnt =
		update_mmc_val(osi_core, mmc->mmc_tx_fpe_frag_cnt,
			       MMC_TX_FPE_FRAG_COUNTER);
	mmc->mmc_tx_fpe_hold_req_cnt =
		update_mmc_val(osi_core, mmc->mmc_tx_fpe_hold_req_cnt,
			       MMC_TX_HOLD_REQ_COUNTER);
	mmc->mmc_rx_packet_reass_err_cnt =
		update_mmc_val(osi_core, mmc->mmc_rx_packet_reass_err_cnt,
			       MMC_RX_PKT_ASSEMBLY_ERR_CNTR);
	mmc->mmc_rx_packet_smd_err_cnt =
		update_mmc_val(osi_core, mmc->mmc_rx_packet_smd_err_cnt,
			       MMC_RX_PKT_SMD_ERR_CNTR);
	mmc->mmc_rx_packet_asm_ok_cnt =
		update_mmc_val(osi_core, mmc->mmc_rx_packet_asm_ok_cnt,
			       MMC_RX_PKT_ASSEMBLY_OK_CNTR);
	mmc->mmc_rx_fpe_fragment_cnt =
		update_mmc_val(osi_core, mmc->mmc_rx_fpe_fragment_cnt,
			       MMC_RX_FPE_FRAG_CNTR);
}
