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

#ifndef OSI_CORE_H
#define OSI_CORE_H

#include "osi_common.h"
#include "mmc.h"

struct osi_core_priv_data;

/**
 * @brief OSI core structure for filters
 */
struct osi_filter {
	/** promiscuous mode enable(1) or disable(0) */
	unsigned int pr_mode;
	/** hash unicast enable(1) or disable(0) */
	unsigned int huc_mode;
	/** hash multicast enable(1) or disable(0) */
	unsigned int hmc_mode;
	/** pass all multicast enable(1) or disable(0) */
	unsigned int pm_mode;
	/** 0x0 (DISABLE): Hash or Perfect Filter is disabled
	 *
	 * 0x1 (ENABLE): Hash or Perfect Filter is enabled */
	unsigned int hpf_mode;
};

/**
 * @brief L3/L4 filter function dependent parameter
 */
struct osi_l3_l4_filter {
	/** Indicates the index of the filter to be modified.
	 * Filter index must be between 0 - 7 */
	unsigned int filter_no;
	/** filter enable(1) or disable(0) */
	unsigned int filter_enb_dis;
	/** source(0) or destination(1) */
	unsigned int src_dst_addr_match;
	/** perfect(0) or inverse(1) */
	unsigned int perfect_inverse_match;
	/** ipv4 address */
	unsigned char ip4_addr[4];
	/** ipv6 address */
	unsigned short ip6_addr[8];
	/** Port number */
	unsigned short port_no;
};

/**
 * @brief Vlan filter Function dependent parameter
 */
struct osi_vlan_filter {
	/** vlan filter enable(1) or disable(0) */
	unsigned int filter_enb_dis;
	/** perfect(0) or hash(1) */
	unsigned int perfect_hash;
	/** perfect(0) or inverse(1) */
	unsigned int perfect_inverse_match;
};

/**
 * @brief L2 filter function dependent parameter
 */
struct osi_l2_da_filter {
	/** perfect(0) or hash(1) */
	unsigned int perfect_hash;
	/** perfect(0) or inverse(1) */
	unsigned int perfect_inverse_match;
};

/**
 * @brief OSI Core avb data structure per queue.
 */
struct  osi_core_avb_algorithm {
	/** TX Queue/TC index */
	unsigned int qindex;
	/** CBS Algorithm enable(1) or disable(0) */
	unsigned int algo;
	/** When this bit is set, the accumulated credit parameter in the
	 * credit-based shaper algorithm logic is not reset to zero when
	 * there is positive credit and no packet to transmit in Channel.
	 * 
	 * Expected values are enable(1) or disable(0) */
	unsigned int credit_control;
	/** idleSlopeCredit value required for CBS */
	  unsigned int idle_slope;
	/** sendSlopeCredit value required for CBS */
	unsigned int send_slope;
	/** hiCredit value required for CBS */
	unsigned int hi_credit;
	/** lowCredit value required for CBS */
	unsigned int low_credit;
	/** Transmit queue operating mode
	 *
	 * 00: disable
	 * 
	 * 01: avb 
	 * 
	 * 10: enable */
	unsigned int oper_mode;
};

/**
 * @brief Initialize MAC & MTL core operations.
 */
struct osi_core_ops {
	/** Called to poll for software reset bit */
	int (*poll_for_swr)(void *ioaddr);
	/** Called to initialize MAC and MTL registers */
	int (*core_init)(struct osi_core_priv_data *osi_core,
			 unsigned int tx_fifo_size,
			 unsigned int rx_fifo_size);
	/** Called to deinitialize MAC and MTL registers */
	void (*core_deinit)(struct osi_core_priv_data *osi_core);
	/** Called periodically to read and validate safety critical
	 * registers against last written value */
	int (*validate_regs)(struct osi_core_priv_data *osi_core);
	/**  Called to start MAC Tx and Rx engine */
	void (*start_mac)(void *addr);
	/** Called to stop MAC Tx and Rx engine */
	void (*stop_mac)(void *addr);
	/** Called to handle common interrupt */
	void (*handle_common_intr)(struct osi_core_priv_data *osi_core);
	/** Called to set the mode at MAC (full/duplex) */
	void (*set_mode)(void *ioaddr, int mode);
	/** Called to set the speed (10/100/1000) at MAC */
	void (*set_speed)(void *ioaddr, int speed);
	/** Called to do pad caliberation */
	int (*pad_calibrate)(void *ioaddr);
	/** Called to set MDC clock rate for MDIO operation */
	void (*set_mdc_clk_rate)(struct osi_core_priv_data *osi_core,
				 unsigned long csr_clk_rate);
	/** Called to flush MTL Tx queue */
	int (*flush_mtl_tx_queue)(void *ioaddr, unsigned int qinx);
	/** Called to configure MAC in loopback mode */
	int (*config_mac_loopback)(void *addr, unsigned int lb_mode);
	/** Called to set av parameter */
	int (*set_avb_algorithm)(struct osi_core_priv_data *osi_core,
				 struct osi_core_avb_algorithm *avb);
	/** Called to get av parameter */
	int (*get_avb_algorithm)(struct osi_core_priv_data *osi_core,
				 struct osi_core_avb_algorithm *avb);
	/** Called to configure MTL RxQ to forward the err pkt */
	int (*config_fw_err_pkts)(void *addr, unsigned int qinx,
				  unsigned int fw_err);
	/** Called to configure the MTL to forward/drop tx status */
	int (*config_tx_status)(void *addr, unsigned int tx_status);
	/** Called to configure the MAC rx crc */
	int (*config_rx_crc_check)(void *addr, unsigned int crc_chk);
	/** Called to configure the MAC flow control */
	int (*config_flow_control)(void *addr, unsigned int flw_ctrl);
	/** Called to enable/disable HW ARP offload feature */
	int (*config_arp_offload)(unsigned int mac_ver, void *addr,
				  unsigned int enable, unsigned char *ip_addr);
	/** Called to configure Rx Checksum offload engine */
	int (*config_rxcsum_offload)(void *addr, unsigned int enabled);
	/** Called to config mac packet filter */
	void (*config_mac_pkt_filter_reg)(struct osi_core_priv_data *osi_core,
					  struct osi_filter filter);
	/** Called to update MAC address 1-127 */
	int (*update_mac_addr_low_high_reg)(
					    struct osi_core_priv_data *osi_core,
					    unsigned int index,
					    unsigned char value[],
					    unsigned int dma_routing_enable,
					    unsigned int dma_chan,
					    unsigned int addr_mask,
					    unsigned int src_dest);
	/** Called to configure l3/L4 filter */
	int (*config_l3_l4_filter_enable)(void *base, unsigned int enable);
	/** Called to configure L2 DA filter */
	int (*config_l2_da_perfect_inverse_match)(void *base, unsigned int
						  perfect_inverse_match);
	/** Called to configure L3 filter */
	int (*config_l3_filters)(struct osi_core_priv_data *osi_core,
				 unsigned int filter_no, unsigned int enb_dis,
				 unsigned int ipv4_ipv6_match,
				 unsigned int src_dst_addr_match,
				 unsigned int perfect_inverse_match,
				 unsigned int dma_routing_enable,
				 unsigned int dma_chan);
	/** Called to update ip4 src or desc address */
	int (*update_ip4_addr)(struct osi_core_priv_data *osi_core,
			       unsigned int filter_no, unsigned char addr[],
			       unsigned int src_dst_addr_match);
	/** Called to update ip6 address */
	int (*update_ip6_addr)(struct osi_core_priv_data *osi_core,
			       unsigned int filter_no, unsigned short addr[]);
	/** Called to configure L4 filter */
	int (*config_l4_filters)(struct osi_core_priv_data *osi_core,
				 unsigned int filter_no, unsigned int enb_dis,
				 unsigned int tcp_udp_match,
				 unsigned int src_dst_port_match,
				 unsigned int perfect_inverse_match,
				 unsigned int dma_routing_enable,
				 unsigned int dma_chan);
	/** Called to update L4 Port for filter packet */
	int (*update_l4_port_no)(struct osi_core_priv_data *osi_core,
				 unsigned int filter_no, unsigned short port_no,
				 unsigned int src_dst_port_match);

	/** Called to configure VLAN filtering */
	  int (*config_vlan_filtering)(struct osi_core_priv_data *osi_core,
				       unsigned int filter_enb_dis,
				       unsigned int perfect_hash_filtering,
				       unsigned int perfect_inverse_match);
	/** called to update VLAN id */
	int (*update_vlan_id)(void *base, unsigned int vid);
	/** Called to set current system time to MAC */
	int (*set_systime_to_mac)(void *addr, unsigned int sec,
				  unsigned int nsec);
	/** Called to set the addend value to adjust the time */
	int (*config_addend)(void *addr, unsigned int addend);
	/** Called to adjust the system time */
	int (*adjust_systime)(void *addr, unsigned int sec, unsigned int nsec,
			      unsigned int neg_adj,
			      unsigned int one_nsec_accuracy);
	/** Called to get the current time from MAC */
	unsigned long long (*get_systime_from_mac)(void *addr);
	/** Called to configure the TimeStampControl register */
	void (*config_tscr)(void *addr, unsigned int ptp_filter);
	/** Called to configure the sub second increment register */
	void (*config_ssir)(void *addr, unsigned int ptp_clock);
	/** Called to update MMC counter from HW register */
	void (*read_mmc)(struct osi_core_priv_data *osi_core);
	/** Called to reset MMC HW counter structure */
	void (*reset_mmc)(struct osi_core_priv_data *osi_core);
};

/**
 * @brief PTP configuration structure
 */
struct osi_ptp_config {
	/** PTP filter parameters bit fields.
	 * 
	 * Enable Time stamp,Fine Timestamp,1 nanosecond accuracy
	 * are enabled by default.
	 * 
	 * Need to set below bit fields accordingly as per the requirements.
	 * 
	 * Enable Timestamp for All Packets			OSI_BIT(8)
	 * 
	 * Enable PTP Packet Processing for Version 2 Format	OSI_BIT(10)
	 * 
	 * Enable Processing of PTP over Ethernet Packets	OSI_BIT(11)
	 * 
	 * Enable Processing of PTP Packets Sent over IPv6-UDP	OSI_BIT(12)
	 * 
	 * Enable Processing of PTP Packets Sent over IPv4-UDP	OSI_BIT(13)
	 * 
	 * Enable Timestamp Snapshot for Event Messages		OSI_BIT(14)
	 * 
	 * Enable Snapshot for Messages Relevant to Master	OSI_BIT(15)
	 * 
	 * Select PTP packets for Taking Snapshots		OSI_BIT(16)
	 * 
	 * Select PTP packets for Taking Snapshots		OSI_BIT(17)
	 * 
	 * Select PTP packets for Taking Snapshots (OSI_BIT(16) + OSI_BIT(17))
	 * 
	 * AV 802.1AS Mode Enable				OSI_BIT(28)
	 * 
	 * if ptp_fitler is set to Zero then Time stamping is disabled */
	unsigned int ptp_filter;
	/** seconds to be updated to MAC */
	unsigned int sec;
	/** nano seconds to be updated to MAC */
	unsigned int nsec;
	/** PTP reference clock read from DT */
	unsigned int ptp_ref_clk_rate;
	/** Use one nsec accuracy (need to set 1) */
	unsigned int one_nsec_accuracy;
	/** PTP system clock which is 62500000Hz */
	unsigned int ptp_clock;
};

/**
 * @brief The OSI Core (MAC & MTL) private data structure.
 */
struct osi_core_priv_data {
	/** Memory mapped base address of MAC IP */
	void *base;
	/** Pointer to OSD private data structure */
	void *osd;
	/** Address of HW Core operations structure */
	struct osi_core_ops *ops;
	/** Number of MTL queues enabled in MAC */
	unsigned int num_mtl_queues;
	/** Array of MTL queues */
	unsigned int mtl_queues[OSI_EQOS_MAX_NUM_CHANS];
	/** List of MTL Rx queue mode that need to be enabled */
	unsigned int rxq_ctrl[OSI_EQOS_MAX_NUM_CHANS];
	/** Rx MTl Queue mapping based on User Priority field */
	unsigned int rxq_prio[OSI_EQOS_MAX_NUM_CHANS];
	/** MAC HW type EQOS based on DT compatible */
	unsigned int mac;
	/** MAC version */
	unsigned int mac_ver;
	/** MDC clock rate */
	unsigned int mdc_cr;
	/** MTU size */
	unsigned int mtu;
	/** Ethernet MAC address */
	unsigned char mac_addr[OSI_ETH_ALEN];
	/** DT entry to enable(0) or disable(1) pause frame support */
	unsigned int pause_frames;
	/** Current flow control settings */
	unsigned int flow_ctrl;
	/** PTP configuration settings */
	struct osi_ptp_config ptp_config;
	/** Default addend value */
	unsigned int default_addend;
	/** mmc counter structure */
	struct osi_mmc_counters mmc;
	/** xtra sw error counters */
	struct osi_xtra_stat_counters xstats;
	/** DMA channel selection enable (1) */
	unsigned int dcs_en;
	/** Functional safety config to do periodic read-verify of
	 * certain safety critical registers */
	void *safety_config;
	/** VLAN tag stripping enable(1) or disable(0) */
	unsigned int strip_vlan_tag;
};

/**
 * @brief osi_poll_for_swr - Poll Software reset bit in MAC HW
 *
 * Algorithm: Invokes EQOS routine to check for SWR (software reset)
 * bit in DMA Basic mode register to make sure IP reset was successful.
 *
 * @param[in] osi_core: OSI Core private data structure.
 *
 * @note MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */

int osi_poll_for_swr(struct osi_core_priv_data *osi_core);

/**
 * @brief osi_set_mdc_clk_rate - Derive MDC clock based on provided AXI_CBB clk.
 *
 * Algorithm: MDC clock rate will be populated in OSI core private data
 * structure based on AXI_CBB clock rate.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] csr_clk_rate: CSR (AXI CBB) clock rate.
 *
 * @note OSD layer needs get the AXI CBB clock rate with OSD clock API
 *	(ex - clk_get_rate())
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_set_mdc_clk_rate(struct osi_core_priv_data *osi_core,
			 unsigned long csr_clk_rate);

/**
 * @brief osi_hw_core_init - EQOS MAC, MTL and common DMA initialization.
 * 
 * Algorithm: Invokes EQOS MAC, MTL and common DMA register init code.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] tx_fifo_size: OSI core private data structure.
 * @param[in] rx_fifo_size: OSI core private data structure.
 *
 * @note
 * 1) MAC should be out of reset. See osi_poll_for_swr() for details.
 * 2) osi_core->base needs to be filled based on ioremap.
 * 3) osi_core->num_mtl_queues needs to be filled.
 * 4)osi_core->mtl_queues[qinx] need to be filled.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_hw_core_init(struct osi_core_priv_data *osi_core,
		     unsigned int tx_fifo_size,
		     unsigned int rx_fifo_size);

/**
 * @brief osi_hw_core_deinit - EQOS MAC deinitialization.
 * 
 * Algorithm: Stops MAC transmisson and reception.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_hw_core_deinit(struct osi_core_priv_data *osi_core);

/**
 * @brief osi_validate_core_regs - Read-validate HW registers for func safety.
 *
 * Algorithm: Reads pre-configured list of MAC/MTL configuration registers
 *	and compares with last written value for any modifications.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note
 *	1) MAC has to be out of reset.
 *	2) osi_hw_core_init has to be called. Internally this would initialize
 *	the safety_config (see osi_core_priv_data) based on MAC version and
 *	which specific registers needs to be validated periodically.
 *	3) Invoke this call iff (osi_core_priv_data->safety_config != OSI_NULL)
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_validate_core_regs(struct osi_core_priv_data *osi_core);

/**
 * @brief osi_start_mac - Start MAC Tx/Rx engine
 * 
 * Algorithm: Enable MAC Tx and Rx engine.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC init should be complete. See osi_hw_core_init() and
 * 	 osi_hw_dma_init()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_start_mac(struct osi_core_priv_data *osi_core);

/**
 * @brief osi_stop_mac - Stop MAC Tx/Rx engine
 * 
 * Algorithm: Stop MAC Tx and Rx engine
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC DMA deinit should be complete. See osi_hw_dma_deinit()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_stop_mac(struct osi_core_priv_data *osi_core);

/**
 * @brief osi_common_isr - Common ISR.
 * 
 * Algorithm: Takes care of handling the common interrupts accordingly as per
 * the MAC IP
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_common_isr(struct osi_core_priv_data *osi_core);

/**
 * @brief osi_set_mode - Set FD/HD mode.
 *
 * Algorithm: Takes care of  setting HD or FD mode accordingly as per the MAC IP
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] mode: Operating mode.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_set_mode(struct osi_core_priv_data *osi_core, int mode);

/**
 * @brief osi_set_speed - Set operating speed.
 * 
 * Algorithm: Takes care of  setting the operating speed accordingly as per
 * the MAC IP.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] speed: Operating speed.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_set_speed(struct osi_core_priv_data *osi_core, int speed);

/**
 * @brief osi_pad_calibrate - PAD calibration
 *
 * Algorithm: Takes care of  doing the pad calibration
 * accordingly as per the MAC IP.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note
 *	1) MAC should out of reset and clocks enabled.
 *	2) RGMII and MDIO interface needs to be IDLE before performing PAD
 *	calibration.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_pad_calibrate(struct osi_core_priv_data *osi_core);

/**
 * @brief osi_flush_mtl_tx_queue - Flushing a MTL Tx Queue.
 *
 * Algorithm: Invokes EQOS flush Tx queue routine.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] qinx: MTL queue index.
 *
 * @note
 *	1) MAC should out of reset and clocks enabled.
 *	2) hw core initialized. see osi_hw_core_init().
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_flush_mtl_tx_queue(struct osi_core_priv_data *osi_core,
			   unsigned int qinx);

/**
 * @brief osi_config_mac_loopback - Configure MAC loopback
 *
 * Algorithm: Configure the MAC to support the loopback.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] lb_mode: Enable or disable MAC loopback
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_config_mac_loopback(struct osi_core_priv_data *osi_core,
			    unsigned int lb_mode);

/**
 * @brief osi_set_avb - Set CBS algo and parameters
 *
 * Algorithm: Set AVB algo and  populated parameter from osi_core_avb
 * structure for TC/TQ
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] avb: osi core avb data structure.
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_set_avb(struct osi_core_priv_data *osi_core,
		struct osi_core_avb_algorithm *avb);

/**
 * @brief osi_get_avb - Get CBS algo and parameters
 *
 * Algorithm: get AVB algo and  populated parameter from osi_core_avb
 * structure for TC/TQ
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[out] avb: osi core avb data structure.
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_get_avb(struct osi_core_priv_data *osi_core,
		struct osi_core_avb_algorithm *avb);

/**
 * @brief osi_configure_txstatus - Configure Tx packet status reporting
 *
 * Algorithm: Configure MAC to enable/disable Tx status error
 * reporting.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] tx_status: Enable or disable tx packet status reporting
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_configure_txstatus(struct osi_core_priv_data *osi_core,
			   unsigned int tx_status);

/**
 * @brief osi_config_fw_err_pkts - Configure forwarding of error packets
 *
 * Algorithm: Configure MAC to enable/disable forwarding of error packets.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] qinx: Q index
 * @param[in] fw_err: Enable or disable forwarding of error packets
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_config_fw_err_pkts(struct osi_core_priv_data *osi_core,
			   unsigned int qinx, unsigned int fw_err);

/**
 * @brief osi_config_rx_crc_check - Configure CRC Checking for Received Packets
 *
 * Algorithm: When this bit is set, the MAC receiver does not check the CRC
 * field in the received packets. When this bit is reset, the MAC receiver
 * always checks the CRC field in the received packets.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] crc_chk: Enable or disable checking of CRC field in received pkts
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_config_rx_crc_check(struct osi_core_priv_data *osi_core,
			    unsigned int crc_chk);

/**
 * @brief osi_configure_flow_ctrl - Configure flow control settings
 *
 * Algorithm: This will enable or disable the flow control.
 * flw_ctrl BIT0 is for tx flow ctrl enable/disable
 * flw_ctrl BIT1 is for rx flow ctrl enable/disable
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] flw_ctrl: Enable or disable flow control settings
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_configure_flow_control(struct osi_core_priv_data *osi_core,
			       unsigned int flw_ctrl);

/**
 * @brief osi_config_arp_offload - Configure ARP offload in MAC.
 *
 * Algorithm: Invokes EQOS config ARP offload routine.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] flags: Enable/disable flag.
 * @param[in] ip_addr: Char array representation of IP address
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) Valid 4 byte IP address as argument ip_addr
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_config_arp_offload(struct osi_core_priv_data *osi_core,
			   unsigned int flags,
			   unsigned char *ip_addr);

/**
 * @brief osi_config_rxcsum_offload - Configure RX checksum offload in MAC.
 *
 * Algorithm: Invokes EQOS config RX checksum offload routine.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enable: Enable/disable flag.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_config_rxcsum_offload(struct osi_core_priv_data *osi_core,
			      unsigned int enable);

/**
 * @brief osi_config_mac_pkt_filter_reg - configure mac filter register.
 *
 * Algorithm: This sequence is used to configure MAC in differnet packet
 * processing modes like promiscuous, multicast, unicast,
 * hash unicast/multicast.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] pfilter: OSI filter structure.
 *
 * @note
 *	1) MAC should be initialized and started. see osi_start_mac()
 *	2) MAC addresses should be configured in HW registers. see
 *	osi_update_mac_addr_low_high_reg().
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_config_mac_pkt_filter_reg(struct osi_core_priv_data *osi_core,
				  struct osi_filter pfilter);

/**
 * @brief osi_update_mac_addr_low_high_reg- invoke API to update L2 address
 *	in filter register
 *
 * Algorithm: This routine update MAC address to register for filtering
 * based on dma_routing_enable, addr_mask and src_dest. Validation of
 * dma_chan as well as DCS bit enabled in RXQ to DMA mapping register
 * performed before updating DCS bits.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] index: filter index
 * @param[in] value: MAC address to write
 * @param[in] dma_routing_enable: dma channel routing enable(1)
 * @param[in] dma_chan: dma channel number
 * @param[in] addr_mask: filter will not consider byte in comparison
 *	Bit 29: MAC_Address${i}_High[15:8]
 *	Bit 28: MAC_Address${i}_High[7:0]
 *	Bit 27: MAC_Address${i}_Low[31:24]
 *	..
 *	Bit 24: MAC_Address${i}_Low[7:0]
 * @param[in] src_dest: SA(1) or DA(0)
 *
 * @note
 *	1) MAC should be initialized and stated. see osi_start_mac()
 *	2) osi_core->osd should be populated.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_update_mac_addr_low_high_reg(struct osi_core_priv_data *osi_core,
				     unsigned int index,
				     unsigned char value[],
				     unsigned int dma_routing_enable,
				     unsigned int dma_chan,
				     unsigned int addr_mask,
				     unsigned int src_dest);

/**
 * @brief osi_config_l3_l4_filter_enable -  invoke OSI call to enable L3/L4
 *	filters.
 *
 * Algorithm: This routine to enable/disable L4/l4 filter
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enable: enable/disable
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_config_l3_l4_filter_enable(struct osi_core_priv_data *osi_core,
				   unsigned int enable);

/**
 * @brief osi_config_l3_filters - invoke OSI call config_l3_filters.
 *
 * Algorithm: Check for DCS_enable as well as validate channel
 * number and if dcs_enable is set. After validation, code flow
 * is used to configure L3(IPv4/IPv6) filters resister
 * for address matching.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] enb_dis:  1 - enable otherwise - disable L3 filter
 * @param[in] ipv4_ipv6_match: 1 - IPv6, otherwise - IPv4
 * @param[in] src_dst_addr_match: 0 - source, otherwise - destination
 * @param[in] perfect_inverse_match: normal match(0) or inverse map(1)
 * @param[in] dma_routing_enable: filter based dma routing enable(1)
 * @param[in] dma_chan: dma channel for routing based on filter
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) L3/L4 filtering should be enabled in MAC PFR register. See
 *	osi_config_l3_l4_filter_enable()
 *	3) osi_core->osd should be populated
 *	4) DCS bits should be enabled in RXQ to DMA map register
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_config_l3_filters(struct osi_core_priv_data *osi_core,
			  unsigned int filter_no,
			  unsigned int enb_dis,
			  unsigned int ipv4_ipv6_match,
			  unsigned int src_dst_addr_match,
			  unsigned int perfect_inverse_match,
			  unsigned int dma_routing_enable,
			  unsigned int dma_chan);

/**
 * @brief osi_update_ip4_addr -  invoke OSI call update_ip4_addr.
 *
 * Algorithm:  This sequence is used to update IPv4 source/destination
 * Address for L3 layer filtering
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] addr: ipv4 address
 * @param[in] src_dst_addr_match: 0- source(addr0) 1- dest (addr1)
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) L3/L4 filtering should be enabled in MAC PFR register. See
 *	osi_config_l3_l4_filter_enable()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_update_ip4_addr(struct osi_core_priv_data *osi_core,
			unsigned int filter_no,
			unsigned char addr[],
			unsigned int src_dst_addr_match);

/**
 * @brief osi_update_ip6_addr -  invoke OSI call update_ip6_addr.
 *
 * Algorithm:  This sequence is used to update IPv6 source/destination
 * Address for L3 layer filtering
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] addr: ipv6 adderss
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) L3/L4 filtering should be enabled in MAC PFR register. See
 *	osi_config_l3_l4_filter_enable()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_update_ip6_addr(struct osi_core_priv_data *osi_core,
			unsigned int filter_no,
			unsigned short addr[]);

/**
 * @brief osi_config_l4_filters - invoke OSI call config_l4_filters.
 *
 * Algorithm: This sequence is used to configure L4(TCP/UDP) filters for
 * SA and DA Port Number matching
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] enb_dis: enable/disable L4 filter
 * @param[in] tcp_udp_match: 1 - udp, 0 - tcp
 * @param[in] src_dst_port_match: port matching enable/disable
 * @param[in] perfect_inverse_match: normal match(0) or inverse map(1)
 * @param[in] dma_routing_enable: filter based dma routing enable(1)
 * @param[in] dma_chan: dma channel for routing based on filter
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) L3/L4 filtering should be enabled in MAC PFR register. See
 *	osi_config_l3_l4_filter_enable()
 *	3) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_config_l4_filters(struct osi_core_priv_data *osi_core,
			  unsigned int filter_no,
			  unsigned int enb_dis,
			  unsigned int tcp_udp_match,
			  unsigned int src_dst_port_match,
			  unsigned int perfect_inverse_match,
			  unsigned int dma_routing_enable,
			  unsigned int dma_chan);

/**
 * @brief osi_update_l4_port_no - invoke OSI call for update_l4_port_no.
 * Algoriths sequence is used to update Source Port Number for
 * L4(TCP/UDP) layer filtering.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_no: filter index
 * @param[in] port_no: port number
 * @param[in] src_dst_port_match: source port - 0, dest port - 1
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) L3/L4 filtering should be enabled in MAC PFR register. See
 *	osi_config_l3_l4_filter_enable()
 *	3) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_update_l4_port_no(struct osi_core_priv_data *osi_core,
			  unsigned int filter_no, unsigned short port_no,
			  unsigned int src_dst_port_match);

/**
 * @brief osi_config_vlan_filtering - OSI call for configuring VLAN filter
 *
 * Algorithm: This sequence is used to enable/disable VLAN filtering and
 * also selects VLAN filtering mode- perfect/hash
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter_enb_dis: vlan filter enable(1) disable(0)
 * @param[in] perfect_hash_filtering: perfect(0) or hash filter(1)
 * @param[in] perfect_inverse_match: normal(0) or inverse filter(1)
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_config_vlan_filtering(struct osi_core_priv_data *osi_core,
			      unsigned int filter_enb_dis,
			      unsigned int perfect_hash_filtering,
			      unsigned int perfect_inverse_match);

/**
 * @brief osi_config_l2_da_perfect_inverse_match -
 * trigger OSI call for config_l2_da_perfect_inverse_match.
 *
 * Algorithm: This sequence is used to select perfect/inverse matching
 *	  for L2 DA
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] perfect_inverse_match: 1 - inverse mode 0- normal mode
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int  osi_config_l2_da_perfect_inverse_match(struct osi_core_priv_data *osi_core,
					    unsigned int perfect_inverse_match);

/**
 * @brief osi_update_vlan_id - invoke osi call to update VLAN ID
 *
 * Algorithm: return 16 bit VLAN ID
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] vid: VLAN ID
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int  osi_update_vlan_id(struct osi_core_priv_data *osi_core,
			unsigned int vid);

/**
 * @brief osi_write_phy_reg - Write to a PHY register through MAC over MDIO bus.
 *
 * Algorithm:
 * 1) Before proceeding for reading for PHY register check whether any MII
 *    operation going on MDIO bus by polling MAC_GMII_BUSY bit.
 * 2) Program data into MAC MDIO data register.
 * 3) Populate required parameters like phy address, phy register etc,,
 *	in MAC MDIO Address register. write and GMII busy bits needs to be set
 *	in this operation.
 * 4) Write into MAC MDIO address register poll for GMII busy for MDIO
 *	operation to complete.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be write to PHY.
 * @param[in] phydata: Data to write to a PHY register.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_write_phy_reg(struct osi_core_priv_data *osi_core, unsigned int phyaddr,
		      unsigned int phyreg, unsigned short phydata);

/**
 * @brief osi_read_mmc - invoke function to read actual registers and update
 *	  structure variable mmc
 * 
 * Algorithm: Read the registers, mask reserve bits if required, update
 *	  structure.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_read_mmc(struct osi_core_priv_data *osi_core);

/**
 * @brief osi_reset_mmc - invoke function to reset MMC counter and data
 *	  structure
 *
 * Algorithm: Read the registers, mask reserve bits if required, update
 *	  structure.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) osi_core->osd should be populated
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_reset_mmc(struct osi_core_priv_data *osi_core);

/**
 * @brief osi_read_phy_reg - Read from a PHY register through MAC over MDIO bus.
 *
 * Algorithm:
 *	  1) Before proceeding for reading for PHY register check whether any MII
 *	  operation going on MDIO bus by polling MAC_GMII_BUSY bit.
 *	  2) Populate required parameters like phy address, phy register etc,,
 *	  in program it in MAC MDIO Address register. Read and GMII busy bits
 *	  needs to be set in this operation.
 *	  3) Write into MAC MDIO address register poll for GMII busy for MDIO
 *	  operation to complete. After this data will be available at MAC MDIO
 *	  data register.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be read from PHY.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval data from PHY register on success
 * @retval -1 on failure
 */
int osi_read_phy_reg(struct osi_core_priv_data *osi_core, unsigned int phyaddr,
		     unsigned int phyreg);

/**
 * @brief initializing the core operations
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @retval data from PHY register on success
 * @retval -1 on failure
 */
int osi_init_core_ops(struct osi_core_priv_data *osi_core);

/**
 * @brief osi_set_systime_to_mac - Handles setting of system time.
 *
 * Algorithm: Set current system time to MAC.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] sec: Seconds to be configured.
 * @param[in] nsec: Nano seconds to be configured.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_set_systime_to_mac(struct osi_core_priv_data *osi_core,
			   unsigned int sec, unsigned int nsec);

/**
 * @brief osi_adjust_freq - Adjust frequency
 *
 * Algorithm: Adjust a drift of +/- comp nanoseconds per second.
 *	  "Compensation" is the difference in frequency between
 *	  the master and slave clocks in Parts Per Billion.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] ppb: Parts per Billion
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_adjust_freq(struct osi_core_priv_data *osi_core, int ppb);

/**
 * @brief osi_adjust_time - Adjust time
 *
 * Algorithm: Adjust/update the MAC time (delta time from MAC to system time
 * passed in nanoseconds, can be + or -).
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] delta: Delta time
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) osi_core->ptp_config.one_nsec_accuracy need to be set to 1
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_adjust_time(struct osi_core_priv_data *osi_core, long delta);

/**
 * @brief osi_get_systime_from_mac - Get system time
 *
 * Algorithm: Gets the current system time
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[out] sec: Value read in Seconds
 * @param[out] nsec: Value read in Nano seconds
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_get_systime_from_mac(struct osi_core_priv_data *osi_core,
			     unsigned int *sec,
			     unsigned int *nsec);
/**
 * @brief osi_ptp_configuration - Configure PTP
 *
 * Algorithm: Configure the PTP registers that are required for PTP.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enable: Enable or disable Time Stamping. 0: Disable 1: Enable
 *
 * @note
 *	1) MAC should be init and started. see osi_start_mac()
 *	2) osi->ptp_config.ptp_filter need to be filled accordingly to the
 *	filter that need to be set for PTP packets. Please check osi_ptp_config
 *	structure declaration on the bit fields that need to be filled.
 *	3) osi->ptp_config.ptp_clock need to be filled with the ptp system clk.
 *	Currently it is set to 62500000Hz.
 *	4) osi->ptp_config.ptp_ref_clk_rate need to be filled with the ptp
 *	reference clock that platform supports.
 *	5) osi->ptp_config.sec need to be filled with current time of seconds
 *	6) osi->ptp_config.nsec need to be filled with current time of nseconds
 *	7) osi->base need to be filled with the ioremapped base address
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
int osi_ptp_configuration(struct osi_core_priv_data *osi_core,
			  unsigned int enable);

/* MAC version specific implementation function prototypes added here
 * for misra compliance to have
 * 1. Visible prototype for all functions.
 * 2. Only one prototype for all function.
 */
struct osi_core_ops *eqos_get_hw_core_ops(void);
void *eqos_get_core_safety_config(void);
#endif /* OSI_CORE_H */
