/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INCLUDED_OSI_DMA_H
#define INCLUDED_OSI_DMA_H

#include <osi_common.h>
#include "osi_dma_txrx.h"

/*
 * @addtogroup Helper Helper MACROS
 *
 * @brief These flags are used for PTP time synchronization
 * @{
 */
#define OSI_PTP_SYNC_MASTER		OSI_BIT(0)
#define OSI_PTP_SYNC_SLAVE		OSI_BIT(1)
#define OSI_PTP_SYNC_ONESTEP		OSI_BIT(2)
#define OSI_PTP_SYNC_TWOSTEP		OSI_BIT(3)
#define OSI_DELAY_1US			1U
/** @} */

/**
 * @addtogroup Helper Helper MACROS
 *
 * @brief EQOS generic helper MACROS.
 * @{
 */
#define OSI_NET_IP_ALIGN	0x2U
#define NV_VLAN_HLEN		0x4U
#define OSI_ETH_HLEN		0xEU

#define OSI_INVALID_VALUE	0xFFFFFFFFU

#define OSI_ONE_MEGA_HZ		1000000U
#define OSI_ULLONG_MAX		(~0ULL)

/* Compiler hints for branch prediction */
#define osi_likely(x)			__builtin_expect(!!(x), 1)
/** @} */

/**
 * @addtogroup Channel Mask
 * @brief Chanel mask for Tx and Rx interrupts
 * @{
 */
#define OSI_VM_IRQ_TX_CHAN_MASK(x)	OSI_BIT((x) * 2U)
#define OSI_VM_IRQ_RX_CHAN_MASK(x)	OSI_BIT(((x) * 2U) + 1U)
/** @} */

/**
 * OSI error macro definition,
 * @param[in] priv: OSD private data OR NULL
 * @param[in] type: error type
 * @param[in] err:  error string
 * @param[in] loga: error additional information
 */
#define OSI_DMA_ERR(priv, type, err, loga)				\
{								\
	osi_dma->osd_ops.ops_log(priv, __func__, __LINE__,	\
				 OSI_LOG_ERR, type, err, loga);	\
}

#ifndef OSI_STRIPPED_LIB
/**
 * OSI info macro definition
 * @param[in] priv: OSD private data OR NULL
 * @param[in] type: error type
 * @param[in] err:  error string
 * @param[in] loga: error additional information
 */
#define OSI_DMA_INFO(priv, type, err, loga)				\
{								\
	osi_dma->osd_ops.ops_log(priv, __func__, __LINE__,	\
				 OSI_LOG_INFO, type, err, loga);\
}
#endif /* !OSI_STRIPPED_LIB */
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
#define OSI_PKT_CX_VLAN			OSI_BIT(0)
/** CSUM packet */
#define OSI_PKT_CX_CSUM			OSI_BIT(1)
/** TSO packet */
#define OSI_PKT_CX_TSO			OSI_BIT(2)
/** PTP packet */
#define OSI_PKT_CX_PTP			OSI_BIT(3)
/** Paged buffer */
#define OSI_PKT_CX_PAGED_BUF		OSI_BIT(4)
/** Rx packet has RSS hash */
#define OSI_PKT_CX_RSS			OSI_BIT(5)
/** Valid packet */
#define OSI_PKT_CX_VALID		OSI_BIT(10)
/** Update Packet Length in Tx Desc3 */
#define OSI_PKT_CX_LEN			OSI_BIT(11)
/** IP CSUM packet */
#define OSI_PKT_CX_IP_CSUM		OSI_BIT(12)
/** @} */

/**
 * @addtogroup SLOT function context fields
 *
 * @brief These flags are used for DMA channel Slot context configuration
 * @{
 */
#ifndef OSI_STRIPPED_LIB
#define OSI_SLOT_INTVL_DEFAULT		125U
#define OSI_SLOT_INTVL_MAX		4095U
#endif /* !OSI_STRIPPED_LIB */
#define OSI_SLOT_NUM_MAX		16U
/** @} */

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
#define OSI_TXDONE_CX_PAGED_BUF		OSI_BIT(0)
/** Flag to indicate if there was any tx error */
#define OSI_TXDONE_CX_ERROR		OSI_BIT(1)
/** Flag to indicate the availability of time stamp */
#define OSI_TXDONE_CX_TS		OSI_BIT(2)
/** Flag to indicate the delayed availability of time stamp */
#define OSI_TXDONE_CX_TS_DELAYED	OSI_BIT(3)
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
#define OSI_CHECKSUM_NONE		0x0U
#endif /* OSI_STRIPPED_LIB */
/* TCP header/payload */
#define OSI_CHECKSUM_TCPv4		OSI_BIT(0)
/* UDP header/payload */
#define OSI_CHECKSUM_UDPv4		OSI_BIT(1)
/* TCP/UDP checksum bad */
#define OSI_CHECKSUM_TCP_UDP_BAD	OSI_BIT(2)
/* IPv6 TCP header/payload */
#define OSI_CHECKSUM_TCPv6		OSI_BIT(4)
/* IPv6 UDP header/payload */
#define OSI_CHECKSUM_UDPv6		OSI_BIT(5)
/* IPv4 header */
#define OSI_CHECKSUM_IPv4		OSI_BIT(6)
/* IPv4 header checksum bad */
#define OSI_CHECKSUM_IPv4_BAD		OSI_BIT(7)
/* Checksum check not required */
#define OSI_CHECKSUM_UNNECESSARY	OSI_BIT(8)
/** @} */

/**
 * @addtogroup EQOS-RX Rx SW context flags
 *
 * @brief Flags to share info about the Rx SW context structure per descriptor
 * between OSI and OSD.
 * @{
 */
/* Rx swcx flags */
#define OSI_RX_SWCX_REUSE	OSI_BIT(0)
#define OSI_RX_SWCX_BUF_VALID	OSI_BIT(1)
/** Packet is processed by driver */
#define OSI_RX_SWCX_PROCESSED	OSI_BIT(3)

/** @} */


/**
 * @addtogroup RSS-HASH type
 *
 * @brief Macros to represent to type of packet for hash stored in receive packet
 * context.
 * @{
 */
#define OSI_RX_PKT_HASH_TYPE_L2	0x1U
#define OSI_RX_PKT_HASH_TYPE_L3	0x2U
#define OSI_RX_PKT_HASH_TYPE_L4	0x3U
/** @} */

/**
 * @addtogroup OSI-INTR OSI DMA interrupt handling macros.
 *
 * @brief Macros to pass osi_handle_dma_intr() API to handle
 * the interrupts between OSI and OSD.
 * @{
 */
#define OSI_DMA_CH_TX_INTR	0U
#define OSI_DMA_CH_RX_INTR	1U
#define OSI_DMA_INTR_DISABLE	0U
#define OSI_DMA_INTR_ENABLE	1U
/** @} */

/**
 * @addtogroup OSI_DMA-DEBUG helper macros
 *
 * @brief Helper macros for OSI dma debugging.
 * @{
 */
#ifdef OSI_DEBUG
#define OSI_DMA_IOCTL_CMD_REG_DUMP	1U
#define OSI_DMA_IOCTL_CMD_STRUCTS_DUMP	2U
#endif /* OSI_DEBUG */
/** @} */

/**
 * @brief OSI packet error stats
 */
struct osi_pkt_err_stats {
	/** IP Header Error */
	nveu64_t ip_header_error;
	/** Jabber time out Error */
	nveu64_t jabber_timeout_error;
	/** Packet Flush Error */
	nveu64_t pkt_flush_error;
	/** Payload Checksum Error */
	nveu64_t payload_cs_error;
	/** Loss of Carrier Error */
	nveu64_t loss_of_carrier_error;
	/** No Carrier Error */
	nveu64_t no_carrier_error;
	/** Late Collision Error */
	nveu64_t late_collision_error;
	/** Excessive Collision Error */
	nveu64_t excessive_collision_error;
	/** Excessive Deferal Error */
	nveu64_t excessive_deferal_error;
	/** Under Flow Error */
	nveu64_t underflow_error;
	/** Rx CRC Error */
	nveu64_t rx_crc_error;
	/** Rx Frame Error */
	nveu64_t rx_frame_error;
	/** clear_tx_pkt_err_stats() API invoked */
	nveu64_t clear_tx_err;
	/** clear_rx_pkt_err_stats() API invoked */
	nveu64_t clear_rx_err;
	/** FRP Parsed count, includes accept
	 * routing-bypass, or result-bypass count.
	 */
	unsigned long frp_parsed;
	/** FRP Dropped count */
	unsigned long frp_dropped;
	/** FRP Parsing Error count */
	unsigned long frp_err;
	/** FRP Incomplete Parsing */
	unsigned long frp_incomplete;
};

/**
 * @brief Receive Descriptor
 */
struct osi_rx_desc {
	/** Receive Descriptor 0 */
	nveu32_t rdes0;
	/** Receive Descriptor 1 */
	nveu32_t rdes1;
	/** Receive Descriptor 2 */
	nveu32_t rdes2;
	/** Receive Descriptor 3 */
	nveu32_t rdes3;
};

/**
 * @brief Receive descriptor software context
 */
struct osi_rx_swcx {
	/** DMA buffer physical address */
	nveu64_t buf_phy_addr;
	/** DMA buffer virtual address */
	void *buf_virt_addr;
	/** Length of buffer */
	nveu32_t len;
	/** Flags to share info about Rx swcx between OSD and OSI */
	nveu32_t flags;
};

/**
 * @brief - Received packet context. This is a single instance
 * and it is reused for all rx packets.
 */
struct osi_rx_pkt_cx {
	/** Bit map which holds the features that rx packets supports */
	nveu32_t flags;
	/** Stores the Rx csum */
	nveu32_t rxcsum;
	/** Stores the VLAN tag ID in received packet */
	nveu32_t vlan_tag;
	/** Length of received packet */
	nveu32_t pkt_len;
	/** Stores received packet hash */
	nveu32_t rx_hash;
	/** Store type of packet for which hash carries at rx_hash */
	nveu32_t rx_hash_type;
	/** TS in nsec for the received packet */
	nveul64_t ns;
};

/**
 * @brief DMA channel Rx ring. The number of instances depends on the
 * number of DMA channels configured
 */
struct osi_rx_ring {
	/** Pointer to Rx DMA descriptor */
	struct osi_rx_desc *rx_desc;
	/** Pointer to Rx DMA descriptor software context information */
	struct osi_rx_swcx *rx_swcx;
	/** Physical address of Rx DMA descriptor */
	nveu64_t rx_desc_phy_addr;
	/** Descriptor index current reception */
	nveu32_t cur_rx_idx;
	/** Descriptor index for descriptor re-allocation */
	nveu32_t refill_idx;
	/** Receive packet context */
	struct osi_rx_pkt_cx rx_pkt_cx;
};

/**
 *@brief Transmit descriptor software context
 */
struct osi_tx_swcx {
	/** Physical address of DMA mapped buffer */
	nveu64_t buf_phy_addr;
	/** Virtual address of DMA buffer */
	void *buf_virt_addr;
	/** Length of buffer */
	nveu32_t len;
	/** Flag to keep track of whether buffer pointed by buf_phy_addr
	 * is a paged buffer/linear buffer */
	nveu32_t is_paged_buf;
	/** Flag to keep track of SWCX
	 * Bit 0 is_paged_buf - whether buffer pointed by buf_phy_addr
	 * is a paged buffer/linear buffer
	 * Bit 1 PTP hwtime form timestamp registers */
	unsigned int flags;
	/** Packet id of packet for which TX timestamp needed */
	unsigned int pktid;
};

/**
 * @brief Transmit descriptor
 */
struct osi_tx_desc {
	/** Transmit descriptor 0 */
	nveu32_t tdes0;
	/** Transmit descriptor 1 */
	nveu32_t tdes1;
	/** Transmit descriptor 2 */
	nveu32_t tdes2;
	/** Transmit descriptor 3 */
	nveu32_t tdes3;
};

/**
 * @brief Transmit packet context for a packet. This is a single instance
 * and it is reused for all tx packets.
 */
struct osi_tx_pkt_cx {
	/** Holds the features which a Tx packets supports */
	nveu32_t flags;
	/** Stores the VLAN tag ID */
	nveu32_t vtag_id;
	/** Descriptor count */
	nveu32_t desc_cnt;
	/** Max. segment size for TSO/USO/GSO/LSO packet */
	nveu32_t mss;
	/** Length of application payload */
	nveu32_t payload_len;
	/** Length of transport layer tcp/udp header */
	nveu32_t tcp_udp_hdrlen;
	/** Length of all headers (ethernet/ip/tcp/udp) */
	nveu32_t total_hdrlen;
};

/**
 * @brief Transmit done packet context for a packet
 */
struct osi_txdone_pkt_cx {
	/** Indicates status flags for Tx complete (tx error occurred, or
	 * indicate whether desc had buf mapped from paged/linear memory etc) */
	nveu32_t flags;
	/** TS captured for the tx packet and this is valid only when the PTP
	 * bit is set in fields */
	nveul64_t ns;
	/** Passing packet id to map TX time to packet */
	unsigned int pktid;
};

/**
 * @brief DMA channel Tx ring. The number of instances depends on the
 * number of DMA channels configured
 */
struct osi_tx_ring {
	/** Pointer to tx dma descriptor */
	struct osi_tx_desc *tx_desc;
	/** Pointer to tx dma descriptor software context information */
	struct osi_tx_swcx *tx_swcx;
	/** Physical address of Tx descriptor */
	nveu64_t tx_desc_phy_addr;
	/** Descriptor index current transmission */
	nveu32_t cur_tx_idx;
	/** Descriptor index for descriptor cleanup */
	nveu32_t clean_idx;
	/** Slot function check */
	nveu32_t slot_check;
	/** Slot number */
	nveu32_t slot_number;
	/** Transmit packet context */
	struct osi_tx_pkt_cx tx_pkt_cx;
	/** Transmit complete packet context information */
	struct osi_txdone_pkt_cx txdone_pkt_cx;
	/** Number of packets or frames transmitted */
	nveu32_t frame_cnt;
};

/**
 * @brief osi_xtra_dma_stat_counters -  OSI DMA extra stats counters
 */
struct osi_xtra_dma_stat_counters {
	/** Per Q TX packet count */
	nveu64_t q_tx_pkt_n[OSI_MGBE_MAX_NUM_QUEUES];
	/** Per Q RX packet count */
	nveu64_t q_rx_pkt_n[OSI_MGBE_MAX_NUM_QUEUES];
	/** Per Q TX complete call count */
	nveu64_t tx_clean_n[OSI_MGBE_MAX_NUM_QUEUES];
	/** Total number of tx packets count */
	nveu64_t tx_pkt_n;
	/** Total number of rx packet count */
	nveu64_t rx_pkt_n;
	/** Total number of VLAN RX packet count */
	nveu64_t rx_vlan_pkt_n;
	/** Total number of VLAN TX packet count */
	nveu64_t tx_vlan_pkt_n;
	/** Total number of TSO packet count */
	nveu64_t tx_tso_pkt_n;
};

struct osi_dma_priv_data;

/**
 *@brief OSD DMA callbacks
 */
struct osd_dma_ops {
	/** DMA transmit complete callback */
	void (*transmit_complete)(void *priv, void *buffer,
				  nveu64_t dmaaddr, nveu32_t len,
				  struct osi_txdone_pkt_cx *txdone_pkt_cx);
	/** DMA receive packet callback */
	void (*receive_packet)(void *priv, struct osi_rx_ring *rx_ring,
			       nveu32_t chan, nveu32_t dma_buf_len,
			       struct osi_rx_pkt_cx *rx_pkt_cx,
			       struct osi_rx_swcx *rx_swcx);
	/** RX buffer reallocation callback */
	void (*realloc_buf)(void *priv, struct osi_rx_ring *rx_ring,
			    nveu32_t chan);
	/**.ops_log function callback */
	void (*ops_log)(void *priv, const nve8_t *func, nveu32_t line,
			nveu32_t level, nveu32_t type, const nve8_t *err,
			nveul64_t loga);
	/**.ops_log function callback */
	void (*udelay)(nveu64_t usec);
#ifdef OSI_DEBUG
	/**.printf function callback */
	void (*printf)(struct osi_dma_priv_data *osi_dma,
		       nveu32_t type,
		       const char *fmt, ...);
#endif /* OSI_DEBUG */
};

/**
 * @brief The OSI DMA IOCTL data structure.
 */
struct osi_dma_ioctl_data {
	/** IOCTL command number */
	nveu32_t cmd;
};

/**
 * @brief The OSI DMA private data structure.
 */
struct osi_dma_priv_data {
	/** Array of pointers to DMA Tx channel Ring */
	struct osi_tx_ring *tx_ring[OSI_MGBE_MAX_NUM_CHANS];
	/** Array of pointers to DMA Rx channel Ring */
	struct osi_rx_ring *rx_ring[OSI_MGBE_MAX_NUM_CHANS];
	/** Memory mapped base address of MAC IP */
	void *base;
	/** Pointer to OSD private data structure */
	void *osd;
	/** MAC HW type (EQOS) */
	nveu32_t mac;
	/** Number of channels enabled in MAC */
	nveu32_t num_dma_chans;
	/** Array of supported DMA channels */
	nveu32_t dma_chans[OSI_MGBE_MAX_NUM_CHANS];
	/** DMA Rx channel buffer length at HW level */
	nveu32_t rx_buf_len;
	/** MTU size */
	nveu32_t mtu;
	/** Packet error stats */
	struct osi_pkt_err_stats pkt_err_stats;
	/** Extra DMA stats */
	struct osi_xtra_dma_stat_counters dstats;
	/** Receive Interrupt Watchdog Timer Count Units */
	nveu32_t rx_riwt;
	/** Flag which decides riwt is enabled(1) or disabled(0) */
	nveu32_t use_riwt;
	/** Max no of pkts to be received before triggering Rx interrupt */
	nveu32_t rx_frames;
	/** Flag which decides rx_frames is enabled(1) or disabled(0) */
	nveu32_t use_rx_frames;
	/** Transmit Interrupt Software Timer Count Units */
	nveu32_t tx_usecs;
	/** Flag which decides Tx timer is enabled(1) or disabled(0) */
	nveu32_t use_tx_usecs;
	/** Max no of pkts to transfer before triggering Tx interrupt */
	nveu32_t tx_frames;
	/** Flag which decides tx_frames is enabled(1) or disabled(0) */
	nveu32_t use_tx_frames;
	/** Flag which decides virtualization is enabled(1) or disabled(0) */
	nveu32_t use_virtualization;
	/** Functional safety config to do periodic read-verify of
	 * certain safety critical dma registers */
	void *safety_config;
	/** Array of DMA channel slot snterval value from DT */
	nveu32_t slot_interval[OSI_MGBE_MAX_NUM_CHANS];
	/** Array of DMA channel slot enabled status from DT*/
	nveu32_t slot_enabled[OSI_MGBE_MAX_NUM_CHANS];
	/** DMA callback ops structure */
	struct osd_dma_ops osd_ops;
	/** Virtual address of reserved DMA buffer */
	void *resv_buf_virt_addr;
	/** Physical address of reserved DMA buffer */
	nveu64_t resv_buf_phy_addr;
	/** Tegra Pre-si platform info */
	nveu32_t pre_si;
	/** PTP flags
	 * bit 0 PTP mode master(1) slave(0)
	 * bit 1 PTP sync method twostep(1) onestep(0) */
	unsigned int ptp_flag;
	/** OSI DMA IOCTL data */
	struct osi_dma_ioctl_data ioctl_data;
#ifdef OSI_DEBUG
	/** Flag to enable/disable descriptor dump */
	nveu32_t enable_desc_dump;
#endif /* OSI_DEBUG */
};

/**
 * @brief osi_disable_chan_tx_intr - Disables DMA Tx channel interrupts.
 *
 * @note
 * Algorithm:
 *  - Disables Tx interrupts at wrapper level.
 *
 * @param[in] osi_dma: OSI DMA private data.
 * @param[in] chan: DMA Tx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - Mapping of physical IRQ line to DMA channel need to be maintained at
 *    OS Dependent layer and pass corresponding channel number.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_001
 *
 * @note
 * Classification:
 * - Interrupt: Yes
 * - Signal handler: Yes
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_disable_chan_tx_intr(struct osi_dma_priv_data *osi_dma,
				 nveu32_t chan);

/**
 * @brief osi_enable_chan_tx_intr - Enable DMA Tx channel interrupts.
 *
 * @note
 * Algorithm:
 *  - Enables Tx interrupts at wrapper level.
 *
 * @param[in] osi_dma: OSI DMA private data.
 * @param[in] chan: DMA Tx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - Mapping of physical IRQ line to DMA channel need to be maintained at
 *    OS Dependent layer and pass corresponding channel number.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_002
 *
 * @note
 * Classification:
 * - Interrupt: Yes
 * - Signal handler: Yes
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
nve32_t osi_enable_chan_tx_intr(struct osi_dma_priv_data *osi_dma,
				nveu32_t chan);

/**
 * @brief osi_disable_chan_rx_intr - Disable DMA Rx channel interrupts.
 *
 * @note
 * Algorithm:
 *  - Disables Rx interrupts at wrapper level.
 *
 * @param[in] osi_dma: OSI DMA private data.
 * @param[in] chan: DMA Rx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - Mapping of physical IRQ line to DMA channel need to be maintained at
 *    OS Dependent layer and pass corresponding channel number.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_003
 *
 * @note
 * Classification:
 * - Interrupt: Yes
 * - Signal handler: Yes
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_disable_chan_rx_intr(struct osi_dma_priv_data *osi_dma,
				 nveu32_t chan);

/**
 * @brief osi_enable_chan_rx_intr - Enable DMA Rx channel interrupts.
 *
 * @note
 * Algorithm:
 *  - Enables Rx interrupts at wrapper level.
 *
 * @param[in] osi_dma: OSI DMA private data.
 * @param[in] chan: DMA Rx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - Mapping of physical IRQ line to DMA channel need to be maintained at
 *    OS Dependent layer and pass corresponding channel number.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_004
 *
 * @note
 * Classification:
 * - Interrupt: Yes
 * - Signal handler: Yes
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
nve32_t osi_enable_chan_rx_intr(struct osi_dma_priv_data *osi_dma,
				nveu32_t chan);

/**
 * @brief osi_get_global_dma_status - Gets DMA status.
 *
 * Algorithm: Returns global DMA Tx/Rx interrupt status
 *
 * @param[in] osi_dma: DMA private data.
 *
 * @note
 *	Dependencies: None.
 *	Protection: None.
 *
 * @retval status
 */
nveu32_t osi_get_global_dma_status(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_clear_vm_tx_intr - Handles VM Tx interrupt source.
 *
 * Algorithm: Clear Tx interrupt source at wrapper level and DMA level.
 *
 * @param[in] osi_dma: DMA private data.
 * @param[in] chan: DMA tx channel number.
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_clear_vm_tx_intr(struct osi_dma_priv_data *osi_dma,
			     nveu32_t chan);

/**
 * @brief osi_clear_vm_rx_intr - Handles VM Rx interrupt source.
 *
 * Algorithm: Clear Rx interrupt source at wrapper level and DMA level.
 *
 * @param[in] osi_dma: DMA private data.
 * @param[in] chan: DMA rx channel number.
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	3) Mapping of physical IRQ line to DMA channel need to be maintained at
 *	OS Dependent layer and pass corresponding channel number.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_clear_vm_rx_intr(struct osi_dma_priv_data *osi_dma,
			     nveu32_t chan);

/**
 * @brief Start DMA
 *
 * @note
 * Algorithm:
 *  - Start the DMA for specific MAC
 *
 * @param[in] osi_dma: OSI DMA private data.
 * @param[in] chan: DMA Tx/Rx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_005
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
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_start_dma(struct osi_dma_priv_data *osi_dma, nveu32_t chan);

/**
 * @brief osi_stop_dma - Stop DMA
 *
 * @note
 * Algorithm:
 *  - Stop the DMA for specific MAC
 *
 * @param[in] osi_dma: OSI DMA private data.
 * @param[in] chan: DMA Tx/Rx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_006
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
 * - Run time: No
 * - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_stop_dma(struct osi_dma_priv_data *osi_dma, nveu32_t chan);

/**
 * @brief osi_get_refill_rx_desc_cnt - Rx descriptors count that needs to refill
 *
 * @note
 * Algorithm:
 *  - subtract current index with fill (need to cleanup)
 *    to get Rx descriptors count that needs to refill.
 *
 * @param[in] rx_ring: DMA channel Rx ring.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_007
 *
 * @note
 * Classification:
 * - Interrupt: Yes
 * - Signal handler: Yes
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval "Number of available free descriptors."
 */
nveu32_t osi_get_refill_rx_desc_cnt(struct osi_rx_ring *rx_ring);

/**
 * @brief osi_rx_dma_desc_init - DMA Rx descriptor init
 *
 * @note
 * Algorithm:
 *  - Initialize a Rx DMA descriptor.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in, out] rx_ring: HW ring corresponding to Rx DMA channel.
 * @param[in] chan: Rx DMA channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - rx_swcx->buf_phy_addr need to be filled with DMA mapped address
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_008
 *
 * @note
 * Classification:
 * - Interrupt: Yes
 * - Signal handler: Yes
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
nve32_t osi_rx_dma_desc_init(struct osi_dma_priv_data *osi_dma,
			     struct osi_rx_ring *rx_ring, nveu32_t chan);

/**
 * @brief Updates rx buffer length.
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - osi_dma->mtu need to be filled with current MTU size <= 9K
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_009
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
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_set_rx_buf_len(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_hw_transmit - Initialize Tx DMA descriptors for a channel
 *
 * @note
 * Algorithm:
 *  - Initialize Transmit descriptors with DMA mappable buffers,
 *    set OWN bit, Tx ring length and set starting address of Tx DMA channel
 *    Tx ring base address in Tx DMA registers.
 *
 * @param[in, out] osi_dma: OSI DMA private data.
 * @param[in] chan: DMA Tx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - DMA channel need to be started, see osi_start_dma
 *  - Need to set update tx_pkt_cx->flags accordingly as per the
 *    requirements
 *    OSI_PKT_CX_VLAN                 OSI_BIT(0)
 *    OSI_PKT_CX_CSUM                 OSI_BIT(1)
 *    OSI_PKT_CX_TSO                  OSI_BIT(2)
 *    OSI_PKT_CX_PTP                  OSI_BIT(3)
 *  - tx_pkt_cx->desc_cnt need to be populated which holds the number
 *    of swcx descriptors allocated for that packet
 *  - tx_swcx structure need to be filled for per packet with the
 *    buffer len, DMA mapped address of buffer for each descriptor
 *    consumed by the packet
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_010
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
nve32_t osi_hw_transmit(struct osi_dma_priv_data *osi_dma, nveu32_t chan);

/**
 * @brief osi_process_tx_completions - Process Tx complete on DMA channel ring.
 *
 * @note
 * Algorithm:
 *  - This function will be invoked by OSD layer to process Tx
 *    complete interrupt.
 *    - First checks whether descriptor owned by DMA or not.
 *    - Invokes OSD layer to release DMA address and Tx buffer which are
 *      updated as part of transmit routine.
 *
 * @param[in, out] osi_dma: OSI dma private data structure.
 * @param[in] chan: Channel number on which Tx complete need to be done.
 *            Max OSI_EQOS_MAX_NUM_CHANS.
 * @param[in] budget: Threshold for reading the packets at a time.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - DMA need to be started, see osi_start_dma
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_011
 *
 * @note
 * Classification:
 * - Interrupt: Yes
 * - Signal handler: Yes
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @returns Number of descriptors (buffers) processed.
 */
nve32_t osi_process_tx_completions(struct osi_dma_priv_data *osi_dma,
				   nveu32_t chan, nve32_t budget);

/**
 * @brief osi_process_rx_completions - Read data from rx channel descriptors
 *
 * @note
 * Algorithm:
 *  - This routine will be invoked by OSD layer to get the
 *    data from Rx descriptors and deliver the packet to the stack.
 *    - Checks descriptor owned by DMA or not.
 *    - If rx buffer is reserve buffer, reallocate receive buffer and
 *      read next descriptor.
 *    - Get the length from Rx descriptor
 *    - Invokes OSD layer to deliver the packet to network stack.
 *    - Re-allocate the receive buffers, populate Rx descriptor and
 *      handover to DMA.
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 * @param[in] chan: Rx DMA channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 * @param[in] budget: Threshold for reading the packets at a time.
 * @param[out] more_data_avail: Pointer to more data available flag. OSI fills
 *         this flag if more rx packets available to read(1) or not(0).
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - DMA need to be started, see osi_start_dma
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_012
 *
 * @note
 * Classification:
 * - Interrupt: Yes
 * - Signal handler: Yes
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @returns Number of descriptors (buffers) processed.
 */
nve32_t osi_process_rx_completions(struct osi_dma_priv_data *osi_dma,
				   nveu32_t chan, nve32_t budget,
				   nveu32_t *more_data_avail);

/**
 * @brief osi_hw_dma_init - Initialize DMA
 *
 * @note
 * Algorithm:
 *  - Takes care of initializing the tx, rx ring and descriptors
 *    based on the number of channels selected.
 *
 * @param[in, out] osi_dma: OSI DMA private data.
 *
 * @pre
 *  - Allocate memory for osi_dma
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - Number of dma channels osi_dma->num_dma_chans
 *  - channel list osi_dma->dma_chan
 *  - base address osi_dma->base
 *  - allocate tx ring osi_dma->tx_ring[chan] for each channel
 *    based on TX_DESC_CNT (256)
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
 *  - osi_dma->use_riwt  ==> OSI_DISABLE/OSI_ENABLE
 *  - osi_dma->rx_riwt  ===> Actual value read from DT
 *  - osi_dma->use_rx_frames  ==> OSI_DISABLE/OSI_ENABLE
 *  - osi_dma->rx_frames ===> Actual value read from DT
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_013
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
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_hw_dma_init(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_hw_dma_deinit - De initialize DMA
 *
 * @note
 * Algorithm:
 *  - Takes care of stopping the MAC
 *
 * @param[in] osi_dma: OSI DMA private data.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_014
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
 * - Run time: No
 * - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_hw_dma_deinit(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_init_dma_ops - Initialize DMA operations
 *
 * @param[in, out] osi_dma: OSI DMA private data.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_015
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
 * - Run time: No
 * - De-initialization: No
 *
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_init_dma_ops(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_dma_get_systime_from_mac - Get system time
 *
 * @note
 * Algorithm:
 *  - Gets the current system time
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[out] sec: Value read in Seconds
 * @param[out] nsec: Value read in Nano seconds
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_016
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
nve32_t osi_dma_get_systime_from_mac(struct osi_dma_priv_data *const osi_dma,
				     nveu32_t *sec, nveu32_t *nsec);

/**
 * @brief osi_is_mac_enabled - Checks if MAC is enabled.
 *
 * @note
 * Algorithm:
 *  - Reads MAC MCR register for Tx and Rx enabled bits.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETCL_017
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
 * - Run time: No
 * - De-initialization: No
 *
 * @retval OSI_ENABLE if MAC enabled.
 * @retval OSI_DISABLE otherwise.
 */
nveu32_t osi_is_mac_enabled(struct osi_dma_priv_data *const osi_dma);

/**
 * @brief osi_handle_dma_intr - Handles DMA interrupts.
 *
 * @note
 * Algorithm:
 *  - Enables/Disables DMA CH TX/RX/VM inetrrupts.
 *
 * @param[in] osi_dma: OSI DMA private data.
 * @param[in] chan: DMA Rx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 * @param[in] tx_rx: Indicates whether DMA channel is Tx or Rx.
 *                   OSI_DMA_CH_TX_INTR for Tx interrupt.
 *                   OSI_DMA_CH_RX_INTR for Rx interrupt.
 * @param[in] en_dis: Enable/Disable DMA channel interrupts.
 *                    OSI_DMA_INTR_DISABLE for disabling the interrupt.
 *                    OSI_DMA_INTR_ENABLE for enabling the interrupt.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *  - Mapping of physical IRQ line to DMA channel need to be maintained at
 *    OS Dependent layer and pass corresponding channel number.
 *
 * @note
 * Traceability Details: TBD
 *
 * @note
 * Classification:
 * - Interrupt: Yes
 * - Signal handler: Yes
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
nve32_t osi_handle_dma_intr(struct osi_dma_priv_data *osi_dma,
			    nveu32_t chan, nveu32_t tx_rx, nveu32_t en_dis);

/**
 * @brief osi_dma_ioctl - OSI DMA IOCTL
 *
 * @param[in] osi_dma: OSI DMA private data.
 *
 * @note
 * Traceability Details: TBD
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
nve32_t osi_dma_ioctl(struct osi_dma_priv_data *osi_dma);
#ifndef OSI_STRIPPED_LIB
/**
 * @brief - Read-validate HW registers for func safety.
 *
 * @note
 * Algorithm:
 *  - Reads pre-configured list of DMA configuration registers
 *    and compares with last written value for any modifications.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @pre
 *  - MAC has to be out of reset.
 *  - osi_hw_dma_init has to be called. Internally this would initialize
 *    the safety_config (see osi_dma_priv_data) based on MAC version and
 *    which specific registers needs to be validated periodically.
 *  - Invoke this call if (osi_dma_priv_data->safety_config != OSI_NULL)
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
nve32_t osi_validate_dma_regs(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_clear_tx_pkt_err_stats - Clear tx packet error stats.
 *
 * @note
 * Algorithm:
 *  - This function will be invoked by OSD layer to clear the
 *    tx stats mentioned in osi_dma->pkt_err_stats structure
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
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
nve32_t osi_clear_tx_pkt_err_stats(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_config_slot_function - Configure slot function
 *
 * @note
 * Algorithm:
 *  - Set or reset the slot function based on set input
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 * @param[in] set: Flag to set with OSI_ENABLE and reset with OSI_DISABLE
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
nve32_t osi_config_slot_function(struct osi_dma_priv_data *osi_dma,
				 nveu32_t set);
/**
 * @brief osi_clear_rx_pkt_err_stats - Clear rx packet error stats.
 *
 * @note
 * Algorithm:
 *  - This function will be invoked by OSD layer to clear the
 *    rx_crc_error mentioned in osi_dma->pkt_err_stats structure.
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 *
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
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
nve32_t osi_clear_rx_pkt_err_stats(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_txring_empty - Check if Txring is empty.
 *
 * @note
 * Algorithm:
 *  - This function will be invoked by OSD layer to check if the Tx ring
 *    is empty or still has outstanding packets to be processed for Tx done.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: Channel number whose ring is to be checked.
 *
 * @pre
 *  - MAC needs to be out of reset and proper clocks need to be configured.
 *  - DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 1 if ring is empty.
 * @retval 0 if ring has outstanding packets.
 */
nve32_t osi_txring_empty(struct osi_dma_priv_data *osi_dma, nveu32_t chan);
#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief osi_get_dma - Get pointer to osi_dma data structure.
 *
 * @note
 * Algorithm:
 *  - Returns OSI DMA data structure.
 *
 * @pre OSD layer should use this as first API to get osi_dma pointer and
 * use the same in remaning API invocation.
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
 * - Run time: No
 * - De-initialization: No
 *
 * @retval valid and unique osi_dma pointer on success
 * @retval NULL on failure.
 */
struct osi_dma_priv_data *osi_get_dma(void);
#endif /* INCLUDED_OSI_DMA_H */
