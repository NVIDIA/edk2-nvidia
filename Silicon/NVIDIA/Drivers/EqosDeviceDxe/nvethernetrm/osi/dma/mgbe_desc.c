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

#include "dma_local.h"
#include "hw_desc.h"
#include "mgbe_desc.h"

/** @brief retry count for ptp context descriptor readiness */
#define PTP_CTX_DESC_RETRY_CNT  (10)

#ifndef OSI_STRIPPED_LIB

/**
 * @brief mgbe_get_rx_vlan - Get Rx VLAN from descriptor
 *
 * Algorithm:
 *      1) Check if the descriptor has CVLAN set
 *      2) If set, set a per packet context flag indicating packet is VLAN
 *      tagged.
 *      3) Extract VLAN tag ID from the descriptor
 *
 * @param[in] rx_desc: Rx descriptor
 * @param[in] rx_pkt_cx: Per-Rx packet context structure
 */
static inline void
mgbe_get_rx_vlan (
  struct osi_rx_desc    *rx_desc,
  struct osi_rx_pkt_cx  *rx_pkt_cx
  )
{
  unsigned int  ellt = rx_desc->rdes3 & RDES3_ELLT;

  if ((ellt & RDES3_ELLT_CVLAN) == RDES3_ELLT_CVLAN) {
    rx_pkt_cx->flags   |= OSI_PKT_CX_VLAN;
    rx_pkt_cx->vlan_tag = rx_desc->rdes0 & RDES0_OVT;
  }
}

/**
 * @brief mgbe_get_rx_err_stats - Detect Errors from Rx Descriptor
 *
 * Algorithm: This routine will be invoked by OSI layer itself which
 *      checks for the Last Descriptor and updates the receive status errors
 *      accordingly.
 *
 * @param[in] rx_desc: Rx Descriptor.
 * @param[in] pkt_err_stats: Packet error stats which stores the errors reported
 */
static inline void
mgbe_update_rx_err_stats (
  struct osi_rx_desc        *rx_desc,
  struct osi_pkt_err_stats  *stats
  )
{
  unsigned int  frpsm = 0;
  unsigned int  frpsl = 0;

  /* increment rx crc if we see CE bit set */
  if ((rx_desc->rdes3 & RDES3_ERR_MGBE_CRC) == RDES3_ERR_MGBE_CRC) {
    stats->rx_crc_error =
      dma_update_stats_counter (stats->rx_crc_error, 1UL);
  }

  /* Update FRP Counters */
  frpsm = rx_desc->rdes2 & MGBE_RDES2_FRPSM;
  frpsl = rx_desc->rdes3 & MGBE_RDES3_FRPSL;
  /* Increment FRP parsed count */
  if ((frpsm == OSI_NONE) && (frpsl == OSI_NONE)) {
    stats->frp_parsed =
      dma_update_stats_counter (stats->frp_parsed, 1UL);
  }

  /* Increment FRP dropped count */
  if ((frpsm == OSI_NONE) && (frpsl == MGBE_RDES3_FRPSL)) {
    stats->frp_dropped =
      dma_update_stats_counter (stats->frp_dropped, 1UL);
  }

  /* Increment FRP Parsing Error count */
  if ((frpsm == MGBE_RDES2_FRPSM) && (frpsl == OSI_NONE)) {
    stats->frp_err =
      dma_update_stats_counter (stats->frp_err, 1UL);
  }

  /* Increment FRP Incomplete Parsing count */
  if ((frpsm == MGBE_RDES2_FRPSM) && (frpsl == MGBE_RDES3_FRPSL)) {
    stats->frp_incomplete =
      dma_update_stats_counter (stats->frp_incomplete, 1UL);
  }
}

/**
 * @brief mgbe_get_rx_hash - Get Rx packet hash from descriptor if valid
 *
 * Algorithm: This routine will be invoked by OSI layer itself to get received
 * packet Hash from descriptor if RSS hash is valid and it also sets the type
 * of RSS hash.
 *
 * @param[in] rx_desc: Rx Descriptor.
 * @param[in] rx_pkt_cx: Per-Rx packet context structure
 */
static void
mgbe_get_rx_hash (
  struct osi_rx_desc    *rx_desc,
  struct osi_rx_pkt_cx  *rx_pkt_cx
  )
{
  unsigned int  pkt_type = rx_desc->rdes3 & RDES3_L34T;

  if ((rx_desc->rdes3 & RDES3_RSV) != RDES3_RSV) {
    return;
  }

  switch (pkt_type) {
    case RDES3_L34T_IPV4_TCP:
    case RDES3_L34T_IPV4_UDP:
    case RDES3_L34T_IPV6_TCP:
    case RDES3_L34T_IPV6_UDP:
      rx_pkt_cx->rx_hash_type = OSI_RX_PKT_HASH_TYPE_L4;
      break;
    default:
      rx_pkt_cx->rx_hash_type = OSI_RX_PKT_HASH_TYPE_L3;
      break;
  }

  /* Get Rx hash from RDES1 RSSH */
  rx_pkt_cx->rx_hash = rx_desc->rdes1;
  rx_pkt_cx->flags  |= OSI_PKT_CX_RSS;
}

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief mgbe_get_rx_csum - Get the Rx checksum from descriptor if valid
 *
 * Algorithm:
 *      1) Check if the descriptor has any checksum validation errors.
 *      2) If none, set a per packet context flag indicating no err in
 *              Rx checksum
 *      3) The OSD layer will mark the packet appropriately to skip
 *              IP/TCP/UDP checksum validation in software based on whether
 *              COE is enabled for the device.
 *
 * @param[in] rx_desc: Rx descriptor
 * @param[in] rx_pkt_cx: Per-Rx packet context structure
 */
static void
mgbe_get_rx_csum (
  const struct osi_rx_desc *const  rx_desc,
  struct osi_rx_pkt_cx             *rx_pkt_cx
  )
{
  nveu32_t  ellt = rx_desc->rdes3 & RDES3_ELLT;
  nveu32_t  pkt_type;

  if ((rx_desc->rdes3 & RDES3_ES_MGBE) != 0U) {
    if (ellt == RDES3_ELLT_CSUM_ERR) {
      rx_pkt_cx->rxcsum |= OSI_CHECKSUM_TCP_UDP_BAD;
    } else if (ellt == RDES3_ELLT_IPHE) {
      rx_pkt_cx->rxcsum |= OSI_CHECKSUM_IPv4_BAD;
    } else {
      /* Do nothing */
    }
  } else {
    pkt_type = rx_desc->rdes3 & MGBE_RDES3_PT_MASK;
    if (pkt_type != 0U) {
      /* ES is zero and PT is non-zero means
       * HW validated CSUM, hence set UNNECESSARY flag for
       * Linux OSD.
       * Remaining flags are for QNX OSD.
       */
      rx_pkt_cx->rxcsum |= OSI_CHECKSUM_UNNECESSARY;
      if (pkt_type == MGBE_RDES3_PT_IPV4_TCP) {
        rx_pkt_cx->rxcsum |= OSI_CHECKSUM_TCPv4;
        rx_pkt_cx->rxcsum |= OSI_CHECKSUM_IPv4;
      } else if (pkt_type == MGBE_RDES3_PT_IPV4_UDP) {
        rx_pkt_cx->rxcsum |= OSI_CHECKSUM_UDPv4;
        rx_pkt_cx->rxcsum |= OSI_CHECKSUM_IPv4;
      } else if (pkt_type == MGBE_RDES3_PT_IPV6_TCP) {
        rx_pkt_cx->rxcsum |= OSI_CHECKSUM_TCPv6;
        rx_pkt_cx->rxcsum |= OSI_CHECKSUM_IPv4;
      } else if (pkt_type == MGBE_RDES3_PT_IPV6_UDP) {
        rx_pkt_cx->rxcsum |= OSI_CHECKSUM_UDPv6;
        rx_pkt_cx->rxcsum |= OSI_CHECKSUM_IPv4;
      } else {
        rx_pkt_cx->rxcsum |= OSI_CHECKSUM_IPv4;
      }
    }
  }
}

/**
 * @brief mgbe_get_rx_hwstamp - Get Rx HW Time stamp
 *
 * Algorithm:
 *      1) Check if packet has context descriptor available (RDES3_CDA set in rx_desc->rdes3).
 *      2) Return -1 if context descriptor is not available.
 *      3) Check for TS availability in context descriptor (RDES3_CTXT/RDES3_TSA both
 *         should be set and RDES3_OWN/RDES3_TSD both should not be set in context_desc->rdes3).
 *             1) If yes, check timestamp from context_desc->rdes0 and context_desc->rdes1 is valid.
 *                (rdes0 and rdes1 should not be equal to OSI_INVALID_VALUE).
 *             2) Return 0 if timestamp is invalid.
 *             3) Extract timestamp from context_desc->rdes0 and context_desc->rdes1
 *                (context_desc->rdes0 + (OSI_NSEC_PER_SEC * context_desc->rdes1)) and store into
 *                rx_pkt_cx->ns.
 *             4) Return 0 if rx_pkt_cx->ns is less than context_desc->rdes0.
 *             5) Set flag OSI_PKT_CX_PTP in rx_pkt_cx->flags.
 *      3) If context descriptor timestamp dropped bit is set (RDES3_CTXT/RDES3_TSD both set and
 *         RDES3_OWN bit not set), return 0.
 *      4) If timestamp descriptor is not yet available, sleep for 1us using
 *         osi_dma->osd_ops.udelay() and retry checking context descriptor from step3 with max
 *         rery count of PTP_CTX_DESC_RETRY_CNT.
 *      5) Return -1 if timestamp descriptor is not yet available even after
 *         PTP_CTX_DESC_RETRY_CNT retries.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] rx_desc: Rx descriptor
 * @param[in] context_desc: Rx context descriptor
 * @param[out] rx_pkt_cx: Rx packet context
 *
 * @retval -1 if TimeStamp is not available
 * @retval 0 if TimeStamp is available or dropped.
 */
static nve32_t
mgbe_get_rx_hwstamp (
  const struct osi_dma_priv_data *const  osi_dma,
  const struct osi_rx_desc *const        rx_desc,
  const struct osi_rx_desc *const        context_desc,
  struct osi_rx_pkt_cx                   *rx_pkt_cx
  )
{
  nve32_t  ret = 0;
  nve32_t  retry;

  if ((rx_desc->rdes3 & RDES3_CDA) != RDES3_CDA) {
    ret = -1;
    goto fail;
  }

  /* RDES3_CDA is set, hence it is a context descriptor.
   * Return always 0 from here on to allow caller to discard context descriptor
   */
  for (retry = 0; retry < PTP_CTX_DESC_RETRY_CNT; retry++) {
    if ((context_desc->rdes3 & (RDES3_OWN | RDES3_CTXT | RDES3_TSA | RDES3_TSD)) ==
        (RDES3_CTXT | RDES3_TSA))
    {
      if ((context_desc->rdes0 == OSI_INVALID_VALUE) &&
          (context_desc->rdes1 == OSI_INVALID_VALUE))
      {
        /* Invalid time stamp */
        break;
      }

      /* Time Stamp can be read */
      rx_pkt_cx->ns = context_desc->rdes0 + (OSI_NSEC_PER_SEC * context_desc->rdes1);
      if (rx_pkt_cx->ns < context_desc->rdes0) {
        break;
      }

      /* Update rx pkt context flags to indicate PTP */
      rx_pkt_cx->flags |= OSI_PKT_CX_PTP;
      break;
    } else {
      if ((context_desc->rdes3 & (RDES3_OWN | RDES3_CTXT | RDES3_TSD)) ==
          (RDES3_CTXT | RDES3_TSD))
      {
        /* Timestamp Dropped by HW, no need to retry */
        break;
      }

      /* TS not available yet, so retrying */
      osi_dma->osd_ops.udelay (OSI_DELAY_1US);
    }
  }

  if (retry == PTP_CTX_DESC_RETRY_CNT) {
    /* Timed out waiting for Rx timestamp */
    ret = -1;
    OSI_DMA_ERR (
      osi_dma->osd,
      OSI_LOG_ARG_INVALID,
      "hwstamp: Context descriptor OWN bit not cleared by HW\n",
      (nveul64_t)(context_desc->rdes3)
      );
    goto fail;
  }

fail:
  return ret;
}

void
mgbe_init_desc_ops (
  struct desc_ops  *p_dops
  )
{
 #ifndef OSI_STRIPPED_LIB
  p_dops->update_rx_err_stats = mgbe_update_rx_err_stats;
  p_dops->get_rx_vlan         = mgbe_get_rx_vlan;
  p_dops->get_rx_hash         = mgbe_get_rx_hash;
 #endif /* !OSI_STRIPPED_LIB */
  p_dops->get_rx_csum    = mgbe_get_rx_csum;
  p_dops->get_rx_hwstamp = mgbe_get_rx_hwstamp;
}
