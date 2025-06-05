// SPDX-License-Identifier: MIT

/* SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifdef OSI_CL_FTRACE
  #include <sys/slog.h>
#endif /* OSI_CL_FTRACE */
#include "dma_local.h"
#include <osi_dma_txrx.h>
#include "hw_desc.h"
#include "mgbe_dma.h"
#ifdef OSI_DEBUG
  #include "debug.h"
#endif /* OSI_DEBUG */

/** DMA descriptor operations */
static struct desc_ops  d_ops[OSI_MAX_MAC_IP_TYPES];

#if defined OSI_DEBUG && !defined OSI_STRIPPED_LIB
static inline void
dump_rx_descriptors (
  struct osi_dma_priv_data  *osi_dma,
  struct osi_rx_ring        *rx_ring,
  nveu32_t                  chan
  )
{
  if (osi_dma->enable_desc_dump == 1U) {
    desc_dump (
      osi_dma,
      rx_ring->cur_rx_idx,
      rx_ring->cur_rx_idx,
      RX_DESC_DUMP,
      chan
      );
  }
}

#endif

/**
 * @brief validate_rx_completions_arg- Validate input argument of rx_completions
 *
 * @note
 * Algorithm:
 *  - This routine validate input arguments to osi_process_rx_completions()
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: Rx DMA channel number
 * @param[in] more_data_avail: Pointer to more data available flag. OSI fills
 *         this flag if more rx packets available to read(1) or not(0).
 * @param[out] rx_ring: OSI DMA channel Rx ring
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
static inline nve32_t
validate_rx_completions_arg (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan,
  const nveu32_t *const     more_data_avail,
  struct osi_rx_ring        **rx_ring
  )
{
  const struct dma_local *const  l_dma = (struct dma_local *)(void *)osi_dma;
  nve32_t                        ret   = 0;

  if (osi_unlikely (
        (osi_dma == OSI_NULL) ||
        (more_data_avail == OSI_NULL) ||
        (chan >= l_dma->num_max_chans) ||
        (chan >= OSI_MGBE_MAX_NUM_CHANS)
        ))
  {
    ret = -1;
    goto fail;
  }

  *rx_ring = osi_dma->rx_ring[chan];
  if (osi_unlikely (*rx_ring == OSI_NULL)) {
    OSI_DMA_ERR (
      osi_dma->osd,
      OSI_LOG_ARG_INVALID,
      "validate_input_rx_completions: Invalid pointers\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  if (((*rx_ring)->cur_rx_idx >= osi_dma->rx_ring_sz) ||
      (osi_dma->rx_ring_sz == 0U))
  {
    OSI_DMA_ERR (
      osi_dma->osd,
      OSI_LOG_ARG_INVALID,
      "dma_txrx: Invalid cur_rx_idx or rx ring size\n",
      0ULL
      );
    ret = -1;
  }

fail:
  return ret;
}

static inline void
process_rx_desc (
  struct osi_dma_priv_data  *osi_dma,
  struct osi_rx_ring        *rx_ring,
  struct osi_rx_desc        *rx_desc,
  struct osi_rx_swcx        *rx_swcx,
  struct osi_rx_pkt_cx      *rx_pkt_cx,
  nveu32_t                  chan,
  const nveu32_t            rx_ring_mask
  )
{
  const nveu32_t      es_bits_mask[OSI_MAX_MAC_IP_TYPES] = {
    RDES3_ES_BITS, RDES3_ES_MGBE, RDES3_ES_MGBE
  };
  struct osi_rx_desc  *context_desc = OSI_NULL;
  struct osi_rx_swcx  *ptp_rx_swcx  = OSI_NULL;
  nveu32_t            ip_type       = osi_dma->mac;
  nve32_t             ret           = 0;

  if ((rx_desc->rdes3 & es_bits_mask[ip_type]) != 0U) {
    /* reset validity if any of the error bits
     * are set
     */
    rx_pkt_cx->flags &= ~OSI_PKT_CX_VALID;
 #ifndef OSI_STRIPPED_LIB
    d_ops[ip_type].update_rx_err_stats (rx_desc, &osi_dma->pkt_err_stats);
 #endif /* !OSI_STRIPPED_LIB */
  }

  /* Check if COE Rx checksum is valid */
  d_ops[ip_type].get_rx_csum (rx_desc, rx_pkt_cx);

 #ifndef OSI_STRIPPED_LIB
  /* Get Rx VLAN from descriptor */
  d_ops[ip_type].get_rx_vlan (rx_desc, rx_pkt_cx);

  /* get_rx_hash for RSS */
  d_ops[ip_type].get_rx_hash (rx_desc, rx_pkt_cx);
 #endif /* !OSI_STRIPPED_LIB */
  context_desc = rx_ring->rx_desc + rx_ring->cur_rx_idx;

  /* Get rx time stamp */
  ret = d_ops[ip_type].get_rx_hwstamp (osi_dma, rx_desc, context_desc, rx_pkt_cx);
  if (ret == 0) {
    ptp_rx_swcx = rx_ring->rx_swcx + rx_ring->cur_rx_idx;

    /* Marking software context as PTP software
     * context so that OSD can skip DMA buffer
     * allocation and DMA mapping. DMA can use PTP
     * software context addresses directly since
     * those are valid.
     */
    ptp_rx_swcx->flags |= OSI_RX_SWCX_REUSE;
 #ifdef OSI_DEBUG
    dump_rx_descriptors (osi_dma, rx_ring, chan);
 #endif /* OSI_DEBUG */

    /* Context descriptor was consumed. Its skb
     * and DMA mapping will be recycled
     */
    rx_ring->cur_rx_idx = ((rx_ring->cur_rx_idx & (nveu32_t)INT_MAX) + 1U) &
                          rx_ring_mask;
  }

  osi_dma->osd_ops.receive_packet (
                     osi_dma->osd,
                     rx_ring,
                     chan,
                     osi_dma->rx_buf_len,
                     rx_pkt_cx,
                     rx_swcx
                     );
}

#ifndef OSI_STRIPPED_LIB
static inline void
check_for_more_data_avail (
  struct osi_rx_ring  *rx_ring,
  nve32_t             received,
  nve32_t             received_resv,
  nve32_t             budget,
  nveu32_t            *more_data_avail
  )
{
  struct osi_rx_desc  *rx_desc = OSI_NULL;
  struct osi_rx_swcx  *rx_swcx = OSI_NULL;

  /* If budget is done, check if HW ring still has unprocessed
   * Rx packets, so that the OSD layer can decide to schedule
   * this function again.
   */
  if ((received_resv < 0) || (received > (INT_MAX - received_resv))) {
    return;
  }

  if ((received + received_resv) >= budget) {
    rx_desc = rx_ring->rx_desc + rx_ring->cur_rx_idx;
    rx_swcx = rx_ring->rx_swcx + rx_ring->cur_rx_idx;
    if (((rx_swcx->flags & OSI_RX_SWCX_PROCESSED) !=
         OSI_RX_SWCX_PROCESSED) &&
        ((rx_desc->rdes3 & RDES3_OWN) != RDES3_OWN))
    {
      /* Next descriptor has owned by SW
       * So set more data avail flag here.
       */
      *more_data_avail = OSI_ENABLE;
    }
  }
}

#endif /* !OSI_STRIPPED_LIB */

#ifdef OSI_CL_FTRACE
nveu32_t  osi_process_rx_completions_cnt = 0;
#endif /* OSI_CL_FTRACE */

/**
 * @brief compltd_rxdesc_cnt - number of Rx descriptors completed by HW
 *
 * @note
 * Algorithm:
 *  - This routine will be invoked by OSI layer internally to get the
 *    available Rx descriptor to process by SW.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @param[in, out] osi_dma: Pointer to OSI DMA private data structure.
 * @param[in] chan: DMA channel number for which stats should be incremented.
 */
static inline nveu32_t
compltd_rx_desc_cnt (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan
  )
{
  struct osi_rx_ring  *rx_ring = osi_dma->rx_ring[chan];
  nveu32_t            value = 0U, rx_desc_wr_idx = 0U, descr_compltd = 0U;

  /* Already has a check for this in teh caller
   * but coverity tool is not able recognize the same
   */
  const nveu32_t  local_chan = chan % OSI_MGBE_MAX_NUM_CHANS;

  value = osi_dma_readl (
            (nveu8_t *)osi_dma->base +
            MGBE_DMA_CHX_RX_DESC_WR_RNG_OFFSET (local_chan)
            );
  if (osi_dma->rx_ring_sz > 0U) {
    /* completed desc write back offset */
    rx_desc_wr_idx = ((value >> MGBE_RX_DESC_WR_RNG_RWDC_SHIFT) &
                      (osi_dma->rx_ring_sz - 1U));
    if (rx_desc_wr_idx >= rx_ring->cur_rx_idx) {
      descr_compltd = (rx_desc_wr_idx - rx_ring->cur_rx_idx) &
                      (osi_dma->rx_ring_sz - 1U);
    } else {
      descr_compltd = ((rx_desc_wr_idx + osi_dma->rx_ring_sz) -
                       rx_ring->cur_rx_idx) & (osi_dma->rx_ring_sz - 1U);
    }
  }

  /* offset/index start from 0, so add 1 to get final count */
  descr_compltd = (((descr_compltd) & ((nveu32_t)0x7FFFFFFFU)) + (1U));
  return descr_compltd;
}

static inline nve32_t
is_data_ready_to_process (
  struct osi_rx_ring  *rx_ring,
  nveu8_t             *base,
  nveu32_t            chan_num
  )
{
  const nveu32_t  dma_debug_shift[OSI_EQOS_MAX_NUM_CHANS] = {
    EQOS_DMA_DEBUG_STATUS_0_RPS0_SHIFT,
    EQOS_DMA_DEBUG_STATUS_0_RPS1_SHIFT,
    EQOS_DMA_DEBUG_STATUS_0_RPS2_SHIFT,
    EQOS_DMA_DEBUG_STATUS_1_RPS3_SHIFT,
    EQOS_DMA_DEBUG_STATUS_1_RPS4_SHIFT,
    EQOS_DMA_DEBUG_STATUS_1_RPS5_SHIFT,
    EQOS_DMA_DEBUG_STATUS_1_RPS6_SHIFT,
    EQOS_DMA_DEBUG_STATUS_2_RPS7_SHIFT
  };
  const nveu32_t  dma_debug_status[OSI_EQOS_MAX_NUM_CHANS] = {
    EQOS_DMA_DEBUG_STATUS_0,
    EQOS_DMA_DEBUG_STATUS_0,
    EQOS_DMA_DEBUG_STATUS_0,
    EQOS_DMA_DEBUG_STATUS_1,
    EQOS_DMA_DEBUG_STATUS_1,
    EQOS_DMA_DEBUG_STATUS_1,
    EQOS_DMA_DEBUG_STATUS_1,
    EQOS_DMA_DEBUG_STATUS_2
  };
  nveu64_t        sw_cur_rx_desc_phy_addr = 0UL;
  nveu64_t        hw_cur_rx_desc_phy_addr = 0UL;
  nveu32_t        chan                    = chan_num & 0xFU;
  nveu32_t        debug_status            = 0U;
  nve32_t         ret                     = 0;

  /* Get current software descriptor phyical address */
  sw_cur_rx_desc_phy_addr = rx_ring->rx_desc_phy_addr +
                            (sizeof (struct osi_rx_desc) * rx_ring->cur_rx_idx);
  sw_cur_rx_desc_phy_addr = L32 (sw_cur_rx_desc_phy_addr);
  /* Get current hardware descriptor phyical address */
  hw_cur_rx_desc_phy_addr = osi_dma_readl (base + EQOS_DMA_CHX_CARD (chan));

  /* Compare HW processing address with software processing addresss */
  if (hw_cur_rx_desc_phy_addr == sw_cur_rx_desc_phy_addr) {
    /* there may be chances that data buffer might not committed memory
     * check for DMA state - only process the pkts if DMA is idle
     */
    debug_status = osi_dma_readl (base + dma_debug_status[chan]);
    debug_status = debug_status >> (dma_debug_shift[chan] & 0x1FU);

    if ((debug_status & EQOS_DMA_DEBUG_STATUS_RPSX_MASK) >=
        EQOS_DMA_DEBUG_STATUS_RPSX_RUN_CRD)
    {
      /* DMA is not idle - its busy. Don't process the data */
      ret = -1;
    }
  }

  return ret;
}

nve32_t
osi_process_rx_completions (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan,
  nve32_t                   budget,
  nveu32_t                  *more_data_avail
  )
{
  struct osi_rx_ring    *rx_ring   = OSI_NULL;
  struct osi_rx_pkt_cx  *rx_pkt_cx = OSI_NULL;
  struct osi_rx_desc    *rx_desc   = OSI_NULL;
  struct osi_rx_swcx    *rx_swcx   = OSI_NULL;
  nveu32_t              rx_ring_sz;
  nveu32_t              rx_ring_mask;
  nve32_t               received = 0;

 #ifndef OSI_STRIPPED_LIB
  nve32_t  received_resv = 0;
 #endif /* !OSI_STRIPPED_LIB */
  nve32_t   ret = 0;
  nveu32_t  rx_desc_compltd;

 #ifdef OSI_CL_FTRACE
  if ((osi_process_rx_completions_cnt % 1000) == 0) {
    slogf (0, 2, "%s : Function Entry\n", __func__);
  }

 #endif /* OSI_CL_FTRACE */

  ret = validate_rx_completions_arg (osi_dma, chan, more_data_avail, &rx_ring);
  if (osi_unlikely (ret < 0)) {
    received = -1;
    goto fail;
  }

  rx_ring_sz   = osi_dma->rx_ring_sz;
  rx_ring_mask = rx_ring_sz - 1U;

  rx_pkt_cx = &rx_ring->rx_pkt_cx;

  /* Reset flag to indicate if more Rx frames available to OSD layer */
  *more_data_avail = OSI_NONE;

  if (osi_dma->mac == OSI_MAC_HW_MGBE_T26X) {
    rx_desc_compltd = compltd_rx_desc_cnt (osi_dma, chan);
    budget          = (budget > ((nve32_t)rx_desc_compltd) ? ((nve32_t)rx_desc_compltd) : budget);
  }

  while (  (received < budget)
 #ifndef OSI_STRIPPED_LIB
        && (received_resv < budget)
 #endif /* !OSI_STRIPPED_LIB */
           )
  {
    rx_desc = rx_ring->rx_desc + rx_ring->cur_rx_idx;

    /* check for data availability */
    if ((rx_desc->rdes3 & RDES3_OWN) == RDES3_OWN) {
      break;
    }

    if (osi_dma->mac == OSI_MAC_HW_EQOS) {
      /* check if data is ready to process */
      if (is_data_ready_to_process (
            rx_ring,
            (nveu8_t *)osi_dma->base,
            chan
            ) != 0)
      {
        /* Data is not ready not process. retry again */
        continue;
      }
    }

    rx_swcx    = rx_ring->rx_swcx + rx_ring->cur_rx_idx;
    *rx_pkt_cx = (struct osi_rx_pkt_cx) {
      0
    };
 #if defined OSI_DEBUG && !defined OSI_STRIPPED_LIB
    dump_rx_descriptors (osi_dma, rx_ring, chan);
 #endif /* OSI_DEBUG */

    INCR_RX_DESC_INDEX (rx_ring->cur_rx_idx, rx_ring_sz);

 #ifndef OSI_STRIPPED_LIB
    if (osi_unlikely (
          rx_swcx->buf_virt_addr ==
          osi_dma->resv_buf_virt_addr
          ))
    {
      rx_swcx->buf_virt_addr = OSI_NULL;
      rx_swcx->buf_phy_addr  = 0;
      /* Reservered buffer used */
      received_resv++;
      if (osi_dma->osd_ops.realloc_buf != OSI_NULL) {
        osi_dma->osd_ops.realloc_buf (
                           osi_dma->osd,
                           rx_ring,
                           chan
                           );
      }

      continue;
    }

 #endif /* !OSI_STRIPPED_LIB */

    /* packet already processed */
    if ((rx_swcx->flags & OSI_RX_SWCX_PROCESSED) ==
        OSI_RX_SWCX_PROCESSED)
    {
      break;
    }

    /* When JE is set, HW will accept any valid packet on Rx upto
     * 9K or 16K (depending on GPSCLE bit), irrespective of whether
     * MTU set is lower than these specific values. When Rx buf len
     * is allocated to be exactly same as MTU, HW will consume more
     * than 1 Rx desc. to place the larger packet and will set the
     * LD bit in RDES3 accordingly.
     * Restrict such Rx packets (which are longer than currently
     * set MTU on DUT), and pass them to driver as invalid packet
     * since HW cannot drop them.
     */
    if ((((rx_desc->rdes3 & RDES3_FD) == RDES3_FD) &&
         ((rx_desc->rdes3 & RDES3_LD) == RDES3_LD)) ==
        BOOLEAN_FALSE)
    {
      rx_pkt_cx->flags  &= ~OSI_PKT_CX_VALID;
      rx_pkt_cx->pkt_len = rx_desc->rdes3 & RDES3_PKT_LEN;
      osi_dma->osd_ops.receive_packet (
                         osi_dma->osd,
                         rx_ring,
                         chan,
                         osi_dma->rx_buf_len,
                         rx_pkt_cx,
                         rx_swcx
                         );
      continue;
    }

    /* get the length of the packet */
    rx_pkt_cx->pkt_len = rx_desc->rdes3 & RDES3_PKT_LEN;

    /* Mark pkt as valid by default */
    rx_pkt_cx->flags |= OSI_PKT_CX_VALID;

    /* Process the Rx descriptor */
    process_rx_desc (osi_dma, rx_ring, rx_desc, rx_swcx, rx_pkt_cx, chan, rx_ring_mask);

 #ifndef OSI_STRIPPED_LIB
    osi_dma->dstats.chan_rx_pkt_n[chan] =
      dma_update_stats_counter (
        osi_dma->dstats.chan_rx_pkt_n[chan],
        1UL
        );
    osi_dma->dstats.rx_pkt_n =
      dma_update_stats_counter (osi_dma->dstats.rx_pkt_n, 1UL);
 #endif /* !OSI_STRIPPED_LIB */
    received++;
  }

 #ifndef OSI_STRIPPED_LIB
  check_for_more_data_avail (rx_ring, received, received_resv, budget, more_data_avail);
 #endif /*!OSI_STRIPPED_LIB */

fail:
 #ifdef OSI_CL_FTRACE
  if ((osi_process_rx_completions_cnt++ % 1000) == 0) {
    slogf (0, 2, "%s : Function Exit\n", __func__);
  }

 #endif /* OSI_CL_FTRACE */
  return received;
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief inc_tx_pkt_stats - Increment Tx packet count Stats
 *
 * @note
 * Algorithm:
 *  - This routine will be invoked by OSI layer internally to increment
 *    stats for successfully transmitted packets on certain DMA channel.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @param[in, out] osi_dma: Pointer to OSI DMA private data structure.
 * @param[in] chan: DMA channel number for which stats should be incremented.
 */
static inline void
inc_tx_pkt_stats (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan
  )
{
  osi_dma->dstats.chan_tx_pkt_n[chan] =
    dma_update_stats_counter (osi_dma->dstats.chan_tx_pkt_n[chan], 1UL);
  osi_dma->dstats.tx_pkt_n =
    dma_update_stats_counter (osi_dma->dstats.tx_pkt_n, 1UL);
}

static inline void
update_err_stats (
  nveu32_t  error_bit,
  nveu64_t  *error_counter
  )
{
  if (error_bit) {
    *error_counter = dma_update_stats_counter (*error_counter, 1UL);
  }
}

/**
 * @brief get_tx_err_stats - Detect Errors from Tx Status
 *
 * @note
 * Algorithm:
 *  - This routine will be invoked by OSI layer itself which
 *    checks for the Last Descriptor and updates the transmit status errors
 *    accordingly.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @param[in] tx_desc: Tx Descriptor.
 * @param[in, out] pkt_err_stats: Packet error stats which stores the errors
 *  reported
 */
static inline void
get_tx_err_stats (
  struct osi_tx_desc        *tx_desc,
  struct osi_pkt_err_stats  *pkt_err_stats
  )
{
  update_err_stats (tx_desc->tdes3 & TDES3_IP_HEADER_ERR, &pkt_err_stats->ip_header_error);
  update_err_stats (tx_desc->tdes3 & TDES3_JABBER_TIMEO_ERR, &pkt_err_stats->jabber_timeout_error);
  update_err_stats (tx_desc->tdes3 & TDES3_PKT_FLUSH_ERR, &pkt_err_stats->pkt_flush_error);
  update_err_stats (tx_desc->tdes3 & TDES3_PL_CHK_SUM_ERR, &pkt_err_stats->payload_cs_error);
  update_err_stats (tx_desc->tdes3 & TDES3_LOSS_CARRIER_ERR, &pkt_err_stats->loss_of_carrier_error);
  update_err_stats (tx_desc->tdes3 & TDES3_NO_CARRIER_ERR, &pkt_err_stats->no_carrier_error);
  update_err_stats (tx_desc->tdes3 & TDES3_LATE_COL_ERR, &pkt_err_stats->late_collision_error);
  update_err_stats (tx_desc->tdes3 & TDES3_EXCESSIVE_COL_ERR, &pkt_err_stats->excessive_collision_error);
  update_err_stats (tx_desc->tdes3 & TDES3_EXCESSIVE_DEF_ERR, &pkt_err_stats->excessive_deferal_error);
  update_err_stats (tx_desc->tdes3 & TDES3_UNDER_FLOW_ERR, &pkt_err_stats->underflow_error);
}

nve32_t
osi_clear_tx_pkt_err_stats (
  struct osi_dma_priv_data  *osi_dma
  )
{
  nve32_t                   ret = -1;
  struct osi_pkt_err_stats  *pkt_err_stats;

  if (osi_dma != OSI_NULL) {
    pkt_err_stats = &osi_dma->pkt_err_stats;
    /* Reset tx packet errors */
    pkt_err_stats->ip_header_error           = 0U;
    pkt_err_stats->jabber_timeout_error      = 0U;
    pkt_err_stats->pkt_flush_error           = 0U;
    pkt_err_stats->payload_cs_error          = 0U;
    pkt_err_stats->loss_of_carrier_error     = 0U;
    pkt_err_stats->no_carrier_error          = 0U;
    pkt_err_stats->late_collision_error      = 0U;
    pkt_err_stats->excessive_collision_error = 0U;
    pkt_err_stats->excessive_deferal_error   = 0U;
    pkt_err_stats->underflow_error           = 0U;
    pkt_err_stats->clear_tx_err              =
      dma_update_stats_counter (
        pkt_err_stats->clear_tx_err,
        1UL
        );
    ret = 0;
  }

  return ret;
}

nve32_t
osi_clear_rx_pkt_err_stats (
  struct osi_dma_priv_data  *osi_dma
  )
{
  nve32_t                   ret = -1;
  struct osi_pkt_err_stats  *pkt_err_stats;

  if (osi_dma != OSI_NULL) {
    pkt_err_stats = &osi_dma->pkt_err_stats;
    /* Reset Rx packet errors */
    pkt_err_stats->rx_crc_error = 0U;
    pkt_err_stats->clear_tx_err =
      dma_update_stats_counter (
        pkt_err_stats->clear_rx_err,
        1UL
        );
    ret = 0;
  }

  return ret;
}

#endif /* !OSI_STRIPPED_LIB */

static inline void
update_tx_done_ts (
  struct osi_tx_desc        *tx_desc,
  struct osi_txdone_pkt_cx  *txdone_pkt_cx
  )
{
  nveu64_t  vartdes1;

  /* check tx tstamp status */
  if (((tx_desc->tdes3 & TDES3_LD) == TDES3_LD) &&
      ((tx_desc->tdes3 & TDES3_CTXT) != TDES3_CTXT) &&
      ((tx_desc->tdes3 & TDES3_TTSS) == TDES3_TTSS))
  {
    vartdes1 = ((nveu64_t)(tx_desc->tdes1) * OSI_NSEC_PER_SEC) &
               (nveu64_t)OSI_LLONG_MAX;
    txdone_pkt_cx->flags |= OSI_TXDONE_CX_TS;
    txdone_pkt_cx->ns     = (nveu64_t)tx_desc->tdes0 + vartdes1;
  }
}

/**
 * @brief validate_tx_completions_arg- Validate input argument of tx_completions
 *
 * @note
 * Algorithm:
 *  - This routine validate input arguments to osi_process_tx_completions()
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: Tx DMA channel number
 * @param[out] tx_ring: OSI DMA channel Rx ring
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
static inline nve32_t
validate_tx_completions_arg (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan,
  struct osi_tx_ring        **tx_ring
  )
{
  const struct dma_local *const  l_dma = (struct dma_local *)(void *)osi_dma;
  nve32_t                        ret   = 0;

  if (osi_unlikely (
        (osi_dma == OSI_NULL) ||
        (chan >= l_dma->num_max_chans)
        ))
  {
    ret = -1;
    goto fail;
  }

  *tx_ring = osi_dma->tx_ring[chan];

  if (osi_unlikely (*tx_ring == OSI_NULL)) {
    OSI_DMA_ERR (
      osi_dma->osd,
      OSI_LOG_ARG_INVALID,
      "validate_tx_completions_arg: Invalid pointers\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

fail:
  return ret;
}

/**
 * @brief is_ptp_twostep_or_slave_mode - check for dut in ptp 2step or slave
 * mode
 *
 * @param[in] ptp_flag: osi statructure variable to identify current ptp
 *                      configuration
 *
 * @retval 1 if condition is true
 * @retval 0 if condition is false.
 */
static inline nveu32_t
is_ptp_twostep_or_slave_mode (
  nveu32_t  ptp_flag
  )
{
  return (((ptp_flag & OSI_PTP_SYNC_SLAVE) == OSI_PTP_SYNC_SLAVE) ||
          ((ptp_flag & OSI_PTP_SYNC_TWOSTEP) == OSI_PTP_SYNC_TWOSTEP)) ?
         OSI_ENABLE : OSI_DISABLE;
}

static inline void
set_paged_buf_and_set_len (
  struct osi_tx_swcx        *tx_swcx,
  struct osi_txdone_pkt_cx  *txdone_pkt_cx
  )
{
  if ((tx_swcx->flags & OSI_PKT_CX_PAGED_BUF) == OSI_PKT_CX_PAGED_BUF) {
    txdone_pkt_cx->flags |= OSI_TXDONE_CX_PAGED_BUF;
  }

  /* if tx_swcx->len == -1 means this is context
   * descriptor for PTP and TSO. Here length will be reset
   * so that for PTP/TSO context descriptors length will
   * not be added to tx_bytes
   */
  if (tx_swcx->len == OSI_INVALID_VALUE) {
    tx_swcx->len = 0;
  }
}

#ifndef OSI_STRIPPED_LIB
static inline nve32_t
process_last_desc (
  struct osi_dma_priv_data  *osi_dma,
  struct osi_tx_desc        *tx_desc,
  struct osi_txdone_pkt_cx  *txdone_pkt_cx,
  nve32_t                   processed,
  nveu32_t                  chan
  )
#else
static inline nve32_t
process_last_desc (
  struct osi_dma_priv_data  *osi_dma,
  struct osi_tx_desc        *tx_desc,
  struct osi_txdone_pkt_cx  *txdone_pkt_cx,
  nve32_t                   processed
  )
#endif
{
  nve32_t  last_processed = processed;

  /* check for Last Descriptor */
  if ((tx_desc->tdes3 & TDES3_LD) == TDES3_LD) {
    if (((tx_desc->tdes3 & TDES3_ES_BITS) != 0U) &&
        (osi_dma->mac == OSI_MAC_HW_EQOS))
    {
      txdone_pkt_cx->flags |= OSI_TXDONE_CX_ERROR;
 #ifndef OSI_STRIPPED_LIB
      /* fill packet error stats */
      get_tx_err_stats (tx_desc, &osi_dma->pkt_err_stats);
 #endif /* !OSI_STRIPPED_LIB */
    } else {
 #ifndef OSI_STRIPPED_LIB
      inc_tx_pkt_stats (osi_dma, chan);
 #endif /* !OSI_STRIPPED_LIB */
    }

    if (last_processed < INT_MAX) {
      last_processed++;
    }
  }

  return last_processed;
}

#ifdef OSI_DEBUG
static inline void
dump_tx_done_desc (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  entry,
  nveu32_t                  chan
  )
{
  if (osi_dma->enable_desc_dump == 1U) {
    desc_dump (
      osi_dma,
      entry,
      entry,
      (TX_DESC_DUMP | TX_DESC_DUMP_TX_DONE),
      chan
      );
  }
}

#endif

#ifdef OSI_CL_FTRACE
nveu32_t  osi_process_tx_completions_cnt = 0;
#endif /* OSI_CL_FTRACE */
nve32_t
osi_process_tx_completions (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan,
  nve32_t                   budget
  )
{
  struct osi_tx_ring        *tx_ring       = OSI_NULL;
  struct osi_txdone_pkt_cx  *txdone_pkt_cx = OSI_NULL;
  struct osi_tx_swcx        *tx_swcx       = OSI_NULL;
  struct osi_tx_desc        *tx_desc       = OSI_NULL;
  nveu32_t                  entry          = 0U;
  nve32_t                   processed      = 0;
  nve32_t                   ret;

 #ifdef OSI_CL_FTRACE
  if ((osi_process_tx_completions_cnt % 1000) == 0) {
    slogf (0, 2, "%s : Function Entry\n", __func__);
  }

 #endif /* OSI_CL_FTRACE */

  ret = validate_tx_completions_arg (osi_dma, chan, &tx_ring);
  if (osi_unlikely (ret < 0)) {
    processed = -1;
    goto fail;
  }

  txdone_pkt_cx = &tx_ring->txdone_pkt_cx;
  entry         = tx_ring->clean_idx;

 #ifndef OSI_STRIPPED_LIB
  osi_dma->dstats.tx_clean_n[chan] =
    dma_update_stats_counter (osi_dma->dstats.tx_clean_n[chan], 1U);
 #endif /* !OSI_STRIPPED_LIB */
  while ((entry != tx_ring->cur_tx_idx) && (entry < osi_dma->tx_ring_sz) &&
         (processed < budget))
  {
    *txdone_pkt_cx = (struct osi_txdone_pkt_cx) {
      0
    };

    tx_desc = tx_ring->tx_desc + entry;
    tx_swcx = tx_ring->tx_swcx + entry;

    if ((tx_desc->tdes3 & TDES3_OWN) == TDES3_OWN) {
      break;
    }

 #ifdef OSI_DEBUG
    dump_tx_done_desc (osi_dma, entry, chan);
 #endif /* OSI_DEBUG */

 #ifndef OSI_STRIPPED_LIB
    processed = process_last_desc (osi_dma, tx_desc, txdone_pkt_cx, processed, chan);
 #else
    processed = process_last_desc (osi_dma, tx_desc, txdone_pkt_cx, processed);
 #endif

    if (osi_dma->mac == OSI_MAC_HW_EQOS) {
      update_tx_done_ts (tx_desc, txdone_pkt_cx);
    } else if (((tx_swcx->flags & OSI_PKT_CX_PTP) == OSI_PKT_CX_PTP) &&
               // if not master in onestep mode
               /* TODO: Is this check needed and can be removed ? */
               (is_ptp_twostep_or_slave_mode (osi_dma->ptp_flag) ==
                OSI_ENABLE) &&
               ((tx_desc->tdes3 & TDES3_CTXT) == 0U))
    {
      txdone_pkt_cx->pktid = tx_swcx->pktid;
      if (osi_dma->mac == OSI_MAC_HW_MGBE_T26X) {
        txdone_pkt_cx->vdmaid = tx_swcx->vdmaid;
      }

      txdone_pkt_cx->flags |= OSI_TXDONE_CX_TS_DELAYED;
    } else {
      /* Do nothing here */
    }

    set_paged_buf_and_set_len (tx_swcx, txdone_pkt_cx);
    osi_dma->osd_ops.transmit_complete (osi_dma->osd, tx_swcx, txdone_pkt_cx);

    tx_desc->tdes3 = 0;
    tx_desc->tdes2 = 0;
    tx_desc->tdes1 = 0;
    tx_desc->tdes0 = 0;
    tx_swcx->len   = 0;

    tx_swcx->buf_virt_addr = OSI_NULL;
    tx_swcx->buf_phy_addr  = 0;
    tx_swcx->flags         = 0;
    tx_swcx->data_idx      = 0;
    INCR_TX_DESC_INDEX (entry, osi_dma->tx_ring_sz);

    /* Don't wait to update tx_ring->clean-idx. It will
     * be used by OSD layer to determine the num. of available
     * descriptors in the ring, which will in turn be used to
     * wake the corresponding transmit queue in OS layer.
     */
    tx_ring->clean_idx = entry;
  }

fail:
 #ifdef OSI_CL_FTRACE
  if ((osi_process_tx_completions_cnt++ % 1000) == 0) {
    slogf (0, 2, "%s : Function Exit\n", __func__);
  }

 #endif /* OSI_CL_FTRACE */
  return processed;
}

/**
 * @brief need_cntx_desc - Helper function to check if context desc is needed.
 *
 * @note
 * Algorithm:
 *  - Check if transmit packet context flags are set
 *  - If set, set the context descriptor bit along
 *    with other context information in the transmit descriptor.
 *
 * @param[in, out] tx_pkt_cx: Pointer to transmit packet context structure
 * @param[in, out] tx_swcx: Pointer to transmit sw packet context structure
 * @param[in, out] tx_desc: Pointer to transmit descriptor to be filled.
 * @param[in] ptp_sync_flag: PTP sync mode to identify.
 * @param[in] mac: HW MAC ver
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 - cntx desc not used
 * @retval 1 - cntx desc used.
 */
static inline nve32_t
need_cntx_desc (
  const struct osi_tx_pkt_cx *const  tx_pkt_cx,
  struct osi_tx_swcx                 *tx_swcx,
  struct osi_tx_desc                 *tx_desc,
  nveu32_t                           ptp_sync_flag,
  nveu32_t                           mac
  )
{
  nve32_t  ret = 0;

  if ((tx_pkt_cx->flags & (OSI_PKT_CX_VLAN | OSI_PKT_CX_TSO | OSI_PKT_CX_PTP)) != 0U) {
    if ((tx_pkt_cx->flags & OSI_PKT_CX_VLAN) == OSI_PKT_CX_VLAN) {
      /* Set context type */
      tx_desc->tdes3 |= TDES3_CTXT;
      /* Fill VLAN Tag ID */
      tx_desc->tdes3 |= tx_pkt_cx->vtag_id;
      /* Set VLAN TAG Valid */
      tx_desc->tdes3 |= TDES3_VLTV;

      if (tx_swcx->len == OSI_INVALID_VALUE) {
        tx_swcx->len = NV_VLAN_HLEN;
      }

      ret = 1;
    }

    if ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO) {
      /* Set context type */
      tx_desc->tdes3 |= TDES3_CTXT;
      /* Fill MSS */
      tx_desc->tdes2 |= tx_pkt_cx->mss;
      /* Set MSS valid */
      tx_desc->tdes3 |= TDES3_TCMSSV;
      ret             = 1;
    }

    /* This part of code must be at the end of function */
    if ((tx_pkt_cx->flags & OSI_PKT_CX_PTP) == OSI_PKT_CX_PTP) {
      if (((mac == OSI_MAC_HW_EQOS) &&
           ((ptp_sync_flag & OSI_PTP_SYNC_TWOSTEP) == OSI_PTP_SYNC_TWOSTEP)))
      {
        /* Doing nothing */
      } else {
        /* Set context type */
        tx_desc->tdes3 |= TDES3_CTXT;
        /* in case of One-step sync */
        if ((ptp_sync_flag & OSI_PTP_SYNC_ONESTEP) ==
            OSI_PTP_SYNC_ONESTEP)
        {
          /* Set TDES3_OSTC */
          tx_desc->tdes3 |= TDES3_OSTC;
          tx_desc->tdes3 &= ~TDES3_TCMSSV;
        }

        ret = 1;
      }
    }
  }

  return ret;
}

/**
 * @brief is_ptp_onestep_and_master_mode - check for dut is in master and
 * onestep mode
 *
 * @param[in] ptp_flag: osi statructure variable to identify current ptp
 *                      configuration
 *
 * @retval 1 if condition is true
 * @retval 0 if condition is false.
 */
static inline nveu32_t
is_ptp_onestep_and_master_mode (
  nveu32_t  ptp_flag
  )
{
  return (((ptp_flag & OSI_PTP_SYNC_MASTER) == OSI_PTP_SYNC_MASTER) &&
          ((ptp_flag & OSI_PTP_SYNC_ONESTEP) == OSI_PTP_SYNC_ONESTEP)) ?
         OSI_ENABLE : OSI_DISABLE;
}

/**
 * @brief fill_first_desc - Helper function to fill the first transmit
 *      descriptor.
 *
 * @note
 * Algorithm:
 *  - Update the buffer address and length of buffer in first desc.
 *  - Check if any features like HW checksum offload, TSO, VLAN insertion
 *    etc. are flagged in transmit packet context. If so, set the fields in
 *    first desc corresponding to those features.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @param[in, out] tx_ring: DMA channel TX ring.
 * @param[in, out] tx_pkt_cx: Pointer to transmit packet context structure
 * @param[in, out] tx_desc: Pointer to transmit descriptor to be filled.
 * @param[in] tx_swcx: Pointer to corresponding tx descriptor software context.
 * @param[in] ptp_flag: osi statructure variable to identify current ptp
 *                      configuration
 */
#ifndef OSI_STRIPPED_LIB
static inline void
fill_first_desc (
  struct osi_tx_ring    *tx_ring,
  struct osi_tx_pkt_cx  *tx_pkt_cx,
  struct osi_tx_desc    *tx_desc,
  struct osi_tx_swcx    *tx_swcx,
  nveu32_t              ptp_flag
  )
#else
static inline void
fill_first_desc (
  OSI_UNUSED struct osi_tx_ring  *tx_ring,
  struct osi_tx_pkt_cx           *tx_pkt_cx,
  struct osi_tx_desc             *tx_desc,
  struct osi_tx_swcx             *tx_swcx,
  nveu32_t                       ptp_flag
  )
#endif /* !OSI_STRIPPED_LIB */
{
 #ifdef OSI_STRIPPED_LIB
  (void)tx_ring;       // unused
 #endif /* !OSI_STRIPPED_LIB */

  tx_desc->tdes0 = L32 (tx_swcx->buf_phy_addr);
  tx_desc->tdes1 = H32 (tx_swcx->buf_phy_addr);
  tx_desc->tdes2 = tx_swcx->len;
  /* Mark it as First descriptor */
  tx_desc->tdes3 |= TDES3_FD;

  /* If HW checksum offload enabled, mark CIC bits of FD */
  if ((tx_pkt_cx->flags & OSI_PKT_CX_CSUM) == OSI_PKT_CX_CSUM) {
    tx_desc->tdes3 |= TDES3_HW_CIC_ALL;
  } else {
    if ((tx_pkt_cx->flags & OSI_PKT_CX_IP_CSUM) ==
        OSI_PKT_CX_IP_CSUM)
    {
      /* If IP only Checksum enabled, mark fist bit of CIC */
      tx_desc->tdes3 |= TDES3_HW_CIC_IP_ONLY;
    }
  }

  /* Enable VTIR in normal descriptor for VLAN packet */
  if ((tx_pkt_cx->flags & OSI_PKT_CX_VLAN) == OSI_PKT_CX_VLAN) {
    tx_desc->tdes2 |= TDES2_VTIR;
  }

  /* if TS is set enable timestamping */
  if ((tx_pkt_cx->flags & OSI_PKT_CX_PTP) == OSI_PKT_CX_PTP) {
    tx_desc->tdes2 |= TDES2_TTSE;
    // ptp master mode in one step sync
    if (is_ptp_onestep_and_master_mode (ptp_flag) ==
        OSI_ENABLE)
    {
      tx_desc->tdes2 &= ~TDES2_TTSE;
    }
  }

  /* if LEN bit is set, update packet payload len */
  if ((tx_pkt_cx->flags & OSI_PKT_CX_LEN) == OSI_PKT_CX_LEN) {
    /* Update packet len in desc */
    tx_desc->tdes3 |= tx_pkt_cx->payload_len;
  }

  /* Enable TSE bit and update TCP hdr, payload len */
  if ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO) {
    tx_desc->tdes3 |= TDES3_TSE;

    /* Minimum value for THL field is 5 for TSO
     * So divide L4 hdr len by 4
     * Typical TCP hdr len = 20B / 4 = 5
     */
    tx_pkt_cx->tcp_udp_hdrlen /= OSI_TSO_HDR_LEN_DIVISOR;

    /* Update hdr len in desc */
    tx_desc->tdes3 |= (tx_pkt_cx->tcp_udp_hdrlen <<
                       TDES3_THL_SHIFT);

    /* Update TCP payload len in desc */
    tx_desc->tdes3 &= ~TDES3_TPL_MASK;
    tx_desc->tdes3 |= tx_pkt_cx->payload_len;
  } else {
 #ifndef OSI_STRIPPED_LIB
    if ((tx_ring->slot_check == OSI_ENABLE) &&
        (tx_ring->slot_number < OSI_SLOT_NUM_MAX))
    {
      /* Fill Slot number */
      tx_desc->tdes3 |= (tx_ring->slot_number <<
                         TDES3_THL_SHIFT);
      tx_ring->slot_number = ((tx_ring->slot_number + 1U) %
                              OSI_SLOT_NUM_MAX);
    }

 #endif /* !OSI_STRIPPED_LIB */
  }
}

/**
 * @brief dmb_oshst() - Data memory barrier operation that waits only
 * for stores to complete, and only to the outer shareable domain.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void
dmb_oshst (
  void
  )
{
  __sync_synchronize ();
}

/**
 * @brief validate_ctx - validate inputs form tx_pkt_cx
 *
 * @note
 * Algorithm:
 *      - Validate tx_pkt_cx with expected value
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @param[in] osi_dma:  OSI private data structure.
 * @param[in] tx_pkt_cx: Pointer to transmit packet context structure
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t
validate_ctx (
  const struct osi_dma_priv_data *const  osi_dma,
  const struct osi_tx_pkt_cx *const      tx_pkt_cx
  )
{
  nve32_t  ret = 0;

  (void)osi_dma;
  if ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO) {
    if (osi_unlikely (
          (tx_pkt_cx->tcp_udp_hdrlen /
           OSI_TSO_HDR_LEN_DIVISOR) > TDES3_THL_MASK
          ))
    {
      OSI_DMA_ERR (
        osi_dma->osd,
        OSI_LOG_ARG_INVALID,
        "dma_txrx: Invalid TSO header len\n",
        (nveul64_t)tx_pkt_cx->tcp_udp_hdrlen
        );
      ret = -1;
      goto fail;
    } else if (osi_unlikely (
                 tx_pkt_cx->payload_len >
                 TDES3_TPL_MASK
                 ))
    {
      OSI_DMA_ERR (
        osi_dma->osd,
        OSI_LOG_ARG_INVALID,
        "dma_txrx: Invalid TSO payload len\n",
        (nveul64_t)tx_pkt_cx->payload_len
        );
      ret = -1;
      goto fail;
    } else if (osi_unlikely (tx_pkt_cx->mss > TDES2_MSS_MASK)) {
      OSI_DMA_ERR (
        osi_dma->osd,
        OSI_LOG_ARG_INVALID,
        "dma_txrx: Invalid MSS\n",
        (nveul64_t)tx_pkt_cx->mss
        );
      ret = -1;
      goto fail;
    } else {
      /* empty statement */
    }
  } else if ((tx_pkt_cx->flags & OSI_PKT_CX_LEN) == OSI_PKT_CX_LEN) {
    if (osi_unlikely (tx_pkt_cx->payload_len > TDES3_PL_MASK)) {
      OSI_DMA_ERR (
        osi_dma->osd,
        OSI_LOG_ARG_INVALID,
        "dma_txrx: Invalid frame len\n",
        (nveul64_t)tx_pkt_cx->payload_len
        );
      ret = -1;
      goto fail;
    }
  } else {
    /* empty statement */
  }

  if (osi_unlikely (tx_pkt_cx->vtag_id > TDES3_VT_MASK)) {
    OSI_DMA_ERR (
      osi_dma->osd,
      OSI_LOG_ARG_INVALID,
      "dma_txrx: Invalid VTAG_ID\n",
      (nveul64_t)tx_pkt_cx->vtag_id
      );
    ret = -1;
  }

fail:
  return ret;
}

#ifndef OSI_STRIPPED_LIB
static inline void
updata_tx_pkt_stats (
  struct osi_tx_pkt_cx      *tx_pkt_cx,
  struct osi_dma_priv_data  *osi_dma
  )
{
  /* Context descriptor for VLAN/TSO */
  if ((tx_pkt_cx->flags & OSI_PKT_CX_VLAN) == OSI_PKT_CX_VLAN) {
    osi_dma->dstats.tx_vlan_pkt_n =
      dma_update_stats_counter (osi_dma->dstats.tx_vlan_pkt_n, 1UL);
  }

  if ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO) {
    osi_dma->dstats.tx_tso_pkt_n =
      dma_update_stats_counter (osi_dma->dstats.tx_tso_pkt_n, 1UL);
  }
}

#endif /* !OSI_STRIPPED_LIB */

static inline void
update_frame_cnt (
  struct osi_dma_priv_data  *osi_dma,
  struct osi_tx_ring        *tx_ring
  )
{
  if (tx_ring->frame_cnt < UINT_MAX) {
    tx_ring->frame_cnt++;
  } else if ((osi_dma->use_tx_frames == OSI_ENABLE) &&
             ((tx_ring->frame_cnt % osi_dma->tx_frames) < UINT_MAX))
  {
    /* make sure count for tx_frame interrupt logic is retained */
    tx_ring->frame_cnt = (tx_ring->frame_cnt % osi_dma->tx_frames) + 1U;
  } else {
    tx_ring->frame_cnt = 1U;
  }
}

static inline void
apply_write_barrier (
  struct osi_tx_ring  *tx_ring
  )
{
  /*
   * We need to make sure Tx descriptor updated above is really updated
   * before setting up the DMA, hence add memory write barrier here.
   */
  if (tx_ring->skip_dmb == 0U) {
    dmb_oshst ();
  }
}

#ifdef OSI_DEBUG
static inline void
dump_tx_descriptors (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  f_idx,
  nveu32_t                  l_idx,
  nveu32_t                  chan
  )
{
  if ((osi_dma->enable_desc_dump == 1U) && (l_idx != 0U)) {
    desc_dump (
      osi_dma,
      f_idx,
      DECR_TX_DESC_INDEX (l_idx, osi_dma->tx_ring_sz),
      (TX_DESC_DUMP | TX_DESC_DUMP_TX),
      chan
      );
  }
}

#endif

static inline void
set_clear_ioc_for_last_desc (
  struct osi_dma_priv_data  *osi_dma,
  struct osi_tx_ring        *tx_ring,
  struct osi_tx_desc        *last_desc,
  struct osi_tx_pkt_cx      *tx_pkt_cx
  )
{
  /* clear IOC bit if tx SW timer based coalescing is enabled */
  if (osi_dma->use_tx_usecs == OSI_ENABLE) {
    last_desc->tdes2 &= ~TDES2_IOC;

    /* update IOC bit if tx_frames is enabled. Tx_frames
     * can be enabled only along with tx_usecs.
     */
    if (osi_dma->use_tx_frames == OSI_ENABLE) {
      if ((tx_ring->frame_cnt % osi_dma->tx_frames) == OSI_NONE) {
        last_desc->tdes2 |= TDES2_IOC;
      }
    } else if (osi_dma->use_tx_descs == OSI_ENABLE) {
      /* Add unlikely to reduce the branch mispredictions for regular data path pkts. */
      if (tx_ring->desc_cnt >= osi_dma->intr_desc_count) {
        last_desc->tdes2 |= TDES2_IOC;
        tx_ring->desc_cnt = tx_ring->desc_cnt % osi_dma->intr_desc_count;
      } else if (osi_unlikely ((tx_pkt_cx->flags & OSI_PKT_CX_PTP) == OSI_PKT_CX_PTP)) {
        last_desc->tdes2 |= TDES2_IOC;
        tx_ring->desc_cnt = 0;
      }
    }
  }
}

static inline void
set_swcx_pkt_id_for_ptp (
  struct osi_dma_priv_data  *osi_dma,
  struct osi_tx_pkt_cx      *tx_pkt_cx,
  struct osi_tx_swcx        *last_swcx,
  nveu32_t                  pkt_id,
  nveu32_t                  vdma_id
  )
{
  if (((tx_pkt_cx->flags & OSI_PKT_CX_PTP) == OSI_PKT_CX_PTP) &&
      (osi_dma->mac > OSI_MAC_HW_EQOS))
  {
    last_swcx->flags |= OSI_PKT_CX_PTP;
    last_swcx->pktid  = pkt_id;
    if (osi_dma->mac == OSI_MAC_HW_MGBE_T26X) {
      last_swcx->vdmaid = vdma_id;
    }
  }
}

static inline void
set_context_desc_own_bit (
  struct osi_tx_desc  *cx_desc,
  nve32_t             cntx_desc_consumed
  )
{
  if (cntx_desc_consumed == 1) {
    cx_desc->tdes3 |= TDES3_OWN;
  }
}

nve32_t
hw_transmit (
  struct osi_dma_priv_data  *osi_dma,
  struct osi_tx_ring        *tx_ring,
  nveu32_t                  dma_chan
  )
{
  const nveu32_t        chan_mask[OSI_MAX_MAC_IP_TYPES] = { 0xFU, 0xFU, 0x3FU };
  struct dma_local      *l_dma                          = (struct dma_local *)(void *)osi_dma;
  struct osi_tx_pkt_cx  *tx_pkt_cx                      = OSI_NULL;
  struct osi_tx_desc    *first_desc                     = OSI_NULL;
  struct osi_tx_desc    *last_desc                      = OSI_NULL;
  struct osi_tx_swcx    *last_swcx                      = OSI_NULL;
  struct osi_tx_desc    *tx_desc                        = OSI_NULL;
  struct osi_tx_swcx    *tx_swcx                        = OSI_NULL;
  struct osi_tx_desc    *cx_desc                        = OSI_NULL;

 #ifdef OSI_DEBUG
  nveu32_t  f_idx = tx_ring->cur_tx_idx;
 #endif /* OSI_DEBUG */
  const nveu32_t  local_mac = osi_dma->mac % OSI_MAX_MAC_IP_TYPES;
  // Added bitwise with 0xFF to avoid CERT INT30-C error
  nveu32_t        chan                               = ((dma_chan & chan_mask[local_mac]) & (0xFFU));
  const nveu32_t  tail_ptr_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_DMA_CHX_TDTP (chan),
    MGBE_DMA_CHX_TDTLP (chan),
    MGBE_DMA_CHX_TDTLP (chan)
  };
  nve32_t         cntx_desc_consumed;
  nveu32_t        pkt_id   = 0x0U;
  nveu32_t        vdma_id  = 0x0U;
  nveu32_t        desc_cnt = 0U;
  nveu64_t        tailptr;
  nveu32_t        entry = 0U;
  nve32_t         ret   = 0;
  nveu32_t        i;

  entry = tx_ring->cur_tx_idx;
  if (entry >= osi_dma->tx_ring_sz) {
    OSI_DMA_ERR (
      osi_dma->osd,
      OSI_LOG_ARG_INVALID,
      "dma_txrx: Invalid cur_tx_idx\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  tx_desc   = tx_ring->tx_desc + entry;
  tx_swcx   = tx_ring->tx_swcx + entry;
  tx_pkt_cx = &tx_ring->tx_pkt_cx;

  desc_cnt = tx_pkt_cx->desc_cnt;
  if (osi_unlikely (desc_cnt == 0U)) {
    /* Will not hit this case */
    OSI_DMA_ERR (
      osi_dma->osd,
      OSI_LOG_ARG_INVALID,
      "dma_txrx: Invalid desc_cnt\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  if (validate_ctx (osi_dma, tx_pkt_cx) < 0) {
    ret = -1;
    goto fail;
  }

 #ifndef OSI_STRIPPED_LIB
  updata_tx_pkt_stats (tx_pkt_cx, osi_dma);
 #endif /* !OSI_STRIPPED_LIB */

  cntx_desc_consumed = need_cntx_desc (
                         tx_pkt_cx,
                         tx_swcx,
                         tx_desc,
                         osi_dma->ptp_flag,
                         osi_dma->mac
                         );
  if (cntx_desc_consumed == 1) {
    if (((tx_pkt_cx->flags & OSI_PKT_CX_PTP) == OSI_PKT_CX_PTP) &&
        (osi_dma->mac > OSI_MAC_HW_EQOS))
    {
      /* mark packet id valid */
      tx_desc->tdes3 |= TDES3_PIDV;
      if ((osi_dma->ptp_flag & OSI_PTP_SYNC_ONESTEP) ==
          OSI_PTP_SYNC_ONESTEP)
      {
        /* packet ID for Onestep is 0x0 always */
        pkt_id = OSI_NONE;
      } else {
        INC_TX_TS_PKTID (l_dma->pkt_id);
        if (osi_dma->mac != OSI_MAC_HW_MGBE_T26X) {
          pkt_id = GET_TX_TS_PKTID (l_dma->pkt_id, chan);
        } else {
          pkt_id         = GET_TX_TS_PKTID_T264 (l_dma->pkt_id);
          vdma_id        = chan;
          tx_desc->tdes0 = (vdma_id << OSI_PTP_VDMA_SHIFT);
        }
      }

      /* update packet id */
      tx_desc->tdes0 |= pkt_id;
    }

    INCR_TX_DESC_INDEX (entry, osi_dma->tx_ring_sz);

    /* Storing context descriptor to set DMA_OWN at last */
    cx_desc = tx_desc;
    tx_desc = tx_ring->tx_desc + entry;
    tx_swcx = tx_ring->tx_swcx + entry;

    desc_cnt--;
  }

  /* Fill first descriptor */
  fill_first_desc (tx_ring, tx_pkt_cx, tx_desc, tx_swcx, osi_dma->ptp_flag);

  INCR_TX_DESC_INDEX (entry, osi_dma->tx_ring_sz);

  first_desc = tx_desc;
  last_desc  = tx_desc;
  last_swcx  = tx_swcx;

  tx_desc = tx_ring->tx_desc + entry;
  tx_swcx = tx_ring->tx_swcx + entry;
  desc_cnt--;

  /* Fill remaining descriptors */
  for (i = 0; i < desc_cnt; i++) {
    /* Increase the desc count for first descriptor */
    if (tx_ring->desc_cnt == UINT_MAX) {
      tx_ring->desc_cnt = 0U;
    }

    tx_ring->desc_cnt++;

    tx_desc->tdes0 = L32 (tx_swcx->buf_phy_addr);
    tx_desc->tdes1 = H32 (tx_swcx->buf_phy_addr);
    tx_desc->tdes2 = tx_swcx->len;
    /* set HW OWN bit for descriptor*/
    tx_desc->tdes3 |= TDES3_OWN;

    INCR_TX_DESC_INDEX (entry, osi_dma->tx_ring_sz);
    last_desc = tx_desc;
    last_swcx = tx_swcx;
    tx_desc   = tx_ring->tx_desc + entry;
    tx_swcx   = tx_ring->tx_swcx + entry;
  }

  if (tx_ring->desc_cnt == UINT_MAX) {
    tx_ring->desc_cnt = 0U;
  }

  /* Mark it as LAST descriptor */
  last_desc->tdes3 |= TDES3_LD;

  set_swcx_pkt_id_for_ptp (osi_dma, tx_pkt_cx, last_swcx, pkt_id, vdma_id);

  /* set Interrupt on Completion*/
  last_desc->tdes2 |= TDES2_IOC;

  update_frame_cnt (osi_dma, tx_ring);
  tx_ring->desc_cnt++;

  set_clear_ioc_for_last_desc (osi_dma, tx_ring, last_desc, tx_pkt_cx);

  /* Set OWN bit for first and context descriptors
   * at the end to avoid race condition
   */
  first_desc->tdes3 |= TDES3_OWN;
  set_context_desc_own_bit (cx_desc, cntx_desc_consumed);

  apply_write_barrier (tx_ring);

 #ifdef OSI_DEBUG
  dump_tx_descriptors (osi_dma, f_idx, entry, chan);
 #endif /* OSI_DEBUG */

  tailptr = tx_ring->tx_desc_phy_addr +
            (entry * sizeof (struct osi_tx_desc));
  if (osi_unlikely (tailptr < tx_ring->tx_desc_phy_addr)) {
    /* Will not hit this case */
    OSI_DMA_ERR (
      osi_dma->osd,
      OSI_LOG_ARG_INVALID,
      "dma_txrx: Invalid tx_desc_phy_addr\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  /*
   * Updating cur_tx_idx allows tx completion thread to read first_desc.
   * Hence cur_tx_idx should be updated after memory barrier.
   */
  tx_ring->cur_tx_idx = entry;

  /* Update the Tx tail pointer */
  osi_dma_writel (L32 (tailptr), (nveu8_t *)osi_dma->base + tail_ptr_reg[local_mac]);

fail:
  return ret;
}

/**
 * @brief rx_dma_desc_initialization - Initialize DMA Receive descriptors for Rx
 *
 * @note
 * Algorithm:
 *  - Initialize Receive descriptors with DMA mappable buffers,
 *    set OWN bit, Rx ring length and set starting address of Rx DMA channel.
 *    Tx ring base address in Tx DMA registers.
 *
 * @param[in, out] osi_dma:     OSI private data structure.
 * @param[in] dma_chan: Rx channel number.
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
static nve32_t
rx_dma_desc_initialization (
  const struct osi_dma_priv_data *const  osi_dma,
  nveu32_t                               dma_chan
  )
{
  const nveu32_t  chan_mask[OSI_MAX_MAC_IP_TYPES] = { 0xFU, 0xFU, 0x3FU };
  const nveu32_t  local_mac                       = osi_dma->mac % OSI_MAX_MAC_IP_TYPES;
  // Added bitwise with 0xFF to avoid CERT INT30-C error
  nveu32_t            chan                                      = ((dma_chan & chan_mask[local_mac]) & (0xFFU));
  const nveu32_t      start_addr_high_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_DMA_CHX_RDLH (chan),
    MGBE_DMA_CHX_RDLH (chan),
    MGBE_DMA_CHX_RDLH (chan)
  };
  const nveu32_t      start_addr_low_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_DMA_CHX_RDLA (chan),
    MGBE_DMA_CHX_RDLA (chan),
    MGBE_DMA_CHX_RDLA (chan)
  };
  const nveu32_t      ring_len_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_DMA_CHX_RDRL (chan),
    MGBE_DMA_CHX_RX_CNTRL2 (chan),
    MGBE_DMA_CHX_RX_CNTRL2 (chan)
  };
  const nveu32_t      mask[OSI_MAX_MAC_IP_TYPES] = { 0x3FFU, 0x3FFFU, 0x3FFFU };
  struct osi_rx_ring  *rx_ring                   = OSI_NULL;
  struct osi_rx_desc  *rx_desc                   = OSI_NULL;
  struct osi_rx_swcx  *rx_swcx                   = OSI_NULL;
  nveu64_t            tailptr                    = 0;
  nve32_t             ret                        = 0;
  nveu32_t            val;
  nveu32_t            i;

  rx_ring = osi_dma->rx_ring[chan];
  if (osi_unlikely (rx_ring == OSI_NULL)) {
    OSI_DMA_ERR (
      osi_dma->osd,
      OSI_LOG_ARG_INVALID,
      "dma_txrx: Invalid argument\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  rx_ring->cur_rx_idx = 0;
  rx_ring->refill_idx = 0;

  for (i = 0; i < osi_dma->rx_ring_sz; i++) {
    rx_swcx = rx_ring->rx_swcx + i;
    rx_desc = rx_ring->rx_desc + i;

    /* Zero initialize the descriptors first */
    rx_desc->rdes0 = 0;
    rx_desc->rdes1 = 0;
    rx_desc->rdes2 = 0;
    rx_desc->rdes3 = 0;

    rx_desc->rdes0 = L32 (rx_swcx->buf_phy_addr);
    rx_desc->rdes1 = H32 (rx_swcx->buf_phy_addr);
    rx_desc->rdes2 = 0;
    rx_desc->rdes3 = RDES3_IOC;

    if (osi_dma->mac == OSI_MAC_HW_EQOS) {
      rx_desc->rdes3 |= RDES3_B1V;
    }

    /* reconfigure INTE bit if RX watchdog timer is enabled */
    if (osi_dma->use_riwt == OSI_ENABLE) {
      rx_desc->rdes3 &= ~RDES3_IOC;
      if (osi_dma->use_rx_frames == OSI_ENABLE) {
        if ((i % osi_dma->rx_frames) == OSI_NONE) {
          /* update IOC bit if rx_frames is
           * enabled. Rx_frames can be enabled
           * only along with RWIT.
           */
          rx_desc->rdes3 |= RDES3_IOC;
        }
      }
    }

    rx_desc->rdes3 |= RDES3_OWN;

    rx_swcx->flags = 0;
  }

  tailptr = rx_ring->rx_desc_phy_addr +
            (sizeof (struct osi_rx_desc) * (osi_dma->rx_ring_sz));

  if (osi_unlikely ((tailptr < rx_ring->rx_desc_phy_addr))) {
    /* Will not hit this case */
    OSI_DMA_ERR (
      osi_dma->osd,
      OSI_LOG_ARG_INVALID,
      "dma_txrx: Invalid phys address\n",
      0ULL
      );
    ret = -1;
    goto fail;
  }

  /* Update the HW DMA ring length */
  val = (osi_dma->rx_ring_sz - 1U) & mask[osi_dma->mac];
  osi_dma_writel (val, (nveu8_t *)osi_dma->base + ring_len_reg[osi_dma->mac]);

  update_rx_tail_ptr (osi_dma, chan, tailptr);

  /* Program Ring start address */
  osi_dma_writel (
    H32 (rx_ring->rx_desc_phy_addr),
    (nveu8_t *)osi_dma->base + start_addr_high_reg[osi_dma->mac]
    );
  osi_dma_writel (
    L32 (rx_ring->rx_desc_phy_addr),
    (nveu8_t *)osi_dma->base + start_addr_low_reg[osi_dma->mac]
    );

fail:
  return ret;
}

/**
 * @brief rx_dma_desc_init - Initialize DMA Receive descriptors for Rx channel.
 *
 * @note
 * Algorithm:
 *  - Initialize Receive descriptors with DMA mappable buffers,
 *    set OWN bit, Rx ring length and set starting address of Rx DMA channel.
 *    Tx ring base address in Tx DMA registers.
 *
 * @param[in, out] osi_dma: OSI private data structure.
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
static nve32_t
rx_dma_desc_init (
  struct osi_dma_priv_data  *osi_dma
  )
{
  nveu32_t  chan = 0;
  nve32_t   ret  = 0;
  nveu32_t  i;

  for (i = 0; i < osi_dma->num_dma_chans; i++) {
    chan = osi_dma->dma_chans[i];

    ret = rx_dma_desc_initialization (osi_dma, chan);
    if (ret != 0) {
      goto fail;
    }
  }

fail:
  return ret;
}

static inline void
set_tx_ring_len_and_start_addr (
  const struct osi_dma_priv_data *const  osi_dma,
  nveu64_t                               tx_desc_phy_addr,
  nveu32_t                               dma_chan,
  nveu32_t                               len
  )
{
  const nveu32_t  chan_mask[OSI_MAX_MAC_IP_TYPES] = { 0xFU, 0xFU, 0x3FU };
  const nveu32_t  local_mac                       = osi_dma->mac % OSI_MAX_MAC_IP_TYPES;
  // Added bitwise with 0xFF to avoid CERT INT30-C error
  nveu32_t        chan                               = ((dma_chan & chan_mask[local_mac]) & (0xFFU));
  const nveu32_t  ring_len_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_DMA_CHX_TDRL (chan),
    MGBE_DMA_CHX_TX_CNTRL2 (chan),
    MGBE_DMA_CHX_TX_CNTRL2 (chan)
  };
  const nveu32_t  start_addr_high_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_DMA_CHX_TDLH (chan),
    MGBE_DMA_CHX_TDLH (chan),
    MGBE_DMA_CHX_TDLH (chan)
  };
  const nveu32_t  start_addr_low_reg[OSI_MAX_MAC_IP_TYPES] = {
    EQOS_DMA_CHX_TDLA (chan),
    MGBE_DMA_CHX_TDLA (chan),
    MGBE_DMA_CHX_TDLA (chan)
  };
  const nveu32_t  mask[OSI_MAX_MAC_IP_TYPES] = { 0x3FFU, 0x3FFFU, 0x3FFFU };
  nveu32_t        val;

  /* Program ring length */
  val = len & mask[osi_dma->mac];
  osi_dma_writel (val, (nveu8_t *)osi_dma->base + ring_len_reg[osi_dma->mac]);

  /* Program tx ring start address */
  osi_dma_writel (
    H32 (tx_desc_phy_addr),
    (nveu8_t *)osi_dma->base + start_addr_high_reg[osi_dma->mac]
    );
  osi_dma_writel (
    L32 (tx_desc_phy_addr),
    (nveu8_t *)osi_dma->base + start_addr_low_reg[osi_dma->mac]
    );
}

/**
 * @brief tx_dma_desc_init - Initialize DMA Transmit descriptors.
 *
 * @note
 * Algorithm:
 *  - Initialize Transmit descriptors and set Tx ring length,
 *    Tx ring base address in Tx DMA registers.
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
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
static nve32_t
tx_dma_desc_init (
  const struct osi_dma_priv_data *const  osi_dma
  )
{
  struct osi_tx_ring  *tx_ring = OSI_NULL;
  struct osi_tx_desc  *tx_desc = OSI_NULL;
  struct osi_tx_swcx  *tx_swcx = OSI_NULL;
  nveu32_t            chan = 0;
  nve32_t             ret = 0;
  nveu32_t            i, j;

  for (i = 0; i < osi_dma->num_dma_chans; i++) {
    chan = osi_dma->dma_chans[i];

    tx_ring = osi_dma->tx_ring[chan];
    if (osi_unlikely (tx_ring == OSI_NULL)) {
      OSI_DMA_ERR (
        osi_dma->osd,
        OSI_LOG_ARG_INVALID,
        "dma_txrx: Invalid pointers\n",
        0ULL
        );
      ret = -1;
      goto fail;
    }

    for (j = 0; j < osi_dma->tx_ring_sz; j++) {
      tx_desc = tx_ring->tx_desc + j;
      tx_swcx = tx_ring->tx_swcx + j;

      tx_desc->tdes0 = 0;
      tx_desc->tdes1 = 0;
      tx_desc->tdes2 = 0;
      tx_desc->tdes3 = 0;

      tx_swcx->len           = 0;
      tx_swcx->buf_virt_addr = OSI_NULL;
      tx_swcx->buf_phy_addr  = 0;
      tx_swcx->flags         = 0;
    }

    tx_ring->cur_tx_idx = 0;
    tx_ring->clean_idx  = 0;

 #ifndef OSI_STRIPPED_LIB
    /* Slot function parameter initialization */
    tx_ring->slot_number = 0U;
    tx_ring->slot_check  = OSI_DISABLE;
 #endif /* !OSI_STRIPPED_LIB */

    set_tx_ring_len_and_start_addr (
      osi_dma,
      tx_ring->tx_desc_phy_addr,
      chan,
      (osi_dma->tx_ring_sz - 1U)
      );
  }

fail:
  return ret;
}

nve32_t
dma_desc_init (
  struct osi_dma_priv_data  *osi_dma
  )
{
  nve32_t  ret = 0;

  ret = tx_dma_desc_init (osi_dma);
  if (ret != 0) {
    goto fail;
  }

  ret = rx_dma_desc_init (osi_dma);
  if (ret != 0) {
    goto fail;
  }

fail:
  return ret;
}

void
init_desc_ops (
  const struct osi_dma_priv_data *const  osi_dma
  )
{
  typedef void (*desc_ops_arr)(
    struct desc_ops  *p_ops
    );

  const desc_ops_arr  desc_ops_a[OSI_MAX_MAC_IP_TYPES] = {
    eqos_init_desc_ops, mgbe_init_desc_ops, mgbe_init_desc_ops
  };

  desc_ops_a[osi_dma->mac](&d_ops[osi_dma->mac]);
}
