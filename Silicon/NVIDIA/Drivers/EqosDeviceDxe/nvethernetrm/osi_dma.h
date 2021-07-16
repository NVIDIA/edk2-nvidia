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

#ifndef OSI_DMA_H
#define OSI_DMA_H

#include "osi_common.h"
#include "osi_dma_txrx.h"
#include "mmc.h"

/**
 * @addtogroup EQOS-PKT Packet context fields
 *
 * @brief These flags are used to convey context information about a packet
 * between HW and SW. The context information includes
 * whether a VLAN tag is to be inserted for a packet,
 * whether a received packet is valid,
 * whether checksum offload is to be enabled for the packet upon transmit,
 * whether TCP segmentation offload is to be enabled for the packet,
 * whether the HW should timestamp transmit/arrival of a packet respectively
 * @{
 */
/** VLAN packet */
#define OSI_PKT_CX_VLAN			OSI_BIT(0)
/** Valid packet */
#define OSI_PKT_CX_VALID		OSI_BIT(10)
/** CSUM packet */
#define OSI_PKT_CX_CSUM			OSI_BIT(1)
/** TSO packet */
#define OSI_PKT_CX_TSO			OSI_BIT(2)
/** PTP packet */
#define OSI_PKT_CX_PTP			OSI_BIT(3)
/** @} */

/**
 * @addtogroup EQOS-TX Tx done packet context fields
 *
 * @brief These flags used to convey transmit done packet context information,
 * whether transmitted packet used a pagged buffer, whether transmitted packet
 * has an tx error, whether tranmitted packet has an TS
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
/** @} */

/**
 * @addtogroup EQOS-CHK Checksum offload results
 *
 * @brief Flag to indicate the result from checksum offload engine
 * to SW network stack in receive path
 * @{
 */
/* Checksum offload result flags */
#define OSI_CHECKSUM_NONE		0x0U
#define OSI_CHECKSUM_UNNECESSARY	0x1U
/** @} */

/**
 * @brief OSI packet error stats
 */
struct osi_pkt_err_stats {
	/** IP Header Error */
	unsigned long ip_header_error;
	/** Jabber time out Error */
	unsigned long jabber_timeout_error;
	/** Packet Flush Error */
	unsigned long pkt_flush_error;
	/** Payload Checksum Error */
	unsigned long payload_cs_error;
	/** Loss of Carrier Error */
	unsigned long loss_of_carrier_error;
	/** No Carrier Error */
	unsigned long no_carrier_error;
	/** Late Collision Error */
	unsigned long late_collision_error;
	/** Excessive Collision Error */
	unsigned long excessive_collision_error;
	/** Excessive Deferal Error */
	unsigned long excessive_deferal_error;
	/** Under Flow Error */
	unsigned long underflow_error;
	/** Rx CRC Error */
	unsigned long rx_crc_error;
};

/**
 * @brief Receive Descriptor
 */
struct osi_rx_desc {
	/** Receive Descriptor 0 */
	unsigned int rdes0;
	/** Receive Descriptor 1 */
	unsigned int rdes1;
	/** Receive Descriptor 2 */
	unsigned int rdes2;
	/** Receive Descriptor 3 */
	unsigned int rdes3;
};

/**
 * @brief Receive descriptor software context
 */
struct osi_rx_swcx {
	/** DMA buffer physical address */
	unsigned long buf_phy_addr;
	/** DMA buffer virtual address */
	void *buf_virt_addr;
	/** Length of buffer */
	unsigned int len;
};

/**
 * @brief - Received packet context. This is a single instance
 * and it is reused for all rx packets.
 */
struct osi_rx_pkt_cx {
	/** Bit map which holds the features that rx packets supports */
	unsigned int flags;
	/** Stores the Rx csum */
	unsigned int rxcsum;
	/** Stores the VLAN tag ID in received packet */
	unsigned int vlan_tag;
	/** Length of received packet */
	unsigned int pkt_len;
	/** TS in nsec for the received packet */
	unsigned long long ns;
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
	unsigned long rx_desc_phy_addr;
	/** Descriptor index current reception */
	unsigned int cur_rx_idx;
	/** Descriptor index for descriptor re-allocation */
	unsigned int refill_idx;
	/** Receive packet context */
	struct osi_rx_pkt_cx rx_pkt_cx;
};

/**
 *@brief Transmit descriptor software context
 */
struct osi_tx_swcx {
	/** Physical address of DMA mapped buffer */
	unsigned long buf_phy_addr;
	/** Virtual address of DMA buffer */
	void *buf_virt_addr;
	/** Length of buffer */
	unsigned int len;
	/** Flag to keep track of whether buffer pointed by buf_phy_addr
	 * is a paged buffer/linear buffer */
	unsigned int is_paged_buf;
};

/**
 * @brief Transmit descriptor
 */
struct osi_tx_desc {
	/** Transmit descriptor 0 */
	unsigned int tdes0;
	/** Transmit descriptor 1 */
	unsigned int tdes1;
	/** Transmit descriptor 2 */
	unsigned int tdes2;
	/** Transmit descriptor 3 */
	unsigned int tdes3;
};

/**
 * @brief Transmit packet context for a packet. This is a single instance
 * and it is reused for all tx packets.
 */
struct osi_tx_pkt_cx {
	/** Holds the features which a Tx packets supports */
	unsigned int flags;
	/** Stores the VLAN tag ID */
	unsigned int vtag_id;
	/** Descriptor count */
	unsigned int desc_cnt;
	/** Max. segment size for TSO/USO/GSO/LSO packet */
	unsigned int mss;
	/** Length of application payload */
	unsigned int payload_len;
	/** Length of transport layer tcp/udp header */
	unsigned int tcp_udp_hdrlen;
	/** Length of all headers (ethernet/ip/tcp/udp) */
	unsigned int total_hdrlen;
};

/**
 * @brief Transmit done packet context for a packet
 */
struct osi_txdone_pkt_cx {
	/** Indicates status flags for Tx complete (tx error occurred, or
	 * indicate whether desc had buf mapped from paged/linear memory etc) */
	unsigned int flags;
	/** TS captured for the tx packet and this is valid only when the PTP
	 * bit is set in fields */
	unsigned long long ns;
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
	unsigned long tx_desc_phy_addr;
	/** Descriptor index current transmission */
	unsigned int cur_tx_idx;
	/** Descriptor index for descriptor cleanup */
	unsigned int clean_idx;
	/** Transmit packet context */
	struct osi_tx_pkt_cx tx_pkt_cx;
	/** Transmit complete packet context information */
	struct osi_txdone_pkt_cx txdone_pkt_cx;
};

struct osi_dma_priv_data;

/**
 *@brief MAC DMA Channel operations
 */
struct osi_dma_chan_ops {
	/** Called to set Transmit Ring length */
	void (*set_tx_ring_len)(void *addr, unsigned int chan,
				unsigned int len);
	/** Called to set Transmit Ring Base address */
	void (*set_tx_ring_start_addr)(void *addr, unsigned int chan,
				       unsigned long base_addr);
	/** Called to update Tx Ring tail pointer */
	void (*update_tx_tailptr)(void *addr, unsigned int chan,
				  unsigned long tailptr);
	/** Called to set Receive channel ring length */
	void (*set_rx_ring_len)(void *addr, unsigned int chan,
				unsigned int len);
	/** Called to set receive channel ring base address */
	void (*set_rx_ring_start_addr)(void *addr, unsigned int chan,
				       unsigned long base_addr);
	/** Called to update Rx ring tail pointer */
	void (*update_rx_tailptr)(void *addr, unsigned int chan,
				  unsigned long tailptr);
	/** Called to clear Tx interrupt source */
	void (*clear_tx_intr)(void *addr, unsigned int chan);
	/** Called to clear Rx interrupt source */
	void (*clear_rx_intr)(void *addr, unsigned int chan);
	/** Called to disable DMA Tx channel interrupts at wrapper level */
	void (*disable_chan_tx_intr)(void *addr, unsigned int chan);
	/** Called to enable DMA Tx channel interrupts at wrapper level */
	void (*enable_chan_tx_intr)(void *addr, unsigned int chan);
	/** Called to disable DMA Rx channel interrupts at wrapper level */
	void (*disable_chan_rx_intr)(void *addr, unsigned int chan);
	/** Called to enable DMA Rx channel interrupts at wrapper level */
	void (*enable_chan_rx_intr)(void *addr, unsigned int chan);
	/** Called to start the Tx/Rx DMA */
	void (*start_dma)(void *addr, unsigned int chan);
	/** Called to stop the Tx/Rx DMA */
	void (*stop_dma)(void *addr, unsigned int chan);
	/** Called to initialize the DMA channel */
	void (*init_dma_channel) (struct osi_dma_priv_data *osi_dma);
	/** Called to set Rx buffer length */
	void (*set_rx_buf_len)(struct osi_dma_priv_data *osi_dma);
	/** Called periodically to read and validate safety critical
	 * registers against last written value */
	int (*validate_regs)(struct osi_dma_priv_data *osi_dma);
};

/**
 * @brief The OSI DMA private data structure.
 */
struct osi_dma_priv_data {
	/** Array of pointers to DMA Tx channel Ring */
	struct osi_tx_ring *tx_ring[OSI_EQOS_MAX_NUM_CHANS];
	/** Array of pointers to DMA Rx channel Ring */
	struct osi_rx_ring *rx_ring[OSI_EQOS_MAX_NUM_CHANS];
	/** Memory mapped base address of MAC IP */
	void *base;
	/** Pointer to OSD private data structure */
	void *osd;
	/** Address of HW operations structure */
	struct osi_dma_chan_ops *ops;
	/** MAC HW type (EQOS) */
	unsigned int mac;
	/** Number of channels enabled in MAC */
	unsigned int num_dma_chans;
	/** Array of supported DMA channels */
	unsigned int dma_chans[OSI_EQOS_MAX_NUM_CHANS];
	/** DMA Rx channel buffer length at HW level */
	unsigned int rx_buf_len;
	/** MTU size */
	unsigned int mtu;
	/** Packet error stats */
	struct osi_pkt_err_stats pkt_err_stats;
	/** Extra DMA stats */
	struct osi_xtra_dma_stat_counters dstats;
	/** Receive Interrupt Watchdog Timer Count Units */
	unsigned int rx_riwt;
	/** Flag which decides riwt is enabled(1) or disabled(0) */
	unsigned int use_riwt;
	/** Functional safety config to do periodic read-verify of
	 * certain safety critical dma registers */
	void *safety_config;
	/** UEFI: Data buffer pointer where Rx packet should be copied */
	void *data;
	/** UEFI: Data buffer length */
	long buffsize;
	/** UEFI: Tx Data buffer pointer */
	void *tx_buff;
};

/**
 * @brief - Read-validate HW registers for func safety.
 *
 * Algorithm: Reads pre-configured list of DMA configuration registers
 *	and compares with last written value for any modifications.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note
 *	1) MAC has to be out of reset.
 *	2) osi_hw_dma_init has to be called. Internally this would initialize
 *	the safety_config (see osi_dma_priv_data) based on MAC version and
 *	which specific registers needs to be validated periodically.
 *	3) Invoke this call iff (osi_dma_priv_data->safety_config != OSI_NULL)
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_validate_dma_regs(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_disable_chan_tx_intr - Disables DMA Tx channel interrupts.
 *
 * Algorithm: Disables Tx interrupts at wrapper level.
 *
 * @param[in] osi_dma: DMA private data.
 * @param[in] chan: DMA Tx channel number.
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
int osi_disable_chan_tx_intr(struct osi_dma_priv_data *osi_dma,
			     unsigned int chan);

/**
 * @brief osi_enable_chan_tx_intr - Enable DMA Tx channel interrupts.
 *
 * Algorithm: Enables Tx interrupts at wrapper level.
 *
 * @param[in] osi_dma: DMA private data.
 * @param[in] chan: DMA Tx channel number.
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
int osi_enable_chan_tx_intr(struct osi_dma_priv_data *osi_dma,
			    unsigned int chan);

/**
 * @brief osi_disable_chan_rx_intr - Disable DMA Rx channel interrupts.
 *
 * Algorithm: Disables Rx interrupts at wrapper level.
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
int osi_disable_chan_rx_intr(struct osi_dma_priv_data *osi_dma,
			     unsigned int chan);

/**
 * @brief osi_enable_chan_rx_intr - Enable DMA Rx channel interrupts.
 *
 * Algorithm: Enables Rx interrupts at wrapper level.
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
int osi_enable_chan_rx_intr(struct osi_dma_priv_data *osi_dma,
			    unsigned int chan);

/**
 * @brief osi_clear_tx_intr - Handles Tx interrupt source.
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
int osi_clear_tx_intr(struct osi_dma_priv_data *osi_dma,
		      unsigned int chan);

/**
 * @brief osi_clear_rx_intr - Handles Rx interrupt source.
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
int osi_clear_rx_intr(struct osi_dma_priv_data *osi_dma,
		      unsigned int chan);

/**
 * @brief Start DMA
 *
 * Algorithm: Start the DMA for specific MAC
 *
 * @param[in] osi_dma: DMA private data.
 * @param[in] chan: DMA Tx/Rx channel number
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_start_dma(struct osi_dma_priv_data *osi_dma,
		  unsigned int chan);

/**
 * @brief osi_stop_dma - Stop DMA
 *
 * Algorithm: Stop the DMA for specific MAC
 *
 * @param[in] osi_dma: DMA private data.
 * @param[in] chan: DMA Tx/Rx channel number
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_stop_dma(struct osi_dma_priv_data *osi_dma,
		 unsigned int chan);

/**
 * @brief osi_get_refill_rx_desc_cnt - Rx descriptors count that needs to refill
 *
 * Algorithm: subtract current index with fill (need to cleanup)
 *	  to get Rx descriptors count that needs to refill.
 *
 * @param[in] rx_ring: DMA channel Rx ring.
 *
 * @note None.
 *
 * @retval "Number of available free descriptors."
 */
unsigned int osi_get_refill_rx_desc_cnt(struct osi_rx_ring *rx_ring);

/**
 * @brief osi_rx_dma_desc_init - DMA Rx descriptor init
 *
 * Algorithm: Initialise a Rx DMA descriptor.
 *
 * @param[in] rx_swcx: OSI DMA Rx ring software context
 * @param[in] rx_desc: OSI DMA Rx ring descriptor
 * @param[in] use_riwt: to enable Rx WDT and disable IOC
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) rx_swcx->buf_phy_addr need to be filled with DMA mapped address
 *	3) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_rx_dma_desc_init(struct osi_rx_swcx *rx_swcx,
			 struct osi_rx_desc *rx_desc,
			 unsigned int use_riwt);

/**
 * @brief osi_update_rx_tailptr - Updates DMA Rx ring tail pointer
 *
 * @param[in] osi_dma: OSI DMA private data struture.
 * @param[in] rx_ring: Pointer to DMA Rx ring.
 * @param[in] chan: DMA channel number.
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_update_rx_tailptr(struct osi_dma_priv_data *osi_dma,
			  struct osi_rx_ring *rx_ring,
			  unsigned int chan);

/**
 * @brief Updates rx buffer length.
 *
 * @param[in] osi_dma: OSI DMA private data struture.
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	3) osi_dma->mtu need to be filled with current MTU size <= 9K
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_set_rx_buf_len(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_hw_transmit - Initialize Tx DMA descriptors for a channel
 *
 * Algorithm: Initialize Transmit descriptors with DMA mappable buffers,
 *	  set OWN bit, Tx ring length and set starting address of Tx DMA channel
 *	  Tx ring base address in Tx DMA registers.
 *
 * @param[in] osi: DMA private data.
 * @param[in] chan: DMA Tx channel number.
 *
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	3) DMA channel need to be started, see osi_start_dma
 *	4) Need to set update tx_pkt_cx->flags accordingly as per the
 *	requirements
 *	OSI_PKT_CX_VLAN                 OSI_BIT(0)
 *	OSI_PKT_CX_CSUM                 OSI_BIT(1)
 *	OSI_PKT_CX_TSO                  OSI_BIT(2)
 *	OSI_PKT_CX_PTP                  OSI_BIT(3)
 *	5) tx_pkt_cx->desc_cnt need to be populated which holds the number
 *	of swcx descriptors allocated for that packet
 *	6) tx_swcx structure need to be filled for per packet with the
 *	buffer len, DMA mapped address of buffer for each descriptor
 *	consumed by the packet
 */
void osi_hw_transmit(struct osi_dma_priv_data *osi, unsigned int chan);

/**
 * @brief osi_process_tx_completions - Process Tx complete on DMA channel ring.
 *
 * Algorithm: This function will be invoked by OSD layer to process Tx
 *	  complete interrupt.
 *	  1) First checks whether descriptor owned by DMA or not.
 *	  2) Invokes OSD layer to release DMA address and Tx buffer which are
 *	  updated as part of transmit routine.
 *
 * @param[in] osi: OSI private data structure.
 * @param[in] chan: Channel number on which Tx complete need to be done.
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	3) DMA need to be started, see osi_start_dma
 *
 * @returns Number of decriptors (buffers) proccessed.
 */
int osi_process_tx_completions(struct osi_dma_priv_data *osi,
			       unsigned int chan, unsigned int budget);

/**
 * @brief osi_process_rx_completions - Read data from rx channel descriptors
 *
 * Algorithm: This routine will be invoked by OSD layer to get the
 *	  data from Rx descriptors and deliver the packet to the stack.
 *	  1) Checks descriptor owned by DMA or not.
 *	  2) Get the length from Rx descriptor
 *	  3) Invokes OSD layer to deliver the packet to network stack.
 *	  4) Re-allocate the receive buffers, populate Rx descriptor and
 *	  handover to DMA.
 *
 * @param[in] osi: OSI private data structure.
 * @param[in] chan: Rx DMA channel number
 * @param[in] budget: Threshould for reading the packets at a time.
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *	3) DMA need to be started, see osi_start_dma
 *
 * @returns Number of decriptors (buffers) proccessed.
 */
int osi_process_rx_completions(struct osi_dma_priv_data *osi,
			       unsigned int chan, int budget);

/**
 * @brief osi_hw_dma_init - Initialize DMA
 *
 * Algorithm: Takes care of initializing the tx, rx ring and descriptors
 *	  based on the number of channels selected.
 *
 * @param[in] osi_dma: DMA private data.
 *
 *
 * @note
 *	1) Allocate memory for osi_dma
 *	2) MAC needs to be out of reset and proper clocks need to be configured.
 *	3) Numer of dma channels osi_dma->num_dma_chans
 *	4) channel list osi_dma->dma_chan
 *	5) base address osi_dma->base
 *	6) allocate tx ring osi_dma->tx_ring[chan] for each channel
 *	based on TX_DESC_CNT (256)
 *	7) allocate tx descriptors osi_dma->tx_ring[chan]->tx_desc for all
 *	channels and dma map it.
 *	8) allocate tx sw descriptors osi_dma->tx_ring[chan]->tx_swcx for all
 *	channels
 *	9) allocate rx ring osi_dma->rx_ring[chan] for each channel
 *	based on RX_DESC_CNT (256)
 *	10) allocate rx descriptors osi_dma->rx_ring[chan]->rx_desc for all
 *	channels and dma map it.
 *	11) allocate rx sw descriptors osi_dma->rx_ring[chan]->rx_swcx for all
 *	channels
 *	12) osi_dma->use_riwt  ==> OSI_DISABLE/OSI_ENABLE
 *	13) osi_dma->rx_riwt  ===> Actual value read from DT
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_hw_dma_init(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_hw_dma_deinit - De initialize DMA
 *
 * Algorithm: Takes care of stopping the MAC
 *
 * @param[in] osi_dma: DMA private data.
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_hw_dma_deinit(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_init_dma_ops - Initialize DMA operations
 *
 * @param[in] osi_dma: DMA private data.
 *
 * @note None
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_init_dma_ops(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_clear_tx_pkt_err_stats - Clear tx packet error stats.
 *
 * Algorithm: This function will be invoked by OSD layer to clear the
 *	  tx stats mentioned in osi_dma->pkt_err_stats structure
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_clear_tx_pkt_err_stats(struct osi_dma_priv_data *osi_dma);

/**
 * @brief osi_clear_rx_pkt_err_stats - Clear rx packet error stats.
 *
 * Algorithm: This function will be invoked by OSD layer to clear the
 *	  rx_crc_error mentioned in osi_dma->pkt_err_stats structure.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 *
 * @note
 *	1) MAC needs to be out of reset and proper clocks need to be configured.
 *	2) DMA HW init need to be completed successfully, see osi_hw_dma_init
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_clear_rx_pkt_err_stats(struct osi_dma_priv_data *osi_dma);

#endif /* OSI_DMA_H */
