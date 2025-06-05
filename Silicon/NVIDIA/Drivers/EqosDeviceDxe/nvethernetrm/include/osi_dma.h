/*
 * SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
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

#ifndef INCLUDED_OSI_DMA_H
#define INCLUDED_OSI_DMA_H

#include <osi_common.h>
#include "osi_dma_txrx.h"

/**
 * @addtogroup Helper Helper MACROS
 *
 * @brief These flags are used for PTP time synchronization
 * @{
 */
/** Bit used to indicate as PTP master */
#define OSI_PTP_SYNC_MASTER  OSI_BIT(0)
/** Bit used to indicate as PTP Slave */
#define OSI_PTP_SYNC_SLAVE  OSI_BIT(1)
/** Bit used to indicate as PTP one step mode */
#define OSI_PTP_SYNC_ONESTEP  OSI_BIT(2)
/** Bit used to indicate as PTP two step mode */
#define OSI_PTP_SYNC_TWOSTEP  OSI_BIT(3)
/** @} */

/**
 * @addtogroup Helper Helper MACROS
 *
 * @brief EQOS generic helper MACROS.
 * @{
 */
/** @brief VLAN Header length */
#define NV_VLAN_HLEN  0x4U
/** @brief Ethernet Header length */
#define OSI_ETH_HLEN  0xEU

#define OSI_INVALID_VALUE  0xFFFFFFFFU

#define OSI_ONE_MEGA_HZ  1000000U
/** @brief MAX ULLONG value */
#define OSI_ULLONG_MAX    (~0ULL)
#define OSI_MSEC_PER_SEC  1000U

/* Compiler hints for branch prediction */
#define osi_likely(x)  __builtin_expect(!!(x), 1)
/** @} */

/**
 * @addtogroup Channel Mask
 * @brief Chanel mask for Tx and Rx interrupts
 * @{
 */
#define OSI_VM_IRQ_TX_CHAN_MASK(x)  OSI_BIT((x) << 1U)
#define OSI_VM_IRQ_RX_CHAN_MASK(x)  OSI_BIT(((x) << 1U) + 1U)
/** @} */

#ifdef LOG_OSI

/**
 * OSI error macro definition,
 * @param[in] priv: OSD private data OR NULL
 * @param[in] type: error type
 * @param[in] err:  error string
 * @param[in] loga: error additional information
 */
#define OSI_DMA_ERR(priv, type, err, loga)                              \
{                                                               \
        osi_dma->osd_ops.ops_log(priv, __func__, __LINE__,      \
                                 OSI_LOG_ERR, type, err, loga); \
}

  #ifndef OSI_STRIPPED_LIB

/**
 * OSI info macro definition
 * @param[in] priv: OSD private data OR NULL
 * @param[in] type: error type
 * @param[in] err:  error string
 * @param[in] loga: error additional information
 */
#define OSI_DMA_INFO(priv, type, err, loga)                             \
{                                                               \
        osi_dma->osd_ops.ops_log(priv, __func__, __LINE__,      \
                                 OSI_LOG_INFO, type, err, loga);\
}
  #endif /* !OSI_STRIPPED_LIB */
#else
#define OSI_DMA_ERR(priv, type, err, loga)
#endif /* LOG_OSI */

/**
 * @addtogroup EQOS-PKT Packet context fields
 *
 * @brief These flags are used to convey context information about a packet
 * between OSI and OSD. The context information includes
 * whether a VLAN tag is to be inserted for a packet,
 * whether a received packet is valid,
 * whether checksum offload is to be enabled for the packet upon transmit,
 * whether IP checksum offload is to be enabled for the packet upon transmit,
 * whether TCP segmentation offload is to be enabled for the packet,
 * whether the HW should timestamp transmit/arrival of a packet respectively
 * whether a paged buffer.
 * @{
 */
/** VLAN packet */
#define OSI_PKT_CX_VLAN  OSI_BIT(0)
/** CSUM packet */
#define OSI_PKT_CX_CSUM  OSI_BIT(1)
/** TSO packet */
#define OSI_PKT_CX_TSO  OSI_BIT(2)
/** PTP packet */
#define OSI_PKT_CX_PTP  OSI_BIT(3)
/** Paged buffer */
#define OSI_PKT_CX_PAGED_BUF  OSI_BIT(4)
#ifndef OSI_STRIPPED_LIB
/** Rx packet has RSS hash */
#define OSI_PKT_CX_RSS  OSI_BIT(5)
#endif /* !OSI_STRIPPED_LIB */
/** Valid packet */
#define OSI_PKT_CX_VALID  OSI_BIT(10)
/** Update Packet Length in Tx Desc3 */
#define OSI_PKT_CX_LEN  OSI_BIT(11)
/** IP CSUM packet */
#define OSI_PKT_CX_IP_CSUM  OSI_BIT(12)
/** @} */

/** VDMA ID in TDESC0 **/
#define OSI_PTP_VDMA_SHIFT  10U

#ifndef OSI_STRIPPED_LIB

/**
 * @addtogroup SLOT function context fields
 *
 * @brief These flags are used for DMA channel Slot context configuration
 * @{
 */
#define OSI_SLOT_INTVL_DEFAULT  125U
#define OSI_SLOT_INTVL_MAX      4095U
#define OSI_SLOT_NUM_MAX        16U
/** @} */
#endif /* !OSI_STRIPPED_LIB */

/**
 * @addtogroup EQOS-TX Tx done packet context fields
 *
 * @brief These flags used to convey transmit done packet context information,
 * whether transmitted packet used a paged buffer, whether transmitted packet
 * has an tx error, whether transmitted packet has an TS
 *
 * @{
 */
/** Flag to indicate if buffer programmed in desc. is DMA map'd from
 * linear/Paged buffer from OS layer */
#define OSI_TXDONE_CX_PAGED_BUF  OSI_BIT(0)
/** Flag to indicate if there was any tx error */
#define OSI_TXDONE_CX_ERROR  OSI_BIT(1)
/** Flag to indicate the availability of time stamp */
#define OSI_TXDONE_CX_TS  OSI_BIT(2)
/** Flag to indicate the delayed availability of time stamp */
#define OSI_TXDONE_CX_TS_DELAYED  OSI_BIT(3)
/** @} */

/**
 * @addtogroup EQOS-CHK Checksum offload results
 *
 * @brief Flag to indicate the result from checksum offload engine
 * to SW network stack in receive path.
 * OSI_CHECKSUM_NONE indicates that HW checksum offload
 * engine did not verify the checksum, SW network stack has to do it.
 * OSI_CHECKSUM_UNNECESSARY indicates that HW validated the
 * checksum already, network stack can skip validation.
 * @{
 */
/* Checksum offload result flags */
#ifndef OSI_STRIPPED_LIB
#define OSI_CHECKSUM_NONE  0x0U
#endif /* OSI_STRIPPED_LIB */
/** TCP header/payload */
#define OSI_CHECKSUM_TCPv4  OSI_BIT(0)
/** UDP header/payload */
#define OSI_CHECKSUM_UDPv4  OSI_BIT(1)
/** TCP/UDP checksum bad */
#define OSI_CHECKSUM_TCP_UDP_BAD  OSI_BIT(2)
/** IPv6 TCP header/payload */
#define OSI_CHECKSUM_TCPv6  OSI_BIT(4)
/** IPv6 UDP header/payload */
#define OSI_CHECKSUM_UDPv6  OSI_BIT(5)
/** IPv4 header */
#define OSI_CHECKSUM_IPv4  OSI_BIT(6)
/** IPv4 header checksum bad */
#define OSI_CHECKSUM_IPv4_BAD  OSI_BIT(7)
/** Checksum check not required */
#define OSI_CHECKSUM_UNNECESSARY  OSI_BIT(8)
/** @} */

/**
 * @addtogroup EQOS-RX Rx SW context flags
 *
 * @brief Flags to share info about the Rx SW context structure per descriptor
 * between OSI and OSD.
 * @{
 */
/** Rx swcx flag to indicate buffer can be reused */
#define OSI_RX_SWCX_REUSE  OSI_BIT(0)
/** Rx swcx flag to indicate buffer is valid */
#define OSI_RX_SWCX_BUF_VALID  OSI_BIT(1)
/** Rx swcx flag to indicate packet is processed by driver */
#define OSI_RX_SWCX_PROCESSED  OSI_BIT(3)

/** @} */

#ifndef OSI_STRIPPED_LIB

/**
 * @addtogroup RSS-HASH type
 *
 * @brief Macros to represent to type of packet for hash stored in receive packet
 * context.
 * @{
 */
#define OSI_RX_PKT_HASH_TYPE_L2  0x1U
#define OSI_RX_PKT_HASH_TYPE_L3  0x2U
#define OSI_RX_PKT_HASH_TYPE_L4  0x3U
/** @} */
#endif /* !OSI_STRIPPED_LIB */

/**
 * @addtogroup OSI-INTR OSI DMA interrupt handling macros.
 *
 * @brief Macros to pass osi_handle_dma_intr() API to handle
 * the interrupts between OSI and OSD.
 * @{
 */
/** DMA Tx channel interrupt bit */
#define OSI_DMA_CH_TX_INTR  0U
/** DMA Rx channel interrupt bit */
#define OSI_DMA_CH_RX_INTR  1U
/** DMA channel interrupt disable */
#define OSI_DMA_INTR_DISABLE  0U
/** DMA channel interrupt enable */
#define OSI_DMA_INTR_ENABLE  1U
/** @} */

/**
 * @addtogroup OSI_DMA-DEBUG helper macros
 *
 * @brief Helper macros for OSI dma debugging.
 * @{
 */
#ifdef OSI_DEBUG
#define OSI_DMA_IOCTL_CMD_REG_DUMP           1U
#define OSI_DMA_IOCTL_CMD_STRUCTS_DUMP       2U
#define OSI_DMA_IOCTL_CMD_DEBUG_INTR_CONFIG  3U
#endif /* OSI_DEBUG */
#define OSI_DMA_IOCTL_CMD_RX_RIIT_CONFIG  4U
/** @} */

/**
 * @brief Maximum buffer length per DMA descriptor (16KB - 1).
 */
#define OSI_TX_MAX_BUFF_SIZE  0x3FFFU

#ifndef OSI_STRIPPED_LIB

/**
 * @brief OSI packet error stats
 */
struct osi_pkt_err_stats {
  /** IP Header Error */
  nveu64_t    ip_header_error;
  /** Jabber time out Error */
  nveu64_t    jabber_timeout_error;
  /** Packet Flush Error */
  nveu64_t    pkt_flush_error;
  /** Payload Checksum Error */
  nveu64_t    payload_cs_error;
  /** Loss of Carrier Error */
  nveu64_t    loss_of_carrier_error;
  /** No Carrier Error */
  nveu64_t    no_carrier_error;
  /** Late Collision Error */
  nveu64_t    late_collision_error;
  /** Excessive Collision Error */
  nveu64_t    excessive_collision_error;
  /** Excessive Deferal Error */
  nveu64_t    excessive_deferal_error;
  /** Under Flow Error */
  nveu64_t    underflow_error;
  /** Rx CRC Error */
  nveu64_t    rx_crc_error;
  /** Rx Frame Error */
  nveu64_t    rx_frame_error;
  /** clear_tx_pkt_err_stats() API invoked */
  nveu64_t    clear_tx_err;
  /** clear_rx_pkt_err_stats() API invoked */
  nveu64_t    clear_rx_err;

  /** FRP Parsed count, includes accept
   * routing-bypass, or result-bypass count.
   */
  nveu64_t    frp_parsed;
  /** FRP Dropped count */
  nveu64_t    frp_dropped;
  /** FRP Parsing Error count */
  nveu64_t    frp_err;
  /** FRP Incomplete Parsing */
  nveu64_t    frp_incomplete;
};

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief RX RIIT value for speed
 */
struct osi_rx_riit {
  /** speed */
  nveu32_t    speed;
  /** riit value */
  nveu32_t    riit;
};

/**
 * @brief Receive Descriptor
 */
struct osi_rx_desc {
  /** Receive Descriptor 0 */
  nveu32_t    rdes0;
  /** Receive Descriptor 1 */
  nveu32_t    rdes1;
  /** Receive Descriptor 2 */
  nveu32_t    rdes2;
  /** Receive Descriptor 3 */
  nveu32_t    rdes3;
};

/**
 * @brief Receive descriptor software context
 */
struct osi_rx_swcx {
  /** DMA buffer physical address. Should be non NULL */
  nveu64_t    buf_phy_addr;
  /** DMA buffer virtual address. Value must be non NULL value */
  void        *buf_virt_addr;
  /** Length of buffer. Maximum value is 0xFFFF */
  nveu32_t    len;

  /** Flags to share info about Rx swcx between OSD and OSI.
   *  valid bits are as below
   *  NVETHERNET_CL$OSI_RX_SWCX_REUSE
   *  NVETHERNET_CL$OSI_RX_SWCX_BUF_VALID
   *  NVETHERNET_CL$OSI_RX_SWCX_PROCESSED
   */
  nveu32_t    flags;
  /** nvsocket data index. Max value is ULONG_MAX */
  nveu64_t    data_idx;
};

/**
 * @brief Received packet context. This is a single instance
 * and it is reused for all rx packets.
 */
struct osi_rx_pkt_cx {
  /** Bit map which holds the features that rx packets supports
   *  Below are valid bits
   *  NVETHERNETCL_PIF$OSI_PKT_CX_VLAN
   *  NVETHERNETCL_PIF$OSI_PKT_CX_PTP
   *  NVETHERNETCL_PIF$OSI_PKT_CX_VALID
   */
  nveu32_t    flags;

  /** Stores the Rx csum, Valid bit field are listed below
   *  NVETHERNETCL_PIF$OSI_CHECKSUM_TCPv4
   *  NVETHERNETCL_PIF$OSI_CHECKSUM_UDPv4
   *  NVETHERNETCL_PIF$OSI_CHECKSUM_TCP_UDP_BAD
   *  NVETHERNETCL_PIF$OSI_CHECKSUM_TCPv6
   *  NVETHERNETCL_PIF$OSI_CHECKSUM_UDPv6
   *  NVETHERNETCL_PIF$OSI_CHECKSUM_IPv4
   *  NVETHERNETCL_PIF$OSI_CHECKSUM_IPv4_BAD
   *  NVETHERNETCL_PIF$OSI_CHECKSUM_UNNECESSARY
   */
  nveu32_t     rxcsum;
  /** Length of received packet, Maximum vlaue 0x7fff */
  nveu32_t     pkt_len;
  /** TS in nsec for the received packet. Can be any non zero value */
  nveul64_t    ns;
 #ifndef OSI_STRIPPED_LIB
  /** Stores the VLAN tag ID in received packet */
  nveu32_t     vlan_tag;
  /** Stores received packet hash */
  nveu32_t     rx_hash;
  /** Store type of packet for which hash carries at rx_hash */
  nveu32_t     rx_hash_type;
 #endif /* !OSI_STRIPPED_LIB */
};

/**
 * @brief DMA channel Rx ring. The number of instances depends on the
 * number of DMA channels configured
 */

struct osi_rx_ring {
  /** Pointer to tx dma descriptor(osi_rx_desc).
   *  Memory of NVETHERNETCL_PIF$OSI_EQOS_RX_DESC_CNT/NVETHERNETCL_PIF$OSI_MGBE_RX_DESC_CNT
   *  structure size should be allocated by OSD
   */
  struct osi_rx_desc    *rx_desc;

  /** Pointer to rx dma descriptor software context information (osi_rx_swcx).
   *  Memory of RX_DESC_CNT strucutre size should be alloced by OSD.
   *  This information is populated base on #osi_rx_desc.
   */
  struct osi_rx_swcx    *rx_swcx;

  /** Physical address to the start of Tx descriptor,
   *  populated by OSD while calling osi_hw_transmit().
   *  Can be any non zero value
   */
  nveu64_t              rx_desc_phy_addr;

  /** Current Rx index used in osi_process_rx_completions()
   *  to start referring to osi_rx_swcx and osi_rx_desc.
   *  Max value of is NVETHERNETCL_PIF$OSI_EQOS_RX_DESC_CNT - 1 or
   *  NVETHERNETCL_PIF#OSI_MGBE_RX_DESC_CNT.
   *  When incremented, this variable rounds off at NVETHERNETCL_PIF$OSI_EQOS_RX_DESC_CNT/
   *  NVETHERNETCL_PIF$OSI_MGBE_RX_DESC_CNT.
   */
  nveu32_t                cur_rx_idx;

  /** Current Rx refill index used in osi_rx_dma_desc_init()
   *  to start referring to osi_rx_swcx and osi_rx_desc.
   *  Increment of this variable is round off at
   *  NVETHERNETCL_PIF$OSI_EQOS_RX_DESC_CNT/NVETHERNETCL_PIF$OSI_MGBE_RX_DESC_CNT
   */
  nveu32_t                refill_idx;
  /** Receive packet context. Refer osi_rx_pkt_cx */
  struct osi_rx_pkt_cx    rx_pkt_cx;
};

/**
 *@brief Transmit descriptor software context
 */
struct osi_tx_swcx {
  /** Physical address of DMA mapped buffer. Must be a valid physical address */
  nveu64_t    buf_phy_addr;
  /** Virtual address of DMA buffer.  Value must be non NULL value */
  void        *buf_virt_addr;
  /** Length of buffer. Maximum value is 0xFFFF */
  nveu32_t    len;
 #ifndef OSI_STRIPPED_LIB

  /** Flag to keep track of whether buffer pointed by buf_phy_addr
   * is a paged buffer/linear buffer */
  nveu32_t    is_paged_buf;
 #endif /* !OSI_STRIPPED_LIB */

  /** Flag to keep track of SWCX. Values are listed as below
   * NVETHERNETCL_PIF$OSI_PKT_CX_PAGED_BUF
   */
  nveu32_t    flags;

  /** Packet id of packet for which TX timestamp needed.
   * Max value is NVETHERNETCL_PIF$UINT_MAX
   */
  nveu32_t    pktid;
  /** VDMA id of packet for which TX packet sent for timestamp needed */
  nveu32_t    vdmaid;

  /** dma channel number for osd use.
   *  Max value is NVETHERNETCL_PIF$OSI_EQOS_MAX_NUM_CHANS or
   *  NVETHERNETCL_PIF$OSI_MGBE_MAX_NUM_CHANS
   */
  nveu32_t    chan;
  /** nvsocket data index. Max value is ULONG_MAX */
  nveu64_t    data_idx;
  /** reserved field 2 for future use */
  nveu64_t    rsvd2;
};

/**
 * @brief Transmit descriptor
 */
struct osi_tx_desc {
  /** Transmit descriptor 0 */
  nveu32_t    tdes0;
  /** Transmit descriptor 1 */
  nveu32_t    tdes1;
  /** Transmit descriptor 2 */
  nveu32_t    tdes2;
  /** Transmit descriptor 3 */
  nveu32_t    tdes3;
};

/**
 * @brief Transmit packet context for a packet. This is a single instance
 * and it is reused for all tx packets.
 */
struct osi_tx_pkt_cx {
  /** Holds the features information of a Tx packets. Refer below for valid bit
   * field information.
   * NVETHERNETCL_PIF$OSI_PKT_CX_VLAN
   * NVETHERNETCL_PIF$OSI_PKT_CX_CSUM
   * NVETHERNETCL_PIF$OSI_PKT_CX_TSO
   * NVETHERNETCL_PIF$OSI_PKT_CX_PTP
   * NVETHERNETCL_PIF$OSI_PKT_CX_CSUM
   * NVETHERNETCL_PIF$OSI_PKT_CX_LEN
   * NVETHERNETCL_PIF$OSI_PKT_IP_CSUM
   */
  nveu32_t    flags;

  /** VLAN tag ID to be updated in tdesc3 of
   * NVETHERNETCL_PIF$osi_tx_desc (context descriptor case)
   */
  nveu32_t    vtag_id;

  /** Number of descriptors to be updated for Tx transmission.
   *  Minimum value is 1, maximum NVETHERNETCL_PIF$OSI_EQOS_TX_DESC_CNT/
   *  NVETHERNETCL_PIF$OSI_MGBE_TX_DESC_CNT - 1
   */
  nveu32_t    desc_cnt;

  /** Max. segment size for TSO/USO/GSO/LSO packet, used to update in desc3 of
   *  NVETHERNETCL_PIF$osi_tx_desc (context descriptor case)
   */
  nveu32_t    mss;

  /** Length of application payload. Updated in tdesc3 of
   *  NVETHERNETCL_PIF$osi_tx_desc (first descriptor)
   */
  nveu32_t    payload_len;

  /** Length of transport layer tcp/udp header. Updated in tdesc3 of
   *  NVETHERNETCL_PIF$osi_tx_desc (first descriptor)
   */
  nveu32_t    tcp_udp_hdrlen;
  /** Length of all headers (ethernet/ip/tcp/udp). This variable is not used in this unit. */
  nveu32_t    total_hdrlen;
};

/**
 * @brief Transmit done packet context for a Tx packet
 */
struct osi_txdone_pkt_cx {
  /** Indicates status flags for Tx complete (tx error occurred, or
   * indicate whether desc had buf mapped from paged/linear memory etc)
   * Refer below for valid bit field information
   * NVETHERNETCL_PIF$OSI_TXDONE_CX_PAGED_BUF
   * NVETHERNETCL_PIF$OSI_TXDONE_CX_ERROR
   * NVETHERNETCL_PIF$OSI_TXDONE_CX_TS
   * NVETHERNETCL_PIF$OSI_TXDONE_CX_TS_DELAYED
   */
  nveu32_t     flags;

  /** TS captured for the tx packet and this is valid only when the PTP
   * bit is set in fields, Max value is NVETHERNETCL_PIF$OSI_ULLONG_MAX
   */
  nveul64_t    ns;

  /** Passing packet id to map TX time to packet.
   *  Max value is NVETHERNETCL_PIF$UINT_MAX
   */
  nveu32_t     pktid;
  /** Passing vdma id to map TX time to packet */
  nveu32_t     vdmaid;
};

/**
 * @brief DMA channel Tx ring. The number of instances depends on the
 * number of DMA channels configured
 */
struct osi_tx_ring {
  /** Pointer to tx dma descriptor NVETHERNETCL_PIF$osi_tx_desc. Memory of
   *  NVETHERNETCL_PIF$OSI_EQOS_TX_DESC_CNT/NVETHERNETCL_PIF$OSI_MGBE_TX_DESC_CNT
   *  structure size should be allocated by OSD
   */
  struct osi_tx_desc          *tx_desc;

  /** Pointer to tx dma descriptor software context information (osi_tx_swcx).
   *  Memory of NVETHERNETCL_PIF$OSI_EQOS_TX_DESC_CNT/NVETHERNETCL_PIF$OSI_MGBE_TX_DESC_CNT
   *  structure size should be allocated by OSD.
   *  OSD is expected to fill this data and is used in osi_hw_transmit().
   *  This information is used to populate members of NVETHERNETCL_PIF$osi_tx_desc.
   */
  struct osi_tx_swcx          *tx_swcx;

  /** Physical address to the start of Tx descriptor,
   *  populated by OSD while calling osi_hw_dma_init().
   */
  nveu64_t                    tx_desc_phy_addr;

  /** Current Tx index used in osi_hw_transmit() to start
   *  referring to NVETHERNETCL_PIF$osi_tx_swcx and NVETHERNETCL_PIF$osi_tx_desc.
   *  Max value of tx_ring->cur_tx_idx is NVETHERNETCL_PIF$OSI_EQOS_TX_DESC_CNT/
   *  NVETHERNETCL_PIF$OSI_MGBE_TX_DESC_CNT - 1.
   */
  nveu32_t                    cur_tx_idx;

  /** Descriptor index for descriptor cleanup, used in osi_process_tx_completions().
   *  This is internal to the unit.
   */
  nveu32_t                    clean_idx;
 #ifndef OSI_STRIPPED_LIB

  /** Slot function check, OSD needs to update with OSI_ENABLE,
   *  if slot_number addition is needed in descriptor. This is used in osi_hw_transmit()
   */
  nveu32_t                    slot_check;

  /** Slot number to be updated in descriptor,
   *  which is done if OSI_ENABLE is value of slot_check. Max value is OSI_SLOT_NUM_MAX.
   */
  nveu32_t                    slot_number;
 #endif /* !OSI_STRIPPED_LIB */

  /** Transmit packet context to be filled by called of osi_hw_transmit()
   *  refer osi_tx_pkt_cx for details
   */
  struct osi_tx_pkt_cx        tx_pkt_cx;

  /** Transmit complete packet context information which is paseed to OSD as
   * txdone_pkt_cx argument of osd_dma_ops->transmit_complete callback.
   * Refer osi_txdone_pkt_cx for details.
   * This parameter is updated in osi_process_tx_completions()
   */
  struct osi_txdone_pkt_cx    txdone_pkt_cx;

  /** Number of packets or frames transmitted. Incremented for every
   *  osi_hw_transmit() submission. On overflow(for max data type storage value)
   *  this value will be restarted. This parameter is internal to this unit only.
   */
  nveu32_t                    frame_cnt;

  /** Total number of desc count. Incremented for every decriptor used. This
   *  adjusted for every delta greater than equal to intr_desc_count.
   */
  nveu32_t                    desc_cnt;
  /** flag to skip memory barrier. Valid values are 1 or 0 */
  nveu32_t                    skip_dmb;
};

#ifndef OSI_STRIPPED_LIB

/**
 * @brief osi_xtra_dma_stat_counters -  OSI DMA extra stats counters
 */
struct osi_xtra_dma_stat_counters {
  /** Per chan TX packet count */
  nveu64_t    chan_tx_pkt_n[OSI_MGBE_MAX_NUM_CHANS];
  /** Per chan RX packet count */
  nveu64_t    chan_rx_pkt_n[OSI_MGBE_MAX_NUM_CHANS];
  /** Per chan TX complete call count */
  nveu64_t    tx_clean_n[OSI_MGBE_MAX_NUM_CHANS];
  /** Total number of tx packets count */
  nveu64_t    tx_pkt_n;
  /** Total number of rx packet count */
  nveu64_t    rx_pkt_n;
  /** Total number of VLAN RX packet count */
  nveu64_t    rx_vlan_pkt_n;
  /** Total number of VLAN TX packet count */
  nveu64_t    tx_vlan_pkt_n;
  /** Total number of TSO packet count */
  nveu64_t    tx_tso_pkt_n;
};

#endif /* !OSI_STRIPPED_LIB */

struct osi_dma_priv_data;

/**
 *@brief OSD DMA callbacks
 */
struct osd_dma_ops {
  /** DMA transmit complete callback */
  void    (*transmit_complete)(
    void                      *priv,
    const struct osi_tx_swcx  *swcx,
    const struct osi_txdone_pkt_cx
                              *txdone_pkt_cx
    );
  /** DMA receive packet callback */
  void    (*receive_packet)(
    void                        *priv,
    struct osi_rx_ring          *rx_ring,
    nveu32_t                    chan,
    nveu32_t                    dma_buf_len,
    const struct osi_rx_pkt_cx  *rx_pkt_cx,
    struct osi_rx_swcx          *rx_swcx
    );
  /** RX buffer reallocation callback */
  void    (*realloc_buf)(
    void                *priv,
    struct osi_rx_ring  *rx_ring,
    nveu32_t            chan
    );
  /** ops_log function callback, called for error logging*/
  void    (*ops_log)(
    void          *priv,
    const nve8_t  *func,
    nveu32_t      line,
    nveu32_t      level,
    nveu32_t      type,
    const nve8_t  *err,
    nveul64_t     loga
    );
  /** micro second delay function callback */
  void    (*udelay)(
    nveu64_t  usec
    );
 #ifdef OSI_DEBUG
  /**.printf function callback */
  void    (*printf)(
    struct osi_dma_priv_data  *osi_dma,
    nveu32_t                  type,
    const char                *fmt,
    ...
    );
 #endif /* OSI_DEBUG */
};

// #ifdef OSI_DEBUG

/**
 * @brief The OSI DMA IOCTL data structure.
 */
struct osi_dma_ioctl_data {
  /** IOCTL command number */
  nveu32_t    cmd;
  /** IOCTL command argument */
  nveu32_t    arg_u32;
};

// #endif /* OSI_DEBUG */

/**
 * @brief The OSI DMA private data structure.
 */
struct osi_dma_priv_data {
  /** Array of pointers to DMA Tx channel Ring. Refer osi_tx_ring for details
   *  OSD is expected to allocate memory for the same
   */
  struct osi_tx_ring                   *tx_ring[OSI_MGBE_MAX_NUM_CHANS];

  /** Array of pointers to DMA Rx channel Ring. Refer osi_rx_ring for details.
   *  OSD is expected to allocate memory for the same
   */
  struct osi_rx_ring                   *rx_ring[OSI_MGBE_MAX_NUM_CHANS];
  /** Memory mapped base address of MAC IP. Should be non NULL */
  void                                 *base;

  /** Pointer to OSD private data structure,
   *  This is passed as priv argument for all callbacks of osd_dma_ops
   */
  void                                 *osd;

  /** MAC HW type, Valid value is NVETHERNETCL_PIF$OSI_MAC_HW_EQOS or
   *  NVETHERNETCL_PIF$OSI_MAC_HW_MGBE
   */
  nveu32_t                             mac;

  /** Number of channels enabled in MAC, Max NVETHERNETCL_PIF$OSI_EQOS_MAX_NUM_CHANS
   *  or NVETHERNETCL_PIF$OSI_MGBE_MAX_NUM_CHANS
   */
  nveu32_t                             num_dma_chans;

  /** Array of supported DMA channels. Max for each member is
   *  NVETHERNETCL_PIF$OSI_EQOS_MAX_NUM_CHANS/NVETHERNETCL_PIF$OSI_MGBE_MAX_NUM_CHANS - 1
   * Valid array size is num_dma_chans
   */
  nveu32_t                             dma_chans[OSI_MGBE_MAX_NUM_CHANS];

  /** DMA Rx channel buffer length at HW level. Max value is related to mtu based
   *  on equation documented in sequence diagram of  osi_set_rx_buf_len()
   */
  nveu32_t                             rx_buf_len;

  /** MTU size, used in osi_set_rx_buf_len() to configure rx_buf_len.
   * Max value is NVETHERNETCL_PIF$OSI_MAX_MTU_SIZE
   */
  nveu32_t                             mtu;
 #ifndef OSI_STRIPPED_LIB
  /** Packet error stats */
  struct osi_pkt_err_stats             pkt_err_stats;
  /** Extra DMA stats */
  struct osi_xtra_dma_stat_counters    dstats;
 #endif /* !OSI_STRIPPED_LIB */
  /** Receive Interrupt Watchdog Timer Count Units. Max value is NVETHERNETCL_PIF$UINT_MAX */
  nveu32_t                             rx_riwt;

  /** Flag which decides riwt is
   *  NVETHERNETCL_PIF$OSI_ENABLE or
   *  NVETHERNETCL_PIF$OSI_DISABLE
   */
  nveu32_t                             use_riwt;
  /** Receive Interrupt Idle Timer in nsec */
  struct osi_rx_riit                   rx_riit[OSI_MGBE_MAX_NUM_RIIT];
  /** num of rx riit configs for different speeds */
  nveu32_t                             num_of_riit;
  /** Flag which decides riit is enabled(1) or disabled(0) */
  nveu32_t                             use_riit;

  /** Max no of pkts to be received before triggering Rx interrupt.
   * Max value is NVETHERNETCL_PIF$UINT_MAX
   */
  nveu32_t                             rx_frames;

  /** Flag which decides tx_frames is
   *  NVETHERNETCL_PIF$OSI_ENABLE or
   *  NVETHERNETCL_PIF$OSI_DISABLE
   */
  nveu32_t                             use_rx_frames;

  /** Transmit Interrupt Software Timer Count Units.
   *  Max value is NVETHERNETCL_PIF$UINT_MAX
   */
  nveu32_t                             tx_usecs;

  /** Flag which decides Tx timer is
   *  NVETHERNETCL_PIF$OSI_ENABLE or
   *  NVETHERNETCL_PIF$OSI_DISABLE
   */
  nveu32_t                             use_tx_usecs;

  /** Max no of pkts to transfer before triggering Tx interrupt.
   *  Max value is NVETHERNETCL_PIF$UINT_MAX
   */
  nveu32_t                             tx_frames;

  /** Max no of descs to transfer before triggering Tx interrupt.
   *  Max value is NVETHERNETCL_PIF$UINT_MAX
   */
  nveu32_t                             intr_desc_count;

  /** Flag which decides tx_frames is
   *  NVETHERNETCL_PIF$OSI_ENABLE or
   *  NVETHERNETCL_PIF$OSI_DISABLE
   */
  nveu32_t                             use_tx_frames;

  /** Flag which decides Tx timer is
   *  NVETHERNETCL_PIF$OSI_ENABLE or
   *  NVETHERNETCL_PIF$OSI_DISABLE
   */
  nveu32_t                             use_tx_descs;
  /** DMA callback ops structure */
  struct osd_dma_ops                   osd_ops;
 #ifndef OSI_STRIPPED_LIB

  /** Flag which decides virtualization is
   *  NVETHERNETCL_PIF$OSI_ENABLE or
   *  NVETHERNETCL_PIF$OSI_DISABLE
   */
  nveu32_t                             use_virtualization;
  /** Array of DMA channel slot snterval value from DT */
  nveu32_t                             slot_interval[OSI_MGBE_MAX_NUM_CHANS];
  /** Array of DMA channel slot enabled status from DT*/
  nveu32_t                             slot_enabled[OSI_MGBE_MAX_NUM_CHANS];
  /** Virtual address of reserved DMA buffer. Should be non NULL */
  void                                 *resv_buf_virt_addr;
  /** Physical address of reserved DMA buffer. Should be non NULL */
  nveu64_t                             resv_buf_phy_addr;
 #endif /* !OSI_STRIPPED_LIB */

  /** PTP flags
   * NVETHERNETCL_PIF$OSI_PTP_SYNC_MASTER - acting as master
   * NVETHERNETCL_PIF$OSI_PTP_SYNC_SLAVE  - acting as slave
   * NVETHERNETCL_PIF$OSI_PTP_SYNC_ONESTEP - one-step mode
   * NVETHENETCL_PIF$OSI_PTP_SYNC_TWOSTEP - two step mode
   */
  nveu32_t                     ptp_flag;
  /** OSI DMA IOCTL data */
  struct osi_dma_ioctl_data    ioctl_data;
 #ifdef OSI_DEBUG
  /** Flag to enable/disable descriptor dump */
  nveu32_t                     enable_desc_dump;
 #endif /* OSI_DEBUG */

  /** Flag which checks is ethernet server enabled(1) or disabled(0)
   *  NVETHERNETCL_PIF$OSI_ENABLE/NVETHERNETCL_PIF$OSI_DISABLE
   */
  nveu32_t                     is_ethernet_server;

  /** DMA Tx channel ring size. Max value is
   *  NVETHERNETCL_PIF$OSI_EQOS_TX_DESC_CNT/NVETHERNETCL_PIF$OSI_MGBE_TX_DESC_CNT
   */
  nveu32_t                     tx_ring_sz;

  /** DMA Rx channel ring size.
   *  Max value is NVETHERNETCL_PIF$OSI_EQOS_RX_DESC_CNT/NVETHERNETCL_PIF$OSI_MGBE_RX_DESC_CNT
   */
  nveu32_t                     rx_ring_sz;
  /** number of PDMA's */
  nveu32_t                     num_of_pdma;
  /** Array of PDMA to VDMA mapping copy of osi_core */
  struct osi_pdma_vdma_data    pdma_data[OSI_MGBE_MAX_NUM_PDMA_CHANS];
};

/**
 * @brief
 * Description: Gets DMA status.
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: Yes
 *  - Signal handler: Yes
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @param[in] osi_dma: DMA private data.
 * - Valid range: Any valid memory address except NULL.
 * @param[out] dma_status: Stores the global DMA Interrupt status register value
 * - Valid range: Any valid memory address except NULL.
 *
 * @retval !=0 DMA status on success
 * @retval 0 on failure - invalid argument
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_001
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_get_global_dma_status (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t *const           dma_status
  );

/**
 * @brief
 * Description: Rx descriptors count that needs to refill
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * - Valid range: Any valid memory address except NULL.
 * @param[in] chan: Channel number whose ring is to be refilled.
 * - Valid range: 0 to NVETHERNETCL_PIF$OSI_MGBE_MAX_NUM_CHANS - 1
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: Yes
 *  - Signal handler: Yes
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval !=0 "Number of available free descriptors."
 * @retval 0 on failure - invalid rx ring
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_002
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nveu32_t
osi_get_refill_rx_desc_cnt (
  const struct osi_dma_priv_data *const  osi_dma,
  nveu32_t                               chan
  );

/**
 * @brief
 * Description: DMA Rx descriptor init
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * - Valid range: Any valid memory address except NULL.
 * @param[in, out] rx_ring: HW ring corresponding to Rx DMA channel.
 * - Valid range: Any valid memory address except NULL.
 * @param[in] chan: Rx DMA channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 * - Valid range: 0 to OSI_MGBE_MAX_NUM_CHANS - 1
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - rx_swcx->buf_phy_addr need to be filled with DMA mapped address
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: Yes
 *  - Signal handler: Yes
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 * - invalid argument
 * - invalid tail pointer
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_003
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_rx_dma_desc_init (
  struct osi_dma_priv_data  *osi_dma,
  struct osi_rx_ring        *rx_ring,
  nveu32_t                  chan
  );

/**
 * @brief
 * Description: Updates rx buffer length.
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 * - Valid range: Any valid memory address except NULL.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 * - invalid argument
 * - invalid mtu setting
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_004
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_set_rx_buf_len (
  struct osi_dma_priv_data  *osi_dma
  );

/**
 * @brief
 * Description: Initialize Tx DMA descriptors for a channel
 *
 * @param[in, out] osi_dma: OSI DMA private data.
 * - Valid range: Any valid memory address except NULL.
 * @param[in] chan: DMA Tx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 * - Valid range: 0 to OSI_MGBE_MAX_NUM_CHANS - 1
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - DMA channel need to be started, see osi_start_dma
 *  - Need to set update tx_pkt_cx->flags accordingly as per the
 *    requirements
 *    NVETHERNETCL_PIF$OSI_PKT_CX_VLAN
 *    NVETHERNETCL_PIF$OSI_PKT_CX_CSUM
 *    NVETHERNETCL_PIF$OSI_PKT_CX_TSO
 *    NVETHERNETCL_PIF$OSI_PKT_CX_PTP
 *  - tx_pkt_cx->desc_cnt need to be populated which holds the number
 *    of swcx descriptors allocated for that packet
 *  - tx_swcx structure need to be filled for per packet with the
 *    buffer len, DMA mapped address of buffer for each descriptor
 *    consumed by the packet
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 * - invalid argument
 * - invalid dma channel number
 * - invalid tx ring
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_005
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_hw_transmit (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan
  );

/**
 * @brief
 * Description: Process Tx complete on DMA channel ring.
 *
 * @param[in, out] osi_dma: OSI dma private data structure.
 * - Valid range: Any valid memory address except NULL.
 * @param[in] chan: Channel number on which Tx complete need to be done.
 *            Max OSI_EQOS_MAX_NUM_CHANS.
 * - Valid range: 0 to OSI_MGBE_MAX_NUM_CHANS - 1
 * @param[in] budget: Threshold for reading the packets at a time.
 * - Valid range: >= 0
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - DMA need to be started, see osi_start_dma
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: Yes
 *  - Signal handler: Yes
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval >=0 Number of descriptors (buffers) processed on success else -1.
 * @retval -1 on failure
 * - invalid argument
 * - invalid dma channel number
 * - invalid tx ring
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_006
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_process_tx_completions (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan,
  nve32_t                   budget
  );

/**
 * @brief
 * Description: Read data from rx channel descriptors
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 * - Valid range: Any valid memory address except NULL.
 * @param[in] chan: Rx DMA channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 * - Valid range: 0 to OSI_MGBE_MAX_NUM_CHANS - 1
 * @param[in] budget: Threshold for reading the packets at a time.
 * - Valid range: >= 0
 * @param[out] more_data_avail: Pointer to more data available flag. OSI fills
 *         this flag if more rx packets available to read(1) or not(0).
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - DMA need to be started, see osi_start_dma
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: Yes
 *  - Signal handler: Yes
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval >=0 Number of descriptors (buffers) processed on success else -1.
 * @retval -1 on failure
 * - invalid argument
 * - invalid dma channel number
 * - invalid rx ring
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_007
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_process_rx_completions (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan,
  nve32_t                   budget,
  nveu32_t                  *more_data_avail
  );

/**
 * @brief
 * Description: Initialize DMA
 *
 * @param[in, out] osi_dma: OSI DMA private data.
 * - Valid range: Any valid memory address except NULL.
 *
 * @pre
 *  - Allocate memory for osi_dma
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - Number of dma channels osi_dma->num_dma_chans
 *  - channel list osi_dma->dma_chan
 *  - base address osi_dma->base
 *  - allocate tx ring osi_dma->tx_ring[chan] for each channel
 *    based on NVETHERNETCL_PIF$OSI_EQOS_TX_DESC_CNT/NVETHERNETCL_PIF$OSI_MGBE_TX_DESC_CNT
 *  - allocate tx descriptors osi_dma->tx_ring[chan]->tx_desc for all
 *    channels and dma map it.
 *  - allocate tx sw descriptors osi_dma->tx_ring[chan]->tx_swcx for all
 *    channels
 *  - allocate rx ring osi_dma->rx_ring[chan] for each channel
 *    based on RX_DESC_CNT (256)
 *  - allocate rx descriptors osi_dma->rx_ring[chan]->rx_desc for all
 *    channels and dma map it.
 *  - allocate rx sw descriptors osi_dma->rx_ring[chan]->rx_swcx for all
 *    channels
 *  - osi_dma->use_riwt  ==> NVETHERNETCL_PIF$OSI_DISABLE/NVETHERNETCL_PIF$OSI_ENABLE
 *  - osi_dma->rx_riwt  ===> Actual value read from DT
 *  - osi_dma->use_rx_frames  ==> NVETHERNETCL_PIF$OSI_DISABLE/NVETHERNETCL_PIF$OSI_ENABLE
 *  - osi_dma->rx_frames ===> Actual value read from DT
 *
 * @note
 * - This API also indirectly programs Tx PBL. It must be made sure that
 *   the Tx FIFO is deep enough to store a complete packet before that packet
 *   is transferred to the MAC transmitter. The reason being that when space
 *   is not available to accept the programmed burst length of data, then the
 *   MTL Tx FIFO starts reading to avoid dead-lock. In such a case, the COE
 *   fails as the start of the packet header is read out before the payload
 *   checksum can be calculated and inserted.It must enable checksum insertion
 *   only in the packets that are less than the number of bytes, given by the
 *   following equation:
 *
 *   Packet size < TxQSize - (PBL + N)*(DATAWIDTH/8),
 *
 *   where, if Datawidth = 32, N = 7, elseif Datawidth != 32, N = 5
 *   and Packet size is determined by the osi_dma->mtu.
 *
 *   The above is applicable only for Orin as PBL setting is per DMA channel.
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 * - invalid argument
 * - invalid MAC version
 * - invalid number of DMA channels
 * - invalid DMA channels
 * @retval !=0 on failure - failure to init DMA descriptors
 * @retval <0 on failure
 * - failure to init tx interrupt
 * - failure to init rx interrupt
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_008
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_hw_dma_init (
  struct osi_dma_priv_data  *osi_dma
  );

/**
 * @brief
 * Description: De initialize DMA
 *
 * @param[in] osi_dma: OSI DMA private data.
 * - Valid range: Any valid memory address except NULL.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: No
 *  - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure
 * - invalid argument
 * - invalid number of DMA channels
 * - invalid DMA channels
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_009
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_hw_dma_deinit (
  struct osi_dma_priv_data  *osi_dma
  );

/**
 * @brief
 * Description: Initialize DMA operations
 *
 * @param[in, out] osi_dma: OSI DMA private data.
 * - Valid range: Any valid memory address except NULL.
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: No
 *
 *
 * @retval 0 on success
 * @retval -1 on failure
 * - invalid argument
 * - invalid MAC HW type
 * - invalid tx ring size
 * - invalid rx ring size
 * - failed to init dma ops
 * - dma ops validation failed
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_010
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_init_dma_ops (
  struct osi_dma_priv_data  *osi_dma
  );

/**
 * @brief
 * Description: Get system time
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * - Valid range: Any valid memory address except NULL.
 * @param[out] sec: Value read in Seconds
 * - Valid range: Any valid memory address except NULL.
 * @param[out] nsec: Value read in Nano seconds
 * - Valid range: Any valid memory address except NULL.
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure - invalid argument
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_011
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_dma_get_systime_from_mac (
  struct osi_dma_priv_data *const  osi_dma,
  nveu32_t                         *sec,
  nveu32_t                         *nsec
  );

#ifndef OSI_STRIPPED_LIB

/**
 * @brief
 * Description: Checks if MAC is enabled.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * - Valid range: Any valid memory address except NULL.
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: No
 *
 * @retval NVETHERNETCL_PIF$OSI_ENABLE if MAC enabled.
 * @retval NVETHERNETCL_PIF$OSI_DISABLE on error
 * - invalid argument
 */
  #ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_012
 *
 */
  #else

/**
 *
 * @dir
 *  forward
 *
 */
  #endif
nveu32_t
osi_is_mac_enabled (
  struct osi_dma_priv_data *const  osi_dma
  );

#endif

/**
 * @brief
 * Description: Handles DMA interrupts.
 *
 * @param[in] osi_dma: OSI DMA private data.
 * - Valid range: Any valid memory address except NULL.
 * @param[in] chan: DMA Rx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 * - Valid range: 0 to OSI_MGBE_MAX_NUM_CHANS - 1
 * @param[in] tx_rx: Indicates whether DMA channel is Tx or Rx.
 *                   NVETHERNETCL_PIF$OSI_DMA_CH_TX_INTR for Tx interrupt.
 *                   NVETHERNETCL_PIF$OSI_DMA_CH_RX_INTR for Rx interrupt.
 * - Valid range: OSI_DMA_CH_TX_INTR or OSI_DMA_CH_RX_INTR
 * @param[in] en_dis: Enable/Disable DMA channel interrupts.
 *                    NVETHERNETCL_PIF$OSI_DMA_INTR_DISABLE for disabling the interrupt.
 *                    NVETHERNETCL_PIF$OSI_DMA_INTR_ENABLE for enabling the interrupt.
 * - Valid range: OSI_DMA_INTR_DISABLE or OSI_DMA_INTR_ENABLE
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - Mapping of physical IRQ line to DMA channel need to be maintained at
 *    OS Dependent layer and pass corresponding channel number.
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: Yes
 *  - Signal handler: Yes
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 * - invalid argument
 * - failed to enable or disable interrupt
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_013
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_handle_dma_intr (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan,
  nveu32_t                  tx_rx,
  nveu32_t                  en_dis
  );

// #ifdef OSI_DEBUG

/**
 * @brief
 * Description: OSI DMA IOCTL
 *
 * @param[in] osi_dma: OSI DMA private data.
 * - Valid range: Any valid memory address except NULL.
 *
 */
#ifndef DOXYGEN_ICD

/**
 * - API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 */
#endif

/**
 * @retval 0 on success
 * @retval -1 on failure
 * - invalid argument
 * - invalid ioctl command within osi data structure
 */
nve32_t
osi_dma_ioctl (
  struct osi_dma_priv_data  *osi_dma
  );

// #endif /* OSI_DEBUG */
#ifndef OSI_STRIPPED_LIB

/**
 * @brief
 * Description: Clear tx packet error stats.
 *
  * @param[in, out] osi_dma: OSI DMA private data structure.
 * - Valid range: Any valid memory address except NULL.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure - invalid argument
 */
nve32_t
osi_clear_tx_pkt_err_stats (
  struct osi_dma_priv_data  *osi_dma
  );

/**
 * @brief
 * Description: Configure slot function
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 * - Valid range: Any valid memory address except NULL.
 * @param[in] set: Flag to set with OSI_ENABLE and reset with OSI_DISABLE
 * - Valid range: OSI_ENABLE or OSI_DISABLE
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 * - invalid argument
 * - invalid slot interval argument
 * - tx ring is full
 */
nve32_t
osi_config_slot_function (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  set
  );

/**
 * @brief
 * Description: Clear rx packet error stats.
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 * - Valid range: Any valid memory address except NULL.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 * - API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure - invalid argument
 */
nve32_t
osi_clear_rx_pkt_err_stats (
  struct osi_dma_priv_data  *osi_dma
  );

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief
 * Description: Check if Txring is empty.
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: Yes
 *  - Signal handler: Yes
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * - Valid range: Any valid memory address except NULL.
 * @param[in] chan: Channel number whose ring is to be checked.
 * - Valid range: 0 to OSI_MGBE_MAX_NUM_CHANS - 1
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 * - API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 1 if ring is empty.
 * @retval 0 if ring has outstanding packets.
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_014
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
nve32_t
osi_txring_empty (
  struct osi_dma_priv_data  *osi_dma,
  nveu32_t                  chan
  );

/**
 * @brief
 * Description: Get pointer to osi_dma data structure.
 *
 * @pre OSD layer should use this as first API to get osi_dma pointer and
 * use the same in remaning API invocation.
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: No
 *
 * @retval !=NULL Valid and unique osi_dma pointer on success
 * @retval NULL on failure.
 */
#ifndef DOXYGEN_ICD

/**
 *
 * Traceability Details:
 * - SWUD_ID: NET_SWUD_TAG_NVETHERNETCL_015
 *
 */
#else

/**
 *
 * @dir
 *  forward
 *
 */
#endif
struct osi_dma_priv_data *
osi_get_dma (
  void
  );

#ifdef FSI_EQOS_SUPPORT

/**
 * @brief
 * Description: Release osi_dma data structure.
 *
 * @pre OSD layer should use this as last API to release osi_dma pointer and
 * shall not use the same after release dma resource
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: No
 *  - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t
osi_release_dma (
  struct osi_dma_priv_data  *osi_dma
  );

#endif /* FSI_EQOS_SUPPORT */

#endif /* INCLUDED_OSI_DMA_H */
