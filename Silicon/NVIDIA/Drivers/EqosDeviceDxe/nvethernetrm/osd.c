/*
 * SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 */

#include "osd.h"
#include "EmacDxeUtil.h"
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>

/**
 * @brief osd_usleep - sleep in micro seconds
 *
 * @param[in] usec: time in usec
 */
void
osd_usleep (
  unsigned long long  usec
  )
{
  DeviceDiscoveryThreadMicroSecondDelay (usec);
}

/**
 * @brief osd_udelay - delay in micro seconds
 *
 * @param[in] usec: time in usec
 */
void
osd_udelay (
  unsigned long long  usec
  )
{
  DeviceDiscoveryThreadMicroSecondDelay (usec);
}

/**
 * @brief osd_log - logging function
 *
 * @param[in] priv: OSD private data
 * @param[in] func: function name
 * @param[in] line: line number
 * @param[in] level: log level
 * @param[in] type: error type
 * @param[in] err: error string
 * @param[in] loga: error data
 */
void
osd_log (
  void                *priv,
  const char          *func,
  unsigned int        line,
  unsigned int        level,
  unsigned int        type,
  const char          *err,
  unsigned long long  loga
  )
{
  if (level == OSI_LOG_ERR) {
    DEBUG ((
      DEBUG_ERROR,
      "Osd: Error: Function: %a Line: %u Type: %u Err: %a Loga:0x%llx \r\n",
      func,
      line,
      type,
      err,
      loga
      ));
  } else if (level == OSI_LOG_WARN) {
    DEBUG ((
      DEBUG_ERROR,
      "Osd: Warning: Function: %a Line: %u Type: %u Err: %a Loga:0x%llx \r\n",
      func,
      line,
      type,
      err,
      loga
      ));
  } else if (level == OSI_LOG_INFO) {
    DEBUG ((
      DEBUG_ERROR,
      "Osd: Info: Function: %a Line: %u Type: %u Err: %a Loga:0x%llx \r\n",
      func,
      line,
      type,
      err,
      loga
      ));
  }
}

/**
 * @brief osd_receive_packet - Handover received packet to network stack.
 *
 * Algorithm:
 *        1) Unmap the DMA buffer address.
 *        2) Updates socket buffer with len and ether type and handover to
 *        OS network stack.
 *        3) Refill the Rx ring based on threshold.
 *        4) Fills the rxpkt_cx->flags with the below bit fields accordingly
 *        OSI_PKT_CX_VLAN
 *        OSI_PKT_CX_VALID
 *        OSI_PKT_CX_CSUM
 *        OSI_PKT_CX_TSO
 *        OSI_PKT_CX_PTP
 *
 * @param[in] priv: OSD private data structure.
 * @param[in] rxring: Pointer to DMA channel Rx ring.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] dma_buf_len: Rx DMA buffer length.
 * @param[in] rxpkt_cx: Received packet context.
 * @param[in] rx_pkt_swcx: Received packet sw context.
 *
 * @note Rx completion need to make sure that Rx descriptors processed properly.
 */
void
osd_receive_packet (
  void                        *priv,
  struct osi_rx_ring          *rxring,
  unsigned int                chan,
  unsigned int                dma_buf_len,
  const struct osi_rx_pkt_cx  *rxpkt_cx,
  struct osi_rx_swcx          *rx_pkt_swcx
  )
{
  EMAC_DRIVER  *EmacDriver = (EMAC_DRIVER *)priv;

  EmacDriver->rx_pkt_swcx = rx_pkt_swcx;
  rx_pkt_swcx->flags     |= OSI_RX_SWCX_PROCESSED;
  EmacDriver->rxpkt_cx    = rxpkt_cx;
}

/**
 * @brief osd_transmit_complete - Transmit completion routine.
 *
 * Algorithm:
 *        1) Updates stats for Linux network stack.
 *        2) unmap and free the buffer DMA address and buffer.
 *        3) Time stamp will be updated to stack if available.
 *
 * @param[in] priv: OSD private data structure.
 * @param[in] swcx: Pointer to struct which has tx done status info.
 * @param[in] txdone_pkt_cx: Pointer to struct which has tx done status info.
 *              This struct has flags to indicate tx error, whether DMA address
 *              is mapped from paged/linear buffer, Time stamp availability,
 *              if TS available txdone_pkt_cx->ns stores the time stamp.
 *              Below are the valid bit maps set for txdone_pkt_cx->flags
 *              OSI_TXDONE_CX_PAGED_BUF         OSI_BIT(0)
 *              OSI_TXDONE_CX_ERROR             OSI_BIT(1)
 *              OSI_TXDONE_CX_TS                OSI_BIT(2)
 *
 * @note Tx completion need to make sure that Tx descriptors processed properly.
 */
void
osd_transmit_complete (
  void                            *priv,
  const struct osi_tx_swcx        *swcx,
  const struct osi_txdone_pkt_cx  *txdone_pkt_cx
  )
{
  EMAC_DRIVER  *EmacDriver = (EMAC_DRIVER *)priv;

  EmacDriver->tx_completed_buffer = swcx->buf_virt_addr;
}

/**.printf function callback */
void
osd_core_printf (
  struct osi_core_priv_data  *priv,
  nveu32_t                   type,
  const char                 *fmt,
  ...
  )
{
  VA_LIST  Marker;

  VA_START (Marker, fmt);
  DebugVPrint (DEBUG_ERROR, fmt, Marker);
  VA_END (Marker);
}

/**.printf function callback */
void
osd_dma_printf (
  struct osi_dma_priv_data  *priv,
  nveu32_t                  type,
  const char                *fmt,
  ...
  )
{
  VA_LIST  Marker;

  VA_START (Marker, fmt);
  DebugVPrint (DEBUG_ERROR, fmt, Marker);
  VA_END (Marker);
}

void
osd_restart_lane_bringup (
  void      *priv,
  nveu32_t  en_disable
  )
{
  DEBUG ((DEBUG_ERROR, "%a\n", __FUNCTION__));
}

nve32_t
osd_padctrl_mii_rx_pins (
  void      *priv,
  nveu32_t  enable
  )
{
  DEBUG ((DEBUG_ERROR, "%a\n", __FUNCTION__));
  return 0;
}
