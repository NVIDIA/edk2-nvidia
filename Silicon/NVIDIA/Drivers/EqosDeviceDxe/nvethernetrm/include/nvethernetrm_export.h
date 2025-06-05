/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INCLUDED_NVETHERNETRM_EXPORT_H
#define INCLUDED_NVETHERNETRM_EXPORT_H

#include <nvethernet_type.h>

/**
 * @addtogroup Helper MACROS
 *
 * @brief EQOS generic helper MACROS.
 * @{
 */
/**
 * @brief size of GCL-256
 */
#define OSI_GCL_SIZE_256  256U

/**
 * @brief Maximum Traffic Classes supported
 */
#define OSI_MAX_TC_NUM  8U

/**
 * @brief Ethernet Address length
 */
#define OSI_ETH_ALEN  6U
/** @} */

/**
 * @brief FRP data matching length
 */
#define OSI_FRP_MATCH_DATA_MAX  12U
#define OSI_FRP_MATCH_DATA_MIN  1U

/**
 * @brief Maximum FRP indexes
 */
#define OSI_FRP_ID_MAX  0xFF

/**
 * @addtogroup MTL queue operation mode
 *
 * @brief MTL queue operation mode options
 * @{
 */
/** @brief MTL queue operation mode is AVB */
#define OSI_MTL_QUEUE_AVB  0x1U

/** @brief MTL queue operation mode is enable and
 * credit control is disabled */
#define OSI_MTL_QUEUE_ENABLE   0x2U
#define OSI_MTL_QUEUE_MODEMAX  0x3U
#ifndef OSI_STRIPPED_LIB
#define OSI_MAX_NUM_CHANS  48U
#endif
/** @} */

/**
 * @addtogroup EQOS_MTL MTL queue AVB algorithm mode
 *
 * @brief MTL AVB queue algorithm type
 * @{
 */
/** @brief AVB algorithm mode is CBS */
#define OSI_MTL_TXQ_AVALG_CBS  1U
/** @brief AVB algorithm mode is Strict Priority */
#define OSI_MTL_TXQ_AVALG_SP  0U
/** @} */

#ifndef OSI_STRIPPED_LIB

/**
 * @addtogroup Helper MACROS
 *
 * @brief EQOS generic helper MACROS.
 * @{
 */
/* L2 DA filter mode(enable/disable) */
#define OSI_OPER_EN_L2_DA_INV   OSI_BIT(4)
#define OSI_OPER_DIS_L2_DA_INV  OSI_BIT(5)
/** @} */
#endif /* !OSI_STRIPPED_LIB */

#pragma pack(push, 1)

/**
 * @brief FRP command structure for OSD to OSI
 */
struct osi_core_frp_cmd {
  /** FRP Command type
   * Valid values are NVETHERNETRM_PIF$OSI_FRP_CMD_ADD or
   * NVETHERNETRM_PIF$OSI_FRP_CMD_UPDATE or
   * NVETHERNETRM_PIF$OSI_FRP_CMD_DEL */
  nveu32_t    cmd;
  /** OSD FRP ID, valid values are from 0 to NVETHERNETRM_PIF$OSI_FRP_ID_MAX */
  nve32_t     frp_id;

  /** OSD match data type
   * valid values are from NVETHERNETRM_PIF$OSI_FRP_MATCH_NORMAL
   * to NVETHERNETRM_PIF$OSI_FRP_MATCH_VLAN*/
  nveu8_t     match_type;
  /** OSD match data */
  nveu8_t     match[OSI_FRP_MATCH_DATA_MAX];

  /** OSD match data length
   * valid value is from NVETHERNETRM_PIF$OSI_FRP_MATCH_DATA_MIN to
   * NVETHERNETRM_PIF$OSI_FRP_MATCH_DATA_MAX
   */
  nveu8_t     match_length;

  /** OSD Offset
   * Valid values are from 0 to (NVETHERNETRM_PIF$OSI_FRP_OFFSET_MAX-1)
   */
  nveu8_t     offset;

  /** OSD FRP filter mode flag
   * Valid values are from NVETHERNETRM_PIF$OSI_FRP_MODE_ROUTE
   * to NVETHERNETRM_PIF$OSI_FRP_MODE_IM_LINK*/
  nveu8_t     filter_mode;

  /** OSD FRP Link ID
   * valid values are from 0 to NVETHERNETRM_PIF$OSI_FRP_ID_MAX
   */
  nve32_t     next_frp_id;

  /** OSD DMA Channel Selection
   * Bit selection of DMA channels to route the frame
   * Bit[0] - DMA channel 0
   * ..
   * Bit [N] - DMA channel N] */
  nveu64_t    dma_sel;
  /** OSD DCHT */
  nveu8_t     dcht;
};

/**
 * @brief OSI Core avb data structure per queue.
 */
struct  osi_core_avb_algorithm {
  /** TX Queue/TC index
   * valid range  0 to NVETHERNETRM_PIF$OSI_MGBE_MAX_NUM_QUEUES for MGBE
   * valid range 0 to NVETHERNETRM_PIF$OSI_EQOS_MAX_NUM_CHANS for EQOS */
  nveu32_t    qindex;

  /** CBS Algorithm is either NVETHERNETRM_PIF$OSI_MTL_TXQ_AVALG_CBS or
   * NVETHERNETRM_PIF$OSI_MTL_TXQ_AVALG_SP */
  nveu32_t    algo;

  /** When this bit is set, the accumulated credit parameter in the
   * credit-based shaper algorithm logic is not reset to zero when
   * there is positive credit and no packet to transmit in the channel.
   *
   * Expected values are enable(1) or disable(0) */
  nveu32_t    credit_control;

  /** idleSlopeCredit value required for CBS
   * Max value for EQOS - 0x000FFFFFU
   * Max value for MGBE - 0x001FFFFFU */
  nveu32_t    idle_slope;

  /** sendSlopeCredit value required for CBS
   * Max value for EQOS - 0x0000FFFFU
   * Max value for MGBE - 0x00003FFFU */
  nveu32_t    send_slope;

  /** hiCredit value required for CBS
   * Max value - 0x1FFFFFFFU */
  nveu32_t    hi_credit;

  /** lowCredit value required for CBS
   * Max value - 0x1FFFFFFFU */
  nveu32_t    low_credit;

  /** Transmit queue operating mode
   * either disable(0) or NVETHERNETRM_PIF$OSI_MTL_QUEUE_AVB or
   * NVETHERNETRM_PIF$OSI_MTL_QUEUE_AVB*/
  nveu32_t    oper_mode;
  /** Traffic Classes from 0 to NVETHERNETRM_PIF$OSI_MAX_TC_NUM-1 */
  nveu32_t    tcindex;
};

/**
 * @brief OSI Core EST structure
 */
struct osi_est_config {
  /** Valid values ate 0 and 1
   * o to disable EST and 1 to enable EST */
  nveu32_t    en_dis;

  /** 64 bit base time register
   * if both values are 0, take ptp time to avoid BTRE
   * index 0 for nsec, index 1 for sec
   * Valid values are from 0 to UNIT32_MAX for each index
   */
  nveu32_t    btr[2];

  /** 64 bit base time offset index 0 for nsec, index 1 for sec
   * 32 bits for Seconds, 32 bits for nanoseconds (max 10^9) */
  nveu32_t    btr_offset[2];

  /** 40 bits cycle time register, index 0 for nsec, index 1 for sec
   * 8 bits for Seconds, 32 bits for nanoseconds (max 10^9) */
  nveu32_t    ctr[2];

  /** Configured Time Interval width(24 bits) + 7 bits
   * extension register
   * Valid values are from 1 to 0x7FFFFFFFU*/
  nveu32_t    ter;

  /** size of the gate control list Max 256 entries
   * valid value range (1-255)*/
  nveu32_t    llr;

  /** data array 8 bit gate op + 24 execution time
   * MGBE HW support GCL depth 256 */
  nveu32_t    gcl[OSI_GCL_SIZE_256];
};

/**
 * @brief OSI Core FPE structure
 */
struct osi_fpe_config {
  /** Queue Mask 1 - preemption 0 - express
   * bit representation for each queue
   * valud values are from 1 to 0xFF*/
  nveu32_t    tx_queue_preemption_enable;

  /** residual queues for all preemptable packets  which are not filtered
   * based on user priority or SA-DA
   * Value range for EQOS 1 to NVETHERNETRM_PIF$OSI_EQOS_MAX_NUM_QUEUES-1
   * Value range for MGBE 1 to NVETHERNETRM_PIF$OSI_MGBE_MAX_NUM_QUEUES-1 */
  nveu32_t    rq;
};

/**
 * @brief OSI Core error stats structure
 */
struct osi_stats {
  /** Constant Gate Control Error
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    const_gate_ctr_err;

  /** Head-Of-Line Blocking due to Scheduling
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    head_of_line_blk_sch;

  /** Per TC Schedule Error
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    hlbs_q[OSI_MAX_TC_NUM];

  /** Head-Of-Line Blocking due to Frame Size
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    head_of_line_blk_frm;

  /** Per TC Frame Size Error/
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    hlbf_q[OSI_MAX_TC_NUM];

  /** BTR Error
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    base_time_reg_err;

  /** Switch to Software Owned List Complete
   * Valid values are from 0 to UINT64_MAX */
  nveu64_t    sw_own_list_complete;
 #ifndef OSI_STRIPPED_LIB
  /** IP Header Error */
  nveu64_t    mgbe_ip_header_err;
  /** Jabber time out Error */
  nveu64_t    mgbe_jabber_timeout_err;
  /** Payload Checksum Error */
  nveu64_t    mgbe_payload_cs_err;
  /** Under Flow Error */
  nveu64_t    mgbe_tx_underflow_err;
  /** RX buffer unavailable irq count */
  nveu64_t    rx_buf_unavail_irq_n[OSI_MAX_NUM_CHANS];
  /** Transmit Process Stopped irq count */
  nveu64_t    tx_proc_stopped_irq_n[OSI_MAX_NUM_CHANS];
  /** Transmit Buffer Unavailable irq count */
  nveu64_t    tx_buf_unavail_irq_n[OSI_MAX_NUM_CHANS];
  /** Receive Process Stopped irq count */
  nveu64_t    rx_proc_stopped_irq_n[OSI_MAX_NUM_CHANS];
  /** Receive Watchdog Timeout irq count */
  nveu64_t    rx_watchdog_irq_n;
  /** Fatal Bus Error irq count */
  nveu64_t    fatal_bus_error_irq_n;
  /** lock fail count node addition */
  nveu64_t    ts_lock_add_fail;
  /** lock fail count node removal */
  nveu64_t    ts_lock_del_fail;
 #endif
};

/**
 * @brief osi_mmc_counters - The structure to hold RMON counter values
 */
struct osi_mmc_counters {
  /** This counter provides the number of bytes transmitted, exclusive of
   * preamble and retried bytes, in good and bad packets */
  nveu64_t    mmc_tx_octetcount_gb;
  /** This counter provides upper 32 bits of transmitted octet count */
  nveu64_t    mmc_tx_octetcount_gb_h;

  /** This counter provides the number of good and
   * bad packets transmitted, exclusive of retried packets */
  nveu64_t    mmc_tx_framecount_gb;

  /** This counter provides upper 32 bits of transmitted good and bad
   * packets count */
  nveu64_t    mmc_tx_framecount_gb_h;

  /** This counter provides number of good broadcast
   * packets transmitted */
  nveu64_t    mmc_tx_broadcastframe_g;

  /** This counter provides upper 32 bits of transmitted good broadcast
   * packets count */
  nveu64_t    mmc_tx_broadcastframe_g_h;

  /** This counter provides number of good multicast
   * packets transmitted */
  nveu64_t    mmc_tx_multicastframe_g;

  /** This counter provides upper 32 bits of transmitted good multicast
   * packet count */
  nveu64_t    mmc_tx_multicastframe_g_h;

  /** This counter provides the number of good and bad packets
   * transmitted with length 64 bytes, exclusive of preamble and
   * retried packets */
  nveu64_t    mmc_tx_64_octets_gb;

  /** This counter provides upper 32 bits of transmitted 64 octet size
   * good and bad packets count */
  nveu64_t    mmc_tx_64_octets_gb_h;

  /** This counter provides the number of good and bad packets
   * transmitted with length 65-127 bytes, exclusive of preamble and
   * retried packets */
  nveu64_t    mmc_tx_65_to_127_octets_gb;

  /** Provides upper 32 bits of transmitted 65-to-127 octet size good and
   * bad packets count */
  nveu64_t    mmc_tx_65_to_127_octets_gb_h;

  /** This counter provides the number of good and bad packets
   * transmitted with length 128-255 bytes, exclusive of preamble and
   * retried packets */
  nveu64_t    mmc_tx_128_to_255_octets_gb;

  /** This counter provides upper 32 bits of transmitted 128-to-255
   * octet size good and bad packets count */
  nveu64_t    mmc_tx_128_to_255_octets_gb_h;

  /** This counter provides the number of good and bad packets
   * transmitted with length 256-511 bytes, exclusive of preamble and
   * retried packets */
  nveu64_t    mmc_tx_256_to_511_octets_gb;

  /** This counter provides upper 32 bits of transmitted 256-to-511
   * octet size good and bad packets count. */
  nveu64_t    mmc_tx_256_to_511_octets_gb_h;

  /** This counter provides the number of good and bad packets
   * transmitted with length 512-1023 bytes, exclusive of preamble and
   * retried packets */
  nveu64_t    mmc_tx_512_to_1023_octets_gb;

  /** This counter provides upper 32 bits of transmitted 512-to-1023
   * octet size good and bad packets count.*/
  nveu64_t    mmc_tx_512_to_1023_octets_gb_h;

  /** This counter provides the number of good and bad packets
   * transmitted with length 1024-max bytes, exclusive of preamble and
   * retried packets */
  nveu64_t    mmc_tx_1024_to_max_octets_gb;

  /** This counter provides upper 32 bits of transmitted 1024-tomaxsize
   * octet size good and bad packets count. */
  nveu64_t    mmc_tx_1024_to_max_octets_gb_h;
  /** This counter provides the number of good and bad unicast packets */
  nveu64_t    mmc_tx_unicast_gb;

  /** This counter provides upper 32 bits of transmitted good bad
   * unicast packets count */
  nveu64_t    mmc_tx_unicast_gb_h;

  /** This counter provides the number of good and bad
   * multicast packets */
  nveu64_t    mmc_tx_multicast_gb;

  /** This counter provides upper 32 bits of transmitted good bad
   * multicast packets count */
  nveu64_t    mmc_tx_multicast_gb_h;

  /** This counter provides the number of good and bad
   * broadcast packets */
  nveu64_t    mmc_tx_broadcast_gb;

  /** This counter provides upper 32 bits of transmitted good bad
   * broadcast packets count */
  nveu64_t    mmc_tx_broadcast_gb_h;

  /** This counter provides the number of abort packets due to
   * underflow error */
  nveu64_t    mmc_tx_underflow_error;

  /** This counter provides upper 32 bits of abort packets due to
   * underflow error */
  nveu64_t    mmc_tx_underflow_error_h;

  /** This counter provides the number of successfully transmitted
   * packets after a single collision in the half-duplex mode */
  nveu64_t    mmc_tx_singlecol_g;

  /** This counter provides the number of successfully transmitted
   * packets after a multi collision in the half-duplex mode */
  nveu64_t    mmc_tx_multicol_g;

  /** This counter provides the number of successfully transmitted
   * after a deferral in the half-duplex mode */
  nveu64_t    mmc_tx_deferred;

  /** This counter provides the number of packets aborted because of
   * late collision error */
  nveu64_t    mmc_tx_latecol;

  /** This counter provides the number of packets aborted because of
   * excessive (16) collision errors */
  nveu64_t    mmc_tx_exesscol;

  /** This counter provides the number of packets aborted because of
   * carrier sense error (no carrier or loss of carrier) */
  nveu64_t    mmc_tx_carrier_error;

  /** This counter provides the number of bytes transmitted,
   * exclusive of preamble, only in good packets */
  nveu64_t    mmc_tx_octetcount_g;

  /** This counter provides upper 32 bytes of bytes transmitted,
   * exclusive of preamble, only in good packets */
  nveu64_t    mmc_tx_octetcount_g_h;
  /** This counter provides the number of good packets transmitted */
  nveu64_t    mmc_tx_framecount_g;
  /** This counter provides upper 32 bytes of good packets transmitted */
  nveu64_t    mmc_tx_framecount_g_h;

  /** This counter provides the number of packets aborted because of
   * excessive deferral error
   * (deferred for more than two max-sized packet times) */
  nveu64_t    mmc_tx_excessdef;

  /** This counter provides the number of good Pause
   * packets transmitted */
  nveu64_t    mmc_tx_pause_frame;

  /** This counter provides upper 32 bytes of good Pause
   * packets transmitted */
  nveu64_t    mmc_tx_pause_frame_h;
  /** This counter provides the number of good VLAN packets transmitted */
  nveu64_t    mmc_tx_vlan_frame_g;

  /** This counter provides upper 32 bytes of good VLAN packets
   * transmitted */
  nveu64_t    mmc_tx_vlan_frame_g_h;

  /** This counter provides the number of packets transmitted without
   * errors and with length greater than the maxsize (1,518 or 1,522 bytes
   * for VLAN tagged packets; 2000 bytes */
  nveu64_t    mmc_tx_osize_frame_g;
  /** This counter provides the number of good and bad packets received */
  nveu64_t    mmc_rx_framecount_gb;

  /** This counter provides upper 32 bytes of good and bad packets
   * received */
  nveu64_t    mmc_rx_framecount_gb_h;

  /** This counter provides the number of bytes received by DWC_ther_qos,
   * exclusive of preamble, in good and bad packets */
  nveu64_t    mmc_rx_octetcount_gb;

  /** This counter provides upper 32 bytes of bytes received by
   * DWC_ether_qos, exclusive of preamble, in good and bad packets */
  nveu64_t    mmc_rx_octetcount_gb_h;

  /** This counter provides the number of bytes received by DWC_ether_qos,
   * exclusive of preamble, in good and bad packets */
  nveu64_t    mmc_rx_octetcount_g;

  /** This counter provides upper 32 bytes of bytes received by
   * DWC_ether_qos, exclusive of preamble, in good and bad packets */
  nveu64_t    mmc_rx_octetcount_g_h;

  /** This counter provides the number of good
   * broadcast packets received */
  nveu64_t    mmc_rx_broadcastframe_g;

  /** This counter provides upper 32 bytes of good
   * broadcast packets received */
  nveu64_t    mmc_rx_broadcastframe_g_h;

  /** This counter provides the number of good
   * multicast packets received */
  nveu64_t    mmc_rx_multicastframe_g;

  /** This counter provides upper 32 bytes of good
   * multicast packets received */
  nveu64_t    mmc_rx_multicastframe_g_h;

  /** This counter provides the number of packets
   * received with CRC error */
  nveu64_t    mmc_rx_crc_error;

  /** This counter provides upper 32 bytes of packets
   * received with CRC error */
  nveu64_t    mmc_rx_crc_error_h;

  /** This counter provides the number of packets received with
   * alignment (dribble) error. It is valid only in 10/100 mode */
  nveu64_t    mmc_rx_align_error;

  /** This counter provides the number of packets received  with
   * runt (length less than 64 bytes and CRC error) error */
  nveu64_t    mmc_rx_runt_error;

  /** This counter provides the number of giant packets received  with
   * length (including CRC) greater than 1,518 bytes (1,522 bytes for
   * VLAN tagged) and with CRC error */
  nveu64_t    mmc_rx_jabber_error;

  /** This counter provides the number of packets received  with length
   * less than 64 bytes, without any errors */
  nveu64_t    mmc_rx_undersize_g;

  /** This counter provides the number of packets received  without error,
   * with length greater than the maxsize */
  nveu64_t    mmc_rx_oversize_g;

  /** This counter provides the number of good and bad packets received
   * with length 64 bytes, exclusive of the preamble */
  nveu64_t    mmc_rx_64_octets_gb;

  /** This counter provides upper 32 bytes of good and bad packets
   * received with length 64 bytes, exclusive of the preamble */
  nveu64_t    mmc_rx_64_octets_gb_h;

  /** This counter provides the number of good and bad packets received
   * with length 65-127 bytes, exclusive of the preamble */
  nveu64_t    mmc_rx_65_to_127_octets_gb;

  /** This counter provides upper 32 bytes of good and bad packets
   * received with length 65-127 bytes, exclusive of the preamble */
  nveu64_t    mmc_rx_65_to_127_octets_gb_h;

  /** This counter provides the number of good and bad packets received
   * with length 128-255 bytes, exclusive of the preamble */
  nveu64_t    mmc_rx_128_to_255_octets_gb;

  /** This counter provides upper 32 bytes of good and bad packets
   * received with length 128-255 bytes, exclusive of the preamble */
  nveu64_t    mmc_rx_128_to_255_octets_gb_h;

  /** This counter provides the number of good and bad packets received
   * with length 256-511 bytes, exclusive of the preamble */
  nveu64_t    mmc_rx_256_to_511_octets_gb;

  /** This counter provides upper 32 bytes of good and bad packets
   * received with length 256-511 bytes, exclusive of the preamble */
  nveu64_t    mmc_rx_256_to_511_octets_gb_h;

  /** This counter provides the number of good and bad packets received
   * with length 512-1023 bytes, exclusive of the preamble */
  nveu64_t    mmc_rx_512_to_1023_octets_gb;

  /** This counter provides upper 32 bytes of good and bad packets
   * received with length 512-1023 bytes, exclusive of the preamble */
  nveu64_t    mmc_rx_512_to_1023_octets_gb_h;

  /** This counter provides the number of good and bad packets received
   * with length 1024-maxbytes, exclusive of the preamble */
  nveu64_t    mmc_rx_1024_to_max_octets_gb;

  /** This counter provides upper 32 bytes of good and bad packets
   * received with length 1024-maxbytes, exclusive of the preamble */
  nveu64_t    mmc_rx_1024_to_max_octets_gb_h;
  /** This counter provides the number of good unicast packets received */
  nveu64_t    mmc_rx_unicast_g;

  /** This counter provides upper 32 bytes of good unicast packets
   * received */
  nveu64_t    mmc_rx_unicast_g_h;

  /** This counter provides the number of packets received  with length
   * error (Length Type field not equal to packet size), for all packets
   * with valid length field */
  nveu64_t    mmc_rx_length_error;

  /** This counter provides upper 32 bytes of packets received  with
   * length error (Length Type field not equal to packet size), for all
   * packets with valid length field */
  nveu64_t    mmc_rx_length_error_h;

  /** This counter provides the number of packets received  with length
   * field not equal to the valid packet size (greater than 1,500 but
   * less than 1,536) */
  nveu64_t    mmc_rx_outofrangetype;

  /** This counter provides upper 32 bytes of packets received  with
   * length field not equal to the valid packet size (greater than 1,500
   * but less than 1,536) */
  nveu64_t    mmc_rx_outofrangetype_h;

  /** This counter provides the number of good and valid Pause packets
   * received */
  nveu64_t    mmc_rx_pause_frames;

  /** This counter provides upper 32 bytes of good and valid Pause packets
   * received */
  nveu64_t    mmc_rx_pause_frames_h;

  /** This counter provides the number of missed received packets
   * because of FIFO overflow in DWC_ether_qos */
  nveu64_t    mmc_rx_fifo_overflow;

  /** This counter provides upper 32 bytes of missed received packets
   * because of FIFO overflow in DWC_ether_qos */
  nveu64_t    mmc_rx_fifo_overflow_h;

  /** This counter provides the number of good and bad VLAN packets
   * received */
  nveu64_t    mmc_rx_vlan_frames_gb;

  /** This counter provides upper 32 bytes of good and bad VLAN packets
   * received */
  nveu64_t    mmc_rx_vlan_frames_gb_h;

  /** This counter provides the number of packets received with error
   * because of watchdog timeout error */
  nveu64_t    mmc_rx_watchdog_error;

  /** This counter provides the number of packets received with Receive
   * error or Packet Extension error on the GMII or MII interface */
  nveu64_t    mmc_rx_receive_error;

  /** This counter provides the number of packets received with Receive
   * error or Packet Extension error on the GMII or MII interface */
  nveu64_t    mmc_rx_ctrl_frames_g;

  /** This counter provides the number of microseconds Tx LPI is asserted
   * in the MAC controller */
  nveu64_t    mmc_tx_lpi_usec_cntr;

  /** This counter provides the number of times MAC controller has
   * entered Tx LPI. */
  nveu64_t    mmc_tx_lpi_tran_cntr;

  /** This counter provides the number of microseconds Rx LPI is asserted
   * in the MAC controller */
  nveu64_t    mmc_rx_lpi_usec_cntr;

  /** This counter provides the number of times MAC controller has
   * entered Rx LPI.*/
  nveu64_t    mmc_rx_lpi_tran_cntr;

  /** This counter provides the number of good IPv4 datagrams received
   * with the TCP, UDP, or ICMP payload */
  nveu64_t    mmc_rx_ipv4_gd;

  /** This counter provides upper 32 bytes of good IPv4 datagrams received
   * with the TCP, UDP, or ICMP payload */
  nveu64_t    mmc_rx_ipv4_gd_h;
  /** RxIPv4 Header Error Packets */
  nveu64_t    mmc_rx_ipv4_hderr;
  /** RxIPv4 of upper 32 bytes of Header Error Packets */
  nveu64_t    mmc_rx_ipv4_hderr_h;

  /** This counter provides the number of IPv4 datagram packets received
   * that did not have a TCP, UDP, or ICMP payload */
  nveu64_t    mmc_rx_ipv4_nopay;

  /** This counter provides upper 32 bytes of IPv4 datagram packets
   * received that did not have a TCP, UDP, or ICMP payload */
  nveu64_t    mmc_rx_ipv4_nopay_h;

  /** This counter provides the number of good IPv4 datagrams received
   * with fragmentation */
  nveu64_t    mmc_rx_ipv4_frag;

  /** This counter provides upper 32 bytes of good IPv4 datagrams received
   * with fragmentation */
  nveu64_t    mmc_rx_ipv4_frag_h;

  /** This counter provides the number of good IPv4 datagrams received
   * that had a UDP payload with checksum disabled */
  nveu64_t    mmc_rx_ipv4_udsbl;

  /** This counter provides upper 32 bytes of good IPv4 datagrams received
   * that had a UDP payload with checksum disabled */
  nveu64_t    mmc_rx_ipv4_udsbl_h;

  /** This counter provides the number of good IPv6 datagrams received
   * with the TCP, UDP, or ICMP payload */
  nveu64_t    mmc_rx_ipv6_gd_octets;

  /** This counter provides upper 32 bytes of good IPv6 datagrams received
   * with the TCP, UDP, or ICMP payload */
  nveu64_t    mmc_rx_ipv6_gd_octets_h;

  /** This counter provides the number of IPv6 datagrams received
   * with header (length or version mismatch) errors */
  nveu64_t    mmc_rx_ipv6_hderr_octets;

  /** This counter provides the number of IPv6 datagrams received
   * with header (length or version mismatch) errors */
  nveu64_t    mmc_rx_ipv6_hderr_octets_h;

  /** This counter provides the number of IPv6 datagram packets received
   * that did not have a TCP, UDP, or ICMP payload */
  nveu64_t    mmc_rx_ipv6_nopay_octets;

  /** This counter provides upper 32 bytes of IPv6 datagram packets
   * received that did not have a TCP, UDP, or ICMP payload */
  nveu64_t    mmc_rx_ipv6_nopay_octets_h;
  /* Protocols */

  /** This counter provides the number of good IP datagrams received by
   * DWC_ether_qos with a good UDP payload */
  nveu64_t    mmc_rx_udp_gd;

  /** This counter provides upper 32 bytes of good IP datagrams received
   * by DWC_ether_qos with a good UDP payload */
  nveu64_t    mmc_rx_udp_gd_h;

  /** This counter provides the number of good IP datagrams received by
   * DWC_ether_qos with a good UDP payload. This counter is not updated
   * when the RxIPv4_UDP_Checksum_Disabled_Packets counter is
   * incremented */
  nveu64_t    mmc_rx_udp_err;

  /** This counter provides upper 32 bytes of good IP datagrams received
   * by DWC_ether_qos with a good UDP payload. This counter is not updated
   * when the RxIPv4_UDP_Checksum_Disabled_Packets counter is
   * incremented */
  nveu64_t    mmc_rx_udp_err_h;

  /** This counter provides the number of good IP datagrams received
   * with a good TCP payload */
  nveu64_t    mmc_rx_tcp_gd;

  /** This counter provides the number of good IP datagrams received
   * with a good TCP payload */
  nveu64_t    mmc_rx_tcp_gd_h;

  /** This counter provides upper 32 bytes of good IP datagrams received
   * with a good TCP payload */
  nveu64_t    mmc_rx_tcp_err;

  /** This counter provides upper 32 bytes of good IP datagrams received
   * with a good TCP payload */
  nveu64_t    mmc_rx_tcp_err_h;

  /** This counter provides the number of good IP datagrams received
   * with a good ICMP payload */
  nveu64_t    mmc_rx_icmp_gd;

  /** This counter provides upper 32 bytes of good IP datagrams received
   * with a good ICMP payload */
  nveu64_t    mmc_rx_icmp_gd_h;

  /** This counter provides the number of good IP datagrams received
   * whose ICMP payload has a checksum error */
  nveu64_t    mmc_rx_icmp_err;

  /** This counter provides upper 32 bytes of good IP datagrams received
   * whose ICMP payload has a checksum error */
  nveu64_t    mmc_rx_icmp_err_h;

  /** This counter provides the number of bytes received by DWC_ether_qos
   * in good IPv4 datagrams encapsulating TCP, UDP, or ICMP data.
   * (Ethernet header, FCS, pad, or IP pad bytes are not included
   * in this counter */
  nveu64_t    mmc_rx_ipv4_gd_octets;

  /** This counter provides upper 32 bytes received by DWC_ether_qos
   * in good IPv4 datagrams encapsulating TCP, UDP, or ICMP data.
   * (Ethernet header, FCS, pad, or IP pad bytes are not included
   * in this counter */
  nveu64_t    mmc_rx_ipv4_gd_octets_h;

  /** This counter provides the number of bytes received in IPv4 datagram
   * with header errors (checksum, length, version mismatch). The value
   * in the Length field of IPv4 header is used to update this counter.
   * (Ethernet header, FCS, pad, or IP pad bytes are not included
   * in this counter */
  nveu64_t    mmc_rx_ipv4_hderr_octets;

  /** This counter provides upper 32 bytes received in IPv4 datagram
   * with header errors (checksum, length, version mismatch). The value
   * in the Length field of IPv4 header is used to update this counter.
   * (Ethernet header, FCS, pad, or IP pad bytes are not included
   * in this counter */
  nveu64_t    mmc_rx_ipv4_hderr_octets_h;

  /** This counter provides the number of bytes received in IPv4 datagram
   * that did not have a TCP, UDP, or ICMP payload. The value in the
   * Length field of IPv4 header is used to update this counter.
   * (Ethernet header, FCS, pad, or IP pad bytes are not included
   * in this counter */
  nveu64_t    mmc_rx_ipv4_nopay_octets;

  /** This counter provides upper 32 bytes received in IPv4 datagram
   * that did not have a TCP, UDP, or ICMP payload. The value in the
   * Length field of IPv4 header is used to update this counter.
   * (Ethernet header, FCS, pad, or IP pad bytes are not included
   * in this counter */
  nveu64_t    mmc_rx_ipv4_nopay_octets_h;

  /** This counter provides the number of bytes received in fragmented
   * IPv4 datagrams. The value in the Length field of IPv4 header is
   * used to update this counter. (Ethernet header, FCS, pad, or IP pad
   * bytes are not included in this counter */
  nveu64_t    mmc_rx_ipv4_frag_octets;

  /** This counter provides upper 32 bytes received in fragmented
   * IPv4 datagrams. The value in the Length field of IPv4 header is
   * used to update this counter. (Ethernet header, FCS, pad, or IP pad
   * bytes are not included in this counter */
  nveu64_t    mmc_rx_ipv4_frag_octets_h;

  /** This counter provides the number of bytes received in a UDP segment
   * that had the UDP checksum disabled. This counter does not count IP
   * Header bytes. (Ethernet header, FCS, pad, or IP pad bytes are not
   * included in this counter */
  nveu64_t    mmc_rx_ipv4_udsbl_octets;

  /** This counter provides upper 32 bytes received in a UDP segment
   * that had the UDP checksum disabled. This counter does not count IP
   * Header bytes. (Ethernet header, FCS, pad, or IP pad bytes are not
   * included in this counter */
  nveu64_t    mmc_rx_ipv4_udsbl_octets_h;

  /** This counter provides the number of bytes received in good IPv6
   * datagrams encapsulating TCP, UDP, or ICMP data. (Ethernet header,
   * FCS, pad, or IP pad bytes are not included in this counter */
  nveu64_t    mmc_rx_ipv6_gd;

  /** This counter provides upper 32 bytes received in good IPv6
   * datagrams encapsulating TCP, UDP, or ICMP data. (Ethernet header,
   * FCS, pad, or IP pad bytes are not included in this counter */
  nveu64_t    mmc_rx_ipv6_gd_h;

  /** This counter provides the number of bytes received in IPv6 datagrams
   * with header errors (length, version mismatch). The value in the
   * Length field of IPv6 header is used to update this counter.
   * (Ethernet header, FCS, pad, or IP pad bytes are not included in
   * this counter */
  nveu64_t    mmc_rx_ipv6_hderr;

  /** This counter provides upper 32 bytes received in IPv6 datagrams
   * with header errors (length, version mismatch). The value in the
   * Length field of IPv6 header is used to update this counter.
   * (Ethernet header, FCS, pad, or IP pad bytes are not included in
   * this counter */
  nveu64_t    mmc_rx_ipv6_hderr_h;

  /** This counter provides the number of bytes received in IPv6
   * datagrams that did not have a TCP, UDP, or ICMP payload. The value
   * in the Length field of IPv6 header is used to update this counter.
   * (Ethernet header, FCS, pad, or IP pad bytes are not included
   * in this counter */
  nveu64_t    mmc_rx_ipv6_nopay;

  /** This counter provides upper 32 bytes received in IPv6
   * datagrams that did not have a TCP, UDP, or ICMP payload. The value
   * in the Length field of IPv6 header is used to update this counter.
   * (Ethernet header, FCS, pad, or IP pad bytes are not included
   * in this counter */
  nveu64_t    mmc_rx_ipv6_nopay_h;
  /* Protocols */

  /** This counter provides the number of bytes received in a good UDP
   * segment. This counter does not count IP header bytes */
  nveu64_t    mmc_rx_udp_gd_octets;

  /** This counter provides upper 32 bytes received in a good UDP
   * segment. This counter does not count IP header bytes */
  nveu64_t    mmc_rx_udp_gd_octets_h;

  /** This counter provides the number of bytes received in a UDP
   * segment that had checksum errors. This counter does not count
   * IP header bytes */
  nveu64_t    mmc_rx_udp_err_octets;

  /** This counter provides upper 32 bytes received in a UDP
   * segment that had checksum errors. This counter does not count
   * IP header bytes */
  nveu64_t    mmc_rx_udp_err_octets_h;

  /** This counter provides the number of bytes received in a good
   * TCP segment. This counter does not count IP header bytes */
  nveu64_t    mmc_rx_tcp_gd_octets;

  /** This counter provides upper 32 bytes received in a good
   * TCP segment. This counter does not count IP header bytes */
  nveu64_t    mmc_rx_tcp_gd_octets_h;

  /** This counter provides the number of bytes received in a TCP
   * segment that had checksum errors. This counter does not count
   * IP header bytes */
  nveu64_t    mmc_rx_tcp_err_octets;

  /** This counter provides upper 32 bytes received in a TCP
   * segment that had checksum errors. This counter does not count
   * IP header bytes */
  nveu64_t    mmc_rx_tcp_err_octets_h;

  /** This counter provides the number of bytes received in a good
   * ICMP segment. This counter does not count IP header bytes */
  nveu64_t    mmc_rx_icmp_gd_octets;

  /** This counter provides upper 32 bytes received in a good
   * ICMP segment. This counter does not count IP header bytes */
  nveu64_t    mmc_rx_icmp_gd_octets_h;

  /** This counter provides the number of bytes received in a ICMP
   * segment that had checksum errors. This counter does not count
   * IP header bytes */
  nveu64_t    mmc_rx_icmp_err_octets;

  /** This counter provides upper 32 bytes received in a ICMP
   * segment that had checksum errors. This counter does not count
   * IP header bytes */
  nveu64_t    mmc_rx_icmp_err_octets_h;

  /** This counter provides the number of additional mPackets
   * transmitted due to preemption */
  nveu64_t    mmc_tx_fpe_frag_cnt;

  /** This counter provides the count of number of times a hold
   *  request is given to MAC */
  nveu64_t    mmc_tx_fpe_hold_req_cnt;

  /** This counter provides the number of MAC frames with reassembly
   *  errors on the Receiver, due to mismatch in the fragment
   *  count value */
  nveu64_t    mmc_rx_packet_reass_err_cnt;

  /** This counter the number of received MAC frames rejected
   *  due to unknown SMD value and MAC frame fragments rejected due
   *  to arriving with an SMD-C when there was no preceding preempted
   *  frame */
  nveu64_t    mmc_rx_packet_smd_err_cnt;

  /** This counter provides the number of MAC frames that were
   * successfully reassembled and delivered to MAC */
  nveu64_t    mmc_rx_packet_asm_ok_cnt;

  /** This counter provides the number of additional mPackets received
   *   due to preemption */
  nveu64_t    mmc_rx_fpe_fragment_cnt;
};

#pragma pack(pop)
#endif /* INCLUDED_NVETHERNETRM_EXPORT_H */
