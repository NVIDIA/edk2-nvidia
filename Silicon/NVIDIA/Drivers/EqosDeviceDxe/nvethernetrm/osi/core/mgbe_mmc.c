// SPDX-License-Identifier: MIT

/* SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "common.h"
#include <osi_common.h>
#include <osi_core.h>
#include "mgbe_mmc.h"
#include "mgbe_core.h"
#include "core_local.h"

/**
 * @brief mgbe_update_mmc_val - function to read register and return value to callee
 *
 * Algorithm: Read the registers, check for boundary, if more, reset
 *        counters else return same to caller.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] last_value: previous value of stats variable.
 * @param[in] offset: HW register offset
 *
 * @note
 *      1) MAC should be init and started. see osi_start_mac()
 *      2) osi_core->osd should be populated
 *
 * @retval 0 on MMC counters overflow
 * @retval value on current MMC counter value.
 */
static inline nveu64_t
mgbe_update_mmc_val (
  struct osi_core_priv_data  *osi_core,
  nveu64_t                   last_value,
  nveu64_t                   offset
  )
{
  nveu32_t  value = osi_readl ((nveu8_t *)osi_core->base + offset);

  return osi_update_stats_counter (last_value, value);
}

/**
 * @brief mgbe_read_mmc - To read MMC registers and ether_mmc_counter structure
 *         variable
 *
 * Algorithm: Pass register offset and old value to helper function and
 *         update structure.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note
 *      1) MAC should be init and started. see osi_start_mac()
 *      2) osi_core->osd should be populated
 */
void
mgbe_read_mmc (
  struct osi_core_priv_data *const  osi_core
  )
{
  struct osi_mmc_counters  *mmc = &osi_core->mmc;

  mmc->mmc_tx_octetcount_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_octetcount_gb,
      MMC_TXOCTETCOUNT_GB_L
      );
  mmc->mmc_tx_octetcount_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_octetcount_gb_h,
      MMC_TXOCTETCOUNT_GB_H
      );
  mmc->mmc_tx_framecount_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_framecount_gb,
      MMC_TXPACKETCOUNT_GB_L
      );
  mmc->mmc_tx_framecount_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_framecount_gb_h,
      MMC_TXPACKETCOUNT_GB_H
      );
  mmc->mmc_tx_broadcastframe_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_broadcastframe_g,
      MMC_TXBROADCASTPACKETS_G_L
      );
  mmc->mmc_tx_broadcastframe_g_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_broadcastframe_g_h,
      MMC_TXBROADCASTPACKETS_G_H
      );
  mmc->mmc_tx_multicastframe_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_multicastframe_g,
      MMC_TXMULTICASTPACKETS_G_L
      );
  mmc->mmc_tx_multicastframe_g_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_multicastframe_g_h,
      MMC_TXMULTICASTPACKETS_G_H
      );
  mmc->mmc_tx_64_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_64_octets_gb,
      MMC_TX64OCTETS_GB_L
      );
  mmc->mmc_tx_64_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_64_octets_gb_h,
      MMC_TX64OCTETS_GB_H
      );
  mmc->mmc_tx_65_to_127_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_65_to_127_octets_gb,
      MMC_TX65TO127OCTETS_GB_L
      );
  mmc->mmc_tx_65_to_127_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_65_to_127_octets_gb_h,
      MMC_TX65TO127OCTETS_GB_H
      );
  mmc->mmc_tx_128_to_255_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_128_to_255_octets_gb,
      MMC_TX128TO255OCTETS_GB_L
      );
  mmc->mmc_tx_128_to_255_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_128_to_255_octets_gb_h,
      MMC_TX128TO255OCTETS_GB_H
      );
  mmc->mmc_tx_256_to_511_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_256_to_511_octets_gb,
      MMC_TX256TO511OCTETS_GB_L
      );
  mmc->mmc_tx_256_to_511_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_256_to_511_octets_gb_h,
      MMC_TX256TO511OCTETS_GB_H
      );
  mmc->mmc_tx_512_to_1023_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_512_to_1023_octets_gb,
      MMC_TX512TO1023OCTETS_GB_L
      );
  mmc->mmc_tx_512_to_1023_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_512_to_1023_octets_gb_h,
      MMC_TX512TO1023OCTETS_GB_H
      );
  mmc->mmc_tx_1024_to_max_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_1024_to_max_octets_gb,
      MMC_TX1024TOMAXOCTETS_GB_L
      );
  mmc->mmc_tx_1024_to_max_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_1024_to_max_octets_gb_h,
      MMC_TX1024TOMAXOCTETS_GB_H
      );
  mmc->mmc_tx_unicast_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_unicast_gb,
      MMC_TXUNICASTPACKETS_GB_L
      );
  mmc->mmc_tx_unicast_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_unicast_gb_h,
      MMC_TXUNICASTPACKETS_GB_H
      );
  mmc->mmc_tx_multicast_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_multicast_gb,
      MMC_TXMULTICASTPACKETS_GB_L
      );
  mmc->mmc_tx_multicast_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_multicast_gb_h,
      MMC_TXMULTICASTPACKETS_GB_H
      );
  mmc->mmc_tx_broadcast_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_broadcast_gb,
      MMC_TXBROADCASTPACKETS_GB_L
      );
  mmc->mmc_tx_broadcast_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_broadcast_gb_h,
      MMC_TXBROADCASTPACKETS_GB_H
      );
  mmc->mmc_tx_underflow_error =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_underflow_error,
      MMC_TXUNDERFLOWERROR_L
      );
  mmc->mmc_tx_underflow_error_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_underflow_error_h,
      MMC_TXUNDERFLOWERROR_H
      );
  mmc->mmc_tx_singlecol_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_singlecol_g,
      MMC_TXSINGLECOL_G
      );
  mmc->mmc_tx_multicol_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_multicol_g,
      MMC_TXMULTICOL_G
      );
  mmc->mmc_tx_deferred =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_deferred,
      MMC_TXDEFERRED
      );
  mmc->mmc_tx_latecol =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_latecol,
      MMC_TXLATECOL
      );
  mmc->mmc_tx_exesscol =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_exesscol,
      MMC_TXEXESSCOL
      );
  mmc->mmc_tx_carrier_error =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_carrier_error,
      MMC_TXCARRIERERROR
      );
  mmc->mmc_tx_octetcount_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_octetcount_g,
      MMC_TXOCTETCOUNT_G_L
      );
  mmc->mmc_tx_octetcount_g_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_octetcount_g_h,
      MMC_TXOCTETCOUNT_G_H
      );
  mmc->mmc_tx_framecount_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_framecount_g,
      MMC_TXPACKETSCOUNT_G_L
      );
  mmc->mmc_tx_framecount_g_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_framecount_g_h,
      MMC_TXPACKETSCOUNT_G_H
      );
  mmc->mmc_tx_excessdef =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_excessdef,
      MMC_TXEXECESS_DEFERRED
      );
  mmc->mmc_tx_pause_frame =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_pause_frame,
      MMC_TXPAUSEPACKETS_L
      );
  mmc->mmc_tx_pause_frame_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_pause_frame_h,
      MMC_TXPAUSEPACKETS_H
      );
  mmc->mmc_tx_vlan_frame_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_vlan_frame_g,
      MMC_TXVLANPACKETS_G_L
      );
  mmc->mmc_tx_vlan_frame_g_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_vlan_frame_g_h,
      MMC_TXVLANPACKETS_G_H
      );
  mmc->mmc_rx_framecount_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_framecount_gb,
      MMC_RXPACKETCOUNT_GB_L
      );
  mmc->mmc_rx_framecount_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_framecount_gb_h,
      MMC_RXPACKETCOUNT_GB_H
      );
  mmc->mmc_rx_octetcount_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_octetcount_gb,
      MMC_RXOCTETCOUNT_GB_L
      );
  mmc->mmc_rx_octetcount_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_octetcount_gb_h,
      MMC_RXOCTETCOUNT_GB_H
      );
  mmc->mmc_rx_octetcount_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_octetcount_g,
      MMC_RXOCTETCOUNT_G_L
      );
  mmc->mmc_rx_octetcount_g_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_octetcount_g_h,
      MMC_RXOCTETCOUNT_G_H
      );
  mmc->mmc_rx_broadcastframe_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_broadcastframe_g,
      MMC_RXBROADCASTPACKETS_G_L
      );
  mmc->mmc_rx_broadcastframe_g_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_broadcastframe_g_h,
      MMC_RXBROADCASTPACKETS_G_H
      );
  mmc->mmc_rx_multicastframe_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_multicastframe_g,
      MMC_RXMULTICASTPACKETS_G_L
      );
  mmc->mmc_rx_multicastframe_g_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_multicastframe_g_h,
      MMC_RXMULTICASTPACKETS_G_H
      );
  mmc->mmc_rx_crc_error =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_crc_error,
      MMC_RXCRCERROR_L
      );
  mmc->mmc_rx_crc_error_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_crc_error_h,
      MMC_RXCRCERROR_H
      );
  mmc->mmc_rx_align_error =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_align_error,
      MMC_RXALIGNMENTERROR
      );
  mmc->mmc_rx_runt_error =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_runt_error,
      MMC_RXRUNTERROR
      );
  mmc->mmc_rx_jabber_error =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_jabber_error,
      MMC_RXJABBERERROR
      );
  mmc->mmc_rx_undersize_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_undersize_g,
      MMC_RXUNDERSIZE_G
      );
  mmc->mmc_rx_oversize_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_oversize_g,
      MMC_RXOVERSIZE_G
      );
  mmc->mmc_rx_64_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_64_octets_gb,
      MMC_RX64OCTETS_GB_L
      );
  mmc->mmc_rx_64_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_64_octets_gb_h,
      MMC_RX64OCTETS_GB_H
      );
  mmc->mmc_rx_65_to_127_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_65_to_127_octets_gb,
      MMC_RX65TO127OCTETS_GB_L
      );
  mmc->mmc_rx_65_to_127_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_65_to_127_octets_gb_h,
      MMC_RX65TO127OCTETS_GB_H
      );
  mmc->mmc_rx_128_to_255_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_128_to_255_octets_gb,
      MMC_RX128TO255OCTETS_GB_L
      );
  mmc->mmc_rx_128_to_255_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_128_to_255_octets_gb_h,
      MMC_RX128TO255OCTETS_GB_H
      );
  mmc->mmc_rx_256_to_511_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_256_to_511_octets_gb,
      MMC_RX256TO511OCTETS_GB_L
      );
  mmc->mmc_rx_256_to_511_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_256_to_511_octets_gb_h,
      MMC_RX256TO511OCTETS_GB_H
      );
  mmc->mmc_rx_512_to_1023_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_512_to_1023_octets_gb,
      MMC_RX512TO1023OCTETS_GB_L
      );
  mmc->mmc_rx_512_to_1023_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_512_to_1023_octets_gb_h,
      MMC_RX512TO1023OCTETS_GB_H
      );
  mmc->mmc_rx_1024_to_max_octets_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_1024_to_max_octets_gb,
      MMC_RX1024TOMAXOCTETS_GB_L
      );
  mmc->mmc_rx_1024_to_max_octets_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_1024_to_max_octets_gb_h,
      MMC_RX1024TOMAXOCTETS_GB_H
      );
  mmc->mmc_rx_unicast_g =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_unicast_g,
      MMC_RXUNICASTPACKETS_G_L
      );
  mmc->mmc_rx_unicast_g_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_unicast_g_h,
      MMC_RXUNICASTPACKETS_G_H
      );
  mmc->mmc_rx_length_error =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_length_error,
      MMC_RXLENGTHERROR_L
      );
  mmc->mmc_rx_length_error_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_length_error_h,
      MMC_RXLENGTHERROR_H
      );
  mmc->mmc_rx_outofrangetype =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_outofrangetype,
      MMC_RXOUTOFRANGETYPE_L
      );
  mmc->mmc_rx_outofrangetype_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_outofrangetype_h,
      MMC_RXOUTOFRANGETYPE_H
      );
  mmc->mmc_rx_pause_frames =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_pause_frames,
      MMC_RXPAUSEPACKETS_L
      );
  mmc->mmc_rx_pause_frames_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_pause_frames_h,
      MMC_RXPAUSEPACKETS_H
      );
  mmc->mmc_rx_fifo_overflow =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_fifo_overflow,
      MMC_RXFIFOOVERFLOW_L
      );
  mmc->mmc_rx_fifo_overflow_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_fifo_overflow_h,
      MMC_RXFIFOOVERFLOW_H
      );
  mmc->mmc_rx_vlan_frames_gb =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_vlan_frames_gb,
      MMC_RXVLANPACKETS_GB_L
      );
  mmc->mmc_rx_vlan_frames_gb_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_vlan_frames_gb_h,
      MMC_RXVLANPACKETS_GB_H
      );
  mmc->mmc_rx_watchdog_error =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_watchdog_error,
      MMC_RXWATCHDOGERROR
      );
  mmc->mmc_tx_lpi_usec_cntr =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_lpi_usec_cntr,
      MMC_TXLPIUSECCNTR
      );
  mmc->mmc_tx_lpi_tran_cntr =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_lpi_tran_cntr,
      MMC_TXLPITRANCNTR
      );
  mmc->mmc_rx_lpi_usec_cntr =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_lpi_usec_cntr,
      MMC_RXLPIUSECCNTR
      );
  mmc->mmc_rx_lpi_tran_cntr =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_lpi_tran_cntr,
      MMC_RXLPITRANCNTR
      );
  mmc->mmc_rx_ipv4_gd =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_gd,
      MMC_RXIPV4_GD_PKTS_L
      );
  mmc->mmc_rx_ipv4_gd_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_gd_h,
      MMC_RXIPV4_GD_PKTS_H
      );
  mmc->mmc_rx_ipv4_hderr =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_hderr,
      MMC_RXIPV4_HDRERR_PKTS_L
      );
  mmc->mmc_rx_ipv4_hderr_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_hderr_h,
      MMC_RXIPV4_HDRERR_PKTS_H
      );
  mmc->mmc_rx_ipv4_nopay =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_nopay,
      MMC_RXIPV4_NOPAY_PKTS_L
      );
  mmc->mmc_rx_ipv4_nopay_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_nopay_h,
      MMC_RXIPV4_NOPAY_PKTS_H
      );
  mmc->mmc_rx_ipv4_frag =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_frag,
      MMC_RXIPV4_FRAG_PKTS_L
      );
  mmc->mmc_rx_ipv4_frag_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_frag_h,
      MMC_RXIPV4_FRAG_PKTS_H
      );
  mmc->mmc_rx_ipv4_udsbl =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_udsbl,
      MMC_RXIPV4_UBSBL_PKTS_L
      );
  mmc->mmc_rx_ipv4_udsbl_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_udsbl_h,
      MMC_RXIPV4_UBSBL_PKTS_H
      );
  mmc->mmc_rx_ipv6_gd =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_gd,
      MMC_RXIPV6_GD_PKTS_L
      );
  mmc->mmc_rx_ipv6_gd_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_gd_h,
      MMC_RXIPV6_GD_PKTS_H
      );
  mmc->mmc_rx_ipv6_hderr =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_hderr,
      MMC_RXIPV6_HDRERR_PKTS_L
      );
  mmc->mmc_rx_ipv6_hderr_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_hderr_h,
      MMC_RXIPV6_HDRERR_PKTS_H
      );
  mmc->mmc_rx_ipv6_nopay =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_nopay,
      MMC_RXIPV6_NOPAY_PKTS_L
      );
  mmc->mmc_rx_ipv6_nopay_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_nopay_h,
      MMC_RXIPV6_NOPAY_PKTS_H
      );
  mmc->mmc_rx_udp_gd =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_udp_gd,
      MMC_RXUDP_GD_PKTS_L
      );
  mmc->mmc_rx_udp_gd_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_udp_gd_h,
      MMC_RXUDP_GD_PKTS_H
      );
  mmc->mmc_rx_udp_err =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_udp_err,
      MMC_RXUDP_ERR_PKTS_L
      );
  mmc->mmc_rx_udp_err_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_udp_err_h,
      MMC_RXUDP_ERR_PKTS_H
      );
  mmc->mmc_rx_tcp_gd =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_tcp_gd,
      MMC_RXTCP_GD_PKTS_L
      );
  mmc->mmc_rx_tcp_gd_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_tcp_gd_h,
      MMC_RXTCP_GD_PKTS_H
      );
  mmc->mmc_rx_tcp_err =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_tcp_err,
      MMC_RXTCP_ERR_PKTS_L
      );
  mmc->mmc_rx_tcp_err_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_tcp_err_h,
      MMC_RXTCP_ERR_PKTS_H
      );
  mmc->mmc_rx_icmp_gd =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_icmp_gd,
      MMC_RXICMP_GD_PKTS_L
      );
  mmc->mmc_rx_icmp_gd_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_icmp_gd_h,
      MMC_RXICMP_GD_PKTS_H
      );
  mmc->mmc_rx_icmp_err =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_icmp_err,
      MMC_RXICMP_ERR_PKTS_L
      );
  mmc->mmc_rx_icmp_err_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_icmp_err_h,
      MMC_RXICMP_ERR_PKTS_H
      );
  mmc->mmc_rx_ipv4_gd_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_gd_octets,
      MMC_RXIPV4_GD_OCTETS_L
      );
  mmc->mmc_rx_ipv4_gd_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_gd_octets_h,
      MMC_RXIPV4_GD_OCTETS_H
      );
  mmc->mmc_rx_ipv4_hderr_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_hderr_octets,
      MMC_RXIPV4_HDRERR_OCTETS_L
      );
  mmc->mmc_rx_ipv4_hderr_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_hderr_octets_h,
      MMC_RXIPV4_HDRERR_OCTETS_H
      );
  mmc->mmc_rx_ipv4_nopay_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_nopay_octets,
      MMC_RXIPV4_NOPAY_OCTETS_L
      );
  mmc->mmc_rx_ipv4_nopay_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_nopay_octets_h,
      MMC_RXIPV4_NOPAY_OCTETS_H
      );
  mmc->mmc_rx_ipv4_frag_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_frag_octets,
      MMC_RXIPV4_FRAG_OCTETS_L
      );
  mmc->mmc_rx_ipv4_frag_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_frag_octets_h,
      MMC_RXIPV4_FRAG_OCTETS_H
      );
  mmc->mmc_rx_ipv4_udsbl_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_udsbl_octets,
      MMC_RXIPV4_UDP_CHKSM_DIS_OCT_L
      );
  mmc->mmc_rx_ipv4_udsbl_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv4_udsbl_octets_h,
      MMC_RXIPV4_UDP_CHKSM_DIS_OCT_H
      );
  mmc->mmc_rx_udp_gd_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_udp_gd_octets,
      MMC_RXUDP_GD_OCTETS_L
      );
  mmc->mmc_rx_udp_gd_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_udp_gd_octets_h,
      MMC_RXUDP_GD_OCTETS_H
      );
  mmc->mmc_rx_ipv6_gd_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_gd_octets,
      MMC_RXIPV6_GD_OCTETS_L
      );
  mmc->mmc_rx_ipv6_gd_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_gd_octets_h,
      MMC_RXIPV6_GD_OCTETS_H
      );
  mmc->mmc_rx_ipv6_hderr_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_hderr_octets,
      MMC_RXIPV6_HDRERR_OCTETS_L
      );
  mmc->mmc_rx_ipv6_hderr_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_hderr_octets_h,
      MMC_RXIPV6_HDRERR_OCTETS_H
      );
  mmc->mmc_rx_ipv6_nopay_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_nopay_octets,
      MMC_RXIPV6_NOPAY_OCTETS_L
      );
  mmc->mmc_rx_ipv6_nopay_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_ipv6_nopay_octets_h,
      MMC_RXIPV6_NOPAY_OCTETS_H
      );
  mmc->mmc_rx_udp_err_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_udp_err_octets,
      MMC_RXUDP_ERR_OCTETS_L
      );
  mmc->mmc_rx_udp_err_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_udp_err_octets_h,
      MMC_RXUDP_ERR_OCTETS_H
      );
  mmc->mmc_rx_tcp_gd_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_tcp_gd_octets,
      MMC_RXTCP_GD_OCTETS_L
      );
  mmc->mmc_rx_tcp_gd_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_tcp_gd_octets_h,
      MMC_RXTCP_GD_OCTETS_H
      );
  mmc->mmc_rx_tcp_err_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_tcp_err_octets,
      MMC_RXTCP_ERR_OCTETS_L
      );
  mmc->mmc_rx_tcp_err_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_tcp_err_octets_h,
      MMC_RXTCP_ERR_OCTETS_H
      );
  mmc->mmc_rx_icmp_gd_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_icmp_gd_octets,
      MMC_RXICMP_GD_OCTETS_L
      );
  mmc->mmc_rx_icmp_gd_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_icmp_gd_octets_h,
      MMC_RXICMP_GD_OCTETS_H
      );
  mmc->mmc_rx_icmp_err_octets =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_icmp_err_octets,
      MMC_RXICMP_ERR_OCTETS_L
      );
  mmc->mmc_rx_icmp_err_octets_h =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_icmp_err_octets_h,
      MMC_RXICMP_ERR_OCTETS_H
      );
  mmc->mmc_tx_fpe_frag_cnt =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_fpe_frag_cnt,
      MMC_TX_FPE_FRAG_COUNTER
      );
  mmc->mmc_tx_fpe_hold_req_cnt =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_tx_fpe_hold_req_cnt,
      MMC_TX_HOLD_REQ_COUNTER
      );
  mmc->mmc_rx_packet_reass_err_cnt =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_packet_reass_err_cnt,
      MMC_RX_PKT_ASSEMBLY_ERR_CNTR
      );
  mmc->mmc_rx_packet_smd_err_cnt =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_packet_smd_err_cnt,
      MMC_RX_PKT_SMD_ERR_CNTR
      );
  mmc->mmc_rx_packet_asm_ok_cnt =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_packet_asm_ok_cnt,
      MMC_RX_PKT_ASSEMBLY_OK_CNTR
      );
  mmc->mmc_rx_fpe_fragment_cnt =
    mgbe_update_mmc_val (
      osi_core,
      mmc->mmc_rx_fpe_fragment_cnt,
      MMC_RX_FPE_FRAG_CNTR
      );
}
